/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollAnimationMSDPhysics.h"
#include "mozilla/StaticPrefs_general.h"

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
  mModelX = AxisPhysicsMSDModel(mStartPos.x, aDestination.x,
                                aCurrentVelocity.width, springConstant, 1);
  mModelY = AxisPhysicsMSDModel(mStartPos.y, aDestination.y,
                                aCurrentVelocity.height, springConstant, 1);
  mIsFirstIteration = false;
}

void ScrollAnimationMSDPhysics::ApplyContentShift(const CSSPoint& aShiftDelta) {
  nsPoint shiftDelta = CSSPoint::ToAppUnits(aShiftDelta);
  mStartPos += shiftDelta;
  mDestination += shiftDelta;
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
  if (!mLastSimulatedTime || aTime < mLastSimulatedTime) {
    return;
  }
  TimeDuration delta = aTime - mLastSimulatedTime;
  mModelX.Simulate(delta);
  mModelY.Simulate(delta);
  mLastSimulatedTime = aTime;
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
