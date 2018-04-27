/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_DesktopFlingPhysics_h_
#define mozilla_layers_DesktopFlingPhysics_h_

#include "AsyncPanZoomController.h"
#include "Units.h"
#include "gfxPrefs.h"
#include "mozilla/Assertions.h"

#define FLING_PHYS_LOG(...)
// #define FLING_PHYS_LOG(...) printf_stderr("FLING: " __VA_ARGS__)

namespace mozilla {
namespace layers {

class DesktopFlingPhysics {
public:
  void Init(const ParentLayerPoint& aStartingVelocity, float aPLPPI /* unused */)
  {
    mVelocity = aStartingVelocity;
  }
  void Sample(const TimeDuration& aDelta,
              ParentLayerPoint* aOutVelocity,
              ParentLayerPoint* aOutOffset)
  {
    float friction = gfxPrefs::APZFlingFriction();
    float threshold = gfxPrefs::APZFlingStoppedThreshold();

    mVelocity = ParentLayerPoint(
        ApplyFrictionOrCancel(mVelocity.x, aDelta, friction, threshold),
        ApplyFrictionOrCancel(mVelocity.y, aDelta, friction, threshold));

    *aOutVelocity = mVelocity;
    *aOutOffset = mVelocity * aDelta.ToMilliseconds();
  }
private:
  /**
   * Applies friction to the given velocity and returns the result, or
   * returns zero if the velocity is too low.
   * |aVelocity| is the incoming velocity.
   * |aDelta| is the amount of time that has passed since the last time
   * friction was applied.
   * |aFriction| is the amount of friction to apply.
   * |aThreshold| is the velocity below which the fling is cancelled.
   */
  static float ApplyFrictionOrCancel(float aVelocity, const TimeDuration& aDelta,
                                     float aFriction, float aThreshold)
  {
    if (fabsf(aVelocity) <= aThreshold) {
      // If the velocity is very low, just set it to 0 and stop the fling,
      // otherwise we'll just asymptotically approach 0 and the user won't
      // actually see any changes.
      return 0.0f;
    }

    aVelocity *= pow(1.0f - aFriction, float(aDelta.ToMilliseconds()));
    return aVelocity;
  }

  ParentLayerPoint mVelocity;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_DesktopFlingPhysics_h_
