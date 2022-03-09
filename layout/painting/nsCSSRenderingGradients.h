/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCSSRenderingGradients_h__
#define nsCSSRenderingGradients_h__

#include "gfxRect.h"
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
  ColorStop(double aPosition, bool aIsMidPoint, const gfx::sRGBColor& aColor)
      : mPosition(aPosition), mIsMidpoint(aIsMidPoint), mColor(aColor) {}
  double mPosition;  // along the gradient line; 0=start, 1=end
  bool mIsMidpoint;
  gfx::sRGBColor mColor;
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
