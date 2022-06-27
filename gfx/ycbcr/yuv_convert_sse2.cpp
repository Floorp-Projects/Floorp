// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <emmintrin.h>
#include "yuv_row.h"

namespace mozilla {
namespace gfx {

// FilterRows combines two rows of the image using linear interpolation.
// SSE2 version does 16 pixels at a time.
void FilterRows_SSE2(uint8* ybuf, const uint8* y0_ptr, const uint8* y1_ptr,
                     int source_width, int source_y_fraction) {
  __m128i zero = _mm_setzero_si128();
  __m128i y1_fraction = _mm_set1_epi16(source_y_fraction);
  __m128i y0_fraction = _mm_set1_epi16(256 - source_y_fraction);

  const __m128i* y0_ptr128 = reinterpret_cast<const __m128i*>(y0_ptr);
  const __m128i* y1_ptr128 = reinterpret_cast<const __m128i*>(y1_ptr);
  __m128i* dest128 = reinterpret_cast<__m128i*>(ybuf);
  __m128i* end128 = reinterpret_cast<__m128i*>(ybuf + source_width);

  do {
    __m128i y0 = _mm_loadu_si128(y0_ptr128);
    __m128i y1 = _mm_loadu_si128(y1_ptr128);
    __m128i y2 = _mm_unpackhi_epi8(y0, zero);
    __m128i y3 = _mm_unpackhi_epi8(y1, zero);
    y0 = _mm_unpacklo_epi8(y0, zero);
    y1 = _mm_unpacklo_epi8(y1, zero);
    y0 = _mm_mullo_epi16(y0, y0_fraction);
    y1 = _mm_mullo_epi16(y1, y1_fraction);
    y2 = _mm_mullo_epi16(y2, y0_fraction);
    y3 = _mm_mullo_epi16(y3, y1_fraction);
    y0 = _mm_add_epi16(y0, y1);
    y2 = _mm_add_epi16(y2, y3);
    y0 = _mm_srli_epi16(y0, 8);
    y2 = _mm_srli_epi16(y2, 8);
    y0 = _mm_packus_epi16(y0, y2);
    *dest128++ = y0;
    ++y0_ptr128;
    ++y1_ptr128;
  } while (dest128 < end128);
}

} // namespace gfx
} // namespace mozilla
