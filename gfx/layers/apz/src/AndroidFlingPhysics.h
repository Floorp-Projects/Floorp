/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AndroidFlingPhysics_h_
#define mozilla_layers_AndroidFlingPhysics_h_

#include "AsyncPanZoomController.h"
#include "Units.h"
#include "gfxPrefs.h"
#include "mozilla/Assertions.h"

namespace mozilla {
namespace layers {

class AndroidFlingPhysics {
public:
  void Init(const ParentLayerPoint& aVelocity, float aPLPPI);
  void Sample(const TimeDuration& aDelta,
              ParentLayerPoint* aOutVelocity,
              ParentLayerPoint* aOutOffset);

  static void InitializeGlobalState();
private:
  // Returns false if the animation should end.
  bool SampleImpl(const TimeDuration& aDelta, float* aOutVelocity);

  // Information pertaining to the current fling.
  // This is initialized on each call to Init().
  ParentLayerCoord mVelocity;  // diagonal velocity (length of velocity vector)
  TimeDuration mTargetDuration;
  TimeDuration mDurationSoFar;
  ParentLayerPoint mLastPos;
  ParentLayerPoint mCurrentPos;
  ParentLayerCoord mTargetDistance;  // diagonal distance
  ParentLayerPoint mTargetPos;  // really a target *offset* relative to the
                                // start position, which we don't track
  ParentLayerPoint mDeltaNorm;  // mTargetPos with length normalized to 1
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_AndroidFlingPhysics_h_
