#include "FluidSim.h"
#include "MetaConfig.h"
#include "Grid.h"
#include "Vec.h"
#include "Utils.h"
#include "Particles.h"
#include "sim.h"

#include <vector>
#include <cmath>
#include <stack>
#include <iostream>
#include <boost//numeric/ublas/matrix.hpp>
using namespace std;

namespace ublas = boost::numeric::ublas;

#define FLUID 1
#define SOLID 2
#define EMPTY 0

/**************************************************************************
 * THE INITIALIZATION ROUTINES    
 **************************************************************************
 * 1. initialize
 * 2. initializeSolidBoundaries
 * 3. initializeFluidBo
 **************************************************************************/

/**
 * 
 * intializes the grid, the start and endTimes
 * 
 * @think must we also initialize the fluid and solid here?
 * 
 * @param grid
 * @param pStartTime
 * @param pEndTime
 */
void FluidSim::initialize(double pStartTime, double pEndTime) {
	if (META_LOG)
		std::cout << "--META--\tFunctionCall\tFluidSim::initialize"
				<< std::endl;

	// First Initialize the Grid
	rGrid.initializeGrid();

	rGrid.makeFlammableBoundary = false;

	//Initialize the simulation times
	startTime = pStartTime;
	endTime = pEndTime;

	///@todo get correct value of this dt
	dt = 0.002;
	fbuoy = 9.8;

	if (SIM_LOG)
		std::cout << "--SIMLOG--\tSimulation initialised" << std::endl;
}

/**
 * @param grid
 */
void FluidSim::initializeSolidBoundaries() {

	// Note:
	// One may wonder why there are 8 for loops ?? it
	// But this is efficient by both fast mem access and time [saving if(s)]
	// can be done by one for loop and if

	// FIRST
	// Mark the solid cells

	// Bottom Boundary
	for (int x = 0; x < rGrid.nj; x++)
		rGrid.marker(0, x) = SOLID;

	// Top Boundary
	//for (int x = 0; x < rGrid.nj; x++)
	//rGrid.marker(rGrid.ni - 1, x) = SOLID;

	// Left Boundary
	for (int y = 0; y < rGrid.ni; y++)
		rGrid.marker(y, 0) = SOLID;

	// Right Boundary
	for (int y = 0; y < rGrid.ni; y++)
		rGrid.marker(y, rGrid.nj - 1) = SOLID;

	// SECOND
	// Apply Initial Boundary Conditions (velocity)

	// Bottom Boundary
	for (int y = 0; y < 2; y++)
		for (int x = 0; x < rGrid.nj; x++)
			rGrid.v(y, x) = 0.0;

	// Top Boundary
	// here y must have values ni-1 and ni
	// this is because v has extra information on upper Boundary (ni+1)
	//for (int y = rGrid.ni - 1; y < rGrid.ni + 1; y++)
	//for (int x = 0; x < rGrid.nj; x++)
	//rGrid.v(y, x) = 0.0;

	// Left Boundary
	for (int y = 0; y < rGrid.ni; y++)
		for (int x = 0; x < 2; x++)
			rGrid.u(y, x) = 0.0;

	// Right Boundary
	// here y must have values nj-1 and nj
	// this is because v has extra information on right Boundary (nj+1)
	for (int y = 0; y < rGrid.ni; y++)
		for (int x = rGrid.nj - 1; x < rGrid.nj + 1; x++)
			rGrid.u(y, x) = 0.0;

	// add custom boundary


	int x_lower = 1;
	int x_upper = 1;

	int x_ref = 18;
	int y_ref = 18;

	for (int y = y_ref; y < y_ref + 2; y++)
		for (int x = x_ref - x_lower; x < x_ref + x_upper; x++) {

			//cout << "y" << y << "x " << x << endl;
			rGrid.marker(y, x) = SOLID;

			rGrid.u(y, x) = 0;
			rGrid.u(y, x + 1) = 0;

		}

	for (int y = y_ref; y < y_ref + 2; y++)
		for (int x = x_ref - x_lower; x < x_ref + x_upper; x++) {

			//cout << "y" << y << "x " << x << endl;
			rGrid.marker(y, x) = SOLID;

			rGrid.v(y, x) = -rGrid.v(y, x);
			rGrid.v(y + 1, x) = -rGrid.v(y + 1, x);

		}

	x_lower = 1;
	x_upper = 1;

	x_ref = 22;
	y_ref = 25;

	for (int y = y_ref; y < y_ref + 2; y++)
		for (int x = x_ref - x_lower; x < x_ref + x_upper; x++) {

			//cout << "y" << y << "x " << x << endl;
			rGrid.marker(y, x) = SOLID;

			rGrid.u(y, x) = -rGrid.u(y, x);
			rGrid.u(y, x + 1) = -rGrid.u(y, x + 1);

		}

	for (int y = y_ref; y < y_ref + 2; y++)
		for (int x = x_ref - x_lower; x < x_ref + x_upper; x++) {

			//cout << "y" << y << "x " << x << endl;
			rGrid.marker(y, x) = SOLID;

			rGrid.v(y, x) = -rGrid.v(y, x);
			rGrid.v(y + 1, x) = -rGrid.v(y + 1, x);

		}



}

/**
 *
 * @param grid
 * @param choice
 */
