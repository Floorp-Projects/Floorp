/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsFilterInstance.h"

// Keep others in (case-insensitive) order:
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "nsISVGChildFrame.h"
#include "nsRenderingContext.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGFilterPaintCallback.h"
#include "nsSVGUtils.h"
#include "SVGContentUtils.h"
#include "FilterSupport.h"
#include "gfx2DGlue.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;

nsresult
nsFilterInstance::PaintFilteredFrame(nsRenderingContext *aContext,
                                     nsIFrame *aFilteredFrame,
                                     nsSVGFilterPaintCallback *aPaintCallback,
                                     const nsRegion *aDirtyArea,
                                     nsIFrame* aTransformRoot)
{
  nsFilterInstance instance(aFilteredFrame, aPaintCallback, aDirtyArea,
                            nullptr, nullptr, nullptr,
                            aTransformRoot);
  if (!instance.IsInitialized()) {
    return NS_OK;
  }
  return instance.Render(aContext->ThebesContext());
}

nsRegion
nsFilterInstance::GetPostFilterDirtyArea(nsIFrame *aFilteredFrame,
                                         const nsRegion& aPreFilterDirtyRegion)
{
  if (aPreFilterDirtyRegion.IsEmpty()) {
    return nsRegion();
  }

  nsFilterInstance instance(aFilteredFrame, nullptr, nullptr,
                            &aPreFilterDirtyRegion);
  if (!instance.IsInitialized()) {
    return nsRegion();
  }
  // We've passed in the source's dirty area so the instance knows about it.
  // Now we can ask the instance to compute the area of the filter output
  // that's dirty.
  nsRegion dirtyRegion;
  nsresult rv = instance.ComputePostFilterDirtyRegion(&dirtyRegion);
  if (NS_SUCCEEDED(rv)) {
    return dirtyRegion;
  }
  return nsRegion();
}

nsRegion
nsFilterInstance::GetPreFilterNeededArea(nsIFrame *aFilteredFrame,
                                         const nsRegion& aPostFilterDirtyRegion)
{
  nsFilterInstance instance(aFilteredFrame, nullptr, &aPostFilterDirtyRegion);
  if (!instance.IsInitialized()) {
    return nsRect();
  }
  // Now we can ask the instance to compute the area of the source
  // that's needed.
  nsRect neededRect;
  nsresult rv = instance.ComputeSourceNeededRect(&neededRect);
  if (NS_SUCCEEDED(rv)) {
    return neededRect;
  }
  return nsRegion();
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
  nsFilterInstance instance(aFilteredFrame, nullptr, nullptr,
                            preFilterRegionPtr, aPreFilterBounds,
                            aOverrideBBox);
  if (!instance.IsInitialized()) {
    return nsRect();
  }
  nsRect bbox;
  nsresult rv = instance.ComputePostFilterExtents(&bbox);
  if (NS_SUCCEEDED(rv)) {
    return bbox;
  }
  return nsRect();
}

nsFilterInstance::nsFilterInstance(nsIFrame *aTargetFrame,
                                   nsSVGFilterPaintCallback *aPaintCallback,
                                   const nsRegion *aPostFilterDirtyRegion,
                                   const nsRegion *aPreFilterDirtyRegion,
                                   const nsRect *aPreFilterVisualOverflowRectOverride,
                                   const gfxRect *aOverrideBBox,
                                   nsIFrame* aTransformRoot) :
  mTargetFrame(aTargetFrame),
  mPaintCallback(aPaintCallback),
  mTransformRoot(aTransformRoot),
  mInitialized(false) {

  mTargetBBox = aOverrideBBox ?
    *aOverrideBBox : nsSVGUtils::GetBBox(mTargetFrame);

  nsresult rv = ComputeUserSpaceToFilterSpaceScale();
  if (NS_FAILED(rv)) {
    return;
  }

  rv = BuildPrimitives();
  if (NS_FAILED(rv)) {
    return;
  }

  if (mPrimitiveDescriptions.IsEmpty()) {
    // Nothing should be rendered.
    return;
  }

  // Get various transforms:

  gfxMatrix filterToUserSpace(mFilterSpaceToUserSpaceScale.width, 0.0f,
                              0.0f, mFilterSpaceToUserSpaceScale.height,
                              0.0f, 0.0f);

  // Only used (so only set) when we paint:
  if (mPaintCallback) {
    mFilterSpaceToDeviceSpaceTransform = filterToUserSpace *
              nsSVGUtils::GetCanvasTM(mTargetFrame, nsISVGChildFrame::FOR_PAINTING);
  }

  // Convert the passed in rects from frame to filter space:

  mAppUnitsPerCSSPx = mTargetFrame->PresContext()->AppUnitsPerCSSPixel();

  mFilterSpaceToFrameSpaceInCSSPxTransform =
    filterToUserSpace * GetUserSpaceToFrameSpaceInCSSPxTransform();
  // mFilterSpaceToFrameSpaceInCSSPxTransform is always invertible
  mFrameSpaceInCSSPxToFilterSpaceTransform =
    mFilterSpaceToFrameSpaceInCSSPxTransform;
  mFrameSpaceInCSSPxToFilterSpaceTransform.Invert();

  mPostFilterDirtyRegion = FrameSpaceToFilterSpace(aPostFilterDirtyRegion);
  mPreFilterDirtyRegion = FrameSpaceToFilterSpace(aPreFilterDirtyRegion);
  if (aPreFilterVisualOverflowRectOverride) {
    mTargetBounds =
      FrameSpaceToFilterSpace(aPreFilterVisualOverflowRectOverride);
  } else {
    nsRect preFilterVOR = mTargetFrame->GetPreEffectsVisualOverflowRect();
    mTargetBounds = FrameSpaceToFilterSpace(&preFilterVOR);
  }

  mInitialized = true;
}

