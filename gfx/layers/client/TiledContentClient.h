/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TILEDCONTENTCLIENT_H
#define MOZILLA_GFX_TILEDCONTENTCLIENT_H

#include "mozilla/layers/ContentClient.h"
#include "TiledLayerBuffer.h"
#include "gfxPlatform.h"

namespace mozilla {
namespace layers {

/**
 * Represent a single tile in tiled buffer. The buffer keeps tiles,
 * each tile keeps a reference to a texture client. The texture client
 * is backed by a gfxReusableSurfaceWrapper that implements a
 * copy-on-write mechanism while locked. The tile should be
 * locked before being sent to the compositor and unlocked
 * as soon as it is uploaded to prevent a copy.
 * Ideal place to store per tile debug information.
 */
struct BasicTiledLayerTile {
  RefPtr<TextureClientTile> mTextureClient;
#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
  TimeStamp        mLastUpdate;
#endif

  // Placeholder
  BasicTiledLayerTile()
    : mTextureClient(nullptr)
  {}

  BasicTiledLayerTile(const BasicTiledLayerTile& o) {
    mTextureClient = o.mTextureClient;
#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
    mLastUpdate = o.mLastUpdate;
#endif
  }
  BasicTiledLayerTile& operator=(const BasicTiledLayerTile& o) {
    if (this == &o) return *this;
    mTextureClient = o.mTextureClient;
#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
    mLastUpdate = o.mLastUpdate;
#endif
    return *this;
  }
  bool operator== (const BasicTiledLayerTile& o) const {
    return mTextureClient == o.mTextureClient;
  }
  bool operator!= (const BasicTiledLayerTile& o) const {
    return mTextureClient != o.mTextureClient;
  }

  bool IsPlaceholderTile() { return mTextureClient == nullptr; }

  void ReadUnlock() {
    GetSurface()->ReadUnlock();
  }
  void ReadLock() {
    GetSurface()->ReadLock();
  }

  gfxReusableSurfaceWrapper* GetSurface() {
    return mTextureClient->GetReusableSurfaceWrapper();
  }
};

/**
 * This struct stores all the data necessary to perform a paint so that it
 * doesn't need to be recalculated on every repeated transaction.
 */
struct BasicTiledLayerPaintData {
  gfx::Point mScrollOffset;
  gfx::Point mLastScrollOffset;
  gfx3DMatrix mTransformScreenToLayer;
  nsIntRect mLayerCriticalDisplayPort;
  gfxSize mResolution;
  nsIntRect mCompositionBounds;
  uint16_t mLowPrecisionPaintCount;
  bool mFirstPaint : 1;
  bool mPaintFinished : 1;
};

class BasicTiledThebesLayer;
class BasicShadowLayerManager;

/**
 * Provide an instance of TiledLayerBuffer backed by image surfaces.
 * This buffer provides an implementation to ValidateTile using a
 * thebes callback and can support painting using a single paint buffer
 * which is much faster then painting directly into the tiles.
 */
class BasicTiledLayerBuffer
  : public TiledLayerBuffer<BasicTiledLayerBuffer, BasicTiledLayerTile>
{
  friend class TiledLayerBuffer<BasicTiledLayerBuffer, BasicTiledLayerTile>;

public:
  BasicTiledLayerBuffer(BasicTiledThebesLayer* aThebesLayer,
                        BasicShadowLayerManager* aManager);
  BasicTiledLayerBuffer()
    : mThebesLayer(nullptr)
    , mManager(nullptr)
    , mLastPaintOpaque(false)
  {}

  void PaintThebes(const nsIntRegion& aNewValidRegion,
                   const nsIntRegion& aPaintRegion,
                   LayerManager::DrawThebesLayerCallback aCallback,
                   void* aCallbackData);

  void ReadUnlock() {
    for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
      if (mRetainedTiles[i].IsPlaceholderTile()) continue;
      mRetainedTiles[i].ReadUnlock();
    }
  }

