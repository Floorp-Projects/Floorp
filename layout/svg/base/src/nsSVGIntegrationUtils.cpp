/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   rocallahan@mozilla.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsSVGIntegrationUtils.h"

#include "nsSVGUtils.h"
#include "nsSVGEffects.h"
#include "nsRegion.h"
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"
#include "nsSVGMatrix.h"
#include "nsSVGFilterPaintCallback.h"
#include "nsSVGFilterFrame.h"
#include "nsSVGClipPathFrame.h"
#include "nsSVGMaskFrame.h"

// ----------------------------------------------------------------------

PRBool
nsSVGIntegrationUtils::UsingEffectsForFrame(const nsIFrame* aFrame)
{
  const nsStyleSVGReset *style = aFrame->GetStyleSVGReset();
  return (style->mFilter || style->mClipPath || style->mMask) &&
          !aFrame->IsFrameOfType(nsIFrame::eSVG);
}

// Get the union the frame border-box rects over all continuations,
// relative to aFirst. This defines "user space" for non-SVG frames.
static nsRect GetNonSVGUserSpace(nsIFrame* aFirst)
{
  NS_ASSERTION(!aFirst->GetPrevContinuation(), "Not first continuation");
  return nsLayoutUtils::GetAllInFlowRectsUnion(aFirst, aFirst);
}

static nsRect
GetPreEffectsOverflowRect(nsIFrame* aFrame)
{
  nsRect* r = static_cast<nsRect*>(aFrame->GetProperty(nsGkAtoms::preEffectsBBoxProperty));
  if (r)
    return *r;
  return aFrame->GetOverflowRect();
}

struct BBoxCollector : public nsLayoutUtils::BoxCallback {
  nsIFrame*     mReferenceFrame;
  nsIFrame*     mCurrentFrame;
  const nsRect& mCurrentFrameOverflowArea;
  nsRect        mResult;

  BBoxCollector(nsIFrame* aReferenceFrame, nsIFrame* aCurrentFrame,
                const nsRect& aCurrentFrameOverflowArea)
    : mReferenceFrame(aReferenceFrame), mCurrentFrame(aCurrentFrame),
      mCurrentFrameOverflowArea(aCurrentFrameOverflowArea) {}

  virtual void AddBox(nsIFrame* aFrame) {
    nsRect overflow = aFrame == mCurrentFrame ? mCurrentFrameOverflowArea
        : GetPreEffectsOverflowRect(aFrame);
    mResult.UnionRect(mResult, overflow + aFrame->GetOffsetTo(mReferenceFrame));
  }
};

static nsRect
GetSVGBBox(nsIFrame* aNonSVGFrame, nsIFrame* aCurrentFrame,
           const nsRect& aCurrentOverflow, const nsRect& aUserSpaceRect)
{
  NS_ASSERTION(!aNonSVGFrame->GetPrevContinuation(),
               "Need first continuation here");
  // Compute union of all overflow areas relative to 'first'.
  BBoxCollector collector(aNonSVGFrame, aCurrentFrame, aCurrentOverflow);
  nsLayoutUtils::GetAllInFlowBoxes(aNonSVGFrame, &collector);
  // Get it into "user space" for non-SVG frames
  return collector.mResult - aUserSpaceRect.TopLeft();
}

nsRect
nsSVGIntegrationUtils::ComputeFrameEffectsRect(nsIFrame* aFrame,
                                               const nsRect& aOverflowRect)
{
  nsIFrame* firstFrame =
    nsLayoutUtils::GetFirstContinuationOrSpecialSibling(aFrame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);
  nsSVGFilterFrame *filterFrame = effectProperties.mFilter ?
    effectProperties.mFilter->GetFilterFrame() : nsnull;
  if (!filterFrame)
    return aOverflowRect;

  // XXX this isn't really right. We can't compute the correct filter
  // bbox until all aFrame's continuations have been reflowed.
  // but then it's too late to set the overflow areas for the earlier frames.
  nsRect userSpaceRect = GetNonSVGUserSpace(firstFrame);
  nsRect r = GetSVGBBox(firstFrame, aFrame, aOverflowRect, userSpaceRect);
  // r is relative to user space
  PRUint32 appUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();
  nsIntRect p = nsRect::ToOutsidePixels(r, appUnitsPerDevPixel);
  p = filterFrame->GetFilterBBox(firstFrame, &p);
  r = nsIntRect::ToAppUnits(p, appUnitsPerDevPixel);
  // Make it relative to aFrame again
  return r + userSpaceRect.TopLeft() - aFrame->GetOffsetTo(firstFrame);
}

