/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/MultiTiledContentClient.h"

#include "ClientTiledPaintedLayer.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/layers/LayerMetricsWrapper.h"

namespace mozilla {

using namespace gfx;

namespace layers {

MultiTiledContentClient::MultiTiledContentClient(
    ClientTiledPaintedLayer& aPaintedLayer, ClientLayerManager* aManager)
    : TiledContentClient(aManager, "Multi"),
      mTiledBuffer(aPaintedLayer, *this, aManager, &mSharedFrameMetricsHelper),
      mLowPrecisionTiledBuffer(aPaintedLayer, *this, aManager,
                               &mSharedFrameMetricsHelper) {
  MOZ_COUNT_CTOR(MultiTiledContentClient);
  mLowPrecisionTiledBuffer.SetResolution(
      StaticPrefs::layers_low_precision_resolution());
  mHasLowPrecision = StaticPrefs::layers_low_precision_buffer();
}

void MultiTiledContentClient::ClearCachedResources() {
  CompositableClient::ClearCachedResources();
  mTiledBuffer.DiscardBuffers();
  mLowPrecisionTiledBuffer.DiscardBuffers();
}

void MultiTiledContentClient::UpdatedBuffer(TiledBufferType aType) {
  ClientMultiTiledLayerBuffer* buffer = aType == LOW_PRECISION_TILED_BUFFER
                                            ? &mLowPrecisionTiledBuffer
                                            : &mTiledBuffer;

  MOZ_ASSERT(aType != LOW_PRECISION_TILED_BUFFER || mHasLowPrecision);

  mForwarder->UseTiledLayerBuffer(this, buffer->GetSurfaceDescriptorTiles());
}

ClientMultiTiledLayerBuffer::ClientMultiTiledLayerBuffer(
    ClientTiledPaintedLayer& aPaintedLayer,
    CompositableClient& aCompositableClient, ClientLayerManager* aManager,
    SharedFrameMetricsHelper* aHelper)
    : ClientTiledLayerBuffer(aPaintedLayer, aCompositableClient),
      mManager(aManager),
      mCallback(nullptr),
      mCallbackData(nullptr),
      mSharedFrameMetricsHelper(aHelper) {}

void ClientMultiTiledLayerBuffer::DiscardBuffers() {
  for (TileClient& tile : mRetainedTiles) {
    tile.DiscardBuffers();
  }
}

SurfaceDescriptorTiles
ClientMultiTiledLayerBuffer::GetSurfaceDescriptorTiles() {
  nsTArray<TileDescriptor> tiles;

  for (TileClient& tile : mRetainedTiles) {
    TileDescriptor tileDesc = tile.GetTileDescriptor();
    tiles.AppendElement(tileDesc);
    // Reset the update rect
    tile.mUpdateRect = IntRect();
  }
  return SurfaceDescriptorTiles(
      mValidRegion, tiles, mTileOrigin, mTileSize, mTiles.mFirst.x,
      mTiles.mFirst.y, mTiles.mSize.width, mTiles.mSize.height, mResolution,
      mFrameResolution.xScale, mFrameResolution.yScale,
      mWasLastPaintProgressive);
}

void ClientMultiTiledLayerBuffer::PaintThebes(
    const nsIntRegion& aNewValidRegion, const nsIntRegion& aPaintRegion,
    const nsIntRegion& aDirtyRegion,
    LayerManager::DrawPaintedLayerCallback aCallback, void* aCallbackData,
    TilePaintFlags aFlags) {
  TILING_LOG("TILING %p: PaintThebes painting region %s\n", &mPaintedLayer,
             Stringify(aPaintRegion).c_str());
  TILING_LOG("TILING %p: PaintThebes new valid region %s\n", &mPaintedLayer,
             Stringify(aNewValidRegion).c_str());

  mCallback = aCallback;
  mCallbackData = aCallbackData;
  mWasLastPaintProgressive = !!(aFlags & TilePaintFlags::Progressive);

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  long start = PR_IntervalNow();
#endif

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 30) {
    const IntRect bounds = aPaintRegion.GetBounds();
    printf_stderr("Time to draw %i: %i, %i, %i, %i\n", PR_IntervalNow() - start,
                  bounds.x, bounds.y, bounds.width, bounds.height);
    if (aPaintRegion.IsComplex()) {
      printf_stderr("Complex region\n");
      for (auto iter = aPaintRegion.RectIter(); !iter.Done(); iter.Next()) {
        const IntRect& rect = iter.Get();
        printf_stderr(" rect %i, %i, %i, %i\n", rect.x, rect.y, rect.width,
                      rect.height);
      }
    }
  }
  start = PR_IntervalNow();
#endif

