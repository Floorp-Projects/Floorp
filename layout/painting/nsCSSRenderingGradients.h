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

namespace mozilla {

namespace layers {
class WebRenderDisplayItemLayer;
} // namespace layers

namespace wr {
class DisplayListBuilder;
} // namespace wr

// A resolved color stop, with a specific position along the gradient line and
// a color.
struct ColorStop {
  ColorStop(): mPosition(0), mIsMidpoint(false) {}
  ColorStop(double aPosition, bool aIsMidPoint, const Color& aColor) :
    mPosition(aPosition), mIsMidpoint(aIsMidPoint), mColor(aColor) {}
  double mPosition; // along the gradient line; 0=start, 1=end
  bool mIsMidpoint;
  Color mColor;
};

class nsCSSGradientRenderer final {
public:
  /**
   * Render a gradient for an element.
   * aDest is the rect for a single tile of the gradient on the destination.
   * aFill is the rect on the destination to be covered by repeated tiling of
   * the gradient.
   * aSrc is the part of the gradient to be rendered into a tile (aDest), if
   * aSrc and aDest are different sizes, the image will be scaled to map aSrc
   * onto aDest.
   * aIntrinsicSize is the size of the source gradient.
   */
  static Maybe<nsCSSGradientRenderer> Create(nsPresContext* aPresContext,
                                             nsStyleGradient* aGradient,
                                             const nsRect& aDest,
                                             const nsRect& aFill,
                                             const nsSize& aRepeatSize,
                                             const mozilla::CSSIntRect& aSrc,
                                             const nsSize& aIntrinsiceSize);

  void Paint(gfxContext& aContext,
             const nsRect& aDirtyRect,
             float aOpacity = 1.0);

  void BuildWebRenderDisplayItems(wr::DisplayListBuilder& aBuilder,
                                  layers::WebRenderDisplayItemLayer* aLayer,
                                  float aOpacity = 1.0);

private:
  nsCSSGradientRenderer() {}

  nsPresContext* mPresContext;
  nsStyleGradient* mGradient;
  CSSIntRect mSrc;
  nsRect mDest;
  nsRect mDirtyRect;
  nsRect mFillArea;
  nsSize mRepeatSize;
  nsTArray<ColorStop> mStops;
  gfxPoint mLineStart, mLineEnd;
  double mRadiusX, mRadiusY;
  bool mForceRepeatToCoverTiles;
  bool mForceRepeatToCoverTilesFlip;
};

} // namespace mozilla

#endif /* nsCSSRenderingGradients_h__ */
