/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TiledContentClient.h"
#include <math.h>                       // for ceil, ceilf, floor
#include "ClientTiledThebesLayer.h"     // for ClientTiledThebesLayer
#include "GeckoProfiler.h"              // for PROFILER_LABEL
#include "ClientLayerManager.h"         // for ClientLayerManager
#include "gfxContext.h"                 // for gfxContext, etc
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxRect.h"                    // for gfxRect
#include "mozilla/MathAlgorithms.h"     // for Abs
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/ShadowLayers.h"  // for ShadowLayerForwarder
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for gfxContext::AddRef, etc
#include "nsSize.h"                     // for nsIntSize
#include "gfxReusableSharedImageSurfaceWrapper.h"
#include "nsMathUtils.h"               // for NS_roundf
#include "gfx2DGlue.h"

#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
#include "cairo.h"
#include <sstream>
using mozilla::layers::Layer;
static void DrawDebugOverlay(gfxASurface* imgSurf, int x, int y)
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

using namespace gfx;

namespace layers {


TiledContentClient::TiledContentClient(ClientTiledThebesLayer* aThebesLayer,
                                       ClientLayerManager* aManager)
  : CompositableClient(aManager->AsShadowForwarder())
  , mTiledBuffer(aThebesLayer, aManager)
  , mLowPrecisionTiledBuffer(aThebesLayer, aManager)
{
  MOZ_COUNT_CTOR(TiledContentClient);

  mLowPrecisionTiledBuffer.SetResolution(gfxPlatform::GetLowPrecisionResolution());
}

void
TiledContentClient::LockCopyAndWrite(TiledBufferType aType)
{
  BasicTiledLayerBuffer* buffer = aType == LOW_PRECISION_TILED_BUFFER
    ? &mLowPrecisionTiledBuffer
    : &mTiledBuffer;

  // Take an extra ReadLock on behalf of the TiledContentHost. This extra
  // reference will be adopted when the descriptor is opened by
  // BasicTiledLayerTile::OpenDescriptor.
  buffer->ReadLock();

  mForwarder->PaintedTiledLayerBuffer(this, buffer->GetSurfaceDescriptorTiles());
  buffer->ClearPaintedRegion();
}

BasicTiledLayerBuffer::BasicTiledLayerBuffer(ClientTiledThebesLayer* aThebesLayer,
                                             ClientLayerManager* aManager)
  : mThebesLayer(aThebesLayer)
  , mManager(aManager)
  , mLastPaintOpaque(false)
{
}

bool
BasicTiledLayerBuffer::HasFormatChanged() const
{
  return mThebesLayer->CanUseOpaqueSurface() != mLastPaintOpaque;
}


gfxContentType
BasicTiledLayerBuffer::GetContentType() const
{
  if (mThebesLayer->CanUseOpaqueSurface()) {
    return GFX_CONTENT_COLOR;
  } else {
    return GFX_CONTENT_COLOR_ALPHA;
  }
}


TileDescriptor
BasicTiledLayerTile::GetTileDescriptor()
{
  gfxReusableSurfaceWrapper* surface = GetSurface();
  switch (surface->GetType()) {
  case gfxReusableSurfaceWrapper::TYPE_IMAGE :
    return BasicTileDescriptor(uintptr_t(surface));

  case gfxReusableSurfaceWrapper::TYPE_SHARED_IMAGE :
    return BasicShmTileDescriptor(static_cast<gfxReusableSharedImageSurfaceWrapper*>(surface)->GetShmem());

  default :
    NS_NOTREACHED("Unhandled gfxReusableSurfaceWrapper type");
    return PlaceholderTileDescriptor();
  }
}


/* static */ BasicTiledLayerTile
BasicTiledLayerTile::OpenDescriptor(ISurfaceAllocator *aAllocator, const TileDescriptor& aDesc)
{
  switch (aDesc.type()) {
  case TileDescriptor::TBasicShmTileDescriptor : {
    nsRefPtr<gfxReusableSurfaceWrapper> surface =
      gfxReusableSharedImageSurfaceWrapper::Open(
        aAllocator, aDesc.get_BasicShmTileDescriptor().reusableSurface());
    return BasicTiledLayerTile(
      new DeprecatedTextureClientTile(nullptr, TextureInfo(BUFFER_TILED), surface));
  }

  case TileDescriptor::TBasicTileDescriptor : {
    nsRefPtr<gfxReusableSurfaceWrapper> surface =
      reinterpret_cast<gfxReusableSurfaceWrapper*>(
        aDesc.get_BasicTileDescriptor().reusableSurface());
    surface->ReadUnlock();
    return BasicTiledLayerTile(
      new DeprecatedTextureClientTile(nullptr, TextureInfo(BUFFER_TILED), surface));
  }

  default :
    NS_NOTREACHED("Unknown tile descriptor type!");
    return nullptr;
  }
}

SurfaceDescriptorTiles
BasicTiledLayerBuffer::GetSurfaceDescriptorTiles()
{
  InfallibleTArray<TileDescriptor> tiles;

  for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
    TileDescriptor tileDesc;
    if (mRetainedTiles.SafeElementAt(i, GetPlaceholderTile()) == GetPlaceholderTile()) {
      tileDesc = PlaceholderTileDescriptor();
    } else {
      tileDesc = mRetainedTiles[i].GetTileDescriptor();
    }
    tiles.AppendElement(tileDesc);
  }
  return SurfaceDescriptorTiles(mValidRegion, mPaintedRegion,
                                tiles, mRetainedWidth, mRetainedHeight,
                                mResolution);
}

