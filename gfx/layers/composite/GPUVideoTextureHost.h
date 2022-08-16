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

  gfx::SurfaceFormat GetFormat() const override;

  already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override {
    return nullptr;  // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

  gfx::YUVColorSpace GetYUVColorSpace() const override;
  gfx::ColorDepth GetColorDepth() const override;
  gfx::ColorRange GetColorRange() const override;

  gfx::IntSize GetSize() const override;

  bool IsValid() override;

#ifdef MOZ_LAYERS_HAVE_LOG
  const char* Name() override { return "GPUVideoTextureHost"; }
#endif

  void CreateRenderTexture(
      const wr::ExternalImageId& aExternalImageId) override;

  void MaybeDestroyRenderTexture() override;

  uint32_t NumSubTextures() override;

  void PushResourceUpdates(wr::TransactionBuilder& aResources,
                           ResourceUpdateOp aOp,
                           const Range<wr::ImageKey>& aImageKeys,
                           const wr::ExternalImageId& aExtID) override;

  void PushDisplayItems(wr::DisplayListBuilder& aBuilder,
                        const wr::LayoutRect& aBounds,
                        const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
                        const Range<wr::ImageKey>& aImageKeys,
                        PushDisplayItemFlagSet aFlags) override;

  bool SupportsExternalCompositing(WebRenderBackend aBackend) override;

  void UnbindTextureSource() override;

  void NotifyNotUsed() override;

  bool IsWrappingBufferTextureHost() override;

  TextureHostType GetTextureHostType() override;

 protected:
  GPUVideoTextureHost(TextureFlags aFlags,
                      const SurfaceDescriptorGPUVideo& aDescriptor);

  TextureHost* EnsureWrappedTextureHost();

  RefPtr<TextureHost> mWrappedTextureHost;
  SurfaceDescriptorGPUVideo mDescriptor;
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_GPUVIDEOTEXTUREHOST_H
