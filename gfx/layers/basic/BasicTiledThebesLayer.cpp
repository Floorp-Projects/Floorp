/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/PLayersChild.h"
#include "BasicTiledThebesLayer.h"
#include "gfxImageSurface.h"
#include "sampler.h"
#include "gfxPlatform.h"

#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
#include "cairo.h"
#include <sstream>
using mozilla::layers::Layer;
static void DrawDebugOverlay(gfxImageSurface* imgSurf, int x, int y)
{
  gfxContext c(imgSurf);

  // Draw border
  c.NewPath();
  c.SetDeviceColor(gfxRGBA(0.0, 0.0, 0.0, 1.0));
  c.Rectangle(gfxRect(gfxPoint(0,0),imgSurf->GetSize()));
  c.Stroke();

  // Build tile description
  std::stringstream ss;
  ss << x << ", " << y;

  // Draw text using cairo toy text API
  cairo_t* cr = c.GetCairo();
  cairo_set_font_size(cr, 25);
  cairo_text_extents_t extents;
  cairo_text_extents(cr, ss.str().c_str(), &extents);

  int textWidth = extents.width + 6;

  c.NewPath();
  c.SetDeviceColor(gfxRGBA(0.0, 0.0, 0.0, 1.0));
  c.Rectangle(gfxRect(gfxPoint(2,2),gfxSize(textWidth, 30)));
  c.Fill();

  c.NewPath();
  c.SetDeviceColor(gfxRGBA(1.0, 0.0, 0.0, 1.0));
  c.Rectangle(gfxRect(gfxPoint(2,2),gfxSize(textWidth, 30)));
  c.Stroke();

  c.NewPath();
  cairo_move_to(cr, 4, 28);
  cairo_show_text(cr, ss.str().c_str());

}

#endif

namespace mozilla {
namespace layers {

bool
BasicTiledLayerBuffer::HasFormatChanged(BasicTiledThebesLayer* aThebesLayer) const
{
  return aThebesLayer->CanUseOpaqueSurface() != mLastPaintOpaque;
}


gfxASurface::gfxImageFormat
BasicTiledLayerBuffer::GetFormat() const
{
  if (mThebesLayer->CanUseOpaqueSurface()) {
    return gfxASurface::ImageFormatRGB16_565;
  } else {
    return gfxASurface::ImageFormatARGB32;
  }
}

void
BasicTiledLayerBuffer::PaintThebes(BasicTiledThebesLayer* aLayer,
                                   const nsIntRegion& aNewValidRegion,
                                   const nsIntRegion& aPaintRegion,
                                   LayerManager::DrawThebesLayerCallback aCallback,
                                   void* aCallbackData)
{
  mThebesLayer = aLayer;
  mCallback = aCallback;
  mCallbackData = aCallbackData;

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  long start = PR_IntervalNow();
#endif

  // If this region is empty XMost() - 1 will give us a negative value.
  NS_ASSERTION(!aPaintRegion.GetBounds().IsEmpty(), "Empty paint region\n");

  bool useSinglePaintBuffer = UseSinglePaintBuffer();
  if (useSinglePaintBuffer) {
    // Check if the paint only spans a single tile. If that's
    // the case there's no point in using a single paint buffer.
    nsIntRect paintBounds = aPaintRegion.GetBounds();
    useSinglePaintBuffer = GetTileStart(paintBounds.x) !=
                           GetTileStart(paintBounds.XMost() - 1) ||
                           GetTileStart(paintBounds.y) !=
                           GetTileStart(paintBounds.YMost() - 1);
  }

  if (useSinglePaintBuffer) {
    const nsIntRect bounds = aPaintRegion.GetBounds();
    {
      SAMPLE_LABEL("BasicTiledLayerBuffer", "PaintThebesSingleBufferAlloc");
      mSinglePaintBuffer = new gfxImageSurface(
        gfxIntSize(ceilf(bounds.width * mResolution),
                   ceilf(bounds.height * mResolution)),
        GetFormat(), !aLayer->CanUseOpaqueSurface());
      mSinglePaintBufferOffset = nsIntPoint(bounds.x, bounds.y);
    }
    nsRefPtr<gfxContext> ctxt = new gfxContext(mSinglePaintBuffer);
    ctxt->NewPath();
    ctxt->Scale(mResolution, mResolution);
    ctxt->Translate(gfxPoint(-bounds.x, -bounds.y));
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
    if (PR_IntervalNow() - start > 3) {
      printf_stderr("Slow alloc %i\n", PR_IntervalNow() - start);
    }
    start = PR_IntervalNow();
#endif
    SAMPLE_LABEL("BasicTiledLayerBuffer", "PaintThebesSingleBufferDraw");

    mCallback(mThebesLayer, ctxt, aPaintRegion, nsIntRegion(), mCallbackData);
  }

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 30) {
    const nsIntRect bounds = aPaintRegion.GetBounds();
    printf_stderr("Time to draw %i: %i, %i, %i, %i\n", PR_IntervalNow() - start, bounds.x, bounds.y, bounds.width, bounds.height);
    if (aPaintRegion.IsComplex()) {
      printf_stderr("Complex region\n");
      nsIntRegionRectIterator it(aPaintRegion);
      for (const nsIntRect* rect = it.Next(); rect != nullptr; rect = it.Next()) {
        printf_stderr(" rect %i, %i, %i, %i\n", rect->x, rect->y, rect->width, rect->height);
      }
    }
  }
  start = PR_IntervalNow();
#endif

