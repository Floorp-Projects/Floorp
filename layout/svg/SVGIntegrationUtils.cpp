/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "SVGIntegrationUtils.h"

// Keep others in (case-insensitive) order:
#include "gfxDrawable.h"

#include "Layers.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSRendering.h"
#include "nsDisplayList.h"
#include "nsLayoutUtils.h"
#include "gfxContext.h"
#include "SVGPaintServerFrame.h"
#include "BasicLayers.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/CSSClipPathInstance.h"
#include "mozilla/FilterInstance.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/SVGClipPathFrame.h"
#include "mozilla/SVGObserverUtils.h"
#include "mozilla/SVGMaskFrame.h"
#include "mozilla/SVGUtils.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/SVGElement.h"

using namespace mozilla::dom;
using namespace mozilla::layers;
using namespace mozilla::gfx;
using namespace mozilla::image;

namespace mozilla {

/**
 * This class is used to get the pre-effects ink overflow rect of a frame,
 * or, in the case of a frame with continuations, to collect the union of the
 * pre-effects ink overflow rects of all the continuations. The result is
 * relative to the origin (top left corner of the border box) of the frame, or,
 * if the frame has continuations, the origin of the  _first_ continuation.
 */
class PreEffectsInkOverflowCollector : public nsLayoutUtils::BoxCallback {
 public:
  /**
   * If the pre-effects ink overflow rect of the frame being examined
   * happens to be known, it can be passed in as aCurrentFrame and its
   * pre-effects ink overflow rect can be passed in as
   * aCurrentFrameOverflowArea. This is just an optimization to save a
   * frame property lookup - these arguments are optional.
   */
  PreEffectsInkOverflowCollector(nsIFrame* aFirstContinuation,
                                 nsIFrame* aCurrentFrame,
                                 const nsRect& aCurrentFrameOverflowArea,
                                 bool aInReflow)
      : mFirstContinuation(aFirstContinuation),
        mCurrentFrame(aCurrentFrame),
        mCurrentFrameOverflowArea(aCurrentFrameOverflowArea),
        mInReflow(aInReflow) {
    NS_ASSERTION(!mFirstContinuation->GetPrevContinuation(),
                 "We want the first continuation here");
  }

  virtual void AddBox(nsIFrame* aFrame) override {
    nsRect overflow = (aFrame == mCurrentFrame)
                          ? mCurrentFrameOverflowArea
                          : PreEffectsInkOverflowRect(aFrame, mInReflow);
    mResult.UnionRect(mResult,
                      overflow + aFrame->GetOffsetTo(mFirstContinuation));
  }

  nsRect GetResult() const { return mResult; }

 private:
  static nsRect PreEffectsInkOverflowRect(nsIFrame* aFrame, bool aInReflow) {
    nsRect* r = aFrame->GetProperty(nsIFrame::PreEffectsBBoxProperty());
    if (r) {
      return *r;
    }

#ifdef DEBUG
    // Having PreTransformOverflowAreasProperty cached means
    // InkOverflowRect() will return post-effect rect, which is not what
    // we want. This function intentional reports pre-effect rect. But it does
    // not matter if there is no SVG effect on this frame, since no effect
    // means post-effect rect matches pre-effect rect.
    //
    // This function may be called during reflow or painting. We should only
    // do this check in painting process since the PreEffectsBBoxProperty of
    // continuations are not set correctly while reflowing.
    if (SVGIntegrationUtils::UsingOverflowAffectingEffects(aFrame) &&
        !aInReflow) {
      OverflowAreas* preTransformOverflows =
          aFrame->GetProperty(nsIFrame::PreTransformOverflowAreasProperty());

      MOZ_ASSERT(!preTransformOverflows,
                 "InkOverflowRect() won't return the pre-effects rect!");
    }
#endif
    return aFrame->InkOverflowRectRelativeToSelf();
  }