  void ReadLock() {
    for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
      if (mRetainedTiles[i].IsPlaceholderTile()) continue;
      mRetainedTiles[i].ReadLock();
    }
  }

  const gfxSize& GetFrameResolution() { return mFrameResolution; }
  void SetFrameResolution(const gfxSize& aResolution) { mFrameResolution = aResolution; }

  bool HasFormatChanged() const;

  /**
   * Performs a progressive update of a given tiled buffer.
   * See ComputeProgressiveUpdateRegion above for parameter documentation.
   */
  bool ProgressiveUpdate(nsIntRegion& aValidRegion,
                         nsIntRegion& aInvalidRegion,
                         const nsIntRegion& aOldValidRegion,
                         BasicTiledLayerPaintData* aPaintData,
                         LayerManager::DrawThebesLayerCallback aCallback,
                         void* aCallbackData);

  /**
   * Copy this buffer duplicating the texture hosts under the tiles
   * XXX This should go. It is a hack because we need to keep the
   * surface wrappers alive whilst they are locked by the compositor.
   * Once we properly implement the texture host/client architecture
   * for tiled layers we shouldn't need this.
   */
  BasicTiledLayerBuffer DeepCopy() const;

protected:
  BasicTiledLayerTile ValidateTile(BasicTiledLayerTile aTile,
                                   const nsIntPoint& aTileRect,
                                   const nsIntRegion& dirtyRect);

  // If this returns true, we perform the paint operation into a single large
  // buffer and copy it out to the tiles instead of calling PaintThebes() on
  // each tile individually. Somewhat surprisingly, this turns out to be faster
  // on Android.
  bool UseSinglePaintBuffer() { return true; }

  void ReleaseTile(BasicTiledLayerTile aTile) { /* No-op. */ }

  void SwapTiles(BasicTiledLayerTile& aTileA, BasicTiledLayerTile& aTileB) {
    std::swap(aTileA, aTileB);
  }

  BasicTiledLayerTile GetPlaceholderTile() const { return BasicTiledLayerTile(); }

private:
  gfxASurface::gfxContentType GetContentType() const;
  BasicTiledThebesLayer* mThebesLayer;
  BasicShadowLayerManager* mManager;
  LayerManager::DrawThebesLayerCallback mCallback;
  void* mCallbackData;
  gfxSize mFrameResolution;
  bool mLastPaintOpaque;

  // The buffer we use when UseSinglePaintBuffer() above is true.
  nsRefPtr<gfxImageSurface>     mSinglePaintBuffer;
  nsIntPoint                    mSinglePaintBufferOffset;

  BasicTiledLayerTile ValidateTileInternal(BasicTiledLayerTile aTile,
                                           const nsIntPoint& aTileOrigin,
                                           const nsIntRect& aDirtyRect);

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
   * aTransform is the transform required to convert from screen-space to
   * layer-space.
   * aScrollOffset is the current scroll offset of the primary scrollable layer.
   * aResolution is the render resolution of the layer.
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
};

class TiledContentClient : public CompositableClient
{
  // XXX: for now the layer which owns us interacts directly with our buffers.
  // We should have a content client for each tiled buffer which manages its
  // own valid region, resolution, etc. Then we could have a much cleaner
  // interface and tidy up BasicTiledThebesLayer::PaintThebes (bug 862547).
  friend class BasicTiledThebesLayer;

public:
  TiledContentClient(BasicTiledThebesLayer* aThebesLayer,
                     BasicShadowLayerManager* aManager);

  ~TiledContentClient()
  {
    MOZ_COUNT_DTOR(TiledContentClient);
  }

  virtual TextureInfo GetTextureInfo() const MOZ_OVERRIDE
  {
    return TextureInfo(BUFFER_TILED);
  }

  enum TiledBufferType {
    TILED_BUFFER,
    LOW_PRECISION_TILED_BUFFER
  };
  void LockCopyAndWrite(TiledBufferType aType);

private:
  BasicTiledLayerBuffer mTiledBuffer;
  BasicTiledLayerBuffer mLowPrecisionTiledBuffer;
};

}
}

#endif
