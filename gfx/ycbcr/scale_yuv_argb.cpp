/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *  Copyright 2016 Mozilla Foundation
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/scale.h"

#include <assert.h>
#include <string.h>

#include "libyuv/cpu_id.h"
#include "libyuv/row.h"
#include "libyuv/scale_row.h"
#include "libyuv/video_common.h"

#include "mozilla/gfx/Types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// YUV to RGB conversion and scaling functions were implemented by referencing
// scale_argb.cc
//
// libyuv already has ScaleYUVToARGBBilinearUp(), but its implementation is not
// completed yet. Implementations of the functions are based on it.
// At first, ScaleYUVToARGBBilinearUp() was implemented by modidying the
// libyuv's one. Then all another functions were implemented similarly.
//
// Function relationship between yuv_convert.cpp abd scale_argb.cc are like
// the followings
//  - ScaleYUVToARGBDown2()      <-- ScaleARGBDown2()
//  - ScaleYUVToARGBDownEven()   <-- ScaleARGBDownEven()
//  - ScaleYUVToARGBBilinearDown() <-- ScaleARGBBilinearDown()
//  - ScaleYUVToARGBBilinearUp() <-- ScaleARGBBilinearUp() and ScaleYUVToARGBBilinearUp() in libyuv
//  - ScaleYUVToARGBSimple()     <-- ScaleARGBSimple()
//  - ScaleYUVToARGB()           <-- ScaleARGB() // Removed some function calls for simplicity.
//  - YUVToARGBScale()           <-- ARGBScale()
//
// Callings and selections of InterpolateRow() and ScaleARGBFilterCols() were
// kept as same as possible.
//
// The followings changes were done to each scaling functions.
//
// -[1] Allocate YUV conversion buffer and use it as source buffer of scaling.
//      Its usage is borrowed from the libyuv's ScaleYUVToARGBBilinearUp().
// -[2] Conversion from YUV to RGB was abstracted as YUVBuferIter.
//      It is for handling multiple yuv color formats.
// -[3] Modified scaling functions as to handle YUV conversion buffer and
//      use YUVBuferIter.
// -[4] Color conversion function selections in YUVBuferIter were borrowed from
//      I444ToARGBMatrix(), I422ToARGBMatrix() and I420ToARGBMatrix()

static __inline int Abs(int v) {
  return v >= 0 ? v : -v;
}

typedef mozilla::gfx::YUVColorSpace YUVColorSpace;

struct YUVBuferIter {
  int src_width;
  int src_height;
  int src_stride_y;
  int src_stride_u;
  int src_stride_v;
  const uint8* src_y;
  const uint8* src_u;
  const uint8* src_v;

  uint32 src_fourcc;
  const struct YuvConstants* yuvconstants;
  int y_index;
  const uint8* src_row_y;
  const uint8* src_row_u;
  const uint8* src_row_v;

  void (*YUVToARGBRow)(const uint8* y_buf,
                       const uint8* u_buf,
                       const uint8* v_buf,
                       uint8* rgb_buf,
                       const struct YuvConstants* yuvconstants,
                       int width);
  void (*MoveTo)(YUVBuferIter& iter, int y_index);
  void (*MoveToNextRow)(YUVBuferIter& iter);
};

void YUVBuferIter_InitI422(YUVBuferIter& iter) {
  iter.YUVToARGBRow = I422ToARGBRow_C;
#if defined(HAS_I422TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    iter.YUVToARGBRow = I422ToARGBRow_Any_SSSE3;
    if (IS_ALIGNED(iter.src_width, 8)) {
      iter.YUVToARGBRow = I422ToARGBRow_SSSE3;
    }
  }
#endif
#if defined(HAS_I422TOARGBROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    iter.YUVToARGBRow = I422ToARGBRow_Any_AVX2;
    if (IS_ALIGNED(iter.src_width, 16)) {
      iter.YUVToARGBRow = I422ToARGBRow_AVX2;
    }
  }
#endif
#if defined(HAS_I422TOARGBROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    iter.YUVToARGBRow = I422ToARGBRow_Any_NEON;
    if (IS_ALIGNED(iter.src_width, 8)) {
      iter.YUVToARGBRow = I422ToARGBRow_NEON;
    }
  }
#endif
#if defined(HAS_I422TOARGBROW_DSPR2)
  if (TestCpuFlag(kCpuHasDSPR2) && IS_ALIGNED(iter.src_width, 4) &&
      IS_ALIGNED(iter.src_y, 4) && IS_ALIGNED(iter.src_stride_y, 4) &&
      IS_ALIGNED(iter.src_u, 2) && IS_ALIGNED(iter.src_stride_u, 2) &&
      IS_ALIGNED(iter.src_v, 2) && IS_ALIGNED(iter.src_stride_v, 2) {
    // Always satisfy IS_ALIGNED(argb_cnv_row, 4) && IS_ALIGNED(argb_cnv_rowstride, 4)
    iter.YUVToARGBRow = I422ToARGBRow_DSPR2;
  }
#endif
}

void YUVBuferIter_InitI444(YUVBuferIter& iter) {
  iter.YUVToARGBRow = I444ToARGBRow_C;
#if defined(HAS_I444TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    iter.YUVToARGBRow = I444ToARGBRow_Any_SSSE3;
    if (IS_ALIGNED(iter.src_width, 8)) {
      iter.YUVToARGBRow = I444ToARGBRow_SSSE3;
    }
  }
