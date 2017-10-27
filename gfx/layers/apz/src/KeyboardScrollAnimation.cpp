/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "KeyboardScrollAnimation.h"
#include "ScrollAnimationBezierPhysics.h"

#include "gfxPrefs.h"

namespace mozilla {
namespace layers {

static ScrollAnimationBezierPhysicsSettings
SettingsForType(KeyboardScrollAction::KeyboardScrollActionType aType)
{
  int32_t minMS = 0;
  int32_t maxMS = 0;

  switch (aType) {
    case KeyboardScrollAction::eScrollCharacter:
    case KeyboardScrollAction::eScrollLine: {
      maxMS = clamped(gfxPrefs::LineSmoothScrollMaxDurationMs(), 0, 10000);
      minMS = clamped(gfxPrefs::LineSmoothScrollMinDurationMs(), 0, maxMS);
      break;
    }
    case KeyboardScrollAction::eScrollPage: {
      maxMS = clamped(gfxPrefs::PageSmoothScrollMaxDurationMs(), 0, 10000);
      minMS = clamped(gfxPrefs::PageSmoothScrollMinDurationMs(), 0, maxMS);
      break;
    }
    case KeyboardScrollAction::eScrollComplete: {
      maxMS = clamped(gfxPrefs::OtherSmoothScrollMaxDurationMs(), 0, 10000);
      minMS = clamped(gfxPrefs::OtherSmoothScrollMinDurationMs(), 0, maxMS);
      break;
    }
  }

  // The pref is 100-based int percentage, while mIntervalRatio is 1-based ratio
  double intervalRatio = ((double)gfxPrefs::SmoothScrollDurationToIntervalRatio()) / 100.0;
  intervalRatio = std::max(1.0, intervalRatio);
  return ScrollAnimationBezierPhysicsSettings { minMS, maxMS, intervalRatio };
}

KeyboardScrollAnimation::KeyboardScrollAnimation(AsyncPanZoomController& aApzc,
                                                 const nsPoint& aInitialPosition,
                                                 KeyboardScrollAction::KeyboardScrollActionType aType)
  : GenericScrollAnimation(aApzc, aInitialPosition, SettingsForType(aType))
{
}

} // namespace layers
} // namespace mozilla
