/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GPUVIDEOTEXTUREHOST_H
#define MOZILLA_GFX_GPUVIDEOTEXTUREHOST_H

#include "mozilla/layers/TextureHost.h"

namespace mozilla {
namespace layers {

class GPUVideoTextureHost : public TextureHost {
 public:
  static GPUVideoTextureHost* CreateFromDescriptor(
      TextureFlags aFlags, const SurfaceDescriptorGPUVideo& aDescriptor);

  virtual ~GPUVideoTextureHost();

  void DeallocateDeviceData() override {}

  virtual void SetTextureSourceProvider(
      TextureSourceProvider* aProvider) override;

  bool Lock() override;

  void Unlock() override;

  gfx::SurfaceFormat GetFormat() const override;

  bool BindTextureSource(CompositableTextureSourceRef& aTexture) override;
  bool AcquireTextureSource(CompositableTextureSourceRef& aTexture) override;

  already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override {
    return nullptr;  // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

  gfx::YUVColorSpace GetYUVColorSpace() const override;

  gfx::IntSize GetSize() const override;

#ifdef MOZ_LAYERS_HAVE_LOG
  const char* Name() override { return "GPUVideoTextureHost"; }
#endif

  bool HasIntermediateBuffer() const override;

  void CreateRenderTexture(
      const wr::ExternalImageId& aExternalImageId) override;

  uint32_t NumSubTextures() const override;

  void PushResourceUpdates(wr::TransactionBuilder& aResources,
                           ResourceUpdateOp aOp,
                           const Range<wr::ImageKey>& aImageKeys,
                           const wr::ExternalImageId& aExtID) override;

  void PushDisplayItems(wr::DisplayListBuilder& aBuilder,
                        const wr::LayoutRect& aBounds,
                        const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
                        const Range<wr::ImageKey>& aImageKeys) override;

  bool SupportsWrNativeTexture() override;

 protected:
  GPUVideoTextureHost(TextureFlags aFlags, TextureHost* aWrappedTextureHost);

  RefPtr<TextureHost> mWrappedTextureHost;
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_GPUVIDEOTEXTUREHOST_H
