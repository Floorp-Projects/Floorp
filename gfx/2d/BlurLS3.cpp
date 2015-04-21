/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Blur.h"

#include <string.h>

#ifdef _MIPS_ARCH_LOONGSON3A

#include "MMIHelpers.h"

namespace mozilla {
namespace gfx {

typedef struct { double l; double h; } __m128i;

MOZ_ALWAYS_INLINE
__m128i loadUnaligned128(__m128i *p)
{
  __m128i v;

  asm volatile (
    ".set push \n\t"
    ".set arch=loongson3a \n\t"
    "gsldlc1 %[vh], 0xf(%[p]) \n\t"
    "gsldrc1 %[vh], 0x8(%[p]) \n\t"
    "gsldlc1 %[vl], 0x7(%[p]) \n\t"
    "gsldrc1 %[vl], 0x0(%[p]) \n\t"
    ".set pop \n\t"
    :[vh]"=f"(v.h), [vl]"=f"(v.l)
    :[p]"r"(p)
    :"memory"
  );

  return v;
}

MOZ_ALWAYS_INLINE
__m128i Divide(__m128i aValues, __m128i aDivisor)
{
  uint64_t tmp;
  double srl32;
  __m128i mask, ra, p4321, t1, t2;

  asm volatile (
    ".set push \n\t"
    ".set arch=loongson3a \n\t"
    "li %[tmp], 0x80000000 \n\t"
    "mtc1 %[tmp], %[ral] \n\t"
    "xor %[maskl], %[maskl], %[maskl] \n\t"
    "mov.d %[rah], %[ral] \n\t"
    "li %[tmp], 0xffffffff \n\t"
    "mthc1 %[tmp], %[maskl] \n\t"
    "mov.d %[maskh], %[maskl] \n\t"
    ".set pop \n\t"
    :[rah]"=f"(ra.h), [ral]"=f"(ra.l),
     [maskh]"=f"(mask.h), [maskl]"=f"(mask.l),
     [tmp]"=&r"(tmp)
  );

  asm volatile (
    ".set push \n\t"
    ".set arch=loongson3a \n\t"
    "ori %[tmp], $0, 32 \n\t"
    "mtc1 %[tmp], %[srl32] \n\t"
    _mm_pmuluw(t1, av, ad)
    _mm_psrld(t2, av, srl32)
    _mm_pmuluw(t2, t2, ad)
    // Add 1 << 31 before shifting or masking the lower 32 bits away, so that the
    // result is rounded.
    _mm_paddd(t1, t1, ra)
    _mm_psrld(t1, t1, srl32)
    _mm_paddd(t2, t2, ra)
    _mm_and(t2, t2, mask)
    _mm_or(p4321, t1, t2)
    ".set pop \n\t"
    :[p4321h]"=&f"(p4321.h), [p4321l]"=&f"(p4321.l),
     [t1h]"=&f"(t1.h), [t1l]"=&f"(t1.l),
     [t2h]"=&f"(t2.h), [t2l]"=&f"(t2.l),
     [srl32]"=&f"(srl32), [tmp]"=&r"(tmp)
    :[rah]"f"(ra.h), [ral]"f"(ra.l),
     [maskh]"f"(mask.h), [maskl]"f"(mask.l),
     [avh]"f"(aValues.h), [avl]"f"(aValues.l),
     [adh]"f"(aDivisor.h), [adl]"f"(aDivisor.l)
  );

  return p4321;
}

MOZ_ALWAYS_INLINE
__m128i BlurFourPixels(const __m128i& aTopLeft, const __m128i& aTopRight,
                       const __m128i& aBottomRight, const __m128i& aBottomLeft,
                       const __m128i& aDivisor)
{
  __m128i values;

  asm volatile (
    ".set push \n\t"
    ".set arch=loongson3a \n\t"
    _mm_psubw(val, abr, atr)
    _mm_psubw(val, val, abl)
    _mm_paddw(val, val, atl)
    ".set pop \n\t"
    :[valh]"=&f"(values.h), [vall]"=&f"(values.l)
    :[abrh]"f"(aBottomRight.h), [abrl]"f"(aBottomRight.l),
     [atrh]"f"(aTopRight.h), [atrl]"f"(aTopRight.l),
     [ablh]"f"(aBottomLeft.h), [abll]"f"(aBottomLeft.l),
     [atlh]"f"(aTopLeft.h), [atll]"f"(aTopLeft.l)
  );

  return Divide(values, aDivisor);
}

MOZ_ALWAYS_INLINE
void LoadIntegralRowFromRow(uint32_t *aDest, const uint8_t *aSource,
                            int32_t aSourceWidth, int32_t aLeftInflation,
                            int32_t aRightInflation)
{
  int32_t currentRowSum = 0;

  for (int x = 0; x < aLeftInflation; x++) {
    currentRowSum += aSource[0];
    aDest[x] = currentRowSum;
  }
  for (int x = aLeftInflation; x < (aSourceWidth + aLeftInflation); x++) {
    currentRowSum += aSource[(x - aLeftInflation)];
    aDest[x] = currentRowSum;
  }
  for (int x = (aSourceWidth + aLeftInflation); x < (aSourceWidth + aLeftInflation + aRightInflation); x++) {
    currentRowSum += aSource[aSourceWidth - 1];
    aDest[x] = currentRowSum;
  }
}

// This function calculates an integral of four pixels stored in the 4
// 32-bit integers on aPixels. i.e. for { 30, 50, 80, 100 } this returns
// { 30, 80, 160, 260 }. This seems to be the fastest way to do this after
// much testing.
MOZ_ALWAYS_INLINE
__m128i AccumulatePixelSums(__m128i aPixels)
{
  uint64_t tr;
  double tmp, s4, s64;
  __m128i sumPixels, currentPixels, zero;

  asm volatile (
    ".set push \n\t"
    ".set arch=loongson3a \n\t"
    _mm_xor(z, z, z)
    "li %[tr], 64 \n\t"
    "mtc1 %[tr], %[s64] \n\t"
    "li %[tr], 32 \n\t"
    "mtc1 %[tr], %[s4] \n\t"
    _mm_psllq(cp, ap, s4, s64, t)
    _mm_paddw(sp, ap, cp)
    _mm_punpckldq(cp, z, sp)
    _mm_paddw(sp, sp, cp)
    ".set pop \n\t"
    :[sph]"=&f"(sumPixels.h), [spl]"=&f"(sumPixels.l),
     [cph]"=&f"(currentPixels.h), [cpl]"=&f"(currentPixels.l),
     [zh]"=&f"(zero.h), [zl]"=&f"(zero.l),
     [s4]"=&f"(s4), [s64]"=&f"(s64), [t]"=&f"(tmp), [tr]"=&r"(tr)
    :[aph]"f"(aPixels.h), [apl]"f"(aPixels.l)
  );

  return sumPixels;
}

MOZ_ALWAYS_INLINE
void GenerateIntegralImage_LS3(int32_t aLeftInflation, int32_t aRightInflation,
                           int32_t aTopInflation, int32_t aBottomInflation,
                           uint32_t *aIntegralImage, size_t aIntegralImageStride,
                           uint8_t *aSource, int32_t aSourceStride, const IntSize &aSize)
{
  MOZ_ASSERT(!(aLeftInflation & 3));

  uint32_t stride32bit = aIntegralImageStride / 4;

  IntSize integralImageSize(aSize.width + aLeftInflation + aRightInflation,
                            aSize.height + aTopInflation + aBottomInflation);

  LoadIntegralRowFromRow(aIntegralImage, aSource, aSize.width, aLeftInflation, aRightInflation);

  for (int y = 1; y < aTopInflation + 1; y++) {
    uint32_t *intRow = aIntegralImage + (y * stride32bit);
    uint32_t *intPrevRow = aIntegralImage + (y - 1) * stride32bit;
    uint32_t *intFirstRow = aIntegralImage;

    for (int x = 0; x < integralImageSize.width; x += 4) {
      __m128i firstRow, previousRow;

      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        "gslqc1 %[frh], %[frl], (%[fr]) \n\t"
        "gslqc1 %[prh], %[prl], (%[pr]) \n\t"
        _mm_paddw(fr, fr, pr)
        "gssqc1 %[frh], %[frl], (%[r]) \n\t"
        ".set pop \n\t"
        :[frh]"=&f"(firstRow.h), [frl]"=&f"(firstRow.l),
         [prh]"=&f"(previousRow.h), [prl]"=&f"(previousRow.l)
        :[fr]"r"(intFirstRow + x), [pr]"r"(intPrevRow + x),
         [r]"r"(intRow + x)
        :"memory"
      );
    }
  }