nsRect
nsSVGIntegrationUtils::GetInvalidAreaForChangedSource(nsIFrame* aFrame,
                                                      const nsRect& aInvalidRect)
{
  // Don't bother calling GetEffectProperties; the filter property should
  // already have been set up during reflow/ComputeFrameEffectsRect
  nsIFrame* firstFrame =
    nsLayoutUtils::GetFirstContinuationOrSpecialSibling(aFrame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);
  if (!effectProperties.mFilter)
    return aInvalidRect;
  nsSVGFilterFrame* filterFrame = nsSVGEffects::GetFilterFrame(firstFrame);
  if (!filterFrame) {
    // The frame is either not there or not currently available,
    // perhaps because we're in the middle of tearing stuff down.
    // Be conservative.
    return aFrame->GetOverflowRect();
  }

  PRInt32 appUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();
  nsRect userSpaceRect = GetNonSVGUserSpace(firstFrame);
  nsPoint offset = aFrame->GetOffsetTo(firstFrame) - userSpaceRect.TopLeft();
  nsRect r = aInvalidRect + offset;
  nsIntRect p = nsRect::ToOutsidePixels(r, appUnitsPerDevPixel);
  p = filterFrame->GetInvalidationBBox(firstFrame, p);
  r = nsIntRect::ToAppUnits(p, appUnitsPerDevPixel);
  return r - offset;
}

nsRect
nsSVGIntegrationUtils::GetRequiredSourceForInvalidArea(nsIFrame* aFrame,
                                                       const nsRect& aDamageRect)
{
  // Don't bother calling GetEffectProperties; the filter property should
  // already have been set up during reflow/ComputeFrameEffectsRect
  nsIFrame* firstFrame =
    nsLayoutUtils::GetFirstContinuationOrSpecialSibling(aFrame);
  nsSVGFilterFrame* filterFrame =
    nsSVGEffects::GetFilterFrame(firstFrame);
  if (!filterFrame)
    return aDamageRect;
  
  PRInt32 appUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();
  nsRect userSpaceRect = GetNonSVGUserSpace(firstFrame);
  nsPoint offset = aFrame->GetOffsetTo(firstFrame) - userSpaceRect.TopLeft();
  nsRect r = aDamageRect + offset;
  nsIntRect p = nsRect::ToOutsidePixels(r, appUnitsPerDevPixel);
  p = filterFrame->GetSourceForInvalidArea(firstFrame, p);
  r = nsIntRect::ToAppUnits(p, appUnitsPerDevPixel);
  return r - offset;
}

PRBool
nsSVGIntegrationUtils::HitTestFrameForEffects(nsIFrame* aFrame, const nsPoint& aPt)
{
  nsIFrame* firstFrame =
    nsLayoutUtils::GetFirstContinuationOrSpecialSibling(aFrame);
  nsRect userSpaceRect = GetNonSVGUserSpace(firstFrame);
  // get point relative to userSpaceRect
  nsPoint pt = aPt + aFrame->GetOffsetTo(firstFrame) - userSpaceRect.TopLeft();
  return nsSVGUtils::HitTestClip(firstFrame, pt);
}

class RegularFramePaintCallback : public nsSVGFilterPaintCallback
{
public:
  RegularFramePaintCallback(nsDisplayListBuilder* aBuilder,
                            nsDisplayList* aInnerList,
                            const nsPoint& aOffset)
    : mBuilder(aBuilder), mInnerList(aInnerList), mOffset(aOffset) {}