  SAMPLE_LABEL("BasicTiledLayerBuffer", "PaintThebesUpdate");
  Update(aNewValidRegion, aPaintRegion);

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 10) {
    const nsIntRect bounds = aPaintRegion.GetBounds();
    printf_stderr("Time to tile %i: %i, %i, %i, %i\n", PR_IntervalNow() - start, bounds.x, bounds.y, bounds.width, bounds.height);
  }
#endif

  mLastPaintOpaque = mThebesLayer->CanUseOpaqueSurface();
  mThebesLayer = nullptr;
  mCallback = nullptr;
  mCallbackData = nullptr;
  mSinglePaintBuffer = nullptr;
}

BasicTiledLayerTile
BasicTiledLayerBuffer::ValidateTileInternal(BasicTiledLayerTile aTile,
                                            const nsIntPoint& aTileOrigin,
                                            const nsIntRect& aDirtyRect)
{
  if (aTile == GetPlaceholderTile() || aTile.mSurface->Format() != GetFormat()) {
    gfxImageSurface* tmpTile = new gfxImageSurface(gfxIntSize(GetTileLength(), GetTileLength()),
                                                   GetFormat(), !mThebesLayer->CanUseOpaqueSurface());
    aTile = BasicTiledLayerTile(tmpTile);
  }

  // Use the gfxReusableSurfaceWrapper, which will reuse the surface
  // if the compositor no longer has a read lock, otherwise the surface
  // will be copied into a new writable surface.
  gfxImageSurface* writableSurface;
  aTile.mSurface = aTile.mSurface->GetWritable(&writableSurface);

  // Bug 742100, this gfxContext really should live on the stack.
  nsRefPtr<gfxContext> ctxt = new gfxContext(writableSurface);

  if (mSinglePaintBuffer) {
    gfxRect drawRect(aDirtyRect.x - aTileOrigin.x, aDirtyRect.y - aTileOrigin.y,
                     aDirtyRect.width, aDirtyRect.height);

    ctxt->SetOperator(gfxContext::OPERATOR_SOURCE);
    ctxt->NewPath();
    ctxt->SetSource(mSinglePaintBuffer.get(),
                    gfxPoint((mSinglePaintBufferOffset.x - aDirtyRect.x + drawRect.x) *
                             mResolution,
                             (mSinglePaintBufferOffset.y - aDirtyRect.y + drawRect.y) *
                             mResolution));
    drawRect.Scale(mResolution, mResolution);
    ctxt->Rectangle(drawRect, true);
    ctxt->Fill();
  } else {
    ctxt->NewPath();
    ctxt->Scale(mResolution, mResolution);
    ctxt->Translate(gfxPoint(-aTileOrigin.x, -aTileOrigin.y));
    nsIntPoint a = nsIntPoint(aTileOrigin.x, aTileOrigin.y);
    mCallback(mThebesLayer, ctxt,
              nsIntRegion(nsIntRect(a, nsIntSize(GetScaledTileLength(),
                                                 GetScaledTileLength()))),
              nsIntRegion(), mCallbackData);
  }

#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
  DrawDebugOverlay(writableSurface, aTileOrigin.x * mResolution,
                   aTileOrigin.y * mResolution);
#endif

  return aTile;
}

