/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_GONK

#include "mozilla/gfx/2D.h"
#include "mozilla/layers/AsyncTransactionTracker.h" // for AsyncTransactionTracker
#include "mozilla/layers/GrallocTextureClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/ShadowLayerUtilsGralloc.h"
#include "mozilla/layers/SharedBufferManagerChild.h"
#include "gfx2DGlue.h"
#include "gfxPrefs.h" // for gfxPrefs
#include "SharedSurfaceGralloc.h"

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
#include <ui/Fence.h>
#endif

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;
using namespace android;

static bool
DisableGralloc(SurfaceFormat aFormat, const gfx::IntSize& aSizeHint)
{
  if (gfxPrefs::DisableGralloc()) {
    return true;
  }
  if (aFormat == gfx::SurfaceFormat::A8) {
    return true;
  }

  return false;
}

gfx::SurfaceFormat
SurfaceFormatForPixelFormat(android::PixelFormat aFormat)
{
  switch (aFormat) {
  case PIXEL_FORMAT_RGBA_8888:
    return gfx::SurfaceFormat::R8G8B8A8;
  case PIXEL_FORMAT_BGRA_8888:
    return gfx::SurfaceFormat::B8G8R8A8;
  case PIXEL_FORMAT_RGBX_8888:
    return gfx::SurfaceFormat::R8G8B8X8;
  case PIXEL_FORMAT_RGB_565:
    return gfx::SurfaceFormat::R5G6B5_UINT16;
  case HAL_PIXEL_FORMAT_YV12:
    return gfx::SurfaceFormat::YUV;
  default:
    return gfx::SurfaceFormat::UNKNOWN;
  }
}

bool
IsGrallocRBSwapped(gfx::SurfaceFormat aFormat) {
  switch (aFormat) {
  case gfx::SurfaceFormat::B8G8R8A8:
  case gfx::SurfaceFormat::B8G8R8X8:
    return true;
  default:
    return false;
  }
}

uint32_t GetAndroidFormat(gfx::SurfaceFormat aFormat)
{
  switch (aFormat) {
  case gfx::SurfaceFormat::R8G8B8A8:
  case gfx::SurfaceFormat::B8G8R8A8:
    return android::PIXEL_FORMAT_RGBA_8888;
  case gfx::SurfaceFormat::R8G8B8X8:
  case gfx::SurfaceFormat::B8G8R8X8:
    return android::PIXEL_FORMAT_RGBX_8888;
  case gfx::SurfaceFormat::R5G6B5_UINT16:
    return android::PIXEL_FORMAT_RGB_565;
  case gfx::SurfaceFormat::YUV:
    return HAL_PIXEL_FORMAT_YV12;
  case gfx::SurfaceFormat::A8:
    NS_WARNING("gralloc does not support SurfaceFormat::A8");
  default:
    NS_WARNING("Unsupported surface format");
    return android::PIXEL_FORMAT_UNKNOWN;
  }
}

GrallocTextureData::GrallocTextureData(MaybeMagicGrallocBufferHandle aGrallocHandle,
                                       gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                       gfx::BackendType aMoz2DBackend)
: mSize(aSize)
, mFormat(aFormat)
, mMoz2DBackend(aMoz2DBackend)
, mGrallocHandle(aGrallocHandle)
, mMappedBuffer(nullptr)
, mMediaBuffer(nullptr)
{
  mGraphicBuffer = GetGraphicBufferFrom(aGrallocHandle);
  MOZ_COUNT_CTOR(GrallocTextureData);
}

GrallocTextureData::~GrallocTextureData()
{
  MOZ_COUNT_DTOR(GrallocTextureData);
}

void
GrallocTextureData::Deallocate(ClientIPCAllocator* aAllocator)
{
  MOZ_ASSERT(aAllocator);
  if (aAllocator && aAllocator->IPCOpen()) {
    SharedBufferManagerChild::GetSingleton()->DeallocGrallocBuffer(mGrallocHandle);
  }

  mGrallocHandle = null_t();
  mGraphicBuffer = nullptr;
}

