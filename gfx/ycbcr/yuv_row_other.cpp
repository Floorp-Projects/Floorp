// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "yuv_row.h"

extern "C" {
void FastConvertYUVToRGB32Row(const uint8_t* y_buf,
                              const uint8_t* u_buf,
                              const uint8_t* v_buf,
                              uint8_t* rgb_buf,
                              int width) {
  FastConvertYUVToRGB32Row_C(y_buf, u_buf, v_buf, rgb_buf, width, 1);
}

void ScaleYUVToRGB32Row(const uint8_t* y_buf,
                        const uint8_t* u_buf,
                        const uint8_t* v_buf,
                        uint8_t* rgb_buf,
                        int width,
                        int source_dx) {
  ScaleYUVToRGB32Row_C(y_buf, u_buf, v_buf, rgb_buf, width, source_dx);
}

void LinearScaleYUVToRGB32Row(const uint8_t* y_buf,
                              const uint8_t* u_buf,
                              const uint8_t* v_buf,
                              uint8_t* rgb_buf,
                              int width,
                              int source_dx) {
  LinearScaleYUVToRGB32Row_C(y_buf, u_buf, v_buf, rgb_buf, width, source_dx);
}

} 
