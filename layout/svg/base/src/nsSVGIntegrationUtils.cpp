/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGIntegrationUtils.h"

// Keep others in (case-insensitive) order:
#include "gfxDrawable.h"
#include "nsDisplayList.h"
#include "nsLayoutUtils.h"
#include "nsRenderingContext.h"
#include "nsSVGClipPathFrame.h"
#include "nsSVGEffects.h"
#include "nsSVGFilterFrame.h"
#include "nsSVGFilterPaintCallback.h"
#include "nsSVGMaskFrame.h"
#include "nsSVGPaintServerFrame.h"
#include "nsSVGUtils.h"

// ----------------------------------------------------------------------

/**
 * This class is used to get the pre-effects visual overflow rect of a frame,
 * or, in the case of a frame with continuations, to collect the union of the
 * pre-effects visual overflow rects of all the continuations. The result is
 * relative to the origin (top left corner of the border box) of the frame, or,
 * if the frame has continuations, the origin of the  _first_ continuation.
 */
class PreEffectsVisualOverflowCollector : public nsLayoutUtils::BoxCallback
{
public:
  /**
   * If the pre-effects visual overflow rect of the frame being examined
   * happens to be known, it can be passed in as aCurrentFrame and its
   * pre-effects visual overflow rect can be passed in as
   * aCurrentFrameOverflowArea. This is just an optimization to save a
   * frame property lookup - these arguments are optional.
   */
  PreEffectsVisualOverflowCollector(nsIFrame* aFirstContinuation,
                                    nsIFrame* aCurrentFrame,
                                    const nsRect& aCurrentFrameOverflowArea)
    : mFirstContinuation(aFirstContinuation)
    , mCurrentFrame(aCurrentFrame)
    , mCurrentFrameOverflowArea(aCurrentFrameOverflowArea)
  {
    NS_ASSERTION(!mFirstContinuation->GetPrevContinuation(),
                 "We want the first continuation here");
  }

  virtual void AddBox(nsIFrame* aFrame) {
    nsRect overflow = (aFrame == mCurrentFrame) ?
      mCurrentFrameOverflowArea : GetPreEffectsVisualOverflowRect(aFrame);
    mResult.UnionRect(mResult, overflow + aFrame->GetOffsetTo(mFirstContinuation));
  }

  nsRect GetResult() const {
    return mResult;
  }

private:

  static nsRect GetPreEffectsVisualOverflowRect(nsIFrame* aFrame) {
    nsRect* r = static_cast<nsRect*>
      (aFrame->Properties().Get(nsIFrame::PreEffectsBBoxProperty()));
    if (r) {
      return *r;
    }
    // Despite the fact that we're invoked for frames with SVG effects applied,
    // we can actually get here. All continuations and special siblings of a
    // frame with SVG effects applied will have the PreEffectsBBoxProperty
    // property set on them. Therefore, the frames that are passed to us will
    // always have that property set...well, with one exception. If the frames
    // for an element with SVG effects applied have been subject to an "IB
    // split", then the block frame(s) that caused the split will have been
    // wrapped in anonymous, inline-block, nsBlockFrames of pseudo-type
    // nsCSSAnonBoxes::mozAnonymousBlock. These "special sibling" anonymous
    // blocks will have the PreEffectsBBoxProperty property set on them, but
    // they will never be passed to us. Instead, we'll be passed the block
    // children that they wrap, which don't have the PreEffectsBBoxProperty
    // property set on them. This is actually okay. What we care about is
    // collecting the _pre_ effects visual overflow rects of the frames to
    // which the SVG effects have been applied. Since the IB split results in
    // any overflow rect adjustments for transforms, effects, etc. taking
    // place on the anonymous block wrappers, the wrapped children are left
    // with their overflow rects unaffected. In other words, calling
    // GetVisualOverflowRect() on the children will return their pre-effects
    // visual overflow rects, just as we need.
    //
    // A couple of tests that demonstrate the IB split and cause us to get here
    // are:
    //
    //  * reftests/svg/svg-integration/clipPath-html-06.xhtml
    //  * reftests/svg/svg-integration/clipPath-html-06-extref.xhtml
    //
    // If we ever got passed a frame with the PreTransformOverflowAreasProperty
    // property set, that would be bad, since then our GetVisualOverflowRect()
    // call would give us the post-effects, and post-transform, overflow rect.
    //
    NS_ASSERTION(aFrame->GetParent()->GetStyleContext()->GetPseudo() ==
                   nsCSSAnonBoxes::mozAnonymousBlock,
                 "How did we getting here, then?");
    NS_ASSERTION(!aFrame->Properties().Get(
                   aFrame->PreTransformOverflowAreasProperty()),
                 "GetVisualOverflowRect() won't return the pre-effects rect!");
    return aFrame->GetVisualOverflowRect();
  }