#endif
#if defined(HAS_I444TOARGBROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    iter.YUVToARGBRow = I444ToARGBRow_Any_AVX2;
    if (IS_ALIGNED(iter.src_width, 16)) {
      iter.YUVToARGBRow = I444ToARGBRow_AVX2;
    }
  }
#endif
#if defined(HAS_I444TOARGBROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    iter.YUVToARGBRow = I444ToARGBRow_Any_NEON;
    if (IS_ALIGNED(iter.src_width, 8)) {
      iter.YUVToARGBRow = I444ToARGBRow_NEON;
    }
  }
#endif
}


static void YUVBuferIter_MoveToForI444(YUVBuferIter& iter, int y_index) {
  iter.y_index = y_index;
  iter.src_row_y = iter.src_y + y_index * iter.src_stride_y;
  iter.src_row_u = iter.src_u + y_index * iter.src_stride_u;
  iter.src_row_v = iter.src_v + y_index * iter.src_stride_v;
}

static void YUVBuferIter_MoveToNextRowForI444(YUVBuferIter& iter) {
  iter.src_row_y += iter.src_stride_y;
  iter.src_row_u += iter.src_stride_u;
  iter.src_row_v += iter.src_stride_v;
  iter.y_index++;
}

static void YUVBuferIter_MoveToForI422(YUVBuferIter& iter, int y_index) {
  iter.y_index = y_index;
  iter.src_row_y = iter.src_y + y_index * iter.src_stride_y;
  iter.src_row_u = iter.src_u + y_index * iter.src_stride_u;
  iter.src_row_v = iter.src_v + y_index * iter.src_stride_v;
}

static void YUVBuferIter_MoveToNextRowForI422(YUVBuferIter& iter) {
  iter.src_row_y += iter.src_stride_y;
  iter.src_row_u += iter.src_stride_u;
  iter.src_row_v += iter.src_stride_v;
  iter.y_index++;
}

static void YUVBuferIter_MoveToForI420(YUVBuferIter& iter, int y_index) {
  const int kYShift = 1;  // Shift Y by 1 to convert Y plane to UV coordinate.
  int uv_y_index = y_index >> kYShift;

  iter.y_index = y_index;
  iter.src_row_y = iter.src_y + y_index * iter.src_stride_y;
  iter.src_row_u = iter.src_u + uv_y_index * iter.src_stride_u;
  iter.src_row_v = iter.src_v + uv_y_index * iter.src_stride_v;
}

static void YUVBuferIter_MoveToNextRowForI420(YUVBuferIter& iter) {
  iter.src_row_y += iter.src_stride_y;
  if (iter.y_index & 1) {
    iter.src_row_u += iter.src_stride_u;
    iter.src_row_v += iter.src_stride_v;
  }
  iter.y_index++;
}

static __inline void YUVBuferIter_ConvertToARGBRow(YUVBuferIter& iter, uint8* argb_row) {
  iter.YUVToARGBRow(iter.src_row_y, iter.src_row_u, iter.src_row_v, argb_row, iter.yuvconstants, iter.src_width);
}

void YUVBuferIter_Init(YUVBuferIter& iter, uint32 src_fourcc, YUVColorSpace yuv_color_space) {
  iter.src_fourcc = src_fourcc;
  iter.y_index = 0;
  iter.src_row_y = iter.src_y;
  iter.src_row_u = iter.src_u;
  iter.src_row_v = iter.src_v;
  switch (yuv_color_space) {
    case YUVColorSpace::BT2020:
      iter.yuvconstants = &kYuv2020Constants;
      break;
    case YUVColorSpace::BT709:
      iter.yuvconstants = &kYuvH709Constants;
      break;
    default:
      iter.yuvconstants = &kYuvI601Constants;
  }

  if (src_fourcc == FOURCC_I444) {
    YUVBuferIter_InitI444(iter);
    iter.MoveTo = YUVBuferIter_MoveToForI444;
    iter.MoveToNextRow = YUVBuferIter_MoveToNextRowForI444;
  } else if(src_fourcc == FOURCC_I422){
    YUVBuferIter_InitI422(iter);
    iter.MoveTo = YUVBuferIter_MoveToForI422;
    iter.MoveToNextRow = YUVBuferIter_MoveToNextRowForI422;
  } else {
    assert(src_fourcc == FOURCC_I420); // Should be FOURCC_I420
    YUVBuferIter_InitI422(iter);
    iter.MoveTo = YUVBuferIter_MoveToForI420;
    iter.MoveToNextRow = YUVBuferIter_MoveToNextRowForI420;
  }
}