void FluidSim::initializeFluidBody(int choice) {

	if (META_LOG)
		std::cout << "--META--\tFluidSim\tintializeFluidBody" << endl;

	float lowerBound_x = 0.3;
	float lowerBound_y = rGrid.dx + rGrid.dx / 2.0;
	float upperBound_x = 0.7;
	float upperBound_y = 1.0 - 2 * rGrid.dx - rGrid.dx / 2.0;

	// these choices are used at time of initialization by init()
	///@todo ideally the same Enum must be used.
	switch (choice) {

	//STATIC_BED
	case 0:

		lowerBound_y = rGrid.dx + rGrid.dx / 2.0;
		upperBound_y = 0.5;
		lowerBound_x = rGrid.dx + rGrid.dx / 2.0;
		upperBound_x = 1.0 - (rGrid.dx + rGrid.dx / 2.0);

		for (int i = 0; i < sqr(rGrid.ni) * 16; ++i) {
			float xpos = randhashf(i * 2, 0, 1);
			float ypos = randhashf(i * 2 + 1, 0, 1);

			// the grid space is normalized from 0.0 to 1.0 in both directions
			// x and y.
			// each cell is of dimension (rGrid.dx X rGrid.dx)

			if (xpos > lowerBound_x && xpos < upperBound_x
					&& ypos < upperBound_y && ypos > lowerBound_y) {
				Vec2d pt(xpos, ypos);
				//rGrid.markerParticles.push_back(pt);
				Particles* p = new Particles(pt);
				if (p != NULL)
					rGrid.fireParticles.push_back(p);
				else
					cout << "BIG ERROR";
			}

		}

		break;

		//DAM_BREAK
	case 1:

		lowerBound_x = 0.6;
		lowerBound_y = rGrid.dx + rGrid.dx / 2.0;
		upperBound_x = 1.0 - rGrid.dx - rGrid.dx / 2.0;
		upperBound_y = 1.0 - rGrid.dx - rGrid.dx / 2.0;

		for (int i = 0; i < sqr(rGrid.ni) * 16; ++i) {
			float xpos = randhashf(i * 2, 0, 1);
			float ypos = randhashf(i * 2 + 1, 0, 1);

			// the grid space is normalized from 0.0 to 1.0 in both directions
			// x and y.
			// each cell is of dimension (rGrid.dx X rGrid.dx)

			if (xpos > lowerBound_x && xpos < upperBound_x
					&& ypos < upperBound_y && ypos > lowerBound_y) {

				Vec2d pt(xpos, ypos);

				Particles* p = new Particles(pt);
				if (p != NULL)
					rGrid.fireParticles.push_back(p);
				else
					cout << "BIG ERROR";
			}

		}

		break;

		// DOUBLE_DAM_FUEL_SOURCE
	case 2:
		lowerBound_x = 0.45;
		lowerBound_y = 1.5 * rGrid.dx + rGrid.dx / 2.0;
		upperBound_x = 0.65;
		upperBound_y = 0.15;

		for (int i = 0; i < sqr(rGrid.ni) * 16; ++i) {
			float xpos = randhashf(i * 2, 0, 1);
			float ypos = randhashf(i * 2 + 1, 0, 1);

			// the grid space is normalized from 0.0 to 1.0 in both directions
			// x and y.
			// each cell is of dimension (rGrid.dx X rGrid.dx)

			if (xpos > lowerBound_x && xpos < upperBound_x
					&& ypos < upperBound_y && ypos > lowerBound_y) {

				Vec2d pt(xpos, ypos);
				//rGrid.markerParticles.push_back(pt);
				Particles* p = new Particles(pt);
				if (p != NULL)
					rGrid.fireParticles.push_back(p);
				else
					cout << "BIG ERROR";
			}

		}

		if (rGrid.makeFlammableBoundary == true) {
			lowerBound_x = 0.425;
			lowerBound_y = 0.425;
			upperBound_x = 0.485;
			upperBound_y = 0.485;

			for (int i = 0; i < sqr(rGrid.ni) * 32; ++i) {
				float xpos = randhashf(i * 2, 0, 1);
				float ypos = randhashf(i * 2 + 1, 0, 1);

				// the grid space is normalized from 0.0 to 1.0 in both directions
				// x and y.
				// each cell is of dimension (rGrid.dx X rGrid.dx)

				if (xpos > lowerBound_x && xpos < upperBound_x
						&& ypos < upperBound_y && ypos > lowerBound_y) {

					//cout<<"true"<<endl;
					Vec2d pt(xpos, ypos);

					Particles* p = new Particles(pt);
					if (p != NULL)
						rGrid.fireParticles.push_back(p);
					else
						cout << "BIG ERROR";
				}

			}
		}
		break;

	case 3:
		break;

		//CHANNEL_FLOW
	case 4:
		break;

		//CHANNEL_FLOW_OBSTACLE
	case 5:
		break;

		//DAM_BREAK
	default:
		for (int i = 0; i < sqr(rGrid.ni) * 8; ++i) {
			float x = randhashf(i * 2, 0, 1);
			float y = randhashf(i * 2 + 1, 0, 1);

			if (x >= 0.5 && y <= 0.6) {
				Vec2d pt(x, y);
				//rGrid.markerParticles.push_back(pt);
			}

			if (x <= 0.3 && y >= 0.5) {
				Vec2d pt(x, y);
				//rGrid.markerParticles.push_back(pt);
			}

		}

		break;
	}

}

/**************************************************************************
 * T	HE CORE SIMULATION ROUTINES
 **************************************************************************
 * 0. cfl
 * 1. advance
 *    1.1 clearMarkedFluidCells
 *    1.2 advanceParticles
 *    1.3 advect
 *    1.4 addForces
 *    1.5 project
 *    1.6 markFluidCells
 **************************************************************************/

/**
 *
 *
 * @return
 */
double FluidSim::cfl() {
	if (META_LOG)
		std::cout << "--META--\tFunctionCall\tFluidSim:cfl" << std::endl;

	double maxvel = 0.0;

	for (int i = 0; i < rGrid.ni + 1; i++)
		for (int j = 0; j < rGrid.nj; j++)
			maxvel = max(maxvel, fabs(rGrid.u(i, j)));

	for (int i = 0; i < rGrid.ni; i++)
		for (int j = 0; j < rGrid.nj + 1; j++)
			maxvel = max(maxvel, fabs(rGrid.v(i, j)));

	if (maxvel != 0.0)
		dt = rGrid.dx / maxvel;

	if (SIM_LOG)
		std::cout << "--SIMLOG--\tcfl invoked: result dt calculated as = " << dt
				<< std::endl;
	return dt;
}

/**
 * The main fluid simulation step
 *
 * @param timestep
 */
void FluidSim::advance(double timestep) {
	if (SIM_LOG)
		std::cout << "--SIMLOG-- simulation advance called for time = "
				<< startTime + timestep << std::endl;

	double t = 0;

	constructLevelSetPhi();

	while (t < timestep) {
		dt = cfl();
		if (t + dt > timestep)
			dt = timestep - t;

		this->initializeFluidBody(2);


		// passively advect the particles
		advectParticles();

		// setting markers
		markFluidCells();

		// add the buoyancy forces
		addForces();

		// advection
		advect();

		markFluidCells();

		// apply boundary conditions
		applyBoundaryConditions();
		applyBoundaryConditions();

		// make fluid incompressible
		project();

		// apply boundary conditions
		applyBoundaryConditions();

		t += dt;
	}
	if (SIM_LOG)
		std::cout << "--SIMLOG-- simulation ended" << std::endl;

}

