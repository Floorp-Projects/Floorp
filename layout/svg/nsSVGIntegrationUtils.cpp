/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGIntegrationUtils.h"

// Keep others in (case-insensitive) order:
#include "gfxDrawable.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSClipPathInstance.h"
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
#include "nsCSSRendering.h"
#include "mozilla/Unused.h"

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::gfx;
using namespace mozilla::image;

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
    nsRect* r = aFrame->Properties().Get(nsIFrame::PreEffectsBBoxProperty());
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
  return aFrame->StyleEffects()->HasFilters() ||
         style->HasClipPath() ||
         style->mMask.HasLayerWithImage();
}

bool
nsSVGIntegrationUtils::UsingMaskOrClipPathForFrame(const nsIFrame* aFrame)
{
  const nsStyleSVGReset *style = aFrame->StyleSVGReset();
  return style->HasClipPath() ||
         style->mMask.HasLayerWithImage() ||
         (aFrame->StyleEffects()->mOpacity != 1.0f);
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

  virtual DrawResult Paint(gfxContext& aContext, nsIFrame *aTarget,
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
    return DrawResult::SUCCESS;
  }

private:
  nsDisplayListBuilder* mBuilder;
  LayerManager* mLayerManager;
  nsPoint mOffset;
};

static IntRect
ComputeClipExtsInDeviceSpace(gfxContext& aCtx)
{
  gfxContextMatrixAutoSaveRestore matRestore(&aCtx);

  // Get the clip extents in device space.
  aCtx.SetMatrix(gfxMatrix());
  gfxRect clippedFrameSurfaceRect = aCtx.GetClipExtents();
  clippedFrameSurfaceRect.RoundOut();

  IntRect result;
  ToRect(clippedFrameSurfaceRect).ToIntRect(&result);
  return mozilla::gfx::Factory::CheckSurfaceSize(result.Size()) ? result
                                                                : IntRect();
}

static IntRect
ComputeMaskGeometry(const nsSVGIntegrationUtils::PaintFramesParams& aParams,
                    const nsStyleSVGReset *svgReset,
                    const nsPoint& aOffsetToUserSpace,
                    const nsTArray<nsSVGMaskFrame *>& aMaskFrames)
{
  gfxContext& ctx = aParams.ctx;
  nsIFrame* frame = aParams.frame;

  // Convert boaderArea and dirtyRect to user space.
  int32_t appUnitsPerDevPixel = frame->PresContext()->AppUnitsPerDevPixel();
  nsRect userSpaceBorderArea = aParams.borderArea - aOffsetToUserSpace;
  nsRect userSpaceDirtyRect = aParams.dirtyRect - aOffsetToUserSpace;

  // Union all mask layer rectangles in user space.
  gfxRect maskInUserSpace;
  for (size_t i = 0; i < aMaskFrames.Length() ; i++) {
    nsSVGMaskFrame* maskFrame = aMaskFrames[i];
    gfxRect currentMaskSurfaceRect;

    if (maskFrame) {
      currentMaskSurfaceRect = maskFrame->GetMaskArea(aParams.frame);
    } else {
      nsCSSRendering::ImageLayerClipState clipState;
      nsCSSRendering::GetImageLayerClip(svgReset->mMask.mLayers[i],
                                       frame,
                                       *frame->StyleBorder(),
                                       userSpaceBorderArea,
                                       userSpaceDirtyRect,
                                       false, /* aWillPaintBorder */
                                       appUnitsPerDevPixel,
                                       &clipState);
      currentMaskSurfaceRect = clipState.mDirtyRectGfx;
    }

    maskInUserSpace = maskInUserSpace.Union(currentMaskSurfaceRect);
  }

  ctx.Save();

  // Clip ctx by both frame's visual overflow rect and mask union.
  gfxRect frameVisualOverflowRect =
    nsLayoutUtils::RectToGfxRect(frame->GetVisualOverflowRectRelativeToSelf(),
                                 appUnitsPerDevPixel);
  ctx.Clip(frameVisualOverflowRect);
  // maskInUserSpace might be empty if all mask references are not resolvable
  // or the size of them are empty. We still need to create a transparent mask
  // before bug 1276834 fixed, so don't clip ctx by an empty rectangle for for
  // now.
  if (!maskInUserSpace.IsEmpty()) {
    ctx.Clip(maskInUserSpace);
  }

  IntRect result = ComputeClipExtsInDeviceSpace(ctx);
  ctx.Restore();

  return result;
}

