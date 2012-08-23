// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MEDIA_BASE_YCBCR_TO_RGB565_H_
#define MEDIA_BASE_YCBCR_TO_RGB565_H_
#include "yuv_convert.h"
#include "mozilla/arm.h"

// It's currently only worth including this if we have NEON support.
#ifdef MOZILLA_MAY_SUPPORT_NEON
#define HAVE_YCBCR_TO_RGB565 1
#endif

namespace mozilla {

namespace gfx {

#ifdef HAVE_YCBCR_TO_RGB565
// Convert a frame of YUV to 16 bit RGB565.
NS_GFX_(void) ConvertYCbCrToRGB565(const uint8* yplane,
                                   const uint8* uplane,
                                   const uint8* vplane,
                                   uint8* rgbframe,
                                   int pic_x,
                                   int pic_y,
                                   int pic_width,
                                   int pic_height,
                                   int ystride,
                                   int uvstride,
                                   int rgbstride,
                                   YUVType yuv_type);

// Used to test if we have an accelerated version.
NS_GFX_(bool) IsConvertYCbCrToRGB565Fast(int pic_x,
                                         int pic_y,
                                         int pic_width,
                                         int pic_height,
                                         YUVType yuv_type);

// Scale a frame of YUV to 16 bit RGB565.
NS_GFX_(void) ScaleYCbCrToRGB565(const uint8_t *yplane,
                                 const uint8_t *uplane,
                                 const uint8_t *vplane,
                                 uint8_t *rgbframe,
                                 int source_x0,
                                 int source_y0,
                                 int source_width,
                                 int source_height,
                                 int width,
                                 int height,
                                 int ystride,
                                 int uvstride,
                                 int rgbstride,
                                 YUVType yuv_type,
                                 ScaleFilter filter);

// Used to test if we have an accelerated version.
NS_GFX_(bool) IsScaleYCbCrToRGB565Fast(int source_x0,
                                       int source_y0,
                                       int source_width,
                                       int source_height,
                                       int width,
                                       int height,
                                       YUVType yuv_type,
                                       ScaleFilter filter);
#endif // HAVE_YCBCR_TO_RGB565

}  // namespace gfx

}  // namespace mozilla

#endif // MEDIA_BASE_YCBCR_TO_RGB565_H_