void FluidSim::clearMarkedFluidCells() {

	for (int y = 0; y < rGrid.ni; y++)
		for (int x = 0; x < rGrid.nj; x++) {
			if (rGrid.marker(y, x) == FLUID)
				rGrid.marker(y, x) = EMPTY;

		}
}

/**
 * Perform 2nd order Runge-Kutta to move the particle23m
 * mM s in the fluid
 *
 */
void FluidSim::advectParticles() {
	if (SIM_LOG)
		std::cout << "--SIMLOG-- Particles passively advected" << std::endl;

	rGrid.advectionWeights.clear();

	rGrid.flammable.clear();

	for (unsigned int fp = 0; fp < rGrid.fireParticles.size(); ++fp) {
		rGrid.fireParticles[fp]->timeAlive += dt;

		double yPos = rGrid.fireParticles[fp]->pos[0];
		double xPos = rGrid.fireParticles[fp]->pos[1];

		if (FLAME_HEIGHT > 0.4) {
			if (rGrid.fireParticles[fp]->timeAlive > FLAME_HEIGHT * 1.4
					|| yPos / rGrid.dx > (rGrid.ni - 2))
				rGrid.fireParticles.erase(rGrid.fireParticles.begin() + fp);
		} else {
			if (yPos / rGrid.dx > (rGrid.ni - 2))
				rGrid.fireParticles.erase(rGrid.fireParticles.begin() + fp);
		}

		int i = yPos / rGrid.dx;
		int j = xPos / rGrid.dx;

		if (rGrid.marker(i, j) == SOLID
				&& rGrid.makeFlammableBoundary == false) {
			//rGrid.flammable(i, j) = true;
			rGrid.makeFlammableBoundary = true;
		}

		rGrid.fireParticles[fp]->pos = rGrid.trace_rk2(
				rGrid.fireParticles[fp]->pos, dt);

		if ((rGrid.fireParticles[fp]->pos[0] / rGrid.dx) > rGrid.ni - 1)
			rGrid.fireParticles[fp]->pos[0] = ((rGrid.ni - 1) * rGrid.dx)
					- rGrid.dx / 2.0;
		if ((rGrid.fireParticles[fp]->pos[1] / rGrid.dx) > rGrid.nj - 1)
			rGrid.fireParticles[fp]->pos[1] = ((rGrid.nj - 1) * rGrid.dx)
					- rGrid.dx / 2.0;

	}
}

/**
 * Basic first order semi-Lagrangian advection of velocities
 *
 */
void FluidSim::advect() {
	if (SIM_LOG)
		std::cout << "--SIMLOG-- normal advection routine finished"
				<< std::endl;

//semi-Lagrangian advection on u-component of velocity

// remember y is the rows in y axis
// information above
// x is columns in x axis..
// information to the right

// u has extra information in x direction or in columns
	for (int y = 0; y < rGrid.ni; ++y)
		for (int x = 0; x < rGrid.nj + 1; ++x) {
			Vec2d pos((x + 0.5) * rGrid.dx, y * rGrid.dx);
			pos = rGrid.trace_rk2(pos, -dt);
			rGrid.temp_u(y, x) = rGrid.getVelocity(pos)[0];
		}

//semi-Lagrangian advection on v-component of velocity

// remember y is the rows in y axis
// information above
// x is columns in x axis..
// information to the right

//rGrid.advectionWeights.clear();
//std::cout<<"step_Break"<<endl;

// v has extra information in y direction or in rows
	for (int y = 0; y < rGrid.ni + 1; ++y)
		for (int x = 0; x < rGrid.nj; ++x) {
			Vec2d pos(x * rGrid.dx, (y + 0.5) * rGrid.dx);
			pos = rGrid.trace_rk2(pos, -dt);
			rGrid.temp_v(y, x) = rGrid.getVelocity(pos)[1];
		}

// semi-Lagrangian advection for the levelSetPhi
	for (int y = 0; y < rGrid.ni; ++y)
		for (int x = 0; x < rGrid.nj; ++x) {
			Vec2d pos(x * rGrid.dx, (y + 0.5) * rGrid.dx);
			pos = rGrid.trace_rk2(pos, -dt);
			rGrid.levelSetTemp(x, y) = rGrid.getLevelSetPhi(pos);
		}
	rGrid.levelSetPhi = rGrid.levelSetTemp;

//move update velocities into u/v vectors
	rGrid.u = rGrid.temp_u;
	rGrid.v = rGrid.temp_v;

}

void FluidSim::calculateCenterVelocityField() {

	rGrid.velCenter.clear();

	for (int y = 0; y < rGrid.ni; y++)
		for (int x = 0; x < rGrid.nj; x++) {
			double u1 = rGrid.u(y, x);
			double u2 = rGrid.u(y, x + 1);
			rGrid.velCenter(y, x)[0] = lerp(u1, u2, 0.5);

		}

	for (int y = 0; y < rGrid.ni; y++)
		for (int x = 0; x < rGrid.nj; x++) {
			double v1 = rGrid.v(y, x);
			double v2 = rGrid.v(y + 1, x);
			rGrid.velCenter(y, x)[1] = lerp(v1, v2, 0.5);
		}

}

void FluidSim::calculateNormal() {

	rGrid.omega.clear();

	for (int y = 0; y < rGrid.ni; y++)
		for (int x = 0; x < rGrid.nj; x++) {

			int iplus_j = (y + 1 > rGrid.ni) ? rGrid.ni : (y + 1);
			int iminus_j = (y - 1 < 0) ? 0 : (y - 1);
			int i_jplus = (x + 1 > rGrid.nj) ? rGrid.nj : (x + 1);
			int i_jminus = (x - 1 < 0) ? 0 : (x + 1);

			double w = (rGrid.velCenter(iplus_j, x)[1]
					- rGrid.velCenter(iminus_j, x)[1])
					- (rGrid.velCenter(y, i_jplus)[0]
							- rGrid.velCenter(y, i_jminus)[0]);

			w = w / (2 * rGrid.dx);

			rGrid.omega(y, x) = w;

		}

	for (int y = 0; y < rGrid.ni; y++)
		for (int x = 0; x < rGrid.nj; x++) {

			int iplus_j = (y + 1 > rGrid.ni) ? rGrid.ni : (y + 1);
			int iminus_j = (y - 1 < 0) ? 0 : (y - 1);
			int i_jplus = (x + 1 > rGrid.nj) ? rGrid.nj : (x + 1);
			int i_jminus = (x - 1 < 0) ? 0 : (x + 1);

			double grad_i = abs(rGrid.omega(iplus_j, x))
					- abs(rGrid.omega(iminus_j, x)) / (2 * rGrid.dx);
			double grad_j = abs(rGrid.omega(y, i_jplus))
					- abs(rGrid.omega(y, i_jminus)) / (2 * rGrid.dx);

			double mag = std::sqrt(grad_i * grad_i + grad_j * grad_j)
					+ 10e-10 * (1.0 / (dt * rGrid.dx));

			if (mag == 0)
				cout << "y " << y << "x " << x;

			rGrid.normal(y, x)[0] = grad_i / mag;
			rGrid.normal(y, x)[1] = grad_j / mag;
		}

}