  nsIFrame* mFirstContinuation;
  nsIFrame* mCurrentFrame;
  const nsRect& mCurrentFrameOverflowArea;
  nsRect mResult;
  bool mInReflow;
};

/**
 * Gets the union of the pre-effects ink overflow rects of all of a frame's
 * continuations, in "user space".
 */
static nsRect GetPreEffectsInkOverflowUnion(
    nsIFrame* aFirstContinuation, nsIFrame* aCurrentFrame,
    const nsRect& aCurrentFramePreEffectsOverflow,
    const nsPoint& aFirstContinuationToUserSpace, bool aInReflow) {
  NS_ASSERTION(!aFirstContinuation->GetPrevContinuation(),
               "Need first continuation here");
  PreEffectsInkOverflowCollector collector(aFirstContinuation, aCurrentFrame,
                                           aCurrentFramePreEffectsOverflow,
                                           aInReflow);
  // Compute union of all overflow areas relative to aFirstContinuation:
  nsLayoutUtils::GetAllInFlowBoxes(aFirstContinuation, &collector);
  // Return the result in user space:
  return collector.GetResult() + aFirstContinuationToUserSpace;
}

/**
 * Gets the pre-effects ink overflow rect of aCurrentFrame in "user space".
 */
static nsRect GetPreEffectsInkOverflow(
    nsIFrame* aFirstContinuation, nsIFrame* aCurrentFrame,
    const nsPoint& aFirstContinuationToUserSpace) {
  NS_ASSERTION(!aFirstContinuation->GetPrevContinuation(),
               "Need first continuation here");
  PreEffectsInkOverflowCollector collector(aFirstContinuation, nullptr,
                                           nsRect(), false);
  // Compute overflow areas of current frame relative to aFirstContinuation:
  nsLayoutUtils::AddBoxesForFrame(aCurrentFrame, &collector);
  // Return the result in user space:
  return collector.GetResult() + aFirstContinuationToUserSpace;
}

bool SVGIntegrationUtils::UsingOverflowAffectingEffects(
    const nsIFrame* aFrame) {
  // Currently overflow don't take account of SVG or other non-absolute
  // positioned clipping, or masking.
  return aFrame->StyleEffects()->HasFilters();
}

bool SVGIntegrationUtils::UsingEffectsForFrame(const nsIFrame* aFrame) {
  // Even when SVG display lists are disabled, returning true for SVG frames
  // does not adversely affect any of our callers. Therefore we don't bother
  // checking the SDL prefs here, since we don't know if we're being called for
  // painting or hit-testing anyway.
  const nsStyleSVGReset* style = aFrame->StyleSVGReset();
  const nsStyleEffects* effects = aFrame->StyleEffects();
  // TODO(cbrewster): remove backdrop-filter from this list once it is supported
  // in preserve-3d cases.
  return effects->HasFilters() || effects->HasBackdropFilters() ||
         style->HasClipPath() || style->HasMask();
}

bool SVGIntegrationUtils::UsingMaskOrClipPathForFrame(const nsIFrame* aFrame) {
  const nsStyleSVGReset* style = aFrame->StyleSVGReset();
  return style->HasClipPath() || style->HasMask();
}

bool SVGIntegrationUtils::UsingSimpleClipPathForFrame(const nsIFrame* aFrame) {
  const nsStyleSVGReset* style = aFrame->StyleSVGReset();
  if (!style->HasClipPath() || style->HasMask()) {
    return false;
  }

  const auto& clipPath = style->mClipPath;
  if (!clipPath.IsShape()) {
    return false;
  }

  return !clipPath.AsShape()._0->IsPolygon();
}

nsPoint SVGIntegrationUtils::GetOffsetToBoundingBox(nsIFrame* aFrame) {
  if (aFrame->HasAnyStateBits(NS_FRAME_SVG_LAYOUT)) {
    // Do NOT call GetAllInFlowRectsUnion for SVG - it will get the
    // covered region relative to the SVGOuterSVGFrame, which is absolutely
    // not what we want. SVG frames are always in user space, so they have
    // no offset adjustment to make.
    return nsPoint();
  }

  // The GetAllInFlowRectsUnion() call gets the union of the frame border-box
  // rects over all continuations, relative to the origin (top-left of the
  // border box) of its second argument (here, aFrame, the first continuation).
  return -nsLayoutUtils::GetAllInFlowRectsUnion(aFrame, aFrame).TopLeft();
}

struct EffectOffsets {
  // The offset between the reference frame and the bounding box of the
  // target frame in app unit.
  nsPoint offsetToBoundingBox;
  // The offset between the reference frame and the bounding box of the
  // target frame in app unit.
  nsPoint offsetToUserSpace;
  // The offset between the reference frame and the bounding box of the
  // target frame in device unit.
  gfxPoint offsetToUserSpaceInDevPx;
};

static EffectOffsets ComputeEffectOffset(
    nsIFrame* aFrame, const SVGIntegrationUtils::PaintFramesParams& aParams) {
  EffectOffsets result;

  result.offsetToBoundingBox =
      aParams.builder->ToReferenceFrame(aFrame) -
      SVGIntegrationUtils::GetOffsetToBoundingBox(aFrame);
  if (!aFrame->IsFrameOfType(nsIFrame::eSVG)) {
    /* Snap the offset if the reference frame is not a SVG frame,
     * since other frames will be snapped to pixel when rendering. */
    result.offsetToBoundingBox =
        nsPoint(aFrame->PresContext()->RoundAppUnitsToNearestDevPixels(
                    result.offsetToBoundingBox.x),
                aFrame->PresContext()->RoundAppUnitsToNearestDevPixels(
                    result.offsetToBoundingBox.y));
  }

  // After applying only "aOffsetToBoundingBox", aParams.ctx would have its
  // origin at the top left corner of frame's bounding box (over all
  // continuations).
  // However, SVG painting needs the origin to be located at the origin of the
  // SVG frame's "user space", i.e. the space in which, for example, the
  // frame's BBox lives.
  // SVG geometry frames and foreignObject frames apply their own offsets, so
  // their position is relative to their user space. So for these frame types,
  // if we want aParams.ctx to be in user space, we first need to subtract the
  // frame's position so that SVG painting can later add it again and the
  // frame is painted in the right place.
  gfxPoint toUserSpaceGfx =
      SVGUtils::FrameSpaceInCSSPxToUserSpaceOffset(aFrame);
  nsPoint toUserSpace =
      nsPoint(nsPresContext::CSSPixelsToAppUnits(float(toUserSpaceGfx.x)),
              nsPresContext::CSSPixelsToAppUnits(float(toUserSpaceGfx.y)));

  result.offsetToUserSpace = result.offsetToBoundingBox - toUserSpace;

#ifdef DEBUG
  bool hasSVGLayout = aFrame->HasAnyStateBits(NS_FRAME_SVG_LAYOUT);
  NS_ASSERTION(
      hasSVGLayout || result.offsetToBoundingBox == result.offsetToUserSpace,
      "For non-SVG frames there shouldn't be any additional offset");
#endif

  result.offsetToUserSpaceInDevPx = nsLayoutUtils::PointToGfxPoint(
      result.offsetToUserSpace, aFrame->PresContext()->AppUnitsPerDevPixel());

  return result;
}

/**
 * Setup transform matrix of a gfx context by a specific frame. Move the
 * origin of aParams.ctx to the user space of aFrame.
 */
static EffectOffsets MoveContextOriginToUserSpace(
    nsIFrame* aFrame, const SVGIntegrationUtils::PaintFramesParams& aParams) {
  EffectOffsets offset = ComputeEffectOffset(aFrame, aParams);

  aParams.ctx.SetMatrixDouble(aParams.ctx.CurrentMatrixDouble().PreTranslate(
      offset.offsetToUserSpaceInDevPx));

  return offset;
}

gfxPoint SVGIntegrationUtils::GetOffsetToUserSpaceInDevPx(
    nsIFrame* aFrame, const PaintFramesParams& aParams) {
  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(aFrame);
  EffectOffsets offset = ComputeEffectOffset(firstFrame, aParams);
  return offset.offsetToUserSpaceInDevPx;
}

/* static */
nsSize SVGIntegrationUtils::GetContinuationUnionSize(nsIFrame* aNonSVGFrame) {
  NS_ASSERTION(!aNonSVGFrame->IsFrameOfType(nsIFrame::eSVG),
               "SVG frames should not get here");
  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(aNonSVGFrame);
  return nsLayoutUtils::GetAllInFlowRectsUnion(firstFrame, firstFrame).Size();
}

/* static */ gfx::Size SVGIntegrationUtils::GetSVGCoordContextForNonSVGFrame(
    nsIFrame* aNonSVGFrame) {
  NS_ASSERTION(!aNonSVGFrame->IsFrameOfType(nsIFrame::eSVG),
               "SVG frames should not get here");
  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(aNonSVGFrame);
  nsRect r = nsLayoutUtils::GetAllInFlowRectsUnion(firstFrame, firstFrame);
  return gfx::Size(nsPresContext::AppUnitsToFloatCSSPixels(r.width),
                   nsPresContext::AppUnitsToFloatCSSPixels(r.height));
}

gfxRect SVGIntegrationUtils::GetSVGBBoxForNonSVGFrame(
    nsIFrame* aNonSVGFrame, bool aUnionContinuations) {
  // Except for SVGOuterSVGFrame, we shouldn't be getting here with SVG
  // frames at all. This function is for elements that are laid out using the
  // CSS box model rules.
  NS_ASSERTION(!aNonSVGFrame->HasAnyStateBits(NS_FRAME_SVG_LAYOUT),
               "Frames with SVG layout should not get here");
  MOZ_ASSERT(!aNonSVGFrame->IsFrameOfType(nsIFrame::eSVG) ||
             aNonSVGFrame->IsSVGOuterSVGFrame());

  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(aNonSVGFrame);
  // 'r' is in "user space":
  nsRect r = (aUnionContinuations)
                 ? GetPreEffectsInkOverflowUnion(
                       firstFrame, nullptr, nsRect(),
                       GetOffsetToBoundingBox(firstFrame), false)
                 : GetPreEffectsInkOverflow(firstFrame, aNonSVGFrame,
                                            GetOffsetToBoundingBox(firstFrame));

  return nsLayoutUtils::RectToGfxRect(r, AppUnitsPerCSSPixel());
}

// XXX Since we're called during reflow, this method is broken for frames with
// continuations. When we're called for a frame with continuations, we're
// called for each continuation in turn as it's reflowed. However, it isn't
// until the last continuation is reflowed that this method's
// GetOffsetToBoundingBox() and GetPreEffectsInkOverflowUnion() calls will
// obtain valid border boxes for all the continuations. As a result, we'll
// end up returning bogus post-filter ink overflow rects for all the prior
// continuations. Unfortunately, by the time the last continuation is
// reflowed, it's too late to go back and set and propagate the overflow
// rects on the previous continuations.
//
// The reason that we need to pass an override bbox to
// GetPreEffectsInkOverflowUnion rather than just letting it call into our
// GetSVGBBoxForNonSVGFrame method is because we get called by
// ComputeEffectsRect when it has been called with
// aStoreRectProperties set to false. In this case the pre-effects visual
// overflow rect that it has been passed may be different to that stored on
// aFrame, resulting in a different bbox.
//
// XXXjwatt The pre-effects ink overflow rect passed to
// ComputeEffectsRect won't include continuation overflows, so
// for frames with continuation the following filter analysis will likely end
// up being carried out with a bbox created as if the frame didn't have
// continuations.
//
// XXXjwatt Using aPreEffectsOverflowRect to create the bbox isn't really right
// for SVG frames, since for SVG frames the SVG spec defines the bbox to be
// something quite different to the pre-effects ink overflow rect. However,
// we're essentially calculating an invalidation area here, and using the
// pre-effects overflow rect will actually overestimate that area which, while
// being a bit wasteful, isn't otherwise a problem.
//
nsRect SVGIntegrationUtils::ComputePostEffectsInkOverflowRect(
    nsIFrame* aFrame, const nsRect& aPreEffectsOverflowRect) {
  MOZ_ASSERT(!aFrame->HasAnyStateBits(NS_FRAME_SVG_LAYOUT),
             "Don't call this on SVG child frames");

  MOZ_ASSERT(aFrame->StyleEffects()->HasFilters(),
             "We should only be called if the frame is filtered, since filters "
             "are the only effect that affects overflow.");

  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(aFrame);
  // Note: we do not return here for eHasNoRefs since we must still handle any
  // CSS filter functions.
  // TODO: We currently pass nullptr instead of an nsTArray* here, but we
  // actually should get the filter frames and then pass them into
  // GetPostFilterBounds below!  See bug 1494263.
  // TODO: we should really return an empty rect for eHasRefsSomeInvalid since
  // in that case we disable painting of the element.
  if (SVGObserverUtils::GetAndObserveFilters(firstFrame, nullptr) ==
      SVGObserverUtils::eHasRefsSomeInvalid) {
    return aPreEffectsOverflowRect;
  }

  // Create an override bbox - see comment above:
  nsPoint firstFrameToBoundingBox = GetOffsetToBoundingBox(firstFrame);
  // overrideBBox is in "user space", in _CSS_ pixels:
  // XXX Why are we rounding out to pixel boundaries? We don't do that in
  // GetSVGBBoxForNonSVGFrame, and it doesn't appear to be necessary.
  gfxRect overrideBBox = nsLayoutUtils::RectToGfxRect(
      GetPreEffectsInkOverflowUnion(firstFrame, aFrame, aPreEffectsOverflowRect,
                                    firstFrameToBoundingBox, true),
      AppUnitsPerCSSPixel());
  overrideBBox.RoundOut();

  nsRect overflowRect =
      FilterInstance::GetPostFilterBounds(firstFrame, &overrideBBox);

  // Return overflowRect relative to aFrame, rather than "user space":
  return overflowRect -
         (aFrame->GetOffsetTo(firstFrame) + firstFrameToBoundingBox);
}

nsIntRegion SVGIntegrationUtils::AdjustInvalidAreaForSVGEffects(
    nsIFrame* aFrame, const nsPoint& aToReferenceFrame,
    const nsIntRegion& aInvalidRegion) {
  if (aInvalidRegion.IsEmpty()) {
    return nsIntRect();
  }

  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(aFrame);

  // If we have any filters to observe then we should have started doing that
  // during reflow/ComputeFrameEffectsRect, so we use GetFiltersIfObserving
  // here to avoid needless work (or masking bugs by setting up observers at
  // the wrong time).
  if (!aFrame->StyleEffects()->HasFilters() ||
      SVGObserverUtils::GetFiltersIfObserving(firstFrame, nullptr) ==
          SVGObserverUtils::eHasRefsSomeInvalid) {
    return aInvalidRegion;
  }

  int32_t appUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();

  // Convert aInvalidRegion into bounding box frame space in app units:
  nsPoint toBoundingBox =
      aFrame->GetOffsetTo(firstFrame) + GetOffsetToBoundingBox(firstFrame);
  // The initial rect was relative to the reference frame, so we need to
  // remove that offset to get a rect relative to the current frame.
  toBoundingBox -= aToReferenceFrame;
  nsRegion preEffectsRegion =
      aInvalidRegion.ToAppUnits(appUnitsPerDevPixel).MovedBy(toBoundingBox);

  // Adjust the dirty area for effects, and shift it back to being relative to
  // the reference frame.
  nsRegion result =
      FilterInstance::GetPostFilterDirtyArea(firstFrame, preEffectsRegion)
          .MovedBy(-toBoundingBox);
  // Return the result, in pixels relative to the reference frame.
  return result.ToOutsidePixels(appUnitsPerDevPixel);
}

nsRect SVGIntegrationUtils::GetRequiredSourceForInvalidArea(
    nsIFrame* aFrame, const nsRect& aDirtyRect) {
  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(aFrame);

  // If we have any filters to observe then we should have started doing that
  // during reflow/ComputeFrameEffectsRect, so we use GetFiltersIfObserving
  // here to avoid needless work (or masking bugs by setting up observers at
  // the wrong time).
  if (!aFrame->StyleEffects()->HasFilters() ||
      SVGObserverUtils::GetFiltersIfObserving(firstFrame, nullptr) ==
          SVGObserverUtils::eHasRefsSomeInvalid) {
    return aDirtyRect;
  }

  // Convert aDirtyRect into "user space" in app units:
  nsPoint toUserSpace =
      aFrame->GetOffsetTo(firstFrame) + GetOffsetToBoundingBox(firstFrame);
  nsRect postEffectsRect = aDirtyRect + toUserSpace;

  // Return ther result, relative to aFrame, not in user space:
  return FilterInstance::GetPreFilterNeededArea(firstFrame, postEffectsRect)
             .GetBounds() -
         toUserSpace;
}

bool SVGIntegrationUtils::HitTestFrameForEffects(nsIFrame* aFrame,
                                                 const nsPoint& aPt) {
  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(aFrame);
  // Convert aPt to user space:
  nsPoint toUserSpace;
  if (aFrame->HasAnyStateBits(NS_FRAME_SVG_LAYOUT)) {
    // XXXmstange Isn't this wrong for svg:use and innerSVG frames?
    toUserSpace = aFrame->GetPosition();
  } else {
    toUserSpace =
        aFrame->GetOffsetTo(firstFrame) + GetOffsetToBoundingBox(firstFrame);
  }
  nsPoint pt = aPt + toUserSpace;
  gfxPoint userSpacePt = gfxPoint(pt.x, pt.y) / AppUnitsPerCSSPixel();
  return SVGUtils::HitTestClip(firstFrame, userSpacePt);
}

using PaintFramesParams = SVGIntegrationUtils::PaintFramesParams;

/**
 * Paint css-positioned-mask onto a given target(aMaskDT).
 * Return value indicates if mask is complete.
 */
static bool PaintMaskSurface(const PaintFramesParams& aParams,
                             DrawTarget* aMaskDT, float aOpacity,
                             ComputedStyle* aSC,
                             const nsTArray<SVGMaskFrame*>& aMaskFrames,
                             const Matrix& aMaskSurfaceMatrix,
                             const nsPoint& aOffsetToUserSpace) {
  MOZ_ASSERT(aMaskFrames.Length() > 0);
  MOZ_ASSERT(aMaskDT->GetFormat() == SurfaceFormat::A8);
  MOZ_ASSERT(aOpacity == 1.0 || aMaskFrames.Length() == 1);

  const nsStyleSVGReset* svgReset = aSC->StyleSVGReset();
  gfxMatrix cssPxToDevPxMatrix = SVGUtils::GetCSSPxToDevPxMatrix(aParams.frame);

  nsPresContext* presContext = aParams.frame->PresContext();
  gfxPoint devPixelOffsetToUserSpace = nsLayoutUtils::PointToGfxPoint(
      aOffsetToUserSpace, presContext->AppUnitsPerDevPixel());

  RefPtr<gfxContext> maskContext =
      gfxContext::CreatePreservingTransformOrNull(aMaskDT);
  MOZ_ASSERT(maskContext);

  bool isMaskComplete = true;

  // Multiple SVG masks interleave with image mask. Paint each layer onto
  // aMaskDT one at a time.
  for (int i = aMaskFrames.Length() - 1; i >= 0; i--) {
    SVGMaskFrame* maskFrame = aMaskFrames[i];
    CompositionOp compositionOp =
        (i == int(aMaskFrames.Length() - 1))
            ? CompositionOp::OP_OVER
            : nsCSSRendering::GetGFXCompositeMode(
                  svgReset->mMask.mLayers[i].mComposite);

    // maskFrame != nullptr means we get a SVG mask.
    // maskFrame == nullptr means we get an image mask.
    if (maskFrame) {
      SVGMaskFrame::MaskParams params(
          maskContext, aParams.frame, cssPxToDevPxMatrix, aOpacity,
          svgReset->mMask.mLayers[i].mMaskMode, aParams.imgParams);
      RefPtr<SourceSurface> svgMask = maskFrame->GetMaskForMaskedFrame(params);
      if (svgMask) {
        Matrix tmp = aMaskDT->GetTransform();
        aMaskDT->SetTransform(Matrix());
        aMaskDT->MaskSurface(ColorPattern(DeviceColor(0.0, 0.0, 0.0, 1.0)),
                             svgMask, Point(0, 0),
                             DrawOptions(1.0, compositionOp));
        aMaskDT->SetTransform(tmp);
      }
    } else if (svgReset->mMask.mLayers[i].mImage.IsResolved()) {
      gfxContextMatrixAutoSaveRestore matRestore(maskContext);

      maskContext->Multiply(gfxMatrix::Translation(-devPixelOffsetToUserSpace));
      nsCSSRendering::PaintBGParams params =
          nsCSSRendering::PaintBGParams::ForSingleLayer(
              *presContext, aParams.dirtyRect, aParams.borderArea,
              aParams.frame,
              aParams.builder->GetBackgroundPaintFlags() |
                  nsCSSRendering::PAINTBG_MASK_IMAGE,
              i, compositionOp, aOpacity);

      aParams.imgParams.result &= nsCSSRendering::PaintStyleImageLayerWithSC(
          params, *maskContext, aSC, *aParams.frame->StyleBorder());
    } else {
      isMaskComplete = false;
    }
  }

  return isMaskComplete;
}

struct MaskPaintResult {
  RefPtr<SourceSurface> maskSurface;
  Matrix maskTransform;
  bool transparentBlackMask;
  bool opacityApplied;

