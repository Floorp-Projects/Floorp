/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TILEDCONTENTCLIENT_H
#define MOZILLA_GFX_TILEDCONTENTCLIENT_H

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint16_t
#include <algorithm>                    // for swap
#include "Layers.h"                     // for LayerManager, etc
#include "TiledLayerBuffer.h"           // for TiledLayerBuffer
#include "Units.h"                      // for CSSPoint
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "gfxTypes.h"
#include "gfxPoint.h"                   // for gfxSize
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient
#include "mozilla/layers/CompositorTypes.h"  // for TextureInfo, etc
#include "mozilla/layers/TextureClient.h"
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl, etc
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_DTOR
#include "mozilla/layers/ISurfaceAllocator.h"
#include "gfxReusableSurfaceWrapper.h"

class gfxImageSurface;

namespace mozilla {
namespace layers {

class BasicTileDescriptor;

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
  RefPtr<DeprecatedTextureClientTile> mDeprecatedTextureClient;
#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
  TimeStamp        mLastUpdate;
#endif

  // Placeholder
  BasicTiledLayerTile()
    : mDeprecatedTextureClient(nullptr)
  {}

  BasicTiledLayerTile(DeprecatedTextureClientTile* aTextureClient)
    : mDeprecatedTextureClient(aTextureClient)
  {}

  BasicTiledLayerTile(const BasicTiledLayerTile& o) {
    mDeprecatedTextureClient = o.mDeprecatedTextureClient;
#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
    mLastUpdate = o.mLastUpdate;
#endif
  }
  BasicTiledLayerTile& operator=(const BasicTiledLayerTile& o) {
    if (this == &o) return *this;
    mDeprecatedTextureClient = o.mDeprecatedTextureClient;
#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
    mLastUpdate = o.mLastUpdate;
#endif
    return *this;
  }
  bool operator== (const BasicTiledLayerTile& o) const {
    return mDeprecatedTextureClient == o.mDeprecatedTextureClient;
  }
  bool operator!= (const BasicTiledLayerTile& o) const {
    return mDeprecatedTextureClient != o.mDeprecatedTextureClient;
  }

  bool IsPlaceholderTile() { return mDeprecatedTextureClient == nullptr; }

  void ReadUnlock() {
    GetSurface()->ReadUnlock();
  }
  void ReadLock() {
    GetSurface()->ReadLock();
  }

  TileDescriptor GetTileDescriptor();
  static BasicTiledLayerTile OpenDescriptor(ISurfaceAllocator *aAllocator, const TileDescriptor& aDesc);

  gfxReusableSurfaceWrapper* GetSurface() {
    return mDeprecatedTextureClient->GetReusableSurfaceWrapper();
  }
};

/**
 * This struct stores all the data necessary to perform a paint so that it
 * doesn't need to be recalculated on every repeated transaction.
 */
struct BasicTiledLayerPaintData {
  /*
   * The scroll offset of the content from the nearest ancestor layer that
   * represents scrollable content with a display port set.
   */
  ScreenPoint mScrollOffset;

  /*
   * The scroll offset of the content from the nearest ancestor layer that
   * represents scrollable content with a display port set, for the last
   * layer update transaction.
   */
  ScreenPoint mLastScrollOffset;

  /*
   * The transform matrix to go from Screen units to transformed LayoutDevice
   * units.
   */
  gfx3DMatrix mTransformScreenToLayout;

  /*
   * The critical displayport of the content from the nearest ancestor layer
   * that represents scrollable content with a display port set. Empty if a
   * critical displayport is not set.
   *
   * This is in transformed LayoutDevice coordinates, but is stored as an
   * nsIntRect for convenience when intersecting with the layer's mValidRegion.
   */
  nsIntRect mLayoutCriticalDisplayPort;

  /*
   * The render resolution of the document that the content this layer
   * represents is in.
   */
  CSSToScreenScale mResolution;

  /*
   * The composition bounds of the primary scrollable layer, in transformed
   * layout device coordinates. This is used to make sure that tiled updates to
   * regions that are visible to the user are grouped coherently.
   */
  LayoutDeviceRect mCompositionBounds;

