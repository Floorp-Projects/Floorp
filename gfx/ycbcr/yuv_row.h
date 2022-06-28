// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
void FastConvertYUVToRGB32Row(const uint8_t* y_buf,
                              const uint8_t* u_buf,
                              const uint8_t* v_buf,
                              uint8_t* rgb_buf,
                              int width);

void FastConvertYUVToRGB32Row_C(const uint8_t* y_buf,
                                const uint8_t* u_buf,
                                const uint8_t* v_buf,
                                uint8_t* rgb_buf,
                                int width,
                                unsigned int x_shift);

void FastConvertYUVToRGB32Row(const uint8_t* y_buf,
                              const uint8_t* u_buf,
                              const uint8_t* v_buf,
                              uint8_t* rgb_buf,
                              int width);

// Can do 1x, half size or any scale down by an integer amount.
// Step can be negative (mirroring, rotate 180).
// This is the third fastest of the scalers.
// Only defined on Windows x86-32.
void ConvertYUVToRGB32Row_SSE(const uint8_t* y_buf,
                              const uint8_t* u_buf,
                              const uint8_t* v_buf,
                              uint8_t* rgb_buf,
                              int width,
                              int step);

// Rotate is like Convert, but applies different step to Y versus U and V.
// This allows rotation by 90 or 270, by stepping by stride.
// This is the forth fastest of the scalers.
// Only defined on Windows x86-32.
void RotateConvertYUVToRGB32Row_SSE(const uint8_t* y_buf,
                                    const uint8_t* u_buf,
                                    const uint8_t* v_buf,
                                    uint8_t* rgb_buf,
                                    int width,
                                    int ystep,
                                    int uvstep);

// Doubler does 4 pixels at a time.  Each pixel is replicated.
// This is the fastest of the scalers.
// Only defined on Windows x86-32.
void DoubleYUVToRGB32Row_SSE(const uint8_t* y_buf,
                             const uint8_t* u_buf,
                             const uint8_t* v_buf,
                             uint8_t* rgb_buf,
                             int width);

// Handles arbitrary scaling up or down.
// Mirroring is supported, but not 90 or 270 degree rotation.
// Chroma is under sampled every 2 pixels for performance.
void ScaleYUVToRGB32Row(const uint8_t* y_buf,
                        const uint8_t* u_buf,
                        const uint8_t* v_buf,
                        uint8_t* rgb_buf,
                        int width,
                        int source_dx);

void ScaleYUVToRGB32Row(const uint8_t* y_buf,
                        const uint8_t* u_buf,
                        const uint8_t* v_buf,
                        uint8_t* rgb_buf,
                        int width,
                        int source_dx);

void ScaleYUVToRGB32Row_C(const uint8_t* y_buf,
                          const uint8_t* u_buf,
                          const uint8_t* v_buf,
                          uint8_t* rgb_buf,
                          int width,
                          int source_dx);

// Handles arbitrary scaling up or down with bilinear filtering.
// Mirroring is supported, but not 90 or 270 degree rotation.
// Chroma is under sampled every 2 pixels for performance.
// This is the slowest of the scalers.
void LinearScaleYUVToRGB32Row(const uint8_t* y_buf,
                              const uint8_t* u_buf,
                              const uint8_t* v_buf,
                              uint8_t* rgb_buf,
                              int width,
                              int source_dx);

void LinearScaleYUVToRGB32Row(const uint8_t* y_buf,
                              const uint8_t* u_buf,
                              const uint8_t* v_buf,
                              uint8_t* rgb_buf,
                              int width,
                              int source_dx);

void LinearScaleYUVToRGB32Row_C(const uint8_t* y_buf,
                                const uint8_t* u_buf,
                                const uint8_t* v_buf,
                                uint8_t* rgb_buf,
                                int width,
                                int source_dx);


#if defined(_MSC_VER) && !defined(__CLR_VER) && !defined(__clang__)
#if defined(VISUALC_HAS_AVX2)
#define SIMD_ALIGNED(var) __declspec(align(32)) var
#else
#define SIMD_ALIGNED(var) __declspec(align(16)) var
#endif
#elif defined(__GNUC__) || defined(__clang__)
// Caveat GCC 4.2 to 4.7 have a known issue using vectors with const.
#if defined(CLANG_HAS_AVX2) || defined(GCC_HAS_AVX2)
#define SIMD_ALIGNED(var) var __attribute__((aligned(32)))
#else
#define SIMD_ALIGNED(var) var __attribute__((aligned(16)))
#endif
#else
#define SIMD_ALIGNED(var) var
#endif

extern SIMD_ALIGNED(const int16_t kCoefficientsRgbY[768][4]);

// x64 uses MMX2 (SSE) so emms is not required.
// Warning C4799: function has no EMMS instruction.
// EMMS() is slow and should be called by the calling function once per image.
#if defined(ARCH_CPU_X86) && !defined(ARCH_CPU_X86_64)
#if defined(_MSC_VER)
#define EMMS() __asm emms
#pragma warning(disable: 4799)
#else
#define EMMS() asm("emms")
#endif
#else
#define EMMS() ((void)0)
#endif

}  // extern "C"

#endif  // MEDIA_BASE_YUV_ROW_H_