static DrawResult
GenerateMaskSurface(const nsSVGIntegrationUtils::PaintFramesParams& aParams,
                    float aOpacity, nsStyleContext* aSC,
                    const nsTArray<nsSVGMaskFrame *>& aMaskFrames,
                    const nsPoint& aOffsetToUserSpace,
                    Matrix& aOutMaskTransform,
                    RefPtr<SourceSurface>& aOutMaskSurface)
{
  const nsStyleSVGReset *svgReset = aSC->StyleSVGReset();
  MOZ_ASSERT(aMaskFrames.Length() > 0);

  gfxMatrix cssPxToDevPxMatrix =
    nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(aParams.frame);

  gfxContext& ctx = aParams.ctx;

  // There is only one SVG mask.
  if (((aMaskFrames.Length() == 1) && aMaskFrames[0])) {
    aOutMaskSurface =
      aMaskFrames[0]->GetMaskForMaskedFrame(&ctx, aParams.frame,
                                            cssPxToDevPxMatrix, aOpacity,
                                            &aOutMaskTransform,
                                            svgReset->mMask.mLayers[0].mMaskMode);
    return DrawResult::SUCCESS;
  }

  IntRect maskSurfaceRect = ComputeMaskGeometry(aParams, svgReset,
                                                aOffsetToUserSpace,
                                                aMaskFrames);
  if (maskSurfaceRect.IsEmpty()) {
    return DrawResult::SUCCESS;
  }

  // Mask composition result on CoreGraphic::A8 surface is not correct
  // when mask-mode is not add(source over). Switch to skia when CG backend
  // detected.
  RefPtr<DrawTarget> maskDT =
    (ctx.GetDrawTarget()->GetBackendType() == BackendType::COREGRAPHICS ||
     ctx.GetDrawTarget()->GetBackendType() == BackendType::DIRECT2D1_1)
    ? Factory::CreateDrawTarget(BackendType::SKIA, maskSurfaceRect.Size(),
                                SurfaceFormat::A8)
    : ctx.GetDrawTarget()->CreateSimilarDrawTarget(maskSurfaceRect.Size(),
                                                   SurfaceFormat::A8);
  if (!maskDT || !maskDT->IsValid()) {
    return DrawResult::TEMPORARY_ERROR;
  }

  RefPtr<gfxContext> maskContext = gfxContext::CreateOrNull(maskDT);
  MOZ_ASSERT(maskContext);

  nsPresContext* presContext = aParams.frame->PresContext();
  gfxPoint devPixelOffsetToUserSpace =
    nsLayoutUtils::PointToGfxPoint(aOffsetToUserSpace,
                                   presContext->AppUnitsPerDevPixel());

  // Set ctx's matrix on maskContext, offset by the maskSurfaceRect's position.
  // This makes sure that we combine the masks in device space.
  gfxMatrix maskSurfaceMatrix =
    ctx.CurrentMatrix() * gfxMatrix::Translation(-maskSurfaceRect.TopLeft());
  maskContext->SetMatrix(maskSurfaceMatrix);

  // Multiple SVG masks interleave with image mask. Paint each layer onto
  // maskDT one at a time.
  for (int i = aMaskFrames.Length() - 1; i >= 0 ; i--) {
    nsSVGMaskFrame *maskFrame = aMaskFrames[i];

    CompositionOp compositionOp = (i == int(aMaskFrames.Length() - 1))
      ? CompositionOp::OP_OVER
      : nsCSSRendering::GetGFXCompositeMode(svgReset->mMask.mLayers[i].mComposite);

    // maskFrame != nullptr means we get a SVG mask.
    // maskFrame == nullptr means we get an image mask.
    if (maskFrame) {
      Matrix svgMaskMatrix;
      RefPtr<SourceSurface> svgMask =
        maskFrame->GetMaskForMaskedFrame(maskContext, aParams.frame,
                                         cssPxToDevPxMatrix, aOpacity,
                                         &svgMaskMatrix,
                                         svgReset->mMask.mLayers[i].mMaskMode);
      if (svgMask) {
        gfxContextMatrixAutoSaveRestore matRestore(maskContext);

        maskContext->Multiply(ThebesMatrix(svgMaskMatrix));
        Rect drawRect = IntRectToRect(IntRect(IntPoint(0, 0), svgMask->GetSize()));
        maskDT->DrawSurface(svgMask, drawRect, drawRect, DrawSurfaceOptions(),
                            DrawOptions(1.0f, compositionOp));
      }
    } else {
      gfxContextMatrixAutoSaveRestore matRestore(maskContext);

      maskContext->Multiply(gfxMatrix::Translation(-devPixelOffsetToUserSpace));
      nsRenderingContext rc(maskContext);
      nsCSSRendering::PaintBGParams  params =
        nsCSSRendering::PaintBGParams::ForSingleLayer(*presContext,
                                                      rc, aParams.dirtyRect,
                                                      aParams.borderArea,
                                                      aParams.frame,
                                                      aParams.builder->GetBackgroundPaintFlags() |
                                                      nsCSSRendering::PAINTBG_MASK_IMAGE,
                                                      i, compositionOp);

      DrawResult result =
        nsCSSRendering::PaintBackgroundWithSC(params, aSC,
                                              *aParams.frame->StyleBorder());
      if (result != DrawResult::SUCCESS) {
        return result;
      }
    }
  }

  aOutMaskTransform = ToMatrix(maskSurfaceMatrix);
  if (!aOutMaskTransform.Invert()) {
    return DrawResult::SUCCESS;
  }

  aOutMaskSurface = maskDT->Snapshot();
  return DrawResult::SUCCESS;
}

