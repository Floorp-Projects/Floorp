/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_GONK

#include "mozilla/gfx/2D.h"
#include "mozilla/layers/GrallocTextureClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/ShadowLayerUtilsGralloc.h"
#include "gfx2DGlue.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;
using namespace android;

GrallocTextureClientOGL::GrallocTextureClientOGL(MaybeMagicGrallocBufferHandle buffer,
                                                 gfx::IntSize aSize,
                                                 gfx::BackendType aMoz2dBackend,
                                                 TextureFlags aFlags)
: BufferTextureClient(nullptr, gfx::SurfaceFormat::UNKNOWN, aMoz2dBackend, aFlags)
, mGrallocHandle(buffer)
, mMappedBuffer(nullptr)
, mMediaBuffer(nullptr)
{
  InitWith(buffer, aSize);
  MOZ_COUNT_CTOR(GrallocTextureClientOGL);
}

GrallocTextureClientOGL::GrallocTextureClientOGL(ISurfaceAllocator* aAllocator,
                                                 gfx::SurfaceFormat aFormat,
                                                 gfx::BackendType aMoz2dBackend,
                                                 TextureFlags aFlags)
: BufferTextureClient(aAllocator, aFormat, aMoz2dBackend, aFlags)
, mGrallocHandle(null_t())
, mMappedBuffer(nullptr)
, mMediaBuffer(nullptr)
{
  MOZ_COUNT_CTOR(GrallocTextureClientOGL);
}

GrallocTextureClientOGL::~GrallocTextureClientOGL()
{
  MOZ_COUNT_DTOR(GrallocTextureClientOGL);
    if (ShouldDeallocateInDestructor()) {
    ISurfaceAllocator* allocator = GetAllocator();
    allocator->DeallocGrallocBuffer(&mGrallocHandle);
  }
}

void
GrallocTextureClientOGL::InitWith(MaybeMagicGrallocBufferHandle aHandle, gfx::IntSize aSize)
{
  MOZ_ASSERT(!IsAllocated());
  MOZ_ASSERT(IsValid());
  mGrallocHandle = aHandle;
  mGraphicBuffer = GetGraphicBufferFrom(aHandle);
  mSize = aSize;
}

bool
GrallocTextureClientOGL::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(IsValid());
  if (!IsAllocated()) {
    return false;
  }

  aOutDescriptor = NewSurfaceDescriptorGralloc(mGrallocHandle, mSize);
  return true;
}

void
GrallocTextureClientOGL::SetReleaseFenceHandle(FenceHandle aReleaseFenceHandle)
{
  mReleaseFenceHandle = aReleaseFenceHandle;
}

void
GrallocTextureClientOGL::WaitReleaseFence()
{
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
   if (mReleaseFenceHandle.IsValid()) {
     android::sp<Fence> fence = mReleaseFenceHandle.mFence;
#if ANDROID_VERSION == 17
     fence->waitForever(1000, "GrallocTextureClientOGL::Lock");
     // 1000 is what Android uses. It is warning timeout ms.
     // This timeous is removed since ANDROID_VERSION 18. 
#else
     fence->waitForever("GrallocTextureClientOGL::Lock");
#endif
     mReleaseFenceHandle = FenceHandle();
   }
#endif
}

bool
GrallocTextureClientOGL::Lock(OpenMode aMode)
{
  MOZ_ASSERT(IsValid());
  if (!IsValid() || !IsAllocated()) {
    return false;
  }

  if (mMappedBuffer) {
    return true;
  }

  WaitReleaseFence();

  uint32_t usage = 0;
  if (aMode & OpenMode::OPEN_READ) {
    usage |= GRALLOC_USAGE_SW_READ_OFTEN;
  }
  if (aMode & OpenMode::OPEN_WRITE) {
    usage |= GRALLOC_USAGE_SW_WRITE_OFTEN;
  }
  int32_t rv = mGraphicBuffer->lock(usage, reinterpret_cast<void**>(&mMappedBuffer));
  if (rv) {
    mMappedBuffer = nullptr;
    NS_WARNING("Couldn't lock graphic buffer");
    return false;
  }
  return BufferTextureClient::Lock(aMode);
}

void
GrallocTextureClientOGL::Unlock()
{
  BufferTextureClient::Unlock();
  mDrawTarget = nullptr;
  if (mMappedBuffer) {
    mMappedBuffer = nullptr;
    mGraphicBuffer->unlock();
  }
}

uint8_t*
GrallocTextureClientOGL::GetBuffer() const
{
  MOZ_ASSERT(IsValid());
  NS_WARN_IF_FALSE(mMappedBuffer, "Trying to get a gralloc buffer without getting the lock?");
  return mMappedBuffer;
}

static gfx::SurfaceFormat
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
    return gfx::SurfaceFormat::R5G6B5;
  default:
    MOZ_CRASH("Unknown gralloc pixel format");
  }
  return gfx::SurfaceFormat::R8G8B8A8;
}

