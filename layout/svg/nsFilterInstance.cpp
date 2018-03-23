/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsFilterInstance.h"

// MFBT headers next:
#include "mozilla/UniquePtr.h"

// Keep others in (case-insensitive) order:
#include "ImgDrawResult.h"
#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/gfx/PatternHelpers.h"
#include "nsSVGDisplayableFrame.h"
#include "nsCSSFilterInstance.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGFilterPaintCallback.h"
#include "nsSVGUtils.h"
#include "SVGContentUtils.h"
#include "FilterSupport.h"
#include "gfx2DGlue.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;

FilterDescription
nsFilterInstance::GetFilterDescription(nsIContent* aFilteredElement,
                                       const nsTArray<nsStyleFilter>& aFilterChain,
                                       bool aFilterInputIsTainted,
                                       const UserSpaceMetrics& aMetrics,
                                       const gfxRect& aBBox,
                                       nsTArray<RefPtr<SourceSurface>>& aOutAdditionalImages)
{
  gfxMatrix identity;
  nsFilterInstance instance(nullptr, aFilteredElement, aMetrics,
                            aFilterChain, aFilterInputIsTainted, nullptr,
                            identity, nullptr, nullptr, nullptr, &aBBox);
  if (!instance.IsInitialized()) {
    return FilterDescription();
  }
  return instance.ExtractDescriptionAndAdditionalImages(aOutAdditionalImages);
}

static UniquePtr<UserSpaceMetrics>
UserSpaceMetricsForFrame(nsIFrame* aFrame)
{
  if (aFrame->GetContent()->IsSVGElement()) {
    nsSVGElement* element = static_cast<nsSVGElement*>(aFrame->GetContent());
    return MakeUnique<SVGElementMetrics>(element);
  }
  return MakeUnique<NonSVGFrameUserSpaceMetrics>(aFrame);
}

void
nsFilterInstance::PaintFilteredFrame(nsIFrame *aFilteredFrame,
                                     gfxContext* aCtx,
                                     nsSVGFilterPaintCallback *aPaintCallback,
                                     const nsRegion *aDirtyArea,
                                     imgDrawingParams& aImgParams)
{
  auto& filterChain = aFilteredFrame->StyleEffects()->mFilters;
  UniquePtr<UserSpaceMetrics> metrics = UserSpaceMetricsForFrame(aFilteredFrame);

  gfxContextMatrixAutoSaveRestore autoSR(aCtx);
  gfxSize scaleFactors = aCtx->CurrentMatrixDouble().ScaleFactors(true);
  if (scaleFactors.IsEmpty()) {
    return;
  }

  gfxMatrix scaleMatrix(scaleFactors.width, 0.0f,
                        0.0f, scaleFactors.height,
                        0.0f, 0.0f);

  gfxMatrix reverseScaleMatrix = scaleMatrix;
  DebugOnly<bool> invertible = reverseScaleMatrix.Invert();
  MOZ_ASSERT(invertible);
  // Pull scale vector out of aCtx's transform, put all scale factors, which
  // includes css and css-to-dev-px scale, into scaleMatrixInDevUnits.
  aCtx->SetMatrixDouble(reverseScaleMatrix * aCtx->CurrentMatrixDouble());

  gfxMatrix scaleMatrixInDevUnits =
    scaleMatrix * nsSVGUtils::GetCSSPxToDevPxMatrix(aFilteredFrame);

  // Hardcode InputIsTainted to true because we don't want JS to be able to
  // read the rendered contents of aFilteredFrame.
  nsFilterInstance instance(aFilteredFrame, aFilteredFrame->GetContent(),
                            *metrics, filterChain, /* InputIsTainted */ true,
                            aPaintCallback, scaleMatrixInDevUnits,
                            aDirtyArea, nullptr, nullptr, nullptr);
  if (instance.IsInitialized()) {
    instance.Render(aCtx, aImgParams);
  }
}

