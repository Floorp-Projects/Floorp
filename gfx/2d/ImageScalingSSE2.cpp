/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageScaling.h"
#include "mozilla/Attributes.h"

#include "SSEHelpers.h"

/* The functions below use the following system for averaging 4 pixels:
 *
 * The first observation is that a half-adder is implemented as follows:
 * R = S + 2C or in the case of a and b (a ^ b) + ((a & b) << 1);
 *
 * This can be trivially extended to three pixels by observaring that when
 * doing (a ^ b ^ c) as the sum, the carry is simply the bitwise-or of the
 * carries of the individual numbers, since the sum of 3 bits can only ever
 * have a carry of one.
 *
 * We then observe that the average is then ((carry << 1) + sum) >> 1, or,
 * assuming eliminating overflows and underflows, carry + (sum >> 1).
 *
 * We now average our existing sum with the fourth number, so we get:
 * sum2 = (sum + d) >> 1 or (sum >> 1) + (d >> 1).
 *
 * We now observe that our sum has been moved into place relative to the
 * carry, so we can now average with the carry to get the final 4 input
 * average: avg = (sum2 + carry) >> 1;
 *
 * Or to reverse the proof:
 * avg = ((sum >> 1) + carry + d >> 1) >> 1
 * avg = ((a + b + c) >> 1 + d >> 1) >> 1
 * avg = ((a + b + c + d) >> 2)
 *
 * An additional fact used in the SSE versions is the concept that we can
 * trivially convert a rounded average to a truncated average:
 *
 * We have:
 * f(a, b) = (a + b + 1) >> 1
 *
 * And want:
 * g(a, b) = (a + b) >> 1
 *
 * Observe:
 * ~f(~a, ~b) == ~((~a + ~b + 1) >> 1)
 *            == ~((-a - 1 + -b - 1 + 1) >> 1)
 *            == ~((-a - 1 + -b) >> 1)
 *            == ~((-(a + b) - 1) >> 1)
 *            == ~((~(a + b)) >> 1)
 *            == (a + b) >> 1
 *            == g(a, b)
 */

MOZ_ALWAYS_INLINE __m128i _mm_not_si128(__m128i arg)
{
  __m128i minusone = _mm_set1_epi32(0xffffffff);
  return _mm_xor_si128(arg, minusone);
}

/* We have to pass pointers here, MSVC does not allow passing more than 3
 * __m128i arguments on the stack. And it does not allow 16-byte aligned
 * stack variables. This inlines properly on MSVC 2010. It does -not- inline
 * with just the inline directive.
 */
MOZ_ALWAYS_INLINE __m128i avg_sse2_8x2(__m128i *a, __m128i *b, __m128i *c, __m128i *d)
{
#define shuf1 _MM_SHUFFLE(2, 0, 2, 0)
#define shuf2 _MM_SHUFFLE(3, 1, 3, 1)

// This cannot be an inline function as the __Imm argument to _mm_shuffle_ps
// needs to be a compile time constant.
#define shuffle_si128(arga, argb, imm) \
  _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps((arga)), _mm_castsi128_ps((argb)), (imm)));

  __m128i t = shuffle_si128(*a, *b, shuf1);
  *b = shuffle_si128(*a, *b, shuf2);
  *a = t;
  t = shuffle_si128(*c, *d, shuf1);
  *d = shuffle_si128(*c, *d, shuf2);
  *c = t;

#undef shuf1
#undef shuf2
#undef shuffle_si128

  __m128i sum = _mm_xor_si128(*a, _mm_xor_si128(*b, *c));

  __m128i carry = _mm_or_si128(_mm_and_si128(*a, *b), _mm_or_si128(_mm_and_si128(*a, *c), _mm_and_si128(*b, *c)));

  sum = _mm_avg_epu8(_mm_not_si128(sum), _mm_not_si128(*d));

  return _mm_not_si128(_mm_avg_epu8(sum, _mm_not_si128(carry)));
}

MOZ_ALWAYS_INLINE __m128i avg_sse2_4x2_4x1(__m128i a, __m128i b)
{
  return _mm_not_si128(_mm_avg_epu8(_mm_not_si128(a), _mm_not_si128(b)));
}

MOZ_ALWAYS_INLINE __m128i avg_sse2_8x1_4x1(__m128i a, __m128i b)
{
  __m128i t = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b), _MM_SHUFFLE(3, 1, 3, 1)));
  b = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b), _MM_SHUFFLE(2, 0, 2, 0)));
  a = t;

  return _mm_not_si128(_mm_avg_epu8(_mm_not_si128(a), _mm_not_si128(b)));
}

MOZ_ALWAYS_INLINE uint32_t Avg2x2(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
  uint32_t sum = a ^ b ^ c;
  uint32_t carry = (a & b) | (a & c) | (b & c);

  uint32_t mask = 0xfefefefe;

  // Not having a byte based average instruction means we should mask to avoid
  // underflow.
  sum = (((sum ^ d) & mask) >> 1) + (sum & d);

  return (((sum ^ carry) & mask) >> 1) + (sum & carry);
}

