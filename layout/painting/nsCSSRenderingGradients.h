/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCSSRenderingGradients_h__
#define nsCSSRenderingGradients_h__

#include "nsLayoutUtils.h"
#include "nsStyleStruct.h"
#include "Units.h"
#include "mozilla/Maybe.h"
#include "mozilla/gfx/2D.h"

namespace mozilla {

namespace layers {
class StackingContextHelper;
class WebRenderDisplayItemLayer;
} // namespace layers

namespace wr {
class DisplayListBuilder;
} // namespace wr

// A resolved color stop, with a specific position along the gradient line and
// a color.
struct ColorStop {
  ColorStop(): mPosition(0), mIsMidpoint(false) {}
  ColorStop(double aPosition, bool aIsMidPoint, const gfx::Color& aColor) :
    mPosition(aPosition), mIsMidpoint(aIsMidPoint), mColor(aColor) {}
  double mPosition; // along the gradient line; 0=start, 1=end
  bool mIsMidpoint;
  gfx::Color mColor;
};

class nsCSSGradientRenderer final {
public:
  /**
   * Prepare a nsCSSGradientRenderer for a gradient for an element.
   * aIntrinsicSize - the size of the source gradient.
   */
  static nsCSSGradientRenderer Create(nsPresContext* aPresContext,
                                      nsStyleGradient* aGradient,
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
  void Paint(gfxContext& aContext,
             const nsRect& aDest,
             const nsRect& aFill,
             const nsSize& aRepeatSize,
             const mozilla::CSSIntRect& aSrc,
             const nsRect& aDirtyRect,
             float aOpacity = 1.0);

  /**
   * Collect the gradient parameters
   */
  void BuildWebRenderParameters(float aOpacity,
                                wr::WrGradientExtendMode& aMode,
                                nsTArray<wr::WrGradientStop>& aStops,
                                LayoutDevicePoint& aLineStart,
                                LayoutDevicePoint& aLineEnd,
                                LayoutDeviceSize& aGradientRadius);

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
                                  layers::WebRenderDisplayItemLayer* aLayer,
                                  const nsRect& aDest,
                                  const nsRect& aFill,
                                  const nsSize& aRepeatSize,
                                  const mozilla::CSSIntRect& aSrc,
                                  float aOpacity = 1.0);

private:
  nsCSSGradientRenderer() {}

  nsPresContext* mPresContext;
  nsStyleGradient* mGradient;
  nsTArray<ColorStop> mStops;
  gfxPoint mLineStart, mLineEnd;
  double mRadiusX, mRadiusY;
};

} // namespace mozilla

#endif /* nsCSSRenderingGradients_h__ */