// ScaleARGB ARGB, 1/2
// This is an optimized version for scaling down a ARGB to 1/2 of
// its original size.
static void ScaleYUVToARGBDown2(int src_width, int src_height,
                                int dst_width, int dst_height,
                                int src_stride_y,
                                int src_stride_u,
                                int src_stride_v,
                                int dst_stride_argb,
                                const uint8* src_y,
                                const uint8* src_u,
                                const uint8* src_v,
                                uint8* dst_argb,
                                int x, int dx, int y, int dy,
                                enum FilterMode filtering,
                                uint32 src_fourcc,
                                YUVColorSpace yuv_color_space) {
  int j;

  // Allocate 2 rows of ARGB for source conversion.
  const int kRowSize = (src_width * 4 + 15) & ~15;
  align_buffer_64(argb_cnv_row, kRowSize * 2);
  uint8* argb_cnv_rowptr = argb_cnv_row;
  int argb_cnv_rowstride = kRowSize;

  YUVBuferIter iter;
  iter.src_width = src_width;
  iter.src_height = src_height;
  iter.src_stride_y = src_stride_y;
  iter.src_stride_u = src_stride_u;
  iter.src_stride_v = src_stride_v;
  iter.src_y = src_y;
  iter.src_u = src_u;
  iter.src_v = src_v;
  YUVBuferIter_Init(iter, src_fourcc, yuv_color_space);

  void (*ScaleARGBRowDown2)(const uint8* src_argb, ptrdiff_t src_stride,
                            uint8* dst_argb, int dst_width) =
    filtering == kFilterNone ? ScaleARGBRowDown2_C :
        (filtering == kFilterLinear ? ScaleARGBRowDown2Linear_C :
        ScaleARGBRowDown2Box_C);
  assert(dx == 65536 * 2);  // Test scale factor of 2.
  assert((dy & 0x1ffff) == 0);  // Test vertical scale is multiple of 2.
  // Advance to odd row, even column.
  int yi = y >> 16;
  iter.MoveTo(iter, yi);
  ptrdiff_t x_offset;
  if (filtering == kFilterBilinear) {
    x_offset = (x >> 16) * 4;
  } else {
    x_offset = ((x >> 16) - 1) * 4;
  }
#if defined(HAS_SCALEARGBROWDOWN2_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    ScaleARGBRowDown2 = filtering == kFilterNone ? ScaleARGBRowDown2_Any_SSE2 :
        (filtering == kFilterLinear ? ScaleARGBRowDown2Linear_Any_SSE2 :
        ScaleARGBRowDown2Box_Any_SSE2);
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBRowDown2 = filtering == kFilterNone ? ScaleARGBRowDown2_SSE2 :
          (filtering == kFilterLinear ? ScaleARGBRowDown2Linear_SSE2 :
          ScaleARGBRowDown2Box_SSE2);
    }
  }

#endif
#if defined(HAS_SCALEARGBROWDOWN2_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ScaleARGBRowDown2 = filtering == kFilterNone ? ScaleARGBRowDown2_Any_NEON :
        (filtering == kFilterLinear ? ScaleARGBRowDown2Linear_Any_NEON :
        ScaleARGBRowDown2Box_Any_NEON);
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleARGBRowDown2 = filtering == kFilterNone ? ScaleARGBRowDown2_NEON :
          (filtering == kFilterLinear ? ScaleARGBRowDown2Linear_NEON :
          ScaleARGBRowDown2Box_NEON);
    }
  }
#endif

  const int dyi = dy >> 16;
  int lastyi = yi;
  YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_rowptr);
  // Prepare next row if necessary
  if (filtering != kFilterLinear) {
    if ((yi + dyi) < (src_height - 1)) {
      iter.MoveTo(iter, yi + dyi);
      YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_rowptr + argb_cnv_rowstride);
    } else {
      argb_cnv_rowstride = 0;
    }
  }

  if (filtering == kFilterLinear) {
    argb_cnv_rowstride = 0;
  }
  const int max_yi = src_height - 1;
  const int max_yi_minus_dyi = max_yi - dyi;
  for (j = 0; j < dst_height; ++j) {
    if (yi != lastyi) {
      if (yi > max_yi) {
        yi = max_yi;
      }
      if (yi != lastyi) {
        if (filtering == kFilterLinear) {
          iter.MoveTo(iter, yi);
          YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_rowptr);
          lastyi = yi;
        } else {
          // Prepare current row
          if (yi == iter.y_index) {
            argb_cnv_rowptr = argb_cnv_rowptr + argb_cnv_rowstride;
            argb_cnv_rowstride = - argb_cnv_rowstride;
          } else {
            iter.MoveTo(iter, yi);
            argb_cnv_rowptr = argb_cnv_row;
            argb_cnv_rowstride = kRowSize;
            YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_rowptr);
          }
          // Prepare next row if necessary
          if (iter.y_index  < max_yi) {
            int next_yi = yi < max_yi_minus_dyi ? yi + dyi : max_yi;
            iter.MoveTo(iter, next_yi);
            YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_rowptr + argb_cnv_rowstride);
          } else {
            argb_cnv_rowstride = 0;
          }
          lastyi = yi;
        }
      }
    }
    ScaleARGBRowDown2(argb_cnv_rowptr + x_offset, argb_cnv_rowstride, dst_argb, dst_width);
    dst_argb += dst_stride_argb;
    yi += dyi;
  }

  free_aligned_buffer_64(argb_cnv_row);
}

