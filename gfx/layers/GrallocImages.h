/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GRALLOCIMAGES_H
#define GRALLOCIMAGES_H

#ifdef MOZ_WIDGET_GONK

#include "ImageLayers.h"
#include "ImageContainer.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/layers/AtomicRefCountedWithFinalize.h"
#include "mozilla/layers/FenceUtils.h"
#include "mozilla/layers/LayersSurfaces.h"

#include <ui/GraphicBuffer.h>

namespace mozilla {
namespace layers {

class GrallocTextureClientOGL;

already_AddRefed<gfx::DataSourceSurface>
GetDataSourceSurfaceFrom(android::sp<android::GraphicBuffer>& aGraphicBuffer,
                         gfx::IntSize aSize,
                         const layers::PlanarYCbCrData& aYcbcrData);

/**
 * The YUV format supported by Android HAL
 *
 * 4:2:0 - CbCr width and height is half that of Y.
 *
 * This format assumes
 * - an even width
 * - an even height
 * - a horizontal stride multiple of 16 pixels
 * - a vertical stride equal to the height
 *
 * y_size = stride * height
 * c_size = ALIGN(stride/2, 16) * height/2
 * size = y_size + c_size * 2
 * cr_offset = y_size
 * cb_offset = y_size + c_size
 *
 * The Image that is rendered is the picture region defined by
 * mPicX, mPicY and mPicSize. The size of the rendered image is
 * mPicSize, not mYSize or mCbCrSize.
 */
class GrallocImage : public PlanarYCbCrImage
{
  typedef PlanarYCbCrData Data;
  static int32_t sColorIdMap[];
public:
  struct GrallocData {
    RefPtr<TextureClient> mGraphicBuffer;
    gfx::IntSize mPicSize;
  };

  GrallocImage();

  virtual ~GrallocImage();

  /**
   * This makes a copy of the data buffers, in order to support functioning
   * in all different layer managers.
   */
  virtual bool SetData(const Data& aData);

  /**
   *  Share the SurfaceDescriptor without making the copy, in order
   *  to support functioning in all different layer managers.
   */
  virtual bool SetData(const GrallocData& aData);

  // From [android 4.0.4]/hardware/msm7k/libgralloc-qsd8k/gralloc_priv.h
  enum {
    /* OEM specific HAL formats */
    HAL_PIXEL_FORMAT_YCbCr_422_P            = 0x102,
    HAL_PIXEL_FORMAT_YCbCr_420_P            = 0x103,
    HAL_PIXEL_FORMAT_YCbCr_420_SP           = 0x109,
    HAL_PIXEL_FORMAT_YCrCb_420_SP_ADRENO    = 0x10A,
    HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED     = 0x7FA30C03,
    HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS     = 0x7FA30C04,
  };

  enum {
    GRALLOC_SW_UAGE = android::GraphicBuffer::USAGE_SOFTWARE_MASK,
  };

  virtual already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;

  android::sp<android::GraphicBuffer> GetGraphicBuffer() const;

  void* GetNativeBuffer();

  virtual bool IsValid() { return !!mTextureClient; }

  virtual TextureClient* GetTextureClient(CompositableClient* aClient) override;

  virtual GrallocImage* AsGrallocImage() override
  {
    return this;
  }

  virtual uint8_t* GetBuffer()
  {
    return static_cast<uint8_t*>(GetNativeBuffer());
  }

  int GetUsage()
  {
    return (static_cast<ANativeWindowBuffer*>(GetNativeBuffer()))->usage;
  }

  static int GetOmxFormat(int aFormat)
  {
    uint32_t omxFormat = 0;

    for (int i = 0; sColorIdMap[i]; i += 2) {
      if (sColorIdMap[i] == aFormat) {
        omxFormat = sColorIdMap[i + 1];
        break;
      }
    }

    return omxFormat;
  }

private:
  RefPtr<GrallocTextureClientOGL> mTextureClient;
};

} // namespace layers
} // namespace mozilla

#endif

#endif /* GRALLOCIMAGES_H */
