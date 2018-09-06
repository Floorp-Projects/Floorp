/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_ScrollAnimationBezierPhysics_h_
#define mozilla_layout_ScrollAnimationBezierPhysics_h_

#include "ScrollAnimationPhysics.h"
#include "nsSMILKeySpline.h"

namespace mozilla {

struct ScrollAnimationBezierPhysicsSettings
{
  // These values are minimum and maximum animation duration per event,
  // and a global ratio which defines how longer is the animation's duration
  // compared to the average recent events intervals (such that for a relatively
  // consistent events rate, the next event arrives before current animation ends)
  int32_t mMinMS;
  int32_t mMaxMS;
  double mIntervalRatio;
};

// This class implements a cubic bezier timing function and automatically
// adapts the animation duration based on the scrolling rate.
class ScrollAnimationBezierPhysics final : public ScrollAnimationPhysics
{
public:
  explicit ScrollAnimationBezierPhysics(const nsPoint& aStartPos,
                                        const ScrollAnimationBezierPhysicsSettings& aSettings);

  void Update(const TimeStamp& aTime,
              const nsPoint& aDestination,
              const nsSize& aCurrentVelocity) override;

  // Get the velocity at a point in time in nscoords/sec.
  nsSize VelocityAt(const TimeStamp& aTime) override;

  // Returns the expected scroll position at a given point in time, in app
  // units, relative to the scroll frame.
  nsPoint PositionAt(const TimeStamp& aTime) override;

  bool IsFinished(const TimeStamp& aTime) override {
    return aTime > mStartTime + mDuration;
  }

protected:
  double ProgressAt(const TimeStamp& aTime) const {
    return clamped((aTime - mStartTime) / mDuration, 0.0, 1.0);
  }

  nscoord VelocityComponent(double aTimeProgress,
                            const nsSMILKeySpline& aTimingFunction,
                            nscoord aStart, nscoord aDestination) const;

  // Calculate duration, possibly dynamically according to events rate and
  // event origin. (also maintain previous timestamps - which are only used
  // here).
  TimeDuration ComputeDuration(const TimeStamp& aTime);

  // Initializes the timing function in such a way that the current velocity is
  // preserved.
  void InitTimingFunction(nsSMILKeySpline& aTimingFunction,
                          nscoord aCurrentPos, nscoord aCurrentVelocity,
                          nscoord aDestination);

  // Initialize event history.
  void InitializeHistory(const TimeStamp& aTime);

  // Cached Preferences values.
  ScrollAnimationBezierPhysicsSettings mSettings;

  // mPrevEventTime holds previous 3 timestamps for intervals averaging (to
  // reduce duration fluctuations). When AsyncScroll is constructed and no
  // previous timestamps are available (indicated with mIsFirstIteration),
  // initialize mPrevEventTime using imaginary previous timestamps with maximum
  // relevant intervals between them.
  TimeStamp mPrevEventTime[3];

  TimeStamp mStartTime;

  nsPoint mStartPos;
  nsPoint mDestination;
  TimeDuration mDuration;
  nsSMILKeySpline mTimingFunctionX;
  nsSMILKeySpline mTimingFunctionY;
  bool mIsFirstIteration;
};

} // namespace mozilla

#endif // mozilla_layout_ScrollAnimationBezierPhysics_h_