static float
ComputeOpacity(const nsSVGIntegrationUtils::PaintFramesParams& aParams)
{
  nsIFrame* frame = aParams.frame;

  MOZ_ASSERT(!nsSVGUtils::CanOptimizeOpacity(frame) ||
             !aParams.callerPaintsOpacity,
             "How can we be optimizing the opacity into the svg as well as having the caller paint it?");

  float opacity = frame->StyleEffects()->mOpacity;

  if (opacity != 1.0f &&
      (nsSVGUtils::CanOptimizeOpacity(frame) || aParams.callerPaintsOpacity)) {
    return 1.0f;
  }

  return opacity;
}

static bool
ValidateSVGFrame(const nsSVGIntegrationUtils::PaintFramesParams& aParams,
                 bool aHasSVGLayout, DrawResult* aResult)
{
#ifdef DEBUG
  NS_ASSERTION(!(aParams.frame->GetStateBits() & NS_FRAME_SVG_LAYOUT) ||
               (NS_SVGDisplayListPaintingEnabled() &&
                !(aParams.frame->GetStateBits() & NS_FRAME_IS_NONDISPLAY)),
               "Should not use nsSVGIntegrationUtils on this SVG frame");
#endif

  nsIFrame* frame = aParams.frame;
  const nsIContent* content = frame->GetContent();
  if (aHasSVGLayout) {
    nsISVGChildFrame *svgChildFrame = do_QueryFrame(frame);
    if (!svgChildFrame || !frame->GetContent()->IsSVGElement()) {
      NS_ASSERTION(false, "why?");
      *aResult = DrawResult::BAD_ARGS;
      return false;
    }
    if (!static_cast<const nsSVGElement*>(content)->HasValidDimensions()) {
      // The SVG spec says not to draw _anything_
      *aResult = DrawResult::SUCCESS;
      return false;
    }
  }

  return true;
}