void
GrallocTextureData::Forget(ClientIPCAllocator* aAllocator)
{
  MOZ_ASSERT(aAllocator);
  if (aAllocator && aAllocator->IPCOpen()) {
    SharedBufferManagerChild::GetSingleton()->DropGrallocBuffer(mGrallocHandle);
  }

  mGrallocHandle = null_t();
  mGraphicBuffer = nullptr;
}

void
GrallocTextureData::FillInfo(TextureData::Info& aInfo) const
{
  aInfo.size = mSize;
  aInfo.format = mFormat;
  aInfo.hasIntermediateBuffer = false;
  aInfo.hasSynchronization = true;
  aInfo.supportsMoz2D = true;
  aInfo.canExposeMappedData = true;
}

bool
GrallocTextureData::Serialize(SurfaceDescriptor& aOutDescriptor)
{
  aOutDescriptor = SurfaceDescriptorGralloc(mGrallocHandle, gfx::IsOpaque(mFormat));
  return true;
}

void
GrallocTextureData::WaitForFence(FenceHandle* aFence)
{
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION < 21 && ANDROID_VERSION >= 17
   if (aFence && aFence->IsValid()) {
     RefPtr<FenceHandle::FdObj> fdObj = aFence->GetAndResetFdObj();
     android::sp<Fence> fence = new Fence(fdObj->GetAndResetFd());
#if ANDROID_VERSION == 17
     fence->waitForever(1000, "GrallocTextureData::Lock");
     // 1000 is what Android uses. It is a warning timeout in ms.
     // This timeout was removed in ANDROID_VERSION 18.
#else
     fence->waitForever("GrallocTextureData::Lock");
#endif
   }
#endif
}

bool
GrallocTextureData::Lock(OpenMode aMode, FenceHandle* aReleaseFence)
{
  MOZ_ASSERT(!mMappedBuffer);

  uint32_t usage = 0;
  if (aMode & OpenMode::OPEN_READ) {
    usage |= GRALLOC_USAGE_SW_READ_OFTEN;
  }
  if (aMode & OpenMode::OPEN_WRITE) {
    usage |= GRALLOC_USAGE_SW_WRITE_OFTEN;
  }

  void** mappedBufferPtr = reinterpret_cast<void**>(&mMappedBuffer);

  int32_t rv = 0;
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 21
  if (aReleaseFence) {
    RefPtr<FenceHandle::FdObj> fdObj = aReleaseFence->GetAndResetFdObj();
    rv = mGraphicBuffer->lockAsync(usage, mappedBufferPtr,
                                   fdObj->GetAndResetFd());
  } else {
    rv = mGraphicBuffer->lock(usage, mappedBufferPtr);
  }
#else
  // older versions of android don't have lockAsync
  WaitForFence(aReleaseFence);
  rv = mGraphicBuffer->lock(usage, mappedBufferPtr);
#endif

  if (rv) {
    mMappedBuffer = nullptr;
    NS_WARNING("Couldn't lock graphic buffer");
    return false;
  }

  return true;
}

void
GrallocTextureData::Unlock()
{
  MOZ_ASSERT(mMappedBuffer);
  mMappedBuffer = nullptr;
  mGraphicBuffer->unlock();
}

already_AddRefed<gfx::DrawTarget>
GrallocTextureData::BorrowDrawTarget()
{
  MOZ_ASSERT(mMappedBuffer);
  if (!mMappedBuffer) {
    return nullptr;
  }
  long byteStride = mGraphicBuffer->getStride() * BytesPerPixel(mFormat);
  return gfxPlatform::CreateDrawTargetForData(mMappedBuffer, mSize,
                                              byteStride, mFormat);
}

bool
GrallocTextureData::BorrowMappedData(MappedTextureData& aMap)
{
  if (mFormat == gfx::SurfaceFormat::YUV || !mMappedBuffer) {
    return false;
  }

  aMap.data = mMappedBuffer;
  aMap.size = mSize;
  aMap.stride = mGraphicBuffer->getStride() * BytesPerPixel(mFormat);
  aMap.format = mFormat;

  return true;
}

