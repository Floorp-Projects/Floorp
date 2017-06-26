/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GPUVIDEOTEXTUREHOST_H
#define MOZILLA_GFX_GPUVIDEOTEXTUREHOST_H

#include "mozilla/layers/TextureHost.h"

namespace mozilla {
namespace layers {

class GPUVideoTextureHost : public TextureHost
{
public:
 GPUVideoTextureHost(TextureFlags aFlags,
                     const SurfaceDescriptorGPUVideo& aDescriptor);
  virtual ~GPUVideoTextureHost();

  virtual void DeallocateDeviceData() override {}

  virtual void SetTextureSourceProvider(TextureSourceProvider* aProvider) override;

  virtual bool Lock() override;

  virtual void Unlock() override;

  virtual gfx::SurfaceFormat GetFormat() const override;

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTexture) override;
  virtual bool AcquireTextureSource(CompositableTextureSourceRef& aTexture) override;

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override
  {
    return nullptr; // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

  virtual YUVColorSpace GetYUVColorSpace() const override;

  virtual gfx::IntSize GetSize() const override;

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() override { return "GPUVideoTextureHost"; }
#endif

  virtual bool HasIntermediateBuffer() const override;

  virtual void CreateRenderTexture(const wr::ExternalImageId& aExternalImageId) override;

  virtual void GetWRImageKeys(nsTArray<wr::ImageKey>& aImageKeys,
                              const std::function<wr::ImageKey()>& aImageKeyAllocator) override;

  virtual void AddWRImage(wr::WebRenderAPI* aAPI,
                          Range<const wr::ImageKey>& aImageKeys,
                          const wr::ExternalImageId& aExtID) override;

  virtual void PushExternalImage(wr::DisplayListBuilder& aBuilder,
                                 const WrRect& aBounds,
                                 const WrRect& aClip,
                                 wr::ImageRendering aFilter,
                                 Range<const wr::ImageKey>& aImageKeys) override;

protected:
  RefPtr<TextureHost> mWrappedTextureHost;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_GFX_GPUVIDEOTEXTUREHOST_H