void FluidSim::confineVorticity() {

}

/**
 * Add Gravity
 *
 */
void FluidSim::addForces() {

// remember y is the rows in y axis
// information above
// x is columns in x axis..

// information to the right

	this->calculateCenterVelocityField();
	this->calculateNormal();

// max rows in v are ni+1 or y ranges from o to ni
	for (int y = 0; y < rGrid.ni; y++)
		for (int x = 0; x < rGrid.nj; x++) {

			if (rGrid.marker(y, x) == FLUID) {
				rGrid.v(y, x) += dt * fbuoy;

				// the cross product
				//rGrid.u(y,x) += rGrid.normal(y,x)[1] * rGrid.omega(y,x);

				/*
				 double force = rGrid.dx
				 * (rGrid.normal(y, x)[0] * rGrid.omega(y, x));
				 rGrid.v(y, x) += 0.065 * force * dt;
				 */

			}
			double force = rGrid.dx
					* (rGrid.normal(y, x)[0] * rGrid.omega(y, x));
			rGrid.v(y, x) += 0.065 * force * dt;
		}

	for (int y = 0; y < rGrid.ni; y++)
		for (int x = 0; x < rGrid.nj; x++) {

			if (rGrid.marker(y, x) == FLUID) {
				// the cross product
				///.cout<<force<<endl;5
				/*
				 double force = rGrid.dx
				 * (rGrid.normal(y, x)[1] * rGrid.omega(y, x));

				 rGrid.u(y, x) += 25 * force * dt;
				 */

			}

			double force = rGrid.dx
					* (rGrid.normal(y, x)[1] * rGrid.omega(y, x));

			rGrid.u(y, x) += 20 * force * dt;

		}

	if (SIM_LOG)
		std::cout << "--SIMLOG-- forces added " << std::endl;
}

/**
 *
 * @param grid
 */
void FluidSim::applyBoundaryConditions() {

// Bottom Boundary
	for (int y = 0; y < 2; y++)
		for (int x = 0; x < rGrid.nj; x++)
			rGrid.v(y, x) = 0.0;

// Top Boundary
// here y must have values ni-1 and ni
// this is because v has extra information on upper Boundary (ni+1)
	for (int y = rGrid.ni - 1; y < rGrid.ni + 1; y++)
		for (int x = 0; x < rGrid.nj; x++)
			rGrid.v(y, x) = 0.0;

// Left Boundary
	for (int y = 0; y < rGrid.ni; y++)
		for (int x = 0; x < 2; x++)
			rGrid.u(y, x) = 0.0;

// Right Boundary
// here y must have values nj-1 and nj
// this is because v has extra information on right Boundary (nj+1)
	for (int y = 0; y < rGrid.ni; y++)
		for (int x = rGrid.nj - 1; x < rGrid.nj + 1; x++)
			rGrid.u(y, x) = 0.0;

	this->initializeSolidBoundaries();

}

/**
 *
 * @param grid
 */
void FluidSim::markFluidCells() {

// first clear all the past marker cells
	rGrid.marker.clear();

	this->initializeSolidBoundaries();

// clear the boundary markers
	rGrid.boundary.clear();

// first detect potential cells from the particles themselves

	for (unsigned int i = 0; i < rGrid.fireParticles.size(); i++) {

		Vec2d pos(rGrid.fireParticles[i]->pos / rGrid.dx);
		int x = int(pos[0]);
		int y = int(pos[1]);

		rGrid.marker(y, x) = FLUID; // means fluid cells
	}

// Updating fluid marker cells on basis of eight neighbor count
// for internal cells i.e. within the boundary

	int eight_neighbor_count = 0;
	int four_neighbor_count = 0;

	for (int y = 1; y < rGrid.ni - 1; y++)
		for (int x = 1; x < rGrid.nj - 1; x++) {

			if (rGrid.marker(y, x) == FLUID) {
				eight_neighbor_count = int(rGrid.marker(y + 1, x) == FLUID)
						+ int(rGrid.marker(y - 1, x) == FLUID)
						+ int(rGrid.marker(y, x - 1) == FLUID)
						+ int(rGrid.marker(y, x + 1) == FLUID)
						+ int(rGrid.marker(y + 1, x) == FLUID)
						+ int(rGrid.marker(y - 1, x) == FLUID)
						+ int(rGrid.marker(y, x - 1) == FLUID)
						+ int(rGrid.marker(y, x + 1) == FLUID);

				four_neighbor_count = int(rGrid.marker(y + 1, x + 1) == FLUID)
						+ int(rGrid.marker(y - 1, x + 1) == FLUID)
						+ int(rGrid.marker(y + 1, x - 1) == FLUID)
						+ int(rGrid.marker(y - 1, x - 1) == FLUID);

				if (eight_neighbor_count >= 7)
					rGrid.marker(y, x) = FLUID;

				if (four_neighbor_count < 4)
					rGrid.boundary(y, x) = FLUID;

			}

		}

}

