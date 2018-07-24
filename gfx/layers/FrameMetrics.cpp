/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FrameMetrics.h"
#include "gfxPrefs.h"
#include "nsStyleConsts.h"

namespace mozilla {
namespace layers {

const FrameMetrics::ViewID FrameMetrics::NULL_SCROLL_ID = 0;

void
FrameMetrics::RecalculateViewportOffset()
{
  if (!mIsRootContent) {
    return;
  }
  CSSRect visualViewport = GetVisualViewport();
  // If the visual viewport is contained within the layout viewport, we don't
  // need to make any adjustments, so we can exit early.
  //
  // Additionally, if the composition bounds changes (due to an orientation
  // change, window resize, etc.), it may take a few frames for mViewport to
  // update and during that time, the visual viewport may be larger than the
  // layout viewport. In such situations, we take an early exit if the visual
  // viewport contains the layout viewport.
  if (mViewport.Contains(visualViewport) || visualViewport.Contains(mViewport)) {
    return;
  }

  // If visual viewport size is greater than the layout viewport, move the layout
  // viewport such that it remains inside the visual viewport. Otherwise,
  // move the layout viewport such that the visual viewport is contained
  // inside the layout viewport.
  if ((mViewport.Width() < visualViewport.Width() &&
        !FuzzyEqualsMultiplicative(mViewport.Width(), visualViewport.Width())) ||
       (mViewport.Height() < visualViewport.Height() &&
        !FuzzyEqualsMultiplicative(mViewport.Height(), visualViewport.Height()))) {

     if (mViewport.X() < visualViewport.X()) {
        // layout viewport moves right
        mViewport.MoveToX(visualViewport.X());
     } else if (visualViewport.XMost() < mViewport.XMost()) {
        // layout viewport moves left
        mViewport.MoveByX(visualViewport.XMost() - mViewport.XMost());
     }
     if (mViewport.Y() < visualViewport.Y()) {
        // layout viewport moves down
        mViewport.MoveToY(visualViewport.Y());
     } else if (visualViewport.YMost() < mViewport.YMost()) {
        // layout viewport moves up
        mViewport.MoveByY(visualViewport.YMost() - mViewport.YMost());
     }
   } else {

     if (visualViewport.X() < mViewport.X()) {
        mViewport.MoveToX(visualViewport.X());
     } else if (mViewport.XMost() < visualViewport.XMost()) {
        mViewport.MoveByX(visualViewport.XMost() - mViewport.XMost());
     }
     if (visualViewport.Y() < mViewport.Y()) {
        mViewport.MoveToY(visualViewport.Y());
     } else if (mViewport.YMost() < visualViewport.YMost()) {
        mViewport.MoveByY(visualViewport.YMost() - mViewport.YMost());
     }
   }
}

void
ScrollMetadata::SetUsesContainerScrolling(bool aValue) {
  MOZ_ASSERT_IF(aValue, gfxPrefs::LayoutUseContainersForRootFrames());
  mUsesContainerScrolling = aValue;
}

static OverscrollBehavior
ToOverscrollBehavior(StyleOverscrollBehavior aBehavior)
{
  switch (aBehavior) {
  case StyleOverscrollBehavior::Auto:
    return OverscrollBehavior::Auto;
  case StyleOverscrollBehavior::Contain:
    return OverscrollBehavior::Contain;
  case StyleOverscrollBehavior::None:
    return OverscrollBehavior::None;
  }
  MOZ_ASSERT_UNREACHABLE("Invalid overscroll behavior");
  return OverscrollBehavior::Auto;
}

OverscrollBehaviorInfo
OverscrollBehaviorInfo::FromStyleConstants(StyleOverscrollBehavior aBehaviorX,
                                           StyleOverscrollBehavior aBehaviorY)
{
  OverscrollBehaviorInfo result;
  result.mBehaviorX = ToOverscrollBehavior(aBehaviorX);
  result.mBehaviorY = ToOverscrollBehavior(aBehaviorY);
  return result;
}

StaticAutoPtr<const ScrollMetadata> ScrollMetadata::sNullMetadata;

}
}
