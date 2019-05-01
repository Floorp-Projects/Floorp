/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Blur.h"
#include <arm_neon.h>

namespace mozilla {
namespace gfx {

MOZ_ALWAYS_INLINE
uint16x4_t Divide(uint32x4_t aValues, uint32x2_t aDivisor) {
  uint64x2_t roundingAddition = vdupq_n_u64(int64_t(1) << 31);
  uint64x2_t multiplied21 = vmull_u32(vget_low_u32(aValues), aDivisor);
  uint64x2_t multiplied43 = vmull_u32(vget_high_u32(aValues), aDivisor);
  return vqmovn_u32(
      vcombine_u32(vshrn_n_u64(vaddq_u64(multiplied21, roundingAddition), 32),
                   vshrn_n_u64(vaddq_u64(multiplied43, roundingAddition), 32)));
}

MOZ_ALWAYS_INLINE
uint16x4_t BlurFourPixels(const uint32x4_t& aTopLeft,
                          const uint32x4_t& aTopRight,
                          const uint32x4_t& aBottomRight,
                          const uint32x4_t& aBottomLeft,
                          const uint32x2_t& aDivisor) {
  uint32x4_t values = vaddq_u32(
      vsubq_u32(vsubq_u32(aBottomRight, aTopRight), aBottomLeft), aTopLeft);
  return Divide(values, aDivisor);
}

MOZ_ALWAYS_INLINE
void LoadIntegralRowFromRow(uint32_t* aDest, const uint8_t* aSource,
                            int32_t aSourceWidth, int32_t aLeftInflation,
                            int32_t aRightInflation) {
  int32_t currentRowSum = 0;

  for (int x = 0; x < aLeftInflation; x++) {
    currentRowSum += aSource[0];
    aDest[x] = currentRowSum;
  }
  for (int x = aLeftInflation; x < (aSourceWidth + aLeftInflation); x++) {
    currentRowSum += aSource[(x - aLeftInflation)];
    aDest[x] = currentRowSum;
  }
  for (int x = (aSourceWidth + aLeftInflation);
       x < (aSourceWidth + aLeftInflation + aRightInflation); x++) {
    currentRowSum += aSource[aSourceWidth - 1];
    aDest[x] = currentRowSum;
  }
}

MOZ_ALWAYS_INLINE void GenerateIntegralImage_NEON(
    int32_t aLeftInflation, int32_t aRightInflation, int32_t aTopInflation,
    int32_t aBottomInflation, uint32_t* aIntegralImage,
    size_t aIntegralImageStride, uint8_t* aSource, int32_t aSourceStride,
    const IntSize& aSize) {
  MOZ_ASSERT(!(aLeftInflation & 3));

  uint32_t stride32bit = aIntegralImageStride / 4;
  IntSize integralImageSize(aSize.width + aLeftInflation + aRightInflation,
                            aSize.height + aTopInflation + aBottomInflation);

  LoadIntegralRowFromRow(aIntegralImage, aSource, aSize.width, aLeftInflation,
                         aRightInflation);

  for (int y = 1; y < aTopInflation + 1; y++) {
    uint32_t* intRow = aIntegralImage + (y * stride32bit);
    uint32_t* intPrevRow = aIntegralImage + (y - 1) * stride32bit;
    uint32_t* intFirstRow = aIntegralImage;

    for (int x = 0; x < integralImageSize.width; x += 4) {
      uint32x4_t firstRow = vld1q_u32(intFirstRow + x);
      uint32x4_t previousRow = vld1q_u32(intPrevRow + x);
      vst1q_u32(intRow + x, vaddq_u32(firstRow, previousRow));
    }
  }

  for (int y = aTopInflation + 1; y < (aSize.height + aTopInflation); y++) {
    uint32x4_t currentRowSum = vdupq_n_u32(0);
    uint32_t* intRow = aIntegralImage + (y * stride32bit);
    uint32_t* intPrevRow = aIntegralImage + (y - 1) * stride32bit;
    uint8_t* sourceRow = aSource + aSourceStride * (y - aTopInflation);

    uint32_t pixel = sourceRow[0];
    for (int x = 0; x < aLeftInflation; x += 4) {
      uint32_t temp[4];
      temp[0] = pixel;
      temp[1] = temp[0] + pixel;
      temp[2] = temp[1] + pixel;
      temp[3] = temp[2] + pixel;
      uint32x4_t sumPixels = vld1q_u32(temp);
      sumPixels = vaddq_u32(sumPixels, currentRowSum);
      currentRowSum = vdupq_n_u32(vgetq_lane_u32(sumPixels, 3));
      vst1q_u32(intRow + x, vaddq_u32(sumPixels, vld1q_u32(intPrevRow + x)));
    }

    for (int x = aLeftInflation; x < (aSize.width + aLeftInflation); x += 4) {
      // It's important to shuffle here. When we exit this loop currentRowSum
      // has to be set to sumPixels, so that the following loop can get the
      // correct pixel for the currentRowSum. The highest order pixel in
      // currentRowSum could've originated from accumulation in the stride.
      currentRowSum = vdupq_n_u32(vgetq_lane_u32(currentRowSum, 3));

      uint32_t temp[4];
      temp[0] = *(sourceRow + (x - aLeftInflation));
      temp[1] = temp[0] + *(sourceRow + (x - aLeftInflation) + 1);
      temp[2] = temp[1] + *(sourceRow + (x - aLeftInflation) + 2);
      temp[3] = temp[2] + *(sourceRow + (x - aLeftInflation) + 3);
      uint32x4_t sumPixels = vld1q_u32(temp);
      sumPixels = vaddq_u32(sumPixels, currentRowSum);
      currentRowSum = sumPixels;
      vst1q_u32(intRow + x, vaddq_u32(sumPixels, vld1q_u32(intPrevRow + x)));
    }

    pixel = sourceRow[aSize.width - 1];
    int x = (aSize.width + aLeftInflation);
    if ((aSize.width & 3)) {
      // Deal with unaligned portion. Get the correct pixel from currentRowSum,
      // see explanation above.
      uint32_t intCurrentRowSum =
          ((uint32_t*)&currentRowSum)[(aSize.width % 4) - 1];
      for (; x < integralImageSize.width; x++) {
        // We could be unaligned here!
        if (!(x & 3)) {
          // aligned!
          currentRowSum = vdupq_n_u32(intCurrentRowSum);
          break;
        }
        intCurrentRowSum += pixel;
        intRow[x] = intPrevRow[x] + intCurrentRowSum;
      }
    } else {
      currentRowSum = vdupq_n_u32(vgetq_lane_u32(currentRowSum, 3));
    }

    for (; x < integralImageSize.width; x += 4) {
      uint32_t temp[4];
      temp[0] = pixel;
      temp[1] = temp[0] + pixel;
      temp[2] = temp[1] + pixel;
      temp[3] = temp[2] + pixel;
      uint32x4_t sumPixels = vld1q_u32(temp);
      sumPixels = vaddq_u32(sumPixels, currentRowSum);
      currentRowSum = vdupq_n_u32(vgetq_lane_u32(sumPixels, 3));
      vst1q_u32(intRow + x, vaddq_u32(sumPixels, vld1q_u32(intPrevRow + x)));
    }
  }

  if (aBottomInflation) {
    // Store the last valid row of our source image in the last row of
    // our integral image. This will be overwritten with the correct values
    // in the upcoming loop.
    LoadIntegralRowFromRow(
        aIntegralImage + (integralImageSize.height - 1) * stride32bit,
        aSource + (aSize.height - 1) * aSourceStride, aSize.width,
        aLeftInflation, aRightInflation);

    for (int y = aSize.height + aTopInflation; y < integralImageSize.height;
         y++) {
      uint32_t* intRow = aIntegralImage + (y * stride32bit);
      uint32_t* intPrevRow = aIntegralImage + (y - 1) * stride32bit;
      uint32_t* intLastRow =
          aIntegralImage + (integralImageSize.height - 1) * stride32bit;
      for (int x = 0; x < integralImageSize.width; x += 4) {
        vst1q_u32(intRow + x, vaddq_u32(vld1q_u32(intLastRow + x),
                                        vld1q_u32(intPrevRow + x)));
      }
    }
  }
}

/**
 * Attempt to do an in-place box blur using an integral image.
 */
void AlphaBoxBlur::BoxBlur_NEON(uint8_t* aData, int32_t aLeftLobe,
                                int32_t aRightLobe, int32_t aTopLobe,
                                int32_t aBottomLobe, uint32_t* aIntegralImage,
                                size_t aIntegralImageStride) const {
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

  GenerateIntegralImage_NEON(leftInflation, aRightLobe, aTopLobe, aBottomLobe,
                             aIntegralImage, aIntegralImageStride, aData,
                             mStride, size);

  uint32x2_t divisor = vdup_n_u32(reciprocal);

  // This points to the start of the rectangle within the IntegralImage that
  // overlaps the surface being blurred.
  uint32_t* innerIntegral =
      aIntegralImage + (aTopLobe * stride32bit) + leftInflation;
  IntRect skipRect = mSkipRect;
  int32_t stride = mStride;
  uint8_t* data = aData;

  for (int32_t y = 0; y < size.height; y++) {
    bool inSkipRectY = y > skipRect.y && y < skipRect.YMost();
    uint32_t* topLeftBase =
        innerIntegral + ((y - aTopLobe) * ptrdiff_t(stride32bit) - aLeftLobe);
    uint32_t* topRightBase =
        innerIntegral + ((y - aTopLobe) * ptrdiff_t(stride32bit) + aRightLobe);
    uint32_t* bottomRightBase =
        innerIntegral +
        ((y + aBottomLobe) * ptrdiff_t(stride32bit) + aRightLobe);
    uint32_t* bottomLeftBase =
        innerIntegral +
        ((y + aBottomLobe) * ptrdiff_t(stride32bit) - aLeftLobe);

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

      uint32x4_t topLeft;
      uint32x4_t topRight;
      uint32x4_t bottomRight;
      uint32x4_t bottomLeft;
      topLeft = vld1q_u32(topLeftBase + x);
      topRight = vld1q_u32(topRightBase + x);
      bottomRight = vld1q_u32(bottomRightBase + x);
      bottomLeft = vld1q_u32(bottomLeftBase + x);
      uint16x4_t result1 =
          BlurFourPixels(topLeft, topRight, bottomRight, bottomLeft, divisor);

      topLeft = vld1q_u32(topLeftBase + x + 4);
      topRight = vld1q_u32(topRightBase + x + 4);
      bottomRight = vld1q_u32(bottomRightBase + x + 4);
      bottomLeft = vld1q_u32(bottomLeftBase + x + 4);
      uint16x4_t result2 =
          BlurFourPixels(topLeft, topRight, bottomRight, bottomLeft, divisor);

      topLeft = vld1q_u32(topLeftBase + x + 8);
      topRight = vld1q_u32(topRightBase + x + 8);
      bottomRight = vld1q_u32(bottomRightBase + x + 8);
      bottomLeft = vld1q_u32(bottomLeftBase + x + 8);
      uint16x4_t result3 =
          BlurFourPixels(topLeft, topRight, bottomRight, bottomLeft, divisor);

      topLeft = vld1q_u32(topLeftBase + x + 12);
      topRight = vld1q_u32(topRightBase + x + 12);
      bottomRight = vld1q_u32(bottomRightBase + x + 12);
      bottomLeft = vld1q_u32(bottomLeftBase + x + 12);
      uint16x4_t result4 =
          BlurFourPixels(topLeft, topRight, bottomRight, bottomLeft, divisor);

      uint8x8_t combine1 = vqmovn_u16(vcombine_u16(result1, result2));
      uint8x8_t combine2 = vqmovn_u16(vcombine_u16(result3, result4));
      uint8x16_t final = vcombine_u8(combine1, combine2);
      vst1q_u8(data + stride * y + x, final);
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

      uint32x4_t topLeft = vld1q_u32(topLeftBase + x);
      uint32x4_t topRight = vld1q_u32(topRightBase + x);
      uint32x4_t bottomRight = vld1q_u32(bottomRightBase + x);
      uint32x4_t bottomLeft = vld1q_u32(bottomLeftBase + x);
      uint16x4_t result =
          BlurFourPixels(topLeft, topRight, bottomRight, bottomLeft, divisor);
      uint32x2_t final =
          vreinterpret_u32_u8(vmovn_u16(vcombine_u16(result, vdup_n_u16(0))));
      *(uint32_t*)(data + stride * y + x) = vget_lane_u32(final, 0);
    }
  }
}

}  // namespace gfx
}  // namespace mozilla
