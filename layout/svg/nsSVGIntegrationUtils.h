/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSVGINTEGRATIONUTILS_H_
#define NSSVGINTEGRATIONUTILS_H_

#include "gfxMatrix.h"
#include "GraphicsFilter.h"
#include "gfxRect.h"
#include "nsAutoPtr.h"

class gfxContext;
class gfxDrawable;
class nsDisplayList;
class nsDisplayListBuilder;
class nsIFrame;
class nsIntRegion;

struct nsRect;

namespace mozilla {
namespace gfx {
class DrawTarget;
}
namespace layers {
class LayerManager;
}
}

struct nsPoint;
struct nsSize;

/**
 * Integration of SVG effects (clipPath clipping, masking and filters) into
 * regular display list based painting and hit-testing.
 */
class nsSVGIntegrationUtils final
{
  typedef mozilla::gfx::DrawTarget DrawTarget;

public:
  /**
   * Returns true if SVG effects are currently applied to this frame.
   */
  static bool
  UsingEffectsForFrame(const nsIFrame* aFrame);

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
  static mozilla::gfx::Size
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
   *
   * @param aFrame The effects frame.
   * @param aToReferenceFrame The offset (in app units) from aFrame to its
   * reference display item.
   * @param aInvalidRegion The pre-effects invalid region in pixels relative to
   * the reference display item.
   * @return The post-effects invalid rect in pixels relative to the reference
   * display item.
   */
  static nsIntRegion
  AdjustInvalidAreaForSVGEffects(nsIFrame* aFrame, const nsPoint& aToReferenceFrame,
                                 const nsIntRegion& aInvalidRegion);

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
  PaintFramesWithEffects(gfxContext& aCtx,
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
   * @param aFlags pass FLAG_SYNC_DECODE_IMAGES and any images in the paint
   * server will be decoding synchronously if they are not decoded already.
   */
  enum {
    FLAG_SYNC_DECODE_IMAGES = 0x01,
  };

  static already_AddRefed<gfxDrawable>
  DrawableFromPaintServer(nsIFrame*         aFrame,
                          nsIFrame*         aTarget,
                          const nsSize&     aPaintServerSize,
                          const gfxIntSize& aRenderSize,
                          const DrawTarget* aDrawTarget,
                          const gfxMatrix&  aContextMatrix,
                          uint32_t          aFlags);
};

#endif /*NSSVGINTEGRATIONUTILS_H_*/