/* static */ BasicTiledLayerBuffer
BasicTiledLayerBuffer::OpenDescriptor(ISurfaceAllocator *aAllocator,
                                      const SurfaceDescriptorTiles& aDescriptor)
{
  return BasicTiledLayerBuffer(aAllocator,
                               aDescriptor.validRegion(),
                               aDescriptor.paintedRegion(),
                               aDescriptor.tiles(),
                               aDescriptor.retainedWidth(),
                               aDescriptor.retainedHeight(),
                               aDescriptor.resolution());
}

void
BasicTiledLayerBuffer::PaintThebes(const nsIntRegion& aNewValidRegion,
                                   const nsIntRegion& aPaintRegion,
                                   LayerManager::DrawThebesLayerCallback aCallback,
                                   void* aCallbackData)
{
  mCallback = aCallback;
  mCallbackData = aCallbackData;

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  long start = PR_IntervalNow();
#endif

  // If this region is empty XMost() - 1 will give us a negative value.
  NS_ASSERTION(!aPaintRegion.GetBounds().IsEmpty(), "Empty paint region\n");

  bool useSinglePaintBuffer = UseSinglePaintBuffer();
  // XXX The single-tile case doesn't work at the moment, see bug 850396
  /*
  if (useSinglePaintBuffer) {
    // Check if the paint only spans a single tile. If that's
    // the case there's no point in using a single paint buffer.
    nsIntRect paintBounds = aPaintRegion.GetBounds();
    useSinglePaintBuffer = GetTileStart(paintBounds.x) !=
                           GetTileStart(paintBounds.XMost() - 1) ||
                           GetTileStart(paintBounds.y) !=
                           GetTileStart(paintBounds.YMost() - 1);
  }
  */

  if (useSinglePaintBuffer) {
    nsRefPtr<gfxContext> ctxt;

    const nsIntRect bounds = aPaintRegion.GetBounds();
    {
      PROFILER_LABEL("BasicTiledLayerBuffer", "PaintThebesSingleBufferAlloc");
      gfxImageFormat format =
        gfxPlatform::GetPlatform()->OptimalFormatForContent(
          GetContentType());

      if (gfxPlatform::GetPlatform()->SupportsAzureContent()) {
        mSinglePaintDrawTarget =
          gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
            gfx::IntSize(ceilf(bounds.width * mResolution),
                         ceilf(bounds.height * mResolution)),
            gfx::ImageFormatToSurfaceFormat(format));

        ctxt = new gfxContext(mSinglePaintDrawTarget);
      } else {
        mSinglePaintBuffer = new gfxImageSurface(
          gfxIntSize(ceilf(bounds.width * mResolution),
                     ceilf(bounds.height * mResolution)),
          format,
          !mThebesLayer->CanUseOpaqueSurface());
        ctxt = new gfxContext(mSinglePaintBuffer);
      }

      mSinglePaintBufferOffset = nsIntPoint(bounds.x, bounds.y);
    }
    ctxt->NewPath();
    ctxt->Scale(mResolution, mResolution);
    ctxt->Translate(gfxPoint(-bounds.x, -bounds.y));
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
    if (PR_IntervalNow() - start > 3) {
      printf_stderr("Slow alloc %i\n", PR_IntervalNow() - start);
    }
    start = PR_IntervalNow();
#endif
    PROFILER_LABEL("BasicTiledLayerBuffer", "PaintThebesSingleBufferDraw");

    mCallback(mThebesLayer, ctxt, aPaintRegion, CLIP_NONE, nsIntRegion(), mCallbackData);
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

  PROFILER_LABEL("BasicTiledLayerBuffer", "PaintThebesUpdate");
  Update(aNewValidRegion, aPaintRegion);

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 10) {
    const nsIntRect bounds = aPaintRegion.GetBounds();
    printf_stderr("Time to tile %i: %i, %i, %i, %i\n", PR_IntervalNow() - start, bounds.x, bounds.y, bounds.width, bounds.height);
  }
