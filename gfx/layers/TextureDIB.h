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

class TextureClientDIB : public TextureClient
{
public:
  virtual bool IsAllocated() const override { return !!mSurface; }

  virtual bool Lock(OpenMode aOpenMode) override;

  virtual void Unlock() override;

  virtual bool IsLocked() const override{ return mIsLocked; }

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  virtual bool CanExposeDrawTarget() const override { return true; }

  virtual gfx::DrawTarget* BorrowDrawTarget() override;

  virtual bool HasInternalBuffer() const override { return true; }

protected:
  TextureClientDIB(ISurfaceAllocator* aAllocator, gfx::SurfaceFormat aFormat, TextureFlags aFlags)
    : TextureClient(aAllocator, aFlags)
    , mFormat(aFormat)
    , mIsLocked(false)
  { }

  nsRefPtr<gfxWindowsSurface> mSurface;
  RefPtr<gfx::DrawTarget> mDrawTarget;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
  bool mIsLocked;
};

/**
  * Can only be drawn into through Cairo.
  * Prefer CairoTextureClient when possible.
  * The coresponding TextureHost depends on the compositor
  */
class TextureClientMemoryDIB : public TextureClientDIB
{
public:
  TextureClientMemoryDIB(ISurfaceAllocator* aAllocator,
                         gfx::SurfaceFormat aFormat,
                         TextureFlags aFlags);

  virtual ~TextureClientMemoryDIB();

  // TextureClient

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) override;

  virtual bool AllocateForSurface(gfx::IntSize aSize,
    TextureAllocationFlags aFlags = ALLOC_DEFAULT) override;

  virtual TemporaryRef<TextureClient>
  CreateSimilar(TextureFlags aFlags = TextureFlags::DEFAULT,
                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override;


};

/**
  * Can only be drawn into through Cairo.
  * Prefer CairoTextureClient when possible.
  * The coresponding TextureHost depends on the compositor
  */
class TextureClientShmemDIB : public TextureClientDIB
{
public:
  TextureClientShmemDIB(ISurfaceAllocator* aAllocator,
                        gfx::SurfaceFormat aFormat,
                        TextureFlags aFlags);

  virtual ~TextureClientShmemDIB();

  // TextureClient

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) override;

  virtual bool AllocateForSurface(gfx::IntSize aSize,
    TextureAllocationFlags aFlags = ALLOC_DEFAULT) override;

  virtual TemporaryRef<TextureClient>
  CreateSimilar(TextureFlags aFlags = TextureFlags::DEFAULT,
                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override;

protected:
  HANDLE mFileMapping;
  HANDLE mHostHandle;
  HDC mDC;
  HBITMAP mBitmap;
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

  virtual gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual bool Lock() override;

  virtual void Unlock() override;

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

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() override
  {
    return nullptr; // TODO: cf bug 872568
  }

protected:
  virtual void UpdatedInternal(const nsIntRegion* aRegion = nullptr) override;

  nsRefPtr<gfxWindowsSurface> mSurface;
};

class TextureHostFileMapping : public TextureHostDirectUpload
{
public:
  TextureHostFileMapping(TextureFlags aFlags,
                         const SurfaceDescriptorFileMapping& aDescriptor);
  ~TextureHostFileMapping();

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() override
  {
    MOZ_CRASH(); // Not implemented! It would be tricky to keep track of the
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