void FluidSim::constructLevelSetPhi() {

	rGrid.levelSetPhi.clear();
	rGrid.levelSetTemp.clear();

	std::stack<int> yPhi;
	std::stack<int> xPhi;

	int band = 5;

// Preparation
	for (int y = 0; y < rGrid.ni; y++)
		for (int x = 0; x < rGrid.nj; x++) {
			if (rGrid.boundary(y, x) == FLUID) {
				rGrid.levelSetPhi(y, x) = 0;
				yPhi.push(y);
				xPhi.push(x);

			} else if (rGrid.marker(y, x) == FLUID
					&& rGrid.boundary(y, x) != FLUID)
				rGrid.levelSetPhi(y, x) = -100.0;
			else
				rGrid.levelSetPhi(y, x) = +100.0;
		}

	while (!yPhi.empty() && !xPhi.empty()) {

		int y = yPhi.top();
		int x = xPhi.top();

		yPhi.pop();
		xPhi.pop();

		for (int i = -band; i < band; i++)
			for (int j = -band; j < band; j++) {
				if ((y + i >= 0) && (y + i < rGrid.ni) && (x + j >= 0)
						&& (x + j < rGrid.nj)
						&& (rGrid.levelSetPhi(y + i, x + j) != 0)) {
					// manhattan distance
					double distance = std::abs(i + y - y) + std::abs(j + x - x);
					rGrid.levelSetPhi(i + y, j + x) = std::min(
							std::abs(rGrid.levelSetPhi(i + y, j + x)),
							distance);
				}

				if (rGrid.marker(i + y, j + x) == FLUID) {
					rGrid.levelSetPhi(i + y, j + x) = -rGrid.levelSetPhi(i + y,
							j + x);
				}
			}

	}

	for (int y = 0; y < rGrid.ni; y++)
		for (int x = 0; x < rGrid.nj; x++) {
			rGrid.smokeDensity(y, x) = rGrid.levelSetPhi(y, x) + band + 1;
		}

}

/**
 * making fluids incompressible
 * @param grid
 */
void FluidSim::project() {
	if (SIM_LOG)
		std::cout << "--SIMLOG-- Fluid Made incompressible done" << std::endl;
// displayverticalVelocities();
	findDivergence();
// displayDivergence();
	formA();
// displayPoisson();
	formPreconditioner();
//displayPreconditioner();
	solvePressure();
//  displayPressure();
	applyPressure();
}

/**************************************************************************
 * THE HELPER SIMULATION FUNCTIONS
 **************************************************************************
 * 1. findDivergence (called by project)
 * 2. formA (called by project)
 * 3. applyA (called by project)
 * 4. copyztos (called by project)
 * 5. zdotr (called by project)
 * 6. zdos (called by project)
 * 7. findRinform (called by project)
 * 8. solvePressre
 * 9. applyPressure
 * 10. formPreconditioner
 **************************************************************************/

void FluidSim::findDivergence() {

	rGrid.rhs.clear();
	rGrid.rhs_u_comp.clear();
	rGrid.rhs_v_comp.clear();

//double scale = 1.0  / rGrid.dx;
	double scale = 1.0;
	for (int y = 0; y < rGrid.nj; y++)
		for (int x = 0; x < rGrid.ni; x++) {
			rGrid.rhs(y, x) = 0.0;
			if (rGrid.marker(y, x) == FLUID) {
				rGrid.rhs(y, x) = scale
						* (rGrid.v(y + 1, x) - rGrid.v(y, x) + rGrid.u(y, x + 1)
								- rGrid.u(y, x));

				rGrid.rhs_u_comp(y, x) = scale
						* (rGrid.u(y, x + 1) - rGrid.u(y, x));
				rGrid.rhs_v_comp(y, x) = scale
						* (rGrid.v(y, x + 1) - rGrid.v(y, x));
			}
		}
}

void FluidSim::formA() {

// clearing off everything
	rGrid.Adiag.clear();
	rGrid.Aplusi.clear();
	rGrid.Aplusj.clear();

// setting up Adiag, Aplusi, Aplusj
	for (int y = 1; y < rGrid.ni - 1; y++)
		for (int x = 1; x < rGrid.nj - 1; x++) {
			if (rGrid.marker(y, x) == FLUID) {
				if (rGrid.marker(y + 1, x) != SOLID)
					rGrid.Adiag(y, x) += 1;
				if (rGrid.marker(y - 1, x) != SOLID)
					rGrid.Adiag(y, x) += 1;
				if (rGrid.marker(y, x + 1) != SOLID)
					rGrid.Adiag(y, x) += 1;
				if (rGrid.marker(y, x - 1) != SOLID)
					rGrid.Adiag(y, x) += 1;
				if (rGrid.marker(y + 1, x) == FLUID)
					rGrid.Aplusi(y, x) = -1;
				if (rGrid.marker(y, x + 1) == FLUID)
					rGrid.Aplusj(y, x) = -1;

			}
		}

}

void FluidSim::applyA() {

// clearing z
	rGrid.z.clear();

	for (int y = 1; y < rGrid.ni - 1; y++)
		for (int x = 1; x < rGrid.nj - 1; x++) {
			if (rGrid.marker(y, x) == FLUID) {
				rGrid.z(y, x) = rGrid.Adiag(y, x) * rGrid.s(y, x)
						+ rGrid.Aplusi(y, x) * rGrid.s(y + 1, x)
						+ rGrid.Aplusj(y, x) * rGrid.s(y, x + 1)
						+ rGrid.Aplusi(y - 1, x) * rGrid.s(y - 1, x)
						+ rGrid.Aplusj(y, x - 1) * rGrid.s(y, x - 1);
			}
		}
}

void FluidSim::formPreconditioner() {

	double p_const = 0.99;
	double e;

	rGrid.preConditioner.clear();

	for (int y = 1; y < rGrid.ni - 1; y++)
		for (int x = 1; x < rGrid.nj - 1; x++) {

			if (rGrid.marker(y, x) == FLUID) {
				e = rGrid.Adiag(y, x)
						- sqr(
								rGrid.Aplusi(y - 1, x)
										* rGrid.preConditioner(y - 1, x))
						- sqr(
								rGrid.Aplusj(y, x - 1)
										* rGrid.preConditioner(y, x - 1))
						- p_const
								* (rGrid.Aplusi(y - 1, x)
										* rGrid.Aplusj(y - 1, x)
										* sqr(rGrid.preConditioner(y - 1, x))
										+ rGrid.Aplusj(y, x - 1)
												* rGrid.Aplusi(y, x - 1)
												* sqr(
														rGrid.preConditioner(y,
																x - 1)));
				rGrid.preConditioner(y, x) = 1 / sqrt(e + 1e-6);
			}

		}
}

