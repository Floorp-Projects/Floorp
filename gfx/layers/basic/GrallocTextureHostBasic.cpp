/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GrallocTextureHostBasic.h"
#include "GrallocImages.h"  // for GetDataSourceSurfaceFrom()
#include "mozilla/layers/SharedBufferManagerParent.h"

#if ANDROID_VERSION >= 17
#include <ui/Fence.h>
#endif

namespace mozilla {
namespace layers {

static gfx::SurfaceFormat
HalFormatToSurfaceFormat(int aHalFormat, TextureFlags aFlags)
{
  bool swapRB = bool(aFlags & TextureFlags::RB_SWAPPED);
  switch (aHalFormat) {
  case android::PIXEL_FORMAT_BGRA_8888:
    return swapRB ? gfx::SurfaceFormat::R8G8B8A8 : gfx::SurfaceFormat::B8G8R8A8;
  case android::PIXEL_FORMAT_RGBA_8888:
    return swapRB ? gfx::SurfaceFormat::B8G8R8A8 : gfx::SurfaceFormat::R8G8B8A8;
  case android::PIXEL_FORMAT_RGBX_8888:
    return swapRB ? gfx::SurfaceFormat::B8G8R8X8 : gfx::SurfaceFormat::R8G8B8X8;
  case android::PIXEL_FORMAT_RGB_565:
    return gfx::SurfaceFormat::R5G6B5_UINT16;
  case HAL_PIXEL_FORMAT_YCbCr_422_SP:
  case HAL_PIXEL_FORMAT_YCrCb_420_SP:
  case HAL_PIXEL_FORMAT_YCbCr_422_I:
  case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
  case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS:
  case HAL_PIXEL_FORMAT_YV12:
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
  case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
#endif
      // Needs convert to RGB565
      return gfx::SurfaceFormat::R5G6B5_UINT16;
  default:
    if (aHalFormat >= 0x100 && aHalFormat <= 0x1FF) {
      // Reserved range for HAL specific formats.
      // Needs convert to RGB565
      return gfx::SurfaceFormat::R5G6B5_UINT16;
    } else {
      MOZ_CRASH("GFX: Unhandled HAL pixel format");
      return gfx::SurfaceFormat::UNKNOWN; // not reached
    }
  }
}

static bool
NeedsConvertFromYUVtoRGB565(int aHalFormat)
{
  switch (aHalFormat) {
  case android::PIXEL_FORMAT_BGRA_8888:
  case android::PIXEL_FORMAT_RGBA_8888:
  case android::PIXEL_FORMAT_RGBX_8888:
  case android::PIXEL_FORMAT_RGB_565:
    return false;
  case HAL_PIXEL_FORMAT_YCbCr_422_SP:
  case HAL_PIXEL_FORMAT_YCrCb_420_SP:
  case HAL_PIXEL_FORMAT_YCbCr_422_I:
  case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
  case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS:
  case HAL_PIXEL_FORMAT_YV12:
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
  case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
#endif
      return true;
  default:
    if (aHalFormat >= 0x100 && aHalFormat <= 0x1FF) {
      // Reserved range for HAL specific formats.
      return true;
    } else {
      MOZ_CRASH("GFX: Unhandled HAL pixel format YUV");
      return false; // not reached
    }
  }
}

GrallocTextureHostBasic::GrallocTextureHostBasic(
  TextureFlags aFlags,
  const SurfaceDescriptorGralloc& aDescriptor)
  : TextureHost(aFlags)
  , mGrallocHandle(aDescriptor)
  , mSize(0, 0)
  , mCropSize(0, 0)
  , mFormat(gfx::SurfaceFormat::UNKNOWN)
  , mIsOpaque(aDescriptor.isOpaque())
{
  android::GraphicBuffer* grallocBuffer = GetGraphicBufferFromDesc(mGrallocHandle).get();
  MOZ_ASSERT(grallocBuffer);

  if (grallocBuffer) {
    mFormat =
      HalFormatToSurfaceFormat(grallocBuffer->getPixelFormat(),
                               aFlags & TextureFlags::RB_SWAPPED);
    mSize = gfx::IntSize(grallocBuffer->getWidth(), grallocBuffer->getHeight());
    mCropSize = mSize;
  } else {
    printf_stderr("gralloc buffer is nullptr\n");
  }
}

bool
GrallocTextureHostBasic::Lock()
{
  if (!mCompositor || !IsValid()) {
    return false;
  }

  if (mTextureSource) {
    return true;
  }

  android::sp<android::GraphicBuffer> graphicBuffer =
    GetGraphicBufferFromDesc(mGrallocHandle);
  MOZ_ASSERT(graphicBuffer.get());

  RefPtr<gfx::DataSourceSurface> surf;
  if (NeedsConvertFromYUVtoRGB565(graphicBuffer->getPixelFormat())) {
    PlanarYCbCrData ycbcrData;
    surf = GetDataSourceSurfaceFrom(graphicBuffer,
                                    mCropSize,
                                    ycbcrData);
  } else {
    uint32_t usage = GRALLOC_USAGE_SW_READ_OFTEN;
    int32_t rv = graphicBuffer->lock(usage,
                                     reinterpret_cast<void**>(&mMappedBuffer));
    if (rv) {
      mMappedBuffer = nullptr;
      NS_WARNING("Couldn't lock graphic buffer");
      return false;
    }
    surf = gfx::Factory::CreateWrappingDataSourceSurface(
             mMappedBuffer,
             graphicBuffer->getStride() * gfx::BytesPerPixel(mFormat),
             mCropSize,
             mFormat);
  }
  if (surf) {
    mTextureSource = mCompositor->CreateDataTextureSource(mFlags);
    mTextureSource->Update(surf, nullptr);
    return true;
  }
  mMappedBuffer = nullptr;
  return false;
}

bool
GrallocTextureHostBasic::IsValid() const
{
  android::GraphicBuffer* graphicBuffer = GetGraphicBufferFromDesc(mGrallocHandle).get();
  return graphicBuffer != nullptr;
}

bool
GrallocTextureHostBasic::BindTextureSource(CompositableTextureSourceRef& aTexture)
{
  aTexture = mTextureSource;
  return !!aTexture;
}

void
GrallocTextureHostBasic::UnbindTextureSource()
{
  TextureHost::UnbindTextureSource();
  ClearTextureSource();
}

void
GrallocTextureHostBasic::ClearTextureSource()
{
  mTextureSource = nullptr;
  if (mMappedBuffer) {
    android::GraphicBuffer* graphicBuffer = GetGraphicBufferFromDesc(mGrallocHandle).get();
    MOZ_ASSERT(graphicBuffer);
    mMappedBuffer = nullptr;
    graphicBuffer->unlock();
  }
}

void
GrallocTextureHostBasic::SetCompositor(Compositor* aCompositor)
{
  BasicCompositor* compositor = AssertBasicCompositor(aCompositor);
  if (!compositor) {
    return;
  }

  mCompositor = compositor;
  if (mTextureSource) {
    mTextureSource->SetCompositor(compositor);
  }
}

Compositor*
GrallocTextureHostBasic::GetCompositor()
{
  return mCompositor;
}

gfx::SurfaceFormat
GrallocTextureHostBasic::GetFormat() const {
  return mFormat;
}

void
GrallocTextureHostBasic::WaitAcquireFenceHandleSyncComplete()
{
  if (!mAcquireFenceHandle.IsValid()) {
    return;
  }

#if ANDROID_VERSION >= 17
  RefPtr<FenceHandle::FdObj> fdObj = mAcquireFenceHandle.GetAndResetFdObj();
  android::sp<android::Fence> fence(
    new android::Fence(fdObj->GetAndResetFd()));

  // Wait fece complete with timeout.
  // If a source of the fence becomes invalid because of error,
  // fene complete is not signaled. See Bug 1061435.
  int rv = fence->wait(400 /*400 msec*/);
  if (rv != android::OK) {
    NS_ERROR("failed to wait fence complete");
  }
#endif
}

void
GrallocTextureHostBasic::SetCropRect(nsIntRect aCropRect)
{
  MOZ_ASSERT(aCropRect.TopLeft() == gfx::IntPoint(0, 0));
  MOZ_ASSERT(!aCropRect.IsEmpty());
  MOZ_ASSERT(aCropRect.width <= mSize.width);
  MOZ_ASSERT(aCropRect.height <= mSize.height);

  gfx::IntSize cropSize(aCropRect.width, aCropRect.height);
  if (mCropSize == cropSize) {
    return;
  }

  mCropSize = cropSize;
  ClearTextureSource();
}

void
GrallocTextureHostBasic::DeallocateSharedData()
{
  ClearTextureSource();

  if (mGrallocHandle.buffer().type() != MaybeMagicGrallocBufferHandle::Tnull_t) {
    MaybeMagicGrallocBufferHandle handle = mGrallocHandle.buffer();
    base::ProcessId owner;
    if (handle.type() == MaybeMagicGrallocBufferHandle::TGrallocBufferRef) {
      owner = handle.get_GrallocBufferRef().mOwner;
    }
    else {
      owner = handle.get_MagicGrallocBufferHandle().mRef.mOwner;
    }

    SharedBufferManagerParent::DropGrallocBuffer(owner, mGrallocHandle);
  }
}

void
GrallocTextureHostBasic::ForgetSharedData()
{
  ClearTextureSource();
}

void
GrallocTextureHostBasic::DeallocateDeviceData()
{
  ClearTextureSource();
}

LayerRenderState
GrallocTextureHostBasic::GetRenderState()
{
  android::GraphicBuffer* graphicBuffer = GetGraphicBufferFromDesc(mGrallocHandle).get();

  if (graphicBuffer) {
    LayerRenderStateFlags flags = LayerRenderStateFlags::LAYER_RENDER_STATE_DEFAULT;
    if (mIsOpaque) {
      flags |= LayerRenderStateFlags::OPAQUE;
    }
    if (mFlags & TextureFlags::ORIGIN_BOTTOM_LEFT) {
      flags |= LayerRenderStateFlags::ORIGIN_BOTTOM_LEFT;
    }
    if (mFlags & TextureFlags::RB_SWAPPED) {
      flags |= LayerRenderStateFlags::FORMAT_RB_SWAP;
    }
    return LayerRenderState(graphicBuffer,
                            mCropSize,
                            flags,
                            this);
  }

  return LayerRenderState();
}

} // namespace layers
} // namespace mozilla