  AUTO_PROFILER_LABEL("ClientMultiTiledLayerBuffer::PaintThebes", GRAPHICS);

  mNewValidRegion = aNewValidRegion;
  Update(aNewValidRegion, aPaintRegion, aDirtyRegion, aFlags);

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 10) {
    const IntRect bounds = aPaintRegion.GetBounds();
    printf_stderr("Time to tile %i: %i, %i, %i, %i\n", PR_IntervalNow() - start,
                  bounds.x, bounds.y, bounds.width, bounds.height);
  }
#endif

  mLastPaintContentType = GetContentType(&mLastPaintSurfaceMode);
  mCallback = nullptr;
  mCallbackData = nullptr;
}

void ClientMultiTiledLayerBuffer::MaybeSyncTextures(
    const nsIntRegion& aPaintRegion, const TilesPlacement& aNewTiles,
    const IntSize& aScaledTileSize) {
  if (mManager->AsShadowForwarder()->SupportsTextureDirectMapping()) {
    AutoTArray<uint64_t, 10> syncTextureSerials;
    SurfaceMode mode;
    Unused << GetContentType(&mode);

    // Pre-pass through the tiles (mirroring the filter logic below) to gather
    // texture IDs that we need to ensure are unused by the GPU before we
    // continue.
    if (!aPaintRegion.IsEmpty()) {
      MOZ_ASSERT(mPaintTasks.IsEmpty());
      for (size_t i = 0; i < mRetainedTiles.Length(); ++i) {
        const TileCoordIntPoint tileCoord = aNewTiles.TileCoord(i);

        IntPoint tileOffset = GetTileOffset(tileCoord);
        nsIntRegion tileDrawRegion = IntRect(tileOffset, aScaledTileSize);
        tileDrawRegion.AndWith(aPaintRegion);

        if (tileDrawRegion.IsEmpty()) {
          continue;
        }

        TileClient& tile = mRetainedTiles[i];
        tile.GetSyncTextureSerials(mode, syncTextureSerials);
      }
    }

    if (syncTextureSerials.Length() > 0) {
      mManager->AsShadowForwarder()->SyncTextures(syncTextureSerials);
    }
  }
}

