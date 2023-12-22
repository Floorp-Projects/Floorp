/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BufferTexture.h"

#include <utility>

#include "libyuv.h"
#include "mozilla/fallible.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/TextureForwarder.h"

#include "gfxPlatform.h"

#ifdef MOZ_WIDGET_GTK
#  include "gfxPlatformGtk.h"
#endif

using mozilla::ipc::IShmemAllocator;

namespace mozilla {
namespace layers {

class MemoryTextureData : public BufferTextureData {
 public:
  static MemoryTextureData* Create(gfx::IntSize aSize,
                                   gfx::SurfaceFormat aFormat,
                                   gfx::BackendType aMoz2DBackend,
                                   LayersBackend aLayersBackend,
                                   TextureFlags aFlags,
                                   TextureAllocationFlags aAllocFlags,
                                   IShmemAllocator* aAllocator);

  virtual TextureData* CreateSimilar(
      LayersIPCChannel* aAllocator, LayersBackend aLayersBackend,
      TextureFlags aFlags = TextureFlags::DEFAULT,
      TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override;

  virtual bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  virtual void Deallocate(LayersIPCChannel*) override;

  MemoryTextureData(const BufferDescriptor& aDesc,
                    gfx::BackendType aMoz2DBackend, uint8_t* aBuffer,
                    size_t aBufferSize, bool aAutoDeallocate = false)
      : BufferTextureData(aDesc, aMoz2DBackend),
        mBuffer(aBuffer),
        mBufferSize(aBufferSize),
        mAutoDeallocate(aAutoDeallocate) {
    MOZ_ASSERT(aBuffer);
    MOZ_ASSERT(aBufferSize);
  }

  virtual ~MemoryTextureData() override {
    if (mAutoDeallocate) {
      Deallocate(nullptr);
    }
  }

  virtual uint8_t* GetBuffer() override { return mBuffer; }

  virtual size_t GetBufferSize() override { return mBufferSize; }

 protected:
  uint8_t* mBuffer;
  size_t mBufferSize;
  bool mAutoDeallocate;
};

class ShmemTextureData : public BufferTextureData {
 public:
  static ShmemTextureData* Create(gfx::IntSize aSize,
                                  gfx::SurfaceFormat aFormat,
                                  gfx::BackendType aMoz2DBackend,
                                  LayersBackend aLayersBackend,
                                  TextureFlags aFlags,
                                  TextureAllocationFlags aAllocFlags,
                                  IShmemAllocator* aAllocator);