  virtual void Paint(nsSVGRenderState *aContext, nsIFrame *aTarget,
                     const nsIntRect* aDirtyRect)
  {
    nsIRenderingContext* ctx = aContext->GetRenderingContext(aTarget);
    gfxContext* gfxCtx = aContext->GetGfxContext();

    // We're expected to paint with 1 unit equal to 1 CSS pixel. But
    // mInnerList->Paint expects 1 unit to equal 1 device pixel. So
    // adjust.
    gfxFloat scale =
      nsPresContext::AppUnitsToFloatCSSPixels(aTarget->PresContext()->AppUnitsPerDevPixel());
    gfxCtx->Scale(scale, scale);

    nsIRenderingContext::AutoPushTranslation push(ctx, -mOffset.x, -mOffset.y);
    nsRect dirty;
    if (aDirtyRect) {
      dirty = nsIntRect::ToAppUnits(*aDirtyRect, nsIDeviceContext::AppUnitsPerCSSPixel());
      dirty += mOffset;
    } else {
      dirty = mInnerList->GetBounds(mBuilder);
    }
    mInnerList->Paint(mBuilder, ctx, dirty);
  }

private:
  nsDisplayListBuilder* mBuilder;
  nsDisplayList* mInnerList;
  nsPoint mOffset;
};

void
nsSVGIntegrationUtils::PaintFramesWithEffects(nsIRenderingContext* aCtx,
                                              nsIFrame* aEffectsFrame,
                                              const nsRect& aDirtyRect,
                                              nsDisplayListBuilder* aBuilder,
                                              nsDisplayList* aInnerList)
{
#ifdef DEBUG
  nsISVGChildFrame *svgChildFrame = do_QueryFrame(aEffectsFrame);
  NS_ASSERTION(!svgChildFrame, "Should never be called on an SVG frame");
#endif

  float opacity = aEffectsFrame->GetStyleDisplay()->mOpacity;
  if (opacity == 0.0f)
    return;

  /* Properties are added lazily and may have been removed by a restyle,
     so make sure all applicable ones are set again. */
  nsIFrame* firstFrame =
    nsLayoutUtils::GetFirstContinuationOrSpecialSibling(aEffectsFrame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);

  /* SVG defines the following rendering model:
   *
   *  1. Render geometry
   *  2. Apply filter
   *  3. Apply clipping, masking, group opacity
   *
   * We follow this, but perform a couple of optimizations:
   *
   * + Use cairo's clipPath when representable natively (single object
   *   clip region).
   *
   * + Merge opacity and masking if both used together.
   */

  PRBool isOK = PR_TRUE;
  nsSVGClipPathFrame *clipPathFrame = effectProperties.GetClipPathFrame(&isOK);
  nsSVGFilterFrame *filterFrame = effectProperties.GetFilterFrame(&isOK);
  nsSVGMaskFrame *maskFrame = effectProperties.GetMaskFrame(&isOK);

  PRBool isTrivialClip = clipPathFrame ? clipPathFrame->IsTrivial() : PR_TRUE;

  if (!isOK) {
    // Some resource is missing. We shouldn't paint anything.
    return;
  }

  gfxContext* gfx = aCtx->ThebesContext();
  gfxMatrix savedCTM = gfx->CurrentMatrix();
  nsSVGRenderState svgContext(aCtx);

  nsRect userSpaceRect = GetNonSVGUserSpace(firstFrame) + aBuilder->ToReferenceFrame(firstFrame);
  PRInt32 appUnitsPerDevPixel = aEffectsFrame->PresContext()->AppUnitsPerDevPixel();
  userSpaceRect = nsIntRect::ToAppUnits(nsRect::ToNearestPixels(userSpaceRect, appUnitsPerDevPixel), appUnitsPerDevPixel);
  aCtx->Translate(userSpaceRect.x, userSpaceRect.y);

  nsCOMPtr<nsIDOMSVGMatrix> matrix = GetInitialMatrix(aEffectsFrame);

  PRBool complexEffects = PR_FALSE;
  /* Check if we need to do additional operations on this child's
   * rendering, which necessitates rendering into another surface. */
  if (opacity != 1.0f || maskFrame || (clipPathFrame && !isTrivialClip)) {
    complexEffects = PR_TRUE;
    gfx->Save();
    gfx->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
  }

  /* If this frame has only a trivial clipPath, set up cairo's clipping now so
   * we can just do normal painting and get it clipped appropriately.
   */
  if (clipPathFrame && isTrivialClip) {
    gfx->Save();
    clipPathFrame->ClipPaint(&svgContext, aEffectsFrame, matrix);
  }

  /* Paint the child */
  if (filterFrame) {
    RegularFramePaintCallback paint(aBuilder, aInnerList, userSpaceRect.TopLeft());
    nsIntRect r = nsRect::ToOutsidePixels(aDirtyRect - userSpaceRect.TopLeft(), appUnitsPerDevPixel);
    filterFrame->FilterPaint(&svgContext, aEffectsFrame, &paint, &r);
  } else {
    gfx->SetMatrix(savedCTM);
    aInnerList->Paint(aBuilder, aCtx, aDirtyRect);
    aCtx->Translate(userSpaceRect.x, userSpaceRect.y);
  }

  if (clipPathFrame && isTrivialClip) {
    gfx->Restore();
  }

  /* No more effects, we're done. */
  if (!complexEffects) {
    gfx->SetMatrix(savedCTM);
    return;
  }

  gfx->PopGroupToSource();

  nsRefPtr<gfxPattern> maskSurface =
    maskFrame ? maskFrame->ComputeMaskAlpha(&svgContext, aEffectsFrame,
                                            matrix, opacity) : nsnull;

  nsRefPtr<gfxPattern> clipMaskSurface;
  if (clipPathFrame && !isTrivialClip) {
    gfx->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);

    nsresult rv = clipPathFrame->ClipPaint(&svgContext, aEffectsFrame, matrix);
    clipMaskSurface = gfx->PopGroup();

    if (NS_SUCCEEDED(rv) && clipMaskSurface) {
      // Still more set after clipping, so clip to another surface
      if (maskSurface || opacity != 1.0f) {
        gfx->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
        gfx->Mask(clipMaskSurface);
        gfx->PopGroupToSource();
      } else {
        gfx->Mask(clipMaskSurface);
      }
    }
  }

  if (maskSurface) {
    gfx->Mask(maskSurface);
  } else if (opacity != 1.0f) {
    gfx->Paint(opacity);
  }

  gfx->Restore();
  gfx->SetMatrix(savedCTM);
}