static void
SetupContextMatrix(const nsSVGIntegrationUtils::PaintFramesParams& aParams,
                   nsPoint& aOffsetToBoundingBox,
                   nsPoint& aToUserSpace,
                   nsPoint& aOffsetToUserSpace)
{
  nsIFrame* frame = aParams.frame;
  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(frame);

  nsPoint firstFrameOffset = GetOffsetToBoundingBox(firstFrame);
  aOffsetToBoundingBox = aParams.builder->ToReferenceFrame(firstFrame) - firstFrameOffset;
  if (!firstFrame->IsFrameOfType(nsIFrame::eSVG)) {
    /* Snap the offset if the reference frame is not a SVG frame,
     * since other frames will be snapped to pixel when rendering. */
    aOffsetToBoundingBox = nsPoint(
      frame->PresContext()->RoundAppUnitsToNearestDevPixels(aOffsetToBoundingBox.x),
      frame->PresContext()->RoundAppUnitsToNearestDevPixels(aOffsetToBoundingBox.y));
  }

  // After applying only "aOffsetToBoundingBox", aCtx would have its origin at
  // the top left corner of frame's bounding box (over all continuations).
  // However, SVG painting needs the origin to be located at the origin of the
  // SVG frame's "user space", i.e. the space in which, for example, the
  // frame's BBox lives.
  // SVG geometry frames and foreignObject frames apply their own offsets, so
  // their position is relative to their user space. So for these frame types,
  // if we want aCtx to be in user space, we first need to subtract the
  // frame's position so that SVG painting can later add it again and the
  // frame is painted in the right place.

  gfxPoint toUserSpaceGfx = nsSVGUtils::FrameSpaceInCSSPxToUserSpaceOffset(frame);
  aToUserSpace =
    nsPoint(nsPresContext::CSSPixelsToAppUnits(float(toUserSpaceGfx.x)),
            nsPresContext::CSSPixelsToAppUnits(float(toUserSpaceGfx.y)));

  aOffsetToUserSpace = aOffsetToBoundingBox - aToUserSpace;

#ifdef DEBUG
  bool hasSVGLayout = (frame->GetStateBits() & NS_FRAME_SVG_LAYOUT);
  NS_ASSERTION(hasSVGLayout || aOffsetToBoundingBox == aOffsetToUserSpace,
               "For non-SVG frames there shouldn't be any additional offset");
#endif

  gfxPoint devPixelOffsetToUserSpace =
    nsLayoutUtils::PointToGfxPoint(aOffsetToUserSpace,
                                   frame->PresContext()->AppUnitsPerDevPixel());
  aParams.ctx.SetMatrix(aParams.ctx.CurrentMatrix().Translate(devPixelOffsetToUserSpace));
}

static already_AddRefed<gfxContext>
CreateBlendTarget(const nsSVGIntegrationUtils::PaintFramesParams& aParams,
                  IntPoint& aTargetOffset)
{
  MOZ_ASSERT(aParams.frame->StyleEffects()->mMixBlendMode !=
             NS_STYLE_BLEND_NORMAL);

  // Create a temporary context to draw to so we can blend it back with
  // another operator.
  IntRect drawRect = ComputeClipExtsInDeviceSpace(aParams.ctx);

  RefPtr<DrawTarget> targetDT = aParams.ctx.GetDrawTarget()->CreateSimilarDrawTarget(drawRect.Size(), SurfaceFormat::B8G8R8A8);
  if (!targetDT || !targetDT->IsValid()) {
    return nullptr;
  }

  RefPtr<gfxContext> target = gfxContext::CreateOrNull(targetDT);
  MOZ_ASSERT(target); // already checked the draw target above
  target->SetMatrix(aParams.ctx.CurrentMatrix() *
                    gfxMatrix::Translation(-drawRect.TopLeft()));
  aTargetOffset = drawRect.TopLeft();

  return target.forget();
}

