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
    // With image masks, there is one more exception.
    //
    // In nsStyleImageLayers::Layer::CalcDifference, we do not add
    // nsChangeHint_UpdateOverflow hint when image mask(not SVG mask) property
    // value changed, since replace image mask does not cause layout change.
    // So even if we apply a new mask image to this frame,
    // PreEffectsBBoxProperty might still left empty.
    NS_ASSERTION(nsSVGEffects::GetEffectProperties(aFrame).MightHaveNoneSVGMask() ||
                 aFrame->GetParent()->StyleContext()->GetPseudo() ==
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
         style->mMask.HasLayerWithImage();
}

nsPoint
nsSVGIntegrationUtils::GetOffsetToBoundingBox(nsIFrame* aFrame)
{
  if ((aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT)) {
    // Do NOT call GetAllInFlowRectsUnion for SVG - it will get the
    // covered region relative to the nsSVGOuterSVGFrame, which is absolutely
    // not what we want. SVG frames are always in user space, so they have
    // no offset adjustment to make.
    return nsPoint();
  }

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
    BasicLayerManager* basic = mLayerManager->AsBasicLayerManager();
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

/**
 * Returns true if any of the masks is an image mask (and not an SVG mask).
 */
static bool
HasNonSVGMask(const nsTArray<nsSVGMaskFrame*>& aMaskFrames)
{
  for (size_t i = 0; i < aMaskFrames.Length() ; i++) {
    nsSVGMaskFrame *maskFrame = aMaskFrames[i];
    if (!maskFrame) {
      return true;
    }
  }

  return false;
}

typedef nsSVGIntegrationUtils::PaintFramesParams PaintFramesParams;

/**
 * Paint css-positioned-mask onto a given target(aMaskDT).
 */
static DrawResult
PaintMaskSurface(const PaintFramesParams& aParams,
                 DrawTarget* aMaskDT, float aOpacity, nsStyleContext* aSC,
                 const nsTArray<nsSVGMaskFrame*>& aMaskFrames,
                 const gfxMatrix& aMaskSurfaceMatrix,
                 const nsPoint& aOffsetToUserSpace)
{
  MOZ_ASSERT(aMaskFrames.Length() > 0);
  MOZ_ASSERT(aMaskDT->GetFormat() == SurfaceFormat::A8);

  const nsStyleSVGReset *svgReset = aSC->StyleSVGReset();
  gfxMatrix cssPxToDevPxMatrix =
    nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(aParams.frame);

  nsPresContext* presContext = aParams.frame->PresContext();
  gfxPoint devPixelOffsetToUserSpace =
    nsLayoutUtils::PointToGfxPoint(aOffsetToUserSpace,
                                   presContext->AppUnitsPerDevPixel());

  RefPtr<gfxContext> maskContext = gfxContext::CreateOrNull(aMaskDT);
  MOZ_ASSERT(maskContext);
  maskContext->SetMatrix(aMaskSurfaceMatrix);

  // Multiple SVG masks interleave with image mask. Paint each layer onto
  // aMaskDT one at a time.
  for (int i = aMaskFrames.Length() - 1; i >= 0 ; i--) {
    nsSVGMaskFrame *maskFrame = aMaskFrames[i];
    DrawResult result = DrawResult::SUCCESS;
    CompositionOp compositionOp = (i == int(aMaskFrames.Length() - 1))
      ? CompositionOp::OP_OVER
      : nsCSSRendering::GetGFXCompositeMode(svgReset->mMask.mLayers[i].mComposite);

    // maskFrame != nullptr means we get a SVG mask.
    // maskFrame == nullptr means we get an image mask.
    if (maskFrame) {
      Matrix svgMaskMatrix;
      nsSVGMaskFrame::MaskParams params(maskContext, aParams.frame,
                                                  cssPxToDevPxMatrix,
                                                  aOpacity, &svgMaskMatrix,
                                                  svgReset->mMask.mLayers[i].mMaskMode);
      RefPtr<SourceSurface> svgMask;
      Tie(result, svgMask) = maskFrame->GetMaskForMaskedFrame(params);

      if (svgMask) {
        MOZ_ASSERT(result == DrawResult::SUCCESS);
        gfxContextMatrixAutoSaveRestore matRestore(maskContext);

        maskContext->Multiply(ThebesMatrix(svgMaskMatrix));
        aMaskDT->MaskSurface(ColorPattern(Color(0.0, 0.0, 0.0, 1.0)), svgMask,
                             Point(0, 0),
                             DrawOptions(1.0, compositionOp));
      }

      if (result != DrawResult::SUCCESS) {
        return result;
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

      result =
        nsCSSRendering::PaintBackgroundWithSC(params, aSC,
                                              *aParams.frame->StyleBorder());
      if (result != DrawResult::SUCCESS) {
        return result;
      }
    }
  }

  return DrawResult::SUCCESS;
}

struct MaskPaintResult {
  RefPtr<SourceSurface> maskSurface;
  Matrix maskTransform;
  DrawResult result;
  bool transparentBlackMask;
  bool opacityApplied;

  MaskPaintResult()
    : result(DrawResult::SUCCESS), transparentBlackMask(false),
      opacityApplied(false)
  {}
};

static MaskPaintResult
CreateAndPaintMaskSurface(const PaintFramesParams& aParams,
                          float aOpacity, nsStyleContext* aSC,
                          const nsTArray<nsSVGMaskFrame*>& aMaskFrames,
                          const nsPoint& aOffsetToUserSpace)
{
  const nsStyleSVGReset *svgReset = aSC->StyleSVGReset();
  MOZ_ASSERT(aMaskFrames.Length() > 0);
  MaskPaintResult paintResult;

  gfxContext& ctx = aParams.ctx;

  // Optimization for single SVG mask.
  if (((aMaskFrames.Length() == 1) && aMaskFrames[0])) {
    gfxMatrix cssPxToDevPxMatrix =
    nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(aParams.frame);
    paintResult.opacityApplied = true;
    nsSVGMaskFrame::MaskParams params(&ctx, aParams.frame, cssPxToDevPxMatrix,
                                      aOpacity, &paintResult.maskTransform,
                                      svgReset->mMask.mLayers[0].mMaskMode);
    Tie(paintResult.result, paintResult.maskSurface) =
      aMaskFrames[0]->GetMaskForMaskedFrame(params);

    if (!paintResult.maskSurface) {
      paintResult.transparentBlackMask = true;
    }

    return paintResult;
  }

  const IntRect& maskSurfaceRect = aParams.maskRect;
  if (maskSurfaceRect.IsEmpty()) {
    paintResult.transparentBlackMask = true;
    return paintResult;
  }

  RefPtr<DrawTarget> maskDT =
      ctx.GetDrawTarget()->CreateSimilarDrawTarget(maskSurfaceRect.Size(),
                                                   SurfaceFormat::A8);
  if (!maskDT || !maskDT->IsValid()) {
    paintResult.result = DrawResult::TEMPORARY_ERROR;
    return paintResult;
  }

  // Set aAppliedOpacity as true only if all mask layers are svg mask.
  // In this case, we will apply opacity into the final mask surface, so the
  // caller does not need to apply it again.
  paintResult.opacityApplied = !HasNonSVGMask(aMaskFrames);

  // Set context's matrix on maskContext, offset by the maskSurfaceRect's
  // position. This makes sure that we combine the masks in device space.
  gfxMatrix maskSurfaceMatrix =
    ctx.CurrentMatrix() * gfxMatrix::Translation(-aParams.maskRect.TopLeft());

  paintResult.result = PaintMaskSurface(aParams, maskDT,
                                        paintResult.opacityApplied
                                          ? aOpacity : 1.0,
                                        aSC, aMaskFrames, maskSurfaceMatrix,
                                        aOffsetToUserSpace);
  if (paintResult.result != DrawResult::SUCCESS) {
    // Now we know the status of mask resource since we used it while painting.
    // According to the return value of PaintMaskSurface, we know whether mask
    // resource is resolvable or not.
    //
    // For a HTML doc:
    //   According to css-masking spec, always create a mask surface when
    //   we have any item in maskFrame even if all of those items are
    //   non-resolvable <mask-sources> or <images>.
    //   Set paintResult.transparentBlackMask as true,  the caller should stop
    //   painting masked content as if this mask is a transparent black one.
    // For a SVG doc:
    //   SVG 1.1 say that if we fail to resolve a mask, we should draw the
    //   object unmasked.
    //   Left patinResult.maskSurface empty, the caller should paint all
    //   masked content as if this mask is an opaque white one(no mask).
    paintResult.transparentBlackMask =
      !(aParams.frame->GetStateBits() & NS_FRAME_SVG_LAYOUT);

    MOZ_ASSERT(!paintResult.maskSurface);
    return paintResult;
  }

  paintResult.maskTransform = ToMatrix(maskSurfaceMatrix);
  if (!paintResult.maskTransform.Invert()) {
    return paintResult;
  }

  paintResult.maskSurface = maskDT->Snapshot();
  return paintResult;
}

static bool
ValidateSVGFrame(nsIFrame* aFrame)
{
#ifdef DEBUG
  NS_ASSERTION(!(aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT) ||
               (NS_SVGDisplayListPaintingEnabled() &&
                !(aFrame->GetStateBits() & NS_FRAME_IS_NONDISPLAY)),
               "Should not use nsSVGIntegrationUtils on this SVG frame");
#endif

  bool hasSVGLayout = (aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT);
  if (hasSVGLayout) {
#ifdef DEBUG
    nsISVGChildFrame *svgChildFrame = do_QueryFrame(aFrame);
    MOZ_ASSERT(svgChildFrame && aFrame->GetContent()->IsSVGElement(),
               "A non-SVG frame carries NS_FRAME_SVG_LAYOUT flag?");
#endif

    const nsIContent* content = aFrame->GetContent();
    if (!static_cast<const nsSVGElement*>(content)->HasValidDimensions()) {
      // The SVG spec says not to draw _anything_
      return false;
    }
  }

  return true;
}

/**
 * Setup transform matrix of a gfx context by a specific frame. Depend on
 * aClipCtx, this function may clip that context by the visual overflow area
 * of aFrame.
 *
 * @param aFrame is the target frame.
 * @param aOffsetToBoundingBox returns the offset between the reference frame
 *        and the bounding box of aFrame.
 * @oaram aOffsetToUserSpace returns the offset between the reference frame and
 *        the user space coordinate of aFrame.
 * @param aClipCtx indicate whether clip aParams.ctx by visual overflow rect of
 *        aFrame or not.
 */
static void
SetupContextMatrix(nsIFrame* aFrame, const PaintFramesParams& aParams,
                   nsPoint& aOffsetToBoundingBox, nsPoint& aOffsetToUserSpace,
                   bool aClipCtx)
{
  aOffsetToBoundingBox = aParams.builder->ToReferenceFrame(aFrame) -
                         nsSVGIntegrationUtils::GetOffsetToBoundingBox(aFrame);
  if (!aFrame->IsFrameOfType(nsIFrame::eSVG)) {
    /* Snap the offset if the reference frame is not a SVG frame,
     * since other frames will be snapped to pixel when rendering. */
    aOffsetToBoundingBox = nsPoint(
      aFrame->PresContext()->RoundAppUnitsToNearestDevPixels(aOffsetToBoundingBox.x),
      aFrame->PresContext()->RoundAppUnitsToNearestDevPixels(aOffsetToBoundingBox.y));
  }

  // After applying only "aOffsetToBoundingBox", aParams.ctx would have its
  // origin at the top left corner of frame's bounding box (over all
  // continuations).
  // However, SVG painting needs the origin to be located at the origin of the
  // SVG frame's "user space", i.e. the space in which, for example, the
  // frame's BBox lives.
  // SVG geometry frames and foreignObject frames apply their own offsets, so
  // their position is relative to their user space. So for these frame types,
  // if we want aCtx to be in user space, we first need to subtract the
  // frame's position so that SVG painting can later add it again and the
  // frame is painted in the right place.

  gfxPoint toUserSpaceGfx = nsSVGUtils::FrameSpaceInCSSPxToUserSpaceOffset(aFrame);
  nsPoint toUserSpace =
    nsPoint(nsPresContext::CSSPixelsToAppUnits(float(toUserSpaceGfx.x)),
            nsPresContext::CSSPixelsToAppUnits(float(toUserSpaceGfx.y)));

  aOffsetToUserSpace = aOffsetToBoundingBox - toUserSpace;

#ifdef DEBUG
  bool hasSVGLayout = (aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT);
  NS_ASSERTION(hasSVGLayout || aOffsetToBoundingBox == aOffsetToUserSpace,
               "For non-SVG frames there shouldn't be any additional offset");
#endif

  gfxPoint devPixelOffsetToUserSpace =
    nsLayoutUtils::PointToGfxPoint(aOffsetToUserSpace,
                                   aFrame->PresContext()->AppUnitsPerDevPixel());
  gfxContext& context = aParams.ctx;
  context.SetMatrix(context.CurrentMatrix().Translate(devPixelOffsetToUserSpace));

  if (aClipCtx) {
    nsRect clipRect =
      aParams.frame->GetVisualOverflowRectRelativeToSelf() + toUserSpace;
    context.Clip(NSRectToSnappedRect(clipRect,
                                  aFrame->PresContext()->AppUnitsPerDevPixel(),
                                  *context.GetDrawTarget()));
  }
}

bool
nsSVGIntegrationUtils::IsMaskResourceReady(nsIFrame* aFrame)
{
  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(aFrame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);
  nsTArray<nsSVGMaskFrame*> maskFrames = effectProperties.GetMaskFrames();
  const nsStyleSVGReset* svgReset = firstFrame->StyleSVGReset();

  for (uint32_t i = 0; i < maskFrames.Length(); i++) {
    // Refers to a valid SVG mask.
    if (maskFrames[i]) {
      continue;
    }

    // Refers to an external resource, which is not ready yet.
    if (!svgReset->mMask.mLayers[i].mImage.IsComplete()) {
      return false;
    }
  }

  // Either all mask resources are ready, or no mask resource is needed.
  return true;
}

DrawResult
nsSVGIntegrationUtils::PaintMask(const PaintFramesParams& aParams)
{
  nsSVGUtils::MaskUsage maskUsage;
  nsSVGUtils::DetermineMaskUsage(aParams.frame, aParams.handleOpacity,
                                 maskUsage);

  nsIFrame* frame = aParams.frame;
  if (!ValidateSVGFrame(frame)) {
    return DrawResult::SUCCESS;
  }

  if (maskUsage.opacity == 0.0f) {
    return DrawResult::SUCCESS;
  }

  gfxContext& ctx = aParams.ctx;
  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(frame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);

  DrawResult result = DrawResult::SUCCESS;
  nsPoint offsetToBoundingBox;
  nsPoint offsetToUserSpace;
  gfxContextMatrixAutoSaveRestore matSR;
    RefPtr<DrawTarget> maskTarget = ctx.GetDrawTarget();

  if (maskUsage.shouldGenerateMaskLayer &&
      maskUsage.shouldGenerateClipMaskLayer) {
    // We will paint both mask of positioned mask and clip-path into
    // maskTarget.
    //
    // Create one extra draw target for drawing positioned mask, so that we do
    // not have to copy the content of maskTarget before painting
    // clip-path into it.
    maskTarget = maskTarget->CreateSimilarDrawTarget(maskTarget->GetSize(),
                                                     SurfaceFormat::A8);
  }

  if (maskUsage.shouldApplyBasicShape) {
    matSR.SetContext(&ctx);

    SetupContextMatrix(firstFrame, aParams, offsetToBoundingBox,
                       offsetToUserSpace, false);

    nsCSSClipPathInstance::ApplyBasicShapeClip(ctx, frame);
    if (!maskUsage.shouldGenerateMaskLayer) {
      // Only have basic-shape clip-path effect. Fill clipped region by
      // opaque white.
      ctx.SetColor(Color(0.0, 0.0, 0.0, 1.0));
      ctx.Fill();
      ctx.PopClip();

      return result;
    }
  }

  // Paint mask onto ctx.
  if (maskUsage.shouldGenerateMaskLayer) {
    matSR.Restore();
    matSR.SetContext(&ctx);

    SetupContextMatrix(frame, aParams, offsetToBoundingBox,
                       offsetToUserSpace, false);
    nsTArray<nsSVGMaskFrame *> maskFrames = effectProperties.GetMaskFrames();
    // XXX Bug 1323912.
    MOZ_ASSERT(maskUsage.opacity == 1.0,
               "nsSVGIntegrationUtils::PaintMask can not handle opacity now.");
    result = PaintMaskSurface(aParams, maskTarget, 1.0,
                              firstFrame->StyleContext(), maskFrames,
                              ctx.CurrentMatrix(), offsetToUserSpace);
    if (result != DrawResult::SUCCESS) {
      if (maskUsage.shouldApplyBasicShape) {
        ctx.PopClip();
      }

      return result;
    }
  }

  if (maskUsage.shouldApplyBasicShape) {
    ctx.PopClip();
    return result;
  }

  // Paint clip-path onto ctx.
  if (maskUsage.shouldGenerateClipMaskLayer || maskUsage.shouldApplyClipPath) {
    matSR.Restore();
    matSR.SetContext(&ctx);

    SetupContextMatrix(firstFrame, aParams, offsetToBoundingBox,
                       offsetToUserSpace, false);

    Matrix clipMaskTransform;
    gfxMatrix cssPxToDevPxMatrix = GetCSSPxToDevPxMatrix(frame);

    nsSVGClipPathFrame *clipPathFrame = effectProperties.GetClipPathFrame();
    RefPtr<SourceSurface> maskSurface =
      maskUsage.shouldGenerateMaskLayer ? maskTarget->Snapshot() : nullptr;
    result =
      clipPathFrame->PaintClipMask(ctx, frame, cssPxToDevPxMatrix,
                                   &clipMaskTransform, maskSurface,
                                   ToMatrix(ctx.CurrentMatrix()));
  }

  return result;
}

DrawResult
nsSVGIntegrationUtils::PaintMaskAndClipPath(const PaintFramesParams& aParams)
{
  MOZ_ASSERT(UsingMaskOrClipPathForFrame(aParams.frame),
             "Should not use this method when no mask or clipPath effect"
             "on this frame");

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
  if (!ValidateSVGFrame(frame)) {
    return result;
  }

  nsSVGUtils::MaskUsage maskUsage;
  nsSVGUtils::DetermineMaskUsage(aParams.frame, aParams.handleOpacity,
                                 maskUsage);

  if (maskUsage.opacity == 0.0f) {
    return DrawResult::SUCCESS;
  }

  gfxContext& context = aParams.ctx;
  gfxContextMatrixAutoSaveRestore matrixAutoSaveRestore(&context);

  /* Properties are added lazily and may have been removed by a restyle,
     so make sure all applicable ones are set again. */
  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(frame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);

  nsSVGClipPathFrame *clipPathFrame = effectProperties.GetClipPathFrame();

  gfxMatrix cssPxToDevPxMatrix = GetCSSPxToDevPxMatrix(frame);
  nsTArray<nsSVGMaskFrame*> maskFrames = effectProperties.GetMaskFrames();

  nsPoint offsetToBoundingBox;
  nsPoint offsetToUserSpace;

  bool shouldGenerateMask = (maskUsage.opacity != 1.0f ||
                             maskUsage.shouldGenerateClipMaskLayer ||
                             maskUsage.shouldGenerateMaskLayer);
  bool shouldPushMask = false;

  /* Check if we need to do additional operations on this child's
   * rendering, which necessitates rendering into another surface. */
  if (shouldGenerateMask) {
    gfxContextMatrixAutoSaveRestore matSR;

    Matrix maskTransform;
    RefPtr<SourceSurface> maskSurface;
    bool opacityApplied = false;

    if (maskUsage.shouldGenerateMaskLayer) {
      matSR.SetContext(&context);

      // For css-mask, we want to generate a mask for each continuation frame,
      // so we setup context matrix by the position of the current frame,
      // instead of the first continuation frame.
      SetupContextMatrix(frame, aParams, offsetToBoundingBox,
                         offsetToUserSpace, false);
      MaskPaintResult paintResult =
        CreateAndPaintMaskSurface(aParams, maskUsage.opacity,
                                  firstFrame->StyleContext(),
                                  maskFrames, offsetToUserSpace);

      if (paintResult.transparentBlackMask) {
        return paintResult.result;
      }

      result &= paintResult.result;
      maskSurface = paintResult.maskSurface;
      if (maskSurface) {
        MOZ_ASSERT(paintResult.result == DrawResult::SUCCESS);
        shouldPushMask = true;
        maskTransform = paintResult.maskTransform;
        opacityApplied = paintResult.opacityApplied;
      }
    }

    if (maskUsage.shouldGenerateClipMaskLayer) {
      matSR.Restore();
      matSR.SetContext(&context);

      SetupContextMatrix(firstFrame, aParams, offsetToBoundingBox,
                         offsetToUserSpace, false);
      Matrix clipMaskTransform;
      DrawResult clipMaskResult;
      RefPtr<SourceSurface> clipMaskSurface;
      Tie(clipMaskResult, clipMaskSurface) =
        clipPathFrame->GetClipMask(context, frame, cssPxToDevPxMatrix,
                                   &clipMaskTransform, maskSurface,
                                   maskTransform);

      if (clipMaskSurface) {
        maskSurface = clipMaskSurface;
        maskTransform = clipMaskTransform;
      } else {
        // Either entire surface is clipped out, or gfx buffer allocation
        // failure in nsSVGClipPathFrame::GetClipMask.
        return clipMaskResult;
      }
      result &= clipMaskResult;
      shouldPushMask = true;
    }

    // opacity != 1.0f.
    if (!maskUsage.shouldGenerateClipMaskLayer &&
        !maskUsage.shouldGenerateMaskLayer) {
      MOZ_ASSERT(maskUsage.opacity != 1.0f);

      matSR.SetContext(&context);
      SetupContextMatrix(firstFrame, aParams, offsetToBoundingBox,
                         offsetToUserSpace, false);
      shouldPushMask = true;
    }

    if (shouldPushMask) {
      if (aParams.layerManager->GetRoot()->GetContentFlags() &
          Layer::CONTENT_COMPONENT_ALPHA) {
        context.PushGroupAndCopyBackground(gfxContentType::COLOR_ALPHA,
                                           opacityApplied
                                             ? 1.0
                                             : maskUsage.opacity,
                                           maskSurface, maskTransform);
      } else {
        context.PushGroupForBlendBack(gfxContentType::COLOR_ALPHA,
                                      opacityApplied ? 1.0 : maskUsage.opacity,
                                      maskSurface, maskTransform);
      }
    }
  }

  /* If this frame has only a trivial clipPath, set up cairo's clipping now so
   * we can just do normal painting and get it clipped appropriately.
   */
  if (maskUsage.shouldApplyClipPath || maskUsage.shouldApplyBasicShape) {
    gfxContextMatrixAutoSaveRestore matSR(&context);

    SetupContextMatrix(firstFrame, aParams, offsetToBoundingBox,
                       offsetToUserSpace, false);

    MOZ_ASSERT(!maskUsage.shouldApplyClipPath ||
               !maskUsage.shouldApplyBasicShape);
    if (maskUsage.shouldApplyClipPath) {
      clipPathFrame->ApplyClipPath(context, frame, cssPxToDevPxMatrix);
    } else {
      nsCSSClipPathInstance::ApplyBasicShapeClip(context, frame);
    }
  }

  /* Paint the child */
  context.SetMatrix(matrixAutoSaveRestore.Matrix());
  BasicLayerManager* basic = aParams.layerManager->AsBasicLayerManager();
  RefPtr<gfxContext> oldCtx = basic->GetTarget();
  basic->SetTarget(&context);
  aParams.layerManager->EndTransaction(FrameLayerBuilder::DrawPaintedLayer,
                                       aParams.builder);
  basic->SetTarget(oldCtx);

  if (gfxPrefs::DrawMaskLayer()) {
    gfxContextAutoSaveRestore saver(&context);

    context.NewPath();
    gfxRect drawingRect =
      nsLayoutUtils::RectToGfxRect(aParams.borderArea,
                                   frame->PresContext()->AppUnitsPerDevPixel());
    context.Rectangle(drawingRect, true);
    context.SetColor(Color(0.0, 1.0, 0.0, 1.0));
    context.Fill();
  }

  if (maskUsage.shouldApplyClipPath || maskUsage.shouldApplyBasicShape) {
    context.PopClip();
  }

  if (shouldPushMask) {
    context.PopGroupAndBlend();
  }

  return result;
}

DrawResult
nsSVGIntegrationUtils::PaintFilter(const PaintFramesParams& aParams)
{
  MOZ_ASSERT(!aParams.builder->IsForGenerateGlyphMask(),
             "Filter effect is discarded while generating glyph mask.");
  MOZ_ASSERT(aParams.frame->StyleEffects()->HasFilters(),
             "Should not use this method when no filter effect on this frame");

  nsIFrame* frame = aParams.frame;
  if (!ValidateSVGFrame(frame)) {
    return DrawResult::SUCCESS;
  }

  float opacity = nsSVGUtils::ComputeOpacity(frame, aParams.handleOpacity);
  if (opacity == 0.0f) {
    return DrawResult::SUCCESS;
  }

  /* Properties are added lazily and may have been removed by a restyle,
     so make sure all applicable ones are set again. */
  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(frame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);

  if (effectProperties.HasInvalidFilter()) {
    return DrawResult::NOT_READY;
  }

  gfxContext& context = aParams.ctx;
  nsPoint offsetToBoundingBox;
  nsPoint offsetToUserSpace;

  gfxContextAutoSaveRestore autoSR(&context);
  SetupContextMatrix(firstFrame, aParams, offsetToBoundingBox,
                     offsetToUserSpace, true);

  if (opacity != 1.0f) {
    context.PushGroupForBlendBack(gfxContentType::COLOR_ALPHA, opacity,
                                  nullptr, Matrix());
  }

  /* Paint the child and apply filters */
  RegularFramePaintCallback callback(aParams.builder, aParams.layerManager,
                                     offsetToUserSpace);
  nsRegion dirtyRegion = aParams.dirtyRect - offsetToBoundingBox;
  gfxMatrix tm = nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(frame);
  nsFilterInstance::PaintFilteredFrame(frame, context.GetDrawTarget(),
                                       tm, &callback, &dirtyRegion);

  if (opacity != 1.0f) {
    context.PopGroupAndBlend();
  }

  return DrawResult::SUCCESS;
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
  nsPoint offset = nsSVGIntegrationUtils::GetOffsetToBoundingBox(mFrame);
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
