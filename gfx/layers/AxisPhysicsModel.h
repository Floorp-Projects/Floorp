/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AxisPhysicsModel_h
#define mozilla_layers_AxisPhysicsModel_h

#include "AxisPhysicsModel.h"
#include <sys/types.h>                  // for int32_t
#include "mozilla/TimeStamp.h"          // for TimeDuration

namespace mozilla {
namespace layers {


/**
 * AxisPhysicsModel encapsulates a generic 1-dimensional physically-based motion
 * model.
 *
 * It performs frame-rate independent interpolation and RK4 integration for
 * smooth animation with stable, deterministic behavior.
 * Implementations are expected to subclass and override the Acceleration()
 * method.
 */
class AxisPhysicsModel {
public:
  AxisPhysicsModel(double aInitialPosition, double aInitialVelocity);
  ~AxisPhysicsModel();

  /**
   * Advance the physics simulation.
   * |aDelta| is the time since the last sample.
   */
  void Simulate(const TimeDuration& aDeltaTime);

  /**
   * Gets the raw velocity of this axis at this moment.
   */
  double GetVelocity();

  /**
   * Sets the raw velocity of this axis at this moment.
   */
  void SetVelocity(double aVelocity);

  /**
   * Gets the raw position of this axis at this moment.
   */
  double GetPosition();

  /**
   * Sets the raw position of this axis at this moment.
   */
  void SetPosition(double aPosition);

protected:

  struct State
  {
    State(double ap, double av) : p(ap), v(av) {};
    double p; // Position
    double v; // Velocity
  };

  struct Derivative
  {
    Derivative() : dp(0.0), dv(0.0) {};
    Derivative(double aDp, double aDv) : dp(aDp), dv(aDv) {};
    double dp; // dp / delta time = Position
    double dv; // dv / delta time = Velocity
  };

  /**
   * Acceleration must be overridden and return the number of
   * axis-position-units / second that should be added or removed from the
   * velocity.
   */
  virtual double Acceleration(const State &aState) = 0;

private:

  /**
   * Duration of fixed delta time step (seconds)
   */
  static const double kFixedTimestep;

  /**
   * 0.0 - 1.0 value indicating progress between current and next simulation
   * sample.  Normalized to units of kFixedTimestep duration.
   */
  double mProgress;

  /**
   * Sample of simulation state as it existed
   * (1.0 - mProgress) * kFixedTimestep seconds in the past.
   */
  State mPrevState;

  /**
   * Sample of simulation state as it will be in mProgress * kFixedTimestep
   * seconds in the future.
   */
  State mNextState;

  /**
   * Perform RK4 (Runge-Kutta method) Integration to calculate the next
   * simulation sample.
   */
  void Integrate(double aDeltaTime);

  /**
   * Apply delta velocity and position represented by aDerivative over
   * aDeltaTime seconds, calculate new acceleration, and return new deltas.
   */
  Derivative Evaluate(const State &aInitState, double aDeltaTime,
                      const Derivative &aDerivative);

  /**
   * Helper function for performing linear interpolation (lerp) of double's
   */
  static double LinearInterpolate(double aV1, double aV2, double aBlend);

};


} // namespace layers
} // namespace mozilla

#endif
