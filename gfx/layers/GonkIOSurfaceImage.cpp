/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkIOSurfaceImage.h"

#include <OMX_IVCommon.h>
#include <ColorConverter.h>
#include <utils/RefBase.h>
#include <utils/Errors.h>

#define ALIGN(x, align) ((x + align - 1) & ~(align - 1))

typedef android::GraphicBuffer GraphicBuffer;

namespace mozilla {
namespace layers {

uint32_t GonkIOSurfaceImage::sColorIdMap[] = {
    HAL_PIXEL_FORMAT_YCbCr_420_P, OMX_COLOR_FormatYUV420Planar,
    HAL_PIXEL_FORMAT_YCbCr_422_P, OMX_COLOR_FormatYUV422Planar,
    HAL_PIXEL_FORMAT_YCbCr_420_SP, OMX_COLOR_FormatYUV420SemiPlanar,
    HAL_PIXEL_FORMAT_YCrCb_420_SP, -1,
    HAL_PIXEL_FORMAT_YCrCb_420_SP_ADRENO, -1,
    HAL_PIXEL_FORMAT_YV12, OMX_COLOR_FormatYUV420Planar,
    0, 0
};

struct GraphicBufferAutoUnlock {
  android::sp<GraphicBuffer> mGraphicBuffer;

  GraphicBufferAutoUnlock(android::sp<GraphicBuffer>& aGraphicBuffer)
    : mGraphicBuffer(aGraphicBuffer) { }

  ~GraphicBufferAutoUnlock() { mGraphicBuffer->unlock(); }
};

/**
 * Converts YVU420 semi planar frames to RGB565, possibly taking different
 * stride values.
 * Needed because the Android ColorConverter class assumes that the Y and UV
 * channels have equal stride.
 */
static void
ConvertYVU420SPToRGB565(void *aYData, uint32_t aYStride,
                        void *aUVData, uint32_t aUVStride,
                        void *aOut,
                        uint32_t aWidth, uint32_t aHeight)
{
  uint8_t *y = (uint8_t*)aYData;
  int8_t *uv = (int8_t*)aUVData;

  uint16_t *rgb = (uint16_t*)aOut;

  for (size_t i = 0; i < aHeight; i++) {
    for (size_t j = 0; j < aWidth; j++) {
      int8_t d = uv[j | 1] - 128;
      int8_t e = uv[j & ~1] - 128;

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

already_AddRefed<gfxASurface>
GonkIOSurfaceImage::GetAsSurface()
{
  android::sp<GraphicBuffer> graphicBuffer =
    GrallocBufferActor::GetFrom(GetSurfaceDescriptor());

  void *buffer;
  int32_t rv =
    graphicBuffer->lock(android::GraphicBuffer::USAGE_SW_READ_OFTEN, &buffer);

  if (rv) {
    NS_WARNING("Couldn't lock graphic buffer");
    return nullptr;
  }

  GraphicBufferAutoUnlock unlock(graphicBuffer);

  uint32_t format = graphicBuffer->getPixelFormat();
  uint32_t omxFormat = 0;

  for (int i = 0; sColorIdMap[i]; i += 2) {
    if (sColorIdMap[i] == format) {
      omxFormat = sColorIdMap[i + 1];
      break;
    }
  }

  if (!omxFormat) {
    NS_WARNING("Unknown color format");
    return nullptr;
  }

  nsRefPtr<gfxImageSurface> imageSurface =
    new gfxImageSurface(GetSize(), gfxASurface::ImageFormatRGB16_565);

  uint32_t width = GetSize().width;
  uint32_t height = GetSize().height;

  if (format == HAL_PIXEL_FORMAT_YCrCb_420_SP_ADRENO) {
    // The Adreno hardware decoder aligns image dimensions to a multiple of 32,
    // so we have to account for that here
    uint32_t alignedWidth = ALIGN(width, 32);
    uint32_t alignedHeight = ALIGN(height, 32);
    uint32_t uvOffset = ALIGN(alignedHeight * alignedWidth, 4096);
    uint32_t uvStride = 2 * ALIGN(width / 2, 32);
    uint8_t* buffer_as_bytes = static_cast<uint8_t*>(buffer);
    ConvertYVU420SPToRGB565(buffer, alignedWidth,
                            buffer_as_bytes + uvOffset, uvStride,
                            imageSurface->Data(),
                            width, height);

    return imageSurface.forget();
  }
  else if (format == HAL_PIXEL_FORMAT_YCrCb_420_SP) {
    uint32_t uvOffset = height * width;
    ConvertYVU420SPToRGB565(buffer, width,
                            buffer + uvOffset, width,
                            imageSurface->Data(),
                            width, height);

    return imageSurface.forget();
  }

  android::ColorConverter colorConverter((OMX_COLOR_FORMATTYPE)omxFormat,
                                         OMX_COLOR_Format16bitRGB565);

  if (!colorConverter.isValid()) {
    NS_WARNING("Invalid color conversion");
    return nullptr;
  }

  rv = colorConverter.convert(buffer, width, height,
                              0, 0, width - 1, height - 1 /* source crop */,
                              imageSurface->Data(), width, height,
                              0, 0, width - 1, height - 1 /* dest crop */);

  if (rv) {
    NS_WARNING("OMX color conversion failed");
    return nullptr;
  }

  return imageSurface.forget();
}

} // namespace layers
} // namespace mozilla
