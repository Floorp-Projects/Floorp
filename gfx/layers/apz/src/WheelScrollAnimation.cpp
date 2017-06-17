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
  : AsyncScrollBase(aInitialPosition)
  , mApzc(aApzc)
  , mFinalDestination(aInitialPosition)
  , mDeltaType(aDeltaType)
{
}

void
WheelScrollAnimation::Update(TimeStamp aTime, nsPoint aDelta, const nsSize& aCurrentVelocity)
{
  InitPreferences(aTime);

  mFinalDestination += aDelta;

  // Clamp the final destination to the scrollable area.
  CSSPoint clamped = CSSPoint::FromAppUnits(mFinalDestination);
  clamped.x = mApzc.mX.ClampOriginToScrollableRect(clamped.x);
  clamped.y = mApzc.mY.ClampOriginToScrollableRect(clamped.y);
  mFinalDestination = CSSPoint::ToAppUnits(clamped);

  AsyncScrollBase::Update(aTime, mFinalDestination, aCurrentVelocity);
}

bool
WheelScrollAnimation::DoSample(FrameMetrics& aFrameMetrics, const TimeDuration& aDelta)
{
  TimeStamp now = mApzc.GetFrameTime();
  CSSToParentLayerScale2D zoom = aFrameMetrics.GetZoom();

  // If the animation is finished, make sure the final position is correct by
  // using one last displacement. Otherwise, compute the delta via the timing
  // function as normal.
  bool finished = IsFinished(now);
  nsPoint sampledDest = finished
                        ? mDestination
                        : PositionAt(now);
  ParentLayerPoint displacement =
    (CSSPoint::FromAppUnits(sampledDest) - aFrameMetrics.GetScrollOffset()) * zoom;

  if (finished) {
    mApzc.mX.SetVelocity(0);
    mApzc.mY.SetVelocity(0);
  } else if (!IsZero(displacement)) {
    // Velocity is measured in ParentLayerCoords / Milliseconds
    float xVelocity = displacement.x / aDelta.ToMilliseconds();
    float yVelocity = displacement.y / aDelta.ToMilliseconds();
    mApzc.mX.SetVelocity(xVelocity);
    mApzc.mY.SetVelocity(yVelocity);
  }

  // Note: we ignore overscroll for wheel animations.
  ParentLayerPoint adjustedOffset, overscroll;
  mApzc.mX.AdjustDisplacement(displacement.x, adjustedOffset.x, overscroll.x);
  mApzc.mY.AdjustDisplacement(displacement.y, adjustedOffset.y, overscroll.y,
                              !mApzc.mScrollMetadata.AllowVerticalScrollWithWheel());

  // If we expected to scroll, but there's no more scroll range on either axis,
  // then end the animation early. Note that the initial displacement could be 0
  // if the compositor ran very quickly (<1ms) after the animation was created.
  // When that happens we want to make sure the animation continues.
  if (!IsZero(displacement) && IsZero(adjustedOffset)) {
    // Nothing more to do - end the animation.
    return false;
  }

  aFrameMetrics.ScrollBy(adjustedOffset / zoom);
  return !finished;
}

void
WheelScrollAnimation::InitPreferences(TimeStamp aTime)
{
  if (!mIsFirstIteration) {
    return;
  }

  switch (mDeltaType) {
  case ScrollWheelInput::SCROLLDELTA_PAGE:
    mOriginMaxMS = clamped(gfxPrefs::PageSmoothScrollMaxDurationMs(), 0, 10000);
    mOriginMinMS = clamped(gfxPrefs::PageSmoothScrollMinDurationMs(), 0, mOriginMaxMS);
    break;
  case ScrollWheelInput::SCROLLDELTA_PIXEL:
    mOriginMaxMS = clamped(gfxPrefs::PixelSmoothScrollMaxDurationMs(), 0, 10000);
    mOriginMinMS = clamped(gfxPrefs::PixelSmoothScrollMinDurationMs(), 0, mOriginMaxMS);
    break;
  case ScrollWheelInput::SCROLLDELTA_SENTINEL:
    MOZ_FALLTHROUGH_ASSERT("Invalid value");
  case ScrollWheelInput::SCROLLDELTA_LINE:
    mOriginMaxMS = clamped(gfxPrefs::WheelSmoothScrollMaxDurationMs(), 0, 10000);
    mOriginMinMS = clamped(gfxPrefs::WheelSmoothScrollMinDurationMs(), 0, mOriginMaxMS);
    break;
  }

  // The pref is 100-based int percentage, while mIntervalRatio is 1-based ratio
  mIntervalRatio = ((double)gfxPrefs::SmoothScrollDurationToIntervalRatio()) / 100.0;
  mIntervalRatio = std::max(1.0, mIntervalRatio);

  InitializeHistory(aTime);
}

} // namespace layers
} // namespace mozilla
