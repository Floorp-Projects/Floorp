/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Blur.h"

#include <algorithm>
#include <math.h>
#include <string.h>

#include "mozilla/CheckedInt.h"

#include "2D.h"
#include "DataSurfaceHelpers.h"
#include "Tools.h"

#ifdef BUILD_ARM_NEON
#include "mozilla/arm.h"
#endif

using namespace std;

namespace mozilla {
namespace gfx {

/**
 * Helper function to process each row of the box blur.
 * It takes care of transposing the data on input or output depending
 * on whether we intend a horizontal or vertical blur, and whether we're
 * reading from the initial source or writing to the final destination.
 * It allows starting or ending anywhere within the row to accomodate
 * a skip rect.
 */
template<bool aTransposeInput, bool aTransposeOutput>
static inline void
BoxBlurRow(const uint8_t* aInput,
           uint8_t* aOutput,
           int32_t aLeftLobe,
           int32_t aRightLobe,
           int32_t aWidth,
           int32_t aStride,
           int32_t aStart,
           int32_t aEnd)
{
  // If the input or output is transposed, then we will move down a row
  // for each step, instead of moving over a column. Since these values
  // only depend on a template parameter, they will more easily get
  // copy-propagated in the non-transposed case, which is why they
  // are not passed as parameters.
  const int32_t inputStep = aTransposeInput ? aStride : 1;
  const int32_t outputStep = aTransposeOutput ? aStride : 1;

  // We need to sample aLeftLobe pixels to the left and aRightLobe pixels
  // to the right of the current position, then average them. So this is
  // the size of the total width of this filter.
  const int32_t boxSize = aLeftLobe + aRightLobe + 1;

  // Instead of dividing the pixel sum by boxSize to average, we can just
  // compute a scale that will normalize the result so that it can be quickly
  // shifted into the desired range.
  const uint32_t reciprocal = (1 << 24) / boxSize;

  // The shift would normally truncate the result, whereas we would rather
  // prefer to round the result to the closest increment. By adding 0.5 units
  // to the initial sum, we bias the sum so that it will be rounded by the
  // truncation instead.
  uint32_t alphaSum = (boxSize + 1) / 2;

  // We process the row with a moving filter, keeping a sum (alphaSum) of
  // boxSize pixels. As we move over a pixel, we need to add on a pixel
  // from the right extreme of the window that moved into range, and subtract
  // off a pixel from the left extreme of window that moved out of range.
  // But first, we need to initialization alphaSum to the contents of
  // the window before we can get going. If the window moves out of bounds
  // of the row, we clamp each sample to be the closest pixel from within
  // row bounds, so the 0th and aWidth-1th pixel.
  int32_t initLeft = aStart - aLeftLobe;
  if (initLeft < 0) {
    // If the left lobe samples before the row, add in clamped samples.
    alphaSum += -initLeft * aInput[0];
    initLeft = 0;
  }
  int32_t initRight = aStart + boxSize - aLeftLobe;
  if (initRight > aWidth) {
    // If the right lobe samples after the row, add in clamped samples.
    alphaSum += (initRight - aWidth) * aInput[(aWidth - 1) * inputStep];
    initRight = aWidth;
  }
  // Finally, add in all the valid, non-clamped samples to fill up the
  // rest of the window.
  const uint8_t* src = &aInput[initLeft * inputStep];
  const uint8_t* iterEnd = &aInput[initRight * inputStep];

  #define INIT_ITER \
    alphaSum += *src; \
    src += inputStep;

  // We unroll the per-pixel loop here substantially. The amount of work
  // done per sample is so small that the cost of a loop condition check
  // and a branch can substantially add to or even dominate the performance
  // of the loop.
  while (src + 16 * inputStep <= iterEnd) {
    INIT_ITER; INIT_ITER; INIT_ITER; INIT_ITER;
    INIT_ITER; INIT_ITER; INIT_ITER; INIT_ITER;
    INIT_ITER; INIT_ITER; INIT_ITER; INIT_ITER;
    INIT_ITER; INIT_ITER; INIT_ITER; INIT_ITER;
  }
  while (src < iterEnd) {
    INIT_ITER;
  }

  // Now we start moving the window over the row. We will be accessing
  // pixels form aStart - aLeftLobe up to aEnd + aRightLobe, which may be
  // out of bounds of the row. To avoid having to check within the inner
  // loops if we are in bound, we instead compute the points at which
  // we will move out of bounds of the row on the left side (splitLeft)
  // and right side (splitRight).
  int32_t splitLeft = min(max(aLeftLobe, aStart), aEnd);
  int32_t splitRight = min(max(aWidth - (boxSize - aLeftLobe), aStart), aEnd);
  // If the filter window is actually large than the size of the row,
  // there will be a middle area of overlap where the leftmost and rightmost
  // pixel of the filter will both be outside the row. In this case, we need
  // to invert the splits so that splitLeft <= splitRight.
  if (boxSize > aWidth) {
    swap(splitLeft, splitRight);
  }

  // Process all pixels up to splitLeft that would sample before the start of the row.
  // Note that because inputStep and outputStep may not be a const 1 value, it is more
  // performant to increment pointers here for the source and destination rather than
  // use a loop counter, since doing so would entail an expensive multiplication that
  // significantly slows down the loop.
  uint8_t* dst = &aOutput[aStart * outputStep];
  iterEnd = &aOutput[splitLeft * outputStep];
  src = &aInput[(aStart + boxSize - aLeftLobe) * inputStep];
  uint8_t firstVal = aInput[0];

  #define LEFT_ITER \
    *dst = (alphaSum * reciprocal) >> 24; \
    alphaSum += *src - firstVal; \
    dst += outputStep; \
    src += inputStep;

  while (dst + 16 * outputStep <= iterEnd) {
    LEFT_ITER; LEFT_ITER; LEFT_ITER; LEFT_ITER;
    LEFT_ITER; LEFT_ITER; LEFT_ITER; LEFT_ITER;
    LEFT_ITER; LEFT_ITER; LEFT_ITER; LEFT_ITER;
    LEFT_ITER; LEFT_ITER; LEFT_ITER; LEFT_ITER;
  }
  while (dst < iterEnd) {
    LEFT_ITER;
  }

  // Process all pixels between splitLeft and splitRight.
  iterEnd = &aOutput[splitRight * outputStep];
  if (boxSize <= aWidth) {
    // The filter window is smaller than the row size, so the leftmost and rightmost
    // samples are both within row bounds.
    src = &aInput[(splitLeft - aLeftLobe) * inputStep];
    int32_t boxStep = boxSize * inputStep;

    #define CENTER_ITER \
      *dst = (alphaSum * reciprocal) >> 24; \
      alphaSum += src[boxStep] - *src; \
      dst += outputStep; \
      src += inputStep;

    while (dst +  16 * outputStep <= iterEnd) {
      CENTER_ITER; CENTER_ITER; CENTER_ITER; CENTER_ITER;
      CENTER_ITER; CENTER_ITER; CENTER_ITER; CENTER_ITER;
      CENTER_ITER; CENTER_ITER; CENTER_ITER; CENTER_ITER;
      CENTER_ITER; CENTER_ITER; CENTER_ITER; CENTER_ITER;
    }
    while (dst < iterEnd) {
      CENTER_ITER;
    }
  } else {
    // The filter window is larger than the row size, and we're in the area of split
    // overlap. So the leftmost and rightmost samples are both out of bounds and need
    // to be clamped. We can just precompute the difference here consequently.
    int32_t firstLastDiff = aInput[(aWidth -1) * inputStep] - aInput[0];
    while (dst < iterEnd) {
      *dst = (alphaSum * reciprocal) >> 24;
      alphaSum += firstLastDiff;
      dst += outputStep;
    }
  }

  // Process all remaining pixels after splitRight that would sample after the row end.
  iterEnd = &aOutput[aEnd * outputStep];
  src = &aInput[(splitRight - aLeftLobe) * inputStep];
  uint8_t lastVal = aInput[(aWidth - 1) * inputStep];

  #define RIGHT_ITER \
    *dst = (alphaSum * reciprocal) >> 24; \
    alphaSum += lastVal - *src; \
    dst += outputStep; \
    src += inputStep;

  while (dst + 16 * outputStep <= iterEnd) {
    RIGHT_ITER; RIGHT_ITER; RIGHT_ITER; RIGHT_ITER;
    RIGHT_ITER; RIGHT_ITER; RIGHT_ITER; RIGHT_ITER;
    RIGHT_ITER; RIGHT_ITER; RIGHT_ITER; RIGHT_ITER;
    RIGHT_ITER; RIGHT_ITER; RIGHT_ITER; RIGHT_ITER;
  }
  while (dst < iterEnd) {
    RIGHT_ITER;
  }
}

/**
 * Box blur involves looking at one pixel, and setting its value to the average
 * of its neighbouring pixels. This is meant to provide a 3-pass approximation of a
 * Gaussian blur.
 * @param aTranspose Whether to transpose the buffer when reading and writing to it.
 * @param aData The buffer to be blurred.
 * @param aLobes The number of pixels to blend on the left and right for each of 3 passes.
 * @param aWidth The number of columns in the buffers.
 * @param aRows The number of rows in the buffers.
 * @param aStride The stride of the buffer.
 */
template<bool aTranspose>
static void
BoxBlur(uint8_t* aData,
        const int32_t aLobes[3][2],
        int32_t aWidth,
        int32_t aRows,
        int32_t aStride,
        IntRect aSkipRect)
{
  if (aTranspose) {
    swap(aWidth, aRows);
    aSkipRect.Swap();
  }

  MOZ_ASSERT(aWidth > 0);

  // All three passes of the box blur that approximate the Gaussian are done
  // on each row in turn, so we only need two temporary row buffers to process
  // each row, instead of a full-sized buffer. Data moves from the source to the
  // first temporary, from the first temporary to the second, then from the second
  // back to the destination. This way is more cache-friendly than processing whe
  // whole buffer in each pass and thus yields a nice speedup.
  uint8_t* tmpRow = new (std::nothrow) uint8_t[2 * aWidth];
  if (!tmpRow) {
    return;
  }
  uint8_t* tmpRow2 = tmpRow + aWidth;

  const int32_t stride = aTranspose ? 1 : aStride;
  bool skipRectCoversWholeRow = 0 >= aSkipRect.X() &&
                                aWidth <= aSkipRect.XMost();

  for (int32_t y = 0; y < aRows; y++) {
    // Check whether the skip rect intersects this row. If the skip
    // rect covers the whole surface in this row, we can avoid
    // this row entirely (and any others along the skip rect).
    bool inSkipRectY = aSkipRect.ContainsY(y);
    if (inSkipRectY && skipRectCoversWholeRow) {
      aData += stride * (aSkipRect.YMost() - y);
      y = aSkipRect.YMost() - 1;
      continue;
    }

    // Read in data from the source transposed if necessary.
    BoxBlurRow<aTranspose, false>(aData, tmpRow, aLobes[0][0], aLobes[0][1], aWidth, aStride, 0, aWidth);

    // For the middle pass, the data is already pre-transposed and does not need to be post-transposed yet.
    BoxBlurRow<false, false>(tmpRow, tmpRow2, aLobes[1][0], aLobes[1][1], aWidth, aStride, 0, aWidth);

    // Write back data to the destination transposed if necessary too.
    // Make sure not to overwrite the skip rect by only outputting to the
    // destination before and after the skip rect, if requested.
    int32_t skipStart = inSkipRectY ? min(max(aSkipRect.X(), 0), aWidth) : aWidth;
    int32_t skipEnd = max(skipStart, aSkipRect.XMost());
    if (skipStart > 0) {
      BoxBlurRow<false, aTranspose>(tmpRow2, aData, aLobes[2][0], aLobes[2][1], aWidth, aStride, 0, skipStart);
    }
    if (skipEnd < aWidth) {
      BoxBlurRow<false, aTranspose>(tmpRow2, aData, aLobes[2][0], aLobes[2][1], aWidth, aStride, skipEnd, aWidth);
    }

    aData += stride;
  }

  delete[] tmpRow;
}

static void ComputeLobes(int32_t aRadius, int32_t aLobes[3][2])
{
    int32_t major, minor, final;

    /* See http://www.w3.org/TR/SVG/filters.html#feGaussianBlur for
     * some notes about approximating the Gaussian blur with box-blurs.
     * The comments below are in the terminology of that page.
     */
    int32_t z = aRadius / 3;
    switch (aRadius % 3) {
    case 0:
        // aRadius = z*3; choose d = 2*z + 1
        major = minor = final = z;
        break;
    case 1:
        // aRadius = z*3 + 1
        // This is a tricky case since there is no value of d which will
        // yield a radius of exactly aRadius. If d is odd, i.e. d=2*k + 1
        // for some integer k, then the radius will be 3*k. If d is even,
        // i.e. d=2*k, then the radius will be 3*k - 1.
        // So we have to choose values that don't match the standard
        // algorithm.
        major = z + 1;
        minor = final = z;
        break;
    case 2:
        // aRadius = z*3 + 2; choose d = 2*z + 2
        major = final = z + 1;
        minor = z;
        break;
    default:
        // Mathematical impossibility!
        MOZ_ASSERT(false);
        major = minor = final = 0;
    }
    MOZ_ASSERT(major + minor + final == aRadius);

    aLobes[0][0] = major;
    aLobes[0][1] = minor;
    aLobes[1][0] = minor;
    aLobes[1][1] = major;
    aLobes[2][0] = final;
    aLobes[2][1] = final;
}

static void
SpreadHorizontal(uint8_t* aInput,
                 uint8_t* aOutput,
                 int32_t aRadius,
                 int32_t aWidth,
                 int32_t aRows,
                 int32_t aStride,
                 const IntRect& aSkipRect)
{
    if (aRadius == 0) {
        memcpy(aOutput, aInput, aStride * aRows);
        return;
    }

    bool skipRectCoversWholeRow = 0 >= aSkipRect.X() &&
                                    aWidth <= aSkipRect.XMost();
    for (int32_t y = 0; y < aRows; y++) {
        // Check whether the skip rect intersects this row. If the skip
        // rect covers the whole surface in this row, we can avoid
        // this row entirely (and any others along the skip rect).
        bool inSkipRectY = aSkipRect.ContainsY(y);
        if (inSkipRectY && skipRectCoversWholeRow) {
            y = aSkipRect.YMost() - 1;
            continue;
        }

        for (int32_t x = 0; x < aWidth; x++) {
            // Check whether we are within the skip rect. If so, go
            // to the next point outside the skip rect.
            if (inSkipRectY && aSkipRect.ContainsX(x)) {
                x = aSkipRect.XMost();
                if (x >= aWidth)
                    break;
            }

            int32_t sMin = max(x - aRadius, 0);
            int32_t sMax = min(x + aRadius, aWidth - 1);
            int32_t v = 0;
            for (int32_t s = sMin; s <= sMax; ++s) {
                v = max<int32_t>(v, aInput[aStride * y + s]);
            }
            aOutput[aStride * y + x] = v;
        }
    }
}

static void
SpreadVertical(uint8_t* aInput,
               uint8_t* aOutput,
               int32_t aRadius,
               int32_t aWidth,
               int32_t aRows,
               int32_t aStride,
               const IntRect& aSkipRect)
{
    if (aRadius == 0) {
        memcpy(aOutput, aInput, aStride * aRows);
        return;
    }

    bool skipRectCoversWholeColumn = 0 >= aSkipRect.Y() &&
                                     aRows <= aSkipRect.YMost();
    for (int32_t x = 0; x < aWidth; x++) {
        bool inSkipRectX = aSkipRect.ContainsX(x);
        if (inSkipRectX && skipRectCoversWholeColumn) {
            x = aSkipRect.XMost() - 1;
            continue;
        }

        for (int32_t y = 0; y < aRows; y++) {
            // Check whether we are within the skip rect. If so, go
            // to the next point outside the skip rect.
            if (inSkipRectX && aSkipRect.ContainsY(y)) {
                y = aSkipRect.YMost();
                if (y >= aRows)
                    break;
            }

            int32_t sMin = max(y - aRadius, 0);
            int32_t sMax = min(y + aRadius, aRows - 1);
            int32_t v = 0;
            for (int32_t s = sMin; s <= sMax; ++s) {
                v = max<int32_t>(v, aInput[aStride * s + x]);
            }
            aOutput[aStride * y + x] = v;
        }
    }
}

CheckedInt<int32_t>
AlphaBoxBlur::RoundUpToMultipleOf4(int32_t aVal)
{
  CheckedInt<int32_t> val(aVal);

  val += 3;
  val /= 4;
  val *= 4;

  return val;
}

AlphaBoxBlur::AlphaBoxBlur(const Rect& aRect,
                           const IntSize& aSpreadRadius,
                           const IntSize& aBlurRadius,
                           const Rect* aDirtyRect,
                           const Rect* aSkipRect)
  : mSurfaceAllocationSize(0)
{
  Init(aRect, aSpreadRadius, aBlurRadius, aDirtyRect, aSkipRect);
}

AlphaBoxBlur::AlphaBoxBlur()
  : mSurfaceAllocationSize(0)
{
}

void
AlphaBoxBlur::Init(const Rect& aRect,
                   const IntSize& aSpreadRadius,
                   const IntSize& aBlurRadius,
                   const Rect* aDirtyRect,
                   const Rect* aSkipRect)
{
  mSpreadRadius = aSpreadRadius;
  mBlurRadius = aBlurRadius;

  Rect rect(aRect);
  rect.Inflate(Size(aBlurRadius + aSpreadRadius));
  rect.RoundOut();

  if (aDirtyRect) {
    // If we get passed a dirty rect from layout, we can minimize the
    // shadow size and make painting faster.
    mHasDirtyRect = true;
    mDirtyRect = *aDirtyRect;
    Rect requiredBlurArea = mDirtyRect.Intersect(rect);
    requiredBlurArea.Inflate(Size(aBlurRadius + aSpreadRadius));
    rect = requiredBlurArea.Intersect(rect);
  } else {
    mHasDirtyRect = false;
  }

  mRect = TruncatedToInt(rect);
  if (mRect.IsEmpty()) {
    return;
  }

  if (aSkipRect) {
    // If we get passed a skip rect, we can lower the amount of
    // blurring/spreading we need to do. We convert it to IntRect to avoid
    // expensive int<->float conversions if we were to use Rect instead.
    Rect skipRect = *aSkipRect;
    skipRect.Deflate(Size(aBlurRadius + aSpreadRadius));
    mSkipRect = RoundedIn(skipRect);
    mSkipRect = mSkipRect.Intersect(mRect);
    if (mSkipRect.IsEqualInterior(mRect))
      return;

    mSkipRect -= mRect.TopLeft();
  } else {
    mSkipRect = IntRect(0, 0, 0, 0);
  }

  CheckedInt<int32_t> stride = RoundUpToMultipleOf4(mRect.Width());
  if (stride.isValid()) {
    mStride = stride.value();

    // We need to leave room for an additional 3 bytes for a potential overrun
    // in our blurring code.
    size_t size = BufferSizeFromStrideAndHeight(mStride, mRect.Height(), 3);
    if (size != 0) {
      mSurfaceAllocationSize = size;
    }
  }
}

AlphaBoxBlur::AlphaBoxBlur(const Rect& aRect,
                           int32_t aStride,
                           float aSigmaX,
                           float aSigmaY)
  : mRect(TruncatedToInt(aRect)),
    mSpreadRadius(),
    mBlurRadius(CalculateBlurRadius(Point(aSigmaX, aSigmaY))),
    mStride(aStride),
    mSurfaceAllocationSize(0)
{
  IntRect intRect;
  if (aRect.ToIntRect(&intRect)) {
    size_t minDataSize = BufferSizeFromStrideAndHeight(intRect.Width(), intRect.Height());
    if (minDataSize != 0) {
      mSurfaceAllocationSize = minDataSize;
    }
  }
}


AlphaBoxBlur::~AlphaBoxBlur()
{
}

IntSize
AlphaBoxBlur::GetSize() const
{
  IntSize size(mRect.Width(), mRect.Height());
  return size;
}

int32_t
AlphaBoxBlur::GetStride() const
{
  return mStride;
}

IntRect
AlphaBoxBlur::GetRect() const
{
  return mRect;
}

Rect*
AlphaBoxBlur::GetDirtyRect()
{
  if (mHasDirtyRect) {
    return &mDirtyRect;
  }

  return nullptr;
}

size_t
AlphaBoxBlur::GetSurfaceAllocationSize() const
{
  return mSurfaceAllocationSize;
}

void
AlphaBoxBlur::Blur(uint8_t* aData) const
{
  if (!aData) {
    return;
  }

  // no need to do all this if not blurring or spreading
  if (mBlurRadius != IntSize(0,0) || mSpreadRadius != IntSize(0,0)) {
    int32_t stride = GetStride();

    IntSize size = GetSize();

    if (mSpreadRadius.width > 0 || mSpreadRadius.height > 0) {
      // No need to use CheckedInt here - we have validated it in the constructor.
      size_t szB = stride * size.height;
      uint8_t* tmpData = new (std::nothrow) uint8_t[szB];

      if (!tmpData) {
        return;
      }

      memset(tmpData, 0, szB);

      SpreadHorizontal(aData, tmpData, mSpreadRadius.width, size.width, size.height, stride, mSkipRect);
      SpreadVertical(tmpData, aData, mSpreadRadius.height, size.width, size.height, stride, mSkipRect);

      delete [] tmpData;
    }

    int32_t horizontalLobes[3][2];
    ComputeLobes(mBlurRadius.width, horizontalLobes);
    int32_t verticalLobes[3][2];
    ComputeLobes(mBlurRadius.height, verticalLobes);

    // We want to allow for some extra space on the left for alignment reasons.
    int32_t maxLeftLobe = RoundUpToMultipleOf4(horizontalLobes[0][0] + 1).value();

    IntSize integralImageSize(size.width + maxLeftLobe + horizontalLobes[1][1],
                              size.height + verticalLobes[0][0] + verticalLobes[1][1] + 1);

    if ((integralImageSize.width * integralImageSize.height) > (1 << 24)) {
      // Fallback to old blurring code when the surface is so large it may
      // overflow our integral image!
      if (mBlurRadius.width > 0) {
        BoxBlur<false>(aData, horizontalLobes, size.width, size.height, stride, mSkipRect);
      }
      if (mBlurRadius.height > 0) {
        BoxBlur<true>(aData, verticalLobes, size.width, size.height, stride, mSkipRect);
      }
    } else {
      size_t integralImageStride = GetAlignedStride<16>(integralImageSize.width, 4);
      if (integralImageStride == 0) {
        return;
      }

      // We need to leave room for an additional 12 bytes for a maximum overrun
      // of 3 pixels in the blurring code.
      size_t bufLen = BufferSizeFromStrideAndHeight(integralImageStride, integralImageSize.height, 12);
      if (bufLen == 0) {
        return;
      }
      // bufLen is a byte count, but here we want a multiple of 32-bit ints, so
      // we divide by 4.
      AlignedArray<uint32_t> integralImage((bufLen / 4) + ((bufLen % 4) ? 1 : 0));

      if (!integralImage) {
        return;
      }

#ifdef USE_SSE2
      if (Factory::HasSSE2()) {
        BoxBlur_SSE2(aData, horizontalLobes[0][0], horizontalLobes[0][1], verticalLobes[0][0],
                     verticalLobes[0][1], integralImage, integralImageStride);
        BoxBlur_SSE2(aData, horizontalLobes[1][0], horizontalLobes[1][1], verticalLobes[1][0],
                     verticalLobes[1][1], integralImage, integralImageStride);
        BoxBlur_SSE2(aData, horizontalLobes[2][0], horizontalLobes[2][1], verticalLobes[2][0],
                     verticalLobes[2][1], integralImage, integralImageStride);
      } else
#endif
#ifdef BUILD_ARM_NEON
      if (mozilla::supports_neon()) {
        BoxBlur_NEON(aData, horizontalLobes[0][0], horizontalLobes[0][1], verticalLobes[0][0],
                     verticalLobes[0][1], integralImage, integralImageStride);
        BoxBlur_NEON(aData, horizontalLobes[1][0], horizontalLobes[1][1], verticalLobes[1][0],
                     verticalLobes[1][1], integralImage, integralImageStride);
        BoxBlur_NEON(aData, horizontalLobes[2][0], horizontalLobes[2][1], verticalLobes[2][0],
                     verticalLobes[2][1], integralImage, integralImageStride);
      } else
#endif
      {
#ifdef _MIPS_ARCH_LOONGSON3A
        BoxBlur_LS3(aData, horizontalLobes[0][0], horizontalLobes[0][1], verticalLobes[0][0],
                     verticalLobes[0][1], integralImage, integralImageStride);
        BoxBlur_LS3(aData, horizontalLobes[1][0], horizontalLobes[1][1], verticalLobes[1][0],
                     verticalLobes[1][1], integralImage, integralImageStride);
        BoxBlur_LS3(aData, horizontalLobes[2][0], horizontalLobes[2][1], verticalLobes[2][0],
                     verticalLobes[2][1], integralImage, integralImageStride);
#else
        BoxBlur_C(aData, horizontalLobes[0][0], horizontalLobes[0][1], verticalLobes[0][0],
                  verticalLobes[0][1], integralImage, integralImageStride);
        BoxBlur_C(aData, horizontalLobes[1][0], horizontalLobes[1][1], verticalLobes[1][0],
                  verticalLobes[1][1], integralImage, integralImageStride);
        BoxBlur_C(aData, horizontalLobes[2][0], horizontalLobes[2][1], verticalLobes[2][0],
                  verticalLobes[2][1], integralImage, integralImageStride);
#endif
      }
    }
  }
}

MOZ_ALWAYS_INLINE void
GenerateIntegralRow(uint32_t  *aDest, const uint8_t *aSource, uint32_t *aPreviousRow,
                    const uint32_t &aSourceWidth, const uint32_t &aLeftInflation, const uint32_t &aRightInflation)
{
  uint32_t currentRowSum = 0;
  uint32_t pixel = aSource[0];
  for (uint32_t x = 0; x < aLeftInflation; x++) {
    currentRowSum += pixel;
    *aDest++ = currentRowSum + *aPreviousRow++;
  }
  for (uint32_t x = aLeftInflation; x < (aSourceWidth + aLeftInflation); x += 4) {
      uint32_t alphaValues = *(uint32_t*)(aSource + (x - aLeftInflation));
#if defined WORDS_BIGENDIAN || defined IS_BIG_ENDIAN || defined __BIG_ENDIAN__
      currentRowSum += (alphaValues >> 24) & 0xff;
      *aDest++ = *aPreviousRow++ + currentRowSum;
      currentRowSum += (alphaValues >> 16) & 0xff;
      *aDest++ = *aPreviousRow++ + currentRowSum;
      currentRowSum += (alphaValues >> 8) & 0xff;
      *aDest++ = *aPreviousRow++ + currentRowSum;
      currentRowSum += alphaValues & 0xff;
      *aDest++ = *aPreviousRow++ + currentRowSum;
#else
      currentRowSum += alphaValues & 0xff;
      *aDest++ = *aPreviousRow++ + currentRowSum;
      alphaValues >>= 8;
      currentRowSum += alphaValues & 0xff;
      *aDest++ = *aPreviousRow++ + currentRowSum;
      alphaValues >>= 8;
      currentRowSum += alphaValues & 0xff;
      *aDest++ = *aPreviousRow++ + currentRowSum;
      alphaValues >>= 8;
      currentRowSum += alphaValues & 0xff;
      *aDest++ = *aPreviousRow++ + currentRowSum;
#endif
  }
  pixel = aSource[aSourceWidth - 1];
  for (uint32_t x = (aSourceWidth + aLeftInflation); x < (aSourceWidth + aLeftInflation + aRightInflation); x++) {
    currentRowSum += pixel;
    *aDest++ = currentRowSum + *aPreviousRow++;
  }
}

MOZ_ALWAYS_INLINE void
GenerateIntegralImage_C(int32_t aLeftInflation, int32_t aRightInflation,
                        int32_t aTopInflation, int32_t aBottomInflation,
                        uint32_t *aIntegralImage, size_t aIntegralImageStride,
                        uint8_t *aSource, int32_t aSourceStride, const IntSize &aSize)
{
  uint32_t stride32bit = aIntegralImageStride / 4;

  IntSize integralImageSize(aSize.width + aLeftInflation + aRightInflation,
                            aSize.height + aTopInflation + aBottomInflation);

  memset(aIntegralImage, 0, aIntegralImageStride);

  GenerateIntegralRow(aIntegralImage, aSource, aIntegralImage,
                      aSize.width, aLeftInflation, aRightInflation);
  for (int y = 1; y < aTopInflation + 1; y++) {
    GenerateIntegralRow(aIntegralImage + (y * stride32bit), aSource, aIntegralImage + (y - 1) * stride32bit,
                        aSize.width, aLeftInflation, aRightInflation);
  }

  for (int y = aTopInflation + 1; y < (aSize.height + aTopInflation); y++) {
    GenerateIntegralRow(aIntegralImage + (y * stride32bit), aSource + aSourceStride * (y - aTopInflation),
                        aIntegralImage + (y - 1) * stride32bit, aSize.width, aLeftInflation, aRightInflation);
  }

  if (aBottomInflation) {
    for (int y = (aSize.height + aTopInflation); y < integralImageSize.height; y++) {
      GenerateIntegralRow(aIntegralImage + (y * stride32bit), aSource + ((aSize.height - 1) * aSourceStride),
                          aIntegralImage + (y - 1) * stride32bit,
                          aSize.width, aLeftInflation, aRightInflation);
    }
  }
}

/**
 * Attempt to do an in-place box blur using an integral image.
 */
void
AlphaBoxBlur::BoxBlur_C(uint8_t* aData,
                        int32_t aLeftLobe,
                        int32_t aRightLobe,
                        int32_t aTopLobe,
                        int32_t aBottomLobe,
                        uint32_t *aIntegralImage,
                        size_t aIntegralImageStride) const
{
  IntSize size = GetSize();

  MOZ_ASSERT(size.width > 0);

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

  int32_t stride32bit = aIntegralImageStride / 4;

  int32_t leftInflation = RoundUpToMultipleOf4(aLeftLobe).value();

  GenerateIntegralImage_C(leftInflation, aRightLobe, aTopLobe, aBottomLobe,
                          aIntegralImage, aIntegralImageStride, aData,
                          mStride, size);

  uint32_t reciprocal = uint32_t((uint64_t(1) << 32) / boxSize);

  uint32_t *innerIntegral = aIntegralImage + (aTopLobe * stride32bit) + leftInflation;

  // Storing these locally makes this about 30% faster! Presumably the compiler
  // can't be sure we're not altering the member variables in this loop.
  IntRect skipRect = mSkipRect;
  uint8_t *data = aData;
  int32_t stride = mStride;
  for (int32_t y = 0; y < size.height; y++) {
    // Not using ContainsY(y) because we do not skip y == skipRect.Y()
    // although that may not be done on purpose
    bool inSkipRectY = y > skipRect.Y() && y < skipRect.YMost();

    uint32_t *topLeftBase = innerIntegral + ((y - aTopLobe) * stride32bit - aLeftLobe);
    uint32_t *topRightBase = innerIntegral + ((y - aTopLobe) * stride32bit + aRightLobe);
    uint32_t *bottomRightBase = innerIntegral + ((y + aBottomLobe) * stride32bit + aRightLobe);
    uint32_t *bottomLeftBase = innerIntegral + ((y + aBottomLobe) * stride32bit - aLeftLobe);

    for (int32_t x = 0; x < size.width; x++) {
      // Not using ContainsX(x) because we do not skip x == skipRect.X()
      // although that may not be done on purpose
      if (inSkipRectY && x > skipRect.X() && x < skipRect.XMost()) {
        x = skipRect.XMost() - 1;
        // Trigger early jump on coming loop iterations, this will be reset
        // next line anyway.
        inSkipRectY = false;
        continue;
      }
      int32_t topLeft = topLeftBase[x];
      int32_t topRight = topRightBase[x];
      int32_t bottomRight = bottomRightBase[x];
      int32_t bottomLeft = bottomLeftBase[x];

      uint32_t value = bottomRight - topRight - bottomLeft;
      value += topLeft;

      data[stride * y + x] = (uint64_t(reciprocal) * value + (uint64_t(1) << 31)) >> 32;
    }
  }
}

/**
 * Compute the box blur size (which we're calling the blur radius) from
 * the standard deviation.
 *
 * Much of this, the 3 * sqrt(2 * pi) / 4, is the known value for
 * approximating a Gaussian using box blurs.  This yields quite a good
 * approximation for a Gaussian.  Then we multiply this by 1.5 since our
 * code wants the radius of the entire triple-box-blur kernel instead of
 * the diameter of an individual box blur.  For more details, see:
 *   http://www.w3.org/TR/SVG11/filters.html#feGaussianBlurElement
 *   https://bugzilla.mozilla.org/show_bug.cgi?id=590039#c19
 */
static const Float GAUSSIAN_SCALE_FACTOR = Float((3 * sqrt(2 * M_PI) / 4) * 1.5);

IntSize
AlphaBoxBlur::CalculateBlurRadius(const Point& aStd)
{
    IntSize size(static_cast<int32_t>(floor(aStd.x * GAUSSIAN_SCALE_FACTOR + 0.5f)),
                 static_cast<int32_t>(floor(aStd.y * GAUSSIAN_SCALE_FACTOR + 0.5f)));

    return size;
}

Float
AlphaBoxBlur::CalculateBlurSigma(int32_t aBlurRadius)
{
  return aBlurRadius / GAUSSIAN_SCALE_FACTOR;
}

} // namespace gfx
} // namespace mozilla
