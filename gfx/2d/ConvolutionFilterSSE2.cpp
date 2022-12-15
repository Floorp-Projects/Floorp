/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2011-2016 Google Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the gfx/skia/LICENSE file.

#include "SkConvolver.h"
#include "mozilla/Attributes.h"
#include <immintrin.h>

namespace skia {

static MOZ_ALWAYS_INLINE void AccumRemainder(
    const unsigned char* pixelsLeft,
    const SkConvolutionFilter1D::ConvolutionFixed* filterValues, __m128i& accum,
    int r) {
  int remainder[4] = {0};
  for (int i = 0; i < r; i++) {
    SkConvolutionFilter1D::ConvolutionFixed coeff = filterValues[i];
    remainder[0] += coeff * pixelsLeft[i * 4 + 0];
    remainder[1] += coeff * pixelsLeft[i * 4 + 1];
    remainder[2] += coeff * pixelsLeft[i * 4 + 2];
    remainder[3] += coeff * pixelsLeft[i * 4 + 3];
  }
  __m128i t =
      _mm_setr_epi32(remainder[0], remainder[1], remainder[2], remainder[3]);
  accum = _mm_add_epi32(accum, t);
}

// Convolves horizontally along a single row. The row data is given in
// |srcData| and continues for the numValues() of the filter.
void convolve_horizontally_sse2(const unsigned char* srcData,
                                const SkConvolutionFilter1D& filter,
                                unsigned char* outRow, bool /*hasAlpha*/) {
  // Output one pixel each iteration, calculating all channels (RGBA) together.
  int numValues = filter.numValues();
  for (int outX = 0; outX < numValues; outX++) {
    // Get the filter that determines the current output pixel.
    int filterOffset, filterLength;
    const SkConvolutionFilter1D::ConvolutionFixed* filterValues =
        filter.FilterForValue(outX, &filterOffset, &filterLength);

    // Compute the first pixel in this row that the filter affects. It will
    // touch |filterLength| pixels (4 bytes each) after this.
    const unsigned char* rowToFilter = &srcData[filterOffset * 4];

    __m128i zero = _mm_setzero_si128();
    __m128i accum = _mm_setzero_si128();

    // We will load and accumulate with four coefficients per iteration.
    for (int filterX = 0; filterX < filterLength >> 2; filterX++) {
      // Load 4 coefficients => duplicate 1st and 2nd of them for all channels.
      __m128i coeff, coeff16;
      // [16] xx xx xx xx c3 c2 c1 c0
      coeff = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(filterValues));
      // [16] xx xx xx xx c1 c1 c0 c0
      coeff16 = _mm_shufflelo_epi16(coeff, _MM_SHUFFLE(1, 1, 0, 0));
      // [16] c1 c1 c1 c1 c0 c0 c0 c0
      coeff16 = _mm_unpacklo_epi16(coeff16, coeff16);

      // Load four pixels => unpack the first two pixels to 16 bits =>
      // multiply with coefficients => accumulate the convolution result.
      // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
      __m128i src8 =
          _mm_loadu_si128(reinterpret_cast<const __m128i*>(rowToFilter));
      // [16] a1 b1 g1 r1 a0 b0 g0 r0
      __m128i src16 = _mm_unpacklo_epi8(src8, zero);
      __m128i mul_hi = _mm_mulhi_epi16(src16, coeff16);
      __m128i mul_lo = _mm_mullo_epi16(src16, coeff16);
      // [32]  a0*c0 b0*c0 g0*c0 r0*c0
      __m128i t = _mm_unpacklo_epi16(mul_lo, mul_hi);
      accum = _mm_add_epi32(accum, t);
      // [32]  a1*c1 b1*c1 g1*c1 r1*c1
      t = _mm_unpackhi_epi16(mul_lo, mul_hi);
      accum = _mm_add_epi32(accum, t);

      // Duplicate 3rd and 4th coefficients for all channels =>
      // unpack the 3rd and 4th pixels to 16 bits => multiply with coefficients
      // => accumulate the convolution results.
      // [16] xx xx xx xx c3 c3 c2 c2
      coeff16 = _mm_shufflelo_epi16(coeff, _MM_SHUFFLE(3, 3, 2, 2));
      // [16] c3 c3 c3 c3 c2 c2 c2 c2
      coeff16 = _mm_unpacklo_epi16(coeff16, coeff16);
      // [16] a3 g3 b3 r3 a2 g2 b2 r2
      src16 = _mm_unpackhi_epi8(src8, zero);
      mul_hi = _mm_mulhi_epi16(src16, coeff16);
      mul_lo = _mm_mullo_epi16(src16, coeff16);
      // [32]  a2*c2 b2*c2 g2*c2 r2*c2
      t = _mm_unpacklo_epi16(mul_lo, mul_hi);
      accum = _mm_add_epi32(accum, t);
      // [32]  a3*c3 b3*c3 g3*c3 r3*c3
      t = _mm_unpackhi_epi16(mul_lo, mul_hi);
      accum = _mm_add_epi32(accum, t);

      // Advance the pixel and coefficients pointers.
      rowToFilter += 16;
      filterValues += 4;
    }

    // When |filterLength| is not divisible by 4, we accumulate the last 1 - 3
    // coefficients one at a time.
    int r = filterLength & 3;
    if (r) {
      int remainderOffset = (filterOffset + filterLength - r) * 4;
      AccumRemainder(srcData + remainderOffset, filterValues, accum, r);
    }

    // Shift right for fixed point implementation.
    accum = _mm_srai_epi32(accum, SkConvolutionFilter1D::kShiftBits);

    // Packing 32 bits |accum| to 16 bits per channel (signed saturation).
    accum = _mm_packs_epi32(accum, zero);
    // Packing 16 bits |accum| to 8 bits per channel (unsigned saturation).
    accum = _mm_packus_epi16(accum, zero);

    // Store the pixel value of 32 bits.
    *(reinterpret_cast<int*>(outRow)) = _mm_cvtsi128_si32(accum);
    outRow += 4;
  }
}