nsRegion
nsFilterInstance::GetPostFilterDirtyArea(nsIFrame *aFilteredFrame,
                                         const nsRegion& aPreFilterDirtyRegion)
{
  if (aPreFilterDirtyRegion.IsEmpty()) {
    return nsRegion();
  }

  gfxMatrix tm = nsSVGUtils::GetCanvasTM(aFilteredFrame);
  auto& filterChain = aFilteredFrame->StyleEffects()->mFilters;
  UniquePtr<UserSpaceMetrics> metrics = UserSpaceMetricsForFrame(aFilteredFrame);
  // Hardcode InputIsTainted to true because we don't want JS to be able to
  // read the rendered contents of aFilteredFrame.
  nsFilterInstance instance(aFilteredFrame, aFilteredFrame->GetContent(),
                            *metrics, filterChain, /* InputIsTainted */ true,
                            nullptr, tm, nullptr, &aPreFilterDirtyRegion);
  if (!instance.IsInitialized()) {
    return nsRegion();
  }

  // We've passed in the source's dirty area so the instance knows about it.
  // Now we can ask the instance to compute the area of the filter output
  // that's dirty.
  return instance.ComputePostFilterDirtyRegion();
}

nsRegion
nsFilterInstance::GetPreFilterNeededArea(nsIFrame *aFilteredFrame,
                                         const nsRegion& aPostFilterDirtyRegion)
{
  gfxMatrix tm = nsSVGUtils::GetCanvasTM(aFilteredFrame);
  auto& filterChain = aFilteredFrame->StyleEffects()->mFilters;
  UniquePtr<UserSpaceMetrics> metrics = UserSpaceMetricsForFrame(aFilteredFrame);
  // Hardcode InputIsTainted to true because we don't want JS to be able to
  // read the rendered contents of aFilteredFrame.
  nsFilterInstance instance(aFilteredFrame, aFilteredFrame->GetContent(),
                            *metrics, filterChain, /* InputIsTainted */ true,
                            nullptr, tm, &aPostFilterDirtyRegion);
  if (!instance.IsInitialized()) {
    return nsRect();
  }

  // Now we can ask the instance to compute the area of the source
  // that's needed.
  return instance.ComputeSourceNeededRect();
}

nsRect
nsFilterInstance::GetPostFilterBounds(nsIFrame *aFilteredFrame,
                                      const gfxRect *aOverrideBBox,
                                      const nsRect *aPreFilterBounds)
{
  MOZ_ASSERT(!(aFilteredFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT) ||
             !(aFilteredFrame->GetStateBits() & NS_FRAME_IS_NONDISPLAY),
             "Non-display SVG do not maintain visual overflow rects");

  nsRegion preFilterRegion;
  nsRegion* preFilterRegionPtr = nullptr;
  if (aPreFilterBounds) {
    preFilterRegion = *aPreFilterBounds;
    preFilterRegionPtr = &preFilterRegion;
  }

  gfxMatrix tm = nsSVGUtils::GetCanvasTM(aFilteredFrame);
  auto& filterChain = aFilteredFrame->StyleEffects()->mFilters;
  UniquePtr<UserSpaceMetrics> metrics = UserSpaceMetricsForFrame(aFilteredFrame);
  // Hardcode InputIsTainted to true because we don't want JS to be able to
  // read the rendered contents of aFilteredFrame.
  nsFilterInstance instance(aFilteredFrame, aFilteredFrame->GetContent(),
                            *metrics, filterChain, /* InputIsTainted */ true,
                            nullptr, tm, nullptr, preFilterRegionPtr,
                            aPreFilterBounds, aOverrideBBox);
  if (!instance.IsInitialized()) {
    return nsRect();
  }

  return instance.ComputePostFilterExtents();
}