// ScaleARGB ARGB Even
// This is an optimized version for scaling down a ARGB to even
// multiple of its original size.
static void ScaleYUVToARGBDownEven(int src_width, int src_height,
                                   int dst_width, int dst_height,
                                   int src_stride_y,
                                   int src_stride_u,
                                   int src_stride_v,
                                   int dst_stride_argb,
                                   const uint8* src_y,
                                   const uint8* src_u,
                                   const uint8* src_v,
                                   uint8* dst_argb,
                                   int x, int dx, int y, int dy,
                                   enum FilterMode filtering,
                                   uint32 src_fourcc,
                                   YUVColorSpace yuv_color_space) {
  int j;
  // Allocate 2 rows of ARGB for source conversion.
  const int kRowSize = (src_width * 4 + 15) & ~15;
  align_buffer_64(argb_cnv_row, kRowSize * 2);
  uint8* argb_cnv_rowptr = argb_cnv_row;
  int argb_cnv_rowstride = kRowSize;

  int col_step = dx >> 16;
  void (*ScaleARGBRowDownEven)(const uint8* src_argb, ptrdiff_t src_stride,
                               int src_step, uint8* dst_argb, int dst_width) =
      filtering ? ScaleARGBRowDownEvenBox_C : ScaleARGBRowDownEven_C;
  assert(IS_ALIGNED(src_width, 2));
  assert(IS_ALIGNED(src_height, 2));
  int yi = y >> 16;
  const ptrdiff_t x_offset = (x >> 16) * 4;

#if defined(HAS_SCALEARGBROWDOWNEVEN_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    ScaleARGBRowDownEven = filtering ? ScaleARGBRowDownEvenBox_Any_SSE2 :
        ScaleARGBRowDownEven_Any_SSE2;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBRowDownEven = filtering ? ScaleARGBRowDownEvenBox_SSE2 :
          ScaleARGBRowDownEven_SSE2;
    }
  }
#endif
#if defined(HAS_SCALEARGBROWDOWNEVEN_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ScaleARGBRowDownEven = filtering ? ScaleARGBRowDownEvenBox_Any_NEON :
        ScaleARGBRowDownEven_Any_NEON;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBRowDownEven = filtering ? ScaleARGBRowDownEvenBox_NEON :
          ScaleARGBRowDownEven_NEON;
    }
  }
#endif

  YUVBuferIter iter;
  iter.src_width = src_width;
  iter.src_height = src_height;
  iter.src_stride_y = src_stride_y;
  iter.src_stride_u = src_stride_u;
  iter.src_stride_v = src_stride_v;
  iter.src_y = src_y;
  iter.src_u = src_u;
  iter.src_v = src_v;
  YUVBuferIter_Init(iter, src_fourcc, yuv_color_space);

  const int dyi = dy >> 16;
  int lastyi = yi;
  YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_rowptr);
  // Prepare next row if necessary
  if (filtering != kFilterLinear) {
    if ((yi + dyi) < (src_height - 1)) {
      iter.MoveTo(iter, yi + dyi);
      YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_rowptr + argb_cnv_rowstride);
    } else {
      argb_cnv_rowstride = 0;
    }
  }

  if (filtering == kFilterLinear) {
    argb_cnv_rowstride = 0;
  }
  const int max_yi = src_height - 1;
  const int max_yi_minus_dyi = max_yi - dyi;
  for (j = 0; j < dst_height; ++j) {
    if (yi != lastyi) {
      if (yi > max_yi) {
        yi = max_yi;
      }
      if (yi != lastyi) {
        if (filtering == kFilterLinear) {
          iter.MoveTo(iter, yi);
          YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_rowptr);
          lastyi = yi;
        } else {
          // Prepare current row
          if (yi == iter.y_index) {
            argb_cnv_rowptr = argb_cnv_rowptr + argb_cnv_rowstride;
            argb_cnv_rowstride = - argb_cnv_rowstride;
          } else {
            iter.MoveTo(iter, yi);
            argb_cnv_rowptr = argb_cnv_row;
            argb_cnv_rowstride = kRowSize;
            YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_rowptr);
          }
          // Prepare next row if necessary
          if (iter.y_index  < max_yi) {
            int next_yi = yi < max_yi_minus_dyi ? yi + dyi : max_yi;
            iter.MoveTo(iter, next_yi);
            YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_rowptr + argb_cnv_rowstride);
          } else {
            argb_cnv_rowstride = 0;
          }
          lastyi = yi;
        }
      }
    }
    ScaleARGBRowDownEven(argb_cnv_rowptr + x_offset, argb_cnv_rowstride, col_step, dst_argb, dst_width);
    dst_argb += dst_stride_argb;
    yi += dyi;
  }
  free_aligned_buffer_64(argb_cnv_row);
}

