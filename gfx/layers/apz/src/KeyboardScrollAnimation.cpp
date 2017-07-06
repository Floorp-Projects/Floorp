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
      mOriginMaxMS = clamped(gfxPrefs::LineSmoothScrollMaxDurationMs(), 0, 10000);
      mOriginMinMS = clamped(gfxPrefs::LineSmoothScrollMinDurationMs(), 0, mOriginMaxMS);
      break;
    }
    case KeyboardScrollAction::eScrollPage: {
      mOriginMaxMS = clamped(gfxPrefs::PageSmoothScrollMaxDurationMs(), 0, 10000);
      mOriginMinMS = clamped(gfxPrefs::PageSmoothScrollMinDurationMs(), 0, mOriginMaxMS);
      break;
    }
    case KeyboardScrollAction::eScrollComplete: {
      mOriginMaxMS = clamped(gfxPrefs::OtherSmoothScrollMaxDurationMs(), 0, 10000);
      mOriginMinMS = clamped(gfxPrefs::OtherSmoothScrollMinDurationMs(), 0, mOriginMaxMS);
      break;
    }
  }

  // The pref is 100-based int percentage, while mIntervalRatio is 1-based ratio
  mIntervalRatio = ((double)gfxPrefs::SmoothScrollDurationToIntervalRatio()) / 100.0;
  mIntervalRatio = std::max(1.0, mIntervalRatio);
}

} // namespace layers
} // namespace mozilla
