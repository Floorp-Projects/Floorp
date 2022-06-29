/*
 *  Copyright 2012 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef INCLUDE_LIBYUV_SCALE_YUV_ARGB_H_  // NOLINT
#define INCLUDE_LIBYUV_SCALE_YUV_ARGB_H_

#include "libyuv/basic_types.h"
#include "libyuv/scale.h"  // For FilterMode

#include "mozilla/gfx/Types.h" // For YUVColorSpace

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

int YUVToARGBScale(const uint8_t* src_y, int src_stride_y,
                   const uint8_t* src_u, int src_stride_u,
                   const uint8_t* src_v, int src_stride_v,
                   uint32_t src_fourcc,
                   mozilla::gfx::YUVColorSpace yuv_color_space,
                   int src_width, int src_height,
                   uint8_t* dst_argb, int dst_stride_argb,
                   int dst_width, int dst_height,
                   enum FilterMode filtering);

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // INCLUDE_LIBYUV_SCALE_YUV_ARGB_H_  NOLINT