  nsIFrame*     mFirstContinuation;
  nsIFrame*     mCurrentFrame;
  const nsRect& mCurrentFrameOverflowArea;
  nsRect        mResult;
};

/**
 * Gets the union of the pre-effects visual overflow rects of all of a frame's
 * continuations, in "user space".
 */
static nsRect
GetPreEffectsVisualOverflowUnion(nsIFrame* aFirstContinuation,
                                 nsIFrame* aCurrentFrame,
                                 const nsRect& aCurrentFramePreEffectsOverflow,
                                 const nsPoint& aFirstContinuationToUserSpace)
{
  NS_ASSERTION(!aFirstContinuation->GetPrevContinuation(),
               "Need first continuation here");
  PreEffectsVisualOverflowCollector collector(aFirstContinuation,
                                              aCurrentFrame,
                                              aCurrentFramePreEffectsOverflow);
  // Compute union of all overflow areas relative to aFirstContinuation:
  nsLayoutUtils::GetAllInFlowBoxes(aFirstContinuation, &collector);
  // Return the result in user space:
  return collector.GetResult() + aFirstContinuationToUserSpace;
}


bool
nsSVGIntegrationUtils::UsingEffectsForFrame(const nsIFrame* aFrame)
{
  if (aFrame->IsFrameOfType(nsIFrame::eSVG)) {
    return false;
  }
  const nsStyleSVGReset *style = aFrame->GetStyleSVGReset();
  return (style->mFilter || style->mClipPath || style->mMask);
}

/* static */ nsPoint
nsSVGIntegrationUtils::GetOffsetToUserSpace(nsIFrame* aFrame)
{
  // We could allow aFrame to be any continuation, but since that would require
  // a GetPrevContinuation() virtual call and conditional returns, and since
  // all our current consumers always pass in the first continuation, we don't
  // currently bother.
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "Not first continuation");

  // The GetAllInFlowRectsUnion() call gets the union of the frame border-box
  // rects over all continuations, relative to the origin (top-left of the
  // border box) of its second argument (here, aFrame, the first continuation).
  return -nsLayoutUtils::GetAllInFlowRectsUnion(aFrame, aFrame).TopLeft();
}

/* static */ nsSize
nsSVGIntegrationUtils::GetContinuationUnionSize(nsIFrame* aNonSVGFrame)
{
  NS_ASSERTION(!aNonSVGFrame->IsFrameOfType(nsIFrame::eSVG),
               "SVG frames should not get here");
  nsIFrame* firstFrame =
    nsLayoutUtils::GetFirstContinuationOrSpecialSibling(aNonSVGFrame);
  return nsLayoutUtils::GetAllInFlowRectsUnion(firstFrame, firstFrame).Size();
}

/* static */ gfxSize
nsSVGIntegrationUtils::GetSVGCoordContextForNonSVGFrame(nsIFrame* aNonSVGFrame)
{
  NS_ASSERTION(!aNonSVGFrame->IsFrameOfType(nsIFrame::eSVG),
               "SVG frames should not get here");
  nsIFrame* firstFrame =
    nsLayoutUtils::GetFirstContinuationOrSpecialSibling(aNonSVGFrame);
  nsRect r = nsLayoutUtils::GetAllInFlowRectsUnion(firstFrame, firstFrame);
  nsPresContext* presContext = firstFrame->PresContext();
  return gfxSize(presContext->AppUnitsToFloatCSSPixels(r.width),
                 presContext->AppUnitsToFloatCSSPixels(r.height));
}

