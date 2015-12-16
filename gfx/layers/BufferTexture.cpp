/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BufferTexture.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/fallible.h"

namespace mozilla {
namespace layers {

class MemoryTextureData : public BufferTextureData
{
public:
  static MemoryTextureData* Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                   gfx::BackendType aMoz2DBackend,TextureFlags aFlags,
                                   TextureAllocationFlags aAllocFlags,
                                   ISurfaceAllocator* aAllocator);

  virtual TextureData*
  CreateSimilar(ISurfaceAllocator* aAllocator,
                TextureFlags aFlags = TextureFlags::DEFAULT,
                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override;

  virtual bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  virtual void Deallocate(ISurfaceAllocator*) override;

  MemoryTextureData(const BufferDescriptor& aDesc,
                    gfx::BackendType aMoz2DBackend,
                    uint8_t* aBuffer, size_t aBufferSize)
  : BufferTextureData(aDesc, aMoz2DBackend)
  , mBuffer(aBuffer)
  , mBufferSize(aBufferSize)
  {
    MOZ_ASSERT(aBuffer);
    MOZ_ASSERT(aBufferSize);
  }

  virtual uint8_t* GetBuffer() override { return mBuffer; }

  virtual size_t GetBufferSize() override { return mBufferSize; }

protected:
  uint8_t* mBuffer;
  size_t mBufferSize;
};

class ShmemTextureData : public BufferTextureData
{
public:
  static ShmemTextureData* Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                  gfx::BackendType aMoz2DBackend, TextureFlags aFlags,
                                  TextureAllocationFlags aAllocFlags,
                                  ISurfaceAllocator* aAllocator);

