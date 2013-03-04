/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BASICTILEDTHEBESLAYER_H
#define GFX_BASICTILEDTHEBESLAYER_H

#include "TiledLayerBuffer.h"
#include "gfxReusableSurfaceWrapper.h"
#include "mozilla/layers/ShadowLayers.h"
#include "BasicLayers.h"
#include "BasicImplData.h"
#include <algorithm>

namespace mozilla {
namespace layers {

/**
 * Represent a single tile in tiled buffer. It's backed
 * by a gfxReusableSurfaceWrapper that implements a
 * copy-on-write mechanism while locked. The tile should be
 * locked before being sent to the compositor and unlocked
 * as soon as it is uploaded to prevent a copy.
 * Ideal place to store per tile debug information.
 */
struct BasicTiledLayerTile {
  nsRefPtr<gfxReusableSurfaceWrapper> mSurface;
#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
  TimeStamp        mLastUpdate;
#endif

  // Placeholder
  BasicTiledLayerTile()
    : mSurface(NULL)
  {}
  explicit BasicTiledLayerTile(gfxImageSurface* aSurface)
    : mSurface(new gfxReusableSurfaceWrapper(aSurface))
  {
  }
  BasicTiledLayerTile(const BasicTiledLayerTile& o) {
    mSurface = o.mSurface;
#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
    mLastUpdate = o.mLastUpdate;
#endif
  }
  BasicTiledLayerTile& operator=(const BasicTiledLayerTile& o) {
    if (this == &o) return *this;
    mSurface = o.mSurface;
#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
    mLastUpdate = o.mLastUpdate;
#endif
    return *this;
  }
  bool operator== (const BasicTiledLayerTile& o) const {
    return mSurface == o.mSurface;
  }
  bool operator!= (const BasicTiledLayerTile& o) const {
    return mSurface != o.mSurface;
  }
  void ReadUnlock() {
    mSurface->ReadUnlock();
  }
  void ReadLock() {
    mSurface->ReadLock();
  }
};

/**
 * This struct stores all the data necessary to perform a paint so that it
 * doesn't need to be recalculated on every repeated transaction.
 */
struct BasicTiledLayerPaintData {
  gfx::Point mScrollOffset;
  gfx3DMatrix mTransformScreenToLayer;
  nsIntRect mLayerCriticalDisplayPort;
  gfxSize mResolution;
  nsIntRect mCompositionBounds;
  uint16_t mLowPrecisionPaintCount;
  bool mPaintFinished : 1;
};

class BasicTiledThebesLayer;

/**
 * Provide an instance of TiledLayerBuffer backed by image surfaces.
 * This buffer provides an implementation to ValidateTile using a
 * thebes callback and can support painting using a single paint buffer
 * which is much faster then painting directly into the tiles.
 */

class BasicTiledLayerBuffer : public TiledLayerBuffer<BasicTiledLayerBuffer, BasicTiledLayerTile>
{
  friend class TiledLayerBuffer<BasicTiledLayerBuffer, BasicTiledLayerTile>;

public:
  BasicTiledLayerBuffer()
    : mLastPaintOpaque(false)
  {}

  void PaintThebes(BasicTiledThebesLayer* aLayer,
                   const nsIntRegion& aNewValidRegion,
                   const nsIntRegion& aPaintRegion,
                   LayerManager::DrawThebesLayerCallback aCallback,
                   void* aCallbackData);

  BasicTiledLayerTile GetPlaceholderTile() const {
    return mPlaceholder;
  }

  void ReadUnlock() {
    for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
      if (mRetainedTiles[i] == GetPlaceholderTile()) continue;
      mRetainedTiles[i].ReadUnlock();
    }
  }

  void ReadLock() {
    for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
      if (mRetainedTiles[i] == GetPlaceholderTile()) continue;
      mRetainedTiles[i].ReadLock();
    }
  }

  const gfxSize& GetFrameResolution() { return mFrameResolution; }
  void SetFrameResolution(const gfxSize& aResolution) { mFrameResolution = aResolution; }

  bool HasFormatChanged(BasicTiledThebesLayer* aThebesLayer) const;
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

private:
  gfxASurface::gfxImageFormat GetFormat() const;
  BasicTiledThebesLayer* mThebesLayer;
  LayerManager::DrawThebesLayerCallback mCallback;
  void* mCallbackData;
  gfxSize mFrameResolution;
  bool mLastPaintOpaque;

  // The buffer we use when UseSinglePaintBuffer() above is true.
  nsRefPtr<gfxImageSurface>     mSinglePaintBuffer;
  nsIntPoint                    mSinglePaintBufferOffset;

  BasicTiledLayerTile           mPlaceholder;

  BasicTiledLayerTile ValidateTileInternal(BasicTiledLayerTile aTile,
                                           const nsIntPoint& aTileOrigin,
                                           const nsIntRect& aDirtyRect);
};

