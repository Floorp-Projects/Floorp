/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
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

class DIBTextureData : public TextureData
{
public:
  virtual bool Lock(OpenMode, FenceHandle*) override { return true; }

  virtual void Unlock() override {}

  virtual void FillInfo(TextureData::Info& aInfo) const override;

  virtual already_AddRefed<gfx::DrawTarget> BorrowDrawTarget() override;

  static
  DIBTextureData* Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                         ClientIPCAllocator* aAllocator);

protected:
  DIBTextureData(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                 gfxWindowsSurface* aSurface)
  : mSurface(aSurface)
  , mSize(aSize)
  , mFormat(aFormat)
  {
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
class TextureHostDirectUpload : public TextureHost
{
public:
  TextureHostDirectUpload(TextureFlags aFlags,
                          gfx::SurfaceFormat aFormat,
                          gfx::IntSize aSize)
    : TextureHost(aFlags)
    , mFormat(aFormat)
    , mSize(aSize)
    , mIsLocked(false)
  { }

  virtual void DeallocateDeviceData() override;

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual Compositor* GetCompositor() override { return mCompositor; }

  virtual gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual bool Lock() override;

  virtual void Unlock() override;

  virtual bool HasIntermediateBuffer() const { return true; }

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTexture) override;

protected:
  RefPtr<DataTextureSource> mTextureSource;
  RefPtr<Compositor> mCompositor;
  gfx::SurfaceFormat mFormat;
  gfx::IntSize mSize;
  bool mIsLocked;
};

class DIBTextureHost : public TextureHostDirectUpload
{
public:
  DIBTextureHost(TextureFlags aFlags,
                 const SurfaceDescriptorDIB& aDescriptor);

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override
  {
    return nullptr; // TODO: cf bug 872568
  }

protected:
  virtual void UpdatedInternal(const nsIntRegion* aRegion = nullptr) override;

  RefPtr<gfxWindowsSurface> mSurface;
};

class TextureHostFileMapping : public TextureHostDirectUpload
{
public:
  TextureHostFileMapping(TextureFlags aFlags,
                         const SurfaceDescriptorFileMapping& aDescriptor);
  ~TextureHostFileMapping();

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override
  {
    MOZ_CRASH("GFX: TextureHostFileMapping::GetAsSurface not implemented");
                 // Not implemented! It would be tricky to keep track of the
                 // scope of the file mapping. We could do this through UserData
                 // on the DataSourceSurface but we don't need this right now.
  }

protected:
  virtual void UpdatedInternal(const nsIntRegion* aRegion = nullptr) override;

  HANDLE mFileMapping;
};

}
}

#endif /* MOZILLA_GFX_TEXTUREDIB_H */
