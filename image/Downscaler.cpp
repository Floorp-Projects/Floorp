/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Downscaler.h"

#include <algorithm>
#include <ctime>
#include "gfxPrefs.h"
#include "image_operations.h"
#include "mozilla/SSE.h"
#include "convolver.h"
#include "skia/include/core/SkTypes.h"

using std::max;
using std::swap;

namespace mozilla {
namespace image {

Downscaler::Downscaler(const nsIntSize& aTargetSize)
  : mTargetSize(aTargetSize)
  , mOutputBuffer(nullptr)
  , mXFilter(MakeUnique<skia::ConvolutionFilter1D>())
  , mYFilter(MakeUnique<skia::ConvolutionFilter1D>())
  , mWindowCapacity(0)
  , mHasAlpha(true)
  , mFlipVertically(false)
{
  MOZ_ASSERT(gfxPrefs::ImageDownscaleDuringDecodeEnabled(),
             "Downscaling even though downscale-during-decode is disabled?");
  MOZ_ASSERT(mTargetSize.width > 0 && mTargetSize.height > 0,
             "Invalid target size");
}

Downscaler::~Downscaler()
{
  ReleaseWindow();
}

void
Downscaler::ReleaseWindow()
{
  if (!mWindow) {
    return;
  }

  for (int32_t i = 0; i < mWindowCapacity; ++i) {
    delete[] mWindow[i];
  }

  mWindow = nullptr;
  mWindowCapacity = 0;
}

nsresult
Downscaler::BeginFrame(const nsIntSize& aOriginalSize,
                       uint8_t* aOutputBuffer,
                       bool aHasAlpha,
                       bool aFlipVertically /* = false */)
{
  MOZ_ASSERT(aOutputBuffer);
  MOZ_ASSERT(mTargetSize != aOriginalSize,
             "Created a downscaler, but not downscaling?");
  MOZ_ASSERT(mTargetSize.width <= aOriginalSize.width,
             "Created a downscaler, but width is larger");
  MOZ_ASSERT(mTargetSize.height <= aOriginalSize.height,
             "Created a downscaler, but height is larger");
  MOZ_ASSERT(aOriginalSize.width > 0 && aOriginalSize.height > 0,
             "Invalid original size");

  mOriginalSize = aOriginalSize;
  mScale = gfxSize(double(mOriginalSize.width) / mTargetSize.width,
                   double(mOriginalSize.height) / mTargetSize.height);
  mOutputBuffer = aOutputBuffer;
  mHasAlpha = aHasAlpha;
  mFlipVertically = aFlipVertically;

  ResetForNextProgressivePass();
  ReleaseWindow();

  auto resizeMethod = skia::ImageOperations::RESIZE_LANCZOS3;

  skia::resize::ComputeFilters(resizeMethod,
                               mOriginalSize.width, mTargetSize.width,
                               0, mTargetSize.width,
                               mXFilter.get());

  skia::resize::ComputeFilters(resizeMethod,
                               mOriginalSize.height, mTargetSize.height,
                               0, mTargetSize.height,
                               mYFilter.get());

  // Allocate the buffer, which contains scanlines of the original image.
  // pad by 15 to handle overreads by the simd code
  mRowBuffer = MakeUnique<uint8_t[]>(mOriginalSize.width * sizeof(uint32_t) + 15);
  if (MOZ_UNLIKELY(!mRowBuffer)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Allocate the window, which contains horizontally downscaled scanlines. (We
  // can store scanlines which are already downscale because our downscaling
  // filter is separable.)
  mWindowCapacity = mYFilter->max_filter();
  mWindow = MakeUnique<uint8_t*[]>(mWindowCapacity);
  if (MOZ_UNLIKELY(!mWindow)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  bool anyAllocationFailed = false;
  // pad by 15 to handle overreads by the simd code
  const int rowSize = mTargetSize.width * sizeof(uint32_t) + 15;
  for (int32_t i = 0; i < mWindowCapacity; ++i) {
    mWindow[i] = new uint8_t[rowSize];
    anyAllocationFailed = anyAllocationFailed || mWindow[i] == nullptr;
  }

  if (MOZ_UNLIKELY(anyAllocationFailed)) {
    // We intentionally iterate through the entire array even if an allocation
    // fails, to ensure that all the pointers in it are either valid or nullptr.
    // That in turn ensures that ReleaseWindow() can clean up correctly.
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

void
Downscaler::ResetForNextProgressivePass()
{
  mPrevInvalidatedLine = 0;
  mCurrentOutLine = 0;
  mCurrentInLine = 0;
  mLinesInBuffer = 0;
}

static void
GetFilterOffsetAndLength(UniquePtr<skia::ConvolutionFilter1D>& aFilter,
                         int32_t aOutputImagePosition,
                         int32_t* aFilterOffsetOut,
                         int32_t* aFilterLengthOut)
{
  MOZ_ASSERT(aOutputImagePosition < aFilter->num_values());
  aFilter->FilterForValue(aOutputImagePosition,
                          aFilterOffsetOut,
                          aFilterLengthOut);
}

void
Downscaler::ClearRow(uint32_t aStartingAtCol)
{
  MOZ_ASSERT(int64_t(mOriginalSize.width) > int64_t(aStartingAtCol));
  uint32_t bytesToClear = (mOriginalSize.width - aStartingAtCol)
                        * sizeof(uint32_t);
  memset(mRowBuffer.get() + (aStartingAtCol * sizeof(uint32_t)),
         0, bytesToClear);
}

void
Downscaler::CommitRow()
{
  MOZ_ASSERT(mOutputBuffer, "Should have a current frame");
  MOZ_ASSERT(mCurrentInLine < mOriginalSize.height, "Past end of input");
  MOZ_ASSERT(mCurrentOutLine < mTargetSize.height, "Past end of output");

  int32_t filterOffset = 0;
  int32_t filterLength = 0;
  GetFilterOffsetAndLength(mYFilter, mCurrentOutLine,
                           &filterOffset, &filterLength);

  int32_t inLineToRead = filterOffset + mLinesInBuffer;
  MOZ_ASSERT(mCurrentInLine <= inLineToRead, "Reading past end of input");
  if (mCurrentInLine == inLineToRead) {
    skia::ConvolveHorizontally(mRowBuffer.get(), *mXFilter,
                               mWindow[mLinesInBuffer++], mHasAlpha,
                               supports_sse2());
  }

  MOZ_ASSERT(mCurrentOutLine < mTargetSize.height,
             "Writing past end of output");

  while (mLinesInBuffer == filterLength) {
    DownscaleInputLine();

    if (mCurrentOutLine == mTargetSize.height) {
      break;  // We're done.
    }

    GetFilterOffsetAndLength(mYFilter, mCurrentOutLine,
                             &filterOffset, &filterLength);
  }

  mCurrentInLine += 1;
}

bool
Downscaler::HasInvalidation() const
{
  return mCurrentOutLine > mPrevInvalidatedLine;
}

DownscalerInvalidRect
Downscaler::TakeInvalidRect()
{
  if (MOZ_UNLIKELY(!HasInvalidation())) {
    return DownscalerInvalidRect();
  }

  DownscalerInvalidRect invalidRect;

  // Compute the target size invalid rect.
  if (mFlipVertically) {
    // We need to flip it. This will implicitly flip the original size invalid
    // rect, since we compute it by scaling this rect.
    invalidRect.mTargetSizeRect =
      IntRect(0, mTargetSize.height - mCurrentOutLine,
              mTargetSize.width, mCurrentOutLine - mPrevInvalidatedLine);
  } else {
    invalidRect.mTargetSizeRect =
      IntRect(0, mPrevInvalidatedLine,
              mTargetSize.width, mCurrentOutLine - mPrevInvalidatedLine);
  }

  mPrevInvalidatedLine = mCurrentOutLine;

  // Compute the original size invalid rect.
  invalidRect.mOriginalSizeRect = invalidRect.mTargetSizeRect;
  invalidRect.mOriginalSizeRect.ScaleRoundOut(mScale.width, mScale.height);

  return invalidRect;
}

void
Downscaler::DownscaleInputLine()
{
  typedef skia::ConvolutionFilter1D::Fixed FilterValue;

  MOZ_ASSERT(mOutputBuffer);
  MOZ_ASSERT(mCurrentOutLine < mTargetSize.height,
             "Writing past end of output");

  int32_t filterOffset = 0;
  int32_t filterLength = 0;
  MOZ_ASSERT(mCurrentOutLine < mYFilter->num_values());
  auto filterValues =
    mYFilter->FilterForValue(mCurrentOutLine, &filterOffset, &filterLength);

  int32_t currentOutLine = mFlipVertically
                         ? mTargetSize.height - (mCurrentOutLine + 1)
                         : mCurrentOutLine;
  MOZ_ASSERT(currentOutLine >= 0);

  uint8_t* outputLine =
    &mOutputBuffer[currentOutLine * mTargetSize.width * sizeof(uint32_t)];
  skia::ConvolveVertically(static_cast<const FilterValue*>(filterValues),
                           filterLength, mWindow.get(), mXFilter->num_values(),
                           outputLine, mHasAlpha, supports_sse2());

  mCurrentOutLine += 1;

  if (mCurrentOutLine == mTargetSize.height) {
    // We're done.
    return;
  }

  int32_t newFilterOffset = 0;
  int32_t newFilterLength = 0;
  GetFilterOffsetAndLength(mYFilter, mCurrentOutLine,
                           &newFilterOffset, &newFilterLength);

  int diff = newFilterOffset - filterOffset;
  MOZ_ASSERT(diff >= 0, "Moving backwards in the filter?");

  // Shift the buffer. We're just moving pointers here, so this is cheap.
  mLinesInBuffer -= diff;
  mLinesInBuffer = max(mLinesInBuffer, 0);
  for (int32_t i = 0; i < mLinesInBuffer; ++i) {
    swap(mWindow[i], mWindow[filterLength - mLinesInBuffer + i]);
  }
}

Deinterlacer::Deinterlacer(const nsIntSize& aImageSize)
  : mImageSize(aImageSize)
  , mBuffer(MakeUnique<uint8_t[]>(mImageSize.width *
                                  mImageSize.height *
                                  sizeof(uint32_t)))
{ }

uint32_t
Deinterlacer::RowSize() const
{
  return mImageSize.width * sizeof(uint32_t);
}

uint8_t*
Deinterlacer::RowBuffer(uint32_t aRow)
{
  uint32_t offset = aRow * RowSize();
  MOZ_ASSERT(offset < mImageSize.width * mImageSize.height * sizeof(uint32_t),
             "Row is outside of image");
  return mBuffer.get() + offset;
}

void
Deinterlacer::PropagatePassToDownscaler(Downscaler& aDownscaler)
{
  for (int32_t row = 0 ; row < mImageSize.height ; ++row) {
    memcpy(aDownscaler.RowBuffer(), RowBuffer(row), RowSize());
    aDownscaler.CommitRow();
  }
}

} // namespace image
} // namespace mozilla
