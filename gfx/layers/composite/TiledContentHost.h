/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_TILEDCONTENTHOST_H
#define GFX_TILEDCONTENTHOST_H

#include "ContentHost.h"
#include "ClientTiledThebesLayer.h" // for BasicTiledLayerBuffer
#include "mozilla/layers/TextureHost.h"

namespace mozilla {
namespace layers {

class ThebesBuffer;
class OptionalThebesBuffer;
struct TexturedEffect;

class TiledTexture {
public:
  // Constructs a placeholder TiledTexture. See the comments above
  // TiledLayerBuffer for more information on what this is used for;
  // essentially, this is a sentinel used to represent an invalid or blank
  // tile.
  TiledTexture()
    : mDeprecatedTextureHost(nullptr)
  {}

  // Constructs a TiledTexture from a DeprecatedTextureHost.
  TiledTexture(DeprecatedTextureHost* aDeprecatedTextureHost)
    : mDeprecatedTextureHost(aDeprecatedTextureHost)
  {}

  TiledTexture(const TiledTexture& o) {
    mDeprecatedTextureHost = o.mDeprecatedTextureHost;
  }
  TiledTexture& operator=(const TiledTexture& o) {
    if (this == &o) {
      return *this;
    }
    mDeprecatedTextureHost = o.mDeprecatedTextureHost;
    return *this;
  }

  void Validate(gfxReusableSurfaceWrapper* aReusableSurface, Compositor* aCompositor, uint16_t aSize);

  bool operator== (const TiledTexture& o) const {
    if (!mDeprecatedTextureHost || !o.mDeprecatedTextureHost) {
      return mDeprecatedTextureHost == o.mDeprecatedTextureHost;
    }
    return *mDeprecatedTextureHost == *o.mDeprecatedTextureHost;
  }
  bool operator!= (const TiledTexture& o) const {
    if (!mDeprecatedTextureHost || !o.mDeprecatedTextureHost) {
      return mDeprecatedTextureHost != o.mDeprecatedTextureHost;
    }
    return *mDeprecatedTextureHost != *o.mDeprecatedTextureHost;
  }

