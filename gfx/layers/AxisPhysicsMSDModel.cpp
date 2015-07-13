/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AxisPhysicsMSDModel.h"
#include <math.h>                       // for sqrt and fabs

namespace mozilla {
namespace layers {

/**
 * Constructs an AxisPhysicsMSDModel with initial values for state.
 *
 * @param aInitialPosition sets the initial position of the simulated spring,
 *        in AppUnits.
 * @param aInitialDestination sets the resting position of the simulated spring,
 *        in AppUnits.
 * @param aInitialVelocity sets the initial velocity of the simulated spring,
 *        in AppUnits / second.  Critically-damped and over-damped systems are
 *        guaranteed not to overshoot aInitialDestination if this is set to 0;
 *        however, it is possible to overshoot and oscillate if not set to 0 or
 *        the system is under-damped.
 * @param aSpringConstant sets the strength of the simulated spring.  Greater
 *        values of mSpringConstant result in a stiffer / stronger spring.
 * @param aDampingRatio controls the amount of dampening force and determines
 *        if the system is under-damped, critically-damped, or over-damped.
 */
AxisPhysicsMSDModel::AxisPhysicsMSDModel(double aInitialPosition,
                                         double aInitialDestination,
                                         double aInitialVelocity,
                                         double aSpringConstant,
                                         double aDampingRatio)
  : AxisPhysicsModel(aInitialPosition, aInitialVelocity)
  , mDestination(aInitialDestination)
  , mSpringConstant(aSpringConstant)
  , mSpringConstantSqrtXTwo(sqrt(mSpringConstant) * 2.0)
  , mDampingRatio(aDampingRatio)
{
}

AxisPhysicsMSDModel::~AxisPhysicsMSDModel()
{
}

double
AxisPhysicsMSDModel::Acceleration(const State &aState)
{
  // Simulate a Mass-Damper-Spring Model; assume a unit mass

  // Hooke’s Law: http://en.wikipedia.org/wiki/Hooke%27s_law
  double spring_force = (mDestination - aState.p) * mSpringConstant;
  double damp_force = -aState.v * mDampingRatio * mSpringConstantSqrtXTwo;

  return spring_force + damp_force;
}


double
AxisPhysicsMSDModel::GetDestination()
{
  return mDestination;
}

void
AxisPhysicsMSDModel::SetDestination(double aDestination)
{
  mDestination = aDestination;
}

bool
AxisPhysicsMSDModel::IsFinished()
{
  // In order to satisfy the condition of reaching the destination, the distance
  // between the simulation position and the destination must be less than
  // kFinishDistance while the speed is simultaneously less than
  // kFinishVelocity.  This enables an under-damped system to overshoot the
  // destination when desired without prematurely triggering the finished state.

  // As the number of app units per css pixel is 60 and retina / HiDPI displays
  // may display two pixels for every css pixel, setting kFinishDistance to 30.0
  // ensures that there will be no perceptable shift in position at the end
  // of the animation.
  const double kFinishDistance = 30.0;

  // If kFinishVelocity is set too low, the animation may end long after
  // oscillation has finished, resulting in unnecessary processing.
  // If set too high, the animation may prematurely terminate when expected
  // to overshoot the destination in an under-damped system.
  // 60.0 was selected through experimentation that revealed that a
  // critically damped system will terminate within 100ms.
  const double kFinishVelocity = 60.0;

  return fabs(mDestination - GetPosition ()) < kFinishDistance
    && fabs(GetVelocity()) <= kFinishVelocity;
}

} // namespace layers
} // namespace mozilla
