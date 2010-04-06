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
                                int width);


}  // extern "C"

// x64 uses MMX2 (SSE) so emms is not required.
#if !defined(ARCH_CPU_X86_64) && !defined(ARCH_CPU_PPC)
#if defined(_MSC_VER)
#define EMMS() __asm emms
#else
#define EMMS() asm("emms")
#endif
#else
#define EMMS()
#endif

#endif  // MEDIA_BASE_YUV_ROW_H_
