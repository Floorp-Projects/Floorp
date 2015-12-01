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

  MemoryTextureData(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                    gfx::BackendType aMoz2DBackend,
                    uint8_t* aBuffer, size_t aBufferSize)
  : BufferTextureData(aSize, aFormat, aMoz2DBackend)
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

  ShmemTextureData(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                   gfx::BackendType aMoz2DBackend, mozilla::ipc::Shmem aShmem)
  : BufferTextureData(aSize, aFormat, aMoz2DBackend)
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
BufferTextureData::CreateWithBufferSize(ISurfaceAllocator* aAllocator,
                                        gfx::SurfaceFormat aFormat,
                                        size_t aSize,
                                        TextureFlags aTextureFlags)
{
  if (aSize == 0) {
    return nullptr;
  }

  BufferTextureData* data;
  if (!aAllocator || aAllocator->IsSameProcess()) {
    uint8_t* buffer = new (fallible) uint8_t[aSize];
    if (!buffer) {
      return nullptr;
    }

    data = new MemoryTextureData(gfx::IntSize(), aFormat, gfx::BackendType::NONE, buffer, aSize);
  } else {
    ipc::Shmem shm;
    if (!aAllocator->AllocUnsafeShmem(aSize, OptimalShmemType(), &shm)) {
      return nullptr;
    }

    data = new ShmemTextureData(gfx::IntSize(), aFormat, gfx::BackendType::NONE, shm);
  }

  // Initialize the metadata with something, even if it will have to be rewritten
  // afterwards since we don't know the dimensions of the texture at this point.
  if (aFormat == gfx::SurfaceFormat::YUV) {
    YCbCrImageDataSerializer serializer(data->GetBuffer(), data->GetBufferSize());
    serializer.InitializeBufferInfo(gfx::IntSize(0,0), gfx::IntSize(0,0), StereoMode::MONO);
  } else {
    ImageDataSerializer serializer(data->GetBuffer(), data->GetBufferSize());
    serializer.InitializeBufferInfo(gfx::IntSize(0, 0), aFormat);
  }

  return data;
}

BufferTextureData*
BufferTextureData::CreateForYCbCr(ISurfaceAllocator* aAllocator,
                                  gfx::IntSize aYSize,
                                  gfx::IntSize aCbCrSize,
                                  StereoMode aStereoMode,
                                  TextureFlags aTextureFlags)
{
  size_t bufSize = YCbCrImageDataSerializer::ComputeMinBufferSize(aYSize, aCbCrSize);
  BufferTextureData* texture = CreateWithBufferSize(aAllocator, gfx::SurfaceFormat::YUV,
                                                    bufSize, aTextureFlags);
  if (!texture) {
    return nullptr;
  }

  YCbCrImageDataSerializer serializer(texture->GetBuffer(), texture->GetBufferSize());
  serializer.InitializeBufferInfo(aYSize, aCbCrSize, aStereoMode);
  texture->mSize = aYSize;

  return texture;
}