void ClientMultiTiledLayerBuffer::Update(const nsIntRegion& newValidRegion,
                                         const nsIntRegion& aPaintRegion,
                                         const nsIntRegion& aDirtyRegion,
                                         TilePaintFlags aFlags) {
  const IntSize scaledTileSize = GetScaledTileSize();
  const gfx::IntRect newBounds = newValidRegion.GetBounds();

  const TilesPlacement oldTiles = mTiles;
  const TilesPlacement newTiles(
      floor_div(newBounds.X(), scaledTileSize.width),
      floor_div(newBounds.Y(), scaledTileSize.height),
      floor_div(
          GetTileStart(newBounds.X(), scaledTileSize.width) + newBounds.Width(),
          scaledTileSize.width) +
          1,
      floor_div(GetTileStart(newBounds.Y(), scaledTileSize.height) +
                    newBounds.Height(),
                scaledTileSize.height) +
          1);

  const size_t oldTileCount = mRetainedTiles.Length();
  const size_t newTileCount = newTiles.mSize.width * newTiles.mSize.height;

  nsTArray<TileClient> oldRetainedTiles;
  mRetainedTiles.SwapElements(oldRetainedTiles);
  mRetainedTiles.SetLength(newTileCount);

  for (size_t oldIndex = 0; oldIndex < oldTileCount; oldIndex++) {
    const TileCoordIntPoint tileCoord = oldTiles.TileCoord(oldIndex);
    const size_t newIndex = newTiles.TileIndex(tileCoord);
    // First, get the already existing tiles to the right place in the new
    // array. Leave placeholders (default constructor) where there was no tile.
    if (newTiles.HasTile(tileCoord)) {
      mRetainedTiles[newIndex] = oldRetainedTiles[oldIndex];
    } else {
      // release tiles that we are not going to reuse before allocating new ones
      // to avoid allocating unnecessarily.
      oldRetainedTiles[oldIndex].DiscardBuffers();
    }
  }

  oldRetainedTiles.Clear();

  nsIntRegion paintRegion = aPaintRegion;
  nsIntRegion dirtyRegion = aDirtyRegion;

  MaybeSyncTextures(paintRegion, newTiles, scaledTileSize);

  if (!paintRegion.IsEmpty()) {
    MOZ_ASSERT(mPaintTasks.IsEmpty());

    for (size_t i = 0; i < newTileCount; ++i) {
      const TileCoordIntPoint tileCoord = newTiles.TileCoord(i);

      IntPoint tileOffset = GetTileOffset(tileCoord);
      nsIntRegion tileDrawRegion = IntRect(tileOffset, scaledTileSize);
      tileDrawRegion.AndWith(paintRegion);

      if (tileDrawRegion.IsEmpty()) {
        continue;
      }

      TileClient& tile = mRetainedTiles[i];
      if (!ValidateTile(tile, GetTileOffset(tileCoord), tileDrawRegion,
                        aFlags)) {
        gfxCriticalError() << "ValidateTile failed";
      }

      // Validating the tile may have required more to be painted.
      paintRegion.OrWith(tileDrawRegion);
      dirtyRegion.OrWith(tileDrawRegion);
    }

    if (!mPaintTiles.IsEmpty()) {
      // Create a tiled draw target
      gfx::TileSet tileset;
      for (size_t i = 0; i < mPaintTiles.Length(); ++i) {
        mPaintTiles[i].mTileOrigin -= mTilingOrigin;
      }
      tileset.mTiles = mPaintTiles.Elements();
      tileset.mTileCount = mPaintTiles.Length();
      RefPtr<DrawTarget> drawTarget =
          gfx::Factory::CreateTiledDrawTarget(tileset);
      if (!drawTarget || !drawTarget->IsValid()) {
        gfxDevCrash(LogReason::InvalidContext) << "Invalid tiled draw target";
        return;
      }
      drawTarget->SetTransform(Matrix());

      // Draw into the tiled draw target
      RefPtr<gfxContext> ctx = gfxContext::CreateOrNull(drawTarget);
      MOZ_ASSERT(ctx);  // already checked the draw target above
      ctx->SetMatrix(ctx->CurrentMatrix()
                         .PreScale(mResolution, mResolution)
                         .PreTranslate(-mTilingOrigin));

      mCallback(&mPaintedLayer, ctx, paintRegion, dirtyRegion,
                DrawRegionClip::DRAW, nsIntRegion(), mCallbackData);
      ctx = nullptr;

      // Edge padding allows us to avoid resampling artifacts
      if (StaticPrefs::layers_tiles_edge_padding_AtStartup() &&
          mResolution == 1) {
        drawTarget->PadEdges(newValidRegion.MovedBy(-mTilingOrigin));
      }

      // Reset
      mPaintTiles.Clear();
      mTilingOrigin = IntPoint(std::numeric_limits<int32_t>::max(),
                               std::numeric_limits<int32_t>::max());
    }

    // Dispatch to the paint thread
    if (aFlags & TilePaintFlags::Async) {
      bool queuedTask = false;

      while (!mPaintTasks.IsEmpty()) {
        UniquePtr<PaintTask> task = mPaintTasks.PopLastElement();
        if (!task->mCapture->IsEmpty()) {
          PaintThread::Get()->QueuePaintTask(std::move(task));
          queuedTask = true;
        }
      }

      if (queuedTask) {
        mManager->SetQueuedAsyncPaints();
      }

      mPaintTasks.Clear();
    }

    for (uint32_t i = 0; i < mRetainedTiles.Length(); ++i) {
      TileClient& tile = mRetainedTiles[i];
      UnlockTile(tile);
    }
  }

  mTiles = newTiles;
  mValidRegion = newValidRegion;
}