nsresult
nsFilterInstance::ComputeUserSpaceToFilterSpaceScale()
{
  gfxMatrix canvasTransform =
    nsSVGUtils::GetCanvasTM(mTargetFrame, nsISVGChildFrame::FOR_OUTERSVG_TM);
  if (canvasTransform.IsSingular()) {
    // Nothing should be rendered.
    return NS_ERROR_FAILURE;
  }

  mUserSpaceToFilterSpaceScale = canvasTransform.ScaleFactors(true);
  if (mUserSpaceToFilterSpaceScale.width <= 0.0f ||
      mUserSpaceToFilterSpaceScale.height <= 0.0f) {
    // Nothing should be rendered.
    return NS_ERROR_FAILURE;
  }

  mFilterSpaceToUserSpaceScale = gfxSize(1.0f / mUserSpaceToFilterSpaceScale.width,
                                         1.0f / mUserSpaceToFilterSpaceScale.height);
  return NS_OK;
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
nsFilterInstance::BuildPrimitives()
{
  NS_ASSERTION(!mPrimitiveDescriptions.Length(),
               "expected to start building primitives from scratch");

  const nsTArray<nsStyleFilter>& filters = mTargetFrame->StyleSVGReset()->mFilters;
  for (uint32_t i = 0; i < filters.Length(); i++) {
    nsresult rv = BuildPrimitivesForFilter(filters[i]);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
nsFilterInstance::BuildPrimitivesForFilter(const nsStyleFilter& aFilter)
{
  NS_ASSERTION(mUserSpaceToFilterSpaceScale.width > 0.0f &&
               mFilterSpaceToUserSpaceScale.height > 0.0f,
               "scale factors between spaces should be positive values");

  if (aFilter.GetType() == NS_STYLE_FILTER_URL) {
    // Build primitives for an SVG filter.
    nsSVGFilterInstance svgFilterInstance(aFilter, mTargetFrame, mTargetBBox,
                                          mUserSpaceToFilterSpaceScale,
                                          mFilterSpaceToUserSpaceScale);
    if (!svgFilterInstance.IsInitialized()) {
      return NS_ERROR_FAILURE;
    }

    return svgFilterInstance.BuildPrimitives(mPrimitiveDescriptions, mInputImages);
  }

  // Eventually, we will build primitives for CSS filters, too.
  return NS_ERROR_FAILURE;
}

void
nsFilterInstance::ComputeNeededBoxes()
{
  if (mPrimitiveDescriptions.IsEmpty())
    return;

  nsIntRegion sourceGraphicNeededRegion;
  nsIntRegion fillPaintNeededRegion;
  nsIntRegion strokePaintNeededRegion;

  FilterDescription filter(mPrimitiveDescriptions);
  FilterSupport::ComputeSourceNeededRegions(
    filter, mPostFilterDirtyRegion,
    sourceGraphicNeededRegion, fillPaintNeededRegion, strokePaintNeededRegion);

  nsIntRect sourceBoundsInt;
  gfxRect sourceBounds = UserSpaceToFilterSpace(mTargetBBox);
  sourceBounds.RoundOut();
  // Detect possible float->int overflow
  if (!gfxUtils::GfxRectToIntRect(sourceBounds, &sourceBoundsInt))
    return;
  sourceBoundsInt.UnionRect(sourceBoundsInt, mTargetBounds);

  sourceGraphicNeededRegion.And(sourceGraphicNeededRegion, sourceBoundsInt);

  mSourceGraphic.mNeededBounds = sourceGraphicNeededRegion.GetBounds();
  mFillPaint.mNeededBounds = fillPaintNeededRegion.GetBounds();
  mStrokePaint.mNeededBounds = strokePaintNeededRegion.GetBounds();
}

nsresult
nsFilterInstance::BuildSourcePaint(SourceInfo *aSource,
                                   DrawTarget* aTargetDT)
{
  nsIntRect neededRect = aSource->mNeededBounds;

  RefPtr<DrawTarget> offscreenDT =
    gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
      ToIntSize(neededRect.Size()), SurfaceFormat::B8G8R8A8);
  if (!offscreenDT) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsRefPtr<gfxContext> ctx = new gfxContext(offscreenDT);
  ctx->Translate(-neededRect.TopLeft());

  nsRefPtr<nsRenderingContext> tmpCtx(new nsRenderingContext());
  tmpCtx->Init(mTargetFrame->PresContext()->DeviceContext(), ctx);

  gfxMatrix deviceToFilterSpace = GetFilterSpaceToDeviceSpaceTransform();
  if (!deviceToFilterSpace.Invert()) {
    return NS_ERROR_FAILURE;
  }
  gfxContext *gfx = tmpCtx->ThebesContext();
  gfx->Multiply(deviceToFilterSpace);

  gfx->Save();

  gfxMatrix matrix =
    nsSVGUtils::GetCanvasTM(mTargetFrame, nsISVGChildFrame::FOR_PAINTING,
                            mTransformRoot);
  if (!matrix.IsSingular()) {
    gfx->Multiply(matrix);
    gfx->Rectangle(FilterSpaceToUserSpace(neededRect));
    if ((aSource == &mFillPaint &&
         nsSVGUtils::SetupCairoFillPaint(mTargetFrame, gfx)) ||
        (aSource == &mStrokePaint &&
         nsSVGUtils::SetupCairoStrokePaint(mTargetFrame, gfx))) {
      gfx->Fill();
    }
  }
  gfx->Restore();


  aSource->mSourceSurface = offscreenDT->Snapshot();
  aSource->mSurfaceRect = ToIntRect(neededRect);

  return NS_OK;
}

nsresult
nsFilterInstance::BuildSourcePaints(DrawTarget* aTargetDT)
{
  nsresult rv = NS_OK;

  if (!mFillPaint.mNeededBounds.IsEmpty()) {
    rv = BuildSourcePaint(&mFillPaint, aTargetDT);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!mStrokePaint.mNeededBounds.IsEmpty()) {
    rv = BuildSourcePaint(&mStrokePaint, aTargetDT);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return  rv;
}

nsresult
nsFilterInstance::BuildSourceImage(DrawTarget* aTargetDT)
{
  nsIntRect neededRect = mSourceGraphic.mNeededBounds;
  if (neededRect.IsEmpty()) {
    return NS_OK;
  }

  RefPtr<DrawTarget> offscreenDT =
    gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
      ToIntSize(neededRect.Size()), SurfaceFormat::B8G8R8A8);
  if (!offscreenDT) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsRefPtr<gfxContext> ctx = new gfxContext(offscreenDT);
  ctx->Translate(-neededRect.TopLeft());

  nsRefPtr<nsRenderingContext> tmpCtx(new nsRenderingContext());
  tmpCtx->Init(mTargetFrame->PresContext()->DeviceContext(), ctx);

  gfxRect r = FilterSpaceToUserSpace(neededRect);
  r.RoundOut();
  nsIntRect dirty;
  if (!gfxUtils::GfxRectToIntRect(r, &dirty))
    return NS_ERROR_FAILURE;

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
  gfxMatrix deviceToFilterSpace = GetFilterSpaceToDeviceSpaceTransform();
  if (!deviceToFilterSpace.Invert()) {
    return NS_ERROR_FAILURE;
  }
  tmpCtx->ThebesContext()->Multiply(deviceToFilterSpace);
  mPaintCallback->Paint(tmpCtx, mTargetFrame, &dirty, mTransformRoot);

  mSourceGraphic.mSourceSurface = offscreenDT->Snapshot();
  mSourceGraphic.mSurfaceRect = ToIntRect(neededRect);

  return NS_OK;
}

nsresult
nsFilterInstance::Render(gfxContext* aContext)
{
  nsIntRect filterRect = mPostFilterDirtyRegion.GetBounds().Intersect(OutputFilterSpaceBounds());
  gfxMatrix ctm = GetFilterSpaceToDeviceSpaceTransform();

  if (filterRect.IsEmpty() || ctm.IsSingular()) {
    return NS_OK;
  }

  Matrix oldDTMatrix;
  RefPtr<DrawTarget> dt = aContext->GetDrawTarget();
  oldDTMatrix = dt->GetTransform();
  Matrix matrix = ToMatrix(ctm);
  matrix.Translate(filterRect.x, filterRect.y);
  dt->SetTransform(matrix * oldDTMatrix);

  ComputeNeededBoxes();

  nsresult rv = BuildSourceImage(dt);
  if (NS_FAILED(rv))
    return rv;
  rv = BuildSourcePaints(dt);
  if (NS_FAILED(rv))
    return rv;

  FilterDescription filter(mPrimitiveDescriptions);
  FilterSupport::RenderFilterDescription(
    dt, filter, ToRect(filterRect),
    mSourceGraphic.mSourceSurface, mSourceGraphic.mSurfaceRect,
    mFillPaint.mSourceSurface, mFillPaint.mSurfaceRect,
    mStrokePaint.mSourceSurface, mStrokePaint.mSurfaceRect,
    mInputImages);

  dt->SetTransform(oldDTMatrix);

  return NS_OK;
}

nsresult
nsFilterInstance::ComputePostFilterDirtyRegion(nsRegion* aPostFilterDirtyRegion)
{
  *aPostFilterDirtyRegion = nsRegion();
  if (mPreFilterDirtyRegion.IsEmpty()) {
    return NS_OK;
  }

  FilterDescription filter(mPrimitiveDescriptions);
  nsIntRegion resultChangeRegion =
    FilterSupport::ComputeResultChangeRegion(filter,
      mPreFilterDirtyRegion, nsIntRegion(), nsIntRegion());
  *aPostFilterDirtyRegion =
    FilterSpaceToFrameSpace(resultChangeRegion);
  return NS_OK;
}

nsresult
nsFilterInstance::ComputePostFilterExtents(nsRect* aPostFilterExtents)
{
  *aPostFilterExtents = nsRect();

  nsIntRect sourceBoundsInt;
  gfxRect sourceBounds = UserSpaceToFilterSpace(mTargetBBox);
  sourceBounds.RoundOut();
  // Detect possible float->int overflow
  if (!gfxUtils::GfxRectToIntRect(sourceBounds, &sourceBoundsInt))
    return NS_ERROR_FAILURE;
  sourceBoundsInt.UnionRect(sourceBoundsInt, mTargetBounds);

  FilterDescription filter(mPrimitiveDescriptions);
  nsIntRegion postFilterExtents =
    FilterSupport::ComputePostFilterExtents(filter, sourceBoundsInt);
  *aPostFilterExtents = FilterSpaceToFrameSpace(postFilterExtents.GetBounds());
  return NS_OK;
}

nsresult
nsFilterInstance::ComputeSourceNeededRect(nsRect* aDirty)
{
  ComputeNeededBoxes();
  *aDirty = FilterSpaceToFrameSpace(mSourceGraphic.mNeededBounds);

  return NS_OK;
}

nsIntRect
nsFilterInstance::OutputFilterSpaceBounds() const
{
  uint32_t numPrimitives = mPrimitiveDescriptions.Length();
  if (numPrimitives <= 0)
    return nsIntRect();

  nsIntRect bounds =
    ThebesIntRect(mPrimitiveDescriptions[numPrimitives - 1].PrimitiveSubregion());
  bool overflow;
  gfxIntSize surfaceSize =
    nsSVGUtils::ConvertToSurfaceSize(bounds.Size(), &overflow);
  bounds.SizeTo(surfaceSize);
  return bounds;
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
      nsLayoutUtils::RectToGfxRect(*aRect, mAppUnitsPerCSSPx);
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
  return nsLayoutUtils::RoundGfxRectToAppRect(r, mAppUnitsPerCSSPx);
}

nsIntRegion
nsFilterInstance::FrameSpaceToFilterSpace(const nsRegion* aRegion) const
{
  if (!aRegion) {
    return OutputFilterSpaceBounds();
  }
  nsIntRegion result;
  nsRegionRectIterator it(*aRegion);
  while (const nsRect* r = it.Next()) {
    // FrameSpaceToFilterSpace rounds out, so this works.
    result.Or(result, FrameSpaceToFilterSpace(r));
  }
  return result;
}

nsRegion
nsFilterInstance::FilterSpaceToFrameSpace(const nsIntRegion& aRegion) const
{
  nsRegion result;
  nsIntRegionRectIterator it(aRegion);
  while (const nsIntRect* r = it.Next()) {
    // FilterSpaceToFrameSpace rounds out, so this works.
    result.Or(result, FilterSpaceToFrameSpace(*r));
  }
  return result;
}

gfxMatrix
nsFilterInstance::GetUserSpaceToFrameSpaceInCSSPxTransform() const
{
  return gfxMatrix().Translate(-nsSVGUtils::FrameSpaceInCSSPxToUserSpaceOffset(mTargetFrame));
}