  virtual TextureData* CreateSimilar(
      LayersIPCChannel* aAllocator, LayersBackend aLayersBackend,
      TextureFlags aFlags = TextureFlags::DEFAULT,
      TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override;

  virtual bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  virtual void Deallocate(LayersIPCChannel* aAllocator) override;

  ShmemTextureData(const BufferDescriptor& aDesc,
                   gfx::BackendType aMoz2DBackend, mozilla::ipc::Shmem aShmem)
      : BufferTextureData(aDesc, aMoz2DBackend), mShmem(aShmem) {
    MOZ_ASSERT(mShmem.Size<uint8_t>());
  }

  virtual uint8_t* GetBuffer() override { return mShmem.get<uint8_t>(); }

  virtual size_t GetBufferSize() override { return mShmem.Size<uint8_t>(); }

 protected:
  mozilla::ipc::Shmem mShmem;
};

BufferTextureData* BufferTextureData::Create(
    gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
    gfx::BackendType aMoz2DBackend, LayersBackend aLayersBackend,
    TextureFlags aFlags, TextureAllocationFlags aAllocFlags,
    mozilla::ipc::IShmemAllocator* aAllocator, bool aIsSameProcess) {
  if (!aAllocator || aIsSameProcess) {
    return MemoryTextureData::Create(aSize, aFormat, aMoz2DBackend,
                                     aLayersBackend, aFlags, aAllocFlags,
                                     aAllocator);
  } else {
    return ShmemTextureData::Create(aSize, aFormat, aMoz2DBackend,
                                    aLayersBackend, aFlags, aAllocFlags,
                                    aAllocator);
  }
}

BufferTextureData* BufferTextureData::CreateInternal(
    LayersIPCChannel* aAllocator, const BufferDescriptor& aDesc,
    gfx::BackendType aMoz2DBackend, int32_t aBufferSize,
    TextureFlags aTextureFlags) {
  if (!aAllocator || aAllocator->IsSameProcess()) {
    uint8_t* buffer = new (fallible) uint8_t[aBufferSize];
    if (!buffer) {
      return nullptr;
    }

    GfxMemoryImageReporter::DidAlloc(buffer);

    return new MemoryTextureData(aDesc, aMoz2DBackend, buffer, aBufferSize);
  } else {
    ipc::Shmem shm;
    if (!aAllocator->AllocUnsafeShmem(aBufferSize, &shm)) {
      return nullptr;
    }

    return new ShmemTextureData(aDesc, aMoz2DBackend, shm);
  }
}

BufferTextureData* BufferTextureData::CreateForYCbCr(
    KnowsCompositor* aAllocator, const gfx::IntRect& aDisplay,
    const gfx::IntSize& aYSize, uint32_t aYStride,
    const gfx::IntSize& aCbCrSize, uint32_t aCbCrStride, StereoMode aStereoMode,
    gfx::ColorDepth aColorDepth, gfx::YUVColorSpace aYUVColorSpace,
    gfx::ColorRange aColorRange, gfx::ChromaSubsampling aSubsampling,
    TextureFlags aTextureFlags) {
  uint32_t bufSize = ImageDataSerializer::ComputeYCbCrBufferSize(
      aYSize, aYStride, aCbCrSize, aCbCrStride);
  if (bufSize == 0) {
    return nullptr;
  }

  uint32_t yOffset;
  uint32_t cbOffset;
  uint32_t crOffset;
  ImageDataSerializer::ComputeYCbCrOffsets(aYStride, aYSize.height, aCbCrStride,
                                           aCbCrSize.height, yOffset, cbOffset,
                                           crOffset);

  YCbCrDescriptor descriptor =
      YCbCrDescriptor(aDisplay, aYSize, aYStride, aCbCrSize, aCbCrStride,
                      yOffset, cbOffset, crOffset, aStereoMode, aColorDepth,
                      aYUVColorSpace, aColorRange, aSubsampling);

  return CreateInternal(
      aAllocator ? aAllocator->GetTextureForwarder() : nullptr, descriptor,
      gfx::BackendType::NONE, bufSize, aTextureFlags);
}

void BufferTextureData::FillInfo(TextureData::Info& aInfo) const {
  aInfo.size = GetSize();
  aInfo.format = GetFormat();
  aInfo.hasSynchronization = false;
  aInfo.canExposeMappedData = true;

  switch (aInfo.format) {
    case gfx::SurfaceFormat::YUV:
    case gfx::SurfaceFormat::UNKNOWN:
      aInfo.supportsMoz2D = false;
      break;
    default:
      aInfo.supportsMoz2D = true;
  }
}

gfx::IntSize BufferTextureData::GetSize() const {
  return ImageDataSerializer::SizeFromBufferDescriptor(mDescriptor);
}

gfx::IntRect BufferTextureData::GetPictureRect() const {
  return ImageDataSerializer::RectFromBufferDescriptor(mDescriptor);
}

Maybe<gfx::IntSize> BufferTextureData::GetYSize() const {
  return ImageDataSerializer::YSizeFromBufferDescriptor(mDescriptor);
}

Maybe<gfx::IntSize> BufferTextureData::GetCbCrSize() const {
  return ImageDataSerializer::CbCrSizeFromBufferDescriptor(mDescriptor);
}

Maybe<int32_t> BufferTextureData::GetYStride() const {
  return ImageDataSerializer::YStrideFromBufferDescriptor(mDescriptor);
}

Maybe<int32_t> BufferTextureData::GetCbCrStride() const {
  return ImageDataSerializer::CbCrStrideFromBufferDescriptor(mDescriptor);
}

Maybe<gfx::YUVColorSpace> BufferTextureData::GetYUVColorSpace() const {
  return ImageDataSerializer::YUVColorSpaceFromBufferDescriptor(mDescriptor);
}

Maybe<gfx::ColorDepth> BufferTextureData::GetColorDepth() const {
  return ImageDataSerializer::ColorDepthFromBufferDescriptor(mDescriptor);
}

Maybe<StereoMode> BufferTextureData::GetStereoMode() const {
  return ImageDataSerializer::StereoModeFromBufferDescriptor(mDescriptor);
}

Maybe<gfx::ChromaSubsampling> BufferTextureData::GetChromaSubsampling() const {
  return ImageDataSerializer::ChromaSubsamplingFromBufferDescriptor(
      mDescriptor);
}

gfx::SurfaceFormat BufferTextureData::GetFormat() const {
  return ImageDataSerializer::FormatFromBufferDescriptor(mDescriptor);
}

already_AddRefed<gfx::DrawTarget> BufferTextureData::BorrowDrawTarget() {
  if (mDescriptor.type() != BufferDescriptor::TRGBDescriptor) {
    return nullptr;
  }

  const RGBDescriptor& rgb = mDescriptor.get_RGBDescriptor();

  uint32_t stride = ImageDataSerializer::GetRGBStride(rgb);
  RefPtr<gfx::DrawTarget> dt;
  if (gfx::Factory::DoesBackendSupportDataDrawtarget(mMoz2DBackend)) {
    dt = gfx::Factory::CreateDrawTargetForData(
        mMoz2DBackend, GetBuffer(), rgb.size(), stride, rgb.format(), true);
  }
  if (!dt) {
    // Fall back to supported platform backend.  Note that mMoz2DBackend
    // does not match the draw target type.
    dt = gfxPlatform::CreateDrawTargetForData(GetBuffer(), rgb.size(), stride,
                                              rgb.format(), true);
  }

  if (!dt) {
    gfxCriticalNote << "BorrowDrawTarget failure, original backend "
                    << (int)mMoz2DBackend;
  }

  return dt.forget();
}

bool BufferTextureData::BorrowMappedData(MappedTextureData& aData) {
  if (GetFormat() == gfx::SurfaceFormat::YUV) {
    return false;
  }

  gfx::IntSize size = GetSize();

  aData.data = GetBuffer();
  aData.size = size;
  aData.format = GetFormat();
  aData.stride =
      ImageDataSerializer::ComputeRGBStride(aData.format, size.width);

  return true;
}

bool BufferTextureData::BorrowMappedYCbCrData(MappedYCbCrTextureData& aMap) {
  if (mDescriptor.type() != BufferDescriptor::TYCbCrDescriptor) {
    return false;
  }

  const YCbCrDescriptor& desc = mDescriptor.get_YCbCrDescriptor();

  uint8_t* data = GetBuffer();
  auto ySize = desc.ySize();
  auto cbCrSize = desc.cbCrSize();

  aMap.stereoMode = desc.stereoMode();
  aMap.metadata = nullptr;
  uint32_t bytesPerPixel =
      BytesPerPixel(SurfaceFormatForColorDepth(desc.colorDepth()));

  aMap.y.data = data + desc.yOffset();
  aMap.y.size = ySize;
  aMap.y.stride = desc.yStride();
  aMap.y.skip = 0;
  aMap.y.bytesPerPixel = bytesPerPixel;

  aMap.cb.data = data + desc.cbOffset();
  aMap.cb.size = cbCrSize;
  aMap.cb.stride = desc.cbCrStride();
  aMap.cb.skip = 0;
  aMap.cb.bytesPerPixel = bytesPerPixel;

  aMap.cr.data = data + desc.crOffset();
  aMap.cr.size = cbCrSize;
  aMap.cr.stride = desc.cbCrStride();
  aMap.cr.skip = 0;
  aMap.cr.bytesPerPixel = bytesPerPixel;

  return true;
}

bool BufferTextureData::UpdateFromSurface(gfx::SourceSurface* aSurface) {
  if (mDescriptor.type() != BufferDescriptor::TRGBDescriptor) {
    return false;
  }
  const RGBDescriptor& rgb = mDescriptor.get_RGBDescriptor();

  uint32_t stride = ImageDataSerializer::GetRGBStride(rgb);
  RefPtr<gfx::DataSourceSurface> surface =
      gfx::Factory::CreateWrappingDataSourceSurface(GetBuffer(), stride,
                                                    rgb.size(), rgb.format());

  if (!surface) {
    gfxCriticalError() << "Failed to get serializer as surface!";
    return false;
  }

  RefPtr<gfx::DataSourceSurface> srcSurf = aSurface->GetDataSurface();

  if (!srcSurf) {
    gfxCriticalError() << "Failed to GetDataSurface in UpdateFromSurface (BT).";
    return false;
  }

  if (surface->GetSize() != srcSurf->GetSize() ||
      surface->GetFormat() != srcSurf->GetFormat()) {
    gfxCriticalError() << "Attempt to update texture client from a surface "
                          "with a different size or format (BT)! This: "
                       << surface->GetSize() << " " << surface->GetFormat()
                       << " Other: " << aSurface->GetSize() << " "
                       << aSurface->GetFormat();
    return false;
  }

  gfx::DataSourceSurface::MappedSurface sourceMap;
  gfx::DataSourceSurface::MappedSurface destMap;
  if (!srcSurf->Map(gfx::DataSourceSurface::READ, &sourceMap)) {
    gfxCriticalError()
        << "Failed to map source surface for UpdateFromSurface (BT).";
    return false;
  }

  if (!surface->Map(gfx::DataSourceSurface::WRITE, &destMap)) {
    srcSurf->Unmap();
    gfxCriticalError()
        << "Failed to map destination surface for UpdateFromSurface.";
    return false;
  }

  for (int y = 0; y < srcSurf->GetSize().height; y++) {
    memcpy(destMap.mData + destMap.mStride * y,
           sourceMap.mData + sourceMap.mStride * y,
           srcSurf->GetSize().width * BytesPerPixel(srcSurf->GetFormat()));
  }

  srcSurf->Unmap();
  surface->Unmap();

  return true;
}

bool MemoryTextureData::Serialize(SurfaceDescriptor& aOutDescriptor) {
  MOZ_ASSERT(GetFormat() != gfx::SurfaceFormat::UNKNOWN);
  if (GetFormat() == gfx::SurfaceFormat::UNKNOWN) {
    return false;
  }

  uintptr_t ptr = reinterpret_cast<uintptr_t>(mBuffer);
  aOutDescriptor = SurfaceDescriptorBuffer(mDescriptor, MemoryOrShmem(ptr));

  return true;
}

static bool InitBuffer(uint8_t* buf, size_t bufSize, gfx::SurfaceFormat aFormat,
                       TextureAllocationFlags aAllocFlags, bool aAlreadyZero) {
  if (!buf) {
    gfxDebug() << "BufferTextureData: Failed to allocate " << bufSize
               << " bytes";
    return false;
  }

  if (aAllocFlags & ALLOC_CLEAR_BUFFER) {
    if (aFormat == gfx::SurfaceFormat::B8G8R8X8) {
      // Even though BGRX was requested, XRGB_UINT32 is what is meant,
      // so use 0xFF000000 to put alpha in the right place.
      libyuv::ARGBRect(buf, bufSize, 0, 0, bufSize / sizeof(uint32_t), 1,
                       0xFF000000);
    } else if (!aAlreadyZero) {
      memset(buf, 0, bufSize);
    }
  }

  return true;
}

MemoryTextureData* MemoryTextureData::Create(gfx::IntSize aSize,
                                             gfx::SurfaceFormat aFormat,
                                             gfx::BackendType aMoz2DBackend,
                                             LayersBackend aLayersBackend,
                                             TextureFlags aFlags,
                                             TextureAllocationFlags aAllocFlags,
                                             IShmemAllocator* aAllocator) {
  // Should have used CreateForYCbCr.
  MOZ_ASSERT(aFormat != gfx::SurfaceFormat::YUV);

  if (aSize.width <= 0 || aSize.height <= 0) {
    gfxDebug() << "Asking for buffer of invalid size " << aSize.width << "x"
               << aSize.height;
    return nullptr;
  }

  uint32_t bufSize = ImageDataSerializer::ComputeRGBBufferSize(aSize, aFormat);
  if (!bufSize) {
    return nullptr;
  }

  uint8_t* buf = new (fallible) uint8_t[bufSize];
  if (!InitBuffer(buf, bufSize, aFormat, aAllocFlags, false)) {
    return nullptr;
  }

  GfxMemoryImageReporter::DidAlloc(buf);

  BufferDescriptor descriptor = RGBDescriptor(aSize, aFormat);

  // Remote textures are not managed by a texture client, so we need to ensure
  // that memory is freed when the owning MemoryTextureData goes away.
  bool autoDeallocate = !!(aFlags & TextureFlags::REMOTE_TEXTURE);
  return new MemoryTextureData(descriptor, aMoz2DBackend, buf, bufSize,
                               autoDeallocate);
}

void MemoryTextureData::Deallocate(LayersIPCChannel*) {
  MOZ_ASSERT(mBuffer);
  GfxMemoryImageReporter::WillFree(mBuffer);
  delete[] mBuffer;
  mBuffer = nullptr;
}

TextureData* MemoryTextureData::CreateSimilar(
    LayersIPCChannel* aAllocator, LayersBackend aLayersBackend,
    TextureFlags aFlags, TextureAllocationFlags aAllocFlags) const {
  return MemoryTextureData::Create(GetSize(), GetFormat(), mMoz2DBackend,
                                   aLayersBackend, aFlags, aAllocFlags,
                                   aAllocator);
}

bool ShmemTextureData::Serialize(SurfaceDescriptor& aOutDescriptor) {
  MOZ_ASSERT(GetFormat() != gfx::SurfaceFormat::UNKNOWN);
  if (GetFormat() == gfx::SurfaceFormat::UNKNOWN) {
    return false;
  }

  aOutDescriptor =
      SurfaceDescriptorBuffer(mDescriptor, MemoryOrShmem(std::move(mShmem)));

  return true;
}

ShmemTextureData* ShmemTextureData::Create(gfx::IntSize aSize,
                                           gfx::SurfaceFormat aFormat,
                                           gfx::BackendType aMoz2DBackend,
                                           LayersBackend aLayersBackend,
                                           TextureFlags aFlags,
                                           TextureAllocationFlags aAllocFlags,
                                           IShmemAllocator* aAllocator) {
  MOZ_ASSERT(aAllocator);
  // Should have used CreateForYCbCr.
  MOZ_ASSERT(aFormat != gfx::SurfaceFormat::YUV);

  if (!aAllocator) {
    return nullptr;
  }

  if (aSize.width <= 0 || aSize.height <= 0) {
    gfxDebug() << "Asking for buffer of invalid size " << aSize.width << "x"
               << aSize.height;
    return nullptr;
  }

  uint32_t bufSize = ImageDataSerializer::ComputeRGBBufferSize(aSize, aFormat);
  if (!bufSize) {
    return nullptr;
  }

  mozilla::ipc::Shmem shm;
  if (!aAllocator->AllocUnsafeShmem(bufSize, &shm)) {
    return nullptr;
  }

  uint8_t* buf = shm.get<uint8_t>();
  if (!InitBuffer(buf, bufSize, aFormat, aAllocFlags, true)) {
    return nullptr;
  }

  BufferDescriptor descriptor = RGBDescriptor(aSize, aFormat);

  return new ShmemTextureData(descriptor, aMoz2DBackend, shm);
}

TextureData* ShmemTextureData::CreateSimilar(
    LayersIPCChannel* aAllocator, LayersBackend aLayersBackend,
    TextureFlags aFlags, TextureAllocationFlags aAllocFlags) const {
  return ShmemTextureData::Create(GetSize(), GetFormat(), mMoz2DBackend,
                                  aLayersBackend, aFlags, aAllocFlags,
                                  aAllocator);
}

void ShmemTextureData::Deallocate(LayersIPCChannel* aAllocator) {
  aAllocator->DeallocShmem(mShmem);
}

}  // namespace layers
}  // namespace mozilla
