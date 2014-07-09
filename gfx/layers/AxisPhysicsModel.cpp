/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AxisPhysicsModel.h"

namespace mozilla {
namespace layers {

/**
 * The simulation is advanced forward in time with a fixed time step to ensure
 * that it remains deterministic given variable framerates.  To determine the
 * position at any variable time, two samples are interpolated.
 *
 * kFixedtimestep is set to 120hz in order to ensure that every frame in a
 * common 60hz refresh rate display will have at least one physics simulation
 * sample.  More accuracy can be obtained by reducing kFixedTimestep to smaller
 * intervals, such as 240hz or 1000hz, at the cost of more CPU cycles.  If
 * kFixedTimestep is increased to much longer intervals, interpolation will
 * become less effective at reducing temporal jitter and the simulation will
 * lose accuracy.
 */
const double AxisPhysicsModel::kFixedTimestep = 1.0 / 120.0; // 120hz

/**
 * Constructs an AxisPhysicsModel with initial values for state.
 *
 * @param aInitialPosition sets the initial position of the simulation,
 *        in AppUnits.
 * @param aInitialVelocity sets the initial velocity of the simulation,
 *        in AppUnits / second.
 */
AxisPhysicsModel::AxisPhysicsModel(double aInitialPosition,
                                   double aInitialVelocity)
  : mProgress(1.0)
  , mPrevState(aInitialPosition, aInitialVelocity)
  , mNextState(aInitialPosition, aInitialVelocity)
{

}

AxisPhysicsModel::~AxisPhysicsModel()
{

}

double
AxisPhysicsModel::GetVelocity()
{
  return LinearInterpolate(mPrevState.v, mNextState.v, mProgress);
}

double
AxisPhysicsModel::GetPosition()
{
  return LinearInterpolate(mPrevState.p, mNextState.p, mProgress);
}

void
AxisPhysicsModel::SetVelocity(double aVelocity)
{
  mNextState.v = aVelocity;
  mNextState.p = GetPosition();
  mProgress = 1.0;
}

void
AxisPhysicsModel::SetPosition(double aPosition)
{
  mNextState.v = GetVelocity();
  mNextState.p = aPosition;
  mProgress = 1.0;
}

void
AxisPhysicsModel::Simulate(const TimeDuration& aDeltaTime)
{
  for(mProgress += aDeltaTime.ToSeconds() / kFixedTimestep;
      mProgress > 1.0; mProgress -= 1.0) {
    Integrate(kFixedTimestep);
  }
}

void
AxisPhysicsModel::Integrate(double aDeltaTime)
{
  mPrevState = mNextState;

  // RK4 (Runge-Kutta method) Integration
  // http://en.wikipedia.org/wiki/Runge%E2%80%93Kutta_methods
  Derivative a = Evaluate( mNextState, 0.0, Derivative() );
  Derivative b = Evaluate( mNextState, aDeltaTime * 0.5, a );
  Derivative c = Evaluate( mNextState, aDeltaTime * 0.5, b );
  Derivative d = Evaluate( mNextState, aDeltaTime, c );

  double dpdt = 1.0 / 6.0 * (a.dp + 2.0 * (b.dp + c.dp) + d.dp);
  double dvdt = 1.0 / 6.0 * (a.dv + 2.0 * (b.dv + c.dv) + d.dv);

  mNextState.p += dpdt * aDeltaTime;
  mNextState.v += dvdt * aDeltaTime;
}

AxisPhysicsModel::Derivative
AxisPhysicsModel::Evaluate(const State &aInitState, double aDeltaTime,
                           const Derivative &aDerivative)
{
  State state( aInitState.p + aDerivative.dp*aDeltaTime, aInitState.v + aDerivative.dv*aDeltaTime );

  return Derivative( state.v, Acceleration(state) );
}

double
AxisPhysicsModel::LinearInterpolate(double aV1, double aV2, double aBlend)
{
  return aV1 * (1.0 - aBlend) + aV2 * aBlend;
}

}
}
