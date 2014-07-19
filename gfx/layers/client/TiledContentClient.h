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
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/ipc/Shmem.h"          // for Shmem
#include "mozilla/ipc/SharedMemory.h"   // for SharedMemory
#include "mozilla/layers/AsyncCompositionManager.h"  // for ViewTransform
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient
#include "mozilla/layers/CompositorTypes.h"  // for TextureInfo, etc
#include "mozilla/layers/LayersMessages.h" // for TileDescriptor
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureClientPool.h"
#include "ClientLayerManager.h"
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsISupportsImpl.h"            // for MOZ_COUNT_DTOR
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl, etc
#include "mozilla/layers/ISurfaceAllocator.h"
#include "gfxReusableSurfaceWrapper.h"
#include "pratom.h"                     // For PR_ATOMIC_INCREMENT/DECREMENT
#include "gfxPrefs.h"

namespace mozilla {
namespace layers {

class BasicTileDescriptor;
class ClientTiledThebesLayer;
class ClientLayerManager;


// A class to help implement copy-on-write semantics for shared tiles.
class gfxSharedReadLock {
protected:
  virtual ~gfxSharedReadLock() {}

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(gfxSharedReadLock)

  virtual int32_t ReadLock() = 0;
  virtual int32_t ReadUnlock() = 0;
  virtual int32_t GetReadCount() = 0;
  virtual bool IsValid() const = 0;

  enum gfxSharedReadLockType {
    TYPE_MEMORY,
    TYPE_SHMEM
  };
  virtual gfxSharedReadLockType GetType() = 0;

protected:
  NS_DECL_OWNINGTHREAD
};

class gfxMemorySharedReadLock : public gfxSharedReadLock {
public:
  gfxMemorySharedReadLock();

  ~gfxMemorySharedReadLock();

  virtual int32_t ReadLock() MOZ_OVERRIDE;

  virtual int32_t ReadUnlock() MOZ_OVERRIDE;

  virtual int32_t GetReadCount() MOZ_OVERRIDE;

  virtual gfxSharedReadLockType GetType() MOZ_OVERRIDE { return TYPE_MEMORY; }

  virtual bool IsValid() const MOZ_OVERRIDE { return true; };

private:
  int32_t mReadCount;
};

class gfxShmSharedReadLock : public gfxSharedReadLock {
private:
  struct ShmReadLockInfo {
    int32_t readCount;
  };

public:
  gfxShmSharedReadLock(ISurfaceAllocator* aAllocator);

  ~gfxShmSharedReadLock();

  virtual int32_t ReadLock() MOZ_OVERRIDE;

  virtual int32_t ReadUnlock() MOZ_OVERRIDE;

  virtual int32_t GetReadCount() MOZ_OVERRIDE;

  virtual bool IsValid() const MOZ_OVERRIDE { return mAllocSuccess; };

  virtual gfxSharedReadLockType GetType() MOZ_OVERRIDE { return TYPE_SHMEM; }

  mozilla::layers::ShmemSection& GetShmemSection() { return mShmemSection; }

  static already_AddRefed<gfxShmSharedReadLock>
  Open(mozilla::layers::ISurfaceAllocator* aAllocator, const mozilla::layers::ShmemSection& aShmemSection)
  {
    nsRefPtr<gfxShmSharedReadLock> readLock = new gfxShmSharedReadLock(aAllocator, aShmemSection);
    return readLock.forget();
  }

private:
  gfxShmSharedReadLock(ISurfaceAllocator* aAllocator, const mozilla::layers::ShmemSection& aShmemSection)
    : mAllocator(aAllocator)
    , mShmemSection(aShmemSection)
    , mAllocSuccess(true)
  {
    MOZ_COUNT_CTOR(gfxShmSharedReadLock);
  }

  ShmReadLockInfo* GetShmReadLockInfoPtr()
  {
    return reinterpret_cast<ShmReadLockInfo*>
      (mShmemSection.shmem().get<char>() + mShmemSection.offset());
  }

  RefPtr<ISurfaceAllocator> mAllocator;
  mozilla::layers::ShmemSection mShmemSection;
  bool mAllocSuccess;
};

/**
 * Represent a single tile in tiled buffer. The buffer keeps tiles,
 * each tile keeps a reference to a texture client and a read-lock. This
 * read-lock is used to help implement a copy-on-write mechanism. The tile
 * should be locked before being sent to the compositor. The compositor should
 * unlock the read-lock as soon as it has finished with the buffer in the
 * TextureHost to prevent more textures being created than is necessary.
 * Ideal place to store per tile debug information.
 */
struct TileClient
{
  // Placeholder
  TileClient();

