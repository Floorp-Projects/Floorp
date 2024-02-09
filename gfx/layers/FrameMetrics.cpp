/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FrameMetrics.h"

#include <ostream>

#include "gfxUtils.h"
#include "nsStyleConsts.h"
#include "mozilla/gfx/Types.h"

namespace mozilla {
namespace layers {

const ScrollableLayerGuid::ViewID ScrollableLayerGuid::NULL_SCROLL_ID = 0;

std::ostream& operator<<(std::ostream& aStream, const FrameMetrics& aMetrics) {
  aStream << "{ [cb=" << aMetrics.GetCompositionBounds()
          << "] [sr=" << aMetrics.GetScrollableRect()
          << "] [s=" << aMetrics.GetVisualScrollOffset();
  if (aMetrics.GetVisualScrollUpdateType() != FrameMetrics::eNone) {
    aStream << "] [vd=" << aMetrics.GetVisualDestination();
  }
  if (aMetrics.IsScrollInfoLayer()) {
    aStream << "] [scrollinfo";
  }
  aStream << "] [dp=" << aMetrics.GetDisplayPort()
          << "] [rcs=" << aMetrics.GetBoundingCompositionSize()
          << "] [v=" << aMetrics.GetLayoutViewport()
          << nsPrintfCString("] [z=(ld=%.3f r=%.3f",
                             aMetrics.GetDevPixelsPerCSSPixel().scale,
                             aMetrics.GetPresShellResolution())
                 .get()
          << " cr=" << aMetrics.GetCumulativeResolution()
          << " z=" << aMetrics.GetZoom()
          << " t=" << aMetrics.GetTransformToAncestorScale() << " )] [u=("
          << (int)aMetrics.GetVisualScrollUpdateType() << " "
          << aMetrics.GetScrollGeneration()
          << ")] scrollId=" << aMetrics.GetScrollId();
  if (aMetrics.IsRootContent()) {
    aStream << " [rcd]";
  }
  aStream << " }";
  return aStream;
}

void FrameMetrics::RecalculateLayoutViewportOffset() {
  // For subframes, the visual and layout viewports coincide, so just
  // keep the layout viewport offset in sync with the visual one.
  if (!mIsRootContent) {
    mLayoutViewport.MoveTo(GetVisualScrollOffset());
    return;
  }
  // For the root, the two viewports can diverge, but the layout
  // viewport needs to keep enclosing the visual viewport.
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

/* static */
CSSRect FrameMetrics::CalculateScrollRange(
    const CSSRect& aScrollableRect, const ParentLayerRect& aCompositionBounds,
    const CSSToParentLayerScale& aZoom) {
  CSSSize scrollPortSize =
      CalculateCompositedSizeInCssPixels(aCompositionBounds, aZoom);
  CSSRect scrollRange = aScrollableRect;
  scrollRange.SetWidth(
      std::max(scrollRange.Width() - scrollPortSize.width, 0.0f));
  scrollRange.SetHeight(
      std::max(scrollRange.Height() - scrollPortSize.height, 0.0f));
  return scrollRange;
}

/* static */
CSSSize FrameMetrics::CalculateCompositedSizeInCssPixels(
    const ParentLayerRect& aCompositionBounds,
    const CSSToParentLayerScale& aZoom) {
  if (aZoom == CSSToParentLayerScale(0)) {
    return CSSSize();  // avoid division by zero
  }
  return aCompositionBounds.Size() / aZoom;
}

std::pair<bool, CSSPoint> FrameMetrics::ApplyAbsoluteScrollUpdateFrom(
    const ScrollPositionUpdate& aUpdate) {
  CSSPoint oldVisualOffset = GetVisualScrollOffset();
  // In applying a main-thread scroll update, try to preserve the relative
  // offset between the visual and layout viewports.
  CSSPoint relativeOffset = oldVisualOffset - GetLayoutScrollOffset();
  MOZ_ASSERT(IsRootContent() || relativeOffset == CSSPoint());
  // We need to set the two offsets together, otherwise a subsequent
  // RecalculateLayoutViewportOffset() could see divergent layout and
  // visual offsets.
  bool offsetChanged = SetLayoutScrollOffset(aUpdate.GetDestination());
  offsetChanged |=
      ClampAndSetVisualScrollOffset(aUpdate.GetDestination() + relativeOffset);
  return {offsetChanged, GetVisualScrollOffset() - oldVisualOffset};
}

CSSPoint FrameMetrics::ApplyRelativeScrollUpdateFrom(
    const ScrollPositionUpdate& aUpdate) {
  MOZ_ASSERT(aUpdate.GetType() == ScrollUpdateType::Relative);
  CSSPoint origin = GetVisualScrollOffset();
  CSSPoint delta = (aUpdate.GetDestination() - aUpdate.GetSource());
  ClampAndSetVisualScrollOffset(origin + delta);
  return GetVisualScrollOffset() - origin;
}

CSSPoint FrameMetrics::ApplyPureRelativeScrollUpdateFrom(
    const ScrollPositionUpdate& aUpdate) {
  MOZ_ASSERT(aUpdate.GetType() == ScrollUpdateType::PureRelative);
  CSSPoint origin = GetVisualScrollOffset();
  ClampAndSetVisualScrollOffset(origin + aUpdate.GetDelta());
  return GetVisualScrollOffset() - origin;
}

void FrameMetrics::UpdatePendingScrollInfo(const ScrollPositionUpdate& aInfo) {
  // We only get this "pending scroll info" for paint-skip transactions,
  // but PureRelative position updates always trigger a full paint, so
  // we should never enter this code with a PureRelative update type. For
  // the other types, the destination field on the ScrollPositionUpdate will
  // tell us the final layout scroll position on the main thread.
  MOZ_ASSERT(aInfo.GetType() != ScrollUpdateType::PureRelative);

  // In applying a main-thread scroll update, try to preserve the relative
  // offset between the visual and layout viewports.
  CSSPoint relativeOffset = GetVisualScrollOffset() - GetLayoutScrollOffset();
  MOZ_ASSERT(IsRootContent() || relativeOffset == CSSPoint());

  SetLayoutScrollOffset(aInfo.GetDestination());
  ClampAndSetVisualScrollOffset(aInfo.GetDestination() + relativeOffset);
  mScrollGeneration = aInfo.GetGeneration();
}

std::ostream& operator<<(std::ostream& aStream,
                         const OverscrollBehavior& aBehavior) {
  switch (aBehavior) {
    case OverscrollBehavior::Auto: {
      aStream << "auto";
      break;
    }
    case OverscrollBehavior::Contain: {
      aStream << "contain";
      break;
    }
    case OverscrollBehavior::None: {
      aStream << "none";
      break;
    }
  }
  return aStream;
}

OverscrollBehaviorInfo::OverscrollBehaviorInfo()
    : mBehaviorX(OverscrollBehavior::Auto),
      mBehaviorY(OverscrollBehavior::Auto) {}

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

bool OverscrollBehaviorInfo::operator==(
    const OverscrollBehaviorInfo& aOther) const {
  return mBehaviorX == aOther.mBehaviorX && mBehaviorY == aOther.mBehaviorY;
}

std::ostream& operator<<(std::ostream& aStream,
                         const OverscrollBehaviorInfo& aInfo) {
  if (aInfo.mBehaviorX == aInfo.mBehaviorY) {
    aStream << aInfo.mBehaviorX;
  } else {
    aStream << "{ x=" << aInfo.mBehaviorX << ", y=" << aInfo.mBehaviorY << " }";
  }
  return aStream;
}

std::ostream& operator<<(std::ostream& aStream,
                         const ScrollMetadata& aMetadata) {
  aStream << "{ [description=" << aMetadata.GetContentDescription()
          << "] [metrics=" << aMetadata.GetMetrics();
  if (aMetadata.GetScrollParentId() != ScrollableLayerGuid::NULL_SCROLL_ID) {
    aStream << "] [scrollParent=" << aMetadata.GetScrollParentId();
  }
  if (aMetadata.GetHasScrollgrab()) {
    aStream << "] [scrollgrab";
  }
  aStream << "] [overscroll=" << aMetadata.GetOverscrollBehavior() << "] ["
          << aMetadata.GetScrollUpdates().Length() << " scrollupdates"
          << "] }";
  return aStream;
}

StaticAutoPtr<const ScrollMetadata> ScrollMetadata::sNullMetadata;

}  // namespace layers
}  // namespace mozilla