nsFilterInstance::nsFilterInstance(nsIFrame *aTargetFrame,
                                   nsIContent* aTargetContent,
                                   const UserSpaceMetrics& aMetrics,
                                   const nsTArray<nsStyleFilter>& aFilterChain,
                                   bool aFilterInputIsTainted,
                                   nsSVGFilterPaintCallback *aPaintCallback,
                                   const gfxMatrix& aPaintTransform,
                                   const nsRegion *aPostFilterDirtyRegion,
                                   const nsRegion *aPreFilterDirtyRegion,
                                   const nsRect *aPreFilterVisualOverflowRectOverride,
                                   const gfxRect *aOverrideBBox)
  : mTargetFrame(aTargetFrame)
  , mTargetContent(aTargetContent)
  , mMetrics(aMetrics)
  , mPaintCallback(aPaintCallback)
  , mPaintTransform(aPaintTransform)
  , mInitialized(false)
{
  if (aOverrideBBox) {
    mTargetBBox = *aOverrideBBox;
  } else {
    MOZ_ASSERT(mTargetFrame, "Need to supply a frame when there's no aOverrideBBox");
    mTargetBBox = nsSVGUtils::GetBBox(mTargetFrame,
                                      nsSVGUtils::eUseFrameBoundsForOuterSVG |
                                      nsSVGUtils::eBBoxIncludeFillGeometry);
  }

  // Compute user space to filter space transforms.
  if (!ComputeUserSpaceToFilterSpaceScale()) {
    return;
  }

  if (!ComputeTargetBBoxInFilterSpace()) {
    return;
  }

  // Get various transforms:
  gfxMatrix filterToUserSpace(mFilterSpaceToUserSpaceScale.width, 0.0f,
                              0.0f, mFilterSpaceToUserSpaceScale.height,
                              0.0f, 0.0f);

  mFilterSpaceToFrameSpaceInCSSPxTransform =
    filterToUserSpace * GetUserSpaceToFrameSpaceInCSSPxTransform();
  // mFilterSpaceToFrameSpaceInCSSPxTransform is always invertible
  mFrameSpaceInCSSPxToFilterSpaceTransform =
    mFilterSpaceToFrameSpaceInCSSPxTransform;
  mFrameSpaceInCSSPxToFilterSpaceTransform.Invert();

  nsIntRect targetBounds;
  if (aPreFilterVisualOverflowRectOverride) {
    targetBounds =
      FrameSpaceToFilterSpace(aPreFilterVisualOverflowRectOverride);
  } else if (mTargetFrame) {
    nsRect preFilterVOR = mTargetFrame->GetPreEffectsVisualOverflowRect();
    targetBounds = FrameSpaceToFilterSpace(&preFilterVOR);
  }
  mTargetBounds.UnionRect(mTargetBBoxInFilterSpace, targetBounds);

  // Build the filter graph.
  if (NS_FAILED(BuildPrimitives(aFilterChain, aTargetFrame,
                                aFilterInputIsTainted))) {
    return;
  }

  // Convert the passed in rects from frame space to filter space:
  mPostFilterDirtyRegion = FrameSpaceToFilterSpace(aPostFilterDirtyRegion);
  mPreFilterDirtyRegion = FrameSpaceToFilterSpace(aPreFilterDirtyRegion);

  mInitialized = true;
}

bool
nsFilterInstance::ComputeTargetBBoxInFilterSpace()
{
  gfxRect targetBBoxInFilterSpace = UserSpaceToFilterSpace(mTargetBBox);
  targetBBoxInFilterSpace.RoundOut();

  return gfxUtils::GfxRectToIntRect(targetBBoxInFilterSpace,
                                    &mTargetBBoxInFilterSpace);
}

bool
nsFilterInstance::ComputeUserSpaceToFilterSpaceScale()
{
  if (mTargetFrame) {
    mUserSpaceToFilterSpaceScale = mPaintTransform.ScaleFactors(true);
    if (mUserSpaceToFilterSpaceScale.width <= 0.0f ||
        mUserSpaceToFilterSpaceScale.height <= 0.0f) {
      // Nothing should be rendered.
      return false;
    }
  } else {
    mUserSpaceToFilterSpaceScale = gfxSize(1.0, 1.0);
  }

  mFilterSpaceToUserSpaceScale =
    gfxSize(1.0f / mUserSpaceToFilterSpaceScale.width,
            1.0f / mUserSpaceToFilterSpaceScale.height);

  return true;
}

gfxRect
nsFilterInstance::UserSpaceToFilterSpace(const gfxRect& aUserSpaceRect) const
{
  gfxRect filterSpaceRect = aUserSpaceRect;
  filterSpaceRect.Scale(mUserSpaceToFilterSpaceScale.width,
                        mUserSpaceToFilterSpaceScale.height);
  return filterSpaceRect;
}

gfxRect
nsFilterInstance::FilterSpaceToUserSpace(const gfxRect& aFilterSpaceRect) const
{
  gfxRect userSpaceRect = aFilterSpaceRect;
  userSpaceRect.Scale(mFilterSpaceToUserSpaceScale.width,
                      mFilterSpaceToUserSpaceScale.height);
  return userSpaceRect;
}