bool ClientMultiTiledLayerBuffer::ValidateTile(TileClient& aTile,
                                               const nsIntPoint& aTileOrigin,
                                               nsIntRegion& aDirtyRegion,
                                               TilePaintFlags aFlags) {
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (aDirtyRegion.IsComplex()) {
    printf_stderr("Complex region\n");
  }
#endif

  SurfaceMode mode;
  gfxContentType content = GetContentType(&mode);

  if (!aTile.mAllocator) {
    aTile.SetTextureAllocator(
        mManager->GetCompositorBridgeChild()->GetTexturePool(
            mManager->AsShadowForwarder(),
            gfxPlatform::GetPlatform()->Optimal2DFormatForContent(content),
            TextureFlags::DISALLOW_BIGIMAGE | TextureFlags::IMMEDIATE_UPLOAD |
                TextureFlags::NON_BLOCKING_READ_LOCK));
    MOZ_ASSERT(aTile.mAllocator);
  }

  nsIntRegion tileDirtyRegion = aDirtyRegion.MovedBy(-aTileOrigin);
  tileDirtyRegion.ScaleRoundOut(mResolution, mResolution);

  nsIntRegion tileVisibleRegion = mNewValidRegion.MovedBy(-aTileOrigin);
  tileVisibleRegion.ScaleRoundOut(mResolution, mResolution);

  std::vector<RefPtr<TextureClient>> asyncPaintClients;

  Maybe<AcquiredBackBuffer> backBuffer =
      aTile.AcquireBackBuffer(mCompositableClient, tileDirtyRegion,
                              tileVisibleRegion, content, mode, aFlags);

  if (!backBuffer) {
    return false;
  }

  // Mark the area we need to paint in the back buffer as invalid in the
  // front buffer as they will become out of sync.
  aTile.mInvalidFront.OrWith(tileDirtyRegion);

  // Add the backbuffer's invalid region intersected with the visible region to
  // the dirty region we will be painting. This will be empty if we are able to
  // copy from the front into the back.
  nsIntRegion tileInvalidRegion = aTile.mInvalidBack;
  tileInvalidRegion.AndWith(tileVisibleRegion);

  nsIntRegion invalidRegion = tileInvalidRegion;
  invalidRegion.MoveBy(aTileOrigin);
  invalidRegion.ScaleInverseRoundOut(mResolution, mResolution);

  tileDirtyRegion.OrWith(tileInvalidRegion);
  aDirtyRegion.OrWith(invalidRegion);

  // Mark the region we will be painting and the region we copied from the front
  // buffer as needing to be uploaded to the compositor
  aTile.mUpdateRect =
      tileDirtyRegion.GetBounds().Union(backBuffer->mUpdatedRect);

  // We need to clear the dirty region of the tile before painting
  // if we are painting non-opaque content
  if (mode != SurfaceMode::SURFACE_OPAQUE) {
    for (auto iter = tileDirtyRegion.RectIter(); !iter.Done(); iter.Next()) {
      const gfx::Rect drawRect(iter.Get().X(), iter.Get().Y(),
                               iter.Get().Width(), iter.Get().Height());
      backBuffer->mTarget->ClearRect(drawRect);
    }
  }

  gfx::Tile paintTile;
  paintTile.mTileOrigin = gfx::IntPoint(aTileOrigin.x, aTileOrigin.y);
  paintTile.mDrawTarget = backBuffer->mTarget;
  mPaintTiles.AppendElement(paintTile);

  if (aFlags & TilePaintFlags::Async) {
    UniquePtr<PaintTask> task(new PaintTask());
    task->mCapture = backBuffer->mCapture;
    task->mTarget = backBuffer->mBackBuffer;
    task->mClients = std::move(backBuffer->mTextureClients);
    mPaintTasks.AppendElement(std::move(task));
  } else {
    MOZ_RELEASE_ASSERT(backBuffer->mTarget == backBuffer->mBackBuffer);
    MOZ_RELEASE_ASSERT(backBuffer->mCapture == nullptr);
  }

  mTilingOrigin.x = std::min(mTilingOrigin.x, paintTile.mTileOrigin.x);
  mTilingOrigin.y = std::min(mTilingOrigin.y, paintTile.mTileOrigin.y);

  // The new buffer is now validated, remove the dirty region from it.
  aTile.mInvalidBack.SubOut(tileDirtyRegion);

  aTile.Flip();

  return true;
}