gfxRect
nsSVGIntegrationUtils::GetSVGBBoxForNonSVGFrame(nsIFrame* aNonSVGFrame)
{
  NS_ASSERTION(!aNonSVGFrame->IsFrameOfType(nsIFrame::eSVG),
               "SVG frames should not get here");
  nsIFrame* firstFrame =
    nsLayoutUtils::GetFirstContinuationOrSpecialSibling(aNonSVGFrame);
  // 'r' is in "user space":
  nsRect r = GetPreEffectsVisualOverflowUnion(firstFrame, nsnull, nsRect(),
                                              GetOffsetToUserSpace(firstFrame));
  return nsLayoutUtils::RectToGfxRect(r,
           aNonSVGFrame->PresContext()->AppUnitsPerCSSPixel());
}

// XXX Since we're called during reflow, this method is broken for frames with
// continuations. When we're called for a frame with continuations, we're
// called for each continuation in turn as it's reflowed. However, it isn't
// until the last continuation is reflowed that this method's
// GetOffsetToUserSpace() and GetPreEffectsVisualOverflowUnion() calls will
// obtain valid border boxes for all the continuations. As a result, we'll
// end up returning bogus post-filter visual overflow rects for all the prior
// continuations. Unfortunately, by the time the last continuation is
// reflowed, it's too late to go back and set and propagate the overflow
// rects on the previous continuations.
//
// The reason that we need to pass an override bbox to
// GetPreEffectsVisualOverflowUnion rather than just letting it call into our
// GetSVGBBoxForNonSVGFrame method is because we get called by
// ComputeOutlineAndEffectsRect when it has been called with
// aStoreRectProperties set to false. In this case the pre-effects visual
// overflow rect that it has been passed may be different to that stored on
// aFrame, resulting in a different bbox.
//
// XXXjwatt The pre-effects visual overflow rect passed to
// ComputeOutlineAndEffectsRect won't include continuation overflows, so
// for frames with continuation the following filter analysis will likely end
// up being carried out with a bbox created as if the frame didn't have
// continuations.
//
// XXXjwatt Using aPreEffectsOverflowRect to create the bbox isn't really right
// for SVG frames, since for SVG frames the SVG spec defines the bbox to be
// something quite different to the pre-effects visual overflow rect. However,
// we're essentially calculating an invalidation area here, and using the
// pre-effects overflow rect will actually overestimate that area which, while
// being a bit wasteful, isn't otherwise a problem.
//
nsRect
  nsSVGIntegrationUtils::
    ComputePostEffectsVisualOverflowRect(nsIFrame* aFrame,
                                         const nsRect& aPreEffectsOverflowRect)
{
  NS_ASSERTION(!(aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT),
                 "Don't call this on SVG child frames");

  nsIFrame* firstFrame =
    nsLayoutUtils::GetFirstContinuationOrSpecialSibling(aFrame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);
  nsSVGFilterFrame *filterFrame = effectProperties.mFilter ?
    effectProperties.mFilter->GetFilterFrame() : nsnull;
  if (!filterFrame)
    return aPreEffectsOverflowRect;

  // Create an override bbox - see comment above:
  nsPoint firstFrameToUserSpace = GetOffsetToUserSpace(firstFrame);
  // overrideBBox is in "user space", in _CSS_ pixels:
  // XXX Why are we rounding out to pixel boundaries? We don't do that in
  // GetSVGBBoxForNonSVGFrame, and it doesn't appear to be necessary.
  gfxRect overrideBBox =
    nsLayoutUtils::RectToGfxRect(
      GetPreEffectsVisualOverflowUnion(firstFrame, aFrame,
                                       aPreEffectsOverflowRect,
                                       firstFrameToUserSpace),
      aFrame->PresContext()->AppUnitsPerCSSPixel());
  overrideBBox.RoundOut();

  nsRect overflowRect =
    filterFrame->GetPostFilterBounds(firstFrame, &overrideBBox);

  // Return overflowRect relative to aFrame, rather than "user space":
  return overflowRect - (aFrame->GetOffsetTo(firstFrame) + firstFrameToUserSpace);
}