TemporaryRef<gfx::DrawTarget>
GrallocTextureClientOGL::GetAsDrawTarget()
{
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(mMappedBuffer, "Calling TextureClient::GetAsDrawTarget without locking :(");

  if (mDrawTarget) {
    return mDrawTarget;
  }

  gfx::SurfaceFormat format = SurfaceFormatForPixelFormat(mGraphicBuffer->getPixelFormat());
  long pixelStride = mGraphicBuffer->getStride();
  long byteStride = pixelStride * BytesPerPixel(format);
  mDrawTarget = gfxPlatform::GetPlatform()->CreateDrawTargetForData(GetBuffer(),
                                                                    mSize,
                                                                    byteStride,
                                                                    mFormat);
  return mDrawTarget;
}

bool
GrallocTextureClientOGL::AllocateForSurface(gfx::IntSize aSize,
                                            TextureAllocationFlags)
{
  MOZ_ASSERT(IsValid());

  uint32_t format;
  uint32_t usage = android::GraphicBuffer::USAGE_SW_READ_OFTEN |
                   android::GraphicBuffer::USAGE_SW_WRITE_OFTEN |
                   android::GraphicBuffer::USAGE_HW_TEXTURE;

  switch (mFormat) {
  case gfx::SurfaceFormat::R8G8B8A8:
    format = android::PIXEL_FORMAT_RGBA_8888;
    break;
  case gfx::SurfaceFormat::B8G8R8A8:
     format = android::PIXEL_FORMAT_RGBA_8888;
     mFlags |= TextureFlags::RB_SWAPPED;
    break;
  case gfx::SurfaceFormat::R8G8B8X8:
    format = android::PIXEL_FORMAT_RGBX_8888;
    break;
  case gfx::SurfaceFormat::B8G8R8X8:
     format = android::PIXEL_FORMAT_RGBX_8888;
     mFlags |= TextureFlags::RB_SWAPPED;
    break;
  case gfx::SurfaceFormat::R5G6B5:
    format = android::PIXEL_FORMAT_RGB_565;
    break;
  case gfx::SurfaceFormat::A8:
    NS_WARNING("gralloc does not support gfx::SurfaceFormat::A8");
    return false;
  default:
    NS_WARNING("Unsupported surface format");
    return false;
  }

  return AllocateGralloc(aSize, format, usage);
}

bool
GrallocTextureClientOGL::AllocateForYCbCr(gfx::IntSize aYSize, gfx::IntSize aCbCrSize, StereoMode aStereoMode)
{
  MOZ_ASSERT(IsValid());
  return AllocateGralloc(aYSize,
                         HAL_PIXEL_FORMAT_YV12,
                         android::GraphicBuffer::USAGE_SW_READ_OFTEN);
}

bool
GrallocTextureClientOGL::AllocateForGLRendering(gfx::IntSize aSize)
{
  MOZ_ASSERT(IsValid());

  uint32_t format;
  uint32_t usage = android::GraphicBuffer::USAGE_HW_RENDER |
                   android::GraphicBuffer::USAGE_HW_TEXTURE;

  switch (mFormat) {
  case gfx::SurfaceFormat::R8G8B8A8:
  case gfx::SurfaceFormat::B8G8R8A8:
    format = android::PIXEL_FORMAT_RGBA_8888;
    break;
  case gfx::SurfaceFormat::R8G8B8X8:
  case gfx::SurfaceFormat::B8G8R8X8:
    // there is no android BGRX format?
    format = android::PIXEL_FORMAT_RGBX_8888;
    break;
  case gfx::SurfaceFormat::R5G6B5:
    format = android::PIXEL_FORMAT_RGB_565;
    break;
  default:
    NS_WARNING("Unsupported surface format");
    return false;
  }

  return AllocateGralloc(aSize, format, usage);
}

bool
GrallocTextureClientOGL::AllocateGralloc(gfx::IntSize aSize,
                                         uint32_t aAndroidFormat,
                                         uint32_t aUsage)
{
  MOZ_ASSERT(IsValid());
  ISurfaceAllocator* allocator = GetAllocator();

  MaybeMagicGrallocBufferHandle handle;
  bool allocateResult =
    allocator->AllocGrallocBuffer(aSize,
                                  aAndroidFormat,
                                  aUsage,
                                  &handle);
  if (!allocateResult) {
    return false;
  }

  sp<GraphicBuffer> graphicBuffer = GetGraphicBufferFrom(handle);
  if (!graphicBuffer.get()) {
    return false;
  }

  if (graphicBuffer->initCheck() != NO_ERROR) {
    return false;
  }

  mGrallocHandle = handle;
  mGraphicBuffer = graphicBuffer;
  mSize = aSize;
  return true;
}

bool
GrallocTextureClientOGL::IsAllocated() const
{
  return !!mGraphicBuffer.get();
}

bool
GrallocTextureClientOGL::Allocate(uint32_t aSize)
{
  // see Bug 908196
  MOZ_CRASH("This method should never be called.");
  return false;
}

size_t
GrallocTextureClientOGL::GetBufferSize() const
{
  // see Bug 908196
  MOZ_CRASH("This method should never be called.");
  return 0;
}

} // namesapace layers
} // namesapace mozilla

#endif // MOZ_WIDGET_GONK