  TileClient(const TileClient& o);

  TileClient& operator=(const TileClient& o);

  bool operator== (const TileClient& o) const
  {
    return mFrontBuffer == o.mFrontBuffer;
  }

  bool operator!= (const TileClient& o) const
  {
    return mFrontBuffer != o.mFrontBuffer;
  }

  void SetLayerManager(ClientLayerManager *aManager)
  {
    mManager = aManager;
  }

  bool IsPlaceholderTile()
  {
    return mBackBuffer == nullptr && mFrontBuffer == nullptr;
  }

  void ReadUnlock()
  {
    MOZ_ASSERT(mFrontLock, "ReadLock with no gfxSharedReadLock");
    if (mFrontLock) {
      mFrontLock->ReadUnlock();
    }
  }

  void ReadLock()
  {
    MOZ_ASSERT(mFrontLock, "ReadLock with no gfxSharedReadLock");
    if (mFrontLock) {
      mFrontLock->ReadLock();
    }
  }

  void Release()
  {
    DiscardFrontBuffer();
    DiscardBackBuffer();
  }

  TileDescriptor GetTileDescriptor();

  /**
  * Swaps the front and back buffers.
  */
  void Flip();

  /**
  * Returns an unlocked TextureClient that can be used for writing new
  * data to the tile. This may flip the front-buffer to the back-buffer if
  * the front-buffer is still locked by the host, or does not have an
  * internal buffer (and so will always be locked).
  */
  TextureClient* GetBackBuffer(const nsIntRegion& aDirtyRegion,
                               TextureClientPool *aPool,
                               bool *aCreatedTextureClient,
                               bool aCanRerasterizeValidRegion);

  void DiscardFrontBuffer();

  void DiscardBackBuffer();

  RefPtr<TextureClient> mBackBuffer;
  RefPtr<TextureClient> mFrontBuffer;
  RefPtr<gfxSharedReadLock> mBackLock;
  RefPtr<gfxSharedReadLock> mFrontLock;
  RefPtr<ClientLayerManager> mManager;
#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
  TimeStamp        mLastUpdate;
#endif
  nsIntRegion mInvalidFront;
  nsIntRegion mInvalidBack;

private:
  void ValidateBackBufferFromFront(const nsIntRegion &aDirtyRegion,
                                   bool aCanRerasterizeValidRegion);
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
  ParentLayerPoint mScrollOffset;

  /*
   * The scroll offset of the content from the nearest ancestor layer that
   * represents scrollable content with a display port set, for the last
   * layer update transaction.
   */
  ParentLayerPoint mLastScrollOffset;

  /*
   * The transform matrix to go from this layer's Layer units to
   * the scroll ancestor's ParentLayer units. The "scroll ancestor" is
   * the closest ancestor layer which scrolls, and is used to obtain
   * the composition bounds that are relevant for this layer.
   */
  gfx3DMatrix mTransformToCompBounds;

  /*
   * The critical displayport of the content from the nearest ancestor layer
   * that represents scrollable content with a display port set. Empty if a
   * critical displayport is not set.
   */
  LayerIntRect mCriticalDisplayPort;

  /*
   * The render resolution of the document that the content this layer
   * represents is in.
   */
  CSSToParentLayerScale mResolution;

  /*
   * The composition bounds of the layer, in Layer coordinates. This is
   * used to make sure that tiled updates to regions that are visible to the
   * user are grouped coherently.
   */
  LayerRect mCompositionBounds;

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

class SharedFrameMetricsHelper
{
public:
  SharedFrameMetricsHelper();
  ~SharedFrameMetricsHelper();

  /**
   * This is called by the BasicTileLayer to determine if it is still interested
   * in the update of this display-port to continue. We can return true here
   * to abort the current update and continue with any subsequent ones. This
   * is useful for slow-to-render pages when the display-port starts lagging
   * behind enough that continuing to draw it is wasted effort.
   */
  bool UpdateFromCompositorFrameMetrics(ContainerLayer* aLayer,
                                        bool aHasPendingNewThebesContent,
                                        bool aLowPrecision,
                                        ViewTransform& aViewTransform);