nsRect
nsSVGIntegrationUtils::AdjustInvalidAreaForSVGEffects(nsIFrame* aFrame,
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

  nsSVGFilterProperty *prop = nsSVGEffects::GetFilterProperty(firstFrame);
  if (!prop || !prop->IsInObserverList()) {
    return aInvalidRect;
  }

  nsSVGFilterFrame* filterFrame = prop->GetFilterFrame();
  if (!filterFrame) {
    // The frame is either not there or not currently available,
    // perhaps because we're in the middle of tearing stuff down.
    // Be conservative.
    return aFrame->GetVisualOverflowRect();
  }

  // Convert aInvalidRect into "user space" in app units:
  nsPoint toUserSpace =
    aFrame->GetOffsetTo(firstFrame) + GetOffsetToUserSpace(firstFrame);
  nsRect preEffectsRect = aInvalidRect + toUserSpace;

  // Return ther result, relative to aFrame, not in user space:
  return filterFrame->GetPostFilterDirtyArea(firstFrame, preEffectsRect) -
           toUserSpace;
}

nsRect
nsSVGIntegrationUtils::GetRequiredSourceForInvalidArea(nsIFrame* aFrame,
                                                       const nsRect& aDirtyRect)
{
  // Don't bother calling GetEffectProperties; the filter property should
  // already have been set up during reflow/ComputeFrameEffectsRect
  nsIFrame* firstFrame =
    nsLayoutUtils::GetFirstContinuationOrSpecialSibling(aFrame);
  nsSVGFilterFrame* filterFrame =
    nsSVGEffects::GetFilterFrame(firstFrame);
  if (!filterFrame)
    return aDirtyRect;
  
  // Convert aDirtyRect into "user space" in app units:
  nsPoint toUserSpace =
    aFrame->GetOffsetTo(firstFrame) + GetOffsetToUserSpace(firstFrame);
  nsRect postEffectsRect = aDirtyRect + toUserSpace;

  // Return ther result, relative to aFrame, not in user space:
  return filterFrame->GetPreFilterNeededArea(firstFrame, postEffectsRect) -
           toUserSpace;
}

bool
nsSVGIntegrationUtils::HitTestFrameForEffects(nsIFrame* aFrame, const nsPoint& aPt)
{
  nsIFrame* firstFrame =
    nsLayoutUtils::GetFirstContinuationOrSpecialSibling(aFrame);
  // Convert aPt to user space:
  nsPoint toUserSpace =
    aFrame->GetOffsetTo(firstFrame) + GetOffsetToUserSpace(firstFrame);
  nsPoint pt = aPt + toUserSpace;
  return nsSVGUtils::HitTestClip(firstFrame, pt);
}

class RegularFramePaintCallback : public nsSVGFilterPaintCallback
{
public:
  RegularFramePaintCallback(nsDisplayListBuilder* aBuilder,
                            nsDisplayList* aInnerList,
                            nsIFrame* aFrame,
                            const nsPoint& aOffset)
    : mBuilder(aBuilder), mInnerList(aInnerList), mFrame(aFrame),
      mOffset(aOffset) {}

  virtual void Paint(nsRenderingContext *aContext, nsIFrame *aTarget,
                     const nsIntRect* aDirtyRect)
  {
    nsRenderingContext::AutoPushTranslation push(aContext, -mOffset);
    mInnerList->PaintForFrame(mBuilder, aContext, mFrame, nsDisplayList::PAINT_DEFAULT);
  }

private:
  nsDisplayListBuilder* mBuilder;
  nsDisplayList* mInnerList;
  nsIFrame* mFrame;
  nsPoint mOffset;
};

