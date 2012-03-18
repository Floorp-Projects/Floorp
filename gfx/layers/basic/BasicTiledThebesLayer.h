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
  BasicTiledThebesLayer(BasicShadowLayerManager* const aManager)
    : ThebesLayer(aManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicTiledThebesLayer);
  }

  ~BasicTiledThebesLayer()
  {
    MOZ_COUNT_DTOR(BasicTiledThebesLayer);
  }


  // Thebes Layer
  virtual Layer* AsLayer() { return this; }
  virtual void InvalidateRegion(const nsIntRegion& aRegion) {
    mValidRegion.Sub(mValidRegion, aRegion);
  }

  // BasicImplData
  virtual bool MustRetainContent() { return HasShadow(); }

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

  // Members
  BasicTiledLayerBuffer mTiledBuffer;
};

} // layers
} // mozilla

#endif
