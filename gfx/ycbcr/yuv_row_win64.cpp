// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "yuv_row.h"

extern "C" {

// x64 compiler doesn't support MMX and inline assembler.  Use SSE2 intrinsics.

#define kCoefficientsRgbU (reinterpret_cast<const uint8*>(kCoefficientsRgbY) + 2048)
#define kCoefficientsRgbV (reinterpret_cast<const uint8*>(kCoefficientsRgbY) + 4096)

#include <emmintrin.h>

static void FastConvertYUVToRGB32Row_SSE2(const uint8* y_buf,
                                          const uint8* u_buf,
                                          const uint8* v_buf,
                                          uint8* rgb_buf,
                                          int width) {
  __m128i xmm0, xmmY1, xmmY2;
  __m128  xmmY;

  while (width >= 2) {
    xmm0 = _mm_adds_epi16(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(kCoefficientsRgbU + 8 * *u_buf++)),
                          _mm_loadl_epi64(reinterpret_cast<const __m128i*>(kCoefficientsRgbV + 8 * *v_buf++)));

    xmmY1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(reinterpret_cast<const uint8*>(kCoefficientsRgbY) + 8 * *y_buf++));
    xmmY1 = _mm_adds_epi16(xmmY1, xmm0);

    xmmY2 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(reinterpret_cast<const uint8*>(kCoefficientsRgbY) + 8 * *y_buf++));
    xmmY2 = _mm_adds_epi16(xmmY2, xmm0);

    xmmY = _mm_shuffle_ps(_mm_castsi128_ps(xmmY1), _mm_castsi128_ps(xmmY2),
                          0x44);
    xmmY1 = _mm_srai_epi16(_mm_castps_si128(xmmY), 6);
    xmmY1 = _mm_packus_epi16(xmmY1, xmmY1);

    _mm_storel_epi64(reinterpret_cast<__m128i*>(rgb_buf), xmmY1);
    rgb_buf += 8;
    width -= 2;
  }

  if (width) {
    xmm0 = _mm_adds_epi16(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(kCoefficientsRgbU + 8 * *u_buf)),
                          _mm_loadl_epi64(reinterpret_cast<const __m128i*>(kCoefficientsRgbV + 8 * *v_buf)));
    xmmY1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(reinterpret_cast<const uint8*>(kCoefficientsRgbY) + 8 * *y_buf));
    xmmY1 = _mm_adds_epi16(xmmY1, xmm0);
    xmmY1 = _mm_srai_epi16(xmmY1, 6);
    xmmY1 = _mm_packus_epi16(xmmY1, xmmY1);
    *reinterpret_cast<uint32*>(rgb_buf) = _mm_cvtsi128_si32(xmmY1);
  }
}

static void ScaleYUVToRGB32Row_SSE2(const uint8* y_buf,
                                    const uint8* u_buf,
                                    const uint8* v_buf,
                                    uint8* rgb_buf,
                                    int width,
                                    int source_dx) {
  __m128i xmm0, xmmY1, xmmY2;
  __m128  xmmY;
  uint8 u, v, y;
  int x = 0;

  while (width >= 2) {
    u = u_buf[x >> 17];
    v = v_buf[x >> 17];
    y = y_buf[x >> 16];
    x += source_dx;

    xmm0 = _mm_adds_epi16(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(kCoefficientsRgbU + 8 * u)),
                          _mm_loadl_epi64(reinterpret_cast<const __m128i*>(kCoefficientsRgbV + 8 * v)));
    xmmY1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(reinterpret_cast<const uint8*>(kCoefficientsRgbY) + 8 * y));
    xmmY1 = _mm_adds_epi16(xmmY1, xmm0);

    y = y_buf[x >> 16];
    x += source_dx;

    xmmY2 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(reinterpret_cast<const uint8*>(kCoefficientsRgbY) + 8 * y));
    xmmY2 = _mm_adds_epi16(xmmY2, xmm0);

    xmmY = _mm_shuffle_ps(_mm_castsi128_ps(xmmY1), _mm_castsi128_ps(xmmY2),
                          0x44);
    xmmY1 = _mm_srai_epi16(_mm_castps_si128(xmmY), 6);
    xmmY1 = _mm_packus_epi16(xmmY1, xmmY1);

    _mm_storel_epi64(reinterpret_cast<__m128i*>(rgb_buf), xmmY1);
    rgb_buf += 8;
    width -= 2;
  }

  if (width) {
    u = u_buf[x >> 17];
    v = v_buf[x >> 17];
    y = y_buf[x >> 16];

    xmm0 = _mm_adds_epi16(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(kCoefficientsRgbU + 8 * u)),
                          _mm_loadl_epi64(reinterpret_cast<const __m128i*>(kCoefficientsRgbV + 8 * v)));
    xmmY1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(reinterpret_cast<const uint8*>(kCoefficientsRgbY) + 8 * y));
    xmmY1 = _mm_adds_epi16(xmmY1, xmm0);
    xmmY1 = _mm_srai_epi16(xmmY1, 6);
    xmmY1 = _mm_packus_epi16(xmmY1, xmmY1);
    *reinterpret_cast<uint32*>(rgb_buf) = _mm_cvtsi128_si32(xmmY1);
  }
}

