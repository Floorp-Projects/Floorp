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

bool ComputeHasIntermediateBuffer(gfx::SurfaceFormat aFormat,
                                  LayersBackend aLayersBackend);

class BufferTextureData : public TextureData
{
public:
  static BufferTextureData* Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                   gfx::BackendType aMoz2DBackend,
                                   LayersBackend aLayersBackend,
                                   TextureFlags aFlags,
                                   TextureAllocationFlags aAllocFlags,
                                   LayersIPCChannel* aAllocator);

  static BufferTextureData* CreateForYCbCr(KnowsCompositor* aAllocator,
                                           gfx::IntSize aYSize,
                                           gfx::IntSize aCbCrSize,
                                           StereoMode aStereoMode,
                                           YUVColorSpace aYUVColorSpace,
                                           TextureFlags aTextureFlags);

  // It is generally better to use CreateForYCbCr instead.
  // This creates a half-initialized texture since we don't know the sizes and
  // offsets in the buffer.
  static BufferTextureData* CreateForYCbCrWithBufferSize(KnowsCompositor* aAllocator,
                                                         int32_t aSize,
                                                         YUVColorSpace aYUVColorSpace,
                                                         TextureFlags aTextureFlags);

  virtual bool Lock(OpenMode aMode) override { return true; }

  virtual void Unlock() override {}

  virtual void FillInfo(TextureData::Info& aInfo) const override;

  virtual already_AddRefed<gfx::DrawTarget> BorrowDrawTarget() override;

  virtual bool BorrowMappedData(MappedTextureData& aMap) override;

  virtual bool BorrowMappedYCbCrData(MappedYCbCrTextureData& aMap) override;

  // use TextureClient's default implementation
  virtual bool UpdateFromSurface(gfx::SourceSurface* aSurface) override;

  virtual BufferTextureData* AsBufferTextureData() override { return this; }

  // Don't use this.
  void SetDesciptor(const BufferDescriptor& aDesc);

  Maybe<gfx::IntSize> GetCbCrSize() const;

  Maybe<YUVColorSpace> GetYUVColorSpace() const;

  Maybe<StereoMode> GetStereoMode() const;

protected:
  gfx::IntSize GetSize() const;

  gfx::SurfaceFormat GetFormat() const;

  static BufferTextureData* CreateInternal(LayersIPCChannel* aAllocator,
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
