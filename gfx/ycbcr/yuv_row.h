// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// yuv_row internal functions to handle YUV conversion and scaling to RGB.
// These functions are used from both yuv_convert.cc and yuv_scale.cc.

// TODO(fbarchard): Write function that can handle rotation and scaling.

#ifndef MEDIA_BASE_YUV_ROW_H_
#define MEDIA_BASE_YUV_ROW_H_

#include "chromium_types.h"

extern "C" {
// Can only do 1x.
// This is the second fastest of the scalers.
void FastConvertYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width);

void FastConvertYUVToRGB32Row_C(const uint8* y_buf,
                                const uint8* u_buf,
                                const uint8* v_buf,
                                uint8* rgb_buf,
                                int width,
                                unsigned int x_shift);


// Can do 1x, half size or any scale down by an integer amount.
// Step can be negative (mirroring, rotate 180).
// This is the third fastest of the scalers.
void ConvertYUVToRGB32Row(const uint8* y_buf,
                          const uint8* u_buf,
                          const uint8* v_buf,
                          uint8* rgb_buf,
                          int width,
                          int step);

// Rotate is like Convert, but applies different step to Y versus U and V.
// This allows rotation by 90 or 270, by stepping by stride.
// This is the forth fastest of the scalers.
void RotateConvertYUVToRGB32Row(const uint8* y_buf,
                                const uint8* u_buf,
                                const uint8* v_buf,
                                uint8* rgb_buf,
                                int width,
                                int ystep,
                                int uvstep);

// Doubler does 4 pixels at a time.  Each pixel is replicated.
// This is the fastest of the scalers.
void DoubleYUVToRGB32Row(const uint8* y_buf,
                         const uint8* u_buf,
                         const uint8* v_buf,
                         uint8* rgb_buf,
                         int width);

// Handles arbitrary scaling up or down.
// Mirroring is supported, but not 90 or 270 degree rotation.
// Chroma is under sampled every 2 pixels for performance.
// This is the slowest of the scalers.
void ScaleYUVToRGB32Row(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width,
                        int scaled_dx);

void ScaleYUVToRGB32Row_C(const uint8* y_buf,
                          const uint8* u_buf,
                          const uint8* v_buf,
                          uint8* rgb_buf,
                          int width,
                          int scaled_dx,
                          unsigned int x_shift);

}  // extern "C"

// x64 uses MMX2 (SSE) so emms is not required.
#if defined(ARCH_CPU_X86)
#if defined(_MSC_VER)
#define EMMS() __asm emms
#else
#define EMMS() asm("emms")
#endif
#else
#define EMMS()
#endif

#endif  // MEDIA_BASE_YUV_ROW_H_