BasicTiledLayerTile
BasicTiledLayerBuffer::ValidateTile(BasicTiledLayerTile aTile,
                                    const nsIntPoint& aTileOrigin,
                                    const nsIntRegion& aDirtyRegion)
{

  SAMPLE_LABEL("BasicTiledLayerBuffer", "ValidateTile");

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (aDirtyRegion.IsComplex()) {
    printf_stderr("Complex region\n");
  }
#endif

  nsIntRegionRectIterator it(aDirtyRegion);
  for (const nsIntRect* rect = it.Next(); rect != nullptr; rect = it.Next()) {
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
    printf_stderr(" break into subrect %i, %i, %i, %i\n", rect->x, rect->y, rect->width, rect->height);
#endif
    aTile = ValidateTileInternal(aTile, aTileOrigin, *rect);
  }

  return aTile;
}

void
BasicTiledThebesLayer::FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
{
  aAttrs = ThebesLayerAttributes(GetValidRegion());
}

static nsIntRect
RoundedTransformViewportBounds(const gfx::Rect& aViewport,
                               const gfx::Point& aScrollOffset,
                               const gfxSize& aResolution,
                               float aScaleX,
                               float aScaleY,
                               const gfx3DMatrix& aTransform)
{
  gfxRect transformedViewport(aViewport.x - (aScrollOffset.x * aResolution.width),
                              aViewport.y - (aScrollOffset.y * aResolution.height),
                              aViewport.width, aViewport.height);
  transformedViewport.Scale((aScaleX / aResolution.width) / aResolution.width,
                            (aScaleY / aResolution.height) / aResolution.height);
  transformedViewport = aTransform.TransformBounds(transformedViewport);

  return nsIntRect((int32_t)floor(transformedViewport.x),
                   (int32_t)floor(transformedViewport.y),
                   (int32_t)ceil(transformedViewport.width),
                   (int32_t)ceil(transformedViewport.height));
}

bool
BasicTiledThebesLayer::ComputeProgressiveUpdateRegion(BasicTiledLayerBuffer& aTiledBuffer,
                                                      const nsIntRegion& aInvalidRegion,
                                                      const nsIntRegion& aOldValidRegion,
                                                      nsIntRegion& aRegionToPaint,
                                                      const gfx3DMatrix& aTransform,
                                                      const gfx::Point& aScrollOffset,
                                                      const gfxSize& aResolution,
                                                      bool aIsRepeated)
{
  aRegionToPaint = aInvalidRegion;

  // If this is a low precision buffer, we force progressive updates. The
  // assumption is that the contents is less important, so visual coherency
  // is lower priority than speed.
  bool drawingLowPrecision = aTiledBuffer.IsLowPrecision();

  // Find out if we have any non-stale content to update.
  nsIntRegion freshRegion;
  if (!mFirstPaint) {
    freshRegion.And(aInvalidRegion, aOldValidRegion);
    freshRegion.Sub(aInvalidRegion, freshRegion);
  }

  // Find out the current view transform to determine which tiles to draw
  // first, and see if we should just abort this paint. Aborting is usually
  // caused by there being an incoming, more relevant paint.
  gfx::Rect viewport;
  float scaleX, scaleY;
  if (BasicManager()->ProgressiveUpdateCallback(!freshRegion.IsEmpty(), viewport,
                                                scaleX, scaleY, !drawingLowPrecision)) {
    SAMPLE_MARKER("Abort painting");
    aRegionToPaint.SetEmpty();
    return aIsRepeated;
  }

  // Transform the screen coordinates into local layer coordinates.
  nsIntRect roundedTransformedViewport =
    RoundedTransformViewportBounds(viewport, aScrollOffset, aResolution,
                                   scaleX, scaleY, aTransform);

  // Paint tiles that have no content before tiles that only have stale content.
  bool drawingStale = freshRegion.IsEmpty();
  if (!drawingStale) {
    aRegionToPaint = freshRegion;
  }

  // Prioritise tiles that are currently visible on the screen.
  bool paintVisible = false;
  if (aRegionToPaint.Intersects(roundedTransformedViewport)) {
    aRegionToPaint.And(aRegionToPaint, roundedTransformedViewport);
    paintVisible = true;
  }

  // The following code decides what order to draw tiles in, based on the
  // current scroll direction of the primary scrollable layer.
  NS_ASSERTION(!aRegionToPaint.IsEmpty(), "Unexpectedly empty paint region!");
  nsIntRect paintBounds = aRegionToPaint.GetBounds();

  int startX, incX, startY, incY;
  int tileLength = aTiledBuffer.GetScaledTileLength();
  if (aScrollOffset.x >= mLastScrollOffset.x) {
    startX = aTiledBuffer.RoundDownToTileEdge(paintBounds.x);
    incX = tileLength;
  } else {
    startX = aTiledBuffer.RoundDownToTileEdge(paintBounds.XMost() - 1);
    incX = -tileLength;
  }

  if (aScrollOffset.y >= mLastScrollOffset.y) {
    startY = aTiledBuffer.RoundDownToTileEdge(paintBounds.y);
    incY = tileLength;
  } else {
    startY = aTiledBuffer.RoundDownToTileEdge(paintBounds.YMost() - 1);
    incY = -tileLength;
  }

  // Find a tile to draw.
  nsIntRect tileBounds(startX, startY, tileLength, tileLength);
  int32_t scrollDiffX = aScrollOffset.x - mLastScrollOffset.x;
  int32_t scrollDiffY = aScrollOffset.y - mLastScrollOffset.y;
  // This loop will always terminate, as there is at least one tile area
  // along the first/last row/column intersecting with regionToPaint, or its
  // bounds would have been smaller.
  while (true) {
    aRegionToPaint.And(aInvalidRegion, tileBounds);
    if (!aRegionToPaint.IsEmpty()) {
      break;
    }
    if (NS_ABS(scrollDiffY) >= NS_ABS(scrollDiffX)) {
      tileBounds.x += incX;
    } else {
      tileBounds.y += incY;
    }
  }

  bool repeatImmediately = false;
  if (!aRegionToPaint.Contains(aInvalidRegion)) {
    // The region needed to paint is larger then our progressive chunk size
    // therefore update what we want to paint and ask for a new paint transaction.

    // If we're drawing stale, visible content, make sure that it happens
    // in one go by repeating this work without calling the painted
    // callback. The remaining content is then drawn tile-by-tile in
    // multiple transactions.
    if (!drawingLowPrecision && paintVisible && drawingStale) {
      repeatImmediately = true;
    } else {
      BasicManager()->SetRepeatTransaction();
    }
  }

  return repeatImmediately;
}

