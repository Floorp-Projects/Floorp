/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Swizzle.h"

#include <emmintrin.h>

namespace mozilla {
namespace gfx {

// Load 1-3 pixels into a 4 pixel vector.
static MOZ_ALWAYS_INLINE __m128i LoadRemainder_SSE2(const uint8_t* aSrc,
                                                    size_t aLength) {
  __m128i px;
  if (aLength >= 2) {
    // Load first 2 pixels
    px = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(aSrc));
    // Load third pixel
    if (aLength >= 3) {
      px = _mm_unpacklo_epi64(
          px,
          _mm_cvtsi32_si128(*reinterpret_cast<const uint32_t*>(aSrc + 2 * 4)));
    }
  } else {
    // Load single pixel
    px = _mm_cvtsi32_si128(*reinterpret_cast<const uint32_t*>(aSrc));
  }
  return px;
}

// Store 1-3 pixels from a vector into memory without overwriting.
static MOZ_ALWAYS_INLINE void StoreRemainder_SSE2(uint8_t* aDst, size_t aLength,
                                                  const __m128i& aSrc) {
  if (aLength >= 2) {
    // Store first 2 pixels
    _mm_storel_epi64(reinterpret_cast<__m128i*>(aDst), aSrc);
    // Store third pixel
    if (aLength >= 3) {
      *reinterpret_cast<uint32_t*>(aDst + 2 * 4) =
          _mm_cvtsi128_si32(_mm_srli_si128(aSrc, 2 * 4));
    }
  } else {
    // Store single pixel
    *reinterpret_cast<uint32_t*>(aDst) = _mm_cvtsi128_si32(aSrc);
  }
}

// Premultiply vector of 4 pixels using splayed math.
template <bool aSwapRB, bool aOpaqueAlpha>
static MOZ_ALWAYS_INLINE __m128i PremultiplyVector_SSE2(const __m128i& aSrc) {
  // Isolate R and B with mask.
  const __m128i mask = _mm_set1_epi32(0x00FF00FF);
  __m128i rb = _mm_and_si128(mask, aSrc);
  // Swap R and B if necessary.
  if (aSwapRB) {
    rb = _mm_shufflelo_epi16(rb, _MM_SHUFFLE(2, 3, 0, 1));
    rb = _mm_shufflehi_epi16(rb, _MM_SHUFFLE(2, 3, 0, 1));
  }
  // Isolate G and A by shifting down to bottom of word.
  __m128i ga = _mm_srli_epi16(aSrc, 8);

  // Duplicate alphas to get vector of A1 A1 A2 A2 A3 A3 A4 A4
  __m128i alphas = _mm_shufflelo_epi16(ga, _MM_SHUFFLE(3, 3, 1, 1));
  alphas = _mm_shufflehi_epi16(alphas, _MM_SHUFFLE(3, 3, 1, 1));

  // rb = rb*a + 255; rb += rb >> 8;
  rb = _mm_add_epi16(_mm_mullo_epi16(rb, alphas), mask);
  rb = _mm_add_epi16(rb, _mm_srli_epi16(rb, 8));

  // If format is not opaque, force A to 255 so that A*alpha/255 = alpha
  if (!aOpaqueAlpha) {
    ga = _mm_or_si128(ga, _mm_set1_epi32(0x00FF0000));
  }
  // ga = ga*a + 255; ga += ga >> 8;
  ga = _mm_add_epi16(_mm_mullo_epi16(ga, alphas), mask);
  ga = _mm_add_epi16(ga, _mm_srli_epi16(ga, 8));
  // If format is opaque, force output A to be 255.
  if (aOpaqueAlpha) {
    ga = _mm_or_si128(ga, _mm_set1_epi32(0xFF000000));
  }

  // Combine back to final pixel with (rb >> 8) | (ga & 0xFF00FF00)
  rb = _mm_srli_epi16(rb, 8);
  ga = _mm_andnot_si128(mask, ga);
  return _mm_or_si128(rb, ga);
}

// Premultiply vector of aAlignedRow + aRemainder pixels.
template <bool aSwapRB, bool aOpaqueAlpha>
static MOZ_ALWAYS_INLINE void PremultiplyChunk_SSE2(const uint8_t*& aSrc,
                                                    uint8_t*& aDst,
                                                    int32_t aAlignedRow,
                                                    int32_t aRemainder) {
  // Process all 4-pixel chunks as one vector.
  for (const uint8_t* end = aSrc + aAlignedRow; aSrc < end;) {
    __m128i px = _mm_loadu_si128(reinterpret_cast<const __m128i*>(aSrc));
    px = PremultiplyVector_SSE2<aSwapRB, aOpaqueAlpha>(px);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(aDst), px);
    aSrc += 4 * 4;
    aDst += 4 * 4;
  }