  virtual TextureData*
  CreateSimilar(ISurfaceAllocator* aAllocator,
                TextureFlags aFlags = TextureFlags::DEFAULT,
                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override;

  virtual bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  virtual void Deallocate(ISurfaceAllocator* aAllocator) override;

  ShmemTextureData(const BufferDescriptor& aDesc,
                   gfx::BackendType aMoz2DBackend, mozilla::ipc::Shmem aShmem)
  : BufferTextureData(aDesc, aMoz2DBackend)
  , mShmem(aShmem)
  {
    MOZ_ASSERT(mShmem.Size<uint8_t>());
  }

  virtual uint8_t* GetBuffer() override { return mShmem.get<uint8_t>(); }

  virtual size_t GetBufferSize() override { return mShmem.Size<uint8_t>(); }

protected:
  mozilla::ipc::Shmem mShmem;
};

BufferTextureData*
BufferTextureData::Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                          gfx::BackendType aMoz2DBackend, TextureFlags aFlags,
                          TextureAllocationFlags aAllocFlags,
                          ISurfaceAllocator* aAllocator)
{
  if (!aAllocator || aAllocator->IsSameProcess()) {
    return MemoryTextureData::Create(aSize, aFormat, aMoz2DBackend, aFlags, aAllocFlags, aAllocator);
  } else {
    return ShmemTextureData::Create(aSize, aFormat, aMoz2DBackend, aFlags, aAllocFlags, aAllocator);
  }
}

BufferTextureData*
BufferTextureData::CreateInternal(ISurfaceAllocator* aAllocator,
                                  const BufferDescriptor& aDesc,
                                  gfx::BackendType aMoz2DBackend,
                                  int32_t aBufferSize,
                                  TextureFlags aTextureFlags)
{
  if (!aAllocator || aAllocator->IsSameProcess()) {
    uint8_t* buffer = new (fallible) uint8_t[aBufferSize];
    if (!buffer) {
      return nullptr;
    }

    return new MemoryTextureData(aDesc, aMoz2DBackend, buffer, aBufferSize);
  } else {
    ipc::Shmem shm;
    if (!aAllocator->AllocUnsafeShmem(aBufferSize, OptimalShmemType(), &shm)) {
      return nullptr;
    }

    return new ShmemTextureData(aDesc, aMoz2DBackend, shm);
  }
}

BufferTextureData*
BufferTextureData::CreateForYCbCrWithBufferSize(ISurfaceAllocator* aAllocator,
                                                gfx::SurfaceFormat aFormat,
                                                int32_t aBufferSize,
                                                TextureFlags aTextureFlags)
{
  if (aBufferSize == 0 || !gfx::Factory::CheckBufferSize(aBufferSize)) {
    return nullptr;
  }

  // Initialize the metadata with something, even if it will have to be rewritten
  // afterwards since we don't know the dimensions of the texture at this point.
  BufferDescriptor desc = YCbCrDescriptor(gfx::IntSize(), gfx::IntSize(),
                                          0, 0, 0, StereoMode::MONO);

  return CreateInternal(aAllocator, desc, gfx::BackendType::NONE, aBufferSize,
                        aTextureFlags);
}

BufferTextureData*
BufferTextureData::CreateForYCbCr(ISurfaceAllocator* aAllocator,
                                  gfx::IntSize aYSize,
                                  gfx::IntSize aCbCrSize,
                                  StereoMode aStereoMode,
                                  TextureFlags aTextureFlags)
{
  uint32_t bufSize = ImageDataSerializer::ComputeYCbCrBufferSize(aYSize, aCbCrSize);
  if (bufSize == 0) {
    return nullptr;
  }

  uint32_t yOffset;
  uint32_t cbOffset;
  uint32_t crOffset;
  ImageDataSerializer::ComputeYCbCrOffsets(aYSize.width, aYSize.height,
                                          aCbCrSize.width, aCbCrSize.height,
                                          yOffset, cbOffset, crOffset);

  YCbCrDescriptor descriptor = YCbCrDescriptor(aYSize, aCbCrSize, yOffset, cbOffset,
                                               crOffset, aStereoMode);

 return CreateInternal(aAllocator, descriptor, gfx::BackendType::NONE, bufSize,
                       aTextureFlags);
}

gfx::IntSize
BufferTextureData::GetSize() const
{
  return ImageDataSerializer::SizeFromBufferDescriptor(mDescriptor);
}

gfx::SurfaceFormat
BufferTextureData::GetFormat() const
{
  return ImageDataSerializer::FormatFromBufferDescriptor(mDescriptor);
}

bool
BufferTextureData::SupportsMoz2D() const
{
  switch (GetFormat()) {
    case gfx::SurfaceFormat::YUV:
    case gfx::SurfaceFormat::NV12:
    case gfx::SurfaceFormat::UNKNOWN:
      return false;
    default:
      return true;
  }
}

already_AddRefed<gfx::DrawTarget>
BufferTextureData::BorrowDrawTarget()
{
  if (mDrawTarget) {
    mDrawTarget->SetTransform(gfx::Matrix());
    RefPtr<gfx::DrawTarget> dt = mDrawTarget;
    return dt.forget();
  }

  if (mDescriptor.type() != BufferDescriptor::TRGBDescriptor) {
    return nullptr;
  }

  const RGBDescriptor& rgb = mDescriptor.get_RGBDescriptor();

  uint32_t stride = ImageDataSerializer::GetRGBStride(rgb);
  mDrawTarget = gfx::Factory::CreateDrawTargetForData(mMoz2DBackend,
                                                      GetBuffer(), rgb.size(),
                                                      stride, rgb.format());

  if (mDrawTarget) {
    RefPtr<gfx::DrawTarget> dt = mDrawTarget;
    return dt.forget();
  }

  // TODO - should we warn? should we really fallback to cairo? perhaps
  // at least update mMoz2DBackend...
  if (mMoz2DBackend != gfx::BackendType::CAIRO) {
    mDrawTarget = gfx::Factory::CreateDrawTargetForData(gfx::BackendType::CAIRO,
                                                        GetBuffer(), rgb.size(),
                                                        stride, rgb.format());
  }

  if (!mDrawTarget) {
    gfxCriticalNote << "BorrowDrawTarget failure, original backend " << (int)mMoz2DBackend;
  }

  RefPtr<gfx::DrawTarget> dt = mDrawTarget;
  return dt.forget();
}

bool
BufferTextureData::BorrowMappedData(MappedTextureData& aData)
{
  if (GetFormat() == gfx::SurfaceFormat::YUV) {
    return false;
  }

  gfx::IntSize size = GetSize();

  aData.data = GetBuffer();
  aData.size = size;
  aData.format = GetFormat();
  aData.stride = ImageDataSerializer::ComputeRGBStride(aData.format, size.width);

  return true;
}

bool
BufferTextureData::BorrowMappedYCbCrData(MappedYCbCrTextureData& aMap)
{
  if (mDescriptor.type() != BufferDescriptor::TYCbCrDescriptor) {
    return false;
  }

  const YCbCrDescriptor& desc = mDescriptor.get_YCbCrDescriptor();

  uint8_t* data = GetBuffer();
  auto ySize = desc.ySize();
  auto cbCrSize = desc.cbCrSize();

  aMap.stereoMode = desc.stereoMode();
  aMap.metadata = nullptr;

  aMap.y.data = data + desc.yOffset();
  aMap.y.size = ySize;
  aMap.y.stride = ySize.width;
  aMap.y.skip = 0;

  aMap.cb.data = data + desc.cbOffset();
  aMap.cb.size = cbCrSize;
  aMap.cb.stride = cbCrSize.width;
  aMap.cb.skip = 0;

  aMap.cr.data = data + desc.crOffset();
  aMap.cr.size = cbCrSize;
  aMap.cr.stride = cbCrSize.width;
  aMap.cr.skip = 0;

  return true;
}

bool
BufferTextureData::UpdateFromSurface(gfx::SourceSurface* aSurface)
{
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
    gfxCriticalError() << "Failed to GetDataSurface in UpdateFromSurface.";
    return false;
  }