/**
 * This function takes the transform stored in aTransformToCompBounds
 * (which was generated in GetTransformToAncestorsParentLayer), and
 * modifies it with the ViewTransform from the compositor side so that
 * it reflects what the compositor is actually rendering. This operation
 * basically adds in the layer's async transform.
 * This function then returns the scroll ancestor's composition bounds,
 * transformed into the painted layer's LayerPixel coordinates, accounting
 * for the compositor state.
 */
static Maybe<LayerRect> GetCompositorSideCompositionBounds(
    const LayerMetricsWrapper& aScrollAncestor,
    const LayerToParentLayerMatrix4x4& aTransformToCompBounds,
    const AsyncTransform& aAPZTransform, const LayerRect& aClip) {
  LayerToParentLayerMatrix4x4 transform =
      aTransformToCompBounds * AsyncTransformComponentMatrix(aAPZTransform);

  return UntransformBy(transform.Inverse(),
                       aScrollAncestor.Metrics().GetCompositionBounds(), aClip);
}

bool ClientMultiTiledLayerBuffer::ComputeProgressiveUpdateRegion(
    const nsIntRegion& aInvalidRegion, const nsIntRegion& aOldValidRegion,
    nsIntRegion& aRegionToPaint, BasicTiledLayerPaintData* aPaintData,
    bool aIsRepeated) {
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

  TILING_LOG("TILING %p: Progressive update stale region %s\n", &mPaintedLayer,
             Stringify(staleRegion).c_str());

  LayerMetricsWrapper scrollAncestor;
  mPaintedLayer.GetAncestorLayers(&scrollAncestor, nullptr, nullptr);

  // Find out the current view transform to determine which tiles to draw
  // first, and see if we should just abort this paint. Aborting is usually
  // caused by there being an incoming, more relevant paint.
  AsyncTransform viewTransform;
  MOZ_ASSERT(mSharedFrameMetricsHelper);

  bool abortPaint = mSharedFrameMetricsHelper->UpdateFromCompositorFrameMetrics(
      scrollAncestor, !staleRegion.Contains(aInvalidRegion),
      drawingLowPrecision, viewTransform);

  TILING_LOG(
      "TILING %p: Progressive update view transform %s zoom %f abort %d\n",
      &mPaintedLayer, ToString(viewTransform.mTranslation).c_str(),
      viewTransform.mScale.scale, abortPaint);

  if (abortPaint) {
    // We ignore if front-end wants to abort if this is the first,
    // non-low-precision paint, as in that situation, we're about to override
    // front-end's page/viewport metrics.
    if (!aPaintData->mFirstPaint || drawingLowPrecision) {
      AUTO_PROFILER_LABEL(
          "ClientMultiTiledLayerBuffer::ComputeProgressiveUpdateRegion",
          GRAPHICS);

      aRegionToPaint.SetEmpty();
      return aIsRepeated;
    }
  }

  Maybe<LayerRect> transformedCompositionBounds =
      GetCompositorSideCompositionBounds(
          scrollAncestor, aPaintData->mTransformToCompBounds, viewTransform,
          LayerRect(mPaintedLayer.GetVisibleRegion().GetBounds()));

  if (!transformedCompositionBounds) {
    aPaintData->mPaintFinished = true;
    return false;
  }

  TILING_LOG("TILING %p: Progressive update transformed compositor bounds %s\n",
             &mPaintedLayer, Stringify(*transformedCompositionBounds).c_str());

  // Compute a "coherent update rect" that we should paint all at once in a
  // single transaction. This is to avoid rendering glitches on animated
  // page content, and when layers change size/shape.
  // On Fennec uploads are more expensive because we're not using gralloc, so
  // we use a coherent update rect that is intersected with the screen at the
  // time of issuing the draw command. This will paint faster but also
  // potentially make the progressive paint more visible to the user while
  // scrolling.
  IntRect coherentUpdateRect(RoundedOut(
#ifdef MOZ_WIDGET_ANDROID
                                 transformedCompositionBounds->Intersect(
                                     aPaintData->mCompositionBounds)
#else
                                 *transformedCompositionBounds
#endif
                                     )
                                 .ToUnknownRect());

  TILING_LOG("TILING %p: Progressive update final coherency rect %s\n",
             &mPaintedLayer, Stringify(coherentUpdateRect).c_str());

  aRegionToPaint.And(aInvalidRegion, coherentUpdateRect);
  aRegionToPaint.Or(aRegionToPaint, staleRegion);
  bool drawingStale = !aRegionToPaint.IsEmpty();
  if (!drawingStale) {
    aRegionToPaint = aInvalidRegion;
  }

  // Prioritise tiles that are currently visible on the screen.
  bool paintingVisible = false;
  if (aRegionToPaint.Intersects(coherentUpdateRect)) {
    aRegionToPaint.And(aRegionToPaint, coherentUpdateRect);
    paintingVisible = true;
  }

  TILING_LOG("TILING %p: Progressive update final paint region %s\n",
             &mPaintedLayer, Stringify(aRegionToPaint).c_str());

  // Paint area that's visible and overlaps previously valid content to avoid
  // visible glitches in animated elements, such as gifs.
  bool paintInSingleTransaction =
      paintingVisible && (drawingStale || aPaintData->mFirstPaint);

  TILING_LOG(
      "TILING %p: paintingVisible %d drawingStale %d firstPaint %d "
      "singleTransaction %d\n",
      &mPaintedLayer, paintingVisible, drawingStale, aPaintData->mFirstPaint,
      paintInSingleTransaction);

  // The following code decides what order to draw tiles in, based on the
  // current scroll direction of the primary scrollable layer.
  NS_ASSERTION(!aRegionToPaint.IsEmpty(), "Unexpectedly empty paint region!");
  IntRect paintBounds = aRegionToPaint.GetBounds();

  int startX, incX, startY, incY;
  gfx::IntSize scaledTileSize = GetScaledTileSize();
  if (aPaintData->mScrollOffset.x >= aPaintData->mLastScrollOffset.x) {
    startX = RoundDownToTileEdge(paintBounds.X(), scaledTileSize.width);
    incX = scaledTileSize.width;
  } else {
    startX = RoundDownToTileEdge(paintBounds.XMost() - 1, scaledTileSize.width);
    incX = -scaledTileSize.width;
  }

  if (aPaintData->mScrollOffset.y >= aPaintData->mLastScrollOffset.y) {
    startY = RoundDownToTileEdge(paintBounds.Y(), scaledTileSize.height);
    incY = scaledTileSize.height;
  } else {
    startY =
        RoundDownToTileEdge(paintBounds.YMost() - 1, scaledTileSize.height);
    incY = -scaledTileSize.height;
  }

  // Find a tile to draw.
  IntRect tileBounds(startX, startY, scaledTileSize.width,
                     scaledTileSize.height);
  int32_t scrollDiffX =
      aPaintData->mScrollOffset.x - aPaintData->mLastScrollOffset.x;
  int32_t scrollDiffY =
      aPaintData->mScrollOffset.y - aPaintData->mLastScrollOffset.y;
  // This loop will always terminate, as there is at least one tile area
  // along the first/last row/column intersecting with regionToPaint, or its
  // bounds would have been smaller.
  while (true) {
    aRegionToPaint.And(aInvalidRegion, tileBounds);
    if (!aRegionToPaint.IsEmpty()) {
      if (mResolution != 1) {
        // Paint the entire tile for low-res. This is aimed to fixing low-res
        // resampling and to avoid doing costly region accurate painting for a
        // small area.
        aRegionToPaint = tileBounds;
      }
      break;
    }
    if (Abs(scrollDiffY) >= Abs(scrollDiffX)) {
      tileBounds.MoveByX(incX);
    } else {
      tileBounds.MoveByY(incY);
    }
  }

  if (!aRegionToPaint.Contains(aInvalidRegion)) {
    // The region needed to paint is larger then our progressive chunk size
    // therefore update what we want to paint and ask for a new paint
    // transaction.

    // If we need to draw more than one tile to maintain coherency, make
    // sure it happens in the same transaction by requesting this work be
    // repeated immediately.
    // If this is unnecessary, the remaining work will be done tile-by-tile in
    // subsequent transactions. The caller code is responsible for scheduling
    // the subsequent transactions as long as we don't set the mPaintFinished
    // flag to true.
    return (!drawingLowPrecision && paintInSingleTransaction);
  }

  // We're not repeating painting and we've not requested a repeat transaction,
  // so the paint is finished. If there's still a separate low precision
  // paint to do, it will get marked as unfinished later.
  aPaintData->mPaintFinished = true;
  return false;
}