  // Handle any 1-3 remaining pixels.
  if (aRemainder) {
    __m128i px = LoadRemainder_SSE2(aSrc, aRemainder);
    px = PremultiplyVector_SSE2<aSwapRB, aOpaqueAlpha>(px);
    StoreRemainder_SSE2(aDst, aRemainder, px);
  }
}

// Premultiply vector of aLength pixels.
template <bool aSwapRB, bool aOpaqueAlpha>
void PremultiplyRow_SSE2(const uint8_t* aSrc, uint8_t* aDst, int32_t aLength) {
  int32_t alignedRow = 4 * (aLength & ~3);
  int32_t remainder = aLength & 3;
  PremultiplyChunk_SSE2<aSwapRB, aOpaqueAlpha>(aSrc, aDst, alignedRow,
                                               remainder);
}

template <bool aSwapRB, bool aOpaqueAlpha>
void Premultiply_SSE2(const uint8_t* aSrc, int32_t aSrcGap, uint8_t* aDst,
                      int32_t aDstGap, IntSize aSize) {
  int32_t alignedRow = 4 * (aSize.width & ~3);
  int32_t remainder = aSize.width & 3;
  // Fold remainder into stride gap.
  aSrcGap += 4 * remainder;
  aDstGap += 4 * remainder;

  for (int32_t height = aSize.height; height > 0; height--) {
    PremultiplyChunk_SSE2<aSwapRB, aOpaqueAlpha>(aSrc, aDst, alignedRow,
                                                 remainder);
    aSrc += aSrcGap;
    aDst += aDstGap;
  }
}

// Force instantiation of premultiply variants here.
template void PremultiplyRow_SSE2<false, false>(const uint8_t*, uint8_t*,
                                                int32_t);
template void PremultiplyRow_SSE2<false, true>(const uint8_t*, uint8_t*,
                                               int32_t);
template void PremultiplyRow_SSE2<true, false>(const uint8_t*, uint8_t*,
                                               int32_t);
template void PremultiplyRow_SSE2<true, true>(const uint8_t*, uint8_t*,
                                              int32_t);
template void Premultiply_SSE2<false, false>(const uint8_t*, int32_t, uint8_t*,
                                             int32_t, IntSize);
template void Premultiply_SSE2<false, true>(const uint8_t*, int32_t, uint8_t*,
                                            int32_t, IntSize);
template void Premultiply_SSE2<true, false>(const uint8_t*, int32_t, uint8_t*,
                                            int32_t, IntSize);
template void Premultiply_SSE2<true, true>(const uint8_t*, int32_t, uint8_t*,
                                           int32_t, IntSize);

// This generates a table of fixed-point reciprocals representing 1/alpha
// similar to the fallback implementation. However, the reciprocal must fit
// in 16 bits to multiply cheaply. Observe that reciprocals of smaller alphas
// require more bits than for larger alphas. We take advantage of this by
// shifting the reciprocal down by either 3 or 8 bits depending on whether
// the alpha value is less than 0x20. This is easy to then undo by multiplying
// the color component to be unpremultiplying by either 8 or 0x100,
// respectively. The 16 bit reciprocal is duplicated into both words of a
// uint32_t here to reduce unpacking overhead.
#define UNPREMULQ_SSE2(x) \
  (0x10001U * (0xFF0220U / ((x) * ((x) < 0x20 ? 0x100 : 8))))
#define UNPREMULQ_SSE2_2(x) UNPREMULQ_SSE2(x), UNPREMULQ_SSE2((x) + 1)
#define UNPREMULQ_SSE2_4(x) UNPREMULQ_SSE2_2(x), UNPREMULQ_SSE2_2((x) + 2)
#define UNPREMULQ_SSE2_8(x) UNPREMULQ_SSE2_4(x), UNPREMULQ_SSE2_4((x) + 4)
#define UNPREMULQ_SSE2_16(x) UNPREMULQ_SSE2_8(x), UNPREMULQ_SSE2_8((x) + 8)
#define UNPREMULQ_SSE2_32(x) UNPREMULQ_SSE2_16(x), UNPREMULQ_SSE2_16((x) + 16)
static const uint32_t sUnpremultiplyTable_SSE2[256] = {0,
                                                       UNPREMULQ_SSE2(1),
                                                       UNPREMULQ_SSE2_2(2),
                                                       UNPREMULQ_SSE2_4(4),
                                                       UNPREMULQ_SSE2_8(8),
                                                       UNPREMULQ_SSE2_16(16),
                                                       UNPREMULQ_SSE2_32(32),
                                                       UNPREMULQ_SSE2_32(64),
                                                       UNPREMULQ_SSE2_32(96),
                                                       UNPREMULQ_SSE2_32(128),
                                                       UNPREMULQ_SSE2_32(160),
                                                       UNPREMULQ_SSE2_32(192),
                                                       UNPREMULQ_SSE2_32(224)};