// Scale YUV to ARGB down with bilinear interpolation.
static void ScaleYUVToARGBBilinearDown(int src_width, int src_height,
                                       int dst_width, int dst_height,
                                       int src_stride_y,
                                       int src_stride_u,
                                       int src_stride_v,
                                       int dst_stride_argb,
                                       const uint8* src_y,
                                       const uint8* src_u,
                                       const uint8* src_v,
                                       uint8* dst_argb,
                                       int x, int dx, int y, int dy,
                                       enum FilterMode filtering,
                                       uint32 src_fourcc,
                                       YUVColorSpace yuv_color_space) {
  int j;
  void (*InterpolateRow)(uint8* dst_argb, const uint8* src_argb,
      ptrdiff_t src_stride, int dst_width, int source_y_fraction) =
      InterpolateRow_C;
  void (*ScaleARGBFilterCols)(uint8* dst_argb, const uint8* src_argb,
      int dst_width, int x, int dx) =
      (src_width >= 32768) ? ScaleARGBFilterCols64_C : ScaleARGBFilterCols_C;
  int64 xlast = x + (int64)(dst_width - 1) * dx;
  int64 xl = (dx >= 0) ? x : xlast;
  int64 xr = (dx >= 0) ? xlast : x;
  int clip_src_width;
  xl = (xl >> 16) & ~3;  // Left edge aligned.
  xr = (xr >> 16) + 1;  // Right most pixel used.  Bilinear uses 2 pixels.
  xr = (xr + 1 + 3) & ~3;  // 1 beyond 4 pixel aligned right most pixel.
  if (xr > src_width) {
    xr = src_width;
  }
  clip_src_width = (int)(xr - xl) * 4;  // Width aligned to 4.
  const ptrdiff_t xl_offset = xl * 4;
  x -= (int)(xl << 16);

  // Allocate 2 row of ARGB for source conversion.
  const int kRowSize = (src_width * 4 + 15) & ~15;
  align_buffer_64(argb_cnv_row, kRowSize * 2);
  uint8* argb_cnv_rowptr = argb_cnv_row;
  int argb_cnv_rowstride = kRowSize;

#if defined(HAS_INTERPOLATEROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    InterpolateRow = InterpolateRow_Any_SSSE3;
    if (IS_ALIGNED(clip_src_width, 16)) {
      InterpolateRow = InterpolateRow_SSSE3;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    InterpolateRow = InterpolateRow_Any_AVX2;
    if (IS_ALIGNED(clip_src_width, 32)) {
      InterpolateRow = InterpolateRow_AVX2;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    InterpolateRow = InterpolateRow_Any_NEON;
    if (IS_ALIGNED(clip_src_width, 16)) {
      InterpolateRow = InterpolateRow_NEON;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_DSPR2)
  if (TestCpuFlag(kCpuHasDSPR2) &&
      IS_ALIGNED(src_argb, 4) && IS_ALIGNED(argb_cnv_rowstride, 4)) {
    InterpolateRow = InterpolateRow_Any_DSPR2;
    if (IS_ALIGNED(clip_src_width, 4)) {
      InterpolateRow = InterpolateRow_DSPR2;
    }
  }
#endif
#if defined(HAS_SCALEARGBFILTERCOLS_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && src_width < 32768) {
    ScaleARGBFilterCols = ScaleARGBFilterCols_SSSE3;
  }
#endif
#if defined(HAS_SCALEARGBFILTERCOLS_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ScaleARGBFilterCols = ScaleARGBFilterCols_Any_NEON;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBFilterCols = ScaleARGBFilterCols_NEON;
    }
  }
#endif

  int yi = y >> 16;

  YUVBuferIter iter;
  iter.src_width = src_width;
  iter.src_height = src_height;
  iter.src_stride_y = src_stride_y;
  iter.src_stride_u = src_stride_u;
  iter.src_stride_v = src_stride_v;
  iter.src_y = src_y;
  iter.src_u = src_u;
  iter.src_v = src_v;
  YUVBuferIter_Init(iter, src_fourcc, yuv_color_space);
  iter.MoveTo(iter, yi);

  // TODO(fbarchard): Consider not allocating row buffer for kFilterLinear.
  // Allocate a row of ARGB.
  align_buffer_64(row, clip_src_width * 4);

  int lastyi = yi;
  YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_rowptr);
  // Prepare next row if necessary
  if (filtering != kFilterLinear) {
    if ((yi + 1) < src_height) {
      iter.MoveToNextRow(iter);
      YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_rowptr + argb_cnv_rowstride);
    } else {
      argb_cnv_rowstride = 0;
    }
  }

  const int max_y = (src_height - 1) << 16;
  const int max_yi = src_height - 1;
  for (j = 0; j < dst_height; ++j) {
    yi = y >> 16;
    if (yi != lastyi) {
      if (y > max_y) {
        y = max_y;
        yi = y >> 16;
      }
      if (yi != lastyi) {
        if (filtering == kFilterLinear) {
          iter.MoveTo(iter, yi);
          YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_rowptr);
          lastyi = yi;
        } else {
          // Prepare current row
          if (yi == iter.y_index) {
            argb_cnv_rowptr = argb_cnv_rowptr + argb_cnv_rowstride;
            argb_cnv_rowstride = - argb_cnv_rowstride;
          } else {
            iter.MoveTo(iter, yi);
            argb_cnv_rowptr = argb_cnv_row;
            argb_cnv_rowstride = kRowSize;
            YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_rowptr);
          }
          // Prepare next row if necessary
          if (iter.y_index < max_yi) {
            iter.MoveToNextRow(iter);
            YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_rowptr + argb_cnv_rowstride);
          } else {
            argb_cnv_rowstride = 0;
          }
          lastyi = yi;
        }
      }
    }
    if (filtering == kFilterLinear) {
      ScaleARGBFilterCols(dst_argb, argb_cnv_rowptr + xl_offset, dst_width, x, dx);
    } else {
      int yf = (y >> 8) & 255;
      InterpolateRow(row, argb_cnv_rowptr + xl_offset, argb_cnv_rowstride, clip_src_width, yf);
      ScaleARGBFilterCols(dst_argb, row, dst_width, x, dx);
    }
    dst_argb += dst_stride_argb;
    y += dy;
  }
  free_aligned_buffer_64(row);
  free_aligned_buffer_64(argb_cnv_row);
}