  /*
   * Low precision updates are always executed a tile at a time in repeated
   * transactions. This counter is set to 1 on the first transaction of a low
   * precision update, and incremented for each subsequent transaction.
   */
  uint16_t mLowPrecisionPaintCount;

  /*
   * Whether this is the first time this layer is painting
   */
  bool mFirstPaint : 1;

  /*
   * Whether there is further work to complete this paint. This is used to
   * determine whether or not to repeat the transaction when painting
   * progressively.
   */
  bool mPaintFinished : 1;
};

class ClientTiledThebesLayer;
class ClientLayerManager;

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
  BasicTiledLayerBuffer(ClientTiledThebesLayer* aThebesLayer,
                        ClientLayerManager* aManager);
  BasicTiledLayerBuffer()
    : mThebesLayer(nullptr)
    , mManager(nullptr)
    , mLastPaintOpaque(false)
  {}

  BasicTiledLayerBuffer(ISurfaceAllocator* aAllocator,
                        const nsIntRegion& aValidRegion,
                        const nsIntRegion& aPaintedRegion,
                        const InfallibleTArray<TileDescriptor>& aTiles,
                        int aRetainedWidth,
                        int aRetainedHeight,
                        float aResolution)
  {
    mValidRegion = aValidRegion;
    mPaintedRegion = aPaintedRegion;
    mRetainedWidth = aRetainedWidth;
    mRetainedHeight = aRetainedHeight;
    mResolution = aResolution;

    for(size_t i = 0; i < aTiles.Length(); i++) {
      if (aTiles[i].type() == TileDescriptor::TPlaceholderTileDescriptor) {
        mRetainedTiles.AppendElement(GetPlaceholderTile());
      } else {
        mRetainedTiles.AppendElement(BasicTiledLayerTile::OpenDescriptor(aAllocator, aTiles[i]));
      }
    }
  }

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

  const CSSToScreenScale& GetFrameResolution() { return mFrameResolution; }
  void SetFrameResolution(const CSSToScreenScale& aResolution) { mFrameResolution = aResolution; }

  bool HasFormatChanged() const;

  /**
   * Performs a progressive update of a given tiled buffer.
   * See ComputeProgressiveUpdateRegion below for parameter documentation.
   */
  bool ProgressiveUpdate(nsIntRegion& aValidRegion,
                         nsIntRegion& aInvalidRegion,
                         const nsIntRegion& aOldValidRegion,
                         BasicTiledLayerPaintData* aPaintData,
                         LayerManager::DrawThebesLayerCallback aCallback,
                         void* aCallbackData);

  SurfaceDescriptorTiles GetSurfaceDescriptorTiles();

  static BasicTiledLayerBuffer OpenDescriptor(ISurfaceAllocator* aAllocator,
                                              const SurfaceDescriptorTiles& aDescriptor);

  void OnActorDestroy()
  {
    for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
      if (mRetainedTiles[i].IsPlaceholderTile()) continue;
      mRetainedTiles[i].mDeprecatedTextureClient->OnActorDestroy();
    }
  }

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
  gfxContentType GetContentType() const;
  ClientTiledThebesLayer* mThebesLayer;
  ClientLayerManager* mManager;
  LayerManager::DrawThebesLayerCallback mCallback;
  void* mCallbackData;
  CSSToScreenScale mFrameResolution;
  bool mLastPaintOpaque;

  // The buffer we use when UseSinglePaintBuffer() above is true.
  nsRefPtr<gfxImageSurface>     mSinglePaintBuffer;
  RefPtr<gfx::DrawTarget>       mSinglePaintDrawTarget;
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
  friend class ClientTiledThebesLayer;

public:
  TiledContentClient(ClientTiledThebesLayer* aThebesLayer,
                     ClientLayerManager* aManager);

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

  virtual void OnActorDestroy() MOZ_OVERRIDE
  {
    mTiledBuffer.OnActorDestroy();
    mLowPrecisionTiledBuffer.OnActorDestroy();
  }

private:
  BasicTiledLayerBuffer mTiledBuffer;
  BasicTiledLayerBuffer mLowPrecisionTiledBuffer;
};

}
}

#endif
