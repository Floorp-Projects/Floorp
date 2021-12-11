// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
// Y8   is a full plane of Y and no chroma planes (i.e., monochrome)
//
// ARGB pixel format is output, which on little endian is stored as BGRA.
// The alpha is set to 255, allowing the application to use RGBA or RGB32.

#include "yuv_convert.h"

#include "mozilla/StaticPrefs_gfx.h"
#include "libyuv.h"
#include "scale_yuv_argb.h"
// Header for low level row functions.
#include "yuv_row.h"
#include "mozilla/SSE.h"
#include "mozilla/IntegerRange.h"

namespace mozilla {

namespace gfx {

// 16.16 fixed point arithmetic
const int kFractionBits = 16;
const int kFractionMax = 1 << kFractionBits;
const int kFractionMask = ((1 << kFractionBits) - 1);

// clang-format off

YUVType TypeFromSize(int ywidth,
                     int yheight,
                     int cbcrwidth,
                     int cbcrheight)
{
  if (ywidth == cbcrwidth && yheight == cbcrheight) {
    return YV24;
  }
  else if ((ywidth + 1) / 2 == cbcrwidth && yheight == cbcrheight) {
    return YV16;
  }
  else if ((ywidth + 1) / 2 == cbcrwidth && (yheight + 1) / 2 == cbcrheight) {
    return YV12;
  }
  else if (cbcrwidth == 0 && cbcrheight == 0) {
    return Y8;
  }
  else {
    MOZ_CRASH("Can't determine YUV type from size");
  }
}

libyuv::FourCC FourCCFromYUVType(YUVType aYUVType) {
  switch (aYUVType) {
    case YV24: return libyuv::FOURCC_I444;
    case YV16: return libyuv::FOURCC_I422;
    case YV12: return libyuv::FOURCC_I420;
    case   Y8: return libyuv::FOURCC_I400;
    default:   return libyuv::FOURCC_ANY;
  }
}

int GBRPlanarToARGB(const uint8_t* src_y, int y_pitch,
                     const uint8_t* src_u, int u_pitch,
                     const uint8_t* src_v, int v_pitch,
                     uint8_t* rgb_buf, int rgb_pitch,
                     int pic_width, int pic_height) {
  // libyuv has no native conversion function for this
  // fixme: replace with something less awful
  for (const auto row : IntegerRange(pic_height)) {
    for (const auto col : IntegerRange(pic_width)) {
      rgb_buf[rgb_pitch * row + col * 4 + 0] = src_u[u_pitch * row + col];
      rgb_buf[rgb_pitch * row + col * 4 + 1] = src_y[y_pitch * row + col];
      rgb_buf[rgb_pitch * row + col * 4 + 2] = src_v[v_pitch * row + col];
      rgb_buf[rgb_pitch * row + col * 4 + 3] = 255;
    }
  }
  return 0;
}

// Convert a frame of YUV to 32 bit ARGB.
void ConvertYCbCrToRGB32(const uint8* y_buf, const uint8* u_buf,
                         const uint8* v_buf, uint8* rgb_buf, int pic_x,
                         int pic_y, int pic_width, int pic_height, int y_pitch,
                         int uv_pitch, int rgb_pitch, YUVType yuv_type,
                         YUVColorSpace yuv_color_space,
                         ColorRange color_range) {
  // Deprecated function's conversion is accurate.
  // libyuv converion is a bit inaccurate to get performance. It dynamically
  // calculates RGB from YUV to use simd. In it, signed byte is used for
  // conversion's coefficient, but it requests 129. libyuv cut 129 to 127. And
  // only 6 bits are used for a decimal part during the dynamic calculation.
  //
  // The function is still fast on some old intel chips.
  // See Bug 1256475.
  bool use_deprecated = StaticPrefs::gfx_ycbcr_accurate_conversion() ||
                        (supports_mmx() && supports_sse() && !supports_sse3() &&
                         yuv_color_space == YUVColorSpace::BT601 &&
                         color_range == ColorRange::LIMITED);
  // The deprecated function only support BT601.
  // See Bug 1210357.
  if (yuv_color_space != YUVColorSpace::BT601) {
    use_deprecated = false;
  }
  if (use_deprecated) {
    ConvertYCbCrToRGB32_deprecated(y_buf, u_buf, v_buf, rgb_buf, pic_x, pic_y,
                                   pic_width, pic_height, y_pitch, uv_pitch,
                                   rgb_pitch, yuv_type);
    return;
  }

  decltype(libyuv::I420ToARGBMatrix)* fConvertYUVToARGB = nullptr;
  const uint8* src_y = nullptr;
  const uint8* src_u = nullptr;
  const uint8* src_v = nullptr;
  const libyuv::YuvConstants* yuv_constant = nullptr;

  switch (yuv_color_space) {
    case YUVColorSpace::BT2020:
      yuv_constant = color_range == ColorRange::LIMITED
        ? &libyuv::kYuv2020Constants
        : &libyuv::kYuvV2020Constants;
      break;
    case YUVColorSpace::BT709:
      yuv_constant = color_range == ColorRange::LIMITED
        ? &libyuv::kYuvH709Constants
        : &libyuv::kYuvF709Constants;
      break;
    case YUVColorSpace::Identity:
      MOZ_ASSERT(yuv_type == YV24, "Identity (aka RGB) with chroma subsampling is unsupported");
      if (yuv_type == YV24) {
        break;
      }
      [[fallthrough]]; // Assuming BT601 for unsupported input is better than crashing
    default:
      MOZ_FALLTHROUGH_ASSERT("Unsupported YUVColorSpace");
    case YUVColorSpace::BT601:
      yuv_constant = color_range == ColorRange::LIMITED
        ? &libyuv::kYuvI601Constants
        : &libyuv::kYuvJPEGConstants;
      break;
  }

  switch (yuv_type) {
    case YV24: {
      src_y = y_buf + y_pitch * pic_y + pic_x;
      src_u = u_buf + uv_pitch * pic_y + pic_x;
      src_v = v_buf + uv_pitch * pic_y + pic_x;

      if (yuv_color_space == YUVColorSpace::Identity) {
        // Special case for RGB image
        DebugOnly<int> err =
          GBRPlanarToARGB(src_y, y_pitch, src_u, uv_pitch, src_v, uv_pitch,
                            rgb_buf, rgb_pitch, pic_width, pic_height);
        MOZ_ASSERT(!err);
        return;
      }

      fConvertYUVToARGB = libyuv::I444ToARGBMatrix;
      break;
    }
    case YV16: {
      src_y = y_buf + y_pitch * pic_y + pic_x;
      src_u = u_buf + uv_pitch * pic_y + pic_x / 2;
      src_v = v_buf + uv_pitch * pic_y + pic_x / 2;

      fConvertYUVToARGB = libyuv::I422ToARGBMatrix;
      break;
    }
    case YV12: {
      src_y = y_buf + y_pitch * pic_y + pic_x;
      src_u = u_buf + (uv_pitch * pic_y + pic_x) / 2;
      src_v = v_buf + (uv_pitch * pic_y + pic_x) / 2;

      fConvertYUVToARGB = libyuv::I420ToARGBMatrix;
      break;
    }
    case Y8: {
      src_y = y_buf + y_pitch * pic_y + pic_x;
      MOZ_ASSERT(u_buf == nullptr);
      MOZ_ASSERT(v_buf == nullptr);

      if (color_range == ColorRange::LIMITED) {
        DebugOnly<int> err =
            libyuv::I400ToARGB(src_y, y_pitch, rgb_buf, rgb_pitch, pic_width,
                              pic_height);
        MOZ_ASSERT(!err);
      } else {
        DebugOnly<int> err =
            libyuv::J400ToARGB(src_y, y_pitch, rgb_buf, rgb_pitch, pic_width,
                              pic_height);
        MOZ_ASSERT(!err);
      }

      return;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported YUV type");
  }

  DebugOnly<int> err =
    fConvertYUVToARGB(src_y, y_pitch, src_u, uv_pitch, src_v, uv_pitch,
                      rgb_buf, rgb_pitch, yuv_constant, pic_width, pic_height);
  MOZ_ASSERT(!err);
}

// Convert a frame of YUV to 32 bit ARGB.
void ConvertYCbCrToRGB32_deprecated(const uint8* y_buf,
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
  // Test for SSE because the optimized code uses movntq, which is not part of MMX.
  bool has_sse = supports_mmx() && supports_sse();
  // There is no optimized YV24 SSE routine so we check for this and
  // fall back to the C code.
  has_sse &= yuv_type != YV24;
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

    if (has_sse) {
      FastConvertYUVToRGB32Row(y_ptr,
                               u_ptr,
                               v_ptr,
                               rgb_row,
                               x_width);
    }
    else {
      FastConvertYUVToRGB32Row_C(y_ptr,
                                 u_ptr,
                                 v_ptr,
                                 rgb_row,
                                 x_width,
                                 x_shift);
    }
  }

  // MMX used for FastConvertYUVToRGB32Row requires emms instruction.
  if (has_sse)
    EMMS();
}

// C version does 8 at a time to mimic MMX code
static void FilterRows_C(uint8* ybuf, const uint8* y0_ptr, const uint8* y1_ptr,
                         int source_width, int source_y_fraction) {
  int y1_fraction = source_y_fraction;
  int y0_fraction = 256 - y1_fraction;
  uint8* end = ybuf + source_width;
  do {
    ybuf[0] = (y0_ptr[0] * y0_fraction + y1_ptr[0] * y1_fraction) >> 8;
    ybuf[1] = (y0_ptr[1] * y0_fraction + y1_ptr[1] * y1_fraction) >> 8;
    ybuf[2] = (y0_ptr[2] * y0_fraction + y1_ptr[2] * y1_fraction) >> 8;
    ybuf[3] = (y0_ptr[3] * y0_fraction + y1_ptr[3] * y1_fraction) >> 8;
    ybuf[4] = (y0_ptr[4] * y0_fraction + y1_ptr[4] * y1_fraction) >> 8;
    ybuf[5] = (y0_ptr[5] * y0_fraction + y1_ptr[5] * y1_fraction) >> 8;
    ybuf[6] = (y0_ptr[6] * y0_fraction + y1_ptr[6] * y1_fraction) >> 8;
    ybuf[7] = (y0_ptr[7] * y0_fraction + y1_ptr[7] * y1_fraction) >> 8;
    y0_ptr += 8;
    y1_ptr += 8;
    ybuf += 8;
  } while (ybuf < end);
}

#ifdef MOZILLA_MAY_SUPPORT_MMX
void FilterRows_MMX(uint8* ybuf, const uint8* y0_ptr, const uint8* y1_ptr,
                    int source_width, int source_y_fraction);
#endif

#ifdef MOZILLA_MAY_SUPPORT_SSE2
void FilterRows_SSE2(uint8* ybuf, const uint8* y0_ptr, const uint8* y1_ptr,
                     int source_width, int source_y_fraction);
#endif

static inline void FilterRows(uint8* ybuf, const uint8* y0_ptr,
                              const uint8* y1_ptr, int source_width,
                              int source_y_fraction) {
#ifdef MOZILLA_MAY_SUPPORT_SSE2
  if (mozilla::supports_sse2()) {
    FilterRows_SSE2(ybuf, y0_ptr, y1_ptr, source_width, source_y_fraction);
    return;
  }
#endif

#ifdef MOZILLA_MAY_SUPPORT_MMX
  if (mozilla::supports_mmx()) {
    FilterRows_MMX(ybuf, y0_ptr, y1_ptr, source_width, source_y_fraction);
    return;
  }
#endif

  FilterRows_C(ybuf, y0_ptr, y1_ptr, source_width, source_y_fraction);
}


// Scale a frame of YUV to 32 bit ARGB.
void ScaleYCbCrToRGB32(const uint8* y_buf,
                       const uint8* u_buf,
                       const uint8* v_buf,
                       uint8* rgb_buf,
                       int source_width,
                       int source_height,
                       int width,
                       int height,
                       int y_pitch,
                       int uv_pitch,
                       int rgb_pitch,
                       YUVType yuv_type,
                       YUVColorSpace yuv_color_space,
                       ScaleFilter filter) {
  bool use_deprecated =
      StaticPrefs::gfx_ycbcr_accurate_conversion() ||
#if defined(XP_WIN) && defined(_M_X64)
      // libyuv does not support SIMD scaling on win 64bit. See Bug 1295927.
      supports_sse3() ||
#endif
      (supports_mmx() && supports_sse() && !supports_sse3());
  // The deprecated function only support BT601.
  // See Bug 1210357.
  if (yuv_color_space != YUVColorSpace::BT601) {
    use_deprecated = false;
  }
  if (use_deprecated) {
    ScaleYCbCrToRGB32_deprecated(y_buf, u_buf, v_buf,
                                 rgb_buf,
                                 source_width, source_height,
                                 width, height,
                                 y_pitch, uv_pitch,
                                 rgb_pitch,
                                 yuv_type,
                                 ROTATE_0,
                                 filter);
    return;
  }

  DebugOnly<int> err =
    libyuv::YUVToARGBScale(y_buf, y_pitch,
                           u_buf, uv_pitch,
                           v_buf, uv_pitch,
                           FourCCFromYUVType(yuv_type),
                           yuv_color_space,
                           source_width, source_height,
                           rgb_buf, rgb_pitch,
                           width, height,
                           libyuv::kFilterBilinear);
  MOZ_ASSERT(!err);
  return;
}

// Scale a frame of YUV to 32 bit ARGB.
void ScaleYCbCrToRGB32_deprecated(const uint8* y_buf,
                                  const uint8* u_buf,
                                  const uint8* v_buf,
                                  uint8* rgb_buf,
                                  int source_width,
                                  int source_height,
                                  int width,
                                  int height,
                                  int y_pitch,
                                  int uv_pitch,
                                  int rgb_pitch,
                                  YUVType yuv_type,
                                  Rotate view_rotate,
                                  ScaleFilter filter) {
  bool has_mmx = supports_mmx();

  // 4096 allows 3 buffers to fit in 12k.
  // Helps performance on CPU with 16K L1 cache.
  // Large enough for 3830x2160 and 30" displays which are 2560x1600.
  const int kFilterBufferSize = 4096;
  // Disable filtering if the screen is too big (to avoid buffer overflows).
  // This should never happen to regular users: they don't have monitors
  // wider than 4096 pixels.
  // TODO(fbarchard): Allow rotated videos to filter.
  if (source_width > kFilterBufferSize || view_rotate)
    filter = FILTER_NONE;

  unsigned int y_shift = yuv_type == YV12 ? 1 : 0;
  // Diagram showing origin and direction of source sampling.
  // ->0   4<-
  // 7       3
  //
  // 6       5
  // ->1   2<-
  // Rotations that start at right side of image.
  if ((view_rotate == ROTATE_180) ||
      (view_rotate == ROTATE_270) ||
      (view_rotate == MIRROR_ROTATE_0) ||
      (view_rotate == MIRROR_ROTATE_90)) {
    y_buf += source_width - 1;
    u_buf += source_width / 2 - 1;
    v_buf += source_width / 2 - 1;
    source_width = -source_width;
  }
  // Rotations that start at bottom of image.
  if ((view_rotate == ROTATE_90) ||
      (view_rotate == ROTATE_180) ||
      (view_rotate == MIRROR_ROTATE_90) ||
      (view_rotate == MIRROR_ROTATE_180)) {
    y_buf += (source_height - 1) * y_pitch;
    u_buf += ((source_height >> y_shift) - 1) * uv_pitch;
    v_buf += ((source_height >> y_shift) - 1) * uv_pitch;
    source_height = -source_height;
  }

  // Handle zero sized destination.
  if (width == 0 || height == 0)
    return;
  int source_dx = source_width * kFractionMax / width;
  int source_dy = source_height * kFractionMax / height;
  int source_dx_uv = source_dx;

  if ((view_rotate == ROTATE_90) ||
      (view_rotate == ROTATE_270)) {
    int tmp = height;
    height = width;
    width = tmp;
    tmp = source_height;
    source_height = source_width;
    source_width = tmp;
    int original_dx = source_dx;
    int original_dy = source_dy;
    source_dx = ((original_dy >> kFractionBits) * y_pitch) << kFractionBits;
    source_dx_uv = ((original_dy >> kFractionBits) * uv_pitch) << kFractionBits;
    source_dy = original_dx;
    if (view_rotate == ROTATE_90) {
      y_pitch = -1;
      uv_pitch = -1;
      source_height = -source_height;
    } else {
      y_pitch = 1;
      uv_pitch = 1;
    }
  }

  // Need padding because FilterRows() will write 1 to 16 extra pixels
  // after the end for SSE2 version.
  uint8 yuvbuf[16 + kFilterBufferSize * 3 + 16];
  uint8* ybuf =
      reinterpret_cast<uint8*>(reinterpret_cast<uintptr_t>(yuvbuf + 15) & ~15);
  uint8* ubuf = ybuf + kFilterBufferSize;
  uint8* vbuf = ubuf + kFilterBufferSize;
  // TODO(fbarchard): Fixed point math is off by 1 on negatives.
  int yscale_fixed = (source_height << kFractionBits) / height;

  // TODO(fbarchard): Split this into separate function for better efficiency.
  for (int y = 0; y < height; ++y) {
    uint8* dest_pixel = rgb_buf + y * rgb_pitch;
    int source_y_subpixel = (y * yscale_fixed);
    if (yscale_fixed >= (kFractionMax * 2)) {
      source_y_subpixel += kFractionMax / 2;  // For 1/2 or less, center filter.
    }
    int source_y = source_y_subpixel >> kFractionBits;

    const uint8* y0_ptr = y_buf + source_y * y_pitch;
    const uint8* y1_ptr = y0_ptr + y_pitch;

    const uint8* u0_ptr = u_buf + (source_y >> y_shift) * uv_pitch;
    const uint8* u1_ptr = u0_ptr + uv_pitch;
    const uint8* v0_ptr = v_buf + (source_y >> y_shift) * uv_pitch;
    const uint8* v1_ptr = v0_ptr + uv_pitch;

    // vertical scaler uses 16.8 fixed point
    int source_y_fraction = (source_y_subpixel & kFractionMask) >> 8;
    int source_uv_fraction =
        ((source_y_subpixel >> y_shift) & kFractionMask) >> 8;

    const uint8* y_ptr = y0_ptr;
    const uint8* u_ptr = u0_ptr;
    const uint8* v_ptr = v0_ptr;
    // Apply vertical filtering if necessary.
    // TODO(fbarchard): Remove memcpy when not necessary.
    if (filter & mozilla::gfx::FILTER_BILINEAR_V) {
      if (yscale_fixed != kFractionMax &&
          source_y_fraction && ((source_y + 1) < source_height)) {
        FilterRows(ybuf, y0_ptr, y1_ptr, source_width, source_y_fraction);
      } else {
        memcpy(ybuf, y0_ptr, source_width);
      }
      y_ptr = ybuf;
      ybuf[source_width] = ybuf[source_width-1];
      int uv_source_width = (source_width + 1) / 2;
      if (yscale_fixed != kFractionMax &&
          source_uv_fraction &&
          (((source_y >> y_shift) + 1) < (source_height >> y_shift))) {
        FilterRows(ubuf, u0_ptr, u1_ptr, uv_source_width, source_uv_fraction);
        FilterRows(vbuf, v0_ptr, v1_ptr, uv_source_width, source_uv_fraction);
      } else {
        memcpy(ubuf, u0_ptr, uv_source_width);
        memcpy(vbuf, v0_ptr, uv_source_width);
      }
      u_ptr = ubuf;
      v_ptr = vbuf;
      ubuf[uv_source_width] = ubuf[uv_source_width - 1];
      vbuf[uv_source_width] = vbuf[uv_source_width - 1];
    }
    if (source_dx == kFractionMax) {  // Not scaled
      FastConvertYUVToRGB32Row(y_ptr, u_ptr, v_ptr,
                               dest_pixel, width);
    } else if (filter & FILTER_BILINEAR_H) {
        LinearScaleYUVToRGB32Row(y_ptr, u_ptr, v_ptr,
                                 dest_pixel, width, source_dx);
    } else {
// Specialized scalers and rotation.
#if defined(MOZILLA_MAY_SUPPORT_SSE) && defined(_MSC_VER) && defined(_M_IX86) && !defined(__clang__)
      if(mozilla::supports_sse()) {
        if (width == (source_width * 2)) {
          DoubleYUVToRGB32Row_SSE(y_ptr, u_ptr, v_ptr,
                                  dest_pixel, width);
        } else if ((source_dx & kFractionMask) == 0) {
          // Scaling by integer scale factor. ie half.
          ConvertYUVToRGB32Row_SSE(y_ptr, u_ptr, v_ptr,
                                   dest_pixel, width,
                                   source_dx >> kFractionBits);
        } else if (source_dx_uv == source_dx) {  // Not rotated.
          ScaleYUVToRGB32Row(y_ptr, u_ptr, v_ptr,
                             dest_pixel, width, source_dx);
        } else {
          RotateConvertYUVToRGB32Row_SSE(y_ptr, u_ptr, v_ptr,
                                         dest_pixel, width,
                                         source_dx >> kFractionBits,
                                         source_dx_uv >> kFractionBits);
        }
      }
      else {
        ScaleYUVToRGB32Row_C(y_ptr, u_ptr, v_ptr,
                             dest_pixel, width, source_dx);
      }
#else
      (void)source_dx_uv;
      ScaleYUVToRGB32Row(y_ptr, u_ptr, v_ptr,
                         dest_pixel, width, source_dx);
#endif
    }
  }
  // MMX used for FastConvertYUVToRGB32Row and FilterRows requires emms.
  if (has_mmx)
    EMMS();
}
void ConvertI420AlphaToARGB32(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              const uint8* a_buf,
                              uint8* argb_buf,
                              int pic_width,
                              int pic_height,
                              int ya_pitch,
                              int uv_pitch,
                              int argb_pitch) {

  // The downstream graphics stack expects an attenuated input, hence why the
  // attenuation parameter is set.
  DebugOnly<int> err = libyuv::I420AlphaToARGB(y_buf, ya_pitch,
                                               u_buf, uv_pitch,
                                               v_buf, uv_pitch,
                                               a_buf, ya_pitch,
                                               argb_buf, argb_pitch,
                                               pic_width, pic_height, 1);
  MOZ_ASSERT(!err);
}

} // namespace gfx
} // namespace mozilla