  /**
   * Determines if the compositor's upcoming composition bounds has fallen
   * outside of the contents display port. If it has then the compositor
   * will start to checker board. Checker boarding is when the compositor
   * tries to composite a tile and it is not available. Historically
   * a tile with a checker board pattern was used. Now a blank tile is used.
   */
  bool AboutToCheckerboard(const FrameMetrics& aContentMetrics,
                           const FrameMetrics& aCompositorMetrics);
private:
  bool mLastProgressiveUpdateWasLowPrecision;
  bool mProgressiveUpdateWasInDanger;
};

/**
 * Provide an instance of TiledLayerBuffer backed by drawable TextureClients.
 * This buffer provides an implementation of ValidateTile using a
 * thebes callback and can support painting using a single paint buffer.
 * Whether a single paint buffer is used is controlled by
 * gfxPrefs::PerTileDrawing().
 */
class ClientTiledLayerBuffer
  : public TiledLayerBuffer<ClientTiledLayerBuffer, TileClient>
{
  friend class TiledLayerBuffer<ClientTiledLayerBuffer, TileClient>;

public:
  ClientTiledLayerBuffer(ClientTiledThebesLayer* aThebesLayer,
                         CompositableClient* aCompositableClient,
                         ClientLayerManager* aManager,
                         SharedFrameMetricsHelper* aHelper);
  ClientTiledLayerBuffer()
    : mThebesLayer(nullptr)
    , mCompositableClient(nullptr)
    , mManager(nullptr)
    , mLastPaintOpaque(false)
    , mSharedFrameMetricsHelper(nullptr)
  {}

  void PaintThebes(const nsIntRegion& aNewValidRegion,
                   const nsIntRegion& aPaintRegion,
                   LayerManager::DrawThebesLayerCallback aCallback,
                   void* aCallbackData);

  void ReadUnlock();

  void ReadLock();

  void Release();

  void DiscardBuffers();

  const CSSToParentLayerScale& GetFrameResolution() { return mFrameResolution; }

  void SetFrameResolution(const CSSToParentLayerScale& aResolution) { mFrameResolution = aResolution; }

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

protected:
  TileClient ValidateTile(TileClient aTile,
                          const nsIntPoint& aTileRect,
                          const nsIntRegion& dirtyRect);

  // If this returns true, we perform the paint operation into a single large
  // buffer and copy it out to the tiles instead of calling PaintThebes() on
  // each tile individually. Somewhat surprisingly, this turns out to be faster
  // on Android.
  bool UseSinglePaintBuffer() { return !gfxPrefs::PerTileDrawing(); }

  void ReleaseTile(TileClient aTile) { aTile.Release(); }

  void SwapTiles(TileClient& aTileA, TileClient& aTileB) { std::swap(aTileA, aTileB); }

  TileClient GetPlaceholderTile() const { return TileClient(); }

private:
  gfxContentType GetContentType() const;
  ClientTiledThebesLayer* mThebesLayer;
  CompositableClient* mCompositableClient;
  ClientLayerManager* mManager;
  LayerManager::DrawThebesLayerCallback mCallback;
  void* mCallbackData;
  CSSToParentLayerScale mFrameResolution;
  bool mLastPaintOpaque;

  // The DrawTarget we use when UseSinglePaintBuffer() above is true.
  RefPtr<gfx::DrawTarget>       mSinglePaintDrawTarget;
  nsIntPoint                    mSinglePaintBufferOffset;
  SharedFrameMetricsHelper*  mSharedFrameMetricsHelper;

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

    mTiledBuffer.Release();
    mLowPrecisionTiledBuffer.Release();
  }

  virtual TextureInfo GetTextureInfo() const MOZ_OVERRIDE
  {
    return TextureInfo(CompositableType::BUFFER_TILED);
  }

  virtual void ClearCachedResources() MOZ_OVERRIDE;

  enum TiledBufferType {
    TILED_BUFFER,
    LOW_PRECISION_TILED_BUFFER
  };
  void UseTiledLayerBuffer(TiledBufferType aType);

private:
  SharedFrameMetricsHelper mSharedFrameMetricsHelper;
  ClientTiledLayerBuffer mTiledBuffer;
  ClientTiledLayerBuffer mLowPrecisionTiledBuffer;
};

}
}

#endif