void
nsSVGIntegrationUtils::PaintFramesWithEffects(nsRenderingContext* aCtx,
                                              nsIFrame* aFrame,
                                              const nsRect& aDirtyRect,
                                              nsDisplayListBuilder* aBuilder,
                                              nsDisplayList* aInnerList)
{
#ifdef DEBUG
  nsISVGChildFrame *svgChildFrame = do_QueryFrame(aFrame);
  NS_ASSERTION(!svgChildFrame, "Should never be called on an SVG frame");
#endif

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

  float opacity = aFrame->GetStyleDisplay()->mOpacity;
  if (opacity == 0.0f) {
    return;
  }

  /* Properties are added lazily and may have been removed by a restyle,
     so make sure all applicable ones are set again. */
  nsIFrame* firstFrame =
    nsLayoutUtils::GetFirstContinuationOrSpecialSibling(aFrame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);

  bool isOK = true;
  nsSVGClipPathFrame *clipPathFrame = effectProperties.GetClipPathFrame(&isOK);
  nsSVGFilterFrame *filterFrame = effectProperties.GetFilterFrame(&isOK);
  nsSVGMaskFrame *maskFrame = effectProperties.GetMaskFrame(&isOK);
  if (!isOK) {
    return; // Some resource is missing. We shouldn't paint anything.
  }

  bool isTrivialClip = clipPathFrame ? clipPathFrame->IsTrivial() : true;

  gfxContext* gfx = aCtx->ThebesContext();
  gfxContextMatrixAutoSaveRestore matrixAutoSaveRestore(gfx);

  PRInt32 appUnitsPerDevPixel = 
    aFrame->PresContext()->AppUnitsPerDevPixel();
  nsPoint firstFrameOffset = GetOffsetToUserSpace(firstFrame);
  nsPoint offset = (aBuilder->ToReferenceFrame(firstFrame) - firstFrameOffset).
                     ToNearestPixels(appUnitsPerDevPixel).
                     ToAppUnits(appUnitsPerDevPixel);
  aCtx->Translate(offset);

  gfxMatrix cssPxToDevPxMatrix = GetCSSPxToDevPxMatrix(aFrame);

  bool complexEffects = false;
  /* Check if we need to do additional operations on this child's
   * rendering, which necessitates rendering into another surface. */
  if (opacity != 1.0f || maskFrame || (clipPathFrame && !isTrivialClip)) {
    complexEffects = true;
    gfx->Save();
    aCtx->IntersectClip(aFrame->GetVisualOverflowRect());
    gfx->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
  }

  /* If this frame has only a trivial clipPath, set up cairo's clipping now so
   * we can just do normal painting and get it clipped appropriately.
   */
  if (clipPathFrame && isTrivialClip) {
    gfx->Save();
    clipPathFrame->ClipPaint(aCtx, aFrame, cssPxToDevPxMatrix);
  }

  /* Paint the child */
  if (filterFrame) {
    RegularFramePaintCallback callback(aBuilder, aInnerList, aFrame,
                                       offset);
    nsRect dirtyRect = aDirtyRect - offset;
    filterFrame->PaintFilteredFrame(aCtx, aFrame, &callback, &dirtyRect);
  } else {
    gfx->SetMatrix(matrixAutoSaveRestore.Matrix());
    aInnerList->PaintForFrame(aBuilder, aCtx, aFrame,
                              nsDisplayList::PAINT_DEFAULT);
    aCtx->Translate(offset);
  }

  if (clipPathFrame && isTrivialClip) {
    gfx->Restore();
  }

  /* No more effects, we're done. */
  if (!complexEffects) {
    return;
  }

  gfx->PopGroupToSource();

  nsRefPtr<gfxPattern> maskSurface =
    maskFrame ? maskFrame->ComputeMaskAlpha(aCtx, aFrame,
                                            cssPxToDevPxMatrix, opacity) : nsnull;

  nsRefPtr<gfxPattern> clipMaskSurface;
  if (clipPathFrame && !isTrivialClip) {
    gfx->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);

    nsresult rv = clipPathFrame->ClipPaint(aCtx, aFrame, cssPxToDevPxMatrix);
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
}

