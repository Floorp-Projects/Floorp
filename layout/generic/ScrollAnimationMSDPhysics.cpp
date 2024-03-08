/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollAnimationMSDPhysics.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_general.h"
#include "mozilla/ToString.h"

static mozilla::LazyLogModule sApzMsdLog("apz.msd");
#define MSD_LOG(...) MOZ_LOG(sApzMsdLog, LogLevel::Debug, (__VA_ARGS__))

using namespace mozilla;

ScrollAnimationMSDPhysics::ScrollAnimationMSDPhysics(const nsPoint& aStartPos)
    : mStartPos(aStartPos),
      mModelX(
          0, 0, 0,
          StaticPrefs::general_smoothScroll_msdPhysics_regularSpringConstant(),
          1),
      mModelY(
          0, 0, 0,
          StaticPrefs::general_smoothScroll_msdPhysics_regularSpringConstant(),
          1),
      mIsFirstIteration(true) {}

void ScrollAnimationMSDPhysics::Update(const TimeStamp& aTime,
                                       const nsPoint& aDestination,
                                       const nsSize& aCurrentVelocity) {
  double springConstant = ComputeSpringConstant(aTime);

  // mLastSimulatedTime is the most recent time that this animation has been
  // "observed" at. We don't want to update back to a state in the past, so we
  // set mStartTime to the more recent of mLastSimulatedTime and aTime.
  // aTime can be in the past if we're processing an input event whose internal
  // timestamp is in the past.
  if (mLastSimulatedTime && aTime < mLastSimulatedTime) {
    mStartTime = mLastSimulatedTime;
  } else {
    mStartTime = aTime;
  }

  if (!mIsFirstIteration) {
    mStartPos = PositionAt(mStartTime);
  }

  mLastSimulatedTime = mStartTime;
  mDestination = aDestination;
  mModelX = NonOscillatingAxisPhysicsMSDModel(
      mStartPos.x, aDestination.x, aCurrentVelocity.width, springConstant, 1);
  mModelY = NonOscillatingAxisPhysicsMSDModel(
      mStartPos.y, aDestination.y, aCurrentVelocity.height, springConstant, 1);
  mIsFirstIteration = false;
}

void ScrollAnimationMSDPhysics::ApplyContentShift(const CSSPoint& aShiftDelta) {
  nsPoint shiftDelta = CSSPoint::ToAppUnits(aShiftDelta);
  mStartPos += shiftDelta;
  mDestination += shiftDelta;
  TimeStamp currentTime = mLastSimulatedTime;
  nsPoint currentPosition = PositionAt(currentTime) + shiftDelta;
  nsSize currentVelocity = VelocityAt(currentTime);
  double springConstant = ComputeSpringConstant(currentTime);
  mModelX = NonOscillatingAxisPhysicsMSDModel(currentPosition.x, mDestination.x,
                                              currentVelocity.width,
                                              springConstant, 1);
  mModelY = NonOscillatingAxisPhysicsMSDModel(currentPosition.y, mDestination.y,
                                              currentVelocity.height,
                                              springConstant, 1);
}

double ScrollAnimationMSDPhysics::ComputeSpringConstant(
    const TimeStamp& aTime) {
  if (!mPreviousEventTime) {
    mPreviousEventTime = aTime;
    mPreviousDelta = TimeDuration();
    return StaticPrefs::
        general_smoothScroll_msdPhysics_motionBeginSpringConstant();
  }

  TimeDuration delta = aTime - mPreviousEventTime;
  TimeDuration previousDelta = mPreviousDelta;

  mPreviousEventTime = aTime;
  mPreviousDelta = delta;

  double deltaMS = delta.ToMilliseconds();
  if (deltaMS >=
      StaticPrefs::
          general_smoothScroll_msdPhysics_continuousMotionMaxDeltaMS()) {
    return StaticPrefs::
        general_smoothScroll_msdPhysics_motionBeginSpringConstant();
  }

  if (previousDelta &&
      deltaMS >=
          StaticPrefs::general_smoothScroll_msdPhysics_slowdownMinDeltaMS() &&
      deltaMS >=
          previousDelta.ToMilliseconds() *
              StaticPrefs::
                  general_smoothScroll_msdPhysics_slowdownMinDeltaRatio()) {
    // The rate of events has slowed (the time delta between events has
    // increased) enough that we think that the current scroll motion is coming
    // to a stop. Use a stiffer spring in order to reach the destination more
    // quickly.
    return StaticPrefs::
        general_smoothScroll_msdPhysics_slowdownSpringConstant();
  }

  return StaticPrefs::general_smoothScroll_msdPhysics_regularSpringConstant();
}

void ScrollAnimationMSDPhysics::SimulateUntil(const TimeStamp& aTime) {
  if (!mLastSimulatedTime || aTime <= mLastSimulatedTime) {
    return;
  }
  TimeDuration delta = aTime - mLastSimulatedTime;
  mModelX.Simulate(delta);
  mModelY.Simulate(delta);
  mLastSimulatedTime = aTime;
  MSD_LOG("Simulated for duration %f, finished %d position %s velocity %s\n",
          delta.ToMilliseconds(), IsFinished(aTime),
          ToString(CSSPoint::FromAppUnits(PositionAt(aTime))).c_str(),
          ToString(CSSPoint::FromAppUnits(VelocityAt(aTime))).c_str());
}

nsPoint ScrollAnimationMSDPhysics::PositionAt(const TimeStamp& aTime) {
  SimulateUntil(aTime);
  return nsPoint(NSToCoordRound(mModelX.GetPosition()),
                 NSToCoordRound(mModelY.GetPosition()));
}

nsSize ScrollAnimationMSDPhysics::VelocityAt(const TimeStamp& aTime) {
  SimulateUntil(aTime);
  return nsSize(NSToCoordRound(mModelX.GetVelocity()),
                NSToCoordRound(mModelY.GetVelocity()));
}

static double ClampVelocityToMaximum(double aVelocity, double aInitialPosition,
                                     double aDestination,
                                     double aSpringConstant) {
  // Clamp velocity to the maximum value it could obtain if we started at this
  // position with zero velocity (see bug 1866904 comment 3). With a damping
  // ratio >= 1.0, this should be low enough to avoid overshooting the
  // destination.
  double velocityLimit =
      sqrt(aSpringConstant) * abs(aDestination - aInitialPosition);
  return clamped(aVelocity, -velocityLimit, velocityLimit);
}

ScrollAnimationMSDPhysics::NonOscillatingAxisPhysicsMSDModel::
    NonOscillatingAxisPhysicsMSDModel(double aInitialPosition,
                                      double aInitialDestination,
                                      double aInitialVelocity,
                                      double aSpringConstant,
                                      double aDampingRatio)
    : AxisPhysicsMSDModel(
          aInitialPosition, aInitialDestination,
          ClampVelocityToMaximum(aInitialVelocity, aInitialPosition,
                                 aInitialDestination, aSpringConstant),
          aSpringConstant, aDampingRatio) {
  MSD_LOG("Constructing axis physics model with parameters %f %f %f %f %f\n",
          aInitialPosition, aInitialDestination, aInitialVelocity,
          aSpringConstant, aDampingRatio);
  MOZ_ASSERT(aDampingRatio >= 1.0,
             "Damping ratio must be >= 1.0 to avoid oscillation");
}
