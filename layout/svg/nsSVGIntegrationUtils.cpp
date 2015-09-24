/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGIntegrationUtils.h"

// Keep others in (case-insensitive) order:
#include "gfxDrawable.h"
#include "nsCSSAnonBoxes.h"
#include "nsDisplayList.h"
#include "nsFilterInstance.h"
#include "nsLayoutUtils.h"
#include "nsRenderingContext.h"
#include "nsSVGClipPathFrame.h"
#include "nsSVGEffects.h"
#include "nsSVGElement.h"
#include "nsSVGFilterPaintCallback.h"
#include "nsSVGMaskFrame.h"
#include "nsSVGPaintServerFrame.h"
#include "nsSVGUtils.h"
#include "FrameLayerBuilder.h"
#include "BasicLayers.h"
#include "mozilla/gfx/Point.h"

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::gfx;

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

  virtual void AddBox(nsIFrame* aFrame) override {
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
    // we can actually get here. All continuations and IB split siblings of a
    // frame with SVG effects applied will have the PreEffectsBBoxProperty
    // property set on them. Therefore, the frames that are passed to us will
    // always have that property set...well, with one exception. If the frames
    // for an element with SVG effects applied have been subject to an "IB
    // split", then the block frame(s) that caused the split will have been
    // wrapped in anonymous, inline-block, nsBlockFrames of pseudo-type
    // nsCSSAnonBoxes::mozAnonymousBlock. These "IB split sibling" anonymous
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
    NS_ASSERTION(aFrame->GetParent()->StyleContext()->GetPseudo() ==
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
  // Even when SVG display lists are disabled, returning true for SVG frames
  // does not adversely affect any of our callers. Therefore we don't bother
  // checking the SDL prefs here, since we don't know if we're being called for
  // painting or hit-testing anyway.
  const nsStyleSVGReset *style = aFrame->StyleSVGReset();
  return (style->HasFilters() ||
          style->mClipPath.GetType() != NS_STYLE_CLIP_PATH_NONE || style->mMask);
}

// For non-SVG frames, this gives the offset to the frame's "user space".
// For SVG frames, this returns a zero offset.
static nsPoint
GetOffsetToBoundingBox(nsIFrame* aFrame)
{
  if ((aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT)) {
    // Do NOT call GetAllInFlowRectsUnion for SVG - it will get the
    // covered region relative to the nsSVGOuterSVGFrame, which is absolutely
    // not what we want. SVG frames are always in user space, so they have
    // no offset adjustment to make.
    return nsPoint();
  }
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
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(aNonSVGFrame);
  return nsLayoutUtils::GetAllInFlowRectsUnion(firstFrame, firstFrame).Size();
}

/* static */ gfx::Size
nsSVGIntegrationUtils::GetSVGCoordContextForNonSVGFrame(nsIFrame* aNonSVGFrame)
{
  NS_ASSERTION(!aNonSVGFrame->IsFrameOfType(nsIFrame::eSVG),
               "SVG frames should not get here");
  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(aNonSVGFrame);
  nsRect r = nsLayoutUtils::GetAllInFlowRectsUnion(firstFrame, firstFrame);
  nsPresContext* presContext = firstFrame->PresContext();
  return gfx::Size(presContext->AppUnitsToFloatCSSPixels(r.width),
                   presContext->AppUnitsToFloatCSSPixels(r.height));
}

gfxRect
nsSVGIntegrationUtils::GetSVGBBoxForNonSVGFrame(nsIFrame* aNonSVGFrame)
{
  NS_ASSERTION(!aNonSVGFrame->IsFrameOfType(nsIFrame::eSVG),
               "SVG frames should not get here");
  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(aNonSVGFrame);
  // 'r' is in "user space":
  nsRect r = GetPreEffectsVisualOverflowUnion(firstFrame, nullptr, nsRect(),
                                              GetOffsetToBoundingBox(firstFrame));
  return nsLayoutUtils::RectToGfxRect(r,
           aNonSVGFrame->PresContext()->AppUnitsPerCSSPixel());
}

// XXX Since we're called during reflow, this method is broken for frames with
// continuations. When we're called for a frame with continuations, we're
// called for each continuation in turn as it's reflowed. However, it isn't
// until the last continuation is reflowed that this method's
// GetOffsetToBoundingBox() and GetPreEffectsVisualOverflowUnion() calls will
// obtain valid border boxes for all the continuations. As a result, we'll
// end up returning bogus post-filter visual overflow rects for all the prior
// continuations. Unfortunately, by the time the last continuation is
// reflowed, it's too late to go back and set and propagate the overflow
// rects on the previous continuations.
//
// The reason that we need to pass an override bbox to
// GetPreEffectsVisualOverflowUnion rather than just letting it call into our
// GetSVGBBoxForNonSVGFrame method is because we get called by
// ComputeEffectsRect when it has been called with
// aStoreRectProperties set to false. In this case the pre-effects visual
// overflow rect that it has been passed may be different to that stored on
// aFrame, resulting in a different bbox.
//
// XXXjwatt The pre-effects visual overflow rect passed to
// ComputeEffectsRect won't include continuation overflows, so
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
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(aFrame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);
  if (!effectProperties.HasValidFilter()) {
    return aPreEffectsOverflowRect;
  }

  // Create an override bbox - see comment above:
  nsPoint firstFrameToBoundingBox = GetOffsetToBoundingBox(firstFrame);
  // overrideBBox is in "user space", in _CSS_ pixels:
  // XXX Why are we rounding out to pixel boundaries? We don't do that in
  // GetSVGBBoxForNonSVGFrame, and it doesn't appear to be necessary.
  gfxRect overrideBBox =
    nsLayoutUtils::RectToGfxRect(
      GetPreEffectsVisualOverflowUnion(firstFrame, aFrame,
                                       aPreEffectsOverflowRect,
                                       firstFrameToBoundingBox),
      aFrame->PresContext()->AppUnitsPerCSSPixel());
  overrideBBox.RoundOut();

  nsRect overflowRect =
    nsFilterInstance::GetPostFilterBounds(firstFrame, &overrideBBox);

  // Return overflowRect relative to aFrame, rather than "user space":
  return overflowRect - (aFrame->GetOffsetTo(firstFrame) + firstFrameToBoundingBox);
}

