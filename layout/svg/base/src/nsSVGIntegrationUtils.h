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
#include "Layers.h"

class nsDisplayList;
class nsDisplayListBuilder;
class nsIFrame;
class nsRenderingContext;

struct nsPoint;
struct nsSize;

/**
 * Integration of SVG effects (clipPath clipping, masking and filters) into
 * regular display list based painting and hit-testing.
 */
class nsSVGIntegrationUtils MOZ_FINAL
{
public:
  /**
   * Returns true if SVG effects are currently applied to this frame.
   */
  static bool
  UsingEffectsForFrame(const nsIFrame* aFrame);

  /**
   * In SVG, an element's "user space" is simply the coordinate system in place
   * at the time that it is drawn. For non-SVG frames, we want any SVG effects
   * to be applied to the union of the border-box rects of all of a given
   * frame's continuations. This means that, when we paint a non-SVG frame with
   * effects, we want to offset the effects by the distance from the frame's
   * origin (the top left of its border box) to the top left of the union of
   * the border-box rects of all its continuations. In other words, we need to
   * apply this offset as a suplimental translation to the current coordinate
   * system in order to establish the correct user space before calling into
   * the SVG effects code. For the purposes of the nsSVGIntegrationUtils code
   * we somewhat misappropriate the term "user space" by using it to refer
   * specifically to this adjusted coordinate system.
   *
   * For consistency with nsIFrame::GetOffsetTo, the offset this method returns
   * is the offset you need to add to a point that's relative to aFrame's
   * origin (the top left of its border box) to convert it to aFrame's user
   * space. In other words the value returned is actually the offset from the
   * origin of aFrame's user space to aFrame.
   *
   * Note: This method currently only accepts a frame's first continuation
   * since none of our current callers need to be able to pass in other
   * continuations.
   */
  static nsPoint
  GetOffsetToUserSpace(nsIFrame* aFrame);

  /**
   * Returns the size of the union of the border-box rects of all of
   * aNonSVGFrame's continuations.
   */
  static nsSize
  GetContinuationUnionSize(nsIFrame* aNonSVGFrame);

  /**
   * When SVG effects need to resolve percentage, userSpaceOnUse lengths, they
   * need a coordinate context to resolve them against. This method provides
   * that coordinate context for non-SVG frames with SVG effects applied to
   * them. The gfxSize returned is the size of the union of all of the given
   * frame's continuations' border boxes, converted to SVG user units (equal to
   * CSS px units), as required by the SVG code.
   */
  static gfxSize
  GetSVGCoordContextForNonSVGFrame(nsIFrame* aNonSVGFrame);

  /**
   * SVG effects such as SVG filters, masking and clipPath may require an SVG
   * "bbox" for the element they're being applied to in order to make decisions
   * about positioning, and to resolve various lengths against. This method
   * provides the "bbox" for non-SVG frames. The bbox returned is in CSS px
   * units, and is the union of all aNonSVGFrame's continuations' overflow
   * areas, relative to the top-left of the union of all aNonSVGFrame's
   * continuations' border box rects.
   */
  static gfxRect
  GetSVGBBoxForNonSVGFrame(nsIFrame* aNonSVGFrame);

  /**
   * Used to adjust a frame's pre-effects visual overflow rect to take account
   * of SVG effects.
   *
   * XXX This method will not do the right thing for frames with continuations.
   * It really needs all the continuations to have been reflowed before being
   * called, but we currently call it on each continuation as its overflow
   * rects are set during the reflow of each particular continuation. Gecko's
   * current reflow architecture does not allow us to set the overflow rects
   * for a whole chain of continuations for a given element at the point when
   * the last continuation is reflowed. See:
   * http://groups.google.com/group/mozilla.dev.tech.layout/msg/6b179066f3051f65
   */
  static nsRect
  ComputePostEffectsVisualOverflowRect(nsIFrame* aFrame,
                                       const nsRect& aPreEffectsOverflowRect);

  /**
   * Used to adjust the area of a frame that needs to be invalidated to take
   * account of SVG effects.
   */
  static nsRect
  AdjustInvalidAreaForSVGEffects(nsIFrame* aFrame, const nsRect& aInvalidRect);

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
   */
  static void
  PaintFramesWithEffects(nsRenderingContext* aCtx,
                         nsIFrame* aFrame, const nsRect& aDirtyRect,
                         nsDisplayListBuilder* aBuilder,
                         mozilla::layers::LayerManager* aManager);

  /**
   * SVG frames expect to paint in SVG user units, which are equal to CSS px
   * units. This method provides a transform matrix to multiply onto a
   * gfxContext's current transform to convert the context's current units from
   * its usual dev pixels to SVG user units/CSS px to keep the SVG code happy.
   */
  static gfxMatrix
  GetCSSPxToDevPxMatrix(nsIFrame* aNonSVGFrame);

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
