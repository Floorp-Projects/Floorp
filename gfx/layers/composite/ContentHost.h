/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CONTENTHOST_H
#define GFX_CONTENTHOST_H

#include "ThebesLayerBuffer.h"
#include "CompositableHost.h"

namespace mozilla {
namespace layers {

class ThebesBuffer;
class OptionalThebesBuffer;
struct TexturedEffect;

/**
 * ContentHosts are used for compositing Thebes layers, always matched by a
 * ContentClient of the same type.
 *
 * ContentHosts support only UpdateThebes(), not Update().
 */
class ContentHost : public CompositableHost
{
public:
  // Subclasses should implement this method if they support being used as a
  // tiling.
  virtual TiledLayerComposer* AsTiledLayerComposer() { return nullptr; }

  virtual void UpdateThebes(const ThebesBufferData& aData,
                            const nsIntRegion& aUpdated,
                            const nsIntRegion& aOldValidRegionBack,
                            nsIntRegion* aUpdatedRegionBack) = 0;

#ifdef MOZ_DUMP_PAINTING
  virtual already_AddRefed<gfxImageSurface> Dump() { return nullptr; }
#endif

protected:
  ContentHost(const TextureInfo& aTextureInfo)
    : CompositableHost(aTextureInfo)
  {}
};

/**
 * Base class for non-tiled ContentHosts.
 *
 * Ownership of the SurfaceDescriptor and the resources it represents is passed
 * from the ContentClient to the ContentHost when the TextureClient/Hosts are
 * created, that is recevied here by SetTextureHosts which assigns one or two
 * texture hosts (for single and double buffering) to the ContentHost.
 *
 * It is the responsibility of the ContentHost to destroy its resources when
 * they are recreated or the ContentHost dies.
 */
class ContentHostBase : public ContentHost
{
public:
  typedef ThebesLayerBuffer::ContentType ContentType;
  typedef ThebesLayerBuffer::PaintState PaintState;

  ContentHostBase(const TextureInfo& aTextureInfo);
  ~ContentHostBase();

  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr,
                         TiledLayerProperties* aLayerProperties = nullptr);

  virtual PaintState BeginPaint(ContentType, uint32_t)
  {
    NS_RUNTIMEABORT("shouldn't BeginPaint for a shadow layer");
    return PaintState();
  }

  virtual LayerRenderState GetRenderState() MOZ_OVERRIDE
  {
    LayerRenderState result = mTextureHost->GetRenderState();

    result.mFlags = (mBufferRotation != nsIntPoint()) ?
                    LAYER_RENDER_STATE_BUFFER_ROTATION : 0;
    return result;
  }

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

#ifdef MOZ_DUMP_PAINTING
  virtual already_AddRefed<gfxImageSurface> Dump()
  {
    return mTextureHost->Dump();
  }
#endif

  virtual TextureHost* GetTextureHost() MOZ_OVERRIDE;

  void SetPaintWillResample(bool aResample) { mPaintWillResample = aResample; }
  // The client has destroyed its texture clients and we should destroy our
  // texture hosts and SurfaceDescriptors. Note that we don't immediately
  // destroy our front buffer so that we can continue to composite.
  virtual void DestroyTextures() = 0;

protected:
  virtual nsIntPoint GetOriginOffset()
  {
    return mBufferRect.TopLeft() - mBufferRotation;
  }

  bool PaintWillResample() { return mPaintWillResample; }

  // Destroy the front buffer's texture host. This should only happen when
  // we have a new front buffer to use or the ContentHost is going to die.
  void DestroyFrontHost();

  nsIntRect mBufferRect;
  nsIntPoint mBufferRotation;
  RefPtr<TextureHost> mTextureHost;
  RefPtr<TextureHost> mTextureHostOnWhite;
  // When we set a new front buffer TextureHost, we don't want to stomp on
  // the old one which might still be used for compositing. So we store it
  // here and move it to mTextureHost once we do the first buffer swap.
  RefPtr<TextureHost> mNewFrontHost;
  RefPtr<TextureHost> mNewFrontHostOnWhite;
  bool mPaintWillResample;
  bool mInitialised;
};

/**
 * Double buffering is implemented by swapping the front and back TextureHosts.
 */
class ContentHostDoubleBuffered : public ContentHostBase
{
public:
  ContentHostDoubleBuffered(const TextureInfo& aTextureInfo)
    : ContentHostBase(aTextureInfo)
  {}

  ~ContentHostDoubleBuffered();

  virtual CompositableType GetType() { return BUFFER_CONTENT_DIRECT; }

  virtual void UpdateThebes(const ThebesBufferData& aData,
                            const nsIntRegion& aUpdated,
                            const nsIntRegion& aOldValidRegionBack,
                            nsIntRegion* aUpdatedRegionBack);

  virtual void EnsureTextureHost(TextureIdentifier aTextureId,
                                 const SurfaceDescriptor& aSurface,
                                 ISurfaceAllocator* aAllocator,
                                 const TextureInfo& aTextureInfo) MOZ_OVERRIDE;
  virtual void DestroyTextures() MOZ_OVERRIDE;

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual void PrintInfo(nsACString& aTo, const char* aPrefix);
#endif
protected:
  nsIntRegion mValidRegionForNextBackBuffer;
  // Texture host for the back buffer. We never read or write this buffer. We
  // only swap it with the front buffer (mTextureHost) when we are told by the
  // content thread.
  RefPtr<TextureHost> mBackHost;
  RefPtr<TextureHost> mBackHostOnWhite;
};

/**
 * Single buffered, therefore we must synchronously upload the image from the
 * TextureHost in the layers transaction (i.e., in UpdateThebes).
 */
class ContentHostSingleBuffered : public ContentHostBase
{
public:
  ContentHostSingleBuffered(const TextureInfo& aTextureInfo)
    : ContentHostBase(aTextureInfo)
  {}
  virtual ~ContentHostSingleBuffered();

  virtual CompositableType GetType() { return BUFFER_CONTENT; }

  virtual void UpdateThebes(const ThebesBufferData& aData,
                            const nsIntRegion& aUpdated,
                            const nsIntRegion& aOldValidRegionBack,
                            nsIntRegion* aUpdatedRegionBack);

  virtual void EnsureTextureHost(TextureIdentifier aTextureId,
                                 const SurfaceDescriptor& aSurface,
                                 ISurfaceAllocator* aAllocator,
                                 const TextureInfo& aTextureInfo) MOZ_OVERRIDE;
  virtual void DestroyTextures() MOZ_OVERRIDE;

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual void PrintInfo(nsACString& aTo, const char* aPrefix);
#endif
};

}
}

#endif
