/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_ScrollAnimationPhysics_h_
#define mozilla_layout_ScrollAnimationPhysics_h_

#include "mozilla/TimeStamp.h"
#include "nsPoint.h"
#include "Units.h"

namespace mozilla {

class ScrollAnimationPhysics {
 public:
  // Update the animation to have |aDestination| as its new destination.
  // The animation's current position remains unchanged, and the shape
  // of the animation curve is recomputed between the current position
  // and the new destination.
  // This is used in cases where an input event that would cause another
  // animation of this kind is received while this animation is running.
  virtual void Update(const TimeStamp& aTime, const nsPoint& aDestination,
                      const nsSize& aCurrentVelocity) = 0;

  // Shift both the current position and the destination of the animation
  // by |aShiftDelta|. The progress of the animation along its animation
  // curve is unchanged.
  // This is used in cases where the main thread changes the scroll offset
  // (e.g. via scrollBy()) but we want the "momentum" represented by the
  // animation to be preserved.
  virtual void ApplyContentShift(const CSSPoint& aShiftDelta) = 0;

  // Get the velocity at a point in time in nscoords/sec.
  virtual nsSize VelocityAt(const TimeStamp& aTime) = 0;

  // Returns the expected scroll position at a given point in time, in app
  // units, relative to the scroll frame.
  virtual nsPoint PositionAt(const TimeStamp& aTime) = 0;

  virtual bool IsFinished(const TimeStamp& aTime) = 0;

  virtual ~ScrollAnimationPhysics() = default;
};

// Helper for accelerated wheel deltas. This can be called from the main thread
// or the APZ Controller thread.
static inline double ComputeAcceleratedWheelDelta(double aDelta,
                                                  int32_t aCounter,
                                                  int32_t aFactor) {
  if (!aDelta) {
    return aDelta;
  }
  return (aDelta * aCounter * double(aFactor) / 10);
}

}  // namespace mozilla

#endif  // mozilla_layout_ScrollAnimationPhysics_h_
