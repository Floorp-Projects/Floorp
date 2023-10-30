/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Downscaler.h"

#include <algorithm>
#include <ctime>

#include "mozilla/gfx/2D.h"

using std::swap;

namespace mozilla {

using gfx::IntRect;

namespace image {

Downscaler::Downscaler(const nsIntSize& aTargetSize)
    : mTargetSize(aTargetSize),
      mOutputBuffer(nullptr),
      mWindowCapacity(0),
      mLinesInBuffer(0),
      mPrevInvalidatedLine(0),
      mCurrentOutLine(0),
      mCurrentInLine(0),
      mHasAlpha(true),
      mFlipVertically(false) {
  MOZ_ASSERT(mTargetSize.width > 0 && mTargetSize.height > 0,
             "Invalid target size");
}

Downscaler::~Downscaler() { ReleaseWindow(); }

void Downscaler::ReleaseWindow() {
  if (!mWindow) {
    return;
  }

  for (int32_t i = 0; i < mWindowCapacity; ++i) {
    delete[] mWindow[i];
  }

  mWindow = nullptr;
  mWindowCapacity = 0;
}

nsresult Downscaler::BeginFrame(const nsIntSize& aOriginalSize,
                                const Maybe<nsIntRect>& aFrameRect,
                                uint8_t* aOutputBuffer, bool aHasAlpha,
                                bool aFlipVertically /* = false */) {
  MOZ_ASSERT(aOutputBuffer);
  MOZ_ASSERT(mTargetSize != aOriginalSize,
             "Created a downscaler, but not downscaling?");
  MOZ_ASSERT(mTargetSize.width <= aOriginalSize.width,
             "Created a downscaler, but width is larger");
  MOZ_ASSERT(mTargetSize.height <= aOriginalSize.height,
             "Created a downscaler, but height is larger");
  MOZ_ASSERT(aOriginalSize.width > 0 && aOriginalSize.height > 0,
             "Invalid original size");

  // Only downscale from reasonable sizes to avoid using too much memory/cpu
  // downscaling and decoding. 1 << 20 == 1,048,576 seems a reasonable limit.
  if (aOriginalSize.width > (1 << 20) || aOriginalSize.height > (1 << 20)) {
    NS_WARNING("Trying to downscale image frame that is too large");
    return NS_ERROR_INVALID_ARG;
  }

  mFrameRect = aFrameRect.valueOr(nsIntRect(nsIntPoint(), aOriginalSize));
  MOZ_ASSERT(mFrameRect.X() >= 0 && mFrameRect.Y() >= 0 &&
                 mFrameRect.Width() >= 0 && mFrameRect.Height() >= 0,
             "Frame rect must have non-negative components");
  MOZ_ASSERT(nsIntRect(0, 0, aOriginalSize.width, aOriginalSize.height)
                 .Contains(mFrameRect),
             "Frame rect must fit inside image");
  MOZ_ASSERT_IF(!nsIntRect(0, 0, aOriginalSize.width, aOriginalSize.height)
                     .IsEqualEdges(mFrameRect),
                aHasAlpha);

  mOriginalSize = aOriginalSize;
  mScale = gfx::MatrixScalesDouble(
      double(mOriginalSize.width) / mTargetSize.width,
      double(mOriginalSize.height) / mTargetSize.height);
  mOutputBuffer = aOutputBuffer;
  mHasAlpha = aHasAlpha;
  mFlipVertically = aFlipVertically;

  ReleaseWindow();

  auto resizeMethod = gfx::ConvolutionFilter::ResizeMethod::LANCZOS3;
  if (!mXFilter.ComputeResizeFilter(resizeMethod, mOriginalSize.width,
                                    mTargetSize.width) ||
      !mYFilter.ComputeResizeFilter(resizeMethod, mOriginalSize.height,
                                    mTargetSize.height)) {
    NS_WARNING("Failed to compute filters for image downscaling");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Allocate the buffer, which contains scanlines of the original image.
  // pad to handle overreads by the simd code
  size_t bufferLen = gfx::ConvolutionFilter::PadBytesForSIMD(
      mOriginalSize.width * sizeof(uint32_t));
  mRowBuffer.reset(new (fallible) uint8_t[bufferLen]);
  if (MOZ_UNLIKELY(!mRowBuffer)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Zero buffer to keep valgrind happy.
  memset(mRowBuffer.get(), 0, bufferLen);

  // Allocate the window, which contains horizontally downscaled scanlines. (We
  // can store scanlines which are already downscale because our downscaling
  // filter is separable.)
  mWindowCapacity = mYFilter.MaxFilter();
  mWindow.reset(new (fallible) uint8_t*[mWindowCapacity]);
  if (MOZ_UNLIKELY(!mWindow)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  bool anyAllocationFailed = false;
  // pad to handle overreads by the simd code
  const size_t rowSize = gfx::ConvolutionFilter::PadBytesForSIMD(
      mTargetSize.width * sizeof(uint32_t));
  for (int32_t i = 0; i < mWindowCapacity; ++i) {
    mWindow[i] = new (fallible) uint8_t[rowSize];
    anyAllocationFailed = anyAllocationFailed || mWindow[i] == nullptr;
  }

  if (MOZ_UNLIKELY(anyAllocationFailed)) {
    // We intentionally iterate through the entire array even if an allocation
    // fails, to ensure that all the pointers in it are either valid or nullptr.
    // That in turn ensures that ReleaseWindow() can clean up correctly.
    return NS_ERROR_OUT_OF_MEMORY;
  }

  ResetForNextProgressivePass();

  return NS_OK;
}

void Downscaler::SkipToRow(int32_t aRow) {
  if (mCurrentInLine < aRow) {
    ClearRow();
    do {
      CommitRow();
    } while (mCurrentInLine < aRow);
  }
}

void Downscaler::ResetForNextProgressivePass() {
  mPrevInvalidatedLine = 0;
  mCurrentOutLine = 0;
  mCurrentInLine = 0;
  mLinesInBuffer = 0;

  if (mFrameRect.IsEmpty()) {
    // Our frame rect is zero size; commit rows until the end of the image.
    SkipToRow(mOriginalSize.height - 1);
  } else {
    // If we have a vertical offset, commit rows to shift us past it.
    SkipToRow(mFrameRect.Y());
  }
}

void Downscaler::ClearRestOfRow(uint32_t aStartingAtCol) {
  MOZ_ASSERT(int64_t(aStartingAtCol) <= int64_t(mOriginalSize.width));
  uint32_t bytesToClear =
      (mOriginalSize.width - aStartingAtCol) * sizeof(uint32_t);
  memset(mRowBuffer.get() + (aStartingAtCol * sizeof(uint32_t)), 0,
         bytesToClear);
}

void Downscaler::CommitRow() {
  MOZ_ASSERT(mOutputBuffer, "Should have a current frame");
  MOZ_ASSERT(mCurrentInLine < mOriginalSize.height, "Past end of input");

  if (mCurrentOutLine < mTargetSize.height) {
    int32_t filterOffset = 0;
    int32_t filterLength = 0;
    mYFilter.GetFilterOffsetAndLength(mCurrentOutLine, &filterOffset,
                                      &filterLength);

    int32_t inLineToRead = filterOffset + mLinesInBuffer;
    MOZ_ASSERT(mCurrentInLine <= inLineToRead, "Reading past end of input");
    if (mCurrentInLine == inLineToRead) {
      MOZ_RELEASE_ASSERT(mLinesInBuffer < mWindowCapacity,
                         "Need more rows than capacity!");
      mXFilter.ConvolveHorizontally(mRowBuffer.get(), mWindow[mLinesInBuffer++],
                                    mHasAlpha);
    }

    MOZ_ASSERT(mCurrentOutLine < mTargetSize.height,
               "Writing past end of output");

    while (mLinesInBuffer >= filterLength) {
      DownscaleInputLine();

      if (mCurrentOutLine == mTargetSize.height) {
        break;  // We're done.
      }

      mYFilter.GetFilterOffsetAndLength(mCurrentOutLine, &filterOffset,
                                        &filterLength);
    }
  }

  mCurrentInLine += 1;

  // If we're at the end of the part of the original image that has data, commit
  // rows to shift us to the end.
  if (mCurrentInLine == (mFrameRect.Y() + mFrameRect.Height())) {
    SkipToRow(mOriginalSize.height - 1);
  }
}

bool Downscaler::HasInvalidation() const {
  return mCurrentOutLine > mPrevInvalidatedLine;
}

DownscalerInvalidRect Downscaler::TakeInvalidRect() {
  if (MOZ_UNLIKELY(!HasInvalidation())) {
    return DownscalerInvalidRect();
  }

  DownscalerInvalidRect invalidRect;

  // Compute the target size invalid rect.
  if (mFlipVertically) {
    // We need to flip it. This will implicitly flip the original size invalid
    // rect, since we compute it by scaling this rect.
    invalidRect.mTargetSizeRect =
        IntRect(0, mTargetSize.height - mCurrentOutLine, mTargetSize.width,
                mCurrentOutLine - mPrevInvalidatedLine);
  } else {
    invalidRect.mTargetSizeRect =
        IntRect(0, mPrevInvalidatedLine, mTargetSize.width,
                mCurrentOutLine - mPrevInvalidatedLine);
  }

  mPrevInvalidatedLine = mCurrentOutLine;

  // Compute the original size invalid rect.
  invalidRect.mOriginalSizeRect = invalidRect.mTargetSizeRect;
  invalidRect.mOriginalSizeRect.ScaleRoundOut(mScale.xScale, mScale.yScale);

  return invalidRect;
}

void Downscaler::DownscaleInputLine() {
  MOZ_ASSERT(mOutputBuffer);
  MOZ_ASSERT(mCurrentOutLine < mTargetSize.height,
             "Writing past end of output");

  int32_t filterOffset = 0;
  int32_t filterLength = 0;
  mYFilter.GetFilterOffsetAndLength(mCurrentOutLine, &filterOffset,
                                    &filterLength);

  int32_t currentOutLine = mFlipVertically
                               ? mTargetSize.height - (mCurrentOutLine + 1)
                               : mCurrentOutLine;
  MOZ_ASSERT(currentOutLine >= 0);

  uint8_t* outputLine =
      &mOutputBuffer[currentOutLine * mTargetSize.width * sizeof(uint32_t)];
  mYFilter.ConvolveVertically(mWindow.get(), outputLine, mCurrentOutLine,
                              mXFilter.NumValues(), mHasAlpha);

  mCurrentOutLine += 1;

  if (mCurrentOutLine == mTargetSize.height) {
    // We're done.
    return;
  }

  int32_t newFilterOffset = 0;
  int32_t newFilterLength = 0;
  mYFilter.GetFilterOffsetAndLength(mCurrentOutLine, &newFilterOffset,
                                    &newFilterLength);

  int diff = newFilterOffset - filterOffset;
  MOZ_ASSERT(diff >= 0, "Moving backwards in the filter?");

  // Shift the buffer. We're just moving pointers here, so this is cheap.
  mLinesInBuffer -= diff;
  mLinesInBuffer = std::min(std::max(mLinesInBuffer, 0), mWindowCapacity);

  // If we already have enough rows to satisfy the filter, there is no need
  // to swap as we won't be writing more before the next convolution.
  if (filterLength > mLinesInBuffer) {
    for (int32_t i = 0; i < mLinesInBuffer; ++i) {
      swap(mWindow[i], mWindow[filterLength - mLinesInBuffer + i]);
    }
  }
}

}  // namespace image
}  // namespace mozilla
