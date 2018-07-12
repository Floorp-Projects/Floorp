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
  // layout viewport, so don't attempt to make any adjustments.
  if (mViewport.Contains(visualViewport) ||
      (mViewport.Width() < visualViewport.Width() &&
       !FuzzyEqualsMultiplicative(mViewport.Width(), visualViewport.Width())) ||
      (mViewport.Height() < visualViewport.Height() &&
       !FuzzyEqualsMultiplicative(mViewport.Height(), visualViewport.Height()))) {
    return;
  }
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
