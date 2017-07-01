/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WheelScrollAnimation.h"

#include "AsyncPanZoomController.h"
#include "gfxPrefs.h"
#include "nsPoint.h"

namespace mozilla {
namespace layers {

WheelScrollAnimation::WheelScrollAnimation(AsyncPanZoomController& aApzc,
                                           const nsPoint& aInitialPosition,
                                           ScrollWheelInput::ScrollDeltaType aDeltaType)
  : GenericScrollAnimation(aApzc, aInitialPosition)
{
  mForceVerticalOverscroll = !mApzc.mScrollMetadata.AllowVerticalScrollWithWheel();

  switch (aDeltaType) {
  case ScrollWheelInput::SCROLLDELTA_PAGE:
    mOriginMaxMS = clamped(gfxPrefs::PageSmoothScrollMaxDurationMs(), 0, 10000);
    mOriginMinMS = clamped(gfxPrefs::PageSmoothScrollMinDurationMs(), 0, mOriginMaxMS);
    break;
  case ScrollWheelInput::SCROLLDELTA_PIXEL:
    mOriginMaxMS = clamped(gfxPrefs::PixelSmoothScrollMaxDurationMs(), 0, 10000);
    mOriginMinMS = clamped(gfxPrefs::PixelSmoothScrollMinDurationMs(), 0, mOriginMaxMS);
    break;
  case ScrollWheelInput::SCROLLDELTA_LINE:
    mOriginMaxMS = clamped(gfxPrefs::WheelSmoothScrollMaxDurationMs(), 0, 10000);
    mOriginMinMS = clamped(gfxPrefs::WheelSmoothScrollMinDurationMs(), 0, mOriginMaxMS);
    break;
  }

  // The pref is 100-based int percentage, while mIntervalRatio is 1-based ratio
  mIntervalRatio = ((double)gfxPrefs::SmoothScrollDurationToIntervalRatio()) / 100.0;
  mIntervalRatio = std::max(1.0, mIntervalRatio);
}

} // namespace layers
} // namespace mozilla
