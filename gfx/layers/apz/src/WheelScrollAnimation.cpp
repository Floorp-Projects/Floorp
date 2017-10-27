/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WheelScrollAnimation.h"
#include "ScrollAnimationBezierPhysics.h"

#include "AsyncPanZoomController.h"
#include "gfxPrefs.h"
#include "nsPoint.h"

namespace mozilla {
namespace layers {

static ScrollAnimationBezierPhysicsSettings
SettingsForDeltaType(ScrollWheelInput::ScrollDeltaType aDeltaType)
{
  int32_t minMS = 0;
  int32_t maxMS = 0;

  switch (aDeltaType) {
    case ScrollWheelInput::SCROLLDELTA_PAGE:
      maxMS = clamped(gfxPrefs::PageSmoothScrollMaxDurationMs(), 0, 10000);
      minMS = clamped(gfxPrefs::PageSmoothScrollMinDurationMs(), 0, maxMS);
      break;
    case ScrollWheelInput::SCROLLDELTA_PIXEL:
      maxMS = clamped(gfxPrefs::PixelSmoothScrollMaxDurationMs(), 0, 10000);
      minMS = clamped(gfxPrefs::PixelSmoothScrollMinDurationMs(), 0, maxMS);
      break;
    case ScrollWheelInput::SCROLLDELTA_LINE:
      maxMS = clamped(gfxPrefs::WheelSmoothScrollMaxDurationMs(), 0, 10000);
      minMS = clamped(gfxPrefs::WheelSmoothScrollMinDurationMs(), 0, maxMS);
      break;
  }

  // The pref is 100-based int percentage, while mIntervalRatio is 1-based ratio
  double intervalRatio = ((double)gfxPrefs::SmoothScrollDurationToIntervalRatio()) / 100.0;
  intervalRatio = std::max(1.0, intervalRatio);
  return ScrollAnimationBezierPhysicsSettings { minMS, maxMS, intervalRatio };
}

WheelScrollAnimation::WheelScrollAnimation(AsyncPanZoomController& aApzc,
                                           const nsPoint& aInitialPosition,
                                           ScrollWheelInput::ScrollDeltaType aDeltaType)
  : GenericScrollAnimation(aApzc, aInitialPosition, SettingsForDeltaType(aDeltaType))
{
  mForceVerticalOverscroll = !mApzc.mScrollMetadata.AllowVerticalScrollWithWheel();
}

} // namespace layers
} // namespace mozilla
