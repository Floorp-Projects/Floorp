/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSVGINTEGRATIONUTILS_H_
#define NSSVGINTEGRATIONUTILS_H_

#include "gfxMatrix.h"
#include "gfxPattern.h"
#include "gfxRect.h"
#include "nsRect.h"

class nsDisplayList;
class nsDisplayListBuilder;
class nsIFrame;
class nsRenderingContext;

struct nsPoint;
struct nsSize;

/***** Integration of SVG effects with regular frame painting *****/

class nsSVGIntegrationUtils
{
public:
  /**
   * Returns true if a non-SVG frame has SVG effects.
   */
  static bool
  UsingEffectsForFrame(const nsIFrame* aFrame);

  /**
   * Get the union the frame border-box rects over all continuations,
   * relative to aFirst. This defines "user space" for non-SVG frames.
   */
  static nsRect
  GetNonSVGUserSpace(nsIFrame* aFirst);
  /**
   * Adjust overflow rect for effects.
   * XXX this is a problem. We really need to compute the effects rect for
   * a whole chain of frames for a given element at once. but we have no
   * way to do this effectively with Gecko's current reflow architecture.
   * See http://groups.google.com/group/mozilla.dev.tech.layout/msg/6b179066f3051f65
   */
  static nsRect
  ComputeFrameEffectsRect(nsIFrame* aFrame, const nsRect& aOverflowRect);
  /**
   * Adjust the frame's invalidation area to cover effects
   */
  static nsRect
  GetInvalidAreaForChangedSource(nsIFrame* aFrame, const nsRect& aInvalidRect);
  /**
   * Figure out which area of the source is needed given an area to
   * repaint
   */
  static nsRect
  GetRequiredSourceForInvalidArea(nsIFrame* aFrame, const nsRect& aDamageRect);
  /**
   * Returns true if the given point is not clipped out by effects.
   * @param aPt in appunits relative to aFrame
   */
  static bool
  HitTestFrameForEffects(nsIFrame* aFrame, const nsPoint& aPt);

  /**
   * Paint non-SVG frame with SVG effects.
   * @param aOffset the offset in appunits where aFrame should be positioned
   * in aCtx's coordinate system
   */
  static void
  PaintFramesWithEffects(nsRenderingContext* aCtx,
                         nsIFrame* aEffectsFrame, const nsRect& aDirtyRect,
                         nsDisplayListBuilder* aBuilder,
                         nsDisplayList* aInnerList);

  static gfxMatrix
  GetInitialMatrix(nsIFrame* aNonSVGFrame);
  /**
   * Returns aNonSVGFrame's rect in CSS pixel units. This is the union
   * of all its continuations' rectangles. The top-left is always 0,0
   * since "user space" origin for non-SVG frames is the top-left of the
   * union of all the continuations' rectangles.
   */
  static gfxRect
  GetSVGRectForNonSVGFrame(nsIFrame* aNonSVGFrame);
  /**
   * Returns aNonSVGFrame's bounding box in CSS units. This is the union
   * of all its continuations' overflow areas, relative to the top-left
   * of all the continuations' rectangles.
   */
  static gfxRect
  GetSVGBBoxForNonSVGFrame(nsIFrame* aNonSVGFrame);

  /**
   * @param aRenderingContext the target rendering context in which the paint
   * server will be rendered
   * @param aTarget the target frame onto which the paint server will be
   * rendered
   * @param aPaintServer a first-continuation frame to use as the source
   * @param aFilter a filter to be applied when scaling
   * @param aDest the area the paint server image should be mapped to
   * @param aFill the area to be filled with copies of the paint server image
   * @param aAnchor a point in aFill which we will ensure is pixel-aligned in
   * the output
   * @param aDirty pixels outside this area may be skipped
   * @param aPaintServerSize the size that would be filled when using
   * background-repeat:no-repeat and background-size:auto. For normal background
   * images, this would be the intrinsic size of the image; for gradients and
   * patterns this would be the whole target frame fill area.
   */
  static void
  DrawPaintServer(nsRenderingContext* aRenderingContext,
                  nsIFrame*            aTarget,
                  nsIFrame*            aPaintServer,
                  gfxPattern::GraphicsFilter aFilter,
                  const nsRect&        aDest,
                  const nsRect&        aFill,
                  const nsPoint&       aAnchor,
                  const nsRect&        aDirty,
                  const nsSize&        aPaintServerSize);
};

#endif /*NSSVGINTEGRATIONUTILS_H_*/
