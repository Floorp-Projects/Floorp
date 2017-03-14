/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCSSRenderingGradients_h__
#define nsCSSRenderingGradients_h__

#include "nsLayoutUtils.h"
#include "nsStyleStruct.h"
#include "Units.h"

namespace mozilla {

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
  static void Paint(nsPresContext* aPresContext,
                    gfxContext& aContext,
                    nsStyleGradient* aGradient,
                    const nsRect& aDirtyRect,
                    const nsRect& aDest,
                    const nsRect& aFill,
                    const nsSize& aRepeatSize,
                    const mozilla::CSSIntRect& aSrc,
                    const nsSize& aIntrinsiceSize,
                    float aOpacity = 1.0);
};

} // namespace mozilla

#endif /* nsCSSRenderingGradients_h__ */