nsIntRegion
nsSVGIntegrationUtils::AdjustInvalidAreaForSVGEffects(nsIFrame* aFrame,
                                                      const nsPoint& aToReferenceFrame,
                                                      const nsIntRegion& aInvalidRegion)
{
  if (aInvalidRegion.IsEmpty()) {
    return nsIntRect();
  }

  // Don't bother calling GetEffectProperties; the filter property should
  // already have been set up during reflow/ComputeFrameEffectsRect
  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(aFrame);
  nsSVGFilterProperty *prop = nsSVGEffects::GetFilterProperty(firstFrame);
  if (!prop || !prop->IsInObserverLists()) {
    return aInvalidRegion;
  }

  int32_t appUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();

  if (!prop || !prop->ReferencesValidResources()) {
    // The frame is either not there or not currently available,
    // perhaps because we're in the middle of tearing stuff down.
    // Be conservative, return our visual overflow rect relative
    // to the reference frame.
    nsRect overflow = aFrame->GetVisualOverflowRect() + aToReferenceFrame;
    return overflow.ToOutsidePixels(appUnitsPerDevPixel);
  }

  // Convert aInvalidRegion into bounding box frame space in app units:
  nsPoint toBoundingBox =
    aFrame->GetOffsetTo(firstFrame) + GetOffsetToBoundingBox(firstFrame);
  // The initial rect was relative to the reference frame, so we need to
  // remove that offset to get a rect relative to the current frame.
  toBoundingBox -= aToReferenceFrame;
  nsRegion preEffectsRegion = aInvalidRegion.ToAppUnits(appUnitsPerDevPixel).MovedBy(toBoundingBox);

  // Adjust the dirty area for effects, and shift it back to being relative to
  // the reference frame.
  nsRegion result = nsFilterInstance::GetPostFilterDirtyArea(firstFrame,
    preEffectsRegion).MovedBy(-toBoundingBox);
  // Return the result, in pixels relative to the reference frame.
  return result.ToOutsidePixels(appUnitsPerDevPixel);
}