/**
 * An implementation of ThebesLayer that ONLY supports remote
 * composition that is backed by tiles. This thebes layer implementation
 * is better suited to mobile hardware to work around slow implementation
 * of glTexImage2D (for OGL compositors), and restrait memory bandwidth.
 */
class BasicTiledThebesLayer : public ThebesLayer,
                              public BasicImplData,
                              public BasicShadowableLayer
{
  typedef ThebesLayer Base;

public:
  BasicTiledThebesLayer(BasicShadowLayerManager* const aManager);
  ~BasicTiledThebesLayer();

  // Thebes Layer
  virtual Layer* AsLayer() { return this; }
  virtual void InvalidateRegion(const nsIntRegion& aRegion) {
    mInvalidRegion.Or(mInvalidRegion, aRegion);
    mValidRegion.Sub(mValidRegion, aRegion);
    mLowPrecisionValidRegion.Sub(mLowPrecisionValidRegion, aRegion);
  }

  // Shadow methods
  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs);
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

  virtual void Disconnect()
  {
    BasicShadowableLayer::Disconnect();
  }

  virtual void PaintThebes(gfxContext* aContext,
                           Layer* aMaskLayer,
                           LayerManager::DrawThebesLayerCallback aCallback,
                           void* aCallbackData,
                           ReadbackProcessor* aReadback);

private:
  BasicShadowLayerManager* BasicManager()
  {
    return static_cast<BasicShadowLayerManager*>(mManager);
  }

  // BasicImplData
  virtual void
  PaintBuffer(gfxContext* aContext,
              const nsIntRegion& aRegionToDraw,
              const nsIntRegion& aExtendedRegionToDraw,
              const nsIntRegion& aRegionToInvalidate,
              bool aDidSelfCopy,
              LayerManager::DrawThebesLayerCallback aCallback,
              void* aCallbackData)
  { NS_RUNTIMEABORT("Not reached."); }

  /**
   * Calculates the region to update in a single progressive update transaction.
   * This employs some heuristics to update the most 'sensible' region to
   * update at this point in time, and how large an update should be performed
   * at once to maintain visual coherency.
   *
   * aInvalidRegion is the current invalid region.
   * aOldValidRegion is the valid region of aTiledBuffer at the beginning of the
   * current transaction.
   * aRegionToPaint will be filled with the region to update. This may be empty,
   * which indicates that there is no more work to do.
   * aTransform is the transform required to convert from screen-space to
   * layer-space.
   * aCompositionBounds is the composition bounds from the primary scrollable
   * layer, transformed into layer coordinates.
   * aScrollOffset is the current scroll offset of the primary scrollable layer.
   * aResolution is the render resolution of the layer.
   * aIsRepeated should be true if this function has already been called during
   * this transaction.
   *
   * Returns true if it should be called again, false otherwise. In the case
   * that aRegionToPaint is empty, this will return aIsRepeated for convenience.
   */
  bool ComputeProgressiveUpdateRegion(BasicTiledLayerBuffer& aTiledBuffer,
                                      const nsIntRegion& aInvalidRegion,
                                      const nsIntRegion& aOldValidRegion,
                                      nsIntRegion& aRegionToPaint,
                                      const gfx3DMatrix& aTransform,
                                      const nsIntRect& aCompositionBounds,
                                      const gfx::Point& aScrollOffset,
                                      const gfxSize& aResolution,
                                      bool aIsRepeated);

  /**
   * Performs a progressive update of a given tiled buffer.
   * See ComputeProgressiveUpdateRegion above for parameter documentation.
   */
  bool ProgressiveUpdate(BasicTiledLayerBuffer& aTiledBuffer,
                         nsIntRegion& aValidRegion,
                         nsIntRegion& aInvalidRegion,
                         const nsIntRegion& aOldValidRegion,
                         const gfx3DMatrix& aTransform,
                         const nsIntRect& aCompositionBounds,
                         const gfx::Point& aScrollOffset,
                         const gfxSize& aResolution,
                         LayerManager::DrawThebesLayerCallback aCallback,
                         void* aCallbackData);

  /**
   * For the initial PaintThebes of a transaction, calculates all the data
   * needed for that paint and any repeated transactions.
   */
  void BeginPaint();

  /**
   * When a paint ends, updates any data necessary to persist until the next
   * paint. If aFinish is true, this will cause the paint to be marked as
   * finished.
   */
  void EndPaint(bool aFinish);

  // Members
  BasicTiledLayerBuffer mTiledBuffer;
  BasicTiledLayerBuffer mLowPrecisionTiledBuffer;
  nsIntRegion mLowPrecisionValidRegion;
  gfx::Point mLastScrollOffset;
  bool mFirstPaint;
  BasicTiledLayerPaintData mPaintData;
};

} // layers
} // mozilla

#endif