#endif

  mLastPaintOpaque = mThebesLayer->CanUseOpaqueSurface();
  mCallback = nullptr;
  mCallbackData = nullptr;
  mSinglePaintBuffer = nullptr;
  mSinglePaintDrawTarget = nullptr;
}

BasicTiledLayerTile
BasicTiledLayerBuffer::ValidateTileInternal(BasicTiledLayerTile aTile,
                                            const nsIntPoint& aTileOrigin,
                                            const nsIntRect& aDirtyRect)
{
  if (aTile.IsPlaceholderTile()) {
    RefPtr<DeprecatedTextureClient> textureClient =
      new DeprecatedTextureClientTile(mManager->AsShadowForwarder(), TextureInfo(BUFFER_TILED));
    aTile.mDeprecatedTextureClient = static_cast<DeprecatedTextureClientTile*>(textureClient.get());
  }
  aTile.mDeprecatedTextureClient->EnsureAllocated(gfx::IntSize(GetTileLength(), GetTileLength()), GetContentType());
  gfxImageSurface* writableSurface = aTile.mDeprecatedTextureClient->LockImageSurface();
  // Bug 742100, this gfxContext really should live on the stack.
  nsRefPtr<gfxContext> ctxt;

  RefPtr<gfx::DrawTarget> writableDrawTarget;
  if (gfxPlatform::GetPlatform()->SupportsAzureContent()) {
    // TODO: Instead of creating a gfxImageSurface to back the tile we should
    // create an offscreen DrawTarget. This would need to be shared cross-thread
    // and support copy on write semantics.
    gfx::SurfaceFormat format =
      gfx::ImageFormatToSurfaceFormat(writableSurface->Format());

    writableDrawTarget =
      gfxPlatform::GetPlatform()->CreateDrawTargetForData(
        writableSurface->Data(),
        gfx::IntSize(writableSurface->Width(), writableSurface->Height()),
        writableSurface->Stride(),
        format);
    ctxt = new gfxContext(writableDrawTarget);
  } else {
    ctxt = new gfxContext(writableSurface);
    ctxt->SetOperator(gfxContext::OPERATOR_SOURCE);
  }

  gfxRect drawRect(aDirtyRect.x - aTileOrigin.x, aDirtyRect.y - aTileOrigin.y,
                   aDirtyRect.width, aDirtyRect.height);

  if (mSinglePaintBuffer || mSinglePaintDrawTarget) {
    if (gfxPlatform::GetPlatform()->SupportsAzureContent()) {
      gfx::Rect drawRect(aDirtyRect.x - aTileOrigin.x,
                         aDirtyRect.y - aTileOrigin.y,
                         aDirtyRect.width,
                         aDirtyRect.height);
      drawRect.Scale(mResolution);

      RefPtr<gfx::SourceSurface> source = mSinglePaintDrawTarget->Snapshot();
      writableDrawTarget->CopySurface(
        source,
        gfx::IntRect(NS_roundf((aDirtyRect.x - mSinglePaintBufferOffset.x) * mResolution),
                     NS_roundf((aDirtyRect.y - mSinglePaintBufferOffset.y) * mResolution),
                     drawRect.width,
                     drawRect.height),
        gfx::IntPoint(NS_roundf(drawRect.x), NS_roundf(drawRect.y)));
    } else {
      gfxRect drawRect(aDirtyRect.x - aTileOrigin.x, aDirtyRect.y - aTileOrigin.y,
                       aDirtyRect.width, aDirtyRect.height);
      drawRect.Scale(mResolution, mResolution);

      ctxt->NewPath();
      ctxt->SetSource(mSinglePaintBuffer.get(),
                      gfxPoint((mSinglePaintBufferOffset.x - aDirtyRect.x) * mResolution + drawRect.x,
                               (mSinglePaintBufferOffset.y - aDirtyRect.y) * mResolution + drawRect.y));
      ctxt->SnappedRectangle(drawRect);
      ctxt->Fill();
    }
  } else {
    ctxt->NewPath();
    ctxt->Scale(mResolution, mResolution);
    ctxt->Translate(gfxPoint(-aTileOrigin.x, -aTileOrigin.y));
    nsIntPoint a = nsIntPoint(aTileOrigin.x, aTileOrigin.y);
    mCallback(mThebesLayer, ctxt,
              nsIntRegion(nsIntRect(a, nsIntSize(GetScaledTileLength(),
                                                 GetScaledTileLength()))),
              CLIP_NONE,
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
  PROFILER_LABEL("BasicTiledLayerBuffer", "ValidateTile");

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

static LayoutDeviceRect
TransformCompositionBounds(const ScreenRect& aCompositionBounds,
                           const CSSToScreenScale& aZoom,
                           const ScreenPoint& aScrollOffset,
                           const CSSToScreenScale& aResolution,
                           const gfx3DMatrix& aTransformScreenToLayout)
{
  // Transform the current composition bounds into transformed layout device
  // space by compensating for the difference in resolution and subtracting the
  // old composition bounds origin.
  ScreenRect offsetViewportRect = (aCompositionBounds / aZoom) * aResolution;
  offsetViewportRect.MoveBy(-aScrollOffset);

  gfxRect transformedViewport =
    aTransformScreenToLayout.TransformBounds(
      gfxRect(offsetViewportRect.x, offsetViewportRect.y,
              offsetViewportRect.width, offsetViewportRect.height));

  return LayoutDeviceRect(transformedViewport.x,
                          transformedViewport.y,
                          transformedViewport.width,
                          transformedViewport.height);
}

bool
BasicTiledLayerBuffer::ComputeProgressiveUpdateRegion(const nsIntRegion& aInvalidRegion,
                                                      const nsIntRegion& aOldValidRegion,
                                                      nsIntRegion& aRegionToPaint,
                                                      BasicTiledLayerPaintData* aPaintData,
                                                      bool aIsRepeated)
{
  aRegionToPaint = aInvalidRegion;

  // If the composition bounds rect is empty, we can't make any sensible
  // decision about how to update coherently. In this case, just update
  // everything in one transaction.
  if (aPaintData->mCompositionBounds.IsEmpty()) {
    aPaintData->mPaintFinished = true;
    return false;
  }

  // If this is a low precision buffer, we force progressive updates. The
  // assumption is that the contents is less important, so visual coherency
  // is lower priority than speed.
  bool drawingLowPrecision = IsLowPrecision();

  // Find out if we have any non-stale content to update.
  nsIntRegion staleRegion;
  staleRegion.And(aInvalidRegion, aOldValidRegion);

  // Find out the current view transform to determine which tiles to draw
  // first, and see if we should just abort this paint. Aborting is usually
  // caused by there being an incoming, more relevant paint.
  ScreenRect compositionBounds;
  CSSToScreenScale zoom;
  if (mManager->ProgressiveUpdateCallback(!staleRegion.Contains(aInvalidRegion),
                                          compositionBounds, zoom,
                                          !drawingLowPrecision)) {
    // We ignore if front-end wants to abort if this is the first,
    // non-low-precision paint, as in that situation, we're about to override
    // front-end's page/viewport metrics.
    if (!aPaintData->mFirstPaint || drawingLowPrecision) {
      PROFILER_LABEL("ContentClient", "Abort painting");
      aRegionToPaint.SetEmpty();
      return aIsRepeated;
    }
  }

  // Transform the screen coordinates into transformed layout device coordinates.
  LayoutDeviceRect transformedCompositionBounds =
    TransformCompositionBounds(compositionBounds, zoom, aPaintData->mScrollOffset,
                            aPaintData->mResolution, aPaintData->mTransformScreenToLayout);

  // Paint tiles that have stale content or that intersected with the screen
  // at the time of issuing the draw command in a single transaction first.
  // This is to avoid rendering glitches on animated page content, and when
  // layers change size/shape.
  LayoutDeviceRect coherentUpdateRect =
    transformedCompositionBounds.Intersect(aPaintData->mCompositionBounds);

  nsIntRect roundedCoherentUpdateRect =
    LayoutDeviceIntRect::ToUntyped(RoundedOut(coherentUpdateRect));

  aRegionToPaint.And(aInvalidRegion, roundedCoherentUpdateRect);
  aRegionToPaint.Or(aRegionToPaint, staleRegion);
  bool drawingStale = !aRegionToPaint.IsEmpty();
  if (!drawingStale) {
    aRegionToPaint = aInvalidRegion;
  }

  // Prioritise tiles that are currently visible on the screen.
  bool paintVisible = false;
  if (aRegionToPaint.Intersects(roundedCoherentUpdateRect)) {
    aRegionToPaint.And(aRegionToPaint, roundedCoherentUpdateRect);
    paintVisible = true;
  }

  // Paint area that's visible and overlaps previously valid content to avoid
  // visible glitches in animated elements, such as gifs.
  bool paintInSingleTransaction = paintVisible && (drawingStale || aPaintData->mFirstPaint);

  // The following code decides what order to draw tiles in, based on the
  // current scroll direction of the primary scrollable layer.
  NS_ASSERTION(!aRegionToPaint.IsEmpty(), "Unexpectedly empty paint region!");
  nsIntRect paintBounds = aRegionToPaint.GetBounds();

  int startX, incX, startY, incY;
  int tileLength = GetScaledTileLength();
  if (aPaintData->mScrollOffset.x >= aPaintData->mLastScrollOffset.x) {
    startX = RoundDownToTileEdge(paintBounds.x);
    incX = tileLength;
  } else {
    startX = RoundDownToTileEdge(paintBounds.XMost() - 1);
    incX = -tileLength;
  }

  if (aPaintData->mScrollOffset.y >= aPaintData->mLastScrollOffset.y) {
    startY = RoundDownToTileEdge(paintBounds.y);
    incY = tileLength;
  } else {
    startY = RoundDownToTileEdge(paintBounds.YMost() - 1);
    incY = -tileLength;
  }

  // Find a tile to draw.
  nsIntRect tileBounds(startX, startY, tileLength, tileLength);
  int32_t scrollDiffX = aPaintData->mScrollOffset.x - aPaintData->mLastScrollOffset.x;
  int32_t scrollDiffY = aPaintData->mScrollOffset.y - aPaintData->mLastScrollOffset.y;
  // This loop will always terminate, as there is at least one tile area
  // along the first/last row/column intersecting with regionToPaint, or its
  // bounds would have been smaller.
  while (true) {
    aRegionToPaint.And(aInvalidRegion, tileBounds);
    if (!aRegionToPaint.IsEmpty()) {
      break;
    }
    if (Abs(scrollDiffY) >= Abs(scrollDiffX)) {
      tileBounds.x += incX;
    } else {
      tileBounds.y += incY;
    }
  }

  if (!aRegionToPaint.Contains(aInvalidRegion)) {
    // The region needed to paint is larger then our progressive chunk size
    // therefore update what we want to paint and ask for a new paint transaction.

    // If we need to draw more than one tile to maintain coherency, make
    // sure it happens in the same transaction by requesting this work be
    // repeated immediately.
    // If this is unnecessary, the remaining work will be done tile-by-tile in
    // subsequent transactions.
    if (!drawingLowPrecision && paintInSingleTransaction) {
      return true;
    }

    mManager->SetRepeatTransaction();
    return false;
  }

  // We're not repeating painting and we've not requested a repeat transaction,
  // so the paint is finished. If there's still a separate low precision
  // paint to do, it will get marked as unfinished later.
  aPaintData->mPaintFinished = true;
  return false;
}

bool
BasicTiledLayerBuffer::ProgressiveUpdate(nsIntRegion& aValidRegion,
                                         nsIntRegion& aInvalidRegion,
                                         const nsIntRegion& aOldValidRegion,
                                         BasicTiledLayerPaintData* aPaintData,
                                         LayerManager::DrawThebesLayerCallback aCallback,
                                         void* aCallbackData)
{
  bool repeat = false;
  bool isBufferChanged = false;
  do {
    // Compute the region that should be updated. Repeat as many times as
    // is required.
    nsIntRegion regionToPaint;
    repeat = ComputeProgressiveUpdateRegion(aInvalidRegion,
                                            aOldValidRegion,
                                            regionToPaint,
                                            aPaintData,
                                            repeat);

    // There's no further work to be done.
    if (regionToPaint.IsEmpty()) {
      break;
    }

    isBufferChanged = true;

    // Keep track of what we're about to refresh.
    aValidRegion.Or(aValidRegion, regionToPaint);

    // aValidRegion may have been altered by InvalidateRegion, but we still
    // want to display stale content until it gets progressively updated.
    // Create a region that includes stale content.
    nsIntRegion validOrStale;
    validOrStale.Or(aValidRegion, aOldValidRegion);

    // Paint the computed region and subtract it from the invalid region.
    PaintThebes(validOrStale, regionToPaint, aCallback, aCallbackData);
    aInvalidRegion.Sub(aInvalidRegion, regionToPaint);
  } while (repeat);

  // Return false if nothing has been drawn, or give what has been drawn
  // to the shadow layer to upload.
  return isBufferChanged;
}

}
}