  uint64_t tmp;
  double s44, see;
  __m128i zero;
  asm volatile (
    ".set push \n\t"
    ".set arch=loongson3a \n\t"
    "li %[tmp], 0xee \n\t"
    "mtc1 %[tmp], %[see] \n\t"
    "li %[tmp], 0x44 \n\t"
    "mtc1 %[tmp], %[s44] \n\t"
    _mm_xor(zero, zero, zero)
    ".set pop \n\t"
    :[tmp]"=&r"(tmp), [s44]"=f"(s44), [see]"=f"(see),
     [zeroh]"=f"(zero.h), [zerol]"=f"(zero.l)
  );
  for (int y = aTopInflation + 1; y < (aSize.height + aTopInflation); y++) {
    __m128i currentRowSum;
    uint32_t *intRow = aIntegralImage + (y * stride32bit);
    uint32_t *intPrevRow = aIntegralImage + (y - 1) * stride32bit;
    uint8_t *sourceRow = aSource + aSourceStride * (y - aTopInflation);
    uint32_t pixel = sourceRow[0];

    asm volatile (
      ".set push \n\t"
      ".set arch=loongson3a \n\t"
      _mm_xor(cr, cr, cr)
      ".set pop \n\t"
      :[crh]"=f"(currentRowSum.h), [crl]"=f"(currentRowSum.l)
    );
    for (int x = 0; x < aLeftInflation; x += 4) {
      __m128i sumPixels, t;
      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        "mtc1 %[pix], %[spl] \n\t"
        "punpcklwd %[spl], %[spl], %[spl] \n\t"
        "mov.d %[sph], %[spl] \n\t"
        "pshufh %[sph], %[spl], %[s44] \n\t"
        "pshufh %[spl], %[spl], %[s44] \n\t"
        ".set pop \n\t"
        :[sph]"=&f"(sumPixels.h), [spl]"=&f"(sumPixels.l)
        :[pix]"r"(pixel), [s44]"f"(s44)
      );
      sumPixels = AccumulatePixelSums(sumPixels);
      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        _mm_paddw(sp, sp, cr)
        "pshufh %[crh], %[sph], %[see] \n\t"
        "pshufh %[crl], %[sph], %[see] \n\t"
        "gslqc1 %[th], %[tl], (%[pr]) \n\t"
        _mm_paddw(t, sp, t)
        "gssqc1 %[th], %[tl], (%[r]) \n\t"
        ".set pop \n\t"
        :[th]"=&f"(t.h), [tl]"=&f"(t.l),
         [sph]"+f"(sumPixels.h), [spl]"+f"(sumPixels.l),
         [crh]"+f"(currentRowSum.h), [crl]"+f"(currentRowSum.l)
        :[r]"r"(intRow + x), [pr]"r"(intPrevRow + x), [see]"f"(see)
        :"memory"
      );
    }
    for (int x = aLeftInflation; x < (aSize.width + aLeftInflation); x += 4) {
      uint32_t pixels = *(uint32_t*)(sourceRow + (x - aLeftInflation));
      __m128i sumPixels, t;

      // It's important to shuffle here. When we exit this loop currentRowSum
      // has to be set to sumPixels, so that the following loop can get the
      // correct pixel for the currentRowSum. The highest order pixel in
      // currentRowSum could've originated from accumulation in the stride.
      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        "pshufh %[crl], %[crh], %[see] \n\t"
        "pshufh %[crh], %[crh], %[see] \n\t"
        "mtc1 %[pix], %[spl] \n\t"
        "punpcklwd %[spl], %[spl], %[spl] \n\t"
        "mov.d %[sph], %[spl] \n\t"
        _mm_punpcklbh(sp, sp, zero)
        _mm_punpcklhw(sp, sp, zero)
        ".set pop \n\t"
        :[sph]"=&f"(sumPixels.h), [spl]"=&f"(sumPixels.l),
         [crh]"+f"(currentRowSum.h), [crl]"+f"(currentRowSum.l)
        :[pix]"r"(pixels), [see]"f"(see),
         [zeroh]"f"(zero.h), [zerol]"f"(zero.l)
      );
      sumPixels = AccumulatePixelSums(sumPixels);
      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        _mm_paddw(sp, sp, cr)
        "mov.d %[crh], %[sph] \n\t"
        "mov.d %[crl], %[spl] \n\t"
        "gslqc1 %[th], %[tl], (%[pr]) \n\t"
        _mm_paddw(t, sp, t)
        "gssqc1 %[th], %[tl], (%[r]) \n\t"
        ".set pop \n\t"
        :[th]"=&f"(t.h), [tl]"=&f"(t.l),
         [sph]"+f"(sumPixels.h), [spl]"+f"(sumPixels.l),
         [crh]"+f"(currentRowSum.h), [crl]"+f"(currentRowSum.l)
        :[r]"r"(intRow + x), [pr]"r"(intPrevRow + x)
        :"memory"
      );
    }

    pixel = sourceRow[aSize.width - 1];
    int x = (aSize.width + aLeftInflation);
    if ((aSize.width & 3)) {
      // Deal with unaligned portion. Get the correct pixel from currentRowSum,
      // see explanation above.
      uint32_t intCurrentRowSum = ((uint32_t*)&currentRowSum)[(aSize.width % 4) - 1];
      for (; x < integralImageSize.width; x++) {
        // We could be unaligned here!
        if (!(x & 3)) {
          // aligned!
          asm volatile (
            ".set push \n\t"
            ".set arch=loongson3a \n\t"
            "mtc1 %[cr], %[crl] \n\t"
            "punpcklwd %[crl], %[crl], %[crl] \n\t"
            "mov.d %[crh], %[crl] \n\t"
            ".set pop \n\t"
            :[crh]"=f"(currentRowSum.h), [crl]"=f"(currentRowSum.l)
            :[cr]"r"(intCurrentRowSum)
          );
          break;
        }
        intCurrentRowSum += pixel;
        intRow[x] = intPrevRow[x] + intCurrentRowSum;
      }
    } else {
      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        "pshufh %[crl], %[crh], %[see] \n\t"
        "pshufh %[crh], %[crh], %[see] \n\t"
        ".set pop \n\t"
        :[crh]"+f"(currentRowSum.h), [crl]"+f"(currentRowSum.l)
        :[see]"f"(see)
      );
    }
    for (; x < integralImageSize.width; x += 4) {
      __m128i sumPixels, t;
      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        "mtc1 %[pix], %[spl] \n\t"
        "punpcklwd %[spl], %[spl], %[spl] \n\t"
        "mov.d %[sph], %[spl] \n\t"
        ".set pop \n\t"
        :[sph]"=f"(sumPixels.h), [spl]"=f"(sumPixels.l)
        :[pix]"r"(pixel)
      );
      sumPixels = AccumulatePixelSums(sumPixels);
      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        _mm_paddw(sp, sp, cr)
        "pshufh %[crh], %[sph], %[see] \n\t"
        "pshufh %[crl], %[sph], %[see] \n\t"
        "gslqc1 %[th], %[tl], (%[pr]) \n\t"
        _mm_paddw(t, sp, t)
        "gssqc1 %[th], %[tl], (%[r]) \n\t"
        ".set pop \n\t"
        :[th]"=&f"(t.h), [tl]"=&f"(t.l),
         [sph]"+f"(sumPixels.h), [spl]"+f"(sumPixels.l),
         [crh]"+f"(currentRowSum.h), [crl]"+f"(currentRowSum.l)
        :[r]"r"(intRow + x), [pr]"r"(intPrevRow + x), [see]"f"(see)
        :"memory"
      );
    }
  }

  if (aBottomInflation) {
    // Store the last valid row of our source image in the last row of
    // our integral image. This will be overwritten with the correct values
    // in the upcoming loop.
    LoadIntegralRowFromRow(aIntegralImage + (integralImageSize.height - 1) * stride32bit,
                           aSource + (aSize.height - 1) * aSourceStride, aSize.width, aLeftInflation, aRightInflation);


    for (int y = aSize.height + aTopInflation; y < integralImageSize.height; y++) {
      __m128i *intRow = (__m128i*)(aIntegralImage + (y * stride32bit));
      __m128i *intPrevRow = (__m128i*)(aIntegralImage + (y - 1) * stride32bit);
      __m128i *intLastRow = (__m128i*)(aIntegralImage + (integralImageSize.height - 1) * stride32bit);

      for (int x = 0; x < integralImageSize.width; x += 4) {
        __m128i t1, t2;
        asm volatile (
          ".set push \n\t"
          ".set arch=loongson3a \n\t"
          "gslqc1 %[t1h], %[t1l], (%[lr]) \n\t"
          "gslqc1 %[t2h], %[t2l], (%[pr]) \n\t"
          _mm_paddw(t1, t1, t2)
          "gssqc1 %[t1h], %[t1l], (%[r]) \n\t"
          ".set pop \n\t"
          :[t1h]"=&f"(t1.h), [t1l]"=&f"(t1.l),
           [t2h]"=&f"(t2.h), [t2l]"=&f"(t2.l)
          :[r]"r"(intRow + (x / 4)),
           [lr]"r"(intLastRow + (x / 4)),
           [pr]"r"(intPrevRow + (x / 4))
          :"memory"
      );
      }
    }
  }
}