static void
BlendToTarget(const nsSVGIntegrationUtils::PaintFramesParams& aParams,
              gfxContext* aTarget, const IntPoint& aTargetOffset)
{
  MOZ_ASSERT(aParams.frame->StyleEffects()->mMixBlendMode !=
             NS_STYLE_BLEND_NORMAL);

  RefPtr<DrawTarget> targetDT = aTarget->GetDrawTarget();
  RefPtr<SourceSurface> targetSurf = targetDT->Snapshot();

  gfxContext& context = aParams.ctx;
  gfxContextAutoSaveRestore save(&context);
  context.SetMatrix(gfxMatrix()); // This will be restored right after.
  RefPtr<gfxPattern> pattern = new gfxPattern(targetSurf, Matrix::Translation(aTargetOffset.x, aTargetOffset.y));
  context.SetPattern(pattern);
  context.Paint();
}

DrawResult
nsSVGIntegrationUtils::PaintMaskAndClipPath(const PaintFramesParams& aParams)
{
  /* SVG defines the following rendering model:
   *
   *  1. Render geometry
   *  2. Apply filter
   *  3. Apply clipping, masking, group opacity
   *
   * We handle #3 here and perform a couple of optimizations:
   *
   * + Use cairo's clipPath when representable natively (single object
   *   clip region).
   *
   * + Merge opacity and masking if both used together.
   */
  nsIFrame* frame = aParams.frame;
  DrawResult result = DrawResult::SUCCESS;
  bool hasSVGLayout = (frame->GetStateBits() & NS_FRAME_SVG_LAYOUT);
  if (!ValidateSVGFrame(aParams, hasSVGLayout, &result)) {
    return result;
  }

  float opacity = ComputeOpacity(aParams);
  if (opacity == 0.0f) {
    return DrawResult::SUCCESS;
  }

  gfxContext& context = aParams.ctx;
  gfxContextMatrixAutoSaveRestore matrixAutoSaveRestore(&context);
  nsPoint offsetToBoundingBox;
  nsPoint toUserSpace;
  nsPoint offsetToUserSpace;
  SetupContextMatrix(aParams, offsetToBoundingBox, toUserSpace,
                     offsetToUserSpace);

  /* Properties are added lazily and may have been removed by a restyle,
     so make sure all applicable ones are set again. */
  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(frame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);

  bool isOK = effectProperties.HasNoFilterOrHasValidFilter();
  nsSVGClipPathFrame *clipPathFrame = effectProperties.GetClipPathFrame(&isOK);

  bool isTrivialClip = clipPathFrame ? clipPathFrame->IsTrivial() : true;

  gfxMatrix cssPxToDevPxMatrix = GetCSSPxToDevPxMatrix(frame);
  const nsStyleSVGReset *svgReset = firstFrame->StyleSVGReset();
  nsTArray<nsSVGMaskFrame *> maskFrames = effectProperties.GetMaskFrames();

#ifdef MOZ_ENABLE_MASK_AS_SHORTHAND
  // For a HTML doc:
  //   According to css-masking spec, always create a mask surface when we
  //   have any item in maskFrame even if all of those items are
  //   non-resolvable <mask-sources> or <images>, we still need to create a
  //   transparent black mask layer under this condition.
  // For a SVG doc:
  //   SVG 1.1 say that  if we fail to resolve a mask, we should draw the
  //   object unmasked.
  bool shouldGenerateMaskLayer = hasSVGLayout
                                 ? maskFrames.Length() == 1 && maskFrames[0]
                                 : maskFrames.Length() > 0;
#else
  // Since we do not support image mask so far, we should treat any
  // unresolvable mask as no mask. Otherwise, any object with a valid image
  // mask, e.g. url("xxx.png"), will become invisible just because we can not
  // handle image mask correctly. (See bug 1294171)
  bool shouldGenerateMaskLayer = maskFrames.Length() == 1 && maskFrames[0];
#endif

  bool shouldGenerateClipMaskLayer = clipPathFrame && !isTrivialClip;
  bool shouldApplyClipPath = clipPathFrame && isTrivialClip;
  bool shouldApplyBasicShape = !clipPathFrame && svgReset->HasClipPath();
  MOZ_ASSERT_IF(shouldGenerateClipMaskLayer,
                !shouldApplyClipPath && !shouldApplyBasicShape);

  // These are used if we require a temporary surface for a custom blend mode.
  IntPoint targetOffset;
  RefPtr<gfxContext> target =
    (aParams.frame->StyleEffects()->mMixBlendMode == NS_STYLE_BLEND_NORMAL)
      ? RefPtr<gfxContext>(&aParams.ctx).forget()
      : CreateBlendTarget(aParams, targetOffset);
  if (!target) {
    return DrawResult::TEMPORARY_ERROR;
  }

  bool shouldGenerateMask = (opacity != 1.0f || shouldGenerateClipMaskLayer ||
                             shouldGenerateMaskLayer);

  /* Check if we need to do additional operations on this child's
   * rendering, which necessitates rendering into another surface. */
  if (shouldGenerateMask) {
    context.Save();
    nsRect clipRect =
      frame->GetVisualOverflowRectRelativeToSelf() + toUserSpace;
    context.Clip(NSRectToSnappedRect(clipRect,
                                  frame->PresContext()->AppUnitsPerDevPixel(),
                                  *context.GetDrawTarget()));
    Matrix maskTransform;
    RefPtr<SourceSurface> maskSurface;
    if (shouldGenerateMaskLayer) {
      result = GenerateMaskSurface(aParams, opacity,
                                  firstFrame->StyleContext(),
                                  maskFrames, offsetToUserSpace,
                                  maskTransform, maskSurface);
    }

    if (shouldGenerateMaskLayer && !maskSurface) {
      // Entire surface is clipped out.
      context.Restore();
      return result;
    }

    if (shouldGenerateClipMaskLayer) {
      Matrix clippedMaskTransform;
      RefPtr<SourceSurface> clipMaskSurface =
        clipPathFrame->GetClipMask(context, frame, cssPxToDevPxMatrix,
                                   &clippedMaskTransform, maskSurface,
                                   maskTransform, &result);

      if (clipMaskSurface) {
        maskSurface = clipMaskSurface;
        maskTransform = clippedMaskTransform;
      }
    }

    target->PushGroupForBlendBack(gfxContentType::COLOR_ALPHA, opacity, maskSurface, maskTransform);
  }

  /* If this frame has only a trivial clipPath, set up cairo's clipping now so
   * we can just do normal painting and get it clipped appropriately.
   */
  if (shouldApplyClipPath) {
    context.Save();
    clipPathFrame->ApplyClipPath(context, frame, cssPxToDevPxMatrix);
  } else if (shouldApplyBasicShape) {
    context.Save();
    nsCSSClipPathInstance::ApplyBasicShapeClip(context, frame);
  }

  /* Paint the child */
  target->SetMatrix(matrixAutoSaveRestore.Matrix());
  BasicLayerManager* basic = static_cast<BasicLayerManager*>(aParams.layerManager);
  RefPtr<gfxContext> oldCtx = basic->GetTarget();
  basic->SetTarget(target);
  aParams.layerManager->EndTransaction(FrameLayerBuilder::DrawPaintedLayer,
                                        aParams.builder);
  basic->SetTarget(oldCtx);

  if (shouldApplyClipPath || shouldApplyBasicShape) {
    context.Restore();
  }

  if (shouldGenerateMask) {
    target->PopGroupAndBlend();
    context.Restore();
  }

  if (aParams.frame->StyleEffects()->mMixBlendMode != NS_STYLE_BLEND_NORMAL) {
    MOZ_ASSERT(target != &aParams.ctx);
    BlendToTarget(aParams, target, targetOffset);
  }

  return result;
}

