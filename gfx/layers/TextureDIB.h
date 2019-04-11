/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTUREDIB_H
#define MOZILLA_GFX_TEXTUREDIB_H

#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/GfxMessageUtils.h"
#include "gfxWindowsPlatform.h"

namespace mozilla {
namespace layers {

class DIBTextureData : public TextureData {
 public:
  bool Lock(OpenMode) override { return true; }

  void Unlock() override {}

  void FillInfo(TextureData::Info& aInfo) const override;

  already_AddRefed<gfx::DrawTarget> BorrowDrawTarget() override;

  static DIBTextureData* Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                LayersIPCChannel* aAllocator);

 protected:
  DIBTextureData(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                 gfxWindowsSurface* aSurface)
      : mSurface(aSurface), mSize(aSize), mFormat(aFormat) {
    MOZ_ASSERT(aSurface);
  }

  RefPtr<gfxWindowsSurface> mSurface;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
};

/**
 * This is meant for a texture host which does a direct upload from
 * Updated to a Compositor specific DataTextureSource and therefor doesn't
 * need any specific Lock/Unlock magic.
 */
class TextureHostDirectUpload : public TextureHost {
 public:
  TextureHostDirectUpload(TextureFlags aFlags, gfx::SurfaceFormat aFormat,
                          gfx::IntSize aSize)
      : TextureHost(aFlags), mFormat(aFormat), mSize(aSize), mIsLocked(false) {}

  void DeallocateDeviceData() override;

  void SetTextureSourceProvider(TextureSourceProvider* aProvider) override;

  gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  gfx::IntSize GetSize() const override { return mSize; }

  bool Lock() override;

  void Unlock() override;

  bool HasIntermediateBuffer() const { return true; }

  bool BindTextureSource(CompositableTextureSourceRef& aTexture) override;
  bool AcquireTextureSource(CompositableTextureSourceRef& aTexture) override;

 protected:
  RefPtr<TextureSourceProvider> mProvider;
  RefPtr<DataTextureSource> mTextureSource;
  gfx::SurfaceFormat mFormat;
  gfx::IntSize mSize;
  bool mIsLocked;
};

class DIBTextureHost : public TextureHostDirectUpload {
 public:
  DIBTextureHost(TextureFlags aFlags, const SurfaceDescriptorDIB& aDescriptor);

  already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override {
    return nullptr;  // TODO: cf bug 872568
  }

 protected:
  void UpdatedInternal(const nsIntRegion* aRegion = nullptr) override;

  RefPtr<gfxWindowsSurface> mSurface;
};

class TextureHostFileMapping : public TextureHostDirectUpload {
 public:
  TextureHostFileMapping(TextureFlags aFlags,
                         const SurfaceDescriptorFileMapping& aDescriptor);
  ~TextureHostFileMapping();

  already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override {
    MOZ_CRASH("GFX: TextureHostFileMapping::GetAsSurface not implemented");
    // Not implemented! It would be tricky to keep track of the
    // scope of the file mapping. We could do this through UserData
    // on the DataSourceSurface but we don't need this right now.
  }

 protected:
  void UpdatedInternal(const nsIntRegion* aRegion = nullptr) override;

  HANDLE mFileMapping;
};

}  // namespace layers
}  // namespace mozilla

#endif /* MOZILLA_GFX_TEXTUREDIB_H */
