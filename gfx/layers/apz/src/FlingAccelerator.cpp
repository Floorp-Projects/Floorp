/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FlingAccelerator.h"

#include "mozilla/StaticPrefs_apz.h"

#include "GenericFlingAnimation.h"  // for FLING_LOG and FlingHandoffState

namespace mozilla {
namespace layers {

void FlingAccelerator::Reset() {
  mPreviousFlingStartingVelocity = ParentLayerPoint{};
  mPreviousFlingCancelVelocity = ParentLayerPoint{};
  mIsTracking = false;
}

static bool SameDirection(float aVelocity1, float aVelocity2) {
  return (aVelocity1 == 0.0f) || (aVelocity2 == 0.0f) ||
         (IsNegative(aVelocity1) == IsNegative(aVelocity2));
}

static float Accelerate(float aBase, float aSupplemental) {
  return (aBase * StaticPrefs::apz_fling_accel_base_mult()) +
         (aSupplemental * StaticPrefs::apz_fling_accel_supplemental_mult());
}

ParentLayerPoint FlingAccelerator::GetFlingStartingVelocity(
    const SampleTime& aNow, const ParentLayerPoint& aVelocity,
    const FlingHandoffState& aHandoffState) {
  // If the fling should be accelerated and is in the same direction as the
  // previous fling, boost the velocity to be the sum of the two. Check separate
  // axes separately because we could have two vertical flings with small
  // horizontal components on the opposite side of zero, and we still want the
  // y-fling to get accelerated.
  ParentLayerPoint velocity = aVelocity;
  if (ShouldAccelerate(aNow, aVelocity, aHandoffState)) {
    if (velocity.x != 0 &&
        SameDirection(velocity.x, mPreviousFlingStartingVelocity.x)) {
      velocity.x = Accelerate(velocity.x, mPreviousFlingStartingVelocity.x);
      FLING_LOG("%p Applying fling x-acceleration from %f to %f (delta %f)\n",
                this, aVelocity.x.value, velocity.x.value,
                mPreviousFlingStartingVelocity.x.value);
    }
    if (velocity.y != 0 &&
        SameDirection(velocity.y, mPreviousFlingStartingVelocity.y)) {
      velocity.y = Accelerate(velocity.y, mPreviousFlingStartingVelocity.y);
      FLING_LOG("%p Applying fling y-acceleration from %f to %f (delta %f)\n",
                this, aVelocity.y.value, velocity.y.value,
                mPreviousFlingStartingVelocity.y.value);
    }
  }

  Reset();

  mPreviousFlingStartingVelocity = velocity;
  mIsTracking = true;

  return velocity;
}

bool FlingAccelerator::ShouldAccelerate(
    const SampleTime& aNow, const ParentLayerPoint& aVelocity,
    const FlingHandoffState& aHandoffState) const {
  if (!IsTracking()) {
    FLING_LOG("%p Fling accelerator was reset, not accelerating.\n", this);
    return false;
  }

  if (!aHandoffState.mTouchStartRestingTime) {
    FLING_LOG("%p Don't have a touch start resting time, not accelerating.\n",
              this);
    return false;
  }

  double msBetweenTouchStartAndPanStart =
      aHandoffState.mTouchStartRestingTime->ToMilliseconds();
  FLING_LOG(
      "%p ShouldAccelerate with pan velocity %f pixels/ms, min pan velocity %f "
      "pixels/ms, previous fling cancel velocity %f pixels/ms, time elapsed "
      "since starting previous time between touch start and pan "
      "start %fms.\n",
      this, float(aVelocity.Length()), float(aHandoffState.mMinPanVelocity),
      float(mPreviousFlingCancelVelocity.Length()),
      float(msBetweenTouchStartAndPanStart));

  if (aVelocity.Length() < StaticPrefs::apz_fling_accel_min_fling_velocity()) {
    FLING_LOG("%p Fling velocity too low (%f), not accelerating.\n", this,
              float(aVelocity.Length()));
    return false;
  }

  if (aHandoffState.mMinPanVelocity <
      StaticPrefs::apz_fling_accel_min_pan_velocity()) {
    FLING_LOG(
        "%p Panning velocity was too slow at some point during the pan (%f), "
        "not accelerating.\n",
        this, float(aHandoffState.mMinPanVelocity));
    return false;
  }

  if (mPreviousFlingCancelVelocity.Length() <
      StaticPrefs::apz_fling_accel_min_fling_velocity()) {
    FLING_LOG(
        "%p The previous fling animation had slowed down too much when it was "
        "interrupted (%f), not accelerating.\n",
        this, float(mPreviousFlingCancelVelocity.Length()));
    return false;
  }

  if (msBetweenTouchStartAndPanStart >=
      StaticPrefs::apz_fling_accel_max_pause_interval_ms()) {
    FLING_LOG(
        "%p Too much time (%fms) elapsed between touch start and pan start, "
        "not accelerating.\n",
        this, msBetweenTouchStartAndPanStart);
    return false;
  }

  return true;
}

}  // namespace layers
}  // namespace mozilla