void FluidSim::applyPreconditioner() {

	double t;

	rGrid.m.clear();

// solve L*m=r
	for (int y = 1; y < rGrid.ni - 1; y++)
		for (int x = 1; x < rGrid.nj - 1; x++) {

			if (rGrid.marker(y, x) == FLUID) {
				t = rGrid.rhs(y, x)
						- rGrid.Aplusi(y - 1, x)
								* rGrid.preConditioner(y - 1, x)
								* rGrid.m(y - 1, x)
						- rGrid.Aplusj(y, x - 1)
								* rGrid.preConditioner(y, x - 1)
								* rGrid.m(y, x - 1);
				rGrid.m(y, x) = t * rGrid.preConditioner(y, x);
			}
		}

// solve L'*z=m

	rGrid.z.clear();

	for (int y = rGrid.ni - 2; y > 0; y--)
		for (int x = rGrid.nj - 2; x > 0; x--) {

			if (rGrid.marker(y, x) == FLUID) {

				t = rGrid.m(y, x)
						- rGrid.Aplusi(y, x) * rGrid.preConditioner(y, x)
								* rGrid.z(y + 1, x)
						- rGrid.Aplusj(y, x) * rGrid.preConditioner(y, x)
								* rGrid.z(y, x + 1);
				rGrid.z(y, x) = t * rGrid.preConditioner(y, x);
			}
		}

}

void FluidSim::copyztos() {
	for (int y = 0; y < rGrid.ni; y++)
		for (int x = 0; x < rGrid.nj; x++) {

			rGrid.s(y, x) = rGrid.z(y, x);
		}
}

double FluidSim::zdotr() {
	double result = 0.0;
	for (int y = 0; y < rGrid.ni; y++)
		for (int x = 0; x < rGrid.nj; x++) {

			result += rGrid.rhs(y, x) * rGrid.z(y, x);
		}
	return result;
}

double FluidSim::zdots() {

	double result = 0.0;
	for (int y = 0; y < rGrid.ni; y++)
		for (int x = 0; x < rGrid.nj; x++) {
			result += rGrid.s(y, x) * rGrid.z(y, x);
		}
	return result;
}

double FluidSim::findRInform() {

	double rinform = 0.0;

	for (int y = 0; y < rGrid.ni; y++)
		for (int x = 0; x < rGrid.nj; x++) {
			if (!(fabs(rGrid.rhs(y, x)) <= rinform))
				rinform = fabs(rGrid.rhs(y, x));
		}
	return rinform;
}

void FluidSim::solvePressure() {

	rGrid.temp_pressure = rGrid.pressure;

// clear the pressure :P
	rGrid.pressure.clear();

	double tol = 1e-5 * findRInform();
//   cout << "maximum r = " << findRInform() << " tol: " << tol << endl;
	if (findRInform() == 0.0)
		return;

	applyPreconditioner();

	this->copyztos();

	double rho = zdotr();

	if (rho == 0.0)
		return;

	for (int iter = 0; iter < 100; iter++) {
		applyA();
		double alpha = rho / zdots();
		for (int y = 0; y < rGrid.ni; y++)
			for (int x = 0; x < rGrid.nj; x++) {
				rGrid.pressure(y, x) += alpha * rGrid.s(y, x);
				rGrid.rhs(y, x) += (-1 * alpha * rGrid.z(y, x));
			}

		if (findRInform() <= tol) {

			return;
		}

		applyPreconditioner();
		double rhonew = zdotr();
		double beta = rhonew / rho;
		for (int y = 0; y < rGrid.ni; y++)
			for (int x = 0; x < rGrid.nj; x++) {

				rGrid.s(y, x) = beta * rGrid.s(y, x) + rGrid.z(y, x);
			}

		rho = rhonew;
	}

}

void FluidSim::applyPressure() {

	for (int y = 2; y < rGrid.ni - 1; y++)
		for (int x = 1; x < rGrid.nj - 1; x++) {
			if (rGrid.marker(y - 1, x) == FLUID || rGrid.marker(y, x) == FLUID) {
				rGrid.v(y, x) += rGrid.pressure(y, x)
						- rGrid.pressure(y - 1, x);
			}
		}

	for (int y = 1; y < rGrid.ni - 1; y++)
		for (int x = 2; x < rGrid.nj - 1; x++) {
			if (rGrid.marker(y, x - 1) == FLUID || rGrid.marker(y, x) == FLUID) {

				rGrid.u(y, x) += rGrid.pressure(y, x)
						- rGrid.pressure(y, x - 1);
			}
		}

}

/**************************************************************************
 * OTHER HELPER SIMULATION ROUTINES FOR FUTURE USE
 **************************************************************************
 * 1. constrainVelcoity
 * 2. copmuteWeights
 **************************************************************************/

/**
 * For extrapolated points, replace the normal component
 * of velocity with the object velocity (in this case zero).
 * @param grid
 */
void FluidSim::constrainVelocity() {
	std::cout << "--SIMLOG-- velcoity extrapolation done" << std::endl;
}

/**
 * Compute finite-volume style face-weights for fluid from nodal signed distances
 * @param grid
 */