nsresult
nsFilterInstance::BuildPrimitives(const nsTArray<nsStyleFilter>& aFilterChain,
                                  nsIFrame* aTargetFrame,
                                  bool aFilterInputIsTainted)
{
  NS_ASSERTION(!mPrimitiveDescriptions.Length(),
               "expected to start building primitives from scratch");

  for (uint32_t i = 0; i < aFilterChain.Length(); i++) {
    bool inputIsTainted =
      mPrimitiveDescriptions.IsEmpty() ? aFilterInputIsTainted :
        mPrimitiveDescriptions.LastElement().IsTainted();
    nsresult rv = BuildPrimitivesForFilter(aFilterChain[i], aTargetFrame, inputIsTainted);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  mFilterDescription = FilterDescription(mPrimitiveDescriptions);

  return NS_OK;
}

nsresult
nsFilterInstance::BuildPrimitivesForFilter(const nsStyleFilter& aFilter,
                                           nsIFrame* aTargetFrame,
                                           bool aInputIsTainted)
{
  NS_ASSERTION(mUserSpaceToFilterSpaceScale.width > 0.0f &&
               mFilterSpaceToUserSpaceScale.height > 0.0f,
               "scale factors between spaces should be positive values");

  if (aFilter.GetType() == NS_STYLE_FILTER_URL) {
    // Build primitives for an SVG filter.
    nsSVGFilterInstance svgFilterInstance(aFilter, aTargetFrame,
                                          mTargetContent,
                                          mMetrics, mTargetBBox,
                                          mUserSpaceToFilterSpaceScale);
    if (!svgFilterInstance.IsInitialized()) {
      return NS_ERROR_FAILURE;
    }

    return svgFilterInstance.BuildPrimitives(mPrimitiveDescriptions, mInputImages,
                                             aInputIsTainted);
  }

  // Build primitives for a CSS filter.

  // If we don't have a frame, use opaque black for shadows with unspecified
  // shadow colors.
  nscolor shadowFallbackColor =
    mTargetFrame ? mTargetFrame->StyleColor()->mColor : NS_RGB(0,0,0);

  nsCSSFilterInstance cssFilterInstance(aFilter, shadowFallbackColor,
                                        mTargetBounds,
                                        mFrameSpaceInCSSPxToFilterSpaceTransform);
  return cssFilterInstance.BuildPrimitives(mPrimitiveDescriptions, aInputIsTainted);
}

static void
UpdateNeededBounds(const nsIntRegion& aRegion, nsIntRect& aBounds)
{
  aBounds = aRegion.GetBounds();

  bool overflow;
  IntSize surfaceSize =
   nsSVGUtils::ConvertToSurfaceSize(SizeDouble(aBounds.Size()), &overflow);
  if (overflow) {
    aBounds.SizeTo(surfaceSize);
  }
}

void
nsFilterInstance::ComputeNeededBoxes()
{
  if (mPrimitiveDescriptions.IsEmpty())
    return;

  nsIntRegion sourceGraphicNeededRegion;
  nsIntRegion fillPaintNeededRegion;
  nsIntRegion strokePaintNeededRegion;

  FilterSupport::ComputeSourceNeededRegions(
    mFilterDescription, mPostFilterDirtyRegion,
    sourceGraphicNeededRegion, fillPaintNeededRegion, strokePaintNeededRegion);

  sourceGraphicNeededRegion.And(sourceGraphicNeededRegion, mTargetBounds);

  UpdateNeededBounds(sourceGraphicNeededRegion, mSourceGraphic.mNeededBounds);
  UpdateNeededBounds(fillPaintNeededRegion, mFillPaint.mNeededBounds);
  UpdateNeededBounds(strokePaintNeededRegion, mStrokePaint.mNeededBounds);
}

void
nsFilterInstance::BuildSourcePaint(SourceInfo *aSource,
                                   imgDrawingParams& aImgParams)
{
  MOZ_ASSERT(mTargetFrame);
  nsIntRect neededRect = aSource->mNeededBounds;
  if (neededRect.IsEmpty()) {
    return;
  }

  RefPtr<DrawTarget> offscreenDT =
    gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
      neededRect.Size(), SurfaceFormat::B8G8R8A8);
  if (!offscreenDT || !offscreenDT->IsValid()) {
    return;
  }

  RefPtr<gfxContext> ctx = gfxContext::CreateOrNull(offscreenDT);
  MOZ_ASSERT(ctx); // already checked the draw target above
  gfxContextAutoSaveRestore saver(ctx);

  ctx->SetMatrixDouble(mPaintTransform *
                       gfxMatrix::Translation(-neededRect.TopLeft()));
  GeneralPattern pattern;
  if (aSource == &mFillPaint) {
    nsSVGUtils::MakeFillPatternFor(mTargetFrame, ctx, &pattern, aImgParams);
  } else if (aSource == &mStrokePaint) {
    nsSVGUtils::MakeStrokePatternFor(mTargetFrame, ctx, &pattern, aImgParams);
  }

  if (pattern.GetPattern()) {
    offscreenDT->FillRect(ToRect(FilterSpaceToUserSpace(ThebesRect(neededRect))),
                          pattern);
  }

  aSource->mSourceSurface = offscreenDT->Snapshot();
  aSource->mSurfaceRect = neededRect;
}