bool
BasicTiledThebesLayer::ProgressiveUpdate(BasicTiledLayerBuffer& aTiledBuffer,
                                         nsIntRegion& aValidRegion,
                                         nsIntRegion& aInvalidRegion,
                                         const nsIntRegion& aOldValidRegion,
                                         const gfx3DMatrix& aTransform,
                                         const gfx::Point& aScrollOffset,
                                         const gfxSize& aResolution,
                                         LayerManager::DrawThebesLayerCallback aCallback,
                                         void* aCallbackData)
{
  bool repeat = false;
  do {
    // Compute the region that should be updated. Repeat as many times as
    // is required.
    nsIntRegion regionToPaint;
    repeat = ComputeProgressiveUpdateRegion(aTiledBuffer,
                                            aInvalidRegion,
                                            aOldValidRegion,
                                            regionToPaint,
                                            aTransform,
                                            aScrollOffset,
                                            aResolution,
                                            repeat);

    // There's no further work to be done, return if nothing has been
    // drawn, or give what has been drawn to the shadow layer to upload.
    if (regionToPaint.IsEmpty()) {
      if (repeat) {
        break;
      } else {
        return false;
      }
    }

    // Keep track of what we're about to refresh.
    aValidRegion.Or(aValidRegion, regionToPaint);

    // aValidRegion may have been altered by InvalidateRegion, but we still
    // want to display stale content until it gets progressively updated.
    // Create a region that includes stale content.
    nsIntRegion validOrStale;
    validOrStale.Or(aValidRegion, aOldValidRegion);

    // Paint the computed region and subtract it from the invalid region.
    aTiledBuffer.PaintThebes(this, validOrStale, regionToPaint, aCallback, aCallbackData);
    aInvalidRegion.Sub(aInvalidRegion, regionToPaint);
  } while (repeat);

  return true;
}

