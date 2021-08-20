/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_MULTITILEDCONTENTCLIENT_H
#define MOZILLA_GFX_MULTITILEDCONTENTCLIENT_H

#include "ClientLayerManager.h"                 // for ClientLayerManager
#include "nsRegion.h"                           // for nsIntRegion
#include "mozilla/gfx/2D.h"                     // for gfx::Tile
#include "mozilla/gfx/Point.h"                  // for IntPoint
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient
#include "mozilla/layers/LayersMessages.h"      // for TileDescriptor
#include "mozilla/layers/TiledContentClient.h"  // for ClientTiledPaintedLayer
#include "mozilla/UniquePtr.h"                  // for UniquePtr
#include "TiledLayerBuffer.h"                   // for TiledLayerBuffer

namespace mozilla {
namespace layers {

class ClientLayerManager;

class ClientMultiTiledLayerBuffer
    : public TiledLayerBuffer<ClientMultiTiledLayerBuffer, TileClient>,
      public ClientTiledLayerBuffer {
  friend class TiledLayerBuffer<ClientMultiTiledLayerBuffer, TileClient>;

 public:
  ClientMultiTiledLayerBuffer(ClientTiledPaintedLayer& aPaintedLayer,
                              CompositableClient& aCompositableClient,
                              ClientLayerManager* aManager,
                              SharedFrameMetricsHelper* aHelper);

  void PaintThebes(const nsIntRegion& aNewValidRegion,
                   const nsIntRegion& aPaintRegion,
                   const nsIntRegion& aDirtyRegion,
                   LayerManager::DrawPaintedLayerCallback aCallback,
                   void* aCallbackData,
                   TilePaintFlags aFlags = TilePaintFlags::None) override;

  bool SupportsProgressiveUpdate() override { return true; }
  /**
   * Performs a progressive update of a given tiled buffer.
   * See ComputeProgressiveUpdateRegion below for parameter documentation.
   * aOutDrawnRegion is an outparameter that contains the region that was
   * drawn, and which can now be added to the layer's valid region.
   */
  bool ProgressiveUpdate(const nsIntRegion& aValidRegion,
                         const nsIntRegion& aInvalidRegion,
                         const nsIntRegion& aOldValidRegion,
                         nsIntRegion& aOutDrawnRegion,
                         BasicTiledLayerPaintData* aPaintData,
                         LayerManager::DrawPaintedLayerCallback aCallback,
                         void* aCallbackData) override;

  void ResetPaintedAndValidState() override {
    mValidRegion.SetEmpty();
    mTiles.mSize.width = 0;
    mTiles.mSize.height = 0;
    DiscardBuffers();
    mRetainedTiles.Clear();
  }

  const nsIntRegion& GetValidRegion() override {
    return TiledLayerBuffer::GetValidRegion();
  }

  bool IsLowPrecision() const override {
    return TiledLayerBuffer::IsLowPrecision();
  }

  void Dump(std::stringstream& aStream, const char* aPrefix, bool aDumpHtml,
            TextureDumpMode aCompress) override {
    TiledLayerBuffer::Dump(aStream, aPrefix, aDumpHtml, aCompress);
  }

  void ReadLock();

  void Release();

  void DiscardBuffers();

  SurfaceDescriptorTiles GetSurfaceDescriptorTiles();

  void SetResolution(float aResolution) {
    if (mResolution == aResolution) {
      return;
    }

    Update(nsIntRegion(), nsIntRegion(), nsIntRegion(), TilePaintFlags::None);
    mResolution = aResolution;
  }

 protected:
  bool ValidateTile(TileClient& aTile, const nsIntPoint& aTileRect,
                    nsIntRegion& aDirtyRegion, TilePaintFlags aFlags);

  void Update(const nsIntRegion& aNewValidRegion,
              const nsIntRegion& aPaintRegion, const nsIntRegion& aDirtyRegion,
              TilePaintFlags aFlags);

  TileClient GetPlaceholderTile() const { return TileClient(); }

 private:
  RefPtr<ClientLayerManager> mManager;
  LayerManager::DrawPaintedLayerCallback mCallback;
  void* mCallbackData;

  // The region that will be made valid during Update(). Once Update() is
  // completed then this is identical to mValidRegion.
  nsIntRegion mNewValidRegion;

  SharedFrameMetricsHelper* mSharedFrameMetricsHelper;

  // Parameters that are collected during Update for a paint before they
  // are either executed or replayed on the paint thread.
  AutoTArray<gfx::Tile, 4> mPaintTiles;

  /**
   * While we're adding tiles, this is used to keep track of the position of
   * the top-left of the top-left-most tile.  When we come to wrap the tiles in
   * TiledDrawTarget we subtract the value of this member from each tile's
   * offset so that all the tiles have a positive offset, then add a
   * translation to the TiledDrawTarget to compensate.  This is important so
   * that the mRect of the TiledDrawTarget is always at a positive x/y
   * position, otherwise its GetSize() methods will be broken.
   */
  gfx::IntPoint mTilingOrigin;
  /**
   * Calculates the region to update in a single progressive update transaction.
   * This employs some heuristics to update the most 'sensible' region to
   * update at this point in time, and how large an update should be performed
   * at once to maintain visual coherency.
   *
   * aInvalidRegion is the current invalid region.
   * aOldValidRegion is the valid region of mTiledBuffer at the beginning of the
   * current transaction.
   * aRegionToPaint will be filled with the region to update. This may be empty,
   * which indicates that there is no more work to do.
   * aIsRepeated should be true if this function has already been called during
   * this transaction.
   *
   * Returns true if it should be called again, false otherwise. In the case
   * that aRegionToPaint is empty, this will return aIsRepeated for convenience.
   */
  bool ComputeProgressiveUpdateRegion(const nsIntRegion& aInvalidRegion,
                                      const nsIntRegion& aOldValidRegion,
                                      nsIntRegion& aRegionToPaint,
                                      BasicTiledLayerPaintData* aPaintData,
                                      bool aIsRepeated);

  void MaybeSyncTextures(const nsIntRegion& aPaintRegion,
                         const TilesPlacement& aNewTiles,
                         const gfx::IntSize& aScaledTileSize);
};

/**
 * An implementation of TiledContentClient that supports
 * multiple tiles and a low precision buffer.
 */
class MultiTiledContentClient : public TiledContentClient {
 public:
  MultiTiledContentClient(ClientTiledPaintedLayer& aPaintedLayer,
                          ClientLayerManager* aManager);

 protected:
  ~MultiTiledContentClient() {
    MOZ_COUNT_DTOR(MultiTiledContentClient);

    mTiledBuffer.DiscardBuffers();
    mLowPrecisionTiledBuffer.DiscardBuffers();
  }

 public:
  void ClearCachedResources() override;
  void UpdatedBuffer(TiledBufferType aType) override;

  ClientTiledLayerBuffer* GetTiledBuffer() override { return &mTiledBuffer; }
  ClientTiledLayerBuffer* GetLowPrecisionTiledBuffer() override {
    if (mHasLowPrecision) {
      return &mLowPrecisionTiledBuffer;
    }
    return nullptr;
  }

 private:
  SharedFrameMetricsHelper mSharedFrameMetricsHelper;
  ClientMultiTiledLayerBuffer mTiledBuffer;
  ClientMultiTiledLayerBuffer mLowPrecisionTiledBuffer;
  bool mHasLowPrecision;
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_MULTITILEDCONTENTCLIENT_H