void
nsFilterInstance::BuildSourcePaints(imgDrawingParams& aImgParams)
{
  if (!mFillPaint.mNeededBounds.IsEmpty()) {
    BuildSourcePaint(&mFillPaint, aImgParams);
  }

  if (!mStrokePaint.mNeededBounds.IsEmpty()) {
    BuildSourcePaint(&mStrokePaint, aImgParams);
  }
}

void
nsFilterInstance::BuildSourceImage(DrawTarget *aDest, imgDrawingParams& aImgParams)
{
  MOZ_ASSERT(mTargetFrame);

  nsIntRect neededRect = mSourceGraphic.mNeededBounds;
  if (neededRect.IsEmpty()) {
    return;
  }

  RefPtr<DrawTarget> offscreenDT =
    aDest->CreateSimilarDrawTarget(neededRect.Size(), SurfaceFormat::B8G8R8A8);
  if (!offscreenDT || !offscreenDT->IsValid()) {
    return;
  }

  gfxRect r = FilterSpaceToUserSpace(ThebesRect(neededRect));
  r.RoundOut();
  nsIntRect dirty;
  if (!gfxUtils::GfxRectToIntRect(r, &dirty)){
    return;
  }

  // SVG graphics paint to device space, so we need to set an initial device
  // space to filter space transform on the gfxContext that SourceGraphic
  // and SourceAlpha will paint to.
  //
  // (In theory it would be better to minimize error by having filtered SVG
  // graphics temporarily paint to user space when painting the sources and
  // only set a user space to filter space transform on the gfxContext
  // (since that would eliminate the transform multiplications from user
  // space to device space and back again). However, that would make the
  // code more complex while being hard to get right without introducing
  // subtle bugs, and in practice it probably makes no real difference.)
  RefPtr<gfxContext> ctx = gfxContext::CreateOrNull(offscreenDT);
  MOZ_ASSERT(ctx); // already checked the draw target above
  gfxMatrix devPxToCssPxTM = nsSVGUtils::GetCSSPxToDevPxMatrix(mTargetFrame);
  DebugOnly<bool> invertible = devPxToCssPxTM.Invert();
  MOZ_ASSERT(invertible);
  ctx->SetMatrixDouble(devPxToCssPxTM * mPaintTransform *
                       gfxMatrix::Translation(-neededRect.TopLeft()));

  mPaintCallback->Paint(*ctx, mTargetFrame, mPaintTransform, &dirty, aImgParams);

  mSourceGraphic.mSourceSurface = offscreenDT->Snapshot();
  mSourceGraphic.mSurfaceRect = neededRect;
}

void
nsFilterInstance::Render(gfxContext* aCtx, imgDrawingParams& aImgParams)
{
  MOZ_ASSERT(mTargetFrame, "Need a frame for rendering");

  if (mPrimitiveDescriptions.IsEmpty()) {
    // An filter without any primitive. Treat it as success and paint nothing.
    return;
  }

  nsIntRect filterRect =
    mPostFilterDirtyRegion.GetBounds().Intersect(OutputFilterSpaceBounds());
  if (filterRect.IsEmpty() || mPaintTransform.IsSingular()) {
    return;
  }

  gfxContextMatrixAutoSaveRestore autoSR(aCtx);
  aCtx->SetMatrix(aCtx->CurrentMatrix().PreTranslate(filterRect.x, filterRect.y));

  ComputeNeededBoxes();

  BuildSourceImage(aCtx->GetDrawTarget(), aImgParams);
  BuildSourcePaints(aImgParams);

  FilterSupport::RenderFilterDescription(
    aCtx->GetDrawTarget(), mFilterDescription, IntRectToRect(filterRect),
    mSourceGraphic.mSourceSurface, mSourceGraphic.mSurfaceRect,
    mFillPaint.mSourceSurface, mFillPaint.mSurfaceRect,
    mStrokePaint.mSourceSurface, mStrokePaint.mSurfaceRect,
    mInputImages, Point(0, 0));
}