// Unpremultiply a vector of 4 pixels using splayed math and a reciprocal table
// that avoids doing any actual division.
template <bool aSwapRB>
static MOZ_ALWAYS_INLINE __m128i UnpremultiplyVector_SSE2(const __m128i& aSrc) {
  // Isolate R and B with mask.
  __m128i rb = _mm_and_si128(aSrc, _mm_set1_epi32(0x00FF00FF));
  // Swap R and B if necessary.
  if (aSwapRB) {
    rb = _mm_shufflelo_epi16(rb, _MM_SHUFFLE(2, 3, 0, 1));
    rb = _mm_shufflehi_epi16(rb, _MM_SHUFFLE(2, 3, 0, 1));
  }

  // Isolate G and A by shifting down to bottom of word.
  __m128i ga = _mm_srli_epi16(aSrc, 8);
  // Extract the alphas for the 4 pixels from the now isolated words.
  int a1 = _mm_extract_epi16(ga, 1);
  int a2 = _mm_extract_epi16(ga, 3);
  int a3 = _mm_extract_epi16(ga, 5);
  int a4 = _mm_extract_epi16(ga, 7);

  // Load the 16 bit reciprocals from the table for each alpha.
  // The reciprocals are doubled in each uint32_t entry.
  // Unpack them to a final vector of duplicated reciprocals of
  // the form Q1 Q1 Q2 Q2 Q3 Q3 Q4 Q4.
  __m128i q12 =
      _mm_unpacklo_epi32(_mm_cvtsi32_si128(sUnpremultiplyTable_SSE2[a1]),
                         _mm_cvtsi32_si128(sUnpremultiplyTable_SSE2[a2]));
  __m128i q34 =
      _mm_unpacklo_epi32(_mm_cvtsi32_si128(sUnpremultiplyTable_SSE2[a3]),
                         _mm_cvtsi32_si128(sUnpremultiplyTable_SSE2[a4]));
  __m128i q1234 = _mm_unpacklo_epi64(q12, q34);

  // Check if the alphas are less than 0x20, so that we can undo
  // scaling of the reciprocals as appropriate.
  __m128i scale = _mm_cmplt_epi32(ga, _mm_set1_epi32(0x00200000));
  // Produce scale factors by ((a < 0x20) ^ 8) & 0x108,
  // such that scale is 0x100 if < 0x20, and 8 otherwise.
  scale = _mm_xor_si128(scale, _mm_set1_epi16(8));
  scale = _mm_and_si128(scale, _mm_set1_epi16(0x108));
  // Isolate G now so that we don't accidentally unpremultiply A.
  ga = _mm_and_si128(ga, _mm_set1_epi32(0x000000FF));

  // Scale R, B, and G as required depending on reciprocal precision.
  rb = _mm_mullo_epi16(rb, scale);
  ga = _mm_mullo_epi16(ga, scale);

  // Multiply R, B, and G by the reciprocal, only taking the high word
  // too effectively shift right by 16.
  rb = _mm_mulhi_epu16(rb, q1234);
  ga = _mm_mulhi_epu16(ga, q1234);

  // Combine back to final pixel with rb | (ga << 8) | (aSrc & 0xFF000000),
  // which will add back on the original alpha value unchanged.
  ga = _mm_slli_si128(ga, 1);
  ga = _mm_or_si128(ga, _mm_and_si128(aSrc, _mm_set1_epi32(0xFF000000)));
  return _mm_or_si128(rb, ga);
}

template <bool aSwapRB>
void Unpremultiply_SSE2(const uint8_t* aSrc, int32_t aSrcGap, uint8_t* aDst,
                        int32_t aDstGap, IntSize aSize) {
  int32_t alignedRow = 4 * (aSize.width & ~3);
  int32_t remainder = aSize.width & 3;
  // Fold remainder into stride gap.
  aSrcGap += 4 * remainder;
  aDstGap += 4 * remainder;

  for (int32_t height = aSize.height; height > 0; height--) {
    // Process all 4-pixel chunks as one vector.
    for (const uint8_t* end = aSrc + alignedRow; aSrc < end;) {
      __m128i px = _mm_loadu_si128(reinterpret_cast<const __m128i*>(aSrc));
      px = UnpremultiplyVector_SSE2<aSwapRB>(px);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(aDst), px);
      aSrc += 4 * 4;
      aDst += 4 * 4;
    }

    // Handle any 1-3 remaining pixels.
    if (remainder) {
      __m128i px = LoadRemainder_SSE2(aSrc, remainder);
      px = UnpremultiplyVector_SSE2<aSwapRB>(px);
      StoreRemainder_SSE2(aDst, remainder, px);
    }

    aSrc += aSrcGap;
    aDst += aDstGap;
  }
}

