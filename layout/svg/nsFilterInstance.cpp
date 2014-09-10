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

nsresult
nsFilterInstance::PaintFilteredFrame(nsIFrame *aFilteredFrame,
                                     nsRenderingContext *aContext,
                                     const gfxMatrix& aTransform,
                                     nsSVGFilterPaintCallback *aPaintCallback,
                                     const nsRegion *aDirtyArea)
{
  nsFilterInstance instance(aFilteredFrame, aPaintCallback, aTransform,
                            aDirtyArea, nullptr, nullptr, nullptr);
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

  gfxMatrix unused; // aPaintTransform arg not used since we're not painting
  nsFilterInstance instance(aFilteredFrame, nullptr, unused, nullptr,
                            &aPreFilterDirtyRegion);
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
  gfxMatrix unused; // aPaintTransform arg not used since we're not painting
  nsFilterInstance instance(aFilteredFrame, nullptr, unused,
                            &aPostFilterDirtyRegion);
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

  gfxMatrix unused; // aPaintTransform arg not used since we're not painting
  nsFilterInstance instance(aFilteredFrame, nullptr, unused, nullptr,
                            preFilterRegionPtr, aPreFilterBounds,
                            aOverrideBBox);
  if (!instance.IsInitialized()) {
    return nsRect();
  }

  return instance.ComputePostFilterExtents();
}

nsFilterInstance::nsFilterInstance(nsIFrame *aTargetFrame,
                                   nsSVGFilterPaintCallback *aPaintCallback,
                                   const gfxMatrix& aPaintTransform,
                                   const nsRegion *aPostFilterDirtyRegion,
                                   const nsRegion *aPreFilterDirtyRegion,
                                   const nsRect *aPreFilterVisualOverflowRectOverride,
                                   const gfxRect *aOverrideBBox)
  : mTargetFrame(aTargetFrame)
  , mPaintCallback(aPaintCallback)
  , mPaintTransform(aPaintTransform)
  , mInitialized(false)
{

  mTargetBBox = aOverrideBBox ?
    *aOverrideBBox : nsSVGUtils::GetBBox(mTargetFrame);

  // Compute user space to filter space transforms.
  nsresult rv = ComputeUserSpaceToFilterSpaceScale();
  if (NS_FAILED(rv)) {
    return;
  }

  gfxRect targetBBoxInFilterSpace = UserSpaceToFilterSpace(mTargetBBox);
  targetBBoxInFilterSpace.RoundOut();
  if (!gfxUtils::GfxRectToIntRect(targetBBoxInFilterSpace, &mTargetBBoxInFilterSpace)) {
    // The target's bbox is way too big if there is float->int overflow.
    return;
  }

  // Get various transforms:

  gfxMatrix filterToUserSpace(mFilterSpaceToUserSpaceScale.width, 0.0f,
                              0.0f, mFilterSpaceToUserSpaceScale.height,
                              0.0f, 0.0f);

  // Only used (so only set) when we paint:
  if (mPaintCallback) {
    mFilterSpaceToDeviceSpaceTransform = filterToUserSpace * mPaintTransform;
  }

  mFilterSpaceToFrameSpaceInCSSPxTransform =
    filterToUserSpace * GetUserSpaceToFrameSpaceInCSSPxTransform();
  // mFilterSpaceToFrameSpaceInCSSPxTransform is always invertible
  mFrameSpaceInCSSPxToFilterSpaceTransform =
    mFilterSpaceToFrameSpaceInCSSPxTransform;
  mFrameSpaceInCSSPxToFilterSpaceTransform.Invert();

  // Build the filter graph.
  rv = BuildPrimitives();
  if (NS_FAILED(rv)) {
    return;
  }

  if (mPrimitiveDescriptions.IsEmpty()) {
    // Nothing should be rendered.
    return;
  }

  // Convert the passed in rects from frame space to filter space:
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
  gfxMatrix canvasTransform = nsSVGUtils::GetCanvasTM(mTargetFrame);
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

  mFilterDescription = FilterDescription(mPrimitiveDescriptions);

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

  // Build primitives for a CSS filter.
  nsCSSFilterInstance cssFilterInstance(aFilter, mTargetFrame,
                                        mTargetBBoxInFilterSpace,
                                        mFrameSpaceInCSSPxToFilterSpaceTransform);
  return cssFilterInstance.BuildPrimitives(mPrimitiveDescriptions);
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

  nsIntRect sourceBounds;
  sourceBounds.UnionRect(mTargetBBoxInFilterSpace, mTargetBounds);

  sourceGraphicNeededRegion.And(sourceGraphicNeededRegion, sourceBounds);

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

  if (!mPaintTransform.IsSingular()) {
    gfx->Multiply(mPaintTransform);
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
  mPaintCallback->Paint(tmpCtx, mTargetFrame, mPaintTransform, &dirty);

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

  FilterSupport::RenderFilterDescription(
    dt, mFilterDescription, ToRect(filterRect),
    mSourceGraphic.mSourceSurface, mSourceGraphic.mSurfaceRect,
    mFillPaint.mSourceSurface, mFillPaint.mSurfaceRect,
    mStrokePaint.mSourceSurface, mStrokePaint.mSurfaceRect,
    mInputImages);

  dt->SetTransform(oldDTMatrix);

  return NS_OK;
}

nsRegion
nsFilterInstance::ComputePostFilterDirtyRegion()
{
  if (mPreFilterDirtyRegion.IsEmpty()) {
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
  nsIntRect sourceBounds;
  sourceBounds.UnionRect(mTargetBBoxInFilterSpace, mTargetBounds);

  nsIntRegion postFilterExtents =
    FilterSupport::ComputePostFilterExtents(mFilterDescription, sourceBounds);
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
  return gfxMatrix::Translation(-nsSVGUtils::FrameSpaceInCSSPxToUserSpaceOffset(mTargetFrame));
}