DrawResult
nsSVGIntegrationUtils::PaintFilter(const PaintFramesParams& aParams)
{
  MOZ_ASSERT(!aParams.builder->IsForGenerateGlyphMask());

  nsIFrame* frame = aParams.frame;
  DrawResult result = DrawResult::SUCCESS;
  bool hasSVGLayout = (frame->GetStateBits() & NS_FRAME_SVG_LAYOUT);
  if (!ValidateSVGFrame(aParams, hasSVGLayout, &result)) {
    return result;
  }

  float opacity = ComputeOpacity(aParams);
  if (opacity == 0.0f) {
    return DrawResult::SUCCESS;
  }

  gfxContext& context = aParams.ctx;
  gfxContextMatrixAutoSaveRestore matrixAutoSaveRestore(&context);
  nsPoint offsetToBoundingBox;
  nsPoint toUserSpace;
  nsPoint offsetToUserSpace;
  SetupContextMatrix(aParams, offsetToBoundingBox, toUserSpace,
                     offsetToUserSpace);

  /* Properties are added lazily and may have been removed by a restyle,
     so make sure all applicable ones are set again. */
  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(frame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);

  if (!effectProperties.HasValidFilter()) {
    return DrawResult::NOT_READY;
  }

  // These are used if we require a temporary surface for a custom blend mode.
  IntPoint targetOffset;
  RefPtr<gfxContext> target =
    (aParams.frame->StyleEffects()->mMixBlendMode == NS_STYLE_BLEND_NORMAL)
    ? RefPtr<gfxContext>(&aParams.ctx).forget()
    : CreateBlendTarget(aParams, targetOffset);
  if (!target) {
    return DrawResult::TEMPORARY_ERROR;
  }

  /* Paint the child and apply filters */
  RegularFramePaintCallback callback(aParams.builder, aParams.layerManager,
                                     offsetToUserSpace);
  nsRegion dirtyRegion = aParams.dirtyRect - offsetToBoundingBox;
  gfxMatrix tm = nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(frame);
  nsFilterInstance::PaintFilteredFrame(frame, target->GetDrawTarget(),
                                       tm, &callback, &dirtyRegion);

  if (aParams.frame->StyleEffects()->mMixBlendMode != NS_STYLE_BLEND_NORMAL) {
    MOZ_ASSERT(target != &aParams.ctx);
    BlendToTarget(aParams, target, targetOffset);
  }

  return result;
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
                          const SamplingFilter aSamplingFilter,
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
                               const SamplingFilter aSamplingFilter,
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

  using PaintFrameFlags = nsLayoutUtils::PaintFrameFlags;
  PaintFrameFlags flags = PaintFrameFlags::PAINT_IN_TRANSFORM;
  if (mFlags & nsSVGIntegrationUtils::FLAG_SYNC_DECODE_IMAGES) {
    flags |= PaintFrameFlags::PAINT_SYNC_DECODE_IMAGES;
  }
  nsRenderingContext context(aContext);
  nsLayoutUtils::PaintFrame(&context, mFrame,
                            dirty, NS_RGBA(0, 0, 0, 0),
                            nsDisplayListBuilderMode::PAINTING,
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
                              nsDisplayListBuilderMode::PAINTING,
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
    RefPtr<gfxPattern> pattern =
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
    RefPtr<gfxDrawable> drawable =
      new gfxPatternDrawable(pattern, aRenderSize);
    return drawable.forget();
  }

  // We don't want to paint into a surface as long as we don't need to, so we
  // set up a drawing callback.
  RefPtr<gfxDrawingCallback> cb =
    new PaintFrameCallback(aFrame, aPaintServerSize, aRenderSize, aFlags);
  RefPtr<gfxDrawable> drawable = new gfxCallbackDrawable(cb, aRenderSize);
  return drawable.forget();
}