// Simple 2 pixel average version of the function above.
MOZ_ALWAYS_INLINE uint32_t Avg2(uint32_t a, uint32_t b)
{
  uint32_t sum = a ^ b;
  uint32_t carry = (a & b);

  uint32_t mask = 0xfefefefe;

  return ((sum & mask) >> 1) + carry;
}

namespace mozilla {
namespace gfx {

void
ImageHalfScaler::HalfImage2D_SSE2(uint8_t *aSource, int32_t aSourceStride,
                                  const IntSize &aSourceSize, uint8_t *aDest,
                                  uint32_t aDestStride)
{
  const int Bpp = 4;

  for (int y = 0; y < aSourceSize.height; y += 2) {
    __m128i *storage = (__m128i*)(aDest + (y / 2) * aDestStride);
    int x = 0;
    // Run a loop depending on alignment.
    if (!(uintptr_t(aSource + (y * aSourceStride)) % 16) &&
        !(uintptr_t(aSource + ((y + 1) * aSourceStride)) % 16)) {
      for (; x < (aSourceSize.width - 7); x += 8) {
        __m128i *upperRow = (__m128i*)(aSource + (y * aSourceStride + x * Bpp));
        __m128i *lowerRow = (__m128i*)(aSource + ((y + 1) * aSourceStride + x * Bpp));

        __m128i a = _mm_load_si128(upperRow);
        __m128i b = _mm_load_si128(upperRow + 1);
        __m128i c = _mm_load_si128(lowerRow);
        __m128i d = _mm_load_si128(lowerRow + 1);

        *storage++ = avg_sse2_8x2(&a, &b, &c, &d);
      }
    } else if (!(uintptr_t(aSource + (y * aSourceStride)) % 16)) {
      for (; x < (aSourceSize.width - 7); x += 8) {
        __m128i *upperRow = (__m128i*)(aSource + (y * aSourceStride + x * Bpp));
        __m128i *lowerRow = (__m128i*)(aSource + ((y + 1) * aSourceStride + x * Bpp));

        __m128i a = _mm_load_si128(upperRow);
        __m128i b = _mm_load_si128(upperRow + 1);
        __m128i c = loadUnaligned128(lowerRow);
        __m128i d = loadUnaligned128(lowerRow + 1);

        *storage++ = avg_sse2_8x2(&a, &b, &c, &d);
      }
    } else if (!(uintptr_t(aSource + ((y + 1) * aSourceStride)) % 16)) {
      for (; x < (aSourceSize.width - 7); x += 8) {
        __m128i *upperRow = (__m128i*)(aSource + (y * aSourceStride + x * Bpp));
        __m128i *lowerRow = (__m128i*)(aSource + ((y + 1) * aSourceStride + x * Bpp));

        __m128i a = loadUnaligned128((__m128i*)upperRow);
        __m128i b = loadUnaligned128((__m128i*)upperRow + 1);
        __m128i c = _mm_load_si128((__m128i*)lowerRow);
        __m128i d = _mm_load_si128((__m128i*)lowerRow + 1);

        *storage++ = avg_sse2_8x2(&a, &b, &c, &d);
      }
    } else {
      for (; x < (aSourceSize.width - 7); x += 8) {
        __m128i *upperRow = (__m128i*)(aSource + (y * aSourceStride + x * Bpp));
        __m128i *lowerRow = (__m128i*)(aSource + ((y + 1) * aSourceStride + x * Bpp));

        __m128i a = loadUnaligned128(upperRow);
        __m128i b = loadUnaligned128(upperRow + 1);
        __m128i c = loadUnaligned128(lowerRow);
        __m128i d = loadUnaligned128(lowerRow + 1);

        *storage++ = avg_sse2_8x2(&a, &b, &c, &d);
      }
    }

    uint32_t *unalignedStorage = (uint32_t*)storage;
    // Take care of the final pixels, we know there's an even number of pixels
    // in the source rectangle. We use a 2x2 'simd' implementation for this.
    //
    // Potentially we only have to do this in the last row since overflowing 
    // 8 pixels in an earlier row would appear to be harmless as it doesn't
    // touch invalid memory. Even when reading and writing to the same surface.
    // in practice we only do this when doing an additional downscale pass, and
    // in this situation we have unused stride to write into harmlessly.
    // I do not believe the additional code complexity would be worth it though.
    for (; x < aSourceSize.width; x += 2) {
      uint8_t *upperRow = aSource + (y * aSourceStride + x * Bpp);
      uint8_t *lowerRow = aSource + ((y + 1) * aSourceStride + x * Bpp);

      *unalignedStorage++ = Avg2x2(*(uint32_t*)upperRow, *((uint32_t*)upperRow + 1),
                                   *(uint32_t*)lowerRow, *((uint32_t*)lowerRow + 1));
    }
  }
}

void
ImageHalfScaler::HalfImageVertical_SSE2(uint8_t *aSource, int32_t aSourceStride,
                                        const IntSize &aSourceSize, uint8_t *aDest,
                                        uint32_t aDestStride)
{
  for (int y = 0; y < aSourceSize.height; y += 2) {
    __m128i *storage = (__m128i*)(aDest + (y / 2) * aDestStride);
    int x = 0;
    // Run a loop depending on alignment.
    if (!(uintptr_t(aSource + (y * aSourceStride)) % 16) &&
        !(uintptr_t(aSource + ((y + 1) * aSourceStride)) % 16)) {
      for (; x < (aSourceSize.width - 3); x += 4) {
        uint8_t *upperRow = aSource + (y * aSourceStride + x * 4);
        uint8_t *lowerRow = aSource + ((y + 1) * aSourceStride + x * 4);

        __m128i a = _mm_load_si128((__m128i*)upperRow);
        __m128i b = _mm_load_si128((__m128i*)lowerRow);

        *storage++ = avg_sse2_4x2_4x1(a, b);
      }
    } else if (!(uintptr_t(aSource + (y * aSourceStride)) % 16)) {
      // This line doesn't align well.
      for (; x < (aSourceSize.width - 3); x += 4) {
        uint8_t *upperRow = aSource + (y * aSourceStride + x * 4);
        uint8_t *lowerRow = aSource + ((y + 1) * aSourceStride + x * 4);

        __m128i a = _mm_load_si128((__m128i*)upperRow);
        __m128i b = loadUnaligned128((__m128i*)lowerRow);

        *storage++ = avg_sse2_4x2_4x1(a, b);
      }
    } else if (!(uintptr_t(aSource + ((y + 1) * aSourceStride)) % 16)) {
      for (; x < (aSourceSize.width - 3); x += 4) {
        uint8_t *upperRow = aSource + (y * aSourceStride + x * 4);
        uint8_t *lowerRow = aSource + ((y + 1) * aSourceStride + x * 4);

        __m128i a = loadUnaligned128((__m128i*)upperRow);
        __m128i b = _mm_load_si128((__m128i*)lowerRow);

        *storage++ = avg_sse2_4x2_4x1(a, b);
      }
    } else {
      for (; x < (aSourceSize.width - 3); x += 4) {
        uint8_t *upperRow = aSource + (y * aSourceStride + x * 4);
        uint8_t *lowerRow = aSource + ((y + 1) * aSourceStride + x * 4);

        __m128i a = loadUnaligned128((__m128i*)upperRow);
        __m128i b = loadUnaligned128((__m128i*)lowerRow);

        *storage++ = avg_sse2_4x2_4x1(a, b);
      }
    }

    uint32_t *unalignedStorage = (uint32_t*)storage;
    // Take care of the final pixels, we know there's an even number of pixels
    // in the source rectangle.
    //
    // Similar overflow considerations are valid as in the previous function.
    for (; x < aSourceSize.width; x++) {
      uint8_t *upperRow = aSource + (y * aSourceStride + x * 4);
      uint8_t *lowerRow = aSource + ((y + 1) * aSourceStride + x * 4);

      *unalignedStorage++ = Avg2(*(uint32_t*)upperRow, *(uint32_t*)lowerRow);
    }
  }
}

void
ImageHalfScaler::HalfImageHorizontal_SSE2(uint8_t *aSource, int32_t aSourceStride,
                                          const IntSize &aSourceSize, uint8_t *aDest,
                                          uint32_t aDestStride)
{
  for (int y = 0; y < aSourceSize.height; y++) {
    __m128i *storage = (__m128i*)(aDest + (y * aDestStride));
    int x = 0;
    // Run a loop depending on alignment.
    if (!(uintptr_t(aSource + (y * aSourceStride)) % 16)) {
      for (; x < (aSourceSize.width - 7); x += 8) {
        __m128i* pixels = (__m128i*)(aSource + (y * aSourceStride + x * 4));

        __m128i a = _mm_load_si128(pixels);
        __m128i b = _mm_load_si128(pixels + 1);

        *storage++ = avg_sse2_8x1_4x1(a, b);
      }
    } else {
      for (; x < (aSourceSize.width - 7); x += 8) {
        __m128i* pixels = (__m128i*)(aSource + (y * aSourceStride + x * 4));

        __m128i a = loadUnaligned128(pixels);
        __m128i b = loadUnaligned128(pixels + 1);

        *storage++ = avg_sse2_8x1_4x1(a, b);
      }
    }

    uint32_t *unalignedStorage = (uint32_t*)storage;
    // Take care of the final pixels, we know there's an even number of pixels
    // in the source rectangle.
    //
    // Similar overflow considerations are valid as in the previous function.
    for (; x < aSourceSize.width; x += 2) {
      uint32_t *pixels = (uint32_t*)(aSource + (y * aSourceStride + x * 4));

      *unalignedStorage++ = Avg2(*pixels, *(pixels + 1));
    }
  }
}

} // namespace gfx
} // namespace mozilla