  RefPtr<DeprecatedTextureHost> mDeprecatedTextureHost;
};

class TiledLayerBufferComposite
  : public TiledLayerBuffer<TiledLayerBufferComposite, TiledTexture>
{
  friend class TiledLayerBuffer<TiledLayerBufferComposite, TiledTexture>;

public:
  typedef TiledLayerBuffer<TiledLayerBufferComposite, TiledTexture>::Iterator Iterator;
  TiledLayerBufferComposite()
    : mCompositor(nullptr)
  {}

  void Upload(const BasicTiledLayerBuffer* aMainMemoryTiledBuffer,
              const nsIntRegion& aNewValidRegion,
              const nsIntRegion& aInvalidateRegion,
              const gfxSize& aResolution);

  TiledTexture GetPlaceholderTile() const { return TiledTexture(); }

  // Stores the absolute resolution of the containing frame, calculated
  // by the sum of the resolutions of all parent layers' FrameMetrics.
  const gfxSize& GetFrameResolution() { return mFrameResolution; }

  void SetCompositor(Compositor* aCompositor)
  {
    mCompositor = aCompositor;
  }

protected:
  TiledTexture ValidateTile(TiledTexture aTile,
                            const nsIntPoint& aTileRect,
                            const nsIntRegion& dirtyRect);

  // do nothing, the desctructor in the texture host takes care of releasing resources
  void ReleaseTile(TiledTexture aTile) {}

  void SwapTiles(TiledTexture& aTileA, TiledTexture& aTileB) {
    std::swap(aTileA, aTileB);
  }

private:
  Compositor* mCompositor;
  const BasicTiledLayerBuffer* mMainMemoryTiledBuffer;
  gfxSize mFrameResolution;
};

class TiledThebesLayerComposite;

/**
 * ContentHost for tiled Thebes layers. Since tiled layers are special snow
 * flakes, we don't call UpdateThebes or AddTextureHost, etc. We do call Composite
 * in the usual way though.
 *
 * There is no corresponding content client - on the client side we use a
 * BasicTiledLayerBuffer owned by a BasicTiledThebesLayer. On the host side, we
 * just use a regular ThebesLayerComposite, but with a tiled content host.
 *
 * TiledContentHost has a TiledLayerBufferComposite which keeps hold of the tiles.
 * Each tile has a reference to a texture host. During the layers transaction, we
 * receive a copy of the client-side tile buffer (PaintedTiledLayerBuffer). This is
 * copied into the main memory tile buffer and then deleted. Copying copies tiles,
 * but we only copy references to the underlying texture clients.
 *
 * When the content host is composited, we first upload any pending tiles
 * (Process*UploadQueue), then render (RenderLayerBuffer). The former calls Validate
 * on the tile (via ValidateTile and Update), that calls Update on the texture host,
 * which works as for regular texture hosts. Rendering takes us to RenderTile which
 * is similar to Composite for non-tiled ContentHosts.
 */
class TiledContentHost : public ContentHost,
                         public TiledLayerComposer
{
public:
  TiledContentHost(const TextureInfo& aTextureInfo)
    : ContentHost(aTextureInfo)
    , mPendingUpload(false)
    , mPendingLowPrecisionUpload(false)
  {}
  ~TiledContentHost();

  virtual LayerRenderState GetRenderState() MOZ_OVERRIDE
  {
    return LayerRenderState();
  }


  virtual void UpdateThebes(const ThebesBufferData& aData,
                            const nsIntRegion& aUpdated,
                            const nsIntRegion& aOldValidRegionBack,
                            nsIntRegion* aUpdatedRegionBack)
  {
    MOZ_ASSERT(false, "N/A for tiled layers");
  }

  const nsIntRegion& GetValidLowPrecisionRegion() const
  {
    return mLowPrecisionVideoMemoryTiledBuffer.GetValidRegion();
  }

  void PaintedTiledLayerBuffer(const BasicTiledLayerBuffer* mTiledBuffer);

  // Renders a single given tile.
  void RenderTile(const TiledTexture& aTile,
                  EffectChain& aEffectChain,
                  float aOpacity,
                  const gfx::Matrix4x4& aTransform,
                  const gfx::Point& aOffset,
                  const gfx::Filter& aFilter,
                  const gfx::Rect& aClipRect,
                  const nsIntRegion& aScreenRegion,
                  const nsIntPoint& aTextureOffset,
                  const nsIntSize& aTextureBounds);

  void Composite(EffectChain& aEffectChain,
                 float aOpacity,
                 const gfx::Matrix4x4& aTransform,
                 const gfx::Point& aOffset,
                 const gfx::Filter& aFilter,
                 const gfx::Rect& aClipRect,
                 const nsIntRegion* aVisibleRegion = nullptr,
                 TiledLayerProperties* aLayerProperties = nullptr);

  virtual CompositableType GetType() { return BUFFER_TILED; }

  virtual TiledLayerComposer* AsTiledLayerComposer() MOZ_OVERRIDE { return this; }

  virtual void EnsureDeprecatedTextureHost(TextureIdentifier aTextureId,
                                 const SurfaceDescriptor& aSurface,
                                 ISurfaceAllocator* aAllocator,
                                 const TextureInfo& aTextureInfo) MOZ_OVERRIDE
  {
    MOZ_CRASH("Does nothing");
  }

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE
  {
    CompositableHost::SetCompositor(aCompositor);
    mVideoMemoryTiledBuffer.SetCompositor(aCompositor);
    mLowPrecisionVideoMemoryTiledBuffer.SetCompositor(aCompositor);
  }

  virtual void Attach(Layer* aLayer,
                      Compositor* aCompositor,
                      AttachFlags aFlags = NO_FLAGS) MOZ_OVERRIDE;

  virtual void Dump(FILE* aFile=nullptr,
                    const char* aPrefix="",
                    bool aDumpHtml=false) MOZ_OVERRIDE;

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual void PrintInfo(nsACString& aTo, const char* aPrefix);
#endif

private:
  void ProcessUploadQueue(nsIntRegion* aNewValidRegion,
                          TiledLayerProperties* aLayerProperties);
  void ProcessLowPrecisionUploadQueue();

  void RenderLayerBuffer(TiledLayerBufferComposite& aLayerBuffer,
                         const nsIntRegion& aValidRegion,
                         EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Point& aOffset,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect,
                         const nsIntRegion& aMaskRegion,
                         nsIntRect aVisibleRect,
                         gfx::Matrix4x4 aTransform);

  void EnsureTileStore() {}

  nsIntRegion                  mRegionToUpload;
  nsIntRegion                  mLowPrecisionRegionToUpload;
  BasicTiledLayerBuffer        mMainMemoryTiledBuffer;
  BasicTiledLayerBuffer        mLowPrecisionMainMemoryTiledBuffer;
  TiledLayerBufferComposite    mVideoMemoryTiledBuffer;
  TiledLayerBufferComposite    mLowPrecisionVideoMemoryTiledBuffer;
  bool                         mPendingUpload : 1;
  bool                         mPendingLowPrecisionUpload : 1;
};

}
}

#endif