nsRect
nsSVGIntegrationUtils::GetRequiredSourceForInvalidArea(nsIFrame* aFrame,
                                                       const nsRect& aDirtyRect)
{
  // Don't bother calling GetEffectProperties; the filter property should
  // already have been set up during reflow/ComputeFrameEffectsRect
  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(aFrame);
  nsSVGFilterProperty *prop = nsSVGEffects::GetFilterProperty(firstFrame);
  if (!prop || !prop->ReferencesValidResources()) {
    return aDirtyRect;
  }

  // Convert aDirtyRect into "user space" in app units:
  nsPoint toUserSpace =
    aFrame->GetOffsetTo(firstFrame) + GetOffsetToBoundingBox(firstFrame);
  nsRect postEffectsRect = aDirtyRect + toUserSpace;

  // Return ther result, relative to aFrame, not in user space:
  return nsFilterInstance::GetPreFilterNeededArea(firstFrame, postEffectsRect).GetBounds()
    - toUserSpace;
}

bool
nsSVGIntegrationUtils::HitTestFrameForEffects(nsIFrame* aFrame, const nsPoint& aPt)
{
  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(aFrame);
  // Convert aPt to user space:
  nsPoint toUserSpace;
  if (aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT) {
    // XXXmstange Isn't this wrong for svg:use and innerSVG frames?
    toUserSpace = aFrame->GetPosition();
  } else {
    toUserSpace =
      aFrame->GetOffsetTo(firstFrame) + GetOffsetToBoundingBox(firstFrame);
  }
  nsPoint pt = aPt + toUserSpace;
  gfxPoint userSpacePt =
    gfxPoint(pt.x, pt.y) / aFrame->PresContext()->AppUnitsPerCSSPixel();
  return nsSVGUtils::HitTestClip(firstFrame, userSpacePt);
}

class RegularFramePaintCallback : public nsSVGFilterPaintCallback
{
public:
  RegularFramePaintCallback(nsDisplayListBuilder* aBuilder,
                            LayerManager* aManager,
                            const nsPoint& aOffset)
    : mBuilder(aBuilder), mLayerManager(aManager),
      mOffset(aOffset) {}

  virtual void Paint(gfxContext& aContext, nsIFrame *aTarget,
                     const gfxMatrix& aTransform,
                     const nsIntRect* aDirtyRect) override
  {
    BasicLayerManager* basic = static_cast<BasicLayerManager*>(mLayerManager);
    basic->SetTarget(&aContext);

    gfxPoint devPixelOffset =
      nsLayoutUtils::PointToGfxPoint(-mOffset,
                                     aTarget->PresContext()->AppUnitsPerDevPixel());

    gfxContextMatrixAutoSaveRestore autoSR(&aContext);
    aContext.SetMatrix(aContext.CurrentMatrix().Translate(devPixelOffset));

    mLayerManager->EndTransaction(FrameLayerBuilder::DrawPaintedLayer, mBuilder);
  }

private:
  nsDisplayListBuilder* mBuilder;
  LayerManager* mLayerManager;
  nsPoint mOffset;
};