  MaskPaintResult() : transparentBlackMask(false), opacityApplied(false) {}
};

static MaskPaintResult CreateAndPaintMaskSurface(
    const PaintFramesParams& aParams, float aOpacity, ComputedStyle* aSC,
    const nsTArray<SVGMaskFrame*>& aMaskFrames,
    const nsPoint& aOffsetToUserSpace) {
  const nsStyleSVGReset* svgReset = aSC->StyleSVGReset();
  MOZ_ASSERT(aMaskFrames.Length() > 0);
  MaskPaintResult paintResult;

  gfxContext& ctx = aParams.ctx;

  // Optimization for single SVG mask.
  if (((aMaskFrames.Length() == 1) && aMaskFrames[0])) {
    gfxMatrix cssPxToDevPxMatrix =
        SVGUtils::GetCSSPxToDevPxMatrix(aParams.frame);
    paintResult.opacityApplied = true;
    SVGMaskFrame::MaskParams params(
        &ctx, aParams.frame, cssPxToDevPxMatrix, aOpacity,
        svgReset->mMask.mLayers[0].mMaskMode, aParams.imgParams);
    paintResult.maskSurface = aMaskFrames[0]->GetMaskForMaskedFrame(params);
    paintResult.maskTransform = ctx.CurrentMatrix();
    paintResult.maskTransform.Invert();
    if (!paintResult.maskSurface) {
      paintResult.transparentBlackMask = true;
    }

    return paintResult;
  }

  const Rect& maskSurfaceRect = aParams.maskRect.valueOr(Rect());
  if (aParams.maskRect.isSome() && maskSurfaceRect.IsEmpty()) {
    // XXX: Is this ever true?
    paintResult.transparentBlackMask = true;
    return paintResult;
  }

  RefPtr<DrawTarget> maskDT = ctx.GetDrawTarget()->CreateClippedDrawTarget(
      maskSurfaceRect, SurfaceFormat::A8);
  if (!maskDT || !maskDT->IsValid()) {
    return paintResult;
  }

  // We can paint mask along with opacity only if
  // 1. There is only one mask, or
  // 2. No overlap among masks.
  // Collision detect in #2 is not that trivial, we only accept #1 here.
  paintResult.opacityApplied = (aMaskFrames.Length() == 1);

  // Set context's matrix on maskContext, offset by the maskSurfaceRect's
  // position. This makes sure that we combine the masks in device space.
  Matrix maskSurfaceMatrix = ctx.CurrentMatrix();

  bool isMaskComplete = PaintMaskSurface(
      aParams, maskDT, paintResult.opacityApplied ? aOpacity : 1.0, aSC,
      aMaskFrames, maskSurfaceMatrix, aOffsetToUserSpace);

  if (!isMaskComplete ||
      (aParams.imgParams.result != ImgDrawResult::SUCCESS &&
       aParams.imgParams.result != ImgDrawResult::SUCCESS_NOT_COMPLETE &&
       aParams.imgParams.result != ImgDrawResult::WRONG_SIZE)) {
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
    //   Left paintResult.maskSurface empty, the caller should paint all
    //   masked content as if this mask is an opaque white one(no mask).
    paintResult.transparentBlackMask =
        !aParams.frame->HasAnyStateBits(NS_FRAME_SVG_LAYOUT);

    MOZ_ASSERT(!paintResult.maskSurface);
    return paintResult;
  }

  paintResult.maskTransform = maskSurfaceMatrix;
  if (!paintResult.maskTransform.Invert()) {
    return paintResult;
  }

  paintResult.maskSurface = maskDT->Snapshot();
  return paintResult;
}

static bool ValidateSVGFrame(nsIFrame* aFrame) {
#ifdef DEBUG
  NS_ASSERTION(!aFrame->HasAnyStateBits(NS_FRAME_SVG_LAYOUT) ||
                   (NS_SVGDisplayListPaintingEnabled() &&
                    !aFrame->HasAnyStateBits(NS_FRAME_IS_NONDISPLAY)),
               "Should not use SVGIntegrationUtils on this SVG frame");
#endif

  bool hasSVGLayout = aFrame->HasAnyStateBits(NS_FRAME_SVG_LAYOUT);
  if (hasSVGLayout) {
#ifdef DEBUG
    ISVGDisplayableFrame* svgFrame = do_QueryFrame(aFrame);
    MOZ_ASSERT(svgFrame && aFrame->GetContent()->IsSVGElement(),
               "A non-SVG frame carries NS_FRAME_SVG_LAYOUT flag?");
#endif

    const nsIContent* content = aFrame->GetContent();
    if (!static_cast<const SVGElement*>(content)->HasValidDimensions()) {
      // The SVG spec says not to draw _anything_
      return false;
    }
  }

  return true;
}

bool SVGIntegrationUtils::IsMaskResourceReady(nsIFrame* aFrame) {
  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(aFrame);
  nsTArray<SVGMaskFrame*> maskFrames;
  // XXX check return value?
  SVGObserverUtils::GetAndObserveMasks(firstFrame, &maskFrames);

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

class AutoPopGroup {
 public:
  AutoPopGroup() : mContext(nullptr) {}

  ~AutoPopGroup() {
    if (mContext) {
      mContext->PopGroupAndBlend();
    }
  }

  void SetContext(gfxContext* aContext) { mContext = aContext; }

 private:
  gfxContext* mContext;
};

bool SVGIntegrationUtils::PaintMask(const PaintFramesParams& aParams,
                                    bool& aOutIsMaskComplete) {
  aOutIsMaskComplete = true;

  SVGUtils::MaskUsage maskUsage;
  SVGUtils::DetermineMaskUsage(aParams.frame, aParams.handleOpacity, maskUsage);
  if (!maskUsage.shouldDoSomething()) {
    return false;
  }

  nsIFrame* frame = aParams.frame;
  if (!ValidateSVGFrame(frame)) {
    return false;
  }

  gfxContext& ctx = aParams.ctx;
  RefPtr<DrawTarget> maskTarget = ctx.GetDrawTarget();

  if (maskUsage.shouldGenerateMaskLayer &&
      (maskUsage.shouldGenerateClipMaskLayer ||
       maskUsage.shouldApplyClipPath)) {
    // We will paint both mask of positioned mask and clip-path into
    // maskTarget.
    //
    // Create one extra draw target for drawing positioned mask, so that we do
    // not have to copy the content of maskTarget before painting
    // clip-path into it.
    if (!maskTarget->CanCreateSimilarDrawTarget(maskTarget->GetSize(),
                                                SurfaceFormat::A8)) {
      return false;
    }
    maskTarget = maskTarget->CreateSimilarDrawTarget(maskTarget->GetSize(),
                                                     SurfaceFormat::A8);
  }

  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(frame);
  nsTArray<SVGMaskFrame*> maskFrames;
  // XXX check return value?
  SVGObserverUtils::GetAndObserveMasks(firstFrame, &maskFrames);

  AutoPopGroup autoPop;
  bool shouldPushOpacity =
      (maskUsage.opacity != 1.0) && (maskFrames.Length() != 1);
  if (shouldPushOpacity) {
    ctx.PushGroupForBlendBack(gfxContentType::COLOR_ALPHA, maskUsage.opacity);
    autoPop.SetContext(&ctx);
  }

  gfxContextMatrixAutoSaveRestore matSR;

  // Paint clip-path-basic-shape onto ctx
  gfxContextAutoSaveRestore basicShapeSR;
  if (maskUsage.shouldApplyBasicShapeOrPath) {
    matSR.SetContext(&ctx);

    MoveContextOriginToUserSpace(firstFrame, aParams);

    basicShapeSR.SetContext(&ctx);
    CSSClipPathInstance::ApplyBasicShapeOrPathClip(
        ctx, frame, SVGUtils::GetCSSPxToDevPxMatrix(frame));
    if (!maskUsage.shouldGenerateMaskLayer) {
      // Only have basic-shape clip-path effect. Fill clipped region by
      // opaque white.
      ctx.SetDeviceColor(DeviceColor::MaskOpaqueWhite());
      ctx.Fill();

      return true;
    }
  }

  // Paint mask onto ctx.
  if (maskUsage.shouldGenerateMaskLayer) {
    matSR.Restore();
    matSR.SetContext(&ctx);

    EffectOffsets offsets = MoveContextOriginToUserSpace(frame, aParams);
    aOutIsMaskComplete = PaintMaskSurface(
        aParams, maskTarget, shouldPushOpacity ? 1.0 : maskUsage.opacity,
        firstFrame->Style(), maskFrames, ctx.CurrentMatrix(),
        offsets.offsetToUserSpace);
  }

  // Paint clip-path onto ctx.
  if (maskUsage.shouldGenerateClipMaskLayer || maskUsage.shouldApplyClipPath) {
    matSR.Restore();
    matSR.SetContext(&ctx);

    MoveContextOriginToUserSpace(firstFrame, aParams);
    Matrix clipMaskTransform;
    gfxMatrix cssPxToDevPxMatrix = SVGUtils::GetCSSPxToDevPxMatrix(frame);

    SVGClipPathFrame* clipPathFrame;
    // XXX check return value?
    SVGObserverUtils::GetAndObserveClipPath(firstFrame, &clipPathFrame);
    RefPtr<SourceSurface> maskSurface =
        maskUsage.shouldGenerateMaskLayer ? maskTarget->Snapshot() : nullptr;
    clipPathFrame->PaintClipMask(ctx, frame, cssPxToDevPxMatrix, maskSurface,
                                 ctx.CurrentMatrix());
  }

  return true;
}

template <class T>
void PaintMaskAndClipPathInternal(const PaintFramesParams& aParams,
                                  const T& aPaintChild) {
  MOZ_ASSERT(SVGIntegrationUtils::UsingMaskOrClipPathForFrame(aParams.frame),
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
  if (!ValidateSVGFrame(frame)) {
    return;
  }

  SVGUtils::MaskUsage maskUsage;
  SVGUtils::DetermineMaskUsage(aParams.frame, aParams.handleOpacity, maskUsage);

  if (maskUsage.opacity == 0.0f) {
    return;
  }

  gfxContext& context = aParams.ctx;
  gfxContextMatrixAutoSaveRestore matrixAutoSaveRestore(&context);

  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(frame);

  SVGClipPathFrame* clipPathFrame;
  // XXX check return value?
  SVGObserverUtils::GetAndObserveClipPath(firstFrame, &clipPathFrame);

  nsTArray<SVGMaskFrame*> maskFrames;
  // XXX check return value?
  SVGObserverUtils::GetAndObserveMasks(firstFrame, &maskFrames);

  gfxMatrix cssPxToDevPxMatrix = SVGUtils::GetCSSPxToDevPxMatrix(frame);

  bool shouldGenerateMask =
      (maskUsage.opacity != 1.0f || maskUsage.shouldGenerateClipMaskLayer ||
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
      EffectOffsets offsets = MoveContextOriginToUserSpace(frame, aParams);
      MaskPaintResult paintResult = CreateAndPaintMaskSurface(
          aParams, maskUsage.opacity, firstFrame->Style(), maskFrames,
          offsets.offsetToUserSpace);

      if (paintResult.transparentBlackMask) {
        return;
      }

      maskSurface = paintResult.maskSurface;
      if (maskSurface) {
        shouldPushMask = true;
        // We want the mask to be untransformed so use the inverse of the
        // current transform as the maskTransform to compensate.
        maskTransform = context.CurrentMatrix();
        maskTransform.Invert();

        opacityApplied = paintResult.opacityApplied;
      }
    }

    if (maskUsage.shouldGenerateClipMaskLayer) {
      matSR.Restore();
      matSR.SetContext(&context);

      MoveContextOriginToUserSpace(firstFrame, aParams);
      RefPtr<SourceSurface> clipMaskSurface = clipPathFrame->GetClipMask(
          context, frame, cssPxToDevPxMatrix, maskSurface, maskTransform);

      if (clipMaskSurface) {
        maskSurface = clipMaskSurface;
        // We want the mask to be untransformed so use the inverse of the
        // current transform as the maskTransform to compensate.
        maskTransform = context.CurrentMatrix();
        maskTransform.Invert();
      } else {
        // Either entire surface is clipped out, or gfx buffer allocation
        // failure in SVGClipPathFrame::GetClipMask.
        return;
      }

      shouldPushMask = true;
    }

    // opacity != 1.0f.
    if (!maskUsage.shouldGenerateClipMaskLayer &&
        !maskUsage.shouldGenerateMaskLayer) {
      MOZ_ASSERT(maskUsage.opacity != 1.0f);

      matSR.SetContext(&context);
      MoveContextOriginToUserSpace(firstFrame, aParams);
      shouldPushMask = true;
    }

    if (shouldPushMask) {
      if (aParams.layerManager &&
          aParams.layerManager->GetRoot()->GetContentFlags() &
              Layer::CONTENT_COMPONENT_ALPHA) {
        context.PushGroupAndCopyBackground(
            gfxContentType::COLOR_ALPHA,
            opacityApplied ? 1.0 : maskUsage.opacity, maskSurface,
            maskTransform);
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
  if (maskUsage.shouldApplyClipPath || maskUsage.shouldApplyBasicShapeOrPath) {
    gfxContextMatrixAutoSaveRestore matSR(&context);

    MoveContextOriginToUserSpace(firstFrame, aParams);

    MOZ_ASSERT(!maskUsage.shouldApplyClipPath ||
               !maskUsage.shouldApplyBasicShapeOrPath);
    if (maskUsage.shouldApplyClipPath) {
      clipPathFrame->ApplyClipPath(context, frame, cssPxToDevPxMatrix);
    } else {
      CSSClipPathInstance::ApplyBasicShapeOrPathClip(context, frame,
                                                     cssPxToDevPxMatrix);
    }
  }

  /* Paint the child */
  context.SetMatrix(matrixAutoSaveRestore.Matrix());
  aPaintChild();

  if (StaticPrefs::layers_draw_mask_debug()) {
    gfxContextAutoSaveRestore saver(&context);

    context.NewPath();
    gfxRect drawingRect = nsLayoutUtils::RectToGfxRect(
        aParams.borderArea, frame->PresContext()->AppUnitsPerDevPixel());
    context.SnappedRectangle(drawingRect);
    sRGBColor overlayColor(0.0f, 0.0f, 0.0f, 0.8f);
    if (maskUsage.shouldGenerateMaskLayer) {
      overlayColor.r = 1.0f;  // red represents css positioned mask.
    }
    if (maskUsage.shouldApplyClipPath ||
        maskUsage.shouldGenerateClipMaskLayer) {
      overlayColor.g = 1.0f;  // green represents clip-path:<clip-source>.
    }
    if (maskUsage.shouldApplyBasicShapeOrPath) {
      overlayColor.b = 1.0f;  // blue represents
                              // clip-path:<basic-shape>||<geometry-box>.
    }

    context.SetColor(overlayColor);
    context.Fill();
  }

  if (maskUsage.shouldApplyClipPath || maskUsage.shouldApplyBasicShapeOrPath) {
    context.PopClip();
  }

  if (shouldPushMask) {
    context.PopGroupAndBlend();
  }
}

void SVGIntegrationUtils::PaintMaskAndClipPath(
    const PaintFramesParams& aParams,
    const std::function<void()>& aPaintChild) {
  PaintMaskAndClipPathInternal(aParams, aPaintChild);
}

void SVGIntegrationUtils::PaintFilter(const PaintFramesParams& aParams,
                                      const SVGFilterPaintCallback& aCallback) {
  MOZ_ASSERT(!aParams.builder->IsForGenerateGlyphMask(),
             "Filter effect is discarded while generating glyph mask.");
  MOZ_ASSERT(aParams.frame->StyleEffects()->HasFilters(),
             "Should not use this method when no filter effect on this frame");

  nsIFrame* frame = aParams.frame;
  if (!ValidateSVGFrame(frame)) {
    return;
  }

  float opacity = SVGUtils::ComputeOpacity(frame, aParams.handleOpacity);
  if (opacity == 0.0f) {
    return;
  }

  /* Properties are added lazily and may have been removed by a restyle,
     so make sure all applicable ones are set again. */
  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(frame);
  // Note: we do not return here for eHasNoRefs since we must still handle any
  // CSS filter functions.
  // TODO: We currently pass nullptr instead of an nsTArray* here, but we
  // actually should get the filter frames and then pass them into
  // PaintFilteredFrame below!  See bug 1494263.
  // XXX: Do we need to check for eHasRefsSomeInvalid here given that
  // nsDisplayFilter::BuildLayer returns nullptr for eHasRefsSomeInvalid?
  // Or can we just assert !eHasRefsSomeInvalid?
  if (SVGObserverUtils::GetAndObserveFilters(firstFrame, nullptr) ==
      SVGObserverUtils::eHasRefsSomeInvalid) {
    return;
  }

  gfxContext& context = aParams.ctx;

  gfxContextAutoSaveRestore autoSR(&context);
  EffectOffsets offsets = MoveContextOriginToUserSpace(firstFrame, aParams);

  /* Paint the child and apply filters */
  nsRegion dirtyRegion = aParams.dirtyRect - offsets.offsetToBoundingBox;

  FilterInstance::PaintFilteredFrame(frame, &context, aCallback, &dirtyRegion,
                                     aParams.imgParams, opacity);
}

bool SVGIntegrationUtils::CreateWebRenderCSSFilters(
    Span<const StyleFilter> aFilters, nsIFrame* aFrame,
    WrFiltersHolder& aWrFilters) {
  // All CSS filters are supported by WebRender. SVG filters are not fully
  // supported, those use NS_STYLE_FILTER_URL and are handled separately.

  // If there are too many filters to render, then just pretend that we
  // succeeded, and don't render any of them.
  if (aFilters.Length() >
      StaticPrefs::gfx_webrender_max_filter_ops_per_chain()) {
    return true;
  }
  aWrFilters.filters.SetCapacity(aFilters.Length());
  auto& wrFilters = aWrFilters.filters;
  for (const StyleFilter& filter : aFilters) {
    switch (filter.tag) {
      case StyleFilter::Tag::Brightness:
        wrFilters.AppendElement(
            wr::FilterOp::Brightness(filter.AsBrightness()));
        break;
      case StyleFilter::Tag::Contrast:
        wrFilters.AppendElement(wr::FilterOp::Contrast(filter.AsContrast()));
        break;
      case StyleFilter::Tag::Grayscale:
        wrFilters.AppendElement(wr::FilterOp::Grayscale(filter.AsGrayscale()));
        break;
      case StyleFilter::Tag::Invert:
        wrFilters.AppendElement(wr::FilterOp::Invert(filter.AsInvert()));
        break;
      case StyleFilter::Tag::Opacity: {
        float opacity = filter.AsOpacity();
        wrFilters.AppendElement(wr::FilterOp::Opacity(
            wr::PropertyBinding<float>::Value(opacity), opacity));
        break;
      }
      case StyleFilter::Tag::Saturate:
        wrFilters.AppendElement(wr::FilterOp::Saturate(filter.AsSaturate()));
        break;
      case StyleFilter::Tag::Sepia:
        wrFilters.AppendElement(wr::FilterOp::Sepia(filter.AsSepia()));
        break;
      case StyleFilter::Tag::HueRotate: {
        wrFilters.AppendElement(
            wr::FilterOp::HueRotate(filter.AsHueRotate().ToDegrees()));
        break;
      }
      case StyleFilter::Tag::Blur: {
        // TODO(emilio): we should go directly from css pixels -> device pixels.
        float appUnitsPerDevPixel =
            aFrame->PresContext()->AppUnitsPerDevPixel();
        float radius = NSAppUnitsToFloatPixels(filter.AsBlur().ToAppUnits(),
                                               appUnitsPerDevPixel);
        wrFilters.AppendElement(wr::FilterOp::Blur(radius, radius));
        break;
      }
      case StyleFilter::Tag::DropShadow: {
        float appUnitsPerDevPixel =
            aFrame->PresContext()->AppUnitsPerDevPixel();
        const StyleSimpleShadow& shadow = filter.AsDropShadow();
        nscolor color = shadow.color.CalcColor(aFrame);

        wr::Shadow wrShadow;
        wrShadow.offset = {
            NSAppUnitsToFloatPixels(shadow.horizontal.ToAppUnits(),
                                    appUnitsPerDevPixel),
            NSAppUnitsToFloatPixels(shadow.vertical.ToAppUnits(),
                                    appUnitsPerDevPixel)};
        wrShadow.blur_radius = NSAppUnitsToFloatPixels(shadow.blur.ToAppUnits(),
                                                       appUnitsPerDevPixel);
        wrShadow.color = {NS_GET_R(color) / 255.0f, NS_GET_G(color) / 255.0f,
                          NS_GET_B(color) / 255.0f, NS_GET_A(color) / 255.0f};
        wrFilters.AppendElement(wr::FilterOp::DropShadow(wrShadow));
        break;
      }
      default:
        return false;
    }
  }

  return true;
}

bool SVGIntegrationUtils::BuildWebRenderFilters(
    nsIFrame* aFilteredFrame, Span<const StyleFilter> aFilters,
    WrFiltersHolder& aWrFilters, Maybe<nsRect>& aPostFilterClip) {
  return FilterInstance::BuildWebRenderFilters(aFilteredFrame, aFilters,
                                               aWrFilters, aPostFilterClip);
}

bool SVGIntegrationUtils::CanCreateWebRenderFiltersForFrame(nsIFrame* aFrame) {
  WrFiltersHolder wrFilters;
  Maybe<nsRect> filterClip;
  auto filterChain = aFrame->StyleEffects()->mFilters.AsSpan();
  return CreateWebRenderCSSFilters(filterChain, aFrame, wrFilters) ||
         BuildWebRenderFilters(aFrame, filterChain, wrFilters, filterClip);
}

bool SVGIntegrationUtils::UsesSVGEffectsNotSupportedInCompositor(
    nsIFrame* aFrame) {
  // WebRender supports masks / clip-paths and some filters in the compositor.
  // Non-WebRender doesn't support any SVG effects in the compositor.
  if (aFrame->StyleEffects()->HasFilters()) {
    return !gfx::gfxVars::UseWebRender() ||
           !SVGIntegrationUtils::CanCreateWebRenderFiltersForFrame(aFrame);
  }
  if (SVGIntegrationUtils::UsingMaskOrClipPathForFrame(aFrame)) {
    return !gfx::gfxVars::UseWebRender();
  }
  return false;
}

class PaintFrameCallback : public gfxDrawingCallback {
 public:
  PaintFrameCallback(nsIFrame* aFrame, const nsSize aPaintServerSize,
                     const IntSize aRenderSize, uint32_t aFlags)
      : mFrame(aFrame),
        mPaintServerSize(aPaintServerSize),
        mRenderSize(aRenderSize),
        mFlags(aFlags) {}
  virtual bool operator()(gfxContext* aContext, const gfxRect& aFillRect,
                          const SamplingFilter aSamplingFilter,
                          const gfxMatrix& aTransform) override;

 private:
  nsIFrame* mFrame;
  nsSize mPaintServerSize;
  IntSize mRenderSize;
  uint32_t mFlags;
};

bool PaintFrameCallback::operator()(gfxContext* aContext,
                                    const gfxRect& aFillRect,
                                    const SamplingFilter aSamplingFilter,
                                    const gfxMatrix& aTransform) {
  if (mFrame->HasAnyStateBits(NS_FRAME_DRAWING_AS_PAINTSERVER)) {
    return false;
  }

  AutoSetRestorePaintServerState paintServer(mFrame);

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
  nsPoint offset = SVGIntegrationUtils::GetOffsetToBoundingBox(mFrame);
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
  nsRect dirty(-offset.x, -offset.y, mPaintServerSize.width,
               mPaintServerSize.height);

  using PaintFrameFlags = nsLayoutUtils::PaintFrameFlags;
  PaintFrameFlags flags = PaintFrameFlags::InTransform;
  if (mFlags & SVGIntegrationUtils::FLAG_SYNC_DECODE_IMAGES) {
    flags |= PaintFrameFlags::SyncDecodeImages;
  }
  nsLayoutUtils::PaintFrame(aContext, mFrame, dirty, NS_RGBA(0, 0, 0, 0),
                            nsDisplayListBuilderMode::Painting, flags);

  nsIFrame* currentFrame = mFrame;
  while ((currentFrame = currentFrame->GetNextContinuation()) != nullptr) {
    offset = currentFrame->GetOffsetToCrossDoc(mFrame);
    devPxOffset = gfxPoint(offset.x, offset.y) / appUnitsPerDevPixel;

    aContext->Save();
    aContext->Multiply(gfxMatrix::Scaling(1 / scaleX, 1 / scaleY));
    aContext->Multiply(gfxMatrix::Translation(devPxOffset));
    aContext->Multiply(gfxMatrix::Scaling(scaleX, scaleY));

    nsLayoutUtils::PaintFrame(aContext, currentFrame, dirty - offset,
                              NS_RGBA(0, 0, 0, 0),
                              nsDisplayListBuilderMode::Painting, flags);

    aContext->Restore();
  }

  aContext->Restore();

  return true;
}

/* static */
already_AddRefed<gfxDrawable> SVGIntegrationUtils::DrawableFromPaintServer(
    nsIFrame* aFrame, nsIFrame* aTarget, const nsSize& aPaintServerSize,
    const IntSize& aRenderSize, const DrawTarget* aDrawTarget,
    const gfxMatrix& aContextMatrix, uint32_t aFlags) {
  // aPaintServerSize is the size that would be filled when using
  // background-repeat:no-repeat and background-size:auto. For normal background
  // images, this would be the intrinsic size of the image; for gradients and
  // patterns this would be the whole target frame fill area.
  // aRenderSize is what we will be actually filling after accounting for
  // background-size.
  if (SVGPaintServerFrame* server = do_QueryFrame(aFrame)) {
    // aFrame is either a pattern or a gradient. These fill the whole target
    // frame by default, so aPaintServerSize is the whole target background fill
    // area.
    gfxRect overrideBounds(0, 0, aPaintServerSize.width,
                           aPaintServerSize.height);
    overrideBounds.Scale(1.0 / aFrame->PresContext()->AppUnitsPerDevPixel());
    imgDrawingParams imgParams(aFlags);
    RefPtr<gfxPattern> pattern = server->GetPaintServerPattern(
        aTarget, aDrawTarget, aContextMatrix, &nsStyleSVG::mFill, 1.0,
        imgParams, &overrideBounds);

    if (!pattern) {
      return nullptr;
    }

    // pattern is now set up to fill aPaintServerSize. But we want it to
    // fill aRenderSize, so we need to add a scaling transform.
    // We couldn't just have set overrideBounds to aRenderSize - it would have
    // worked for gradients, but for patterns it would result in a different
    // pattern size.
    gfxFloat scaleX = overrideBounds.Width() / aRenderSize.width;
    gfxFloat scaleY = overrideBounds.Height() / aRenderSize.height;
    gfxMatrix scaleMatrix = gfxMatrix::Scaling(scaleX, scaleY);
    pattern->SetMatrix(scaleMatrix * pattern->GetMatrix());
    RefPtr<gfxDrawable> drawable = new gfxPatternDrawable(pattern, aRenderSize);
    return drawable.forget();
  }

  if (aFrame->IsFrameOfType(nsIFrame::eSVG) &&
      !static_cast<ISVGDisplayableFrame*>(do_QueryFrame(aFrame))) {
    MOZ_ASSERT_UNREACHABLE(
        "We should prevent painting of unpaintable SVG "
        "before we get here");
    return nullptr;
  }

  // We don't want to paint into a surface as long as we don't need to, so we
  // set up a drawing callback.
  RefPtr<gfxDrawingCallback> cb =
      new PaintFrameCallback(aFrame, aPaintServerSize, aRenderSize, aFlags);
  RefPtr<gfxDrawable> drawable = new gfxCallbackDrawable(cb, aRenderSize);
  return drawable.forget();
}

}  // namespace mozilla
