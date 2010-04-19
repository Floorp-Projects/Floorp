// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_YUV_CONVERT_H_
#define MEDIA_BASE_YUV_CONVERT_H_

#include "chromium_types.h"
#include "gfxCore.h"

namespace mozilla {

namespace gfx {

// Type of YUV surface.
// The value of these enums matter as they are used to shift vertical indices.
enum YUVType {
  YV16 = 0,           // YV16 is half width and full height chroma channels.
  YV12 = 1            // YV12 is half width and half height chroma channels.
};

// Convert a frame of YUV to 32 bit ARGB.
// Pass in YV16/YV12 depending on source format
NS_GFX_(void) ConvertYCbCrToRGB32(const uint8* yplane,
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

}  // namespace gfx
}  // namespace mozilla

#endif  // MEDIA_BASE_YUV_CONVERT_H_