bool ClientMultiTiledLayerBuffer::ProgressiveUpdate(
    const nsIntRegion& aValidRegion, const nsIntRegion& aInvalidRegion,
    const nsIntRegion& aOldValidRegion, nsIntRegion& aOutDrawnRegion,
    BasicTiledLayerPaintData* aPaintData,
    LayerManager::DrawPaintedLayerCallback aCallback, void* aCallbackData) {
  TILING_LOG("TILING %p: Progressive update valid region %s\n", &mPaintedLayer,
             Stringify(aValidRegion).c_str());
  TILING_LOG("TILING %p: Progressive update invalid region %s\n",
             &mPaintedLayer, Stringify(aInvalidRegion).c_str());
  TILING_LOG("TILING %p: Progressive update old valid region %s\n",
             &mPaintedLayer, Stringify(aOldValidRegion).c_str());

  bool repeat = false;
  bool isBufferChanged = false;
  nsIntRegion remainingInvalidRegion = aInvalidRegion;
  nsIntRegion updatedValidRegion = aValidRegion;
  do {
    // Compute the region that should be updated. Repeat as many times as
    // is required.
    nsIntRegion regionToPaint;
    repeat =
        ComputeProgressiveUpdateRegion(remainingInvalidRegion, aOldValidRegion,
                                       regionToPaint, aPaintData, repeat);

    TILING_LOG(
        "TILING %p: Progressive update computed paint region %s repeat %d\n",
        &mPaintedLayer, Stringify(regionToPaint).c_str(), repeat);

    // There's no further work to be done.
    if (regionToPaint.IsEmpty()) {
      break;
    }

    isBufferChanged = true;

    // Keep track of what we're about to refresh.
    aOutDrawnRegion.OrWith(regionToPaint);
    updatedValidRegion.OrWith(regionToPaint);

    // aValidRegion may have been altered by InvalidateRegion, but we still
    // want to display stale content until it gets progressively updated.
    // Create a region that includes stale content.
    nsIntRegion validOrStale;
    validOrStale.Or(updatedValidRegion, aOldValidRegion);

    // Paint the computed region and subtract it from the invalid region.
    PaintThebes(validOrStale, regionToPaint, remainingInvalidRegion, aCallback,
                aCallbackData, TilePaintFlags::Progressive);
    remainingInvalidRegion.SubOut(regionToPaint);
  } while (repeat);

  TILING_LOG(
      "TILING %p: Progressive update final valid region %s buffer changed %d\n",
      &mPaintedLayer, Stringify(updatedValidRegion).c_str(), isBufferChanged);
  TILING_LOG("TILING %p: Progressive update final invalid region %s\n",
             &mPaintedLayer, Stringify(remainingInvalidRegion).c_str());

  // Return false if nothing has been drawn, or give what has been drawn
  // to the shadow layer to upload.
  return isBufferChanged;
}

}  // namespace layers
}  // namespace mozilla