gfxMatrix
nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(nsIFrame* aNonSVGFrame)
{
  PRInt32 appUnitsPerDevPixel = aNonSVGFrame->PresContext()->AppUnitsPerDevPixel();
  float devPxPerCSSPx =
    1 / nsPresContext::AppUnitsToFloatCSSPixels(appUnitsPerDevPixel);

  return gfxMatrix(devPxPerCSSPx, 0.0,
                   0.0, devPxPerCSSPx,
                   0.0, 0.0);
}

class PaintFrameCallback : public gfxDrawingCallback {
public:
  PaintFrameCallback(nsIFrame* aFrame,
                     nsIFrame* aTarget,
                     const nsSize aPaintServerSize,
                     const gfxIntSize aRenderSize)
   : mFrame(aFrame)
   , mTarget(aTarget)
   , mPaintServerSize(aPaintServerSize)
   , mRenderSize(aRenderSize)
  {}
  virtual bool operator()(gfxContext* aContext,
                            const gfxRect& aFillRect,
                            const gfxPattern::GraphicsFilter& aFilter,
                            const gfxMatrix& aTransform);
private:
  nsIFrame* mFrame;
  nsIFrame* mTarget;
  nsSize mPaintServerSize;
  gfxIntSize mRenderSize;
};

bool
PaintFrameCallback::operator()(gfxContext* aContext,
                               const gfxRect& aFillRect,
                               const gfxPattern::GraphicsFilter& aFilter,
                               const gfxMatrix& aTransform)
{
  if (mFrame->GetStateBits() & NS_FRAME_DRAWING_AS_PAINTSERVER)
    return false;

  mFrame->AddStateBits(NS_FRAME_DRAWING_AS_PAINTSERVER);

  nsRenderingContext context;
  context.Init(mFrame->PresContext()->DeviceContext(), aContext);
  aContext->Save();

  // Clip to aFillRect so that we don't paint outside.
  aContext->NewPath();
  aContext->Rectangle(aFillRect);
  aContext->Clip();

  aContext->Multiply(gfxMatrix(aTransform).Invert());

  // nsLayoutUtils::PaintFrame will anchor its painting at mFrame. But we want
  // to have it anchored at the top left corner of the bounding box of all of
  // mFrame's continuations. So we add a translation transform.
  PRInt32 appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  nsPoint offset = nsSVGIntegrationUtils::GetOffsetToUserSpace(mFrame);
  gfxPoint devPxOffset = gfxPoint(offset.x, offset.y) / appUnitsPerDevPixel;
  aContext->Multiply(gfxMatrix().Translate(devPxOffset));

  gfxSize paintServerSize =
    gfxSize(mPaintServerSize.width, mPaintServerSize.height) /
      mFrame->PresContext()->AppUnitsPerDevPixel();

  // nsLayoutUtils::PaintFrame wants to render with paintServerSize, but we
  // want it to render with mRenderSize, so we need to set up a scale transform.
  gfxFloat scaleX = mRenderSize.width / paintServerSize.width;
  gfxFloat scaleY = mRenderSize.height / paintServerSize.height;
  gfxMatrix scaleMatrix = gfxMatrix().Scale(scaleX, scaleY);
  aContext->Multiply(scaleMatrix);

  // Draw.
  nsRect dirty(-offset.x, -offset.y,
               mPaintServerSize.width, mPaintServerSize.height);
  nsLayoutUtils::PaintFrame(&context, mFrame,
                            dirty, NS_RGBA(0, 0, 0, 0),
                            nsLayoutUtils::PAINT_IN_TRANSFORM |
                            nsLayoutUtils::PAINT_ALL_CONTINUATIONS);

  aContext->Restore();

  mFrame->RemoveStateBits(NS_FRAME_DRAWING_AS_PAINTSERVER);

  return true;
}

