// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <mmintrin.h>
#include "yuv_row.h"

namespace mozilla {
namespace gfx {

// FilterRows combines two rows of the image using linear interpolation.
// MMX version does 8 pixels at a time.
void FilterRows_MMX(uint8* ybuf, const uint8* y0_ptr, const uint8* y1_ptr,
                    int source_width, int source_y_fraction) {
  __m64 zero = _mm_setzero_si64();
  __m64 y1_fraction = _mm_set1_pi16(source_y_fraction);
  __m64 y0_fraction = _mm_set1_pi16(256 - source_y_fraction);

  const __m64* y0_ptr64 = reinterpret_cast<const __m64*>(y0_ptr);
  const __m64* y1_ptr64 = reinterpret_cast<const __m64*>(y1_ptr);
  __m64* dest64 = reinterpret_cast<__m64*>(ybuf);
  __m64* end64 = reinterpret_cast<__m64*>(ybuf + source_width);

  do {
    __m64 y0 = *y0_ptr64++;
    __m64 y1 = *y1_ptr64++;
    __m64 y2 = _mm_unpackhi_pi8(y0, zero);
    __m64 y3 = _mm_unpackhi_pi8(y1, zero);
    y0 = _mm_unpacklo_pi8(y0, zero);
    y1 = _mm_unpacklo_pi8(y1, zero);
    y0 = _mm_mullo_pi16(y0, y0_fraction);
    y1 = _mm_mullo_pi16(y1, y1_fraction);
    y2 = _mm_mullo_pi16(y2, y0_fraction);
    y3 = _mm_mullo_pi16(y3, y1_fraction);
    y0 = _mm_add_pi16(y0, y1);
    y2 = _mm_add_pi16(y2, y3);
    y0 = _mm_srli_pi16(y0, 8);
    y2 = _mm_srli_pi16(y2, 8);
    y0 = _mm_packs_pu16(y0, y2);
    *dest64++ = y0;
  } while (dest64 < end64);
}

} // namespace gfx
} // namespace mozilla