/**
 * Attempt to do an in-place box blur using an integral image.
 */
void
AlphaBoxBlur::BoxBlur_LS3(uint8_t* aData,
                           int32_t aLeftLobe,
                           int32_t aRightLobe,
                           int32_t aTopLobe,
                           int32_t aBottomLobe,
                           uint32_t *aIntegralImage,
                           size_t aIntegralImageStride)
{
  IntSize size = GetSize();

  MOZ_ASSERT(size.height > 0);

  // Our 'left' or 'top' lobe will include the current pixel. i.e. when
  // looking at an integral image the value of a pixel at 'x,y' is calculated
  // using the value of the integral image values above/below that.
  aLeftLobe++;
  aTopLobe++;
  int32_t boxSize = (aLeftLobe + aRightLobe) * (aTopLobe + aBottomLobe);

  MOZ_ASSERT(boxSize > 0);

  if (boxSize == 1) {
      return;
  }

  uint32_t reciprocal = uint32_t((uint64_t(1) << 32) / boxSize);

  uint32_t stride32bit = aIntegralImageStride / 4;
  int32_t leftInflation = RoundUpToMultipleOf4(aLeftLobe).value();

  GenerateIntegralImage_LS3(leftInflation, aRightLobe, aTopLobe, aBottomLobe,
                             aIntegralImage, aIntegralImageStride, aData,
                             mStride, size);

  __m128i divisor, zero;
  asm volatile (
    ".set push \n\t"
    ".set arch=loongson3a \n\t"
    "mtc1 %[rec], %[divl] \n\t"
    "punpcklwd %[divl], %[divl], %[divl] \n\t"
    "mov.d %[divh], %[divl] \n\t"
    _mm_xor(zero, zero, zero)
    ".set pop \n\t"
    :[divh]"=f"(divisor.h), [divl]"=f"(divisor.l),
     [zeroh]"=f"(zero.h), [zerol]"=f"(zero.l)
    :[rec]"r"(reciprocal)
  );

  // This points to the start of the rectangle within the IntegralImage that overlaps
  // the surface being blurred.
  uint32_t *innerIntegral = aIntegralImage + (aTopLobe * stride32bit) + leftInflation;

  IntRect skipRect = mSkipRect;
  int32_t stride = mStride;
  uint8_t *data = aData;
  for (int32_t y = 0; y < size.height; y++) {
    bool inSkipRectY = y > skipRect.y && y < skipRect.YMost();

    uint32_t *topLeftBase = innerIntegral + ((y - aTopLobe) * ptrdiff_t(stride32bit) - aLeftLobe);
    uint32_t *topRightBase = innerIntegral + ((y - aTopLobe) * ptrdiff_t(stride32bit) + aRightLobe);
    uint32_t *bottomRightBase = innerIntegral + ((y + aBottomLobe) * ptrdiff_t(stride32bit) + aRightLobe);
    uint32_t *bottomLeftBase = innerIntegral + ((y + aBottomLobe) * ptrdiff_t(stride32bit) - aLeftLobe);

    int32_t x = 0;
    // Process 16 pixels at a time for as long as possible.
    for (; x <= size.width - 16; x += 16) {
      if (inSkipRectY && x > skipRect.x && x < skipRect.XMost()) {
        x = skipRect.XMost() - 16;
        // Trigger early jump on coming loop iterations, this will be reset
        // next line anyway.
        inSkipRectY = false;
        continue;
      }

      __m128i topLeft;
      __m128i topRight;
      __m128i bottomRight;
      __m128i bottomLeft;

      topLeft = loadUnaligned128((__m128i*)(topLeftBase + x));
      topRight = loadUnaligned128((__m128i*)(topRightBase + x));
      bottomRight = loadUnaligned128((__m128i*)(bottomRightBase + x));
      bottomLeft = loadUnaligned128((__m128i*)(bottomLeftBase + x));
      __m128i result1 = BlurFourPixels(topLeft, topRight, bottomRight, bottomLeft, divisor);

      topLeft = loadUnaligned128((__m128i*)(topLeftBase + x + 4));
      topRight = loadUnaligned128((__m128i*)(topRightBase + x + 4));
      bottomRight = loadUnaligned128((__m128i*)(bottomRightBase + x + 4));
      bottomLeft = loadUnaligned128((__m128i*)(bottomLeftBase + x + 4));
      __m128i result2 = BlurFourPixels(topLeft, topRight, bottomRight, bottomLeft, divisor);

      topLeft = loadUnaligned128((__m128i*)(topLeftBase + x + 8));
      topRight = loadUnaligned128((__m128i*)(topRightBase + x + 8));
      bottomRight = loadUnaligned128((__m128i*)(bottomRightBase + x + 8));
      bottomLeft = loadUnaligned128((__m128i*)(bottomLeftBase + x + 8));
      __m128i result3 = BlurFourPixels(topLeft, topRight, bottomRight, bottomLeft, divisor);

      topLeft = loadUnaligned128((__m128i*)(topLeftBase + x + 12));
      topRight = loadUnaligned128((__m128i*)(topRightBase + x + 12));
      bottomRight = loadUnaligned128((__m128i*)(bottomRightBase + x + 12));
      bottomLeft = loadUnaligned128((__m128i*)(bottomLeftBase + x + 12));
      __m128i result4 = BlurFourPixels(topLeft, topRight, bottomRight, bottomLeft, divisor);

      double t;
      __m128i final;
      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        _mm_packsswh(r3, r3, r4, t)
        _mm_packsswh(f, r1, r2, t)
        _mm_packushb(f, f, r3, t)
        "gssdlc1 %[fh], 0xf(%[d]) \n\t"
        "gssdrc1 %[fh], 0x8(%[d]) \n\t"
        "gssdlc1 %[fl], 0x7(%[d]) \n\t"
        "gssdrc1 %[fl], 0x0(%[d]) \n\t"
        ".set pop \n\t"
        :[fh]"=&f"(final.h), [fl]"=&f"(final.l),
         [r3h]"+f"(result3.h), [r3l]"+f"(result3.l),
         [t]"=&f"(t)
        :[r1h]"f"(result1.h), [r1l]"f"(result1.l),
         [r2h]"f"(result2.h), [r2l]"f"(result2.l),
         [r4h]"f"(result4.h), [r4l]"f"(result4.l),
         [d]"r"(data + stride * y + x)
        :"memory"
      );
    }

    // Process the remaining pixels 4 bytes at a time.
    for (; x < size.width; x += 4) {
      if (inSkipRectY && x > skipRect.x && x < skipRect.XMost()) {
        x = skipRect.XMost() - 4;
        // Trigger early jump on coming loop iterations, this will be reset
        // next line anyway.
        inSkipRectY = false;
        continue;
      }
      __m128i topLeft = loadUnaligned128((__m128i*)(topLeftBase + x));
      __m128i topRight = loadUnaligned128((__m128i*)(topRightBase + x));
      __m128i bottomRight = loadUnaligned128((__m128i*)(bottomRightBase + x));
      __m128i bottomLeft = loadUnaligned128((__m128i*)(bottomLeftBase + x));

      __m128i result = BlurFourPixels(topLeft, topRight, bottomRight, bottomLeft, divisor);

      double t;
      __m128i final;
      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        _mm_packsswh(f, r, zero, t)
        _mm_packushb(f, f, zero, t)
        "swc1 %[fl], (%[d]) \n\t"
        ".set pop \n\t"
        :[fh]"=&f"(final.h), [fl]"=&f"(final.l),
         [t]"=&f"(t)
        :[d]"r"(data + stride * y + x),
         [rh]"f"(result.h), [rl]"f"(result.l),
         [zeroh]"f"(zero.h), [zerol]"f"(zero.l)
        :"memory"
      );
    }
  }

}

}
}

#endif /* _MIPS_ARCH_LOONGSON3A */
