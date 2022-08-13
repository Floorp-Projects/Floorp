/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYER_DCOMP_SURFACE_IMAGE_H
#define GFX_LAYER_DCOMP_SURFACE_IMAGE_H

#include "ImageContainer.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureHost.h"

namespace mozilla::wr {
class DisplayListBuilder;
class TransactionBuilder;
}  // namespace mozilla::wr

namespace mozilla::layers {

already_AddRefed<TextureHost> CreateTextureHostDcompSurface(
    const SurfaceDescriptor& aDesc, ISurfaceAllocator* aDeallocator,
    LayersBackend aBackend, TextureFlags aFlags);

/**
 * A texture data wrapping a dcomp surface handle, which is provided by the
 * media foundation media engine. We won't be able to control real texture data
 * because that is protected by the media engine.
 */
class DcompSurfaceTexture final : public TextureData {
 public:
  static already_AddRefed<TextureClient> CreateTextureClient(
      HANDLE aHandle, gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
      KnowsCompositor* aKnowsCompositor);

  ~DcompSurfaceTexture();

  bool Serialize(SurfaceDescriptor& aOutDescriptor) override;
  void GetSubDescriptor(RemoteDecoderVideoSubDescriptor* aOutDesc) override;
  void FillInfo(TextureData::Info& aInfo) const {
    aInfo.size = mSize;
    aInfo.supportsMoz2D = false;
    aInfo.canExposeMappedData = false;
    aInfo.hasSynchronization = false;
  }
  bool Lock(OpenMode) override { return true; }
  void Unlock() override {}
  void Deallocate(LayersIPCChannel* aAllocator) override {}

 private:
  DcompSurfaceTexture(HANDLE aHandle, gfx::IntSize aSize,
                      gfx::SurfaceFormat aFormat)
      : mHandle(aHandle), mSize(aSize), mFormat(aFormat) {}

  const HANDLE mHandle;
  const gfx::IntSize mSize;
  const gfx::SurfaceFormat mFormat;
};

/**
 * A image data which holds a dcomp texture which is provided by the media
 * foundation media engine. As the real texture is protected, it does not
 * support being accessed as a source surface.
 */
class DcompSurfaceImage : public Image {
 public:
  DcompSurfaceImage(HANDLE aHandle, gfx::IntSize aSize,
                    gfx::SurfaceFormat aFormat,
                    KnowsCompositor* aKnowsCompositor);
  virtual ~DcompSurfaceImage() = default;

  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override {
    return nullptr;
  }

  gfx::IntSize GetSize() const { return mTextureClient->GetSize(); };

  TextureClient* GetTextureClient(KnowsCompositor* aKnowsCompositor) override;

 private:
  RefPtr<TextureClient> mTextureClient;
};

/**
 * A Texture host living in GPU process which owns a dcomp surface handle which
 * is provided by the media foundation media engine.
 */
class DcompSurfaceHandleHost : public TextureHost {
 public:
  DcompSurfaceHandleHost(TextureFlags aFlags,
                         const SurfaceDescriptorDcompSurface& aDescriptor);

  gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  gfx::IntSize GetSize() const override { return mSize; }

  const char* Name() override { return "DcompSurfaceHandleHost"; }

  already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override {
    return nullptr;
  }

  void CreateRenderTexture(
      const wr::ExternalImageId& aExternalImageId) override;

  void PushResourceUpdates(
      wr::TransactionBuilder& aResources, ResourceUpdateOp aOp,
      const Range<wr::ImageKey>& aImageKeys,
      const wr::ExternalImageId& aExternalImageId) override;

  void PushDisplayItems(wr::DisplayListBuilder& aBuilder,
                        const wr::LayoutRect& aBounds,
                        const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
                        const Range<wr::ImageKey>& aImageKeys,
                        PushDisplayItemFlagSet aFlags) override;

  bool SupportsExternalCompositing(WebRenderBackend aBackend) override {
    return true;
  }

 protected:
  ~DcompSurfaceHandleHost();

  // Handle will be closed automatically when `UniqueFileHandle` gets destroyed.
  const mozilla::UniqueFileHandle mHandle;
  const gfx::IntSize mSize;
  const gfx::SurfaceFormat mFormat;
};

}  // namespace mozilla::layers

#endif  // GFX_LAYER_DCOMP_SURFACE_IMAGE_H
