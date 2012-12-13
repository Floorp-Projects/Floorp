/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/gfx/Blur.h"

#include <algorithm>
#include <math.h>
#include <string.h>

#include "mozilla/CheckedInt.h"
#include "mozilla/Constants.h"
#include "mozilla/Util.h"

#include "2D.h"
#include "Tools.h"

using namespace std;

namespace mozilla {
namespace gfx {

/**
 * Box blur involves looking at one pixel, and setting its value to the average
 * of its neighbouring pixels.
 * @param aInput The input buffer.
 * @param aOutput The output buffer.
 * @param aLeftLobe The number of pixels to blend on the left.
 * @param aRightLobe The number of pixels to blend on the right.
 * @param aWidth The number of columns in the buffers.
 * @param aRows The number of rows in the buffers.
 * @param aSkipRect An area to skip blurring in.
 * XXX shouldn't we pass stride in separately here?
 */
static void
BoxBlurHorizontal(unsigned char* aInput,
                  unsigned char* aOutput,
                  int32_t aLeftLobe,
                  int32_t aRightLobe,
                  int32_t aWidth,
                  int32_t aRows,
                  const IntRect& aSkipRect)
{
    MOZ_ASSERT(aWidth > 0);

    int32_t boxSize = aLeftLobe + aRightLobe + 1;
    bool skipRectCoversWholeRow = 0 >= aSkipRect.x &&
                                  aWidth <= aSkipRect.XMost();
    if (boxSize == 1) {
        memcpy(aOutput, aInput, aWidth*aRows);
        return;
    }
    uint32_t reciprocal = uint32_t((uint64_t(1) << 32) / boxSize);

    for (int32_t y = 0; y < aRows; y++) {
        // Check whether the skip rect intersects this row. If the skip
        // rect covers the whole surface in this row, we can avoid
        // this row entirely (and any others along the skip rect).
        bool inSkipRectY = y >= aSkipRect.y &&
                           y < aSkipRect.YMost();
        if (inSkipRectY && skipRectCoversWholeRow) {
            y = aSkipRect.YMost() - 1;
            continue;
        }

        uint32_t alphaSum = 0;
        for (int32_t i = 0; i < boxSize; i++) {
            int32_t pos = i - aLeftLobe;
            // See assertion above; if aWidth is zero, then we would have no
            // valid position to clamp to.
            pos = max(pos, 0);
            pos = min(pos, aWidth - 1);
            alphaSum += aInput[aWidth * y + pos];
        }
        for (int32_t x = 0; x < aWidth; x++) {
            // Check whether we are within the skip rect. If so, go
            // to the next point outside the skip rect.
            if (inSkipRectY && x >= aSkipRect.x &&
                x < aSkipRect.XMost()) {
                x = aSkipRect.XMost();
                if (x >= aWidth)
                    break;

                // Recalculate the neighbouring alpha values for
                // our new point on the surface.
                alphaSum = 0;
                for (int32_t i = 0; i < boxSize; i++) {
                    int32_t pos = x + i - aLeftLobe;
                    // See assertion above; if aWidth is zero, then we would have no
                    // valid position to clamp to.
                    pos = max(pos, 0);
                    pos = min(pos, aWidth - 1);
                    alphaSum += aInput[aWidth * y + pos];
                }
            }
            int32_t tmp = x - aLeftLobe;
            int32_t last = max(tmp, 0);
            int32_t next = min(tmp + boxSize, aWidth - 1);

            aOutput[aWidth * y + x] = (uint64_t(alphaSum) * reciprocal) >> 32;

            alphaSum += aInput[aWidth * y + next] -
                        aInput[aWidth * y + last];
        }
    }
}

/**
 * Identical to BoxBlurHorizontal, except it blurs top and bottom instead of
 * left and right.
 * XXX shouldn't we pass stride in separately here?
 */
static void
BoxBlurVertical(unsigned char* aInput,
                unsigned char* aOutput,
                int32_t aTopLobe,
                int32_t aBottomLobe,
                int32_t aWidth,
                int32_t aRows,
                const IntRect& aSkipRect)
{
    MOZ_ASSERT(aRows > 0);

    int32_t boxSize = aTopLobe + aBottomLobe + 1;
    bool skipRectCoversWholeColumn = 0 >= aSkipRect.y &&
                                     aRows <= aSkipRect.YMost();
    if (boxSize == 1) {
        memcpy(aOutput, aInput, aWidth*aRows);
        return;
    }
    uint32_t reciprocal = uint32_t((uint64_t(1) << 32) / boxSize);

    for (int32_t x = 0; x < aWidth; x++) {
        bool inSkipRectX = x >= aSkipRect.x &&
                           x < aSkipRect.XMost();
        if (inSkipRectX && skipRectCoversWholeColumn) {
            x = aSkipRect.XMost() - 1;
            continue;
        }

        uint32_t alphaSum = 0;
        for (int32_t i = 0; i < boxSize; i++) {
            int32_t pos = i - aTopLobe;
            // See assertion above; if aRows is zero, then we would have no
            // valid position to clamp to.
            pos = max(pos, 0);
            pos = min(pos, aRows - 1);
            alphaSum += aInput[aWidth * pos + x];
        }
        for (int32_t y = 0; y < aRows; y++) {
            if (inSkipRectX && y >= aSkipRect.y &&
                y < aSkipRect.YMost()) {
                y = aSkipRect.YMost();
                if (y >= aRows)
                    break;

                alphaSum = 0;
                for (int32_t i = 0; i < boxSize; i++) {
                    int32_t pos = y + i - aTopLobe;
                    // See assertion above; if aRows is zero, then we would have no
                    // valid position to clamp to.
                    pos = max(pos, 0);
                    pos = min(pos, aRows - 1);
                    alphaSum += aInput[aWidth * pos + x];
                }
            }
            int32_t tmp = y - aTopLobe;
            int32_t last = max(tmp, 0);
            int32_t next = min(tmp + boxSize, aRows - 1);

            aOutput[aWidth * y + x] = (uint64_t(alphaSum) * reciprocal) >> 32;

            alphaSum += aInput[aWidth * next + x] -
                        aInput[aWidth * last + x];
        }
    }
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
SpreadHorizontal(unsigned char* aInput,
                 unsigned char* aOutput,
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

    bool skipRectCoversWholeRow = 0 >= aSkipRect.x &&
                                    aWidth <= aSkipRect.XMost();
    for (int32_t y = 0; y < aRows; y++) {
        // Check whether the skip rect intersects this row. If the skip
        // rect covers the whole surface in this row, we can avoid
        // this row entirely (and any others along the skip rect).
        bool inSkipRectY = y >= aSkipRect.y &&
                             y < aSkipRect.YMost();
        if (inSkipRectY && skipRectCoversWholeRow) {
            y = aSkipRect.YMost() - 1;
            continue;
        }

        for (int32_t x = 0; x < aWidth; x++) {
            // Check whether we are within the skip rect. If so, go
            // to the next point outside the skip rect.
            if (inSkipRectY && x >= aSkipRect.x &&
                x < aSkipRect.XMost()) {
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
SpreadVertical(unsigned char* aInput,
               unsigned char* aOutput,
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

    bool skipRectCoversWholeColumn = 0 >= aSkipRect.y &&
                                     aRows <= aSkipRect.YMost();
    for (int32_t x = 0; x < aWidth; x++) {
        bool inSkipRectX = x >= aSkipRect.x &&
                           x < aSkipRect.XMost();
        if (inSkipRectX && skipRectCoversWholeColumn) {
            x = aSkipRect.XMost() - 1;
            continue;
        }

        for (int32_t y = 0; y < aRows; y++) {
            // Check whether we are within the skip rect. If so, go
            // to the next point outside the skip rect.
            if (inSkipRectX && y >= aSkipRect.y &&
                y < aSkipRect.YMost()) {
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
 : mSpreadRadius(aSpreadRadius),
   mBlurRadius(aBlurRadius),
   mData(nullptr),
   mFreeData(true)
{
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

  mRect = IntRect(int32_t(rect.x), int32_t(rect.y),
                  int32_t(rect.width), int32_t(rect.height));
  if (mRect.IsEmpty()) {
    return;
  }

  if (aSkipRect) {
    // If we get passed a skip rect, we can lower the amount of
    // blurring/spreading we need to do. We convert it to IntRect to avoid
    // expensive int<->float conversions if we were to use Rect instead.
    Rect skipRect = *aSkipRect;
    skipRect.RoundIn();
    skipRect.Deflate(Size(aBlurRadius + aSpreadRadius));
    mSkipRect = IntRect(int32_t(skipRect.x), int32_t(skipRect.y),
                        int32_t(skipRect.width), int32_t(skipRect.height));

    mSkipRect = mSkipRect.Intersect(mRect);
    if (mSkipRect.IsEqualInterior(mRect))
      return;

    mSkipRect -= mRect.TopLeft();
  } else {
    mSkipRect = IntRect(0, 0, 0, 0);
  }

  CheckedInt<int32_t> stride = RoundUpToMultipleOf4(mRect.width);
  if (stride.isValid()) {
    mStride = stride.value();

    // We need to leave room for an additional 3 bytes for a potential overrun
    // in our blurring code.
    CheckedInt<int32_t> size = CheckedInt<int32_t>(mStride) * mRect.height + 3;
    if (size.isValid()) {
      mData = new uint8_t[size.value()];
      memset(mData, 0, size.value());
    }
  }
}

AlphaBoxBlur::AlphaBoxBlur(uint8_t* aData,
                           const Rect& aRect,
                           int32_t aStride,
                           float aSigma)
  : mRect(int32_t(aRect.x), int32_t(aRect.y),
          int32_t(aRect.width), int32_t(aRect.height)),
    mSpreadRadius(),
    mBlurRadius(CalculateBlurRadius(Point(aSigma, aSigma))),
    mData(aData),
    mFreeData(false),
    mStride(aStride)
{
}


AlphaBoxBlur::~AlphaBoxBlur()
{
  if (mFreeData) {
    delete [] mData;
  }
}

unsigned char*
AlphaBoxBlur::GetData()
{
  return mData;
}

IntSize
AlphaBoxBlur::GetSize()
{
  IntSize size(mRect.width, mRect.height);
  return size;
}

int32_t
AlphaBoxBlur::GetStride()
{
  return mStride;
}

IntRect
AlphaBoxBlur::GetRect()
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

void
AlphaBoxBlur::Blur()
{
  if (!mData) {
    return;
  }

  // no need to do all this if not blurring or spreading
  if (mBlurRadius != IntSize(0,0) || mSpreadRadius != IntSize(0,0)) {
    int32_t stride = GetStride();

    IntSize size = GetSize();

    if (mSpreadRadius.width > 0 || mSpreadRadius.height > 0) {
      // No need to use CheckedInt here - we have validated it in the constructor.
      size_t szB = stride * size.height;
      unsigned char* tmpData = new uint8_t[szB];

      memset(tmpData, 0, szB);

      SpreadHorizontal(mData, tmpData, mSpreadRadius.width, GetSize().width, GetSize().height, stride, mSkipRect);
      SpreadVertical(tmpData, mData, mSpreadRadius.height, GetSize().width, GetSize().height, stride, mSkipRect);

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

      // No need to use CheckedInt here - we have validated it in the constructor.
      size_t szB = stride * size.height;
      uint8_t* tmpData = new uint8_t[szB];
      memset(tmpData, 0, szB);

      uint8_t* a = mData;
      uint8_t* b = tmpData;
      if (mBlurRadius.width > 0) {
        BoxBlurHorizontal(a, b, horizontalLobes[0][0], horizontalLobes[0][1], stride, GetSize().height, mSkipRect);
        BoxBlurHorizontal(b, a, horizontalLobes[1][0], horizontalLobes[1][1], stride, GetSize().height, mSkipRect);
        BoxBlurHorizontal(a, b, horizontalLobes[2][0], horizontalLobes[2][1], stride, GetSize().height, mSkipRect);
      } else {
        a = tmpData;
        b = mData;
      }
      // The result is in 'b' here.
      if (mBlurRadius.height > 0) {
        BoxBlurVertical(b, a, verticalLobes[0][0], verticalLobes[0][1], stride, GetSize().height, mSkipRect);
        BoxBlurVertical(a, b, verticalLobes[1][0], verticalLobes[1][1], stride, GetSize().height, mSkipRect);
        BoxBlurVertical(b, a, verticalLobes[2][0], verticalLobes[2][1], stride, GetSize().height, mSkipRect);
      } else {
        a = b;
      }
      // The result is in 'a' here.
      if (a == tmpData) {
        memcpy(mData, tmpData, szB);
      }
      delete [] tmpData;
    } else {
      size_t integralImageStride = GetAlignedStride<16>(integralImageSize.width * 4);

      // We need to leave room for an additional 12 bytes for a maximum overrun
      // of 3 pixels in the blurring code.
      AlignedArray<uint32_t> integralImage((integralImageStride / 4) * integralImageSize.height + 12);

#ifdef USE_SSE2
      if (Factory::HasSSE2()) {
        BoxBlur_SSE2(horizontalLobes[0][0], horizontalLobes[0][1], verticalLobes[0][0],
                     verticalLobes[0][1], integralImage, integralImageStride);
        BoxBlur_SSE2(horizontalLobes[1][0], horizontalLobes[1][1], verticalLobes[1][0],
                     verticalLobes[1][1], integralImage, integralImageStride);
        BoxBlur_SSE2(horizontalLobes[2][0], horizontalLobes[2][1], verticalLobes[2][0],
                     verticalLobes[2][1], integralImage, integralImageStride);
      } else
#endif
      {
        BoxBlur_C(horizontalLobes[0][0], horizontalLobes[0][1], verticalLobes[0][0],
                  verticalLobes[0][1], integralImage, integralImageStride);
        BoxBlur_C(horizontalLobes[1][0], horizontalLobes[1][1], verticalLobes[1][0],
                  verticalLobes[1][1], integralImage, integralImageStride);
        BoxBlur_C(horizontalLobes[2][0], horizontalLobes[2][1], verticalLobes[2][0],
                  verticalLobes[2][1], integralImage, integralImageStride);
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
AlphaBoxBlur::BoxBlur_C(int32_t aLeftLobe,
                        int32_t aRightLobe,
                        int32_t aTopLobe,
                        int32_t aBottomLobe,
                        uint32_t *aIntegralImage,
                        size_t aIntegralImageStride)
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

  uint32_t stride32bit = aIntegralImageStride / 4;

  int32_t leftInflation = RoundUpToMultipleOf4(aLeftLobe).value();

  GenerateIntegralImage_C(leftInflation, aRightLobe, aTopLobe, aBottomLobe,
                          aIntegralImage, aIntegralImageStride, mData,
                          mStride, size);

  uint32_t reciprocal = uint32_t((uint64_t(1) << 32) / boxSize);

  uint32_t *innerIntegral = aIntegralImage + (aTopLobe * stride32bit) + leftInflation;

  // Storing these locally makes this about 30% faster! Presumably the compiler
  // can't be sure we're not altering the member variables in this loop.
  IntRect skipRect = mSkipRect;
  uint8_t *data = mData;
  int32_t stride = mStride;
  for (int32_t y = 0; y < size.height; y++) {
    bool inSkipRectY = y > skipRect.y && y < skipRect.YMost();

    uint32_t *topLeftBase = innerIntegral + ((y - aTopLobe) * stride32bit - aLeftLobe);
    uint32_t *topRightBase = innerIntegral + ((y - aTopLobe) * stride32bit + aRightLobe);
    uint32_t *bottomRightBase = innerIntegral + ((y + aBottomLobe) * stride32bit + aRightLobe);
    uint32_t *bottomLeftBase = innerIntegral + ((y + aBottomLobe) * stride32bit - aLeftLobe);

    for (int32_t x = 0; x < size.width; x++) {
      if (inSkipRectY && x > skipRect.x && x < skipRect.XMost()) {
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

      data[stride * y + x] = (uint64_t(reciprocal) * value) >> 32;
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
    IntSize size(static_cast<int32_t>(floor(aStd.x * GAUSSIAN_SCALE_FACTOR + 0.5)),
                 static_cast<int32_t>(floor(aStd.y * GAUSSIAN_SCALE_FACTOR + 0.5)));

    return size;
}

}
}
