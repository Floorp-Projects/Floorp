/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WheelScrollAnimation.h"

namespace mozilla {
namespace layers {

WheelScrollAnimation::WheelScrollAnimation(AsyncPanZoomController& aApzc, const nsPoint& aInitialPosition)
  : AsyncPanZoomAnimation(TimeDuration::FromMilliseconds(gfxPrefs::APZSmoothScrollRepaintInterval()))
  , AsyncScrollBase(aInitialPosition)
  , mApzc(aApzc)
  , mFinalDestination(aInitialPosition)
{
}

void
WheelScrollAnimation::Update(TimeStamp aTime, nsPoint aDelta, const nsSize& aCurrentVelocity)
{
  InitPreferences(aTime);
  mFinalDestination += aDelta;
  AsyncScrollBase::Update(aTime, mFinalDestination, aCurrentVelocity);
}

bool
WheelScrollAnimation::DoSample(FrameMetrics& aFrameMetrics, const TimeDuration& aDelta)
{
  TimeStamp now = AsyncPanZoomController::GetFrameTime();
  if (IsFinished(now)) {
    return false;
  }

  CSSToParentLayerScale2D zoom = aFrameMetrics.GetZoom();

  nsPoint position = PositionAt(now);
  ParentLayerPoint displacement =
    (CSSPoint::FromAppUnits(position) - aFrameMetrics.GetScrollOffset()) * zoom;

  // Note: we ignore overscroll for wheel animations.
  ParentLayerPoint adjustedOffset, overscroll;
  mApzc.mX.AdjustDisplacement(displacement.x, adjustedOffset.x, overscroll.x);
  mApzc.mY.AdjustDisplacement(displacement.y, adjustedOffset.y, overscroll.y,
                              !aFrameMetrics.AllowVerticalScrollWithWheel());

  aFrameMetrics.ScrollBy(adjustedOffset / zoom);
  return true;
}

void
WheelScrollAnimation::InitPreferences(TimeStamp aTime)
{
  if (!mIsFirstIteration) {
    return;
  }

  mOriginMaxMS = clamped(gfxPrefs::WheelSmoothScrollMaxDurationMs(), 0, 10000);
  mOriginMinMS = clamped(gfxPrefs::WheelSmoothScrollMinDurationMs(), 0, mOriginMaxMS);

  mIntervalRatio = (gfxPrefs::SmoothScrollDurationToIntervalRatio() * 100) / 100.0;
  mIntervalRatio = std::max(1.0, mIntervalRatio);

  if (mIsFirstIteration) {
    InitializeHistory(aTime);
  }
}

} // namespace layers
} // namespace mozilla