bool
GrallocTextureData::UpdateFromSurface(gfx::SourceSurface* aSurface)
{
  MOZ_ASSERT(mMappedBuffer, "Calling TextureClient::BorrowDrawTarget without locking :(");

  if (!mMappedBuffer) {
    return false;
  }

  RefPtr<DataSourceSurface> srcSurf = aSurface->GetDataSurface();

  if (!srcSurf) {
    gfxCriticalError() << "Failed to GetDataSurface in UpdateFromSurface (GTC).";
    return false;
  }

  gfx::SurfaceFormat format = SurfaceFormatForPixelFormat(mGraphicBuffer->getPixelFormat());
  if (mSize != srcSurf->GetSize() || mFormat != srcSurf->GetFormat()) {
    gfxCriticalError() << "Attempt to update texture client from a surface with a different size or format! This: " << mSize << " " << format << " Other: " << srcSurf->GetSize() << " " << srcSurf->GetFormat();
    return false;
  }

  long pixelStride = mGraphicBuffer->getStride();
  long byteStride = pixelStride * BytesPerPixel(format);

  DataSourceSurface::MappedSurface sourceMap;

  if (!srcSurf->Map(DataSourceSurface::READ, &sourceMap)) {
    gfxCriticalError() << "Failed to map source surface for UpdateFromSurface (GTC).";
    return false;
  }

  for (int y = 0; y < srcSurf->GetSize().height; y++) {
    memcpy(mMappedBuffer + byteStride * y,
           sourceMap.mData + sourceMap.mStride * y,
           srcSurf->GetSize().width * BytesPerPixel(srcSurf->GetFormat()));
  }

  srcSurf->Unmap();

  return true;
}

// static
GrallocTextureData*
GrallocTextureData::Create(gfx::IntSize aSize, AndroidFormat aAndroidFormat,
                           gfx::BackendType aMoz2dBackend, uint32_t aUsage,
                           ClientIPCAllocator* aAllocator)
{
  if (!aAllocator || !aAllocator->IPCOpen()) {
    return nullptr;
  }
  int32_t maxSize = aAllocator->AsClientAllocator()->GetMaxTextureSize();
  if (aSize.width > maxSize || aSize.height > maxSize) {
    return nullptr;
  }
  gfx::SurfaceFormat format;
  switch (aAndroidFormat) {
  case android::PIXEL_FORMAT_RGBA_8888:
    format = gfx::SurfaceFormat::B8G8R8A8;
    break;
  case android::PIXEL_FORMAT_BGRA_8888:
    format = gfx::SurfaceFormat::B8G8R8A8;
    break;
  case android::PIXEL_FORMAT_RGBX_8888:
    format = gfx::SurfaceFormat::B8G8R8X8;
    break;
  case android::PIXEL_FORMAT_RGB_565:
    format = gfx::SurfaceFormat::R5G6B5_UINT16;
    break;
  case HAL_PIXEL_FORMAT_YV12:
    format = gfx::SurfaceFormat::YUV;
    break;
  default:
    format = gfx::SurfaceFormat::UNKNOWN;
  }

  if (DisableGralloc(format, aSize)) {
    return nullptr;
  }

  MaybeMagicGrallocBufferHandle handle;
  if (!SharedBufferManagerChild::GetSingleton()->AllocGrallocBuffer(aSize, aAndroidFormat, aUsage, &handle)) {
    return nullptr;
  }

  sp<GraphicBuffer> graphicBuffer = GetGraphicBufferFrom(handle);
  if (!graphicBuffer.get()) {
    return nullptr;
  }

  if (graphicBuffer->initCheck() != NO_ERROR) {
    return nullptr;
  }

  return new GrallocTextureData(handle, aSize, format, aMoz2dBackend);
}

