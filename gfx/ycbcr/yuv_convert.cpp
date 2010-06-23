// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This webpage shows layout of YV12 and other YUV formats
// http://www.fourcc.org/yuv.php
// The actual conversion is best described here
// http://en.wikipedia.org/wiki/YUV
// An article on optimizing YUV conversion using tables instead of multiplies
// http://lestourtereaux.free.fr/papers/data/yuvrgb.pdf
//
// YV12 is a full plane of Y and a half height, half width chroma planes
// YV16 is a full plane of Y and a full height, half width chroma planes
// YV24 is a full plane of Y and a full height, full width chroma planes
//
// ARGB pixel format is output, which on little endian is stored as BGRA.
// The alpha is set to 255, allowing the application to use RGBA or RGB32.

#include "yuv_convert.h"

// Header for low level row functions.
#include "yuv_row.h"
#include "mozilla/SSE.h"

namespace mozilla {

namespace gfx {

// Convert a frame of YUV to 32 bit ARGB.
NS_GFX_(void) ConvertYCbCrToRGB32(const uint8* y_buf,
                                  const uint8* u_buf,
                                  const uint8* v_buf,
                                  uint8* rgb_buf,
                                  int pic_x,
                                  int pic_y,
                                  int pic_width,
                                  int pic_height,
                                  int y_pitch,
                                  int uv_pitch,
                                  int rgb_pitch,
                                  YUVType yuv_type) {
  unsigned int y_shift = yuv_type == YV12 ? 1 : 0;
  unsigned int x_shift = yuv_type == YV24 ? 0 : 1;
  // There is no optimized YV24 MMX routine so we check for this and
  // fall back to the C code.
  bool has_mmx = supports_mmx() && yuv_type != YV24;
  bool odd_pic_x = yuv_type != YV24 && pic_x % 2 != 0;
  int x_width = odd_pic_x ? pic_width - 1 : pic_width;

  for (int y = pic_y; y < pic_height + pic_y; ++y) {
    uint8* rgb_row = rgb_buf + (y - pic_y) * rgb_pitch;
    const uint8* y_ptr = y_buf + y * y_pitch + pic_x;
    const uint8* u_ptr = u_buf + (y >> y_shift) * uv_pitch + (pic_x >> x_shift);
    const uint8* v_ptr = v_buf + (y >> y_shift) * uv_pitch + (pic_x >> x_shift);

    if (odd_pic_x) {
      // Handle the single odd pixel manually and use the
      // fast routines for the remaining.
      FastConvertYUVToRGB32Row_C(y_ptr++,
                                 u_ptr++,
                                 v_ptr++,
                                 rgb_row,
                                 1,
                                 x_shift);
      rgb_row += 4;
    }

    if (has_mmx)
      FastConvertYUVToRGB32Row(y_ptr,
                               u_ptr,
                               v_ptr,
                               rgb_row,
                               x_width);
    else
      FastConvertYUVToRGB32Row_C(y_ptr,
                                 u_ptr,
                                 v_ptr,
                                 rgb_row,
                                 x_width,
                                 x_shift);
  }

#ifdef ARCH_CPU_X86_FAMILY
  // MMX used for FastConvertYUVToRGB32Row requires emms instruction.
  if (has_mmx)
    EMMS();
#endif
}

}  // namespace gfx
}  // namespace mozilla
