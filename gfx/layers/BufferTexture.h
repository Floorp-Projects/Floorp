/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_BUFFERETEXTURE
#define MOZILLA_LAYERS_BUFFERETEXTURE

#include "mozilla/RefPtr.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/ipc/SharedMemory.h"
#include "mozilla/layers/TextureClient.h"

namespace mozilla {
namespace layers {

class BufferTextureData : public TextureData {
 public:
  // ShmemAllocator needs to implement IShmemAllocator and IsSameProcess,
  // as done in LayersIPCChannel and ISurfaceAllocator.
  template <typename ShmemAllocator>
  static BufferTextureData* Create(gfx::IntSize aSize,
                                   gfx::SurfaceFormat aFormat,
                                   gfx::BackendType aMoz2DBackend,
                                   LayersBackend aLayersBackend,
                                   TextureFlags aFlags,
                                   TextureAllocationFlags aAllocFlags,
                                   ShmemAllocator aAllocator);

  static BufferTextureData* CreateForYCbCr(
      KnowsCompositor* aAllocator, const gfx::IntRect& aDisplay,
      const gfx::IntSize& aYSize, uint32_t aYStride,
      const gfx::IntSize& aCbCrSize, uint32_t aCbCrStride,
      StereoMode aStereoMode, gfx::ColorDepth aColorDepth,
      gfx::YUVColorSpace aYUVColorSpace, gfx::ColorRange aColorRange,
      gfx::ChromaSubsampling aSubsampling, TextureFlags aTextureFlags);

  bool Lock(OpenMode aMode) override { return true; }

  void Unlock() override {}

  void FillInfo(TextureData::Info& aInfo) const override;

  already_AddRefed<gfx::DrawTarget> BorrowDrawTarget() override;

  bool BorrowMappedData(MappedTextureData& aMap) override;

  bool BorrowMappedYCbCrData(MappedYCbCrTextureData& aMap) override;

  // use TextureClient's default implementation
  bool UpdateFromSurface(gfx::SourceSurface* aSurface) override;

  BufferTextureData* AsBufferTextureData() override { return this; }

  Maybe<gfx::IntSize> GetYSize() const;

  Maybe<gfx::IntSize> GetCbCrSize() const;

  Maybe<int32_t> GetYStride() const;

  Maybe<int32_t> GetCbCrStride() const;

  Maybe<gfx::YUVColorSpace> GetYUVColorSpace() const;

  Maybe<gfx::ColorDepth> GetColorDepth() const;

  Maybe<StereoMode> GetStereoMode() const;

  Maybe<gfx::ChromaSubsampling> GetChromaSubsampling() const;

  gfx::IntRect GetPictureRect() const;

 protected:
  gfx::IntSize GetSize() const;

  gfx::SurfaceFormat GetFormat() const;

  static BufferTextureData* Create(
      gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
      gfx::BackendType aMoz2DBackend, LayersBackend aLayersBackend,
      TextureFlags aFlags, TextureAllocationFlags aAllocFlags,
      mozilla::ipc::IShmemAllocator* aAllocator, bool aIsSameProcess);

  static BufferTextureData* CreateInternal(LayersIPCChannel* aAllocator,
                                           const BufferDescriptor& aDesc,
                                           gfx::BackendType aMoz2DBackend,
                                           int32_t aBufferSize,
                                           TextureFlags aTextureFlags);

  virtual uint8_t* GetBuffer() = 0;
  virtual size_t GetBufferSize() = 0;

  BufferTextureData(const BufferDescriptor& aDescriptor,
                    gfx::BackendType aMoz2DBackend)
      : mDescriptor(aDescriptor), mMoz2DBackend(aMoz2DBackend) {}

  BufferDescriptor mDescriptor;
  gfx::BackendType mMoz2DBackend;
};

template <typename ShmemAllocator>
inline BufferTextureData* BufferTextureData::Create(
    gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
    gfx::BackendType aMoz2DBackend, LayersBackend aLayersBackend,
    TextureFlags aFlags, TextureAllocationFlags aAllocFlags,
    ShmemAllocator aAllocator) {
  return Create(aSize, aFormat, aMoz2DBackend, aLayersBackend, aFlags,
                aAllocFlags, aAllocator, aAllocator->IsSameProcess());
}

// nullptr allocator specialization
template <>
inline BufferTextureData* BufferTextureData::Create(
    gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
    gfx::BackendType aMoz2DBackend, LayersBackend aLayersBackend,
    TextureFlags aFlags, TextureAllocationFlags aAllocFlags, std::nullptr_t) {
  return Create(aSize, aFormat, aMoz2DBackend, aLayersBackend, aFlags,
                aAllocFlags, nullptr, true);
}

}  // namespace layers
}  // namespace mozilla

#endif
