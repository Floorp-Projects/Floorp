/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScrollVelocityQueue_h_
#define ScrollVelocityQueue_h_

#include "nsTArray.h"
#include "nsPoint.h"
#include "mozilla/TimeStamp.h"

class nsPresContext;

namespace mozilla {
namespace layout {

/**
 * ScrollVelocityQueue is used to determine the current velocity of a
 * scroll frame, derived from scroll position samples.
 *
 * Using the last iteration's scroll position, stored in mLastPosition, a
 * delta of the scroll position is calculated and accumulated in mAccumulator
 * until the refresh driver returns a new timestamp for MostRecentRefresh().
 *
 * When there is a new timestamp from the refresh driver, the accumulated
 * change in scroll position is divided by the delta of the timestamp to
 * get an average velocity over that period.  This velocity is pushed into
 * mQueue as a std::pair associating each velocity with the
 * duration over which it was sampled.
 *
 * Samples are removed from mQueue, leaving only those necessary to determine
 * the average velocity over the recent relevant period, which has a duration
 * set by the apz.velocity_relevance_time_ms preference.
 *
 * The velocity of each sample is clamped to a value set by the
 * layout.css.scroll-snap.prediction-max-velocity.
 *
 * As the average velocity will later be integrated over a duration set by
 * the layout.css.scroll-snap.prediction-sensitivity preference and the
 * velocity samples are clamped to a set value, the maximum expected scroll
 * offset can be calculated.  This maximum offset is used to clamp
 * mAccumulator, eliminating samples that would otherwise result in scroll
 * snap position selection that is not consistent with the user's perception
 * of scroll velocity.
 */

class ScrollVelocityQueue final {
public:
  explicit ScrollVelocityQueue(nsPresContext *aPresContext)
    : mPresContext(aPresContext) {}

  // Sample() is to be called periodically when scroll movement occurs, to
  // record samples of scroll position used later by GetVelocity().
  void Sample(const nsPoint& aScrollPosition);

  // Discards velocity samples, resulting in velocity of 0 returned by
  // GetVelocity until move scroll position updates.
  void Reset();

  // Get scroll velocity averaged from recent movement, in appunits / second
  nsPoint GetVelocity();
private:
  // A queue of (duration, velocity) pairs; these are the historical average
  // velocities over the given durations.  Durations are in milliseconds,
  // velocities are in app units per second.
  nsTArray<std::pair<uint32_t, nsPoint> > mQueue;

  // Accumulates the distance and direction travelled by the scroll frame since
  // mSampleTime.
  nsPoint mAccumulator;

  // Time that mAccumulator was last reset and began accumulating.
  TimeStamp mSampleTime;

  // Scroll offset at the mAccumulator was last reset and began
  // accumulating.
  nsPoint mLastPosition;

  // PresContext of the containing frame, used to get timebase
  nsPresContext* mPresContext;

  // Remove samples from mQueue that no longer contribute to GetVelocity()
  // due to their age
  void TrimQueue();
};

} // namespace layout
} // namespace mozilla

#endif  /* !defined(ScrollVelocityQueue_h_) */