// static
GrallocTextureData*
GrallocTextureData::CreateForDrawing(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                     gfx::BackendType aMoz2dBackend,
                                     ClientIPCAllocator* aAllocator)
{
  if (DisableGralloc(aFormat, aSize)) {
    return nullptr;
  }

#if ANDROID_VERSION <= 15
  // Adreno 200 has a problem of drawing gralloc buffer width less than 64 and
  // drawing gralloc buffer with a height 9px-16px.
  // See Bug 983971.
  // We only have this restriction in TextureClients that we'll use for drawing
  // (not with WebGL for instance). Not sure why that's OK, but we have tests that
  // rely on being able to create 32x32 webgl canvases with gralloc, so moving
  // this check in DisableGralloc will break them.
  if (aSize.width < 64 || aSize.height < 32) {
    return nullptr;
  }
#endif

  uint32_t usage = android::GraphicBuffer::USAGE_SW_READ_OFTEN |
                   android::GraphicBuffer::USAGE_SW_WRITE_OFTEN |
                   android::GraphicBuffer::USAGE_HW_TEXTURE;
  auto data =  GrallocTextureData::Create(aSize, GetAndroidFormat(aFormat),
                                          aMoz2dBackend, usage, aAllocator);

  if (!data) {
    return nullptr;
  }

  DebugOnly<gfx::SurfaceFormat> grallocFormat =
    SurfaceFormatForPixelFormat(data->mGraphicBuffer->getPixelFormat());
  // mFormat may be different from the format the graphic buffer reports if we
  // swap the R and B channels but we should always have at least the same bytes
  // per pixel!
  MOZ_ASSERT(BytesPerPixel(data->mFormat) == BytesPerPixel(grallocFormat));

  return data;
}

TextureFlags
GrallocTextureData::GetTextureFlags() const
{
  if (IsGrallocRBSwapped(mFormat)) {
    return TextureFlags::RB_SWAPPED;
  }
  return TextureFlags::NO_FLAGS;
}


// static
GrallocTextureData*
GrallocTextureData::CreateForYCbCr(gfx::IntSize aYSize, gfx::IntSize aCbCrSize,
                                   ClientIPCAllocator* aAllocator)
{
  MOZ_ASSERT(aYSize.width == aCbCrSize.width * 2);
  MOZ_ASSERT(aYSize.height == aCbCrSize.height * 2);
  return GrallocTextureData::Create(aYSize, HAL_PIXEL_FORMAT_YV12,
                                    gfx::BackendType::NONE,
                                    android::GraphicBuffer::USAGE_SW_READ_OFTEN,
                                    aAllocator);
}

// static
GrallocTextureData*
GrallocTextureData::CreateForGLRendering(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                         ClientIPCAllocator* aAllocator)
{
  if (aFormat == gfx::SurfaceFormat::YUV) {
    return nullptr;
  }
  uint32_t usage = android::GraphicBuffer::USAGE_HW_RENDER |
                   android::GraphicBuffer::USAGE_HW_TEXTURE;
  return GrallocTextureData::Create(aSize, GetAndroidFormat(aFormat),
                                    gfx::BackendType::NONE, usage, aAllocator);
}

// static
already_AddRefed<TextureClient>
GrallocTextureData::TextureClientFromSharedSurface(gl::SharedSurface* abstractSurf,
                                                   TextureFlags flags)
{
  auto surf = gl::SharedSurface_Gralloc::Cast(abstractSurf);

  RefPtr<TextureClient> ret = surf->GetTextureClient();

  TextureFlags mask = TextureFlags::ORIGIN_BOTTOM_LEFT |
                      TextureFlags::RB_SWAPPED |
                      TextureFlags::NON_PREMULTIPLIED;
  TextureFlags required = flags & mask;
  TextureFlags present = ret->GetFlags() & mask;

  if (present != required) {
    printf_stderr("Present flags: 0x%x. Required: 0x%x.\n",
                  (uint32_t)present,
                  (uint32_t)required);
    MOZ_CRASH("Flag requirement mismatch.");
  }
  return ret.forget();
}

TextureData*
GrallocTextureData::CreateSimilar(ClientIPCAllocator* aAllocator,
                                  TextureFlags aFlags,
                                  TextureAllocationFlags aAllocFlags) const
{
  if (mFormat == gfx::SurfaceFormat::YUV) {
    return GrallocTextureData::CreateForYCbCr(mSize, mSize*2, aAllocator);
  } else {
    return GrallocTextureData::CreateForDrawing(mSize, mFormat, mMoz2DBackend, aAllocator);
  }
}

} // namesapace layers
} // namesapace mozilla

#endif // MOZ_WIDGET_GONK