void
nsSVGIntegrationUtils::PaintFramesWithEffects(gfxContext& aContext,
                                              nsIFrame* aFrame,
                                              const nsRect& aDirtyRect,
                                              nsDisplayListBuilder* aBuilder,
                                              LayerManager *aLayerManager)
{
#ifdef DEBUG
  NS_ASSERTION(!(aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT) ||
               (NS_SVGDisplayListPaintingEnabled() &&
                !(aFrame->GetStateBits() & NS_FRAME_IS_NONDISPLAY)),
               "Should not use nsSVGIntegrationUtils on this SVG frame");
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

  const nsIContent* content = aFrame->GetContent();
  bool hasSVGLayout = (aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT);
  if (hasSVGLayout) {
    nsISVGChildFrame *svgChildFrame = do_QueryFrame(aFrame);
    if (!svgChildFrame || !aFrame->GetContent()->IsSVGElement()) {
      NS_ASSERTION(false, "why?");
      return;
    }
    if (!static_cast<const nsSVGElement*>(content)->HasValidDimensions()) {
      return; // The SVG spec says not to draw _anything_
    }
  }

  float opacity = aFrame->StyleDisplay()->mOpacity;
  if (opacity == 0.0f) {
    return;
  }
  if (opacity != 1.0f &&
      hasSVGLayout && nsSVGUtils::CanOptimizeOpacity(aFrame)) {
    opacity = 1.0f;
  }

  /* Properties are added lazily and may have been removed by a restyle,
     so make sure all applicable ones are set again. */
  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(aFrame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);

  bool isOK = effectProperties.HasNoFilterOrHasValidFilter();
  nsSVGClipPathFrame *clipPathFrame = effectProperties.GetClipPathFrame(&isOK);
  nsSVGMaskFrame *maskFrame = effectProperties.GetMaskFrame(&isOK);
  if (!isOK) {
    return; // Some resource is missing. We shouldn't paint anything.
  }

  bool isTrivialClip = clipPathFrame ? clipPathFrame->IsTrivial() : true;

  DrawTarget* drawTarget = aContext.GetDrawTarget();
  gfxContextMatrixAutoSaveRestore matrixAutoSaveRestore(&aContext);

  nsPoint firstFrameOffset = GetOffsetToBoundingBox(firstFrame);
  nsPoint offsetToBoundingBox = aBuilder->ToReferenceFrame(firstFrame) - firstFrameOffset;
  if (!firstFrame->IsFrameOfType(nsIFrame::eSVG)) {
    /* Snap the offset if the reference frame is not a SVG frame,
     * since other frames will be snapped to pixel when rendering. */
    offsetToBoundingBox = nsPoint(
      aFrame->PresContext()->RoundAppUnitsToNearestDevPixels(offsetToBoundingBox.x),
      aFrame->PresContext()->RoundAppUnitsToNearestDevPixels(offsetToBoundingBox.y));
  }

  // After applying only "offsetToBoundingBox", aCtx would have its origin at
  // the top left corner of aFrame's bounding box (over all continuations).
  // However, SVG painting needs the origin to be located at the origin of the
  // SVG frame's "user space", i.e. the space in which, for example, the
  // frame's BBox lives.
  // SVG geometry frames and foreignObject frames apply their own offsets, so
  // their position is relative to their user space. So for these frame types,
  // if we want aCtx to be in user space, we first need to subtract the
  // frame's position so that SVG painting can later add it again and the
  // frame is painted in the right place.

  gfxPoint toUserSpaceGfx = nsSVGUtils::FrameSpaceInCSSPxToUserSpaceOffset(aFrame);
  nsPoint toUserSpace(nsPresContext::CSSPixelsToAppUnits(float(toUserSpaceGfx.x)),
                      nsPresContext::CSSPixelsToAppUnits(float(toUserSpaceGfx.y)));
  nsPoint offsetToUserSpace = offsetToBoundingBox - toUserSpace;

  NS_ASSERTION(hasSVGLayout || offsetToBoundingBox == offsetToUserSpace,
               "For non-SVG frames there shouldn't be any additional offset");

  gfxPoint devPixelOffsetToUserSpace =
    nsLayoutUtils::PointToGfxPoint(offsetToUserSpace,
                                   aFrame->PresContext()->AppUnitsPerDevPixel());
  aContext.SetMatrix(aContext.CurrentMatrix().Translate(devPixelOffsetToUserSpace));

  gfxMatrix cssPxToDevPxMatrix = GetCSSPxToDevPxMatrix(aFrame);

  bool complexEffects = false;
  /* Check if we need to do additional operations on this child's
   * rendering, which necessitates rendering into another surface. */
  if (opacity != 1.0f || maskFrame || (clipPathFrame && !isTrivialClip)
      || aFrame->StyleDisplay()->mMixBlendMode != NS_STYLE_BLEND_NORMAL) {
    complexEffects = true;
    aContext.Save();
    nsRect clipRect =
      aFrame->GetVisualOverflowRectRelativeToSelf() + toUserSpace;
    aContext.Clip(NSRectToSnappedRect(clipRect,
                                  aFrame->PresContext()->AppUnitsPerDevPixel(),
                                  *drawTarget));
    aContext.PushGroup(gfxContentType::COLOR_ALPHA);
  }

  /* If this frame has only a trivial clipPath, set up cairo's clipping now so
   * we can just do normal painting and get it clipped appropriately.
   */
  if (clipPathFrame && isTrivialClip) {
    aContext.Save();
    clipPathFrame->ApplyClipOrPaintClipMask(aContext, aFrame, cssPxToDevPxMatrix);
  }

  /* Paint the child */
  if (effectProperties.HasValidFilter()) {
    RegularFramePaintCallback callback(aBuilder, aLayerManager,
                                       offsetToUserSpace);

    nsRegion dirtyRegion = aDirtyRect - offsetToBoundingBox;
    gfxMatrix tm = nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(aFrame);
    nsFilterInstance::PaintFilteredFrame(aFrame, aContext, tm, &callback, &dirtyRegion);
  } else {
    aContext.SetMatrix(matrixAutoSaveRestore.Matrix());
    aLayerManager->EndTransaction(FrameLayerBuilder::DrawPaintedLayer, aBuilder);
    aContext.SetMatrix(aContext.CurrentMatrix().Translate(devPixelOffsetToUserSpace));
  }

  if (clipPathFrame && isTrivialClip) {
    aContext.Restore();
  }

  /* No more effects, we're done. */
  if (!complexEffects) {
    return;
  }

  aContext.PopGroupToSource();

  Matrix maskTransform;
  RefPtr<SourceSurface> maskSurface =
    maskFrame ? maskFrame->GetMaskForMaskedFrame(&aContext,
                                                 aFrame, cssPxToDevPxMatrix,
                                                 opacity, &maskTransform)
              : nullptr;

  if (clipPathFrame && !isTrivialClip) {
    aContext.PushGroup(gfxContentType::COLOR_ALPHA);

    nsresult rv = clipPathFrame->ApplyClipOrPaintClipMask(aContext, aFrame, cssPxToDevPxMatrix);
    Matrix clippedMaskTransform;
    RefPtr<SourceSurface> clipMaskSurface = aContext.PopGroupToSurface(&clippedMaskTransform);

    if (NS_SUCCEEDED(rv) && clipMaskSurface) {
      // Still more set after clipping, so clip to another surface
      if (maskSurface || opacity != 1.0f) {
        aContext.PushGroup(gfxContentType::COLOR_ALPHA);
        aContext.Mask(clipMaskSurface, clippedMaskTransform);
        aContext.PopGroupToSource();
      } else {
        aContext.Mask(clipMaskSurface, clippedMaskTransform);
      }
    }
  }

  if (maskSurface) {
    aContext.Mask(maskSurface, maskTransform);
  } else if (opacity != 1.0f ||
             aFrame->StyleDisplay()->mMixBlendMode != NS_STYLE_BLEND_NORMAL) {
    aContext.Paint(opacity);
  }

  aContext.Restore();
}

