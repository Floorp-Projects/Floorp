/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCSSRenderingGradients_h__
#define nsCSSRenderingGradients_h__

#include "gfxRect.h"
#include "gfxUtils.h"
#include "nsStyleStruct.h"
#include "Units.h"
#include "mozilla/Maybe.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/webrender/webrender_ffi.h"

class gfxPattern;

namespace mozilla {

namespace layers {
class StackingContextHelper;
}  // namespace layers

namespace wr {
class DisplayListBuilder;
}  // namespace wr

// A resolved color stop, with a specific position along the gradient line and
// a color.
struct ColorStop {
  ColorStop() : mPosition(0), mIsMidpoint(false) {}
  ColorStop(double aPosition, bool aIsMidPoint,
            const StyleAbsoluteColor& aColor)
      : mPosition(aPosition), mIsMidpoint(aIsMidPoint), mColor(aColor) {}
  double mPosition;  // along the gradient line; 0=start, 1=end
  bool mIsMidpoint;
  StyleAbsoluteColor mColor;
};

template <class T>
class MOZ_STACK_CLASS ColorStopInterpolator {
 public:
  ColorStopInterpolator(
      const nsTArray<ColorStop>& aStops,
      const StyleColorInterpolationMethod& aStyleColorInterpolationMethod)
      : mStyleColorInterpolationMethod(aStyleColorInterpolationMethod),
        mStops(aStops) {}

  void CreateStops() {
    for (uint32_t i = 0; i < mStops.Length() - 1; i++) {
      const auto& start = mStops[i];
      const auto& end = mStops[i + 1];
      uint32_t extraStops =
          (uint32_t)(floor(end.mPosition * kFullRangeExtraStops) -
                     floor(start.mPosition * kFullRangeExtraStops));
      extraStops = clamped(extraStops, 1U, kFullRangeExtraStops);
      float step = 1.0f / (float)extraStops;
      for (uint32_t extraStop = 0; extraStop <= extraStops; extraStop++) {
        auto progress = (float)extraStop * step;
        auto position =
            start.mPosition + progress * (end.mPosition - start.mPosition);
        StyleAbsoluteColor color =
            Servo_InterpolateColor(mStyleColorInterpolationMethod, &end.mColor,
                                   &start.mColor, progress);
        static_cast<T*>(this)->CreateStop(float(position),
                                          gfx::ToDeviceColor(color));
      }
    }
  }

 protected:
  StyleColorInterpolationMethod mStyleColorInterpolationMethod;
  const nsTArray<ColorStop>& mStops;

  // this could be made tunable, but at 1.0/128 the error is largely
  // irrelevant, as WebRender re-encodes it to 128 pairs of stops.
  //
  // note that we don't attempt to place the positions of these stops
  // precisely at intervals, we just add this many extra stops across the
  // range where it is convenient.
  inline static const uint32_t kFullRangeExtraStops = 128;
};

class nsCSSGradientRenderer final {
 public:
  /**
   * Prepare a nsCSSGradientRenderer for a gradient for an element.
   * aIntrinsicSize - the size of the source gradient.
   */
  static nsCSSGradientRenderer Create(nsPresContext* aPresContext,
                                      ComputedStyle* aComputedStyle,
                                      const StyleGradient& aGradient,
                                      const nsSize& aIntrinsiceSize);

  /**
   * Draw the gradient to aContext
   * aDest - where the first tile of gradient is
   * aFill - the area to be filled with tiles of aDest
   * aSrc - the area of the gradient that will fill aDest
   * aRepeatSize - the distance from the origin of a tile
   *               to the next origin of a tile
   * aDirtyRect - pixels outside of this area may be skipped
   */
  void Paint(gfxContext& aContext, const nsRect& aDest, const nsRect& aFill,
             const nsSize& aRepeatSize, const mozilla::CSSIntRect& aSrc,
             const nsRect& aDirtyRect, float aOpacity = 1.0);

  /**
   * Collect the gradient parameters
   */
  void BuildWebRenderParameters(float aOpacity, wr::ExtendMode& aMode,
                                nsTArray<wr::GradientStop>& aStops,
                                LayoutDevicePoint& aLineStart,
                                LayoutDevicePoint& aLineEnd,
                                LayoutDeviceSize& aGradientRadius,
                                LayoutDevicePoint& aGradientCenter,
                                float& aGradientAngle);

  /**
   * Build display items for the gradient
   * aLayer - the layer to make this display item relative to
   * aDest - where the first tile of gradient is
   * aFill - the area to be filled with tiles of aDest
   * aRepeatSize - the distance from the origin of a tile
   *               to the next origin of a tile
   * aSrc - the area of the gradient that will fill aDest
   */
  void BuildWebRenderDisplayItems(wr::DisplayListBuilder& aBuilder,
                                  const layers::StackingContextHelper& aSc,
                                  const nsRect& aDest, const nsRect& aFill,
                                  const nsSize& aRepeatSize,
                                  const mozilla::CSSIntRect& aSrc,
                                  bool aIsBackfaceVisible,
                                  float aOpacity = 1.0);

 private:
  nsCSSGradientRenderer()
      : mPresContext(nullptr),
        mGradient(nullptr),
        mRadiusX(0.0),
        mRadiusY(0.0),
        mAngle(0.0) {}

  /**
   * Attempts to paint the tiles for a gradient by painting it once to an
   * offscreen surface and then painting that offscreen surface with
   * ExtendMode::Repeat to cover all tiles.
   *
   * Returns false if the optimization wasn't able to be used, in which case
   * a fallback should be used.
   */
  bool TryPaintTilesWithExtendMode(
      gfxContext& aContext, gfxPattern* aGradientPattern, nscoord aXStart,
      nscoord aYStart, const gfxRect& aDirtyAreaToFill, const nsRect& aDest,
      const nsSize& aRepeatSize, bool aForceRepeatToCoverTiles);

  nsPresContext* mPresContext;
  const StyleGradient* mGradient;
  nsTArray<ColorStop> mStops;
  gfxPoint mLineStart, mLineEnd;  // only for linear/radial gradients
  double mRadiusX, mRadiusY;      // only for radial gradients
  gfxPoint mCenter;               // only for conic gradients
  float mAngle;                   // only for conic gradients
};

}  // namespace mozilla

#endif /* nsCSSRenderingGradients_h__ */
