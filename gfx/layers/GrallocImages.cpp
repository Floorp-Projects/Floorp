/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GrallocImages.h"
#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for int8_t, uint8_t, uint32_t, etc
#include "nsDebug.h"                    // for NS_WARNING, NS_PRECONDITION
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/GrallocTextureClient.h"
#include "gfx2DGlue.h"
#include "YCbCrUtils.h"                 // for YCbCr conversions

#include <ColorConverter.h>
#include <OMX_IVCommon.h>


using namespace mozilla::ipc;
using namespace android;

#define ALIGN(x, align) ((x + align - 1) & ~(align - 1))

namespace mozilla {
namespace layers {

int32_t GrallocImage::sColorIdMap[] = {
    HAL_PIXEL_FORMAT_YCbCr_420_P, OMX_COLOR_FormatYUV420Planar,
    HAL_PIXEL_FORMAT_YCbCr_422_P, OMX_COLOR_FormatYUV422Planar,
    HAL_PIXEL_FORMAT_YCbCr_420_SP, OMX_COLOR_FormatYUV420SemiPlanar,
    HAL_PIXEL_FORMAT_YCrCb_420_SP, -1,
    HAL_PIXEL_FORMAT_YCrCb_420_SP_ADRENO, -1,
    HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED, HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED,
    HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS, HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS,
    HAL_PIXEL_FORMAT_YV12, OMX_COLOR_FormatYUV420Planar,
    HAL_PIXEL_FORMAT_RGBA_8888, -1,
    0, 0
};

struct GraphicBufferAutoUnlock {
  android::sp<GraphicBuffer> mGraphicBuffer;

  GraphicBufferAutoUnlock(android::sp<GraphicBuffer>& aGraphicBuffer)
    : mGraphicBuffer(aGraphicBuffer) { }

