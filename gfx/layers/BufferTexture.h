/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_BUFFERETEXTURE
#define MOZILLA_LAYERS_BUFFERETEXTURE

#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/YCbCrImageDataSerializer.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/ipc/SharedMemory.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace layers {

class BufferTextureData : public TextureData
{
public:
  static BufferTextureData* Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                   gfx::BackendType aMoz2DBackend,TextureFlags aFlags,
                                   TextureAllocationFlags aAllocFlags,
                                   ISurfaceAllocator* aAllocator);

  static BufferTextureData* CreateWithBufferSize(ISurfaceAllocator* aAllocator,
                                                 gfx::SurfaceFormat aFormat,
                                                 size_t aSize,
                                                 TextureFlags aTextureFlags);

  static BufferTextureData* CreateForYCbCr(ISurfaceAllocator* aAllocator,
                                           gfx::IntSize aYSize,
                                           gfx::IntSize aCbCrSize,
                                           StereoMode aStereoMode,
                                           TextureFlags aTextureFlags);

  virtual bool Lock(OpenMode aMode, FenceHandle*) override { return true; }

  virtual void Unlock() override {}

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  virtual already_AddRefed<gfx::DrawTarget> BorrowDrawTarget() override;

  virtual bool CanExposeMappedData() const override { return true; }

  virtual bool BorrowMappedData(MappedTextureData& aMap) override;

  virtual bool BorrowMappedYCbCrData(MappedYCbCrTextureData& aMap) override;

  virtual bool SupportsMoz2D() const override;

  virtual bool HasInternalBuffer() const override { return true; }

  // use TextureClient's default implementation
  virtual bool UpdateFromSurface(gfx::SourceSurface* aSurface) override;

protected:
  virtual uint8_t* GetBuffer() = 0;
  virtual size_t GetBufferSize() = 0;

  BufferTextureData(gfx::IntSize aSize, gfx::SurfaceFormat aFormat, gfx::BackendType aMoz2DBackend)
  : mSize(aSize)
  , mFormat(aFormat)
  , mMoz2DBackend(aMoz2DBackend)
  {}

  RefPtr<gfx::DrawTarget> mDrawTarget;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
  gfx::BackendType mMoz2DBackend;
};

} // namespace
} // namespace

#endif