static already_AddRefed<gfxDrawable>
DrawableFromPaintServer(nsIFrame*         aFrame,
                        nsIFrame*         aTarget,
                        const nsSize&     aPaintServerSize,
                        const gfxIntSize& aRenderSize)
{
  // aPaintServerSize is the size that would be filled when using
  // background-repeat:no-repeat and background-size:auto. For normal background
  // images, this would be the intrinsic size of the image; for gradients and
  // patterns this would be the whole target frame fill area.
  // aRenderSize is what we will be actually filling after accounting for
  // background-size.
  if (aFrame->IsFrameOfType(nsIFrame::eSVGPaintServer)) {
    // aFrame is either a pattern or a gradient. These fill the whole target
    // frame by default, so aPaintServerSize is the whole target background fill
    // area.
    nsSVGPaintServerFrame* server =
      static_cast<nsSVGPaintServerFrame*>(aFrame);

    gfxRect overrideBounds(0, 0,
                           aPaintServerSize.width, aPaintServerSize.height);
    overrideBounds.ScaleInverse(aFrame->PresContext()->AppUnitsPerDevPixel());
    nsRefPtr<gfxPattern> pattern =
      server->GetPaintServerPattern(aTarget, &nsStyleSVG::mFill, 1.0, &overrideBounds);

    if (!pattern)
      return nsnull;

    // pattern is now set up to fill aPaintServerSize. But we want it to
    // fill aRenderSize, so we need to add a scaling transform.
    // We couldn't just have set overrideBounds to aRenderSize - it would have
    // worked for gradients, but for patterns it would result in a different
    // pattern size.
    gfxFloat scaleX = overrideBounds.Width() / aRenderSize.width;
    gfxFloat scaleY = overrideBounds.Height() / aRenderSize.height;
    gfxMatrix scaleMatrix = gfxMatrix().Scale(scaleX, scaleY);
    pattern->SetMatrix(scaleMatrix.Multiply(pattern->GetMatrix()));
    nsRefPtr<gfxDrawable> drawable =
      new gfxPatternDrawable(pattern, aRenderSize);
    return drawable.forget();
  }

  // We don't want to paint into a surface as long as we don't need to, so we
  // set up a drawing callback.
  nsRefPtr<gfxDrawingCallback> cb =
    new PaintFrameCallback(aFrame, aTarget, aPaintServerSize, aRenderSize);
  nsRefPtr<gfxDrawable> drawable = new gfxCallbackDrawable(cb, aRenderSize);
  return drawable.forget();
}

/* static */ void
nsSVGIntegrationUtils::DrawPaintServer(nsRenderingContext* aRenderingContext,
                                       nsIFrame*            aTarget,
                                       nsIFrame*            aPaintServer,
                                       gfxPattern::GraphicsFilter aFilter,
                                       const nsRect&        aDest,
                                       const nsRect&        aFill,
                                       const nsPoint&       aAnchor,
                                       const nsRect&        aDirty,
                                       const nsSize&        aPaintServerSize)
{
  if (aDest.IsEmpty() || aFill.IsEmpty())
    return;

  PRInt32 appUnitsPerDevPixel = aTarget->PresContext()->AppUnitsPerDevPixel();
  nsRect destSize = aDest - aDest.TopLeft();
  nsIntSize roundedOut = destSize.ToOutsidePixels(appUnitsPerDevPixel).Size();
  gfxIntSize imageSize(roundedOut.width, roundedOut.height);
  nsRefPtr<gfxDrawable> drawable =
    DrawableFromPaintServer(aPaintServer, aTarget, aPaintServerSize, imageSize);

  if (drawable) {
    nsLayoutUtils::DrawPixelSnapped(aRenderingContext, drawable, aFilter,
                                    aDest, aFill, aAnchor, aDirty);
  }
}
