/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_BUFFERETEXTURE
#define MOZILLA_LAYERS_BUFFERETEXTURE

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

  static BufferTextureData* CreateForYCbCr(ISurfaceAllocator* aAllocator,
                                           gfx::IntSize aYSize,
                                           gfx::IntSize aCbCrSize,
                                           StereoMode aStereoMode,
                                           TextureFlags aTextureFlags);

  // It is generally better to use CreateForYCbCr instead.
  // This creates a half-initialized texture since we don't know the sizes and
  // offsets in the buffer.
  static BufferTextureData* CreateForYCbCrWithBufferSize(ISurfaceAllocator* aAllocator,
                                                         gfx::SurfaceFormat aFormat,
                                                         int32_t aSize,
                                                         TextureFlags aTextureFlags);

  virtual bool Lock(OpenMode aMode, FenceHandle*) override { return true; }

  virtual void Unlock() override {}

  virtual gfx::IntSize GetSize() const override;

  virtual gfx::SurfaceFormat GetFormat() const override;

  virtual already_AddRefed<gfx::DrawTarget> BorrowDrawTarget() override;

  virtual bool CanExposeMappedData() const override { return true; }

  virtual bool BorrowMappedData(MappedTextureData& aMap) override;

  virtual bool BorrowMappedYCbCrData(MappedYCbCrTextureData& aMap) override;

  virtual bool SupportsMoz2D() const override;

  virtual bool HasInternalBuffer() const override { return true; }

  // use TextureClient's default implementation
  virtual bool UpdateFromSurface(gfx::SourceSurface* aSurface) override;

  // Don't use this.
  void SetDesciptor(const BufferDescriptor& aDesc);

protected:
  static BufferTextureData* CreateInternal(ISurfaceAllocator* aAllocator,
                                           const BufferDescriptor& aDesc,
                                           gfx::BackendType aMoz2DBackend,
                                           int32_t aBufferSize,
                                           TextureFlags aTextureFlags);

  virtual uint8_t* GetBuffer() = 0;
  virtual size_t GetBufferSize() = 0;

  BufferTextureData(const BufferDescriptor& aDescriptor, gfx::BackendType aMoz2DBackend)
  : mDescriptor(aDescriptor)
  , mMoz2DBackend(aMoz2DBackend)
  {}

  RefPtr<gfx::DrawTarget> mDrawTarget;
  BufferDescriptor mDescriptor;
  gfx::BackendType mMoz2DBackend;
};

} // namespace
} // namespace

#endif
