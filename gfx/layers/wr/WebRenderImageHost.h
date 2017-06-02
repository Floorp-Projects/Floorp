/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_WEBRENDERIMAGEHOST_H
#define MOZILLA_GFX_WEBRENDERIMAGEHOST_H

#include "CompositableHost.h"           // for CompositableHost
#include "mozilla/layers/ImageComposite.h"  // for ImageComposite

namespace mozilla {
namespace layers {

class WebRenderBridgeParent;

/**
 * ImageHost. Works with ImageClientSingle and ImageClientBuffered
 */
class WebRenderImageHost : public CompositableHost,
                           public ImageComposite
{
public:
  explicit WebRenderImageHost(const TextureInfo& aTextureInfo);
  ~WebRenderImageHost();

  virtual CompositableType GetType() override { return mTextureInfo.mCompositableType; }

  virtual void Composite(Compositor* aCompositor,
                         LayerComposite* aLayer,
                         EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::SamplingFilter aSamplingFilter,
                         const gfx::IntRect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr,
                         const Maybe<gfx::Polygon>& aGeometry = Nothing()) override;

  virtual void UseTextureHost(const nsTArray<TimedTexture>& aTextures) override;
  virtual void UseComponentAlphaTextures(TextureHost* aTextureOnBlack,
                                         TextureHost* aTextureOnWhite) override;
  virtual void RemoveTextureHost(TextureHost* aTexture) override;

  virtual TextureHost* GetAsTextureHost(gfx::IntRect* aPictureRect = nullptr) override;

  virtual void Attach(Layer* aLayer,
                      TextureSourceProvider* aProvider,
                      AttachFlags aFlags = NO_FLAGS) override;

  virtual void SetTextureSourceProvider(TextureSourceProvider* aProvider) override;

  gfx::IntSize GetImageSize() const override;

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;

  virtual void Dump(std::stringstream& aStream,
                    const char* aPrefix = "",
                    bool aDumpHtml = false) override;

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override;

  virtual bool Lock() override;

  virtual void Unlock() override;

  virtual void CleanupResources() override;

  virtual WebRenderImageHost* AsWebRenderImageHost() override { return this; }

  TextureHost* GetAsTextureHostForComposite();

  void SetWrBridge(WebRenderBridgeParent* aWrBridge);

  void ClearWrBridge();

  TextureHost* GetCurrentTextureHost() { return mCurrentTextureHost; }

protected:
  // ImageComposite
  virtual TimeStamp GetCompositionTime() const override;

  void SetCurrentTextureHost(TextureHost* aTexture);

  WebRenderBridgeParent* MOZ_NON_OWNING_REF mWrBridge;

  uint32_t mWrBridgeBindings;

  CompositableTextureHostRef mCurrentTextureHost;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_GFX_WEBRENDERIMAGEHOST_H
