/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZUtils.h"

#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/StaticPrefs_layers.h"

namespace mozilla {
namespace layers {

namespace apz {

bool IsCloseToHorizontal(float aAngle, float aThreshold) {
  return (aAngle < aThreshold || aAngle > (M_PI - aThreshold));
}

bool IsCloseToVertical(float aAngle, float aThreshold) {
  return (fabs(aAngle - (M_PI / 2)) < aThreshold);
}

bool IsStuckAtBottom(gfxFloat aTranslation,
                     const LayerRectAbsolute& aInnerRange,
                     const LayerRectAbsolute& aOuterRange) {
  // The item will be stuck at the bottom if the async scroll delta is in
  // the range [aOuterRange.Y(), aInnerRange.Y()]. Since the translation
  // is negated with repect to the async scroll delta (i.e. scrolling down
  // produces a positive scroll delta and negative translation), we invert it
  // and check to see if it falls in the specified range.
  return aOuterRange.Y() <= -aTranslation && -aTranslation <= aInnerRange.Y();
}

bool IsStuckAtTop(gfxFloat aTranslation, const LayerRectAbsolute& aInnerRange,
                  const LayerRectAbsolute& aOuterRange) {
  // Same as IsStuckAtBottom, except we want to check for the range
  // [aInnerRange.YMost(), aOuterRange.YMost()].
  return aInnerRange.YMost() <= -aTranslation &&
         -aTranslation <= aOuterRange.YMost();
}

ScreenPoint ComputeFixedMarginsOffset(
    const ScreenMargin& aCompositorFixedLayerMargins, SideBits aFixedSides,
    const ScreenMargin& aGeckoFixedLayerMargins) {
  // Work out the necessary translation, in screen space.
  ScreenPoint translation;

  ScreenMargin effectiveMargin =
      aCompositorFixedLayerMargins - aGeckoFixedLayerMargins;
  if ((aFixedSides & SideBits::eLeftRight) == SideBits::eLeftRight) {
    translation.x += (effectiveMargin.left - effectiveMargin.right) / 2;
  } else if (aFixedSides & SideBits::eRight) {
    translation.x -= effectiveMargin.right;
  } else if (aFixedSides & SideBits::eLeft) {
    translation.x += effectiveMargin.left;
  }

  if ((aFixedSides & SideBits::eTopBottom) == SideBits::eTopBottom) {
    translation.y += (effectiveMargin.top - effectiveMargin.bottom) / 2;
  } else if (aFixedSides & SideBits::eBottom) {
    translation.y -= effectiveMargin.bottom;
  } else if (aFixedSides & SideBits::eTop) {
    translation.y += effectiveMargin.top;
  }

  return translation;
}

bool AboutToCheckerboard(const FrameMetrics& aPaintedMetrics,
                         const FrameMetrics& aCompositorMetrics) {
  // The main-thread code to compute the painted area can introduce some
  // rounding error due to multiple unit conversions, so we inflate the rect by
  // one app unit to account for that.
  CSSRect painted = (aPaintedMetrics.GetCriticalDisplayPort().IsEmpty()
                         ? aPaintedMetrics.GetDisplayPort()
                         : aPaintedMetrics.GetCriticalDisplayPort()) +
                    aPaintedMetrics.GetLayoutScrollOffset();
  painted.Inflate(CSSMargin::FromAppUnits(nsMargin(1, 1, 1, 1)));

  // Inflate the rect by the danger zone. See the description of the danger zone
  // prefs in AsyncPanZoomController.cpp for an explanation of this.
  CSSRect visible =
      CSSRect(aCompositorMetrics.GetVisualScrollOffset(),
              aCompositorMetrics.CalculateBoundedCompositedSizeInCssPixels());
  visible.Inflate(LayerSize(StaticPrefs::apz_danger_zone_x(),
                            StaticPrefs::apz_danger_zone_y()) /
                  aCompositorMetrics.LayersPixelsPerCSSPixel());

  // Clamp both rects to the scrollable rect, because having either of those
  // exceed the scrollable rect doesn't make sense, and could lead to false
  // positives.
  painted = painted.Intersect(aPaintedMetrics.GetScrollableRect());
  visible = visible.Intersect(aPaintedMetrics.GetScrollableRect());

  return !painted.Contains(visible);
}

bool ShouldUseProgressivePaint() {
  // The mutexes required for progressive painting pose a security risk
  // on sandboxed platforms. Additionally, Android is the only platform
  // that supports progressive painting, so if on a sandboxed platform
  // or not on Android, we should not use progressive painting.
#if defined(MOZ_SANDBOX) || !defined(MOZ_WIDGET_ANDROID)
  return false;
#else
  return StaticPrefs::layers_progressive_paint_DoNotUseDirectly();
#endif
}

SideBits GetOverscrollSideBits(const ParentLayerPoint& aOverscrollAmount) {
  SideBits sides = SideBits::eNone;

  if (aOverscrollAmount.x < 0) {
    sides |= SideBits::eLeft;
  } else if (aOverscrollAmount.x > 0) {
    sides |= SideBits::eRight;
  }

  if (aOverscrollAmount.y < 0) {
    sides |= SideBits::eTop;
  } else if (aOverscrollAmount.y > 0) {
    sides |= SideBits::eBottom;
  }

  return sides;
}

}  // namespace apz
}  // namespace layers
}  // namespace mozilla
