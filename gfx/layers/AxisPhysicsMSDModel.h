/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AxisPhysicsMSDModel_h
#define mozilla_layers_AxisPhysicsMSDModel_h

#include "AxisPhysicsModel.h"

namespace mozilla {
namespace layers {

/**
 * AxisPhysicsMSDModel encapsulates a 1-dimensional MSD (Mass-Spring-Damper)
 * model.  A unit mass is assumed.
 */
class AxisPhysicsMSDModel : public AxisPhysicsModel {
public:
  AxisPhysicsMSDModel(double aInitialPosition, double aInitialDestination,
                      double aInitialVelocity, double aSpringConstant,
                      double aDampingRatio);

  ~AxisPhysicsMSDModel();

  /**
   * Gets the raw destination of this axis at this moment.
   */
  double GetDestination() const;

  /**
   * Sets the raw destination of this axis at this moment.
   */
  void SetDestination(double aDestination);

  /**
   * Returns true when the position is close to the destination and the
   * velocity is low.
   */
  bool IsFinished(double aSmallestVisibleIncrement);

protected:
  virtual double Acceleration(const State &aState);

private:

  /**
   * mDestination represents the target position and the resting position of
   * the simulated spring.
   */
  double mDestination;

  /**
   * Greater values of mSpringConstant result in a stiffer / stronger spring.
   */
  double mSpringConstant;

  /**
   * mSpringConstantSqrtTimesTwo is calculated from mSpringConstant to reduce
   * calculations performed in the inner loop.
   */
  double mSpringConstantSqrtXTwo;

  /**
   * Damping Ratio: http://en.wikipedia.org/wiki/Damping_ratio
   *
   * When mDampingRatio < 1.0, this is an under damped system.
   * - Overshoots destination and oscillates with the amplitude gradually
   *   decreasing to zero.
   *
   * When mDampingRatio == 1.0, this is a critically damped system.
   * - Reaches destination as quickly as possible without oscillating.
   *
   * When mDampingRatio > 1.0, this is an over damped system.
   * - Reaches destination (exponentially decays) without oscillating.
   */
  double mDampingRatio;

};


} // namespace layers
} // namespace mozilla

#endif
