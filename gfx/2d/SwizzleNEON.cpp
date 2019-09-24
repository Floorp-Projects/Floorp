/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Swizzle.h"

#include <arm_neon.h>

namespace mozilla {
namespace gfx {

// Load 1-3 pixels into a 4 pixel vector.
static MOZ_ALWAYS_INLINE uint16x8_t LoadRemainder_NEON(const uint8_t* aSrc,
                                                       size_t aLength) {
  const uint32_t* src32 = reinterpret_cast<const uint32_t*>(aSrc);
  uint32x4_t dst32;
  if (aLength >= 2) {
    // Load first 2 pixels
    dst32 = vcombine_u32(vld1_u32(src32), vdup_n_u32(0));
    // Load third pixel
    if (aLength >= 3) {
      dst32 = vld1q_lane_u32(src32 + 2, dst32, 2);
    }
  } else {
    // Load single pixel
    dst32 = vld1q_lane_u32(src32, vdupq_n_u32(0), 0);
  }
  return vreinterpretq_u16_u32(dst32);
}

// Store 1-3 pixels from a vector into memory without overwriting.
static MOZ_ALWAYS_INLINE void StoreRemainder_NEON(uint8_t* aDst, size_t aLength,
                                                  const uint16x8_t& aSrc) {
  uint32_t* dst32 = reinterpret_cast<uint32_t*>(aDst);
  uint32x4_t src32 = vreinterpretq_u32_u16(aSrc);
  if (aLength >= 2) {
    // Store first 2 pixels
    vst1_u32(dst32, vget_low_u32(src32));
    // Store third pixel
    if (aLength >= 3) {
      vst1q_lane_u32(dst32 + 2, src32, 2);
    }
  } else {
    // Store single pixel
    vst1q_lane_u32(dst32, src32, 0);
  }
}

// Premultiply vector of 4 pixels using splayed math.
template <bool aSwapRB, bool aOpaqueAlpha>
static MOZ_ALWAYS_INLINE uint16x8_t
PremultiplyVector_NEON(const uint16x8_t& aSrc) {
  // Isolate R and B with mask.
  const uint16x8_t mask = vdupq_n_u16(0x00FF);
  uint16x8_t rb = vandq_u16(aSrc, mask);
  // Swap R and B if necessary.
  if (aSwapRB) {
    rb = vrev32q_u16(rb);
  }
  // Isolate G and A by shifting down to bottom of word.
  uint16x8_t ga = vshrq_n_u16(aSrc, 8);

  // Duplicate alphas to get vector of A1 A1 A2 A2 A3 A3 A4 A4
  uint16x8_t alphas = vtrnq_u16(ga, ga).val[1];

  // rb = rb*a + 255; rb += rb >> 8;
  rb = vmlaq_u16(mask, rb, alphas);
  rb = vsraq_n_u16(rb, rb, 8);

  // If format is not opaque, force A to 255 so that A*alpha/255 = alpha
  if (!aOpaqueAlpha) {
    ga = vorrq_u16(ga, vreinterpretq_u16_u32(vdupq_n_u32(0x00FF0000)));
  }
  // ga = ga*a + 255; ga += ga >> 8;
  ga = vmlaq_u16(mask, ga, alphas);
  ga = vsraq_n_u16(ga, ga, 8);
  // If format is opaque, force output A to be 255.
  if (aOpaqueAlpha) {
    ga = vorrq_u16(ga, vreinterpretq_u16_u32(vdupq_n_u32(0xFF000000)));
  }

  // Combine back to final pixel with (rb >> 8) | (ga & 0xFF00FF00)
  return vsriq_n_u16(ga, rb, 8);
}

template <bool aSwapRB, bool aOpaqueAlpha>
static MOZ_ALWAYS_INLINE void PremultiplyChunk_NEON(const uint8_t*& aSrc,
                                                    uint8_t*& aDst,
                                                    int32_t aAlignedRow,
                                                    int32_t aRemainder) {
  // Process all 4-pixel chunks as one vector.
  for (const uint8_t* end = aSrc + aAlignedRow; aSrc < end;) {
    uint16x8_t px = vld1q_u16(reinterpret_cast<const uint16_t*>(aSrc));
    px = PremultiplyVector_NEON<aSwapRB, aOpaqueAlpha>(px);
    vst1q_u16(reinterpret_cast<uint16_t*>(aDst), px);
    aSrc += 4 * 4;
    aDst += 4 * 4;
  }

  // Handle any 1-3 remaining pixels.
  if (aRemainder) {
    uint16x8_t px = LoadRemainder_NEON(aSrc, aRemainder);
    px = PremultiplyVector_NEON<aSwapRB, aOpaqueAlpha>(px);
    StoreRemainder_NEON(aDst, aRemainder, px);
  }
}

template <bool aSwapRB, bool aOpaqueAlpha>
void PremultiplyRow_NEON(const uint8_t* aSrc, uint8_t* aDst, int32_t aLength) {
  int32_t alignedRow = 4 * (aLength & ~3);
  int32_t remainder = aLength & 3;
  PremultiplyChunk_NEON<aSwapRB, aOpaqueAlpha>(aSrc, aDst, alignedRow,
                                               remainder);
}

template <bool aSwapRB, bool aOpaqueAlpha>
void Premultiply_NEON(const uint8_t* aSrc, int32_t aSrcGap, uint8_t* aDst,
                      int32_t aDstGap, IntSize aSize) {
  int32_t alignedRow = 4 * (aSize.width & ~3);
  int32_t remainder = aSize.width & 3;
  // Fold remainder into stride gap.
  aSrcGap += 4 * remainder;
  aDstGap += 4 * remainder;

  for (int32_t height = aSize.height; height > 0; height--) {
    PremultiplyChunk_NEON<aSwapRB, aOpaqueAlpha>(aSrc, aDst, alignedRow,
                                                 remainder);
    aSrc += aSrcGap;
    aDst += aDstGap;
  }
}

// Force instantiation of premultiply variants here.
template void PremultiplyRow_NEON<false, false>(const uint8_t*, uint8_t*,
                                                int32_t);
template void PremultiplyRow_NEON<false, true>(const uint8_t*, uint8_t*,
                                               int32_t);
template void PremultiplyRow_NEON<true, false>(const uint8_t*, uint8_t*,
                                               int32_t);
template void PremultiplyRow_NEON<true, true>(const uint8_t*, uint8_t*,
                                              int32_t);
template void Premultiply_NEON<false, false>(const uint8_t*, int32_t, uint8_t*,
                                             int32_t, IntSize);
template void Premultiply_NEON<false, true>(const uint8_t*, int32_t, uint8_t*,
                                            int32_t, IntSize);
template void Premultiply_NEON<true, false>(const uint8_t*, int32_t, uint8_t*,
                                            int32_t, IntSize);
template void Premultiply_NEON<true, true>(const uint8_t*, int32_t, uint8_t*,
                                           int32_t, IntSize);

// This generates a table of fixed-point reciprocals representing 1/alpha
// similar to the fallback implementation. However, the reciprocal must
// ultimately be multiplied as an unsigned 9 bit upper part and a signed
// 15 bit lower part to cheaply multiply. Thus, the lower 15 bits of the
// reciprocal is stored 15 bits of the reciprocal are masked off and
// stored in the low word. The upper 9 bits are masked and shifted to fit
// into the high word. These then get independently multiplied with the
// color component and recombined to provide the full recriprocal multiply.
#define UNPREMULQ_NEON(x) \
  ((((0xFF00FFU / (x)) & 0xFF8000U) << 1) | ((0xFF00FFU / (x)) & 0x7FFFU))
#define UNPREMULQ_NEON_2(x) UNPREMULQ_NEON(x), UNPREMULQ_NEON((x) + 1)
#define UNPREMULQ_NEON_4(x) UNPREMULQ_NEON_2(x), UNPREMULQ_NEON_2((x) + 2)
#define UNPREMULQ_NEON_8(x) UNPREMULQ_NEON_4(x), UNPREMULQ_NEON_4((x) + 4)
#define UNPREMULQ_NEON_16(x) UNPREMULQ_NEON_8(x), UNPREMULQ_NEON_8((x) + 8)
#define UNPREMULQ_NEON_32(x) UNPREMULQ_NEON_16(x), UNPREMULQ_NEON_16((x) + 16)
static const uint32_t sUnpremultiplyTable_NEON[256] = {0,
                                                       UNPREMULQ_NEON(1),
                                                       UNPREMULQ_NEON_2(2),
                                                       UNPREMULQ_NEON_4(4),
                                                       UNPREMULQ_NEON_8(8),
                                                       UNPREMULQ_NEON_16(16),
                                                       UNPREMULQ_NEON_32(32),
                                                       UNPREMULQ_NEON_32(64),
                                                       UNPREMULQ_NEON_32(96),
                                                       UNPREMULQ_NEON_32(128),
                                                       UNPREMULQ_NEON_32(160),
                                                       UNPREMULQ_NEON_32(192),
                                                       UNPREMULQ_NEON_32(224)};

// Unpremultiply a vector of 4 pixels using splayed math and a reciprocal table
// that avoids doing any actual division.
template <bool aSwapRB>
static MOZ_ALWAYS_INLINE uint16x8_t
UnpremultiplyVector_NEON(const uint16x8_t& aSrc) {
  // Isolate R and B with mask.
  uint16x8_t rb = vandq_u16(aSrc, vdupq_n_u16(0x00FF));
  // Swap R and B if necessary.
  if (aSwapRB) {
    rb = vrev32q_u16(rb);
  }

  // Isolate G and A by shifting down to bottom of word.
  uint16x8_t ga = vshrq_n_u16(aSrc, 8);
  // Extract the alphas for the 4 pixels from the now isolated words.
  int a1 = vgetq_lane_u16(ga, 1);
  int a2 = vgetq_lane_u16(ga, 3);
  int a3 = vgetq_lane_u16(ga, 5);
  int a4 = vgetq_lane_u16(ga, 7);

  // First load all of the interleaved low and high portions of the reciprocals
  // and combine them a single vector as lo1 hi1 lo2 hi2 lo3 hi3 lo4 hi4
  uint16x8_t q1234 = vreinterpretq_u16_u32(vld1q_lane_u32(
      &sUnpremultiplyTable_NEON[a4],
      vld1q_lane_u32(
          &sUnpremultiplyTable_NEON[a3],
          vld1q_lane_u32(
              &sUnpremultiplyTable_NEON[a2],
              vld1q_lane_u32(&sUnpremultiplyTable_NEON[a1], vdupq_n_u32(0), 0),
              1),
          2),
      3));
  // Transpose the interleaved low/high portions so that we produce
  // two separate duplicated vectors for the low and high portions respectively:
  // lo1 lo1 lo2 lo2 lo3 lo3 lo4 lo4 and hi1 hi1 hi2 hi2 hi3 hi3 hi4 hi4
  uint16x8x2_t q1234lohi = vtrnq_u16(q1234, q1234);

  // VQDMULH is a signed multiply that doubles (*2) the result, then takes the
  // high word.  To work around the signedness and the doubling, the low
  // portion of the reciprocal only stores the lower 15 bits, which fits in a
  // signed 16 bit integer. The high 9 bit portion is effectively also doubled
  // by 2 as a side-effect of being shifted for storage. Thus the output scale
  // of doing a normal multiply by the high portion and the VQDMULH by the low
  // portion are both doubled and can be safely added together. The resulting
  // sum just needs to be halved (via VHADD) to thus cancel out the doubling.
  // All this combines to produce a reciprocal multiply of the form:
  // rb = ((rb * hi) + ((rb * lo * 2) >> 16)) / 2
  rb = vhaddq_u16(
      vmulq_u16(rb, q1234lohi.val[1]),
      vreinterpretq_u16_s16(vqdmulhq_s16(
          vreinterpretq_s16_u16(rb), vreinterpretq_s16_u16(q1234lohi.val[0]))));

  // ga = ((ga * hi) + ((ga * lo * 2) >> 16)) / 2
  ga = vhaddq_u16(
      vmulq_u16(ga, q1234lohi.val[1]),
      vreinterpretq_u16_s16(vqdmulhq_s16(
          vreinterpretq_s16_u16(ga), vreinterpretq_s16_u16(q1234lohi.val[0]))));

  // Combine to the final pixel with ((rb | (ga << 8)) & ~0xFF000000) | (aSrc &
  // 0xFF000000), which inserts back in the original alpha value unchanged.
  return vbslq_u16(vreinterpretq_u16_u32(vdupq_n_u32(0xFF000000)), aSrc,
                   vsliq_n_u16(rb, ga, 8));
}

template <bool aSwapRB>
void Unpremultiply_NEON(const uint8_t* aSrc, int32_t aSrcGap, uint8_t* aDst,
                        int32_t aDstGap, IntSize aSize) {
  int32_t alignedRow = 4 * (aSize.width & ~3);
  int32_t remainder = aSize.width & 3;
  // Fold remainder into stride gap.
  aSrcGap += 4 * remainder;
  aDstGap += 4 * remainder;

  for (int32_t height = aSize.height; height > 0; height--) {
    // Process all 4-pixel chunks as one vector.
    for (const uint8_t* end = aSrc + alignedRow; aSrc < end;) {
      uint16x8_t px = vld1q_u16(reinterpret_cast<const uint16_t*>(aSrc));
      px = UnpremultiplyVector_NEON<aSwapRB>(px);
      vst1q_u16(reinterpret_cast<uint16_t*>(aDst), px);
      aSrc += 4 * 4;
      aDst += 4 * 4;
    }

    // Handle any 1-3 remaining pixels.
    if (remainder) {
      uint16x8_t px = LoadRemainder_NEON(aSrc, remainder);
      px = UnpremultiplyVector_NEON<aSwapRB>(px);
      StoreRemainder_NEON(aDst, remainder, px);
    }

    aSrc += aSrcGap;
    aDst += aDstGap;
  }
}

// Force instantiation of unpremultiply variants here.
template void Unpremultiply_NEON<false>(const uint8_t*, int32_t, uint8_t*,
                                        int32_t, IntSize);
template void Unpremultiply_NEON<true>(const uint8_t*, int32_t, uint8_t*,
                                       int32_t, IntSize);

// Swizzle a vector of 4 pixels providing swaps and opaquifying.
template <bool aSwapRB, bool aOpaqueAlpha>
static MOZ_ALWAYS_INLINE uint16x8_t SwizzleVector_NEON(const uint16x8_t& aSrc) {
  // Swap R and B, then add to G and A (forced to 255):
  // (((src>>16) | (src << 16)) & 0x00FF00FF) |
  //   ((src | 0xFF000000) & ~0x00FF00FF)
  return vbslq_u16(
      vdupq_n_u16(0x00FF), vrev32q_u16(aSrc),
      aOpaqueAlpha
          ? vorrq_u16(aSrc, vreinterpretq_u16_u32(vdupq_n_u32(0xFF000000)))
          : aSrc);
}

#if 0
// These specializations currently do not profile faster than the generic versions,
// so disable them for now.

// Optimized implementations for when there is no R and B swap.
template<>
static MOZ_ALWAYS_INLINE uint16x8_t
SwizzleVector_NEON<false, true>(const uint16x8_t& aSrc)
{
  // Force alpha to 255.
  return vorrq_u16(aSrc, vreinterpretq_u16_u32(vdupq_n_u32(0xFF000000)));
}

template<>
static MOZ_ALWAYS_INLINE uint16x8_t
SwizzleVector_NEON<false, false>(const uint16x8_t& aSrc)
{
  return aSrc;
}
#endif

template <bool aSwapRB, bool aOpaqueAlpha>
static MOZ_ALWAYS_INLINE void SwizzleChunk_NEON(const uint8_t*& aSrc,
                                                uint8_t*& aDst,
                                                int32_t aAlignedRow,
                                                int32_t aRemainder) {
  // Process all 4-pixel chunks as one vector.
  for (const uint8_t* end = aSrc + aAlignedRow; aSrc < end;) {
    uint16x8_t px = vld1q_u16(reinterpret_cast<const uint16_t*>(aSrc));
    px = SwizzleVector_NEON<aSwapRB, aOpaqueAlpha>(px);
    vst1q_u16(reinterpret_cast<uint16_t*>(aDst), px);
    aSrc += 4 * 4;
    aDst += 4 * 4;
  }

  // Handle any 1-3 remaining pixels.
  if (aRemainder) {
    uint16x8_t px = LoadRemainder_NEON(aSrc, aRemainder);
    px = SwizzleVector_NEON<aSwapRB, aOpaqueAlpha>(px);
    StoreRemainder_NEON(aDst, aRemainder, px);
  }
}

template <bool aSwapRB, bool aOpaqueAlpha>
void SwizzleRow_NEON(const uint8_t* aSrc, uint8_t* aDst, int32_t aLength) {
  int32_t alignedRow = 4 * (aLength & ~3);
  int32_t remainder = aLength & 3;
  SwizzleChunk_NEON<aSwapRB, aOpaqueAlpha>(aSrc, aDst, alignedRow, remainder);
}

template <bool aSwapRB, bool aOpaqueAlpha>
void Swizzle_NEON(const uint8_t* aSrc, int32_t aSrcGap, uint8_t* aDst,
                  int32_t aDstGap, IntSize aSize) {
  int32_t alignedRow = 4 * (aSize.width & ~3);
  int32_t remainder = aSize.width & 3;
  // Fold remainder into stride gap.
  aSrcGap += 4 * remainder;
  aDstGap += 4 * remainder;

  for (int32_t height = aSize.height; height > 0; height--) {
    SwizzleChunk_NEON<aSwapRB, aOpaqueAlpha>(aSrc, aDst, alignedRow,
                                             remainder);
    aSrc += aSrcGap;
    aDst += aDstGap;
  }
}

// Force instantiation of swizzle variants here.
template void SwizzleRow_NEON<true, false>(const uint8_t*, uint8_t*, int32_t);
template void SwizzleRow_NEON<true, true>(const uint8_t*, uint8_t*, int32_t);
template void Swizzle_NEON<true, false>(const uint8_t*, int32_t, uint8_t*,
                                        int32_t, IntSize);
template void Swizzle_NEON<true, true>(const uint8_t*, int32_t, uint8_t*,
                                       int32_t, IntSize);

}  // namespace gfx
}  // namespace mozilla
