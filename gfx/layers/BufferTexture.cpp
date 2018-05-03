/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BufferTexture.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/fallible.h"
#include "libyuv.h"

#ifdef MOZ_WIDGET_GTK
#include "gfxPlatformGtk.h"
#endif

namespace mozilla {
namespace layers {

class MemoryTextureData : public BufferTextureData
{
public:
  static MemoryTextureData* Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                   gfx::BackendType aMoz2DBackend,
                                   LayersBackend aLayersBackend,
                                   TextureFlags aFlags,
                                   TextureAllocationFlags aAllocFlags,
                                   LayersIPCChannel* aAllocator);

  virtual TextureData*
  CreateSimilar(LayersIPCChannel* aAllocator,
                LayersBackend aLayersBackend,
                TextureFlags aFlags = TextureFlags::DEFAULT,
                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override;

  virtual bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  virtual void Deallocate(LayersIPCChannel*) override;

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
                                  gfx::BackendType aMoz2DBackend,
                                  LayersBackend aLayersBackend,
                                  TextureFlags aFlags,
                                  TextureAllocationFlags aAllocFlags,
                                  LayersIPCChannel* aAllocator);

  virtual TextureData*
  CreateSimilar(LayersIPCChannel* aAllocator,
                LayersBackend aLayersBackend,
                TextureFlags aFlags = TextureFlags::DEFAULT,
                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override;

  virtual bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  virtual void Deallocate(LayersIPCChannel* aAllocator) override;

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

static bool UsingX11Compositor()
{
#ifdef MOZ_WIDGET_GTK
  return gfx::gfxVars::UseXRender();
#endif
  return false;
}

bool ComputeHasIntermediateBuffer(gfx::SurfaceFormat aFormat,
                                  LayersBackend aLayersBackend,
                                  bool aSupportsTextureDirectMapping)
{
  if (aSupportsTextureDirectMapping) {
    return false;
  }

  return aLayersBackend != LayersBackend::LAYERS_BASIC
      || UsingX11Compositor()
      || aFormat == gfx::SurfaceFormat::UNKNOWN;
}

BufferTextureData*
BufferTextureData::Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                          gfx::BackendType aMoz2DBackend,
                          LayersBackend aLayersBackend, TextureFlags aFlags,
                          TextureAllocationFlags aAllocFlags,
                          LayersIPCChannel* aAllocator)
{
  if (!aAllocator || aAllocator->IsSameProcess()) {
    return MemoryTextureData::Create(aSize, aFormat, aMoz2DBackend,
                                     aLayersBackend, aFlags,
                                     aAllocFlags, aAllocator);
  } else {
    return ShmemTextureData::Create(aSize, aFormat, aMoz2DBackend,
                                    aLayersBackend, aFlags,
                                    aAllocFlags, aAllocator);
  }
}

BufferTextureData*
BufferTextureData::CreateInternal(LayersIPCChannel* aAllocator,
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

    GfxMemoryImageReporter::DidAlloc(buffer);

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
BufferTextureData::CreateForYCbCrWithBufferSize(KnowsCompositor* aAllocator,
                                                int32_t aBufferSize,
                                                YUVColorSpace aYUVColorSpace,
                                                uint32_t aBitDepth,
                                                TextureFlags aTextureFlags)
{
  if (aBufferSize == 0 || !gfx::Factory::CheckBufferSize(aBufferSize)) {
    return nullptr;
  }

  bool hasIntermediateBuffer = aAllocator ? ComputeHasIntermediateBuffer(gfx::SurfaceFormat::YUV,
                                                                         aAllocator->GetCompositorBackendType(),
                                                                         aAllocator->SupportsTextureDirectMapping())
                                          : true;

  // Initialize the metadata with something, even if it will have to be rewritten
  // afterwards since we don't know the dimensions of the texture at this point.
  BufferDescriptor desc = YCbCrDescriptor(gfx::IntSize(), 0, gfx::IntSize(), 0,
                                          0, 0, 0, StereoMode::MONO,
                                          aYUVColorSpace,
                                          aBitDepth,
                                          hasIntermediateBuffer);

  return CreateInternal(aAllocator ? aAllocator->GetTextureForwarder() : nullptr,
                       desc, gfx::BackendType::NONE, aBufferSize, aTextureFlags);
}

BufferTextureData*
BufferTextureData::CreateForYCbCr(KnowsCompositor* aAllocator,
                                  gfx::IntSize aYSize,
                                  uint32_t aYStride,
                                  gfx::IntSize aCbCrSize,
                                  uint32_t aCbCrStride,
                                  StereoMode aStereoMode,
                                  YUVColorSpace aYUVColorSpace,
                                  uint32_t aBitDepth,
                                  TextureFlags aTextureFlags)
{
  uint32_t bufSize = ImageDataSerializer::ComputeYCbCrBufferSize(
    aYSize, aYStride, aCbCrSize, aCbCrStride);
  if (bufSize == 0) {
    return nullptr;
  }

  uint32_t yOffset;
  uint32_t cbOffset;
  uint32_t crOffset;
  ImageDataSerializer::ComputeYCbCrOffsets(aYStride, aYSize.height,
                                           aCbCrStride, aCbCrSize.height,
                                           yOffset, cbOffset, crOffset);

  bool hasIntermediateBuffer =
    aAllocator
      ? ComputeHasIntermediateBuffer(gfx::SurfaceFormat::YUV,
                                     aAllocator->GetCompositorBackendType(),
                                     aAllocator->SupportsTextureDirectMapping())
      : true;

  YCbCrDescriptor descriptor = YCbCrDescriptor(aYSize, aYStride,
                                               aCbCrSize, aCbCrStride,
                                               yOffset, cbOffset, crOffset,
                                               aStereoMode,
                                               aYUVColorSpace,
                                               aBitDepth,
                                               hasIntermediateBuffer);

  return CreateInternal(aAllocator ? aAllocator->GetTextureForwarder()
                                   : nullptr,
                        descriptor,
                        gfx::BackendType::NONE,
                        bufSize,
                        aTextureFlags);
}

void
BufferTextureData::FillInfo(TextureData::Info& aInfo) const
{
  aInfo.size = GetSize();
  aInfo.format = GetFormat();
  aInfo.hasSynchronization = false;
  aInfo.canExposeMappedData = true;

  if (mDescriptor.type() == BufferDescriptor::TYCbCrDescriptor) {
    aInfo.hasIntermediateBuffer = mDescriptor.get_YCbCrDescriptor().hasIntermediateBuffer();
  } else {
    aInfo.hasIntermediateBuffer = mDescriptor.get_RGBDescriptor().hasIntermediateBuffer();
  }

  switch (aInfo.format) {
    case gfx::SurfaceFormat::YUV:
    case gfx::SurfaceFormat::UNKNOWN:
      aInfo.supportsMoz2D = false;
      break;
    default:
      aInfo.supportsMoz2D = true;
  }
}

gfx::IntSize
BufferTextureData::GetSize() const
{
  return ImageDataSerializer::SizeFromBufferDescriptor(mDescriptor);
}

Maybe<gfx::IntSize>
BufferTextureData::GetCbCrSize() const
{
  return ImageDataSerializer::CbCrSizeFromBufferDescriptor(mDescriptor);
}

Maybe<YUVColorSpace>
BufferTextureData::GetYUVColorSpace() const
{
  return ImageDataSerializer::YUVColorSpaceFromBufferDescriptor(mDescriptor);
}

Maybe<uint32_t>
BufferTextureData::GetBitDepth() const
{
  return ImageDataSerializer::BitDepthFromBufferDescriptor(mDescriptor);
}

Maybe<StereoMode>
BufferTextureData::GetStereoMode() const
{
  return ImageDataSerializer::StereoModeFromBufferDescriptor(mDescriptor);
}

gfx::SurfaceFormat
BufferTextureData::GetFormat() const
{
  return ImageDataSerializer::FormatFromBufferDescriptor(mDescriptor);
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
  if (gfx::Factory::DoesBackendSupportDataDrawtarget(mMoz2DBackend)) {
    mDrawTarget = gfx::Factory::CreateDrawTargetForData(mMoz2DBackend,
                                                        GetBuffer(), rgb.size(),
                                                        stride, rgb.format(), true);
  } else {
    // Fall back to supported platform backend.  Note that mMoz2DBackend
    // does not match the draw target type.
    mDrawTarget = gfxPlatform::CreateDrawTargetForData(GetBuffer(), rgb.size(),
                                                       stride, rgb.format(),
                                                       true);
  }

  if (mDrawTarget) {
    RefPtr<gfx::DrawTarget> dt = mDrawTarget;
    return dt.forget();
  }

  // TODO - should we warn? should we really fallback to cairo? perhaps
  // at least update mMoz2DBackend...
  if (mMoz2DBackend != gfx::BackendType::CAIRO) {
    gfxCriticalNote << "Falling to CAIRO from " << (int)mMoz2DBackend;
    mDrawTarget = gfx::Factory::CreateDrawTargetForData(gfx::BackendType::CAIRO,
                                                        GetBuffer(), rgb.size(),
                                                        stride, rgb.format(), true);
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
  uint32_t bytesPerPixel = desc.bitDepth() > 8 ? 2 : 1;

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
    gfxCriticalError() << "Failed to GetDataSurface in UpdateFromSurface (BT).";
    return false;
  }

  if (surface->GetSize() != srcSurf->GetSize() || surface->GetFormat() != srcSurf->GetFormat()) {
    gfxCriticalError() << "Attempt to update texture client from a surface with a different size or format (BT)! This: " << surface->GetSize() << " " << surface->GetFormat() << " Other: " << aSurface->GetSize() << " " << aSurface->GetFormat();
    return false;
  }

  gfx::DataSourceSurface::MappedSurface sourceMap;
  gfx::DataSourceSurface::MappedSurface destMap;
  if (!srcSurf->Map(gfx::DataSourceSurface::READ, &sourceMap)) {
    gfxCriticalError() << "Failed to map source surface for UpdateFromSurface (BT).";
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

static bool
InitBuffer(uint8_t* buf, size_t bufSize,
           gfx::SurfaceFormat aFormat, TextureAllocationFlags aAllocFlags,
           bool aAlreadyZero)
{
  if (!buf) {
    gfxDebug() << "BufferTextureData: Failed to allocate " << bufSize << " bytes";
    return false;
  }

  if ((aAllocFlags & ALLOC_CLEAR_BUFFER) ||
      (aAllocFlags & ALLOC_CLEAR_BUFFER_BLACK)) {
    if (aFormat == gfx::SurfaceFormat::B8G8R8X8) {
      // Even though BGRX was requested, XRGB_UINT32 is what is meant,
      // so use 0xFF000000 to put alpha in the right place.
      libyuv::ARGBRect(buf, bufSize, 0, 0, bufSize / sizeof(uint32_t), 1, 0xFF000000);
    } else if (!aAlreadyZero) {
      memset(buf, 0, bufSize);
    }
  }

  if (aAllocFlags & ALLOC_CLEAR_BUFFER_WHITE) {
    memset(buf, 0xFF, bufSize);
  }

  return true;
}

MemoryTextureData*
MemoryTextureData::Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                          gfx::BackendType aMoz2DBackend,
                          LayersBackend aLayersBackend, TextureFlags aFlags,
                          TextureAllocationFlags aAllocFlags,
                          LayersIPCChannel* aAllocator)
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
  if (!InitBuffer(buf, bufSize, aFormat, aAllocFlags, false)) {
    return nullptr;
  }

  bool hasIntermediateBuffer = ComputeHasIntermediateBuffer(aFormat,
                                                            aLayersBackend,
                                                            aAllocFlags & ALLOC_ALLOW_DIRECT_MAPPING);

  GfxMemoryImageReporter::DidAlloc(buf);

  BufferDescriptor descriptor = RGBDescriptor(aSize, aFormat, hasIntermediateBuffer);

  return new MemoryTextureData(descriptor, aMoz2DBackend, buf, bufSize);
}

void
MemoryTextureData::Deallocate(LayersIPCChannel*)
{
  MOZ_ASSERT(mBuffer);
  GfxMemoryImageReporter::WillFree(mBuffer);
  delete [] mBuffer;
  mBuffer = nullptr;
}

TextureData*
MemoryTextureData::CreateSimilar(LayersIPCChannel* aAllocator,
                                 LayersBackend aLayersBackend,
                                 TextureFlags aFlags,
                                 TextureAllocationFlags aAllocFlags) const
{
  return MemoryTextureData::Create(GetSize(), GetFormat(), mMoz2DBackend,
                                   aLayersBackend, aFlags, aAllocFlags, aAllocator);
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
                         gfx::BackendType aMoz2DBackend,
                         LayersBackend aLayersBackend, TextureFlags aFlags,
                         TextureAllocationFlags aAllocFlags,
                         LayersIPCChannel* aAllocator)
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
  if (!InitBuffer(buf, bufSize, aFormat, aAllocFlags, true)) {
    return nullptr;
  }

  bool hasIntermediateBuffer = ComputeHasIntermediateBuffer(aFormat,
                                                            aLayersBackend,
                                                            aAllocFlags & ALLOC_ALLOW_DIRECT_MAPPING);

  BufferDescriptor descriptor = RGBDescriptor(aSize, aFormat, hasIntermediateBuffer);

  return new ShmemTextureData(descriptor, aMoz2DBackend, shm);

  return nullptr;
}

TextureData*
ShmemTextureData::CreateSimilar(LayersIPCChannel* aAllocator,
                                LayersBackend aLayersBackend,
                                TextureFlags aFlags,
                                TextureAllocationFlags aAllocFlags) const
{
  return ShmemTextureData::Create(GetSize(), GetFormat(), mMoz2DBackend,
                                  aLayersBackend, aFlags, aAllocFlags, aAllocator);
}

void
ShmemTextureData::Deallocate(LayersIPCChannel* aAllocator)
{
  aAllocator->DeallocShmem(mShmem);
}

} // namespace
} // namespace