bool
BufferTextureData::SupportsMoz2D() const
{
  switch (mFormat) {
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

  ImageDataSerializer serializer(GetBuffer(), GetBufferSize());
  if (!serializer.IsValid()) {
    return nullptr;
  }

  mDrawTarget = serializer.GetAsDrawTarget(mMoz2DBackend);
  if (mDrawTarget) {
    RefPtr<gfx::DrawTarget> dt = mDrawTarget;
    return dt.forget();
  }

  // TODO - should we warn? should we really fallback to cairo? perhaps
  // at least update mMoz2DBackend...
  mDrawTarget = serializer.GetAsDrawTarget(gfx::BackendType::CAIRO);
  if (!mDrawTarget) {
    gfxCriticalNote << "BorrowDrawTarget failure, original backend " << (int)mMoz2DBackend;
  }

  RefPtr<gfx::DrawTarget> dt = mDrawTarget;
  return dt.forget();
}

bool
BufferTextureData::BorrowMappedData(MappedTextureData& aData)
{
  if (mFormat == gfx::SurfaceFormat::YUV) {
    return false;
  }

  ImageDataDeserializer view(GetBuffer(), GetBufferSize());
  if (!view.IsValid()) {
    return false;
  }

  aData.data = view.GetData();
  aData.size = view.GetSize();
  aData.stride = view.GetStride();
  aData.format = mFormat;

  return true;
}

bool
BufferTextureData::BorrowMappedYCbCrData(MappedYCbCrTextureData& aMap)
{
  if (mFormat != gfx::SurfaceFormat::YUV) {
    return false;
  }

  YCbCrImageDataDeserializer view(GetBuffer(), GetBufferSize());
  if (!view.IsValid()) {
    return false;
  }

  aMap.stereoMode = view.GetStereoMode();
  aMap.metadata = GetBuffer();

  aMap.y.data = view.GetYData();
  aMap.y.size = view.GetYSize();
  aMap.y.stride = view.GetYStride();
  aMap.y.skip = 0;

  aMap.cb.data = view.GetCbData();
  aMap.cb.size = view.GetCbCrSize();
  aMap.cb.stride = view.GetCbCrStride();
  aMap.cb.skip = 0;

  aMap.cr.data = view.GetCrData();
  aMap.cr.size = view.GetCbCrSize();
  aMap.cr.stride = view.GetCbCrStride();
  aMap.cr.skip = 0;

  return true;
}

bool
BufferTextureData::UpdateFromSurface(gfx::SourceSurface* aSurface)
{
  ImageDataSerializer serializer(GetBuffer(), GetBufferSize());

  RefPtr<gfx::DataSourceSurface> surface = serializer.GetAsSurface();

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

bool
MemoryTextureData::Serialize(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(GetFormat() != gfx::SurfaceFormat::UNKNOWN);
  if (GetFormat() == gfx::SurfaceFormat::UNKNOWN) {
    return false;
  }

  aOutDescriptor = SurfaceDescriptorMemory(reinterpret_cast<uintptr_t>(mBuffer),
                                           GetFormat());
  return true;
}

static bool InitBuffer(uint8_t* buf, size_t bufSize,
                       gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                       TextureAllocationFlags aAllocFlags)
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

  ImageDataSerializer serializer(buf, bufSize);
  serializer.InitializeBufferInfo(aSize, aFormat);
  return true;
}

MemoryTextureData*
MemoryTextureData::Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                          gfx::BackendType aMoz2DBackend, TextureFlags aFlags,
                          TextureAllocationFlags aAllocFlags,
                          ISurfaceAllocator*)
{
  if (aSize.width <= 0 || aSize.height <= 0) {
    gfxDebug() << "Asking for buffer of invalid size " << aSize.width << "x" << aSize.height;
    return nullptr;
  }

  uint32_t bufSize = ImageDataSerializer::ComputeMinBufferSize(aSize, aFormat);
  if (!bufSize) {
    return nullptr;
  }

  uint8_t* buf = new (fallible) uint8_t[bufSize];

  if (InitBuffer(buf, bufSize, aSize, aFormat, aAllocFlags)) {
    GfxMemoryImageReporter::DidAlloc(buf);
    return new MemoryTextureData(aSize, aFormat, aMoz2DBackend, buf, bufSize);
  }

  return nullptr;
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
  return MemoryTextureData::Create(mSize, mFormat, mMoz2DBackend,
                                   aFlags, aAllocFlags, aAllocator);
}

bool
ShmemTextureData::Serialize(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(GetFormat() != gfx::SurfaceFormat::UNKNOWN);
  if (GetFormat() == gfx::SurfaceFormat::UNKNOWN) {
    return false;
  }

  aOutDescriptor = SurfaceDescriptorShmem(mShmem, GetFormat());

  return true;
}

ShmemTextureData*
ShmemTextureData::Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                         gfx::BackendType aMoz2DBackend, TextureFlags aFlags,
                         TextureAllocationFlags aAllocFlags,
                         ISurfaceAllocator* aAllocator)
{
  MOZ_ASSERT(aAllocator);
  if (!aAllocator) {
    return nullptr;
  }

  if (aSize.width <= 0 || aSize.height <= 0) {
    gfxDebug() << "Asking for buffer of invalid size " << aSize.width << "x" << aSize.height;
    return nullptr;
  }

  uint32_t bufSize = ImageDataSerializer::ComputeMinBufferSize(aSize, aFormat);
  if (!bufSize) {
    return nullptr;
  }

  mozilla::ipc::Shmem shm;
  if (!aAllocator->AllocUnsafeShmem(bufSize, OptimalShmemType(), &shm)) {
    return nullptr;
  }

  uint8_t* buf = shm.get<uint8_t>();

  if (InitBuffer(buf, bufSize, aSize, aFormat, aAllocFlags)) {
    return new ShmemTextureData(aSize, aFormat, aMoz2DBackend, shm);
  }

  return nullptr;
}

TextureData*
ShmemTextureData::CreateSimilar(ISurfaceAllocator* aAllocator,
                                TextureFlags aFlags,
                                TextureAllocationFlags aAllocFlags) const
{
  return ShmemTextureData::Create(mSize, mFormat, mMoz2DBackend,
                                  aFlags, aAllocFlags, aAllocator);
}

void
ShmemTextureData::Deallocate(ISurfaceAllocator* aAllocator)
{
  aAllocator->DeallocShmem(mShmem);
}

} // namespace
} // namespace