gfxMatrix
nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(nsIFrame* aNonSVGFrame)
{
  int32_t appUnitsPerDevPixel = aNonSVGFrame->PresContext()->AppUnitsPerDevPixel();
  float devPxPerCSSPx =
    1 / nsPresContext::AppUnitsToFloatCSSPixels(appUnitsPerDevPixel);

  return gfxMatrix(devPxPerCSSPx, 0.0,
                   0.0, devPxPerCSSPx,
                   0.0, 0.0);
}

class PaintFrameCallback : public gfxDrawingCallback {
public:
  PaintFrameCallback(nsIFrame* aFrame,
                     const nsSize aPaintServerSize,
                     const IntSize aRenderSize,
                     uint32_t aFlags)
   : mFrame(aFrame)
   , mPaintServerSize(aPaintServerSize)
   , mRenderSize(aRenderSize)
   , mFlags (aFlags)
  {}
  virtual bool operator()(gfxContext* aContext,
                            const gfxRect& aFillRect,
                            const GraphicsFilter& aFilter,
                            const gfxMatrix& aTransform) override;
private:
  nsIFrame* mFrame;
  nsSize mPaintServerSize;
  IntSize mRenderSize;
  uint32_t mFlags;
};

bool
PaintFrameCallback::operator()(gfxContext* aContext,
                               const gfxRect& aFillRect,
                               const GraphicsFilter& aFilter,
                               const gfxMatrix& aTransform)
{
  if (mFrame->GetStateBits() & NS_FRAME_DRAWING_AS_PAINTSERVER)
    return false;

  mFrame->AddStateBits(NS_FRAME_DRAWING_AS_PAINTSERVER);

  aContext->Save();

  // Clip to aFillRect so that we don't paint outside.
  aContext->NewPath();
  aContext->Rectangle(aFillRect);
  aContext->Clip();

  gfxMatrix invmatrix = aTransform;
  if (!invmatrix.Invert()) {
    return false;
  }
  aContext->Multiply(invmatrix);

  // nsLayoutUtils::PaintFrame will anchor its painting at mFrame. But we want
  // to have it anchored at the top left corner of the bounding box of all of
  // mFrame's continuations. So we add a translation transform.
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  nsPoint offset = GetOffsetToBoundingBox(mFrame);
  gfxPoint devPxOffset = gfxPoint(offset.x, offset.y) / appUnitsPerDevPixel;
  aContext->Multiply(gfxMatrix::Translation(devPxOffset));

  gfxSize paintServerSize =
    gfxSize(mPaintServerSize.width, mPaintServerSize.height) /
      mFrame->PresContext()->AppUnitsPerDevPixel();

  // nsLayoutUtils::PaintFrame wants to render with paintServerSize, but we
  // want it to render with mRenderSize, so we need to set up a scale transform.
  gfxFloat scaleX = mRenderSize.width / paintServerSize.width;
  gfxFloat scaleY = mRenderSize.height / paintServerSize.height;
  aContext->Multiply(gfxMatrix::Scaling(scaleX, scaleY));

  // Draw.
  nsRect dirty(-offset.x, -offset.y,
               mPaintServerSize.width, mPaintServerSize.height);

  uint32_t flags = nsLayoutUtils::PAINT_IN_TRANSFORM;
  if (mFlags & nsSVGIntegrationUtils::FLAG_SYNC_DECODE_IMAGES) {
    flags |= nsLayoutUtils::PAINT_SYNC_DECODE_IMAGES;
  }
  nsRenderingContext context(aContext);
  nsLayoutUtils::PaintFrame(&context, mFrame,
                            dirty, NS_RGBA(0, 0, 0, 0),
                            flags);

  nsIFrame* currentFrame = mFrame;
   while ((currentFrame = currentFrame->GetNextContinuation()) != nullptr) {
    offset = currentFrame->GetOffsetToCrossDoc(mFrame);
    devPxOffset = gfxPoint(offset.x, offset.y) / appUnitsPerDevPixel;

    aContext->Save();
    aContext->Multiply(gfxMatrix::Scaling(1/scaleX, 1/scaleY));
    aContext->Multiply(gfxMatrix::Translation(devPxOffset));
    aContext->Multiply(gfxMatrix::Scaling(scaleX, scaleY));

    nsLayoutUtils::PaintFrame(&context, currentFrame,
                              dirty - offset, NS_RGBA(0, 0, 0, 0),
                              flags);

    aContext->Restore();
  }

  aContext->Restore();

  mFrame->RemoveStateBits(NS_FRAME_DRAWING_AS_PAINTSERVER);

  return true;
}