// Does vertical convolution to produce one output row. The filter values and
// length are given in the first two parameters. These are applied to each
// of the rows pointed to in the |sourceDataRows| array, with each row
// being |pixelWidth| wide.
//
// The output must have room for |pixelWidth * 4| bytes.
template <bool hasAlpha>
static void ConvolveVertically(
    const SkConvolutionFilter1D::ConvolutionFixed* filterValues,
    int filterLength, unsigned char* const* sourceDataRows, int pixelWidth,
    unsigned char* outRow) {
  // Output four pixels per iteration (16 bytes).
  int width = pixelWidth & ~3;
  __m128i zero = _mm_setzero_si128();
  for (int outX = 0; outX < width; outX += 4) {
    // Accumulated result for each pixel. 32 bits per RGBA channel.
    __m128i accum0 = _mm_setzero_si128();
    __m128i accum1 = _mm_setzero_si128();
    __m128i accum2 = _mm_setzero_si128();
    __m128i accum3 = _mm_setzero_si128();

    // Convolve with one filter coefficient per iteration.
    for (int filterY = 0; filterY < filterLength; filterY++) {
      // Duplicate the filter coefficient 8 times.
      // [16] cj cj cj cj cj cj cj cj
      __m128i coeff16 = _mm_set1_epi16(filterValues[filterY]);

      // Load four pixels (16 bytes) together.
      // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
      const __m128i* src =
          reinterpret_cast<const __m128i*>(&sourceDataRows[filterY][outX << 2]);
      __m128i src8 = _mm_loadu_si128(src);

      // Unpack 1st and 2nd pixels from 8 bits to 16 bits for each channels =>
      // multiply with current coefficient => accumulate the result.
      // [16] a1 b1 g1 r1 a0 b0 g0 r0
      __m128i src16 = _mm_unpacklo_epi8(src8, zero);
      __m128i mul_hi = _mm_mulhi_epi16(src16, coeff16);
      __m128i mul_lo = _mm_mullo_epi16(src16, coeff16);
      // [32] a0 b0 g0 r0
      __m128i t = _mm_unpacklo_epi16(mul_lo, mul_hi);
      accum0 = _mm_add_epi32(accum0, t);
      // [32] a1 b1 g1 r1
      t = _mm_unpackhi_epi16(mul_lo, mul_hi);
      accum1 = _mm_add_epi32(accum1, t);

      // Unpack 3rd and 4th pixels from 8 bits to 16 bits for each channels =>
      // multiply with current coefficient => accumulate the result.
      // [16] a3 b3 g3 r3 a2 b2 g2 r2
      src16 = _mm_unpackhi_epi8(src8, zero);
      mul_hi = _mm_mulhi_epi16(src16, coeff16);
      mul_lo = _mm_mullo_epi16(src16, coeff16);
      // [32] a2 b2 g2 r2
      t = _mm_unpacklo_epi16(mul_lo, mul_hi);
      accum2 = _mm_add_epi32(accum2, t);
      // [32] a3 b3 g3 r3
      t = _mm_unpackhi_epi16(mul_lo, mul_hi);
      accum3 = _mm_add_epi32(accum3, t);
    }

    // Shift right for fixed point implementation.
    accum0 = _mm_srai_epi32(accum0, SkConvolutionFilter1D::kShiftBits);
    accum1 = _mm_srai_epi32(accum1, SkConvolutionFilter1D::kShiftBits);
    accum2 = _mm_srai_epi32(accum2, SkConvolutionFilter1D::kShiftBits);
    accum3 = _mm_srai_epi32(accum3, SkConvolutionFilter1D::kShiftBits);

    // Packing 32 bits |accum| to 16 bits per channel (signed saturation).
    // [16] a1 b1 g1 r1 a0 b0 g0 r0
    accum0 = _mm_packs_epi32(accum0, accum1);
    // [16] a3 b3 g3 r3 a2 b2 g2 r2
    accum2 = _mm_packs_epi32(accum2, accum3);

    // Packing 16 bits |accum| to 8 bits per channel (unsigned saturation).
    // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
    accum0 = _mm_packus_epi16(accum0, accum2);

    if (hasAlpha) {
      // Compute the max(ri, gi, bi) for each pixel.
      // [8] xx a3 b3 g3 xx a2 b2 g2 xx a1 b1 g1 xx a0 b0 g0
      __m128i a = _mm_srli_epi32(accum0, 8);
      // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
      __m128i b = _mm_max_epu8(a, accum0);  // Max of r and g.
      // [8] xx xx a3 b3 xx xx a2 b2 xx xx a1 b1 xx xx a0 b0
      a = _mm_srli_epi32(accum0, 16);
      // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
      b = _mm_max_epu8(a, b);  // Max of r and g and b.
      // [8] max3 00 00 00 max2 00 00 00 max1 00 00 00 max0 00 00 00
      b = _mm_slli_epi32(b, 24);

      // Make sure the value of alpha channel is always larger than maximum
      // value of color channels.
      accum0 = _mm_max_epu8(b, accum0);
    } else {
      // Set value of alpha channels to 0xFF.
      __m128i mask = _mm_set1_epi32(0xff000000);
      accum0 = _mm_or_si128(accum0, mask);
    }

    // Store the convolution result (16 bytes) and advance the pixel pointers.
    _mm_storeu_si128(reinterpret_cast<__m128i*>(outRow), accum0);
    outRow += 16;
  }

  // When the width of the output is not divisible by 4, We need to save one
  // pixel (4 bytes) each time. And also the fourth pixel is always absent.
  int r = pixelWidth & 3;
  if (r) {
    __m128i accum0 = _mm_setzero_si128();
    __m128i accum1 = _mm_setzero_si128();
    __m128i accum2 = _mm_setzero_si128();
    for (int filterY = 0; filterY < filterLength; ++filterY) {
      __m128i coeff16 = _mm_set1_epi16(filterValues[filterY]);
      // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
      const __m128i* src = reinterpret_cast<const __m128i*>(
          &sourceDataRows[filterY][width << 2]);
      __m128i src8 = _mm_loadu_si128(src);
      // [16] a1 b1 g1 r1 a0 b0 g0 r0
      __m128i src16 = _mm_unpacklo_epi8(src8, zero);
      __m128i mul_hi = _mm_mulhi_epi16(src16, coeff16);
      __m128i mul_lo = _mm_mullo_epi16(src16, coeff16);
      // [32] a0 b0 g0 r0
      __m128i t = _mm_unpacklo_epi16(mul_lo, mul_hi);
      accum0 = _mm_add_epi32(accum0, t);
      // [32] a1 b1 g1 r1
      t = _mm_unpackhi_epi16(mul_lo, mul_hi);
      accum1 = _mm_add_epi32(accum1, t);
      // [16] a3 b3 g3 r3 a2 b2 g2 r2
      src16 = _mm_unpackhi_epi8(src8, zero);
      mul_hi = _mm_mulhi_epi16(src16, coeff16);
      mul_lo = _mm_mullo_epi16(src16, coeff16);
      // [32] a2 b2 g2 r2
      t = _mm_unpacklo_epi16(mul_lo, mul_hi);
      accum2 = _mm_add_epi32(accum2, t);
    }

    accum0 = _mm_srai_epi32(accum0, SkConvolutionFilter1D::kShiftBits);
    accum1 = _mm_srai_epi32(accum1, SkConvolutionFilter1D::kShiftBits);
    accum2 = _mm_srai_epi32(accum2, SkConvolutionFilter1D::kShiftBits);
    // [16] a1 b1 g1 r1 a0 b0 g0 r0
    accum0 = _mm_packs_epi32(accum0, accum1);
    // [16] a3 b3 g3 r3 a2 b2 g2 r2
    accum2 = _mm_packs_epi32(accum2, zero);
    // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
    accum0 = _mm_packus_epi16(accum0, accum2);
    if (hasAlpha) {
      // [8] xx a3 b3 g3 xx a2 b2 g2 xx a1 b1 g1 xx a0 b0 g0
      __m128i a = _mm_srli_epi32(accum0, 8);
      // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
      __m128i b = _mm_max_epu8(a, accum0);  // Max of r and g.
      // [8] xx xx a3 b3 xx xx a2 b2 xx xx a1 b1 xx xx a0 b0
      a = _mm_srli_epi32(accum0, 16);
      // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
      b = _mm_max_epu8(a, b);  // Max of r and g and b.
      // [8] max3 00 00 00 max2 00 00 00 max1 00 00 00 max0 00 00 00
      b = _mm_slli_epi32(b, 24);
      accum0 = _mm_max_epu8(b, accum0);
    } else {
      __m128i mask = _mm_set1_epi32(0xff000000);
      accum0 = _mm_or_si128(accum0, mask);
    }

    for (int i = 0; i < r; i++) {
      *(reinterpret_cast<int*>(outRow)) = _mm_cvtsi128_si32(accum0);
      accum0 = _mm_srli_si128(accum0, 4);
      outRow += 4;
    }
  }
}

void convolve_vertically_sse2(
    const SkConvolutionFilter1D::ConvolutionFixed* filterValues,
    int filterLength, unsigned char* const* sourceDataRows, int pixelWidth,
    unsigned char* outRow, bool hasAlpha) {
  if (hasAlpha) {
    ConvolveVertically<true>(filterValues, filterLength, sourceDataRows,
                             pixelWidth, outRow);
  } else {
    ConvolveVertically<false>(filterValues, filterLength, sourceDataRows,
                              pixelWidth, outRow);
  }
}

}  // namespace skia