  if (surface->GetSize() != srcSurf->GetSize() || surface->GetFormat() != srcSurf->GetFormat()) {
    gfxCriticalError() << "Attempt to update texture client from a surface with a different size or format! This: " << surface->GetSize() << " " << surface->GetFormat() << " Other: " << aSurface->GetSize() << " " << aSurface->GetFormat();
    return false;
  }

  gfx::DataSourceSurface::MappedSurface sourceMap;
  gfx::DataSourceSurface::MappedSurface destMap;
  if (!srcSurf->Map(gfx::DataSourceSurface::READ, &sourceMap)) {
    gfxCriticalError() << "Failed to map source surface for UpdateFromSurface.";
    return false;
  }

  if (!surface->Map(gfx::DataSourceSurface::WRITE, &destMap)) {
    srcSurf->Unmap();
    gfxCriticalError() << "Failed to map destination surface for UpdateFromSurface.";
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

void
BufferTextureData::SetDesciptor(const BufferDescriptor& aDescriptor)
{
  MOZ_ASSERT(mDescriptor.type() == BufferDescriptor::TYCbCrDescriptor);
  MOZ_ASSERT(mDescriptor.get_YCbCrDescriptor().ySize() == gfx::IntSize());
  mDescriptor = aDescriptor;
}

bool
MemoryTextureData::Serialize(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(GetFormat() != gfx::SurfaceFormat::UNKNOWN);
  if (GetFormat() == gfx::SurfaceFormat::UNKNOWN) {
    return false;
  }

  uintptr_t ptr = reinterpret_cast<uintptr_t>(mBuffer);
  aOutDescriptor = SurfaceDescriptorBuffer(mDescriptor, MemoryOrShmem(ptr));

  return true;
}

static bool InitBuffer(uint8_t* buf, size_t bufSize, TextureAllocationFlags aAllocFlags)
{
  if (!buf) {
    gfxDebug() << "BufferTextureData: Failed to allocate " << bufSize << " bytes";
    return false;
  }

  if (aAllocFlags & ALLOC_CLEAR_BUFFER) {
    memset(buf, 0, bufSize);
  }
  if (aAllocFlags & ALLOC_CLEAR_BUFFER_WHITE) {
    memset(buf, 0xFF, bufSize);
  }

  return true;
}

MemoryTextureData*
MemoryTextureData::Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                          gfx::BackendType aMoz2DBackend, TextureFlags aFlags,
                          TextureAllocationFlags aAllocFlags,
                          ISurfaceAllocator*)
{
  // Should have used CreateForYCbCr.
  MOZ_ASSERT(aFormat != gfx::SurfaceFormat::YUV);

  if (aSize.width <= 0 || aSize.height <= 0) {
    gfxDebug() << "Asking for buffer of invalid size " << aSize.width << "x" << aSize.height;
    return nullptr;
  }

  uint32_t bufSize = ImageDataSerializer::ComputeRGBBufferSize(aSize, aFormat);
  if (!bufSize) {
    return nullptr;
  }

  uint8_t* buf = new (fallible) uint8_t[bufSize];
  if (!InitBuffer(buf, bufSize, aAllocFlags)) {
    return nullptr;
  }

  GfxMemoryImageReporter::DidAlloc(buf);

  BufferDescriptor descriptor = RGBDescriptor(aSize, aFormat);

  return new MemoryTextureData(descriptor, aMoz2DBackend, buf, bufSize);
}

void
MemoryTextureData::Deallocate(ISurfaceAllocator*)
{
  MOZ_ASSERT(mBuffer);
  GfxMemoryImageReporter::WillFree(mBuffer);
  delete [] mBuffer;
  mBuffer = nullptr;
}

TextureData*
MemoryTextureData::CreateSimilar(ISurfaceAllocator* aAllocator,
                                 TextureFlags aFlags,
                                 TextureAllocationFlags aAllocFlags) const
{
  return MemoryTextureData::Create(GetSize(), GetFormat(), mMoz2DBackend,
                                   aFlags, aAllocFlags, aAllocator);
}

bool
ShmemTextureData::Serialize(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(GetFormat() != gfx::SurfaceFormat::UNKNOWN);
  if (GetFormat() == gfx::SurfaceFormat::UNKNOWN) {
    return false;
  }

  aOutDescriptor = SurfaceDescriptorBuffer(mDescriptor, MemoryOrShmem(mShmem));

  return true;
}

ShmemTextureData*
ShmemTextureData::Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                         gfx::BackendType aMoz2DBackend, TextureFlags aFlags,
                         TextureAllocationFlags aAllocFlags,
                         ISurfaceAllocator* aAllocator)
{
  MOZ_ASSERT(aAllocator);
  // Should have used CreateForYCbCr.
  MOZ_ASSERT(aFormat != gfx::SurfaceFormat::YUV);

  if (!aAllocator) {
    return nullptr;
  }

  if (aSize.width <= 0 || aSize.height <= 0) {
    gfxDebug() << "Asking for buffer of invalid size " << aSize.width << "x" << aSize.height;
    return nullptr;
  }

  uint32_t bufSize = ImageDataSerializer::ComputeRGBBufferSize(aSize, aFormat);
  if (!bufSize) {
    return nullptr;
  }

  mozilla::ipc::Shmem shm;
  if (!aAllocator->AllocUnsafeShmem(bufSize, OptimalShmemType(), &shm)) {
    return nullptr;
  }

  uint8_t* buf = shm.get<uint8_t>();
  if (!InitBuffer(buf, bufSize, aAllocFlags)) {
    return nullptr;
  }

  BufferDescriptor descriptor = RGBDescriptor(aSize, aFormat);

  return new ShmemTextureData(descriptor, aMoz2DBackend, shm);

  return nullptr;
}

TextureData*
ShmemTextureData::CreateSimilar(ISurfaceAllocator* aAllocator,
                                TextureFlags aFlags,
                                TextureAllocationFlags aAllocFlags) const
{
  return ShmemTextureData::Create(GetSize(), GetFormat(), mMoz2DBackend,
                                  aFlags, aAllocFlags, aAllocator);
}

void
ShmemTextureData::Deallocate(ISurfaceAllocator* aAllocator)
{
  aAllocator->DeallocShmem(mShmem);
}

} // namespace
} // namespace