already_AddRefed<nsIDOMSVGMatrix>
nsSVGIntegrationUtils::GetInitialMatrix(nsIFrame* aNonSVGFrame)
{
  NS_ASSERTION(!aNonSVGFrame->IsFrameOfType(nsIFrame::eSVG),
               "SVG frames should not get here");
  PRInt32 appUnitsPerDevPixel = aNonSVGFrame->PresContext()->AppUnitsPerDevPixel();
  nsCOMPtr<nsIDOMSVGMatrix> matrix;
  float devPxPerCSSPx =
    1 / nsPresContext::AppUnitsToFloatCSSPixels(appUnitsPerDevPixel);
  NS_NewSVGMatrix(getter_AddRefs(matrix),
                  devPxPerCSSPx, 0.0f,
                  0.0f, devPxPerCSSPx);
  return matrix.forget();
}

gfxRect
nsSVGIntegrationUtils::GetSVGRectForNonSVGFrame(nsIFrame* aNonSVGFrame)
{
  NS_ASSERTION(!aNonSVGFrame->IsFrameOfType(nsIFrame::eSVG),
               "SVG frames should not get here");
  nsIFrame* firstFrame =
    nsLayoutUtils::GetFirstContinuationOrSpecialSibling(aNonSVGFrame);
  nsRect r = GetNonSVGUserSpace(firstFrame);
  nsPresContext* presContext = firstFrame->PresContext();
  return gfxRect(0, 0, presContext->AppUnitsToFloatCSSPixels(r.width),
                       presContext->AppUnitsToFloatCSSPixels(r.height));
}

gfxRect
nsSVGIntegrationUtils::GetSVGBBoxForNonSVGFrame(nsIFrame* aNonSVGFrame)
{
  NS_ASSERTION(!aNonSVGFrame->IsFrameOfType(nsIFrame::eSVG),
               "SVG frames should not get here");
  nsIFrame* firstFrame =
    nsLayoutUtils::GetFirstContinuationOrSpecialSibling(aNonSVGFrame);
  nsRect userSpaceRect = GetNonSVGUserSpace(firstFrame);
  nsRect r = GetSVGBBox(firstFrame, nsnull, nsRect(), userSpaceRect);
  gfxRect result(r.x, r.y, r.width, r.height);
  nsPresContext* presContext = aNonSVGFrame->PresContext();
  result.ScaleInverse(presContext->AppUnitsPerCSSPixel());
  return result;
}
