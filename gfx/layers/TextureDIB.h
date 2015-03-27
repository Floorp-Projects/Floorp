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

/**
  * Can only be drawn into through Cairo.
  * Prefer CairoTextureClient when possible.
  * The coresponding TextureHost depends on the compositor
  */
class DIBTextureClient : public TextureClient
{
public:
  DIBTextureClient(ISurfaceAllocator* aAllocator,
                   gfx::SurfaceFormat aFormat,
                   TextureFlags aFlags);

  virtual ~DIBTextureClient();

  // TextureClient

  virtual bool IsAllocated() const override { return !!mSurface; }

  virtual bool Lock(OpenMode aOpenMode) override;

  virtual void Unlock() override;

  virtual bool IsLocked() const override{ return mIsLocked; }

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) override;

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  virtual bool CanExposeDrawTarget() const override { return true; }

  virtual gfx::DrawTarget* BorrowDrawTarget() override;

  virtual bool AllocateForSurface(gfx::IntSize aSize,
    TextureAllocationFlags aFlags = ALLOC_DEFAULT) override;

  virtual bool HasInternalBuffer() const override { return true; }

  virtual TemporaryRef<TextureClient>
  CreateSimilar(TextureFlags aFlags = TextureFlags::DEFAULT,
                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override;

protected:
  nsRefPtr<gfxWindowsSurface> mSurface;
  RefPtr<gfx::DrawTarget> mDrawTarget;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
  bool mIsLocked;
};

class DIBTextureHost : public TextureHost
{
public:
  DIBTextureHost(TextureFlags aFlags,
                 const SurfaceDescriptorDIB& aDescriptor);

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTexture) override;

  virtual void DeallocateDeviceData() override;

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual bool Lock() override;

  virtual void Unlock() override;

  virtual void Updated(const nsIntRegion* aRegion = nullptr) override;

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() override
  {
    return nullptr; // TODO: cf bug 872568
  }

protected:
  nsRefPtr<gfxWindowsSurface> mSurface;
  RefPtr<DataTextureSource> mTextureSource;
  RefPtr<Compositor> mCompositor;
  gfx::SurfaceFormat mFormat;
  gfx::IntSize mSize;
  bool mIsLocked;
};

}
}

#endif /* MOZILLA_GFX_TEXTUREDIB_H */