/* static */ already_AddRefed<gfxDrawable>
nsSVGIntegrationUtils::DrawableFromPaintServer(nsIFrame*         aFrame,
                                               nsIFrame*         aTarget,
                                               const nsSize&     aPaintServerSize,
                                               const IntSize& aRenderSize,
                                               const DrawTarget* aDrawTarget,
                                               const gfxMatrix&  aContextMatrix,
                                               uint32_t          aFlags)
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
    server->GetPaintServerPattern(aTarget, aDrawTarget,
                                  aContextMatrix, &nsStyleSVG::mFill, 1.0,
                                  &overrideBounds);

    if (!pattern)
      return nullptr;

    // pattern is now set up to fill aPaintServerSize. But we want it to
    // fill aRenderSize, so we need to add a scaling transform.
    // We couldn't just have set overrideBounds to aRenderSize - it would have
    // worked for gradients, but for patterns it would result in a different
    // pattern size.
    gfxFloat scaleX = overrideBounds.Width() / aRenderSize.width;
    gfxFloat scaleY = overrideBounds.Height() / aRenderSize.height;
    gfxMatrix scaleMatrix = gfxMatrix::Scaling(scaleX, scaleY);
    pattern->SetMatrix(scaleMatrix * pattern->GetMatrix());
    nsRefPtr<gfxDrawable> drawable =
      new gfxPatternDrawable(pattern, aRenderSize);
    return drawable.forget();
  }

  // We don't want to paint into a surface as long as we don't need to, so we
  // set up a drawing callback.
  nsRefPtr<gfxDrawingCallback> cb =
    new PaintFrameCallback(aFrame, aPaintServerSize, aRenderSize, aFlags);
  nsRefPtr<gfxDrawable> drawable = new gfxCallbackDrawable(cb, aRenderSize);
  return drawable.forget();
}
