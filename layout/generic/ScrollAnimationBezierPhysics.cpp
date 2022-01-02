/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollAnimationBezierPhysics.h"
#include "mozilla/StaticPrefs_general.h"

using namespace mozilla;

ScrollAnimationBezierPhysics::ScrollAnimationBezierPhysics(
    const nsPoint& aStartPos,
    const ScrollAnimationBezierPhysicsSettings& aSettings)
    : mSettings(aSettings), mStartPos(aStartPos), mIsFirstIteration(true) {}

void ScrollAnimationBezierPhysics::Update(const TimeStamp& aTime,
                                          const nsPoint& aDestination,
                                          const nsSize& aCurrentVelocity) {
  if (mIsFirstIteration) {
    InitializeHistory(aTime);
  }

  TimeDuration duration = ComputeDuration(aTime);
  nsSize currentVelocity = aCurrentVelocity;

  if (!mIsFirstIteration) {
    // If an additional event has not changed the destination, then do not let
    // another minimum duration reset slow things down.  If it would then
    // instead continue with the existing timing function.
    if (aDestination == mDestination &&
        aTime + duration > mStartTime + mDuration) {
      return;
    }

    currentVelocity = VelocityAt(aTime);
    mStartPos = PositionAt(aTime);
  }

  mStartTime = aTime;
  mDuration = duration;
  mDestination = aDestination;
  InitTimingFunction(mTimingFunctionX, mStartPos.x, currentVelocity.width,
                     aDestination.x);
  InitTimingFunction(mTimingFunctionY, mStartPos.y, currentVelocity.height,
                     aDestination.y);
  mIsFirstIteration = false;
}

void ScrollAnimationBezierPhysics::ApplyContentShift(
    const CSSPoint& aShiftDelta) {
  nsPoint shiftDelta = CSSPoint::ToAppUnits(aShiftDelta);
  mStartPos += shiftDelta;
  mDestination += shiftDelta;
}

TimeDuration ScrollAnimationBezierPhysics::ComputeDuration(
    const TimeStamp& aTime) {
  // Average last 3 delta durations (rounding errors up to 2ms are negligible
  // for us)
  int32_t eventsDeltaMs = (aTime - mPrevEventTime[2]).ToMilliseconds() / 3;
  mPrevEventTime[2] = mPrevEventTime[1];
  mPrevEventTime[1] = mPrevEventTime[0];
  mPrevEventTime[0] = aTime;

  // Modulate duration according to events rate (quicker events -> shorter
  // durations). The desired effect is to use longer duration when scrolling
  // slowly, such that it's easier to follow, but reduce the duration to make it
  // feel more snappy when scrolling quickly. To reduce fluctuations of the
  // duration, we average event intervals using the recent 4 timestamps (now +
  // three prev -> 3 intervals).
  int32_t durationMS =
      clamped<int32_t>(eventsDeltaMs * mSettings.mIntervalRatio,
                       mSettings.mMinMS, mSettings.mMaxMS);

  return TimeDuration::FromMilliseconds(durationMS);
}

void ScrollAnimationBezierPhysics::InitializeHistory(const TimeStamp& aTime) {
  // Starting a new scroll (i.e. not when extending an existing scroll
  // animation), create imaginary prev timestamps with maximum relevant
  // intervals between them.

  // Longest relevant interval (which results in maximum duration)
  TimeDuration maxDelta = TimeDuration::FromMilliseconds(
      mSettings.mMaxMS / mSettings.mIntervalRatio);
  mPrevEventTime[0] = aTime - maxDelta;
  mPrevEventTime[1] = mPrevEventTime[0] - maxDelta;
  mPrevEventTime[2] = mPrevEventTime[1] - maxDelta;
}

void ScrollAnimationBezierPhysics::InitTimingFunction(
    SMILKeySpline& aTimingFunction, nscoord aCurrentPos,
    nscoord aCurrentVelocity, nscoord aDestination) {
  if (aDestination == aCurrentPos ||
      StaticPrefs::general_smoothScroll_currentVelocityWeighting() == 0) {
    aTimingFunction.Init(
        0, 0, 1 - StaticPrefs::general_smoothScroll_stopDecelerationWeighting(),
        1);
    return;
  }

  const TimeDuration oneSecond = TimeDuration::FromSeconds(1);
  double slope =
      aCurrentVelocity * (mDuration / oneSecond) / (aDestination - aCurrentPos);
  double normalization = sqrt(1.0 + slope * slope);
  double dt = 1.0 / normalization *
              StaticPrefs::general_smoothScroll_currentVelocityWeighting();
  double dxy = slope / normalization *
               StaticPrefs::general_smoothScroll_currentVelocityWeighting();
  aTimingFunction.Init(
      dt, dxy,
      1 - StaticPrefs::general_smoothScroll_stopDecelerationWeighting(), 1);
}

nsPoint ScrollAnimationBezierPhysics::PositionAt(const TimeStamp& aTime) {
  if (IsFinished(aTime)) {
    return mDestination;
  }

  double progressX = mTimingFunctionX.GetSplineValue(ProgressAt(aTime));
  double progressY = mTimingFunctionY.GetSplineValue(ProgressAt(aTime));
  return nsPoint(NSToCoordRound((1 - progressX) * mStartPos.x +
                                progressX * mDestination.x),
                 NSToCoordRound((1 - progressY) * mStartPos.y +
                                progressY * mDestination.y));
}

nsSize ScrollAnimationBezierPhysics::VelocityAt(const TimeStamp& aTime) {
  if (IsFinished(aTime)) {
    return nsSize(0, 0);
  }

  double timeProgress = ProgressAt(aTime);
  return nsSize(VelocityComponent(timeProgress, mTimingFunctionX, mStartPos.x,
                                  mDestination.x),
                VelocityComponent(timeProgress, mTimingFunctionY, mStartPos.y,
                                  mDestination.y));
}

nscoord ScrollAnimationBezierPhysics::VelocityComponent(
    double aTimeProgress, const SMILKeySpline& aTimingFunction, nscoord aStart,
    nscoord aDestination) const {
  double dt, dxy;
  aTimingFunction.GetSplineDerivativeValues(aTimeProgress, dt, dxy);
  if (dt == 0) return dxy >= 0 ? nscoord_MAX : nscoord_MIN;

  const TimeDuration oneSecond = TimeDuration::FromSeconds(1);
  double slope = dxy / dt;
  return NSToCoordRound(slope * (aDestination - aStart) /
                        (mDuration / oneSecond));
}