nsRegion
nsFilterInstance::ComputePostFilterDirtyRegion()
{
  if (mPreFilterDirtyRegion.IsEmpty() || mPrimitiveDescriptions.IsEmpty()) {
    return nsRegion();
  }

  nsIntRegion resultChangeRegion =
    FilterSupport::ComputeResultChangeRegion(mFilterDescription,
      mPreFilterDirtyRegion, nsIntRegion(), nsIntRegion());
  return FilterSpaceToFrameSpace(resultChangeRegion);
}

nsRect
nsFilterInstance::ComputePostFilterExtents()
{
  if (mPrimitiveDescriptions.IsEmpty()) {
    return nsRect();
  }

  nsIntRegion postFilterExtents =
    FilterSupport::ComputePostFilterExtents(mFilterDescription, mTargetBounds);
  return FilterSpaceToFrameSpace(postFilterExtents.GetBounds());
}

nsRect
nsFilterInstance::ComputeSourceNeededRect()
{
  ComputeNeededBoxes();
  return FilterSpaceToFrameSpace(mSourceGraphic.mNeededBounds);
}

nsIntRect
nsFilterInstance::OutputFilterSpaceBounds() const
{
  uint32_t numPrimitives = mPrimitiveDescriptions.Length();
  if (numPrimitives <= 0)
    return nsIntRect();

  return mPrimitiveDescriptions[numPrimitives - 1].PrimitiveSubregion();
}

nsIntRect
nsFilterInstance::FrameSpaceToFilterSpace(const nsRect* aRect) const
{
  nsIntRect rect = OutputFilterSpaceBounds();
  if (aRect) {
    if (aRect->IsEmpty()) {
      return nsIntRect();
    }
    gfxRect rectInCSSPx =
      nsLayoutUtils::RectToGfxRect(*aRect, nsPresContext::AppUnitsPerCSSPixel());
    gfxRect rectInFilterSpace =
      mFrameSpaceInCSSPxToFilterSpaceTransform.TransformBounds(rectInCSSPx);
    rectInFilterSpace.RoundOut();
    nsIntRect intRect;
    if (gfxUtils::GfxRectToIntRect(rectInFilterSpace, &intRect)) {
      rect = intRect;
    }
  }
  return rect;
}

nsRect
nsFilterInstance::FilterSpaceToFrameSpace(const nsIntRect& aRect) const
{
  if (aRect.IsEmpty()) {
    return nsRect();
  }
  gfxRect r(aRect.x, aRect.y, aRect.width, aRect.height);
  r = mFilterSpaceToFrameSpaceInCSSPxTransform.TransformBounds(r);
  // nsLayoutUtils::RoundGfxRectToAppRect rounds out.
  return nsLayoutUtils::RoundGfxRectToAppRect(r, nsPresContext::AppUnitsPerCSSPixel());
}

nsIntRegion
nsFilterInstance::FrameSpaceToFilterSpace(const nsRegion* aRegion) const
{
  if (!aRegion) {
    return OutputFilterSpaceBounds();
  }
  nsIntRegion result;
  for (auto iter = aRegion->RectIter(); !iter.Done(); iter.Next()) {
    // FrameSpaceToFilterSpace rounds out, so this works.
    nsRect rect = iter.Get();
    result.Or(result, FrameSpaceToFilterSpace(&rect));
  }
  return result;
}

nsRegion
nsFilterInstance::FilterSpaceToFrameSpace(const nsIntRegion& aRegion) const
{
  nsRegion result;
  for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
    // FilterSpaceToFrameSpace rounds out, so this works.
    result.Or(result, FilterSpaceToFrameSpace(iter.Get()));
  }
  return result;
}

gfxMatrix
nsFilterInstance::GetUserSpaceToFrameSpaceInCSSPxTransform() const
{
  if (!mTargetFrame) {
    return gfxMatrix();
  }
  return gfxMatrix::Translation(-nsSVGUtils::FrameSpaceInCSSPxToUserSpaceOffset(mTargetFrame));
}