void
BasicTiledThebesLayer::PaintThebes(gfxContext* aContext,
                                   Layer* aMaskLayer,
                                   LayerManager::DrawThebesLayerCallback aCallback,
                                   void* aCallbackData,
                                   ReadbackProcessor* aReadback)
{
  if (!aCallback) {
    BasicManager()->SetTransactionIncomplete();
    return;
  }

  if (!HasShadow()) {
    NS_ASSERTION(false, "Shadow requested for painting\n");
    return;
  }

  if (mTiledBuffer.HasFormatChanged(this)) {
    mValidRegion = nsIntRegion();
  }
  if (mLowPrecisionTiledBuffer.HasFormatChanged(this)) {
    mLowPrecisionValidRegion = nsIntRegion();
  }

  nsIntRegion invalidRegion = mVisibleRegion;
  invalidRegion.Sub(invalidRegion, mValidRegion);
  if (invalidRegion.IsEmpty())
    return;

  // Calculate the transform required to convert screen space into layer space
  gfx3DMatrix transform = GetEffectiveTransform();
  // XXX Not sure if this code for intermediate surfaces is correct.
  //     It rarely gets hit though, and shouldn't have terrible consequences
  //     even if it is wrong.
  for (ContainerLayer* parent = GetParent(); parent; parent = parent->GetParent()) {
    if (parent->UseIntermediateSurface()) {
      transform.PreMultiply(parent->GetEffectiveTransform());
    }
  }
  transform.Invert();

  nsIntRect layerDisplayPort;
  const gfx::Rect& criticalDisplayPort = GetParent()->GetFrameMetrics().mCriticalDisplayPort;
  if (!criticalDisplayPort.IsEmpty()) {
    // Find the critical display port in layer space.
    gfxRect transformedCriticalDisplayPort = transform.TransformBounds(
      gfxRect(criticalDisplayPort.x, criticalDisplayPort.y,
              criticalDisplayPort.width, criticalDisplayPort.height));
    transformedCriticalDisplayPort.RoundOut();
    layerDisplayPort = nsIntRect(transformedCriticalDisplayPort.x,
                                 transformedCriticalDisplayPort.y,
                                 transformedCriticalDisplayPort.width,
                                 transformedCriticalDisplayPort.height);

    // Clip the invalid region to the critical display-port
    invalidRegion.And(invalidRegion, layerDisplayPort);
    if (invalidRegion.IsEmpty())
      return;
  }

  gfxSize resolution(1, 1);
  for (ContainerLayer* parent = GetParent(); parent; parent = parent->GetParent()) {
    const FrameMetrics& metrics = parent->GetFrameMetrics();
    resolution.width *= metrics.mResolution.width;
    resolution.height *= metrics.mResolution.height;
  }

  // Calculate the scroll offset since the last transaction.
  gfx::Point scrollOffset(0, 0);
  Layer* primaryScrollable = BasicManager()->GetPrimaryScrollableLayer();
  if (primaryScrollable) {
    const FrameMetrics& metrics = primaryScrollable->AsContainerLayer()->GetFrameMetrics();
    scrollOffset = metrics.mScrollOffset;
  }

  // Only draw progressively when the resolution is unchanged.
  if (gfxPlatform::UseProgressiveTilePainting() &&
      !BasicManager()->HasShadowTarget() &&
      mTiledBuffer.GetFrameResolution() == resolution) {
    // Store the old valid region, then clear it before painting.
    // We clip the old valid region to the visible region, as it only gets
    // used to decide stale content (currently valid and previously visible)
    nsIntRegion oldValidRegion = mTiledBuffer.GetValidRegion();
    oldValidRegion.And(oldValidRegion, mVisibleRegion);
    if (!layerDisplayPort.IsEmpty()) {
      oldValidRegion.And(oldValidRegion, layerDisplayPort);
    }

    // Make sure that tiles that fall outside of the visible region are
    // discarded on the first update.
    if (!BasicManager()->IsRepeatTransaction()) {
      mValidRegion.And(mValidRegion, mVisibleRegion);
      if (!layerDisplayPort.IsEmpty()) {
        mValidRegion.And(mValidRegion, layerDisplayPort);
      }
    }

    if (!ProgressiveUpdate(mTiledBuffer, mValidRegion, invalidRegion,
                           oldValidRegion, transform, scrollOffset, resolution,
                           aCallback, aCallbackData))
      return;
  } else {
    mTiledBuffer.SetFrameResolution(resolution);
    mValidRegion = mVisibleRegion;
    if (!layerDisplayPort.IsEmpty()) {
      mValidRegion.And(mValidRegion, layerDisplayPort);
    }
    mTiledBuffer.PaintThebes(this, mValidRegion, invalidRegion, aCallback, aCallbackData);
  }

  mTiledBuffer.ReadLock();

  // Only paint the mask layer on the first transaction.
  if (aMaskLayer && !BasicManager()->IsRepeatTransaction()) {
    static_cast<BasicImplData*>(aMaskLayer->ImplData())
      ->Paint(aContext, nullptr);
  }

  // Create a heap copy owned and released by the compositor. This is needed
  // since we're sending this over an async message and content needs to be
  // be able to modify the tiled buffer in the next transaction.
  // TODO: Remove me once Bug 747811 lands.
  BasicTiledLayerBuffer *heapCopy = new BasicTiledLayerBuffer(mTiledBuffer);

  BasicManager()->PaintedTiledLayerBuffer(BasicManager()->Hold(this), heapCopy);
  mTiledBuffer.ClearPaintedRegion();

  // If we have a critical display-port defined, render the full display-port
  // progressively in the low-precision tiled buffer.
  bool clearedLowPrecision = false;
  bool updatedLowPrecision = false;
  if (!criticalDisplayPort.IsEmpty() &&
      !nsIntRegion(layerDisplayPort).Contains(mVisibleRegion)) {
    nsIntRegion oldValidRegion = mLowPrecisionTiledBuffer.GetValidRegion();
    oldValidRegion.And(oldValidRegion, mVisibleRegion);

    // If the frame resolution has changed, invalidate the buffer
    if (mLowPrecisionTiledBuffer.GetFrameResolution() != resolution) {
      if (!mLowPrecisionValidRegion.IsEmpty()) {
        clearedLowPrecision = true;
      }
      oldValidRegion.SetEmpty();
      mLowPrecisionValidRegion.SetEmpty();
      mLowPrecisionTiledBuffer.SetFrameResolution(resolution);
    }

    // Invalidate previously valid content that is no longer visible
    if (!BasicManager()->IsRepeatTransaction()) {
      mLowPrecisionValidRegion.And(mLowPrecisionValidRegion, mVisibleRegion);
    }

    nsIntRegion lowPrecisionInvalidRegion;
    lowPrecisionInvalidRegion.Sub(mVisibleRegion, mLowPrecisionValidRegion);

    // Remove the valid high-precision region from the invalid low-precision
    // region. We don't want to spend time drawing things twice.
    nsIntRegion invalidHighPrecisionIntersect;
    invalidHighPrecisionIntersect.And(lowPrecisionInvalidRegion, mValidRegion);
    lowPrecisionInvalidRegion.Sub(lowPrecisionInvalidRegion, invalidHighPrecisionIntersect);

    if (!lowPrecisionInvalidRegion.IsEmpty()) {
      updatedLowPrecision =
        ProgressiveUpdate(mLowPrecisionTiledBuffer, mLowPrecisionValidRegion,
                          lowPrecisionInvalidRegion, oldValidRegion, transform,
                          scrollOffset, resolution, aCallback, aCallbackData);
    }

    // Re-add the high-precision valid region intersection so that we can
    // maintain coherency when the valid region changes.
    lowPrecisionInvalidRegion.Or(lowPrecisionInvalidRegion, invalidHighPrecisionIntersect);
  } else if (!mLowPrecisionValidRegion.IsEmpty()) {
    // Clear the low precision tiled buffer
    clearedLowPrecision = true;
    mLowPrecisionValidRegion.SetEmpty();
    mLowPrecisionTiledBuffer.PaintThebes(this, mLowPrecisionValidRegion,
                                         mLowPrecisionValidRegion, aCallback,
                                         aCallbackData);
  }

  // We send a Painted callback if we clear the valid region of the low
  // precision buffer, so that the shadow buffer's valid region can be updated
  // and the associated resources can be freed.
  if (clearedLowPrecision || updatedLowPrecision) {
    mLowPrecisionTiledBuffer.ReadLock();
    BasicTiledLayerBuffer *heapCopy = new BasicTiledLayerBuffer(mLowPrecisionTiledBuffer);

    // The GL layer manager uses the buffer resolution to distinguish calls
    // to PaintedTiledLayerBuffer.
    BasicManager()->PaintedTiledLayerBuffer(BasicManager()->Hold(this), heapCopy);
    mLowPrecisionTiledBuffer.ClearPaintedRegion();
  }

  // The transaction is completed, store the last scroll offset.
  if (!BasicManager()->GetRepeatTransaction()) {
    mLastScrollOffset = scrollOffset;
  }
  mFirstPaint = false;
}

} // mozilla
} // layers
