/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TILEDCONTENTCLIENT_H
#define MOZILLA_GFX_TILEDCONTENTCLIENT_H

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint16_t
#include <algorithm>                    // for swap
#include <limits>
#include "Layers.h"                     // for LayerManager, etc
#include "TiledLayerBuffer.h"           // for TiledLayerBuffer
#include "Units.h"                      // for CSSPoint
#include "gfxTypes.h"
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/ipc/Shmem.h"          // for Shmem
#include "mozilla/ipc/SharedMemory.h"   // for SharedMemory
#include "mozilla/layers/AsyncCompositionManager.h"  // for ViewTransform
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient
#include "mozilla/layers/CompositorTypes.h"  // for TextureInfo, etc
#include "mozilla/layers/LayersMessages.h" // for TileDescriptor
#include "mozilla/layers/LayersTypes.h" // for TextureDumpMode
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureClientPool.h"
#include "ClientLayerManager.h"
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsISupportsImpl.h"            // for MOZ_COUNT_DTOR
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl, etc
#include "nsExpirationTracker.h"
#include "mozilla/layers/ISurfaceAllocator.h"

namespace mozilla {
namespace layers {

class ClientTiledPaintedLayer;
class ClientLayerManager;

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
  ~TileClient();

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

  void SetTextureAllocator(TextureClientAllocator* aAllocator)
  {
    mAllocator = aAllocator;
  }

  bool IsPlaceholderTile() const
  {
    return mBackBuffer == nullptr && mFrontBuffer == nullptr;
  }

  void DiscardBuffers()
  {
    DiscardFrontBuffer();
    DiscardBackBuffer();
  }

  nsExpirationState *GetExpirationState() { return &mExpirationState; }

  TileDescriptor GetTileDescriptor();

  /**
   * For debugging.
   */
  void Dump(std::stringstream& aStream);

  /**
  * Swaps the front and back buffers.
  */
  void Flip();

  void DumpTexture(std::stringstream& aStream, TextureDumpMode aCompress) {
    // TODO We should combine the OnWhite/OnBlack here an just output a single image.
    CompositableClient::DumpTextureClient(aStream, mFrontBuffer, aCompress);
  }

  /**
  * Returns an unlocked TextureClient that can be used for writing new
  * data to the tile. This may flip the front-buffer to the back-buffer if
  * the front-buffer is still locked by the host, or does not have an
  * internal buffer (and so will always be locked).
  *
  * If getting the back buffer required copying pixels from the front buffer
  * then the copied region is stored in aAddPaintedRegion so the host side
  * knows to upload it.
  *
  * If nullptr is returned, aTextureClientOnWhite is undefined.
  */
  TextureClient* GetBackBuffer(CompositableClient&,
                               const nsIntRegion& aDirtyRegion,
                               gfxContentType aContent, SurfaceMode aMode,
                               nsIntRegion& aAddPaintedRegion,
                               RefPtr<TextureClient>* aTextureClientOnWhite);

  void DiscardFrontBuffer();

  void DiscardBackBuffer();

  /* We wrap the back buffer in a class that disallows assignment
   * so that we can track when ever it changes so that we can update
   * the expiry tracker for expiring the back buffers */
  class PrivateProtector {
    public:
      void Set(TileClient * container, RefPtr<TextureClient>);
      void Set(TileClient * container, TextureClient*);
      // Implicitly convert to TextureClient* because we can't chain
      // implicit conversion that would happen on RefPtr<TextureClient>
      operator TextureClient*() const { return mBuffer; }
      RefPtr<TextureClient> operator ->() { return mBuffer; }
    private:
      PrivateProtector& operator=(const PrivateProtector &);
      RefPtr<TextureClient> mBuffer;
  } mBackBuffer;
  RefPtr<TextureClient> mBackBufferOnWhite;
  RefPtr<TextureClient> mFrontBuffer;
  RefPtr<TextureClient> mFrontBufferOnWhite;
  RefPtr<TextureClientAllocator> mAllocator;
  gfx::IntRect mUpdateRect;
  bool mWasPlaceholder;
#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
  TimeStamp        mLastUpdate;
#endif
  nsIntRegion mInvalidFront;
  nsIntRegion mInvalidBack;
  nsExpirationState mExpirationState;
private:
  // Copies dirty pixels from the front buffer into the back buffer,
  // and records the copied region in aAddPaintedRegion.
  void ValidateBackBufferFromFront(const nsIntRegion &aDirtyRegion,
                                   nsIntRegion& aAddPaintedRegion);
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
  LayerToParentLayerMatrix4x4 mTransformToCompBounds;

  /*
   * The critical displayport of the content from the nearest ancestor layer
   * that represents scrollable content with a display port set. isNothing()
   * if a critical displayport is not set.
   */
  Maybe<LayerIntRect> mCriticalDisplayPort;

  /*
   * The render resolution of the document that the content this layer
   * represents is in.
   */
  CSSToParentLayerScale2D mResolution;

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

  /*
   * Whether or not there is an async transform animation active
   */
  bool mHasTransformAnimation : 1;

