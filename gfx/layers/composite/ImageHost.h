/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGEHOST_H
#define MOZILLA_GFX_IMAGEHOST_H

#include "CompositableHost.h"
#include "mozilla/layers/LayerManagerComposite.h"

namespace mozilla {
namespace layers {

/**
 * Used for compositing Image and Canvas layers, matched on the content-side
 * by an ImageClient or CanvasClient.
 *
 * ImageHosts support Update., not UpdateThebes().
 */
class ImageHost : public CompositableHost
{
public:
  TextureHost* GetTextureHost() MOZ_OVERRIDE { return nullptr; }

protected:
  ImageHost(const TextureInfo& aTextureInfo)
  : CompositableHost(aTextureInfo)
  {
    MOZ_COUNT_CTOR(ImageHost);
  }

  ~ImageHost()
  {
    MOZ_COUNT_DTOR(ImageHost);
  }
};

// ImageHost with a single TextureHost
class ImageHostSingle : public ImageHost
{
public:
  ImageHostSingle(const TextureInfo& aTextureInfo)
    : ImageHost(aTextureInfo)
    , mTextureHost(nullptr)
    , mHasPictureRect(false)
  {}

  virtual CompositableType GetType() { return mTextureInfo.mCompositableType; }

  virtual void EnsureTextureHost(TextureIdentifier aTextureId,
                                 const SurfaceDescriptor& aSurface,
                                 ISurfaceAllocator* aAllocator,
                                 const TextureInfo& aTextureInfo) MOZ_OVERRIDE;

  TextureHost* GetTextureHost() MOZ_OVERRIDE { return mTextureHost; }

  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr,
                         TiledLayerProperties* aLayerProperties = nullptr);

  virtual bool Update(const SurfaceDescriptor& aImage,
                      SurfaceDescriptor* aResult = nullptr) MOZ_OVERRIDE
  {
    return ImageHost::Update(aImage, aResult);
  }

  virtual void SetPictureRect(const nsIntRect& aPictureRect) MOZ_OVERRIDE
  {
    mPictureRect = aPictureRect;
    mHasPictureRect = true;
  }

  virtual LayerRenderState GetRenderState() MOZ_OVERRIDE
  {
    return mTextureHost->GetRenderState();
  }

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual void PrintInfo(nsACString& aTo, const char* aPrefix);
#endif

protected:
  virtual void MakeTextureHost(TextureIdentifier aTextureId,
                               const SurfaceDescriptor& aSurface,
                               ISurfaceAllocator* aAllocator,
                               const TextureInfo& aTextureInfo);

  RefPtr<TextureHost> mTextureHost;
  nsIntRect mPictureRect;
  bool mHasPictureRect;
};

// Double buffered ImageHost. We have a single TextureHost and double buffering
// is done at the TextureHost/Client level. This is in contrast with buffered
// ContentHosts which do their own double buffering 
class ImageHostBuffered : public ImageHostSingle
{
public:
  ImageHostBuffered(const TextureInfo& aTextureInfo)
    : ImageHostSingle(aTextureInfo)
  {}

  virtual bool Update(const SurfaceDescriptor& aImage,
                      SurfaceDescriptor* aResult = nullptr) MOZ_OVERRIDE;

protected:
  virtual void MakeTextureHost(TextureIdentifier aTextureId,
                               const SurfaceDescriptor& aSurface,
                               ISurfaceAllocator* aAllocator,
                               const TextureInfo& aTextureInfo) MOZ_OVERRIDE;
};

}
}

#endif
