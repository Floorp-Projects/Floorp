/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_FlingAccelerator_h
#define mozilla_layers_FlingAccelerator_h

#include "mozilla/layers/SampleTime.h"
#include "Units.h"

namespace mozilla {
namespace layers {

struct FlingHandoffState;

/**
 * This class is used to track state that is used when determining whether a
 * fling should be accelerated.
 */
class FlingAccelerator final {
 public:
  FlingAccelerator() {}

  // Resets state so that the next fling will not be accelerated.
  void Reset();

  // Returns false after a reset or before the first fling.
  bool IsTracking() const { return mIsTracking; }

  // Starts a new fling, and returns the (potentially accelerated) velocity that
  // should be used for that fling.
  ParentLayerPoint GetFlingStartingVelocity(
      const SampleTime& aNow, const ParentLayerPoint& aVelocity,
      const FlingHandoffState& aHandoffState);

  void ObserveFlingCanceled(const ParentLayerPoint& aVelocity) {
    mPreviousFlingCancelVelocity = aVelocity;
  }

 protected:
  bool ShouldAccelerate(const SampleTime& aNow,
                        const ParentLayerPoint& aVelocity,
                        const FlingHandoffState& aHandoffState) const;

  // The initial velocity of the most recent fling.
  ParentLayerPoint mPreviousFlingStartingVelocity;
  // The velocity that the previous fling animation had at the point it was
  // interrupted.
  ParentLayerPoint mPreviousFlingCancelVelocity;
  // Whether the upcoming fling is eligible for acceleration.
  bool mIsTracking = false;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_FlingAccelerator_h
