/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_ScrollAnimationPhysics_h_
#define mozilla_layout_ScrollAnimationPhysics_h_

#include "mozilla/TimeStamp.h"
#include "nsPoint.h"

namespace mozilla {

class ScrollAnimationPhysics
{
public:
  virtual void Update(const TimeStamp& aTime,
                      const nsPoint& aDestination,
                      const nsSize& aCurrentVelocity) = 0;

  // Get the velocity at a point in time in nscoords/sec.
  virtual nsSize VelocityAt(const TimeStamp& aTime) = 0;

  // Returns the expected scroll position at a given point in time, in app
  // units, relative to the scroll frame.
  virtual nsPoint PositionAt(const TimeStamp& aTime) = 0;

  virtual bool IsFinished(const TimeStamp& aTime) = 0;

  virtual ~ScrollAnimationPhysics() {}
};

// Helper for accelerated wheel deltas. This can be called from the main thread
// or the APZ Controller thread.
static inline double
ComputeAcceleratedWheelDelta(double aDelta, int32_t aCounter, int32_t aFactor)
{
  if (!aDelta) {
    return aDelta;
  }
  return (aDelta * aCounter * double(aFactor) / 10);
}

static const uint32_t kScrollSeriesTimeoutMs = 80; // in milliseconds

} // namespace mozilla

#endif // mozilla_layout_ScrollAnimationPhysics_h_
