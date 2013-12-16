/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfx2DGlue.h"

#include "YCbCrUtils.h"
#include "yuv_convert.h"
#include "ycbcr_to_rgb565.h"

namespace mozilla {
namespace gfx {

void
GetYCbCrToRGBDestFormatAndSize(const layers::PlanarYCbCrData& aData,
                               SurfaceFormat& aSuggestedFormat,
                               IntSize& aSuggestedSize)
{
  YUVType yuvtype =
    TypeFromSize(aData.mYSize.width,
                 aData.mYSize.height,
                 aData.mCbCrSize.width,
                 aData.mCbCrSize.height);

  // 'prescale' is true if the scaling is to be done as part of the
  // YCbCr to RGB conversion rather than on the RGB data when rendered.
  bool prescale = aSuggestedSize.width > 0 && aSuggestedSize.height > 0 &&
                  aSuggestedSize != aData.mPicSize;

  if (aSuggestedFormat == FORMAT_R5G6B5) {
#if defined(HAVE_YCBCR_TO_RGB565)
    if (prescale &&
        !IsScaleYCbCrToRGB565Fast(aData.mPicX,
                                  aData.mPicY,
                                  aData.mPicSize.width,
                                  aData.mPicSize.height,
                                  aSuggestedSize.width,
                                  aSuggestedSize.height,
                                  yuvtype,
                                  FILTER_BILINEAR) &&
        IsConvertYCbCrToRGB565Fast(aData.mPicX,
                                   aData.mPicY,
                                   aData.mPicSize.width,
                                   aData.mPicSize.height,
                                   yuvtype)) {
      prescale = false;
    }
#else
    // yuv2rgb16 function not available
    aSuggestedFormat = FORMAT_B8G8R8X8;
#endif
  }
  else if (aSuggestedFormat != FORMAT_B8G8R8X8) {
    // No other formats are currently supported.
    aSuggestedFormat = FORMAT_B8G8R8X8;
  }
  if (aSuggestedFormat == FORMAT_B8G8R8X8) {
    /* ScaleYCbCrToRGB32 does not support a picture offset, nor 4:4:4 data.
     See bugs 639415 and 640073. */
    if (aData.mPicX != 0 || aData.mPicY != 0 || yuvtype == YV24)
      prescale = false;
  }
  if (!prescale) {
    aSuggestedSize = aData.mPicSize;
  }
}

void
ConvertYCbCrToRGB(const layers::PlanarYCbCrData& aData,
                  const SurfaceFormat& aDestFormat,
                  const IntSize& aDestSize,
                  unsigned char* aDestBuffer,
                  int32_t aStride)
{
  // ConvertYCbCrToRGB et al. assume the chroma planes are rounded up if the
  // luma plane is odd sized.
  MOZ_ASSERT((aData.mCbCrSize.width == aData.mYSize.width ||
              aData.mCbCrSize.width == (aData.mYSize.width + 1) >> 1) &&
             (aData.mCbCrSize.height == aData.mYSize.height ||
              aData.mCbCrSize.height == (aData.mYSize.height + 1) >> 1));
  YUVType yuvtype =
    TypeFromSize(aData.mYSize.width,
                 aData.mYSize.height,
                 aData.mCbCrSize.width,
                 aData.mCbCrSize.height);

  // Convert from YCbCr to RGB now, scaling the image if needed.
  if (aDestSize != ToIntSize(aData.mPicSize)) {
#if defined(HAVE_YCBCR_TO_RGB565)
    if (aDestFormat == FORMAT_R5G6B5) {
      ScaleYCbCrToRGB565(aData.mYChannel,
                         aData.mCbChannel,
                         aData.mCrChannel,
                         aDestBuffer,
                         aData.mPicX,
                         aData.mPicY,
                         aData.mPicSize.width,
                         aData.mPicSize.height,
                         aDestSize.width,
                         aDestSize.height,
                         aData.mYStride,
                         aData.mCbCrStride,
                         aStride,
                         yuvtype,
                         FILTER_BILINEAR);
    } else
#endif
      ScaleYCbCrToRGB32(aData.mYChannel, //
                        aData.mCbChannel,
                        aData.mCrChannel,
                        aDestBuffer,
                        aData.mPicSize.width,
                        aData.mPicSize.height,
                        aDestSize.width,
                        aDestSize.height,
                        aData.mYStride,
                        aData.mCbCrStride,
                        aStride,
                        yuvtype,
                        ROTATE_0,
                        FILTER_BILINEAR);
  } else { // no prescale
#if defined(HAVE_YCBCR_TO_RGB565)
    if (aDestFormat == FORMAT_R5G6B5) {
      ConvertYCbCrToRGB565(aData.mYChannel,
                           aData.mCbChannel,
                           aData.mCrChannel,
                           aDestBuffer,
                           aData.mPicX,
                           aData.mPicY,
                           aData.mPicSize.width,
                           aData.mPicSize.height,
                           aData.mYStride,
                           aData.mCbCrStride,
                           aStride,
                           yuvtype);
    } else // aDestFormat != gfxImageFormatRGB16_565
#endif
      ConvertYCbCrToRGB32(aData.mYChannel, //
                          aData.mCbChannel,
                          aData.mCrChannel,
                          aDestBuffer,
                          aData.mPicX,
                          aData.mPicY,
                          aData.mPicSize.width,
                          aData.mPicSize.height,
                          aData.mYStride,
                          aData.mCbCrStride,
                          aStride,
                          yuvtype);
  }
}

}
}
