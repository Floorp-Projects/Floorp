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
    HAL_PIXEL_FORMAT_YCrCb_420_SP_ADRENO, OMX_QCOM_COLOR_FormatYVU420SemiPlanar,
    0, 0
};

struct GraphicBufferAutoUnlock {
  android::sp<GraphicBuffer> mGraphicBuffer;

  GraphicBufferAutoUnlock(android::sp<GraphicBuffer>& aGraphicBuffer)
    : mGraphicBuffer(aGraphicBuffer) { }

  ~GraphicBufferAutoUnlock() { mGraphicBuffer->unlock(); }
};

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

  android::ColorConverter *colorConverter =
    new android::ColorConverter((OMX_COLOR_FORMATTYPE)omxFormat,
                                OMX_COLOR_Format16bitRGB565);

  if (!colorConverter->isValid()) {
    NS_WARNING("Invalid color conversion");
    return nullptr;
  }

  uint32_t width = GetSize().width;
  uint32_t height = GetSize().height;

  if (omxFormat == OMX_QCOM_COLOR_FormatYVU420SemiPlanar) {
    // The Adreno hardware decoder aligns image dimensions to a multiple of 32,
    //  so we have to account for that here
    width = ALIGN(width, 32);
    height = ALIGN(height, 32);
  }

  rv = colorConverter->convert(buffer, width, height,
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