// Force instantiation of unpremultiply variants here.
template void Unpremultiply_SSE2<false>(const uint8_t*, int32_t, uint8_t*,
                                        int32_t, IntSize);
template void Unpremultiply_SSE2<true>(const uint8_t*, int32_t, uint8_t*,
                                       int32_t, IntSize);

// Swizzle a vector of 4 pixels providing swaps and opaquifying.
template <bool aSwapRB, bool aOpaqueAlpha>
static MOZ_ALWAYS_INLINE __m128i SwizzleVector_SSE2(const __m128i& aSrc) {
  // Isolate R and B.
  __m128i rb = _mm_and_si128(aSrc, _mm_set1_epi32(0x00FF00FF));
  // Swap R and B.
  rb = _mm_shufflelo_epi16(rb, _MM_SHUFFLE(2, 3, 0, 1));
  rb = _mm_shufflehi_epi16(rb, _MM_SHUFFLE(2, 3, 0, 1));
  // Isolate G and A.
  __m128i ga = _mm_and_si128(aSrc, _mm_set1_epi32(0xFF00FF00));
  // Force alpha to 255 if necessary.
  if (aOpaqueAlpha) {
    ga = _mm_or_si128(ga, _mm_set1_epi32(0xFF000000));
  }
  // Combine everything back together.
  return _mm_or_si128(rb, ga);
}

#if 0
// These specializations currently do not profile faster than the generic versions,
// so disable them for now.

// Optimized implementations for when there is no R and B swap.
template<>
MOZ_ALWAYS_INLINE __m128i
SwizzleVector_SSE2<false, true>(const __m128i& aSrc)
{
  // Force alpha to 255.
  return _mm_or_si128(aSrc, _mm_set1_epi32(0xFF000000));
}

template<>
MOZ_ALWAYS_INLINE __m128i
SwizzleVector_SSE2<false, false>(const __m128i& aSrc)
{
  return aSrc;
}
#endif

template <bool aSwapRB, bool aOpaqueAlpha>
static MOZ_ALWAYS_INLINE void SwizzleChunk_SSE2(const uint8_t*& aSrc,
                                                uint8_t*& aDst,
                                                int32_t aAlignedRow,
                                                int32_t aRemainder) {
  // Process all 4-pixel chunks as one vector.
  for (const uint8_t* end = aSrc + aAlignedRow; aSrc < end;) {
    __m128i px = _mm_loadu_si128(reinterpret_cast<const __m128i*>(aSrc));
    px = SwizzleVector_SSE2<aSwapRB, aOpaqueAlpha>(px);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(aDst), px);
    aSrc += 4 * 4;
    aDst += 4 * 4;
  }

  // Handle any 1-3 remaining pixels.
  if (aRemainder) {
    __m128i px = LoadRemainder_SSE2(aSrc, aRemainder);
    px = SwizzleVector_SSE2<aSwapRB, aOpaqueAlpha>(px);
    StoreRemainder_SSE2(aDst, aRemainder, px);
  }
}

template <bool aSwapRB, bool aOpaqueAlpha>
void SwizzleRow_SSE2(const uint8_t* aSrc, uint8_t* aDst, int32_t aLength) {
  int32_t alignedRow = 4 * (aLength & ~3);
  int32_t remainder = aLength & 3;
  SwizzleChunk_SSE2<aSwapRB, aOpaqueAlpha>(aSrc, aDst, alignedRow, remainder);
}

template <bool aSwapRB, bool aOpaqueAlpha>
void Swizzle_SSE2(const uint8_t* aSrc, int32_t aSrcGap, uint8_t* aDst,
                  int32_t aDstGap, IntSize aSize) {
  int32_t alignedRow = 4 * (aSize.width & ~3);
  int32_t remainder = aSize.width & 3;
  // Fold remainder into stride gap.
  aSrcGap += 4 * remainder;
  aDstGap += 4 * remainder;

  for (int32_t height = aSize.height; height > 0; height--) {
    SwizzleChunk_SSE2<aSwapRB, aOpaqueAlpha>(aSrc, aDst, alignedRow, remainder);
    aSrc += aSrcGap;
    aDst += aDstGap;
  }
}

// Force instantiation of swizzle variants here.
template void SwizzleRow_SSE2<true, false>(const uint8_t*, uint8_t*, int32_t);
template void SwizzleRow_SSE2<true, true>(const uint8_t*, uint8_t*, int32_t);
template void Swizzle_SSE2<true, false>(const uint8_t*, int32_t, uint8_t*,
                                        int32_t, IntSize);
template void Swizzle_SSE2<true, true>(const uint8_t*, int32_t, uint8_t*,
                                       int32_t, IntSize);

}  // namespace gfx
}  // namespace mozilla