// Scale YUV to ARGB up with bilinear interpolation.
static void ScaleYUVToARGBBilinearUp(int src_width, int src_height,
                                     int dst_width, int dst_height,
                                     int src_stride_y,
                                     int src_stride_u,
                                     int src_stride_v,
                                     int dst_stride_argb,
                                     const uint8* src_y,
                                     const uint8* src_u,
                                     const uint8* src_v,
                                     uint8* dst_argb,
                                     int x, int dx, int y, int dy,
                                     enum FilterMode filtering,
                                     uint32 src_fourcc,
                                     YUVColorSpace yuv_color_space) {
  int j;
  void (*InterpolateRow)(uint8* dst_argb, const uint8* src_argb,
      ptrdiff_t src_stride, int dst_width, int source_y_fraction) =
      InterpolateRow_C;
  void (*ScaleARGBFilterCols)(uint8* dst_argb, const uint8* src_argb,
      int dst_width, int x, int dx) =
      filtering ? ScaleARGBFilterCols_C : ScaleARGBCols_C;
  const int max_y = (src_height - 1) << 16;

  // Allocate 1 row of ARGB for source conversion.
  align_buffer_64(argb_cnv_row, src_width * 4);

#if defined(HAS_INTERPOLATEROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    InterpolateRow = InterpolateRow_Any_SSSE3;
    if (IS_ALIGNED(dst_width, 4)) {
      InterpolateRow = InterpolateRow_SSSE3;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    InterpolateRow = InterpolateRow_Any_AVX2;
    if (IS_ALIGNED(dst_width, 8)) {
      InterpolateRow = InterpolateRow_AVX2;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    InterpolateRow = InterpolateRow_Any_NEON;
    if (IS_ALIGNED(dst_width, 4)) {
      InterpolateRow = InterpolateRow_NEON;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_DSPR2)
  if (TestCpuFlag(kCpuHasDSPR2) &&
      IS_ALIGNED(dst_argb, 4) && IS_ALIGNED(dst_stride_argb, 4)) {
    InterpolateRow = InterpolateRow_DSPR2;
  }
#endif
  if (src_width >= 32768) {
    ScaleARGBFilterCols = filtering ?
        ScaleARGBFilterCols64_C : ScaleARGBCols64_C;
  }
#if defined(HAS_SCALEARGBFILTERCOLS_SSSE3)
  if (filtering && TestCpuFlag(kCpuHasSSSE3) && src_width < 32768) {
    ScaleARGBFilterCols = ScaleARGBFilterCols_SSSE3;
  }
#endif
#if defined(HAS_SCALEARGBFILTERCOLS_NEON)
  if (filtering && TestCpuFlag(kCpuHasNEON)) {
    ScaleARGBFilterCols = ScaleARGBFilterCols_Any_NEON;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBFilterCols = ScaleARGBFilterCols_NEON;
    }
  }
#endif
#if defined(HAS_SCALEARGBCOLS_SSE2)
  if (!filtering && TestCpuFlag(kCpuHasSSE2) && src_width < 32768) {
    ScaleARGBFilterCols = ScaleARGBCols_SSE2;
  }
#endif
#if defined(HAS_SCALEARGBCOLS_NEON)
  if (!filtering && TestCpuFlag(kCpuHasNEON)) {
    ScaleARGBFilterCols = ScaleARGBCols_Any_NEON;
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleARGBFilterCols = ScaleARGBCols_NEON;
    }
  }
#endif
  if (!filtering && src_width * 2 == dst_width && x < 0x8000) {
    ScaleARGBFilterCols = ScaleARGBColsUp2_C;
#if defined(HAS_SCALEARGBCOLSUP2_SSE2)
    if (TestCpuFlag(kCpuHasSSE2) && IS_ALIGNED(dst_width, 8)) {
      ScaleARGBFilterCols = ScaleARGBColsUp2_SSE2;
    }
#endif
  }

  if (y > max_y) {
    y = max_y;
  }

  int yi = y >> 16;

  YUVBuferIter iter;
  iter.src_width = src_width;
  iter.src_height = src_height;
  iter.src_stride_y = src_stride_y;
  iter.src_stride_u = src_stride_u;
  iter.src_stride_v = src_stride_v;
  iter.src_y = src_y;
  iter.src_u = src_u;
  iter.src_v = src_v;
  YUVBuferIter_Init(iter, src_fourcc, yuv_color_space);
  iter.MoveTo(iter, yi);

  // Allocate 2 rows of ARGB.
  const int kRowSize = (dst_width * 4 + 15) & ~15;
  align_buffer_64(row, kRowSize * 2);

  uint8* rowptr = row;
  int rowstride = kRowSize;
  int lastyi = yi;

  YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_row);
  ScaleARGBFilterCols(rowptr, argb_cnv_row, dst_width, x, dx);

  if (filtering == kFilterLinear) {
    rowstride = 0;
  }
  // Prepare next row if necessary
  if (filtering != kFilterLinear) {
    if ((yi + 1) < src_height) {
      iter.MoveToNextRow(iter);
      YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_row);
      ScaleARGBFilterCols(rowptr + rowstride, argb_cnv_row, dst_width, x, dx);
    }else {
      rowstride = 0;
    }
  }

  const int max_yi = src_height - 1;
  for (j = 0; j < dst_height; ++j) {
    yi = y >> 16;
    if (yi != lastyi) {
      if (y > max_y) {
        y = max_y;
        yi = y >> 16;
      }
      if (yi != lastyi) {
        if (filtering == kFilterLinear) {
            iter.MoveToNextRow(iter);
            YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_row);
            ScaleARGBFilterCols(rowptr, argb_cnv_row, dst_width, x, dx);
        } else {
          // Prepare next row if necessary
          if (yi < max_yi) {
            iter.MoveToNextRow(iter);
            rowptr += rowstride;
            rowstride = -rowstride;
            // TODO(fbarchard): Convert the clipped region of row.
            YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_row);
            ScaleARGBFilterCols(rowptr + rowstride, argb_cnv_row, dst_width, x, dx);
          } else {
            rowstride = 0;
          }
        }
        lastyi = yi;
      }
    }
    if (filtering == kFilterLinear) {
      InterpolateRow(dst_argb, rowptr, 0, dst_width * 4, 0);
    } else {
      int yf = (y >> 8) & 255;
      InterpolateRow(dst_argb, rowptr, rowstride, dst_width * 4, yf);
    }
    dst_argb += dst_stride_argb;
    y += dy;
  }
  free_aligned_buffer_64(row);
  free_aligned_buffer_64(argb_cnv_row);
}