  /*
   * Initializes/clears data to prepare for paint action.
   */
  void ResetPaintData();
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
  bool UpdateFromCompositorFrameMetrics(const LayerMetricsWrapper& aLayer,
                                        bool aHasPendingNewThebesContent,
                                        bool aLowPrecision,
                                        AsyncTransform& aViewTransform);

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
{
public:
  ClientTiledLayerBuffer(ClientTiledPaintedLayer& aPaintedLayer,
                         CompositableClient& aCompositableClient)
    : mPaintedLayer(aPaintedLayer)
    , mCompositableClient(aCompositableClient)
    , mLastPaintContentType(gfxContentType::COLOR)
    , mLastPaintSurfaceMode(SurfaceMode::SURFACE_OPAQUE)
    , mWasLastPaintProgressive(false)
  {}

  virtual void PaintThebes(const nsIntRegion& aNewValidRegion,
                   const nsIntRegion& aPaintRegion,
                   const nsIntRegion& aDirtyRegion,
                   LayerManager::DrawPaintedLayerCallback aCallback,
                   void* aCallbackData,
                   bool aIsProgressive = false) = 0;

  virtual bool SupportsProgressiveUpdate() = 0;
  virtual bool ProgressiveUpdate(nsIntRegion& aValidRegion,
                         nsIntRegion& aInvalidRegion,
                         const nsIntRegion& aOldValidRegion,
                         BasicTiledLayerPaintData* aPaintData,
                         LayerManager::DrawPaintedLayerCallback aCallback,
                         void* aCallbackData) = 0;
  virtual void ResetPaintedAndValidState() = 0;

  virtual const nsIntRegion& GetValidRegion() = 0;

  virtual bool IsLowPrecision() const = 0;

  virtual void Dump(std::stringstream& aStream,
                    const char* aPrefix,
                    bool aDumpHtml,
                    TextureDumpMode aCompress) {}

  const CSSToParentLayerScale2D& GetFrameResolution() { return mFrameResolution; }
  void SetFrameResolution(const CSSToParentLayerScale2D& aResolution) { mFrameResolution = aResolution; }

  bool HasFormatChanged() const;

protected:
  void UnlockTile(TileClient& aTile);
  gfxContentType GetContentType(SurfaceMode* aMode = nullptr) const;

  ClientTiledPaintedLayer& mPaintedLayer;
  CompositableClient& mCompositableClient;

  gfxContentType mLastPaintContentType;
  SurfaceMode mLastPaintSurfaceMode;
  CSSToParentLayerScale2D mFrameResolution;

  bool mWasLastPaintProgressive;
};

class ClientMultiTiledLayerBuffer
  : public TiledLayerBuffer<ClientMultiTiledLayerBuffer, TileClient>
  , public ClientTiledLayerBuffer
{
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
                   bool aIsProgressive = false) override;

  virtual bool SupportsProgressiveUpdate() override { return true; }
  /**
   * Performs a progressive update of a given tiled buffer.
   * See ComputeProgressiveUpdateRegion below for parameter documentation.
   */
  bool ProgressiveUpdate(nsIntRegion& aValidRegion,
                         nsIntRegion& aInvalidRegion,
                         const nsIntRegion& aOldValidRegion,
                         BasicTiledLayerPaintData* aPaintData,
                         LayerManager::DrawPaintedLayerCallback aCallback,
                         void* aCallbackData) override;
  
  void ResetPaintedAndValidState() override {
    mPaintedRegion.SetEmpty();
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

  void Dump(std::stringstream& aStream,
            const char* aPrefix,
            bool aDumpHtml,
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

    Update(nsIntRegion(), nsIntRegion(), nsIntRegion());
    mResolution = aResolution;
  }

protected:
  bool ValidateTile(TileClient& aTile,
                    const nsIntPoint& aTileRect,
                    const nsIntRegion& dirtyRect);
  
  void Update(const nsIntRegion& aNewValidRegion,
              const nsIntRegion& aPaintRegion,
              const nsIntRegion& aDirtyRegion);

  TileClient GetPlaceholderTile() const { return TileClient(); }

private:
  RefPtr<ClientLayerManager> mManager;
  LayerManager::DrawPaintedLayerCallback mCallback;
  void* mCallbackData;

  // The region that will be made valid during Update(). Once Update() is
  // completed then this is identical to mValidRegion.
  nsIntRegion mNewValidRegion;

  SharedFrameMetricsHelper*  mSharedFrameMetricsHelper;
  // When using Moz2D's CreateTiledDrawTarget we maintain a list of gfx::Tiles
  std::vector<gfx::Tile> mMoz2DTiles;
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
};

class TiledContentClient : public CompositableClient
{
public:
  TiledContentClient(ClientLayerManager* aManager,
                     const char* aName = "")
    : CompositableClient(aManager->AsShadowForwarder())
    , mName(aName)
  {}

protected:
  ~TiledContentClient()
  {}

public:
  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix);

  virtual void Dump(std::stringstream& aStream,
                    const char* aPrefix="",
                    bool aDumpHtml=false,
                    TextureDumpMode aCompress=TextureDumpMode::Compress) override;

  virtual TextureInfo GetTextureInfo() const override
  {
    return TextureInfo(CompositableType::CONTENT_TILED);
  }


  virtual ClientTiledLayerBuffer* GetTiledBuffer() = 0;
  virtual ClientTiledLayerBuffer* GetLowPrecisionTiledBuffer() = 0;

  enum TiledBufferType {
    TILED_BUFFER,
    LOW_PRECISION_TILED_BUFFER
  };
  virtual void UpdatedBuffer(TiledBufferType aType) = 0;

private:
  const char* mName;
};

/**
 * An implementation of TiledContentClient that supports
 * multiple tiles and a low precision buffer.
 */
class MultiTiledContentClient : public TiledContentClient
{
public:
  MultiTiledContentClient(ClientTiledPaintedLayer& aPaintedLayer,
                          ClientLayerManager* aManager);

protected:
  ~MultiTiledContentClient()
  {
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

} // namespace layers
} // namespace mozilla

#endif
