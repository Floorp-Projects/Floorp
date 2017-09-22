/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "KeyboardScrollAnimation.h"

#include "gfxPrefs.h"

namespace mozilla {
namespace layers {

KeyboardScrollAnimation::KeyboardScrollAnimation(AsyncPanZoomController& aApzc,
                                                 const nsPoint& aInitialPosition,
                                                 KeyboardScrollAction::KeyboardScrollActionType aType)
  : GenericScrollAnimation(aApzc, aInitialPosition)
{
  switch (aType) {
    case KeyboardScrollAction::eScrollCharacter:
    case KeyboardScrollAction::eScrollLine: {
      mAnimationPhysics.mOriginMaxMS = clamped(gfxPrefs::LineSmoothScrollMaxDurationMs(), 0, 10000);
      mAnimationPhysics.mOriginMinMS = clamped(gfxPrefs::LineSmoothScrollMinDurationMs(), 0, mAnimationPhysics.mOriginMaxMS);
      break;
    }
    case KeyboardScrollAction::eScrollPage: {
      mAnimationPhysics.mOriginMaxMS = clamped(gfxPrefs::PageSmoothScrollMaxDurationMs(), 0, 10000);
      mAnimationPhysics.mOriginMinMS = clamped(gfxPrefs::PageSmoothScrollMinDurationMs(), 0, mAnimationPhysics.mOriginMaxMS);
      break;
    }
    case KeyboardScrollAction::eScrollComplete: {
      mAnimationPhysics.mOriginMaxMS = clamped(gfxPrefs::OtherSmoothScrollMaxDurationMs(), 0, 10000);
      mAnimationPhysics.mOriginMinMS = clamped(gfxPrefs::OtherSmoothScrollMinDurationMs(), 0, mAnimationPhysics.mOriginMaxMS);
      break;
    }
  }

  // The pref is 100-based int percentage, while mIntervalRatio is 1-based ratio
  mAnimationPhysics.mIntervalRatio = ((double)gfxPrefs::SmoothScrollDurationToIntervalRatio()) / 100.0;
  mAnimationPhysics.mIntervalRatio = std::max(1.0, mAnimationPhysics.mIntervalRatio);
}

} // namespace layers
} // namespace mozilla
