/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Swizzle.h"

#include <emmintrin.h>
#include <tmmintrin.h>

namespace mozilla {
namespace gfx {

template <bool aSwapRB>
void UnpackRowRGB24(const uint8_t* aSrc, uint8_t* aDst, int32_t aLength);

template <bool aSwapRB>
void UnpackRowRGB24_SSSE3(const uint8_t* aSrc, uint8_t* aDst, int32_t aLength) {
  // Because this implementation will read an additional 4 bytes of data that
  // is ignored and masked over, we cannot use the accelerated version for the
  // last 1-5 pixels (3-15 bytes remaining) to guarantee we don't access memory
  // outside the buffer (we read in 16 byte chunks).
  if (aLength < 6) {
    UnpackRowRGB24<aSwapRB>(aSrc, aDst, aLength);
    return;
  }

  // Because we are expanding, we can only process the data back to front in
  // case we are performing this in place.
  int32_t alignedRow = (aLength - 2) & ~3;
  int32_t remainder = aLength - alignedRow;

  const uint8_t* src = aSrc + alignedRow * 3;
  uint8_t* dst = aDst + alignedRow * 4;

  // Handle 2-5 remaining pixels.
  UnpackRowRGB24<aSwapRB>(src, dst, remainder);

  __m128i mask;
  if (aSwapRB) {
    mask = _mm_set_epi8(15, 9, 10, 11, 14, 6, 7, 8, 13, 3, 4, 5, 12, 0, 1, 2);
  } else {
    mask = _mm_set_epi8(15, 11, 10, 9, 14, 8, 7, 6, 13, 5, 4, 3, 12, 2, 1, 0);
  }

  __m128i alpha = _mm_set1_epi32(0xFF000000);

  // Process all 4-pixel chunks as one vector.
  src -= 4 * 3;
  dst -= 4 * 4;
  while (src >= aSrc) {
    __m128i px = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
    px = _mm_shuffle_epi8(px, mask);
    px = _mm_or_si128(px, alpha);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), px);
    src -= 4 * 3;
    dst -= 4 * 4;
  }
}

// Force instantiation of swizzle variants here.
template void UnpackRowRGB24_SSSE3<false>(const uint8_t*, uint8_t*, int32_t);
template void UnpackRowRGB24_SSSE3<true>(const uint8_t*, uint8_t*, int32_t);

}  // namespace gfx
}  // namespace mozilla
