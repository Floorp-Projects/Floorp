/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AutoscrollAnimation.h"

#include <cmath>  // for sqrtf()

#include "AsyncPanZoomController.h"
#include "mozilla/Telemetry.h"                  // for Telemetry

namespace mozilla {
namespace layers {

// Helper function for AutoscrollAnimation::DoSample().
// Basically copied as-is from toolkit/content/browser-content.js.
static float
Accelerate(ScreenCoord curr, ScreenCoord start)
{
  static const int speed = 12;
  float val = (curr - start) / speed;
  if (val > 1) {
    return val * sqrtf(val) - 1;
  }
  if (val < -1) {
    return val * sqrtf(-val) + 1;
  }
  return 0;
}

AutoscrollAnimation::AutoscrollAnimation(AsyncPanZoomController& aApzc,
                                         const ScreenPoint& aAnchorLocation)
  : mApzc(aApzc)
  , mAnchorLocation(aAnchorLocation)
{
}

bool
AutoscrollAnimation::DoSample(FrameMetrics& aFrameMetrics, const TimeDuration& aDelta)
{
  APZCTreeManager* treeManager = mApzc.GetApzcTreeManager();
  if (!treeManager) {
    return false;
  }

  ScreenPoint mouseLocation = treeManager->GetCurrentMousePosition();

  // The implementation of this function closely mirrors that of its main-
  // thread equivalent, the autoscrollLoop() function in
  // toolkit/content/browser-content.js.

  // Avoid long jumps when the browser hangs for more than |maxTimeDelta| ms.
  static const TimeDuration maxTimeDelta = TimeDuration::FromMilliseconds(100);
  TimeDuration timeDelta = TimeDuration::Min(aDelta, maxTimeDelta);

  float timeCompensation = timeDelta.ToMilliseconds() / 20;

  // Notes:
  //   - The main-thread implementation rounds the scroll delta to an integer,
  //     and keeps track of the fractional part as an "error". It does this
  //     because it uses Window.scrollBy() or Element.scrollBy() to perform
  //     the scrolling, and those functions truncate the fractional part of
  //     the offset. APZ does no such truncation, so there's no need to keep
  //     track of the fractional part separately.
  //   - The Accelerate() function takes Screen coordinates as inputs, but
  //     its output is interpreted as CSS coordinates. This is intentional,
  //     insofar as autoscrollLoop() does the same thing.
  CSSPoint scrollDelta{
    Accelerate(mouseLocation.x, mAnchorLocation.x) * timeCompensation,
    Accelerate(mouseLocation.y, mAnchorLocation.y) * timeCompensation
  };

  mApzc.ScrollByAndClamp(scrollDelta);

  // An autoscroll animation never ends of its own accord.
  // It can be stopped in response to various input events, in which case
  // AsyncPanZoomController::StopAutoscroll() will stop it via
  // CancelAnimation().
  return true;
}

void
AutoscrollAnimation::Cancel(CancelAnimationFlags aFlags)
{
  // The cancellation was initiated by browser.xml, so there's no need to
  // notify it.
  if (aFlags & TriggeredExternally) {
    return;
  }

  if (RefPtr<GeckoContentController> controller = mApzc.GetGeckoContentController()) {
    controller->CancelAutoscroll(mApzc.GetGuid());
  }
}

} // namespace layers
} // namespace mozilla