// Scale ARGB to/from any dimensions, without interpolation.
// Fixed point math is used for performance: The upper 16 bits
// of x and dx is the integer part of the source position and
// the lower 16 bits are the fixed decimal part.

static void ScaleYUVToARGBSimple(int src_width, int src_height,
                                 int dst_width, int dst_height,
                                 int src_stride_y,
                                 int src_stride_u,
                                 int src_stride_v,
                                 int dst_stride_argb,
                                 const uint8* src_y,
                                 const uint8* src_u,
                                 const uint8* src_v,
                                 uint8* dst_argb,
                                 int x, int dx, int y, int dy,
                                 uint32 src_fourcc,
                                 YUVColorSpace yuv_color_space) {
  int j;
  void (*ScaleARGBCols)(uint8* dst_argb, const uint8* src_argb,
      int dst_width, int x, int dx) =
      (src_width >= 32768) ? ScaleARGBCols64_C : ScaleARGBCols_C;

  // Allocate 1 row of ARGB for source conversion.
  align_buffer_64(argb_cnv_row, src_width * 4);

#if defined(HAS_SCALEARGBCOLS_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) && src_width < 32768) {
    ScaleARGBCols = ScaleARGBCols_SSE2;
  }
#endif
#if defined(HAS_SCALEARGBCOLS_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ScaleARGBCols = ScaleARGBCols_Any_NEON;
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleARGBCols = ScaleARGBCols_NEON;
    }
  }
#endif
  if (src_width * 2 == dst_width && x < 0x8000) {
    ScaleARGBCols = ScaleARGBColsUp2_C;
#if defined(HAS_SCALEARGBCOLSUP2_SSE2)
    if (TestCpuFlag(kCpuHasSSE2) && IS_ALIGNED(dst_width, 8)) {
      ScaleARGBCols = ScaleARGBColsUp2_SSE2;
    }
#endif
  }

  int yi = y >> 16;

  YUVBuferIter iter;
  iter.src_width = src_width;
  iter.src_height = src_height;
  iter.src_stride_y = src_stride_y;
  iter.src_stride_u = src_stride_u;
  iter.src_stride_v = src_stride_v;
  iter.src_y = src_y;
  iter.src_u = src_u;
  iter.src_v = src_v;
  YUVBuferIter_Init(iter, src_fourcc, yuv_color_space);
  iter.MoveTo(iter, yi);

  int lasty = yi;
  YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_row);

  for (j = 0; j < dst_height; ++j) {
    yi = y >> 16;
    if (yi != lasty) {
      iter.MoveTo(iter, yi);
      YUVBuferIter_ConvertToARGBRow(iter, argb_cnv_row);
      lasty = yi;
    }
    ScaleARGBCols(dst_argb, argb_cnv_row, dst_width, x, dx);
    dst_argb += dst_stride_argb;
    y += dy;
  }
  free_aligned_buffer_64(argb_cnv_row);
}

static void YUVToARGBCopy(const uint8* src_y, int src_stride_y,
                          const uint8* src_u, int src_stride_u,
                          const uint8* src_v, int src_stride_v,
                          int src_width, int src_height,
                          uint8* dst_argb, int dst_stride_argb,
                          int dst_width, int dst_height,
                          uint32 src_fourcc,
                          YUVColorSpace yuv_color_space)
{
  YUVBuferIter iter;
  iter.src_width = src_width;
  iter.src_height = src_height;
  iter.src_stride_y = src_stride_y;
  iter.src_stride_u = src_stride_u;
  iter.src_stride_v = src_stride_v;
  iter.src_y = src_y;
  iter.src_u = src_u;
  iter.src_v = src_v;
  YUVBuferIter_Init(iter, src_fourcc, yuv_color_space);

  for (int j = 0; j < dst_height; ++j) {
    YUVBuferIter_ConvertToARGBRow(iter, dst_argb);
    iter.MoveToNextRow(iter);
    dst_argb += dst_stride_argb;
  }
}