static void LinearScaleYUVToRGB32Row_SSE2(const uint8* y_buf,
                                          const uint8* u_buf,
                                          const uint8* v_buf,
                                          uint8* rgb_buf,
                                          int width,
                                          int source_dx) {
  __m128i xmm0, xmmY1, xmmY2;
  __m128  xmmY;
  uint8 u0, u1, v0, v1, y0, y1;
  uint32 uv_frac, y_frac, u, v, y;
  int x = 0;

  if (source_dx >= 0x20000) {
    x = 32768;
  }

  while(width >= 2) {
    u0 = u_buf[x >> 17];
    u1 = u_buf[(x >> 17) + 1];
    v0 = v_buf[x >> 17];
    v1 = v_buf[(x >> 17) + 1];
    y0 = y_buf[x >> 16];
    y1 = y_buf[(x >> 16) + 1];
    uv_frac = (x & 0x1fffe);
    y_frac = (x & 0xffff);
    u = (uv_frac * u1 + (uv_frac ^ 0x1fffe) * u0) >> 17;
    v = (uv_frac * v1 + (uv_frac ^ 0x1fffe) * v0) >> 17;
    y = (y_frac * y1 + (y_frac ^ 0xffff) * y0) >> 16;
    x += source_dx;

    xmm0 = _mm_adds_epi16(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(kCoefficientsRgbU + 8 * u)),
                          _mm_loadl_epi64(reinterpret_cast<const __m128i*>(kCoefficientsRgbV + 8 * v)));
    xmmY1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(reinterpret_cast<const uint8*>(kCoefficientsRgbY) + 8 * y));
    xmmY1 = _mm_adds_epi16(xmmY1, xmm0);

    y0 = y_buf[x >> 16];
    y1 = y_buf[(x >> 16) + 1];
    y_frac = (x & 0xffff);
    y = (y_frac * y1 + (y_frac ^ 0xffff) * y0) >> 16;
    x += source_dx;

    xmmY2 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(reinterpret_cast<const uint8*>(kCoefficientsRgbY) + 8 * y));
    xmmY2 = _mm_adds_epi16(xmmY2, xmm0);

    xmmY = _mm_shuffle_ps(_mm_castsi128_ps(xmmY1), _mm_castsi128_ps(xmmY2),
                          0x44);
    xmmY1 = _mm_srai_epi16(_mm_castps_si128(xmmY), 6);
    xmmY1 = _mm_packus_epi16(xmmY1, xmmY1);

    _mm_storel_epi64(reinterpret_cast<__m128i*>(rgb_buf), xmmY1);
    rgb_buf += 8;
    width -= 2;
  }

  if (width) {
    u = u_buf[x >> 17];
    v = v_buf[x >> 17];
    y = y_buf[x >> 16];

    xmm0 = _mm_adds_epi16(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(kCoefficientsRgbU + 8 * u)),
                          _mm_loadl_epi64(reinterpret_cast<const __m128i*>(kCoefficientsRgbV + 8 * v)));
    xmmY1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(reinterpret_cast<const uint8*>(kCoefficientsRgbY) + 8 * y));

    xmmY1 = _mm_adds_epi16(xmmY1, xmm0);
    xmmY1 = _mm_srai_epi16(xmmY1, 6);
    xmmY1 = _mm_packus_epi16(xmmY1, xmmY1);
    *reinterpret_cast<uint32*>(rgb_buf) = _mm_cvtsi128_si32(xmmY1);
  }
}

void FastConvertYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width) {
  FastConvertYUVToRGB32Row_SSE2(y_buf, u_buf, v_buf, rgb_buf, width);
}

void ScaleYUVToRGB32Row(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width,
                        int source_dx) {
  ScaleYUVToRGB32Row_SSE2(y_buf, u_buf, v_buf, rgb_buf, width, source_dx);
}

void LinearScaleYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width,
                              int source_dx) {
  LinearScaleYUVToRGB32Row_SSE2(y_buf, u_buf, v_buf, rgb_buf, width,
                                source_dx);
}

} // extern "C"