  ~GraphicBufferAutoUnlock() { mGraphicBuffer->unlock(); }
};

GrallocImage::GrallocImage()
  : PlanarYCbCrImage(nullptr)
{
  mFormat = ImageFormat::GRALLOC_PLANAR_YCBCR;
}

GrallocImage::~GrallocImage()
{
}

bool
GrallocImage::SetData(const Data& aData)
{
  MOZ_ASSERT(!mTextureClient, "TextureClient is already set");
  NS_PRECONDITION(aData.mYSize.width % 2 == 0, "Image should have even width");
  NS_PRECONDITION(aData.mYSize.height % 2 == 0, "Image should have even height");
  NS_PRECONDITION(aData.mYStride % 16 == 0, "Image should have stride of multiple of 16 pixels");

  mData = aData;
  mSize = aData.mPicSize;

  if (gfxPlatform::GetPlatform()->IsInGonkEmulator()) {
    // Emulator does not support HAL_PIXEL_FORMAT_YV12.
    return false;
  }

  RefPtr<GrallocTextureClientOGL> textureClient =
       new GrallocTextureClientOGL(ImageBridgeChild::GetSingleton(),
                                   gfx::SurfaceFormat::UNKNOWN,
                                   gfx::BackendType::NONE);
  // GrallocImages are all YUV and don't support alpha.
  textureClient->SetIsOpaque(true);
  bool result =
    textureClient->AllocateGralloc(mData.mYSize,
                                   HAL_PIXEL_FORMAT_YV12,
                                   GraphicBuffer::USAGE_SW_READ_OFTEN |
                                   GraphicBuffer::USAGE_SW_WRITE_OFTEN |
                                   GraphicBuffer::USAGE_HW_TEXTURE);
  sp<GraphicBuffer> graphicBuffer = textureClient->GetGraphicBuffer();
  if (!result || !graphicBuffer.get()) {
    mTextureClient = nullptr;
    return false;
  }

  mTextureClient = textureClient;

  void* vaddr;
  if (graphicBuffer->lock(GraphicBuffer::USAGE_SW_WRITE_OFTEN,
                          &vaddr) != OK) {
    return false;
  }

  uint8_t* yChannel = static_cast<uint8_t*>(vaddr);
  gfx::IntSize ySize = aData.mYSize;
  int32_t yStride = graphicBuffer->getStride();

  uint8_t* vChannel = yChannel + (yStride * ySize.height);
  gfx::IntSize uvSize = gfx::IntSize(ySize.width / 2,
                                 ySize.height / 2);
  // Align to 16 bytes boundary
  int32_t uvStride = ((yStride / 2) + 15) & ~0x0F;
  uint8_t* uChannel = vChannel + (uvStride * uvSize.height);

  // Memory outside of the image width may not writable. If the stride
  // equals to the image width then we can use only one copy.
  if (yStride == mData.mYStride &&
      yStride == ySize.width) {
    memcpy(yChannel, mData.mYChannel, yStride * ySize.height);
  } else {
    for (int i = 0; i < ySize.height; i++) {
      memcpy(yChannel + i * yStride,
             mData.mYChannel + i * mData.mYStride,
             ySize.width);
    }
  }
  if (uvStride == mData.mCbCrStride &&
      uvStride == uvSize.width) {
    memcpy(uChannel, mData.mCbChannel, uvStride * uvSize.height);
    memcpy(vChannel, mData.mCrChannel, uvStride * uvSize.height);
  } else {
    for (int i = 0; i < uvSize.height; i++) {
      memcpy(uChannel + i * uvStride,
             mData.mCbChannel + i * mData.mCbCrStride,
             uvSize.width);
      memcpy(vChannel + i * uvStride,
             mData.mCrChannel + i * mData.mCbCrStride,
             uvSize.width);
    }
  }
  graphicBuffer->unlock();
  // Initialze the channels' addresses.
  // Do not cache the addresses when gralloc buffer is not locked.
  // gralloc hal could map gralloc buffer only when the buffer is locked,
  // though some gralloc hals implementation maps it when it is allocated.
  mData.mYChannel     = nullptr;
  mData.mCrChannel    = nullptr;
  mData.mCbChannel    = nullptr;
  return true;
}

bool GrallocImage::SetData(const GrallocData& aData)
{
  mTextureClient = static_cast<GrallocTextureClientOGL*>(aData.mGraphicBuffer.get());
  mSize = aData.mPicSize;
  return true;
}

/**
 * Converts YVU420 semi planar frames to RGB565, possibly taking different
 * stride values.
 * Needed because the Android ColorConverter class assumes that the Y and UV
 * channels have equal stride.
 */
static void
ConvertYVU420SPToRGB565(void *aYData, uint32_t aYStride,
                        void *aUData, void *aVData, uint32_t aUVStride,
                        void *aOut,
                        uint32_t aWidth, uint32_t aHeight)
{
  uint8_t *y = (uint8_t*)aYData;
  bool isCbCr;
  int8_t *uv;

  if (aUData < aVData) {
    // The color format is YCbCr
    isCbCr = true;
    uv = (int8_t*)aUData;
  } else {
    // The color format is YCrCb
    isCbCr = false;
    uv = (int8_t*)aVData;
  }

  uint16_t *rgb = (uint16_t*)aOut;

  for (size_t i = 0; i < aHeight; i++) {
    for (size_t j = 0; j < aWidth; j++) {
      int8_t d, e;

      if (isCbCr) {
        d = uv[j & ~1] - 128;
        e = uv[j | 1] - 128;
      } else {
        d = uv[j | 1] - 128;
        e = uv[j & ~1] - 128;
      }

      // Constants taken from https://en.wikipedia.org/wiki/YUV
      int32_t r = (298 * y[j] + 409 * e + 128) >> 11;
      int32_t g = (298 * y[j] - 100 * d - 208 * e + 128) >> 10;
      int32_t b = (298 * y[j] + 516 * d + 128) >> 11;

      r = r > 0x1f ? 0x1f : r < 0 ? 0 : r;
      g = g > 0x3f ? 0x3f : g < 0 ? 0 : g;
      b = b > 0x1f ? 0x1f : b < 0 ? 0 : b;

      *rgb++ = (uint16_t)(r << 11 | g << 5 | b);
    }

    y += aYStride;
    if (i % 2) {
      uv += aUVStride;
    }
  }
}

/**
 * Converts the format of vendor-specific YVU420(planar and semi-planar)
 * with the help of GraphicBuffer::lockYCbCr. In this way, we can convert
 * the YUV color format without awaring actual definition/enumeration
 * of vendor formats.
 */
static status_t
ConvertVendorYUVFormatToRGB565(android::sp<GraphicBuffer>& aBuffer,
                               gfx::DataSourceSurface *aSurface,
                               gfx::DataSourceSurface::MappedSurface *aMappedSurface)
{
  status_t rv = BAD_VALUE;

#if ANDROID_VERSION >= 18
  android_ycbcr ycbcr;

  // Check if the vendor provides explicit addresses of Y/Cb/Cr buffer from lockYCbCr
  rv = aBuffer->lockYCbCr(android::GraphicBuffer::USAGE_SW_READ_OFTEN, &ycbcr);

  if (rv != OK) {
    NS_WARNING("Couldn't lock graphic buffer using lockYCbCr()");
    return rv;
  }

  GraphicBufferAutoUnlock unlock(aBuffer);

  uint32_t width = aSurface->GetSize().width;
  uint32_t height = aSurface->GetSize().height;

  if (ycbcr.chroma_step == 2) {
    // From the system/core/include/system/graphics.h
    // @chroma_step is the distance in bytes from one chroma pixel value to
    // the next.  This is 2 bytes for semiplanar (because chroma values are
    // interleaved and each chroma value is one byte) and 1 for planar.
    ConvertYVU420SPToRGB565(ycbcr.y, ycbcr.ystride,
                            ycbcr.cb, ycbcr.cr, ycbcr.cstride,
                            aMappedSurface->mData,
                            width, height);
  } else {
    layers::PlanarYCbCrData ycbcrData;
    ycbcrData.mYChannel     = static_cast<uint8_t*>(ycbcr.y);
    ycbcrData.mYStride      = ycbcr.ystride;
    ycbcrData.mYSize        = aSurface->GetSize();
    ycbcrData.mCbChannel    = static_cast<uint8_t*>(ycbcr.cb);
    ycbcrData.mCrChannel    = static_cast<uint8_t*>(ycbcr.cr);
    ycbcrData.mCbCrStride   = ycbcr.cstride;
    ycbcrData.mCbCrSize     = aSurface->GetSize() / 2;
    ycbcrData.mPicSize      = aSurface->GetSize();

    gfx::ConvertYCbCrToRGB(ycbcrData,
                           aSurface->GetFormat(),
                           aSurface->GetSize(),
                           aMappedSurface->mData,
                           aMappedSurface->mStride);
  }
#endif

  return rv;
}

static status_t
ConvertOmxYUVFormatToRGB565(android::sp<GraphicBuffer>& aBuffer,
                            gfx::DataSourceSurface *aSurface,
                            gfx::DataSourceSurface::MappedSurface *aMappedSurface,
                            const layers::PlanarYCbCrData& aYcbcrData)
{
  uint32_t omxFormat =
    GrallocImage::GetOmxFormat(aBuffer->getPixelFormat());
  if (!omxFormat) {
    NS_WARNING("Unknown color format");
    return BAD_VALUE;
  }

  status_t rv;
  uint8_t *buffer;

  rv = aBuffer->lock(android::GraphicBuffer::USAGE_SW_READ_OFTEN,
                     reinterpret_cast<void **>(&buffer));
  if (rv != OK) {
    NS_WARNING("Couldn't lock graphic buffer");
    return BAD_VALUE;
  }

  GraphicBufferAutoUnlock unlock(aBuffer);

  uint32_t format = aBuffer->getPixelFormat();
  uint32_t width = aSurface->GetSize().width;
  uint32_t height = aSurface->GetSize().height;
  uint32_t stride = aBuffer->getStride();

  if (format == GrallocImage::HAL_PIXEL_FORMAT_YCrCb_420_SP_ADRENO) {
    // The Adreno hardware decoder aligns image dimensions to a multiple of 32,
    // so we have to account for that here
    uint32_t alignedWidth = ALIGN(width, 32);
    uint32_t alignedHeight = ALIGN(height, 32);
    uint32_t uvOffset = ALIGN(alignedHeight * alignedWidth, 4096);
    uint32_t uvStride = 2 * ALIGN(width / 2, 32);
    ConvertYVU420SPToRGB565(buffer, alignedWidth,
                            buffer + uvOffset + 1,
                            buffer + uvOffset,
                            uvStride,
                            aMappedSurface->mData,
                            width, height);
    return OK;
  }

  if (format == HAL_PIXEL_FORMAT_YCrCb_420_SP) {
    uint32_t uvOffset = height * stride;
    ConvertYVU420SPToRGB565(buffer, stride,
                            buffer + uvOffset + 1,
                            buffer + uvOffset,
                            stride,
                            aMappedSurface->mData,
                            width, height);
    return OK;
  }

  if (format == HAL_PIXEL_FORMAT_YV12) {
    // Depend on platforms, it is possible for HW decoder to output YV12 format.
    // It means the mData won't be configured during the SetData API because the
    // yuv data has already stored in GraphicBuffer. Here we try to confgiure the
    // mData if it doesn't contain valid configuration.
    layers::PlanarYCbCrData ycbcrData = aYcbcrData;
    if (!ycbcrData.mYChannel) {
      ycbcrData.mYChannel     = buffer;
      ycbcrData.mYSkip        = 0;
      ycbcrData.mYStride      = aBuffer->getStride();
      ycbcrData.mYSize        = aSurface->GetSize();
      ycbcrData.mCbSkip       = 0;
      ycbcrData.mCbCrSize     = aSurface->GetSize() / 2;
      ycbcrData.mPicSize      = aSurface->GetSize();
      ycbcrData.mCrChannel    = buffer + ycbcrData.mYStride * aBuffer->getHeight();
      ycbcrData.mCrSkip       = 0;
      // Align to 16 bytes boundary
      ycbcrData.mCbCrStride   = ALIGN(ycbcrData.mYStride / 2, 16);
      ycbcrData.mCbChannel    = ycbcrData.mCrChannel + (ycbcrData.mCbCrStride * aBuffer->getHeight() / 2);
    } else {
      // Update channels' address.
      // Gralloc buffer could map gralloc buffer only when the buffer is locked.
      ycbcrData.mYChannel     = buffer;
      ycbcrData.mCrChannel    = buffer + ycbcrData.mYStride * aBuffer->getHeight();
      ycbcrData.mCbChannel    = ycbcrData.mCrChannel + (ycbcrData.mCbCrStride * aBuffer->getHeight() / 2);
    }
    gfx::ConvertYCbCrToRGB(ycbcrData,
                           aSurface->GetFormat(),
                           aSurface->GetSize(),
                           aMappedSurface->mData,
                           aMappedSurface->mStride);
    return OK;
  }

  if (format == HAL_PIXEL_FORMAT_RGBA_8888) {
    uint32_t* src = (uint32_t*)(buffer);
    uint16_t* dest = (uint16_t*)(aMappedSurface->mData);

    // Convert RGBA8888 to RGB565
    for (size_t i = 0; i < width * height; i++) {
      uint32_t r = ((*src >> 0 ) & 0xFF);
      uint32_t g = ((*src >> 8 ) & 0xFF);
      uint32_t b = ((*src >> 16) & 0xFF);
      *dest++ = ((r >> 3) << 11) | ((g >> 2) << 5) | ((b >> 3) << 0);
      src++;
    }
    return OK;
  }

  android::ColorConverter colorConverter((OMX_COLOR_FORMATTYPE)omxFormat,
                                         OMX_COLOR_Format16bitRGB565);
  if (!colorConverter.isValid()) {
    NS_WARNING("Invalid color conversion");
    return BAD_VALUE;
  }

  uint32_t pixelStride = aMappedSurface->mStride/gfx::BytesPerPixel(gfx::SurfaceFormat::R5G6B5_UINT16);
  rv = colorConverter.convert(buffer, width, height,
                              0, 0, width - 1, height - 1 /* source crop */,
                              aMappedSurface->mData, pixelStride, height,
                              0, 0, width - 1, height - 1 /* dest crop */);
  if (rv) {
    NS_WARNING("OMX color conversion failed");
    return BAD_VALUE;
  }

  return OK;
}

already_AddRefed<gfx::DataSourceSurface>
GetDataSourceSurfaceFrom(android::sp<android::GraphicBuffer>& aGraphicBuffer,
                         gfx::IntSize aSize,
                         const layers::PlanarYCbCrData& aYcbcrData)
{
  MOZ_ASSERT(aGraphicBuffer.get());

  RefPtr<gfx::DataSourceSurface> surface =
    gfx::Factory::CreateDataSourceSurface(aSize, gfx::SurfaceFormat::R5G6B5_UINT16);
  if (NS_WARN_IF(!surface)) {
    return nullptr;
  }

  gfx::DataSourceSurface::MappedSurface mappedSurface;
  if (!surface->Map(gfx::DataSourceSurface::WRITE, &mappedSurface)) {
    NS_WARNING("Could not map DataSourceSurface");
    return nullptr;
  }

  int32_t rv;
  rv = ConvertOmxYUVFormatToRGB565(aGraphicBuffer, surface, &mappedSurface, aYcbcrData);
  if (rv == OK) {
    surface->Unmap();
    return surface.forget();
  }

  rv = ConvertVendorYUVFormatToRGB565(aGraphicBuffer, surface, &mappedSurface);
  surface->Unmap();
  if (rv != OK) {
    NS_WARNING("Unknown color format");
    return nullptr;
  }

  return surface.forget();
}

already_AddRefed<gfx::SourceSurface>
GrallocImage::GetAsSourceSurface()
{
  if (!mTextureClient) {
    return nullptr;
  }

  android::sp<GraphicBuffer> graphicBuffer =
    mTextureClient->GetGraphicBuffer();

  RefPtr<gfx::DataSourceSurface> surface =
    GetDataSourceSurfaceFrom(graphicBuffer, mSize, mData);

  return surface.forget();
}

android::sp<android::GraphicBuffer>
GrallocImage::GetGraphicBuffer() const
{
  if (!mTextureClient) {
    return nullptr;
  }
  return mTextureClient->GetGraphicBuffer();
}

void*
GrallocImage::GetNativeBuffer()
{
  if (!mTextureClient) {
    return nullptr;
  }
  android::sp<android::GraphicBuffer> graphicBuffer =
    mTextureClient->GetGraphicBuffer();
  if (!graphicBuffer.get()) {
    return nullptr;
  }
  return graphicBuffer->getNativeBuffer();
}

TextureClient*
GrallocImage::GetTextureClient(CompositableClient* aClient)
{
  return mTextureClient;
}

} // namespace layers
} // namespace mozilla