static void ScaleYUVToARGB(const uint8* src_y, int src_stride_y,
                           const uint8* src_u, int src_stride_u,
                           const uint8* src_v, int src_stride_v,
                           int src_width, int src_height,
                           uint8* dst_argb, int dst_stride_argb,
                           int dst_width, int dst_height,
                           enum FilterMode filtering,
                           uint32 src_fourcc,
                           YUVColorSpace yuv_color_space)
{
  // Initial source x/y coordinate and step values as 16.16 fixed point.
  int x = 0;
  int y = 0;
  int dx = 0;
  int dy = 0;
  // ARGB does not support box filter yet, but allow the user to pass it.
  // Simplify filtering when possible.
  filtering = ScaleFilterReduce(src_width, src_height,
                                dst_width, dst_height,
                                filtering);
  ScaleSlope(src_width, src_height, dst_width, dst_height, filtering,
             &x, &y, &dx, &dy);

  // Special case for integer step values.
  if (((dx | dy) & 0xffff) == 0) {
    if (!dx || !dy) {  // 1 pixel wide and/or tall.
      filtering = kFilterNone;
    } else {
      // Optimized even scale down. ie 2, 4, 6, 8, 10x.
      if (!(dx & 0x10000) && !(dy & 0x10000)) {
        if (dx == 0x20000) {
          // Optimized 1/2 downsample.
          ScaleYUVToARGBDown2(src_width, src_height,
                              dst_width, dst_height,
                              src_stride_y,
                              src_stride_u,
                              src_stride_v,
                              dst_stride_argb,
                              src_y,
                              src_u,
                              src_v,
                              dst_argb,
                              x, dx, y, dy,
                              filtering,
                              src_fourcc,
                              yuv_color_space);
          return;
        }
        ScaleYUVToARGBDownEven(src_width, src_height,
                               dst_width, dst_height,
                               src_stride_y,
                               src_stride_u,
                               src_stride_v,
                               dst_stride_argb,
                               src_y,
                               src_u,
                               src_v,
                               dst_argb,
                               x, dx, y, dy,
                               filtering,
                               src_fourcc,
                               yuv_color_space);
        return;
      }
      // Optimized odd scale down. ie 3, 5, 7, 9x.
      if ((dx & 0x10000) && (dy & 0x10000)) {
        filtering = kFilterNone;
        if (dx == 0x10000 && dy == 0x10000) {
          // Straight conversion and copy.
          YUVToARGBCopy(src_y, src_stride_y,
                        src_u, src_stride_u,
                        src_v, src_stride_v,
                        src_width, src_height,
                        dst_argb, dst_stride_argb,
                        dst_width, dst_height,
                        src_fourcc,
                        yuv_color_space);
          return;
        }
      }
    }
  }
  if (filtering && dy < 65536) {
    ScaleYUVToARGBBilinearUp(src_width, src_height,
                             dst_width, dst_height,
                             src_stride_y,
                             src_stride_u,
                             src_stride_v,
                             dst_stride_argb,
                             src_y,
                             src_u,
                             src_v,
                             dst_argb,
                             x, dx, y, dy,
                             filtering,
                             src_fourcc,
                             yuv_color_space);
    return;
  }
  if (filtering) {
    ScaleYUVToARGBBilinearDown(src_width, src_height,
                               dst_width, dst_height,
                               src_stride_y,
                               src_stride_u,
                               src_stride_v,
                               dst_stride_argb,
                               src_y,
                               src_u,
                               src_v,
                               dst_argb,
                               x, dx, y, dy,
                               filtering,
                               src_fourcc,
                               yuv_color_space);
    return;
  }
  ScaleYUVToARGBSimple(src_width, src_height,
                       dst_width, dst_height,
                       src_stride_y,
                       src_stride_u,
                       src_stride_v,
                       dst_stride_argb,
                       src_y,
                       src_u,
                       src_v,
                       dst_argb,
                       x, dx, y, dy,
                       src_fourcc,
                       yuv_color_space);
}

bool IsConvertSupported(uint32 src_fourcc)
{
  if (src_fourcc == FOURCC_I444 ||
      src_fourcc == FOURCC_I422 ||
      src_fourcc == FOURCC_I420) {
    return true;
  }
  return false;
}

LIBYUV_API
int YUVToARGBScale(const uint8* src_y, int src_stride_y,
                   const uint8* src_u, int src_stride_u,
                   const uint8* src_v, int src_stride_v,
                   uint32 src_fourcc,
                   YUVColorSpace yuv_color_space,
                   int src_width, int src_height,
                   uint8* dst_argb, int dst_stride_argb,
                   int dst_width, int dst_height,
                   enum FilterMode filtering)
{
  if (!src_y || !src_u || !src_v ||
      src_width == 0 || src_height == 0 ||
      !dst_argb || dst_width <= 0 || dst_height <= 0) {
    return -1;
  }
  if (!IsConvertSupported(src_fourcc)) {
    return -1;
  }
  ScaleYUVToARGB(src_y, src_stride_y,
                 src_u, src_stride_u,
                 src_v, src_stride_v,
                 src_width, src_height,
                 dst_argb, dst_stride_argb,
                 dst_width, dst_height,
                 filtering,
                 src_fourcc,
                 yuv_color_space);
  return 0;
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
