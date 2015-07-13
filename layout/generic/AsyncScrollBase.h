/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_AsyncScrollBase_h_
#define mozilla_layout_AsyncScrollBase_h_

#include "mozilla/TimeStamp.h"
#include "nsPoint.h"
#include "nsSMILKeySpline.h"

namespace mozilla {

// This is the base class for driving scroll wheel animation on both the
// compositor and main thread.
class AsyncScrollBase
{
public:
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  explicit AsyncScrollBase(nsPoint aStartPos);

  void Update(TimeStamp aTime,
              nsPoint aDestination,
              const nsSize& aCurrentVelocity);

  // Get the velocity at a point in time in nscoords/sec.
  nsSize VelocityAt(TimeStamp aTime) const;

  // Returns the expected scroll position at a given point in time, in app
  // units, relative to the scroll frame.
  nsPoint PositionAt(TimeStamp aTime) const;

  bool IsFinished(TimeStamp aTime) {
    return aTime > mStartTime + mDuration;
  }

protected:
  double ProgressAt(TimeStamp aTime) const {
    return clamped((aTime - mStartTime) / mDuration, 0.0, 1.0);
  }

  nscoord VelocityComponent(double aTimeProgress,
                            const nsSMILKeySpline& aTimingFunction,
                            nscoord aStart, nscoord aDestination) const;

  // Calculate duration, possibly dynamically according to events rate and
  // event origin. (also maintain previous timestamps - which are only used
  // here).
  TimeDuration ComputeDuration(TimeStamp aTime);

  // Initialize event history.
  void InitializeHistory(TimeStamp aTime);

  // Initializes the timing function in such a way that the current velocity is
  // preserved.
  void InitTimingFunction(nsSMILKeySpline& aTimingFunction,
                          nscoord aCurrentPos, nscoord aCurrentVelocity,
                          nscoord aDestination);

  // mPrevEventTime holds previous 3 timestamps for intervals averaging (to
  // reduce duration fluctuations). When AsyncScroll is constructed and no
  // previous timestamps are available (indicated with mIsFirstIteration),
  // initialize mPrevEventTime using imaginary previous timestamps with maximum
  // relevant intervals between them.
  TimeStamp mPrevEventTime[3];
  bool mIsFirstIteration;

  TimeStamp mStartTime;

  // Cached Preferences value.
  //
  // These values are minimum and maximum animation duration per event origin,
  // and a global ratio which defines how longer is the animation's duration
  // compared to the average recent events intervals (such that for a relatively
  // consistent events rate, the next event arrives before current animation ends)
  int32_t mOriginMinMS;
  int32_t mOriginMaxMS;
  double mIntervalRatio;

  nsPoint mStartPos;
  TimeDuration mDuration;
  nsPoint mDestination;
  nsSMILKeySpline mTimingFunctionX;
  nsSMILKeySpline mTimingFunctionY;
};

} // namespace mozilla

#endif // mozilla_layout_AsyncScrollBase_h_