void FluidSim::computeWeights() {
	std::cout << "--SIMLOG-- Weights are being computed" << std::endl;

}
//
//vo	id FluidSim::myProject() {
//
//	// Calculate the negative divergence b rhs) with modifications at the solid-wall boundaries
//	this->calculateNegativeDivergence();
//
//	// Set entries of A (stored in Adiag)
//	this->formCoefficientMatrixA();
//
//	// Construct  the MIC(0) pre conditioner.
//	this->formMyPreConditioner();
//
//	// Solve Ap=b with MICCG(0) , i.e. the PCG algorithm wih MIC(o) as pre conditioner
//	this->solveForPressure();
//
//	// Compute the new velocities u(n+1) according to pressure-gradient update to u
//	//this->updateVelocitiesFromPressure();
//}
//
//void FluidSim::calculateNegativeDivergence() {
//
//	if (SIM_LOG) {
//		cout << "--sub-SIM-LOG--\t calculateNegativeDivergence()" << endl;
//	}
//
//	rGrid.customRhs.clear();
//	rGrid.diagnose_u_rhs.clear();
//	rGrid.diagnose_v_rhs.clear();
//
//	// normal loop to account for Rhs
//	//double scale = -1.0 / rGrid.dx;
//	double scale = 1.0;
//
//	for (int x = 0; x < rGrid.nj; x++)
//		for (int y = 0; y < rGrid.ni; y++) {
//			if (rGrid.marker(y, x) == FLUID) {
//				rGrid.customRhs(y, x) = scale
//						* (rGrid.u(y, x + 1) - rGrid.u(y, x) + rGrid.v(y + 1, x)
//								- rGrid.v(y, x));
//
//				rGrid.diagnose_u_rhs(y, x) = scale
//						* (rGrid.u(y, x + 1) - rGrid.u(y, x));
//				rGrid.diagnose_v_rhs(y, x) = scale
//		we wi				* (rGrid.v(y, x + 1) - rGrid.v(y, x));
//			}
//
//		}
//
//	// additional loop to account for solid velocities. if nay
//
//	for (int x = 0; x < rGrid.nj; x++)
//		for (int y = 0; y < rGrid.ni; y++) {
//
//			if (rGrid.marker(y, x) == FLUID) {
//				// here usolid(y,x) == 0, had the solid been moving.. it might have been different
//				if (rGrid.marker(y, x - 1) == SOLID) {
//					rGrid.customRhs(y, x) -= (double) 1 / rGrid.dx
//							* (rGrid.u(y, x) - 0);
//
//					// for diagnostics
//					rGrid.diagnose_u_rhs(y, x) -= (double) 1 / rGrid.dx
//							* (rGrid.u(y, x) - 0);
//
//				}
//
//				// here usolid(y,x) == 0
//				if (rGrid.marker(y, x + 1) == SOLID) {
//
//					rGrid.customRhs(y, x) += (double) 1 / rGrid.dx
//							* (rGrid.u(y, x + 1) - 0);
//
//					// for diagnostics
//					rGrid.diagnose_u_rhs(y, x) += (double) 1 / rGrid.dx
//							* (rGrid.u(y, x + 1) - 0);
//				}
//
//				// here usolid(y,x) == 0
//				if (rGrid.marker(y - 1, x) == SOLID) {
//
//					rGrid.customRhs(y, x) -= (double) 1 / rGrid.dx
//							* (rGrid.v(y, x) - 0);
//
//					// for diagnostics
//					rGrid.diagnose_v_rhs(y, x) -= (double) 1 / rGrid.dx
//							* (rGrid.v(y, x) - 0);
//
//				}
//
//				// here usolid(y,x) == 0
//				if (rGrid.marker(y + 1, x) == SOLID) {
//
//					rGrid.customRhs(y, x) += (double) 1 / rGrid.dx
//							* (rGrid.v(y + 1, x) - 0);
//
//					// for diagnostics
//					rGrid.diagnose_v_rhs(y, x) += (double) 1 / rGrid.dx
//							* (rGrid.v(y + 1, x) - 0);
//
//				}
//
//			}
//		}
//}
//
//void FluidSim::formCoefficientMatrixA() {
//
//	rGrid.myAdiag.clear();
//	rGrid.Aplusy.clear();
//	rGrid.Aplusx.clear();
//
//	double scale = 0.01;
//
//	//    if (this->dt > std::pow(10, 4))
//	//        scale = 0.001 / (1.0 * rGrid.dx * rGrid.dx);
//	//    else
//	scale = dt / (1.0 * rGrid.dx * rGrid.dx);
//
//	if (SIM_LOG)
//		std::cout
//				<< "--sub-SIMLOG--\t formCoefficientMatrix(): scale calculated as = "
//				<< scale << std::endl;
//
//	for (int y = 1; y < rGrid.ni - 2; y++)
//		for (int x = 1; x < rGrid.nj - 2; x++) {
//
//			// x direction
//			if (rGrid.marker(y, x) == FLUID && rGrid.marker(y, x + 1) == FLUID) {
//
//				rGrid.myAdiag(y, x) += scale;
//				rGrid.myAdiag(y, x + 1) += scale;
//				rGrid.Aplusx(y, x) = -scale;
//
//			} else if (rGrid.marker(y, x) == FLUID
//					&& rGrid.marker(y, x + 1) == EMPTY) {
//				rGrid.myAdiag(y, x) += scale;
//			}
//
//			// y direction
//			if (rGrid.marker(y, x) == FLUID && rGrid.marker(y + 1, x) == FLUID) {
//
//				rGrid.myAdiag(y, x) += scale;
//				rGrid.myAdiag(y + 1, x) += scale;
//				rGrid.Aplusy(y, x) = -scale;
//			} else if (rGrid.marker(y, x) == FLUID
//					&& rGrid.marker(y + 1, x) == EMPTY) {
//
//				rGrid.myAdiag(y, x) += scale;
//			}
//
//		}
//
//	if (SIM_LOG) {
//		std::cout
//				<< "--sub-SIM-LOG--\t formCoefficientMatrix(): forming Adiag, Aplusx, Aplusy"
//				<< std::endl;
//	}
//
//}
//
//void FluidSim::formMyPreConditioner() {
//
//	if (SIM_LOG) {
//		cout << "--sub-sub-SIM-LOG--\t applying Pre-conditioner" << endl;
//	}
//
//	rGrid.myPreConditioner.clear();
//
//	const double tuningConst = 0.97;
//	const double safetyConst = 0.25;
//	double e;
//
//	for (int y = 0; y < rGrid.ni; y++)
//		for (int x = 0; x < rGrid.nj; x++) {
//			if (rGrid.marker(y, x) == FLUID) {
//				e = rGrid.myAdiag(y, x)
//						- std::pow(
//								(rGrid.Aplusx(y, x - 1)
//										* rGrid.myPreConditioner(y, x - 1)), 2)
//						- std::pow(
//								(rGrid.Aplusy(y - 1, x)
//										* rGrid.myPreConditioner(y - 1, x)), 2)
//						- tuningConst
//								* (rGrid.Aplusx(y, x - 1)
//										* rGrid.Aplusy(y, x - 1)
//										* std::pow(
//												rGrid.myPreConditioner(y,
//														x - 1), 2)
//										+ rGrid.Aplusy(y - 1, x)
//												* rGrid.Aplusx(y - 1, x)
//												* std::pow(
//														rGrid.myPreConditioner(
//																y - 1, x), 2));
//
//				if (e < safetyConst * rGrid.myAdiag(y, x))
//					e = rGrid.myAdiag(y, x);
//
//				rGrid.myPreConditioner(y, x) = 1.0 / std::sqrt(e);
//			}
//		}
//}
//
//void FluidSim::solveForPressure() {
//
//	double alpha = 0.0;
//	double newSigma = 0.0;
//	double beta = 0.0;
//
//	// set initial guess p = 0;
//	rGrid.pressure.clear();
//
//	if (SIM_LOG) {
//		std::cout << "--sub-sub-SIM-LOG--\t solveForPressure(): set p = 0"
//				<< std::endl;
//	}
//
//	// set residual vector residualR to b in [Ap = b]
//	rGrid.residualR = rGrid.customRhs;
//
//	if (SIM_LOG) {
//		std::cout << "--sub-sub-SIM-LOG--\t solveForPressure(): r = b"
//				<< std::endl;
//	}
//
//	// apply the pre-conditioner
//	// result stored in auxillaryZ
//	// and then searchS = auxillaryZ
//	this->applyMyPreconditioner();
//
//	rGrid.searchS = rGrid.auxillaryZ;
//
//	if (SIM_LOG) {
//		std::cout
//				<< "--sub-sub-SIM-LOG--\t solveForPressure(): applying Pre conditioner"
//				<< std::endl;
//	}
//
//	double sigma = this->dotProduct(rGrid.auxillaryZ, rGrid.residualR);
//
//	if (SIM_LOG) {
//		std::cout << "--sub-sub-SIM-LOG--\t solveForPressure(): sigma = z.r"
//				<< std::endl;
//	}
//
//	for (int i = 0; i < 50; i++) {
//
//		// set auxillary vector Z = applyA()
//		this->applyMyA();
//
//		// alpha = sigma / dotProductZS
//		alpha = sigma / this->dotProduct(rGrid.auxillaryZ, rGrid.searchS);
//
//		// update pressure = pressure + alpha * pressure
//		rGrid.pressure = rGrid.pressure + (alpha * rGrid.pressure);
//
//		// update residual = residual - alpha * auxillary_z
//		rGrid.residualR = rGrid.residualR - (alpha * rGrid.residualR);
//
//		// if |max(residual)| < tolerance
//		// then return pressure;
//
//		// set auxillary_z  = apply PreConditioner
//		// apply the pre-conditioner
//		// result stored in auxillaryZ
//		this->applyMyPreconditioner();
//
//		// and then searchS = auxillaryZ
//		rGrid.searchS = rGrid.auxillaryZ;
//
//		// newSigma = dotProduct(auxiallry_z, residual_r)
//		newSigma = this->dotProduct(rGrid.auxillaryZ, rGrid.residualR);
//
//		// beta = newSigma / sigma
//		beta = newSigma / sigma;
//
//		// search vector s = z + beta * s
//		rGrid.searchS = rGrid.auxillaryZ + (beta * rGrid.searchS);
//
//		// sigma = newSigma
//		sigma = newSigma;
//
//	}
//}
//
//void FluidSim::applyMyA() {
//	// clearing z
//	rGrid.auxillaryZ.clear();
//
//	for (int y = 1; y < rGrid.ni - 1; y++)
//		for (int x = 1; x < rGrid.nj - 1; x++) {
//			if (rGrid.marker(y, x) == FLUID) {
//				rGrid.auxillaryZ(y, x) = rGrid.myAdiag(y, x)
//						* rGrid.searchS(y, x)
//						+ rGrid.Aplusy(y, x) * rGrid.searchS(y + 1, x)
//						+ rGrid.Aplusx(y, x) * rGrid.searchS(y, x + 1)
//						+ rGrid.Aplusy(y - 1, x) * rGrid.searchS(y - 1, x)
//						+ rGrid.Aplusx(y, x - 1) * rGrid.searchS(y, x - 1);
//			}
//		}
//}
//void FluidSim::applyMyPreconditioner() {
//
//	// first solve [L q = residualR]
//
//	rGrid.q.clear();
//
//	for (int y = 0; y < rGrid.ni; y++)
//		for (int x = 0; x < rGrid.nj; x++) {
//
//			if (rGrid.marker(y, x) == FLUID) {
//
//				//may cause a segmentation fault if the cell(i,j) is FLUID like for 0,0
//
//				double t = rGrid.residualR(y, x)
//						- rGrid.Aplusx(y, x - 1)
//								* rGrid.myPreConditioner(y, x - 1)
//								* rGrid.q(y, x - 1)
//						- rGrid.Aplusy(y - 1, x)
//								* rGrid.myPreConditioner(y - 1, x)
//								* rGrid.q(y - 1, x);
//
//				rGrid.q(y, x) = t * rGrid.myPreConditioner(y, x);
//			}
//		}
//
//	// Next solve [L_transpose auxillaryZ = q]
//
//	rGrid.auxillaryZ.clear();
//
//	for (int y = rGrid.ni - 1; y >= 0; y--)
//		for (int x = rGrid.nj - 1; x >= 0; x--) {
//			if (rGrid.marker(y, x) == FLUID) {
//
//				//may cause a segmentation fault if the cell(i,j) is FLUID like fir (ni-1, nj-1)
//
//				double t = rGrid.q(y, x)
//						- rGrid.Aplusx(y, x) * rGrid.myPreConditioner(y, x)
//								* rGrid.auxillaryZ(y, x + 1)
//						- rGrid.Aplusy(y, x) * rGrid.myPreConditioner(y, x)
//								* rGrid.auxillaryZ(y + 1, x);
//
//				rGrid.auxillaryZ(y, x) = t * rGrid.myPreConditioner(y, x);
//
//			}
//		}
//
//}
//
//void FluidSim::updateVelocitiesFromPressure() {
//
//	for (int y = 2; y < rGrid.ni - 1; y++)
//		for (int x = 1; x < rGrid.nj - 1; x++) {
//			if (rGrid.marker(y - 1, x) == FLUID || rGrid.marker(y, x) == FLUID) {
//				rGrid.v(y, x) += rGrid.pressure(y, x)
//						- rGrid.pressure(y - 1, x);
//			}
//		}
//
//	for (int y = 1; y < rGrid.ni - 1; y++)
//		for (int x = 2; x < rGrid.nj - 1; x++) {
//			if (rGrid.marker(y, x - 1) == FLUID || rGrid.marker(y, x) == FLUID) {
//
//				rGrid.u(y, x) += rGrid.pressure(y, x)
//						- rGrid.pressure(y, x - 1);
//			}
//		}
//
//}
//
//double FluidSim::dotProduct(matrix<double> m1, matrix<double> m2) {
//
//	double scalarProduct = -1.0;
//
//	if (m1.size1() == m2.size2() && m1.size2() == m2.size2()) {
//
//		for (unsigned int y = 0; y < m1.size1(); y++)
//			for (unsigned int x = 0; x < m2.size2(); x++)
//				scalarProduct += m1(y, x) * m2(y, x);
//	}
//
//	return scalarProduct;
//
//}
