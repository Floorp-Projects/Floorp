/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FrameMetrics.h"

#include "gfxUtils.h"
#include "nsStyleConsts.h"
#include "nsStyleStruct.h"
#include "mozilla/WritingModes.h"
#include "mozilla/gfx/Types.h"

namespace mozilla {
namespace layers {

const ScrollableLayerGuid::ViewID ScrollableLayerGuid::NULL_SCROLL_ID = 0;

void FrameMetrics::RecalculateLayoutViewportOffset() {
  if (!mIsRootContent) {
    return;
  }
  KeepLayoutViewportEnclosingVisualViewport(GetVisualViewport(),
                                            mScrollableRect, mLayoutViewport);
}

/* static */
void FrameMetrics::KeepLayoutViewportEnclosingVisualViewport(
    const CSSRect& aVisualViewport, const CSSRect& aScrollableRect,
    CSSRect& aLayoutViewport) {
  // If the visual viewport is contained within the layout viewport, we don't
  // need to make any adjustments, so we can exit early.
  //
  // Additionally, if the composition bounds changes (due to an orientation
  // change, window resize, etc.), it may take a few frames for aLayoutViewport
  // to update and during that time, the visual viewport may be larger than the
  // layout viewport. In such situations, we take an early exit if the visual
  // viewport contains the layout viewport.
  if (aLayoutViewport.Contains(aVisualViewport) ||
      aVisualViewport.Contains(aLayoutViewport)) {
    return;
  }

  // If visual viewport size is greater than the layout viewport, move the
  // layout viewport such that it remains inside the visual viewport. Otherwise,
  // move the layout viewport such that the visual viewport is contained
  // inside the layout viewport.
  if ((aLayoutViewport.Width() < aVisualViewport.Width() &&
       !FuzzyEqualsMultiplicative(aLayoutViewport.Width(),
                                  aVisualViewport.Width())) ||
      (aLayoutViewport.Height() < aVisualViewport.Height() &&
       !FuzzyEqualsMultiplicative(aLayoutViewport.Height(),
                                  aVisualViewport.Height()))) {
    if (aLayoutViewport.X() < aVisualViewport.X()) {
      // layout viewport moves right
      aLayoutViewport.MoveToX(aVisualViewport.X());
    } else if (aVisualViewport.XMost() < aLayoutViewport.XMost()) {
      // layout viewport moves left
      aLayoutViewport.MoveByX(aVisualViewport.XMost() -
                              aLayoutViewport.XMost());
    }
    if (aLayoutViewport.Y() < aVisualViewport.Y()) {
      // layout viewport moves down
      aLayoutViewport.MoveToY(aVisualViewport.Y());
    } else if (aVisualViewport.YMost() < aLayoutViewport.YMost()) {
      // layout viewport moves up
      aLayoutViewport.MoveByY(aVisualViewport.YMost() -
                              aLayoutViewport.YMost());
    }
  } else {
    if (aVisualViewport.X() < aLayoutViewport.X()) {
      aLayoutViewport.MoveToX(aVisualViewport.X());
    } else if (aLayoutViewport.XMost() < aVisualViewport.XMost()) {
      aLayoutViewport.MoveByX(aVisualViewport.XMost() -
                              aLayoutViewport.XMost());
    }
    if (aVisualViewport.Y() < aLayoutViewport.Y()) {
      aLayoutViewport.MoveToY(aVisualViewport.Y());
    } else if (aLayoutViewport.YMost() < aVisualViewport.YMost()) {
      aLayoutViewport.MoveByY(aVisualViewport.YMost() -
                              aLayoutViewport.YMost());
    }
  }

  // Regardless of any adjustment above, the layout viewport is not allowed
  // to go outside the scrollable rect.
  aLayoutViewport = aLayoutViewport.MoveInsideAndClamp(aScrollableRect);
}

ScrollSnapInfo::ScrollSnapInfo()
    : mScrollSnapStrictnessX(StyleScrollSnapStrictness::None),
      mScrollSnapStrictnessY(StyleScrollSnapStrictness::None) {}

bool ScrollSnapInfo::HasScrollSnapping() const {
  return mScrollSnapStrictnessY != StyleScrollSnapStrictness::None ||
         mScrollSnapStrictnessX != StyleScrollSnapStrictness::None;
}

bool ScrollSnapInfo::HasSnapPositions() const {
  return (!mSnapPositionX.IsEmpty() &&
          mScrollSnapStrictnessX != StyleScrollSnapStrictness::None) ||
         (!mSnapPositionY.IsEmpty() &&
          mScrollSnapStrictnessY != StyleScrollSnapStrictness::None);
}

void ScrollSnapInfo::InitializeScrollSnapStrictness(
    WritingMode aWritingMode, const nsStyleDisplay* aDisplay) {
  if (aDisplay->mScrollSnapType.strictness == StyleScrollSnapStrictness::None) {
    return;
  }

  mScrollSnapStrictnessX = StyleScrollSnapStrictness::None;
  mScrollSnapStrictnessY = StyleScrollSnapStrictness::None;

  switch (aDisplay->mScrollSnapType.axis) {
    case StyleScrollSnapAxis::X:
      mScrollSnapStrictnessX = aDisplay->mScrollSnapType.strictness;
      break;
    case StyleScrollSnapAxis::Y:
      mScrollSnapStrictnessY = aDisplay->mScrollSnapType.strictness;
      break;
    case StyleScrollSnapAxis::Block:
      if (aWritingMode.IsVertical()) {
        mScrollSnapStrictnessX = aDisplay->mScrollSnapType.strictness;
      } else {
        mScrollSnapStrictnessY = aDisplay->mScrollSnapType.strictness;
      }
      break;
    case StyleScrollSnapAxis::Inline:
      if (aWritingMode.IsVertical()) {
        mScrollSnapStrictnessY = aDisplay->mScrollSnapType.strictness;
      } else {
        mScrollSnapStrictnessX = aDisplay->mScrollSnapType.strictness;
      }
      break;
    case StyleScrollSnapAxis::Both:
      mScrollSnapStrictnessX = aDisplay->mScrollSnapType.strictness;
      mScrollSnapStrictnessY = aDisplay->mScrollSnapType.strictness;
      break;
  }
}

static OverscrollBehavior ToOverscrollBehavior(
    StyleOverscrollBehavior aBehavior) {
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

OverscrollBehaviorInfo OverscrollBehaviorInfo::FromStyleConstants(
    StyleOverscrollBehavior aBehaviorX, StyleOverscrollBehavior aBehaviorY) {
  OverscrollBehaviorInfo result;
  result.mBehaviorX = ToOverscrollBehavior(aBehaviorX);
  result.mBehaviorY = ToOverscrollBehavior(aBehaviorY);
  return result;
}

void ScrollMetadata::SetBackgroundColor(
    const gfx::sRGBColor& aBackgroundColor) {
  mBackgroundColor = gfx::ToDeviceColor(aBackgroundColor);
}

StaticAutoPtr<const ScrollMetadata> ScrollMetadata::sNullMetadata;

}  // namespace layers
}  // namespace mozilla
