/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Swizzle.h"

#include <immintrin.h>
#include <tmmintrin.h>

namespace mozilla {
namespace gfx {

template <bool aSwapRB>
void UnpackRowRGB24_SSSE3(const uint8_t* aSrc, uint8_t* aDst, int32_t aLength);

template <bool aSwapRB>
void UnpackRowRGB24_AVX2(const uint8_t* aSrc, uint8_t* aDst, int32_t aLength) {
  // Because this implementation will read an additional 8 bytes of data that
  // is ignored and masked over, we cannot use the accelerated version for the
  // last 1-10 pixels (3-30 bytes remaining) to guarantee we don't access memory
  // outside the buffer (we read in 32 byte chunks).
  if (aLength < 11) {
    UnpackRowRGB24_SSSE3<aSwapRB>(aSrc, aDst, aLength);
    return;
  }

  // Because we are expanding, we can only process the data back to front in
  // case we are performing this in place.
  int32_t alignedRow = (aLength - 4) & ~7;
  int32_t remainder = aLength - alignedRow;

  const uint8_t* src = aSrc + alignedRow * 3;
  uint8_t* dst = aDst + alignedRow * 4;

  // Handle any 3-10 remaining pixels.
  UnpackRowRGB24_SSSE3<aSwapRB>(src, dst, remainder);

  // Used to shuffle the two final 32-bit words which we ignore into the last
  // 32-bit word of each 128-bit lane, such that
  //   RGBR GBRG BRGB RGBR GBRG BRGB RGBR GBRG
  //   BRGB RGBR GBRG BRGB ZZZZ ZZZZ ZZZZ ZZZZ
  // becomes
  //   RGBR GBRG BRGB RGBR GBRG BRGB ZZZZ ZZZZ
  //   RGBR GBRG BRGB RGBR GBRG BRGB ZZZZ ZZZZ
  const __m256i discardMask = _mm256_set_epi32(7, 5, 4, 3, 6, 2, 1, 0);

  // Used to shuffle 8-bit words within a 128-bit lane, such that we transform
  //   RGBR GBRG BRGB RGBR GBRG BRGB ZZZZ ZZZZ
  // into
  //   RGBZ RGBZ RGBZ RGBZ RGBZ RGBZ RGBZ RGBZ
  // or
  //   BGRZ BGRZ BGRZ BGRZ BGRZ BGRZ BGRZ BGRZ
  const __m256i colorMask =
      aSwapRB ? _mm256_set_epi8(15, 9, 10, 11, 14, 6, 7, 8, 13, 3, 4, 5, 12, 0,
                                1, 2, 15, 9, 10, 11, 14, 6, 7, 8, 13, 3, 4, 5,
                                12, 0, 1, 2)
              : _mm256_set_epi8(15, 11, 10, 9, 14, 8, 7, 6, 13, 5, 4, 3, 12, 2,
                                1, 0, 15, 11, 10, 9, 14, 8, 7, 6, 13, 5, 4, 3,
                                12, 2, 1, 0);

  // Used to transform RGBZ/BGRZ to RGBX/BGRX, or force the alpha opaque.
  const __m256i alphaMask = _mm256_set1_epi32(0xFF000000);

  // Process all 8-pixel chunks as one vector.
  src -= 8 * 3;
  dst -= 8 * 4;
  while (src >= aSrc) {
    __m256i px = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src));
    px = _mm256_permutevar8x32_epi32(px, discardMask);
    px = _mm256_shuffle_epi8(px, colorMask);
    px = _mm256_or_si256(px, alphaMask);
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst), px);
    src -= 8 * 3;
    dst -= 8 * 4;
  }
}

// Force instantiation of swizzle variants here.
template void UnpackRowRGB24_AVX2<false>(const uint8_t*, uint8_t*, int32_t);
template void UnpackRowRGB24_AVX2<true>(const uint8_t*, uint8_t*, int32_t);

}  // namespace gfx
}  // namespace mozilla
