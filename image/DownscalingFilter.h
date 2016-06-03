/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * DownscalingSurfaceFilter is a SurfaceFilter implementation for use with
 * SurfacePipe which performs Lanczos downscaling.
 *
 * It's in this header file, separated from the other SurfaceFilters, because
 * some preprocessor magic is necessary to ensure that there aren't compilation
 * issues on platforms where Skia is unavailable.
 */

#ifndef mozilla_image_DownscalingFilter_h
#define mozilla_image_DownscalingFilter_h

#include <algorithm>
#include <ctime>
#include <stdint.h>
#include <string.h>

#include "mozilla/Maybe.h"
#include "mozilla/SSE.h"
#include "mozilla/mips.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/gfx/2D.h"
#include "gfxPrefs.h"

#ifdef MOZ_ENABLE_SKIA
#include "convolver.h"
#include "image_operations.h"
#include "skia/include/core/SkTypes.h"
#endif

#include "SurfacePipe.h"

namespace mozilla {
namespace image {

//////////////////////////////////////////////////////////////////////////////
// DownscalingFilter
//////////////////////////////////////////////////////////////////////////////

template <typename Next> class DownscalingFilter;

/**
 * A configuration struct for DownscalingConfig.
 */
struct DownscalingConfig
{
  template <typename Next> using Filter = DownscalingFilter<Next>;
  gfx::IntSize mInputSize;     /// The size of the input image. We'll downscale
                               /// from this size to the input size of the next
                               /// SurfaceFilter in the chain.
  gfx::SurfaceFormat mFormat;  /// The pixel format - BGRA or BGRX. (BGRX has
                               /// slightly better performance.)
};

#ifndef MOZ_ENABLE_SKIA

/**
 * DownscalingFilter requires Skia. This is a fallback implementation for
 * non-Skia builds that fails when Configure() is called (which will prevent
 * SurfacePipeFactory from returning an instance of it) and crashes if a caller
 * manually constructs an instance and attempts to actually use it. Callers
 * should avoid this by ensuring that they do not request downscaling in
 * non-Skia builds.
 */
template <typename Next>
class DownscalingFilter final : public SurfaceFilter
{
public:
  Maybe<SurfaceInvalidRect> TakeInvalidRect() override { return Nothing(); }

  template <typename... Rest>
  nsresult Configure(const DownscalingConfig& aConfig, Rest... aRest)
  {
    return NS_ERROR_FAILURE;
  }

protected:
  uint8_t* DoResetToFirstRow() override { MOZ_CRASH(); return nullptr; }
  uint8_t* DoAdvanceRow() override { MOZ_CRASH(); return nullptr; }
};

#else

/**
 * DownscalingFilter performs Lanczos downscaling, taking image input data at one size
 * and outputting it rescaled to a different size.
 *
 * The 'Next' template parameter specifies the next filter in the chain.
 */
template <typename Next>
class DownscalingFilter final : public SurfaceFilter
{
public:
  DownscalingFilter()
    : mXFilter(MakeUnique<skia::ConvolutionFilter1D>())
    , mYFilter(MakeUnique<skia::ConvolutionFilter1D>())
    , mWindowCapacity(0)
    , mRowsInWindow(0)
    , mInputRow(0)
    , mOutputRow(0)
    , mHasAlpha(true)
  {
    MOZ_ASSERT(gfxPrefs::ImageDownscaleDuringDecodeEnabled(),
               "Downscaling even though downscale-during-decode is disabled?");
  }

  ~DownscalingFilter()
  {
    ReleaseWindow();
  }

  template <typename... Rest>
  nsresult Configure(const DownscalingConfig& aConfig, Rest... aRest)
  {
    nsresult rv = mNext.Configure(aRest...);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (mNext.IsValidPalettedPipe()) {
      NS_WARNING("Created a downscaler for a paletted surface?");
      return NS_ERROR_INVALID_ARG;
    }
    if (mNext.InputSize() == aConfig.mInputSize) {
      NS_WARNING("Created a downscaler, but not downscaling?");
      return NS_ERROR_INVALID_ARG;
    }
    if (mNext.InputSize().width > aConfig.mInputSize.width) {
      NS_WARNING("Created a downscaler, but width is larger");
      return NS_ERROR_INVALID_ARG;
    }
    if (mNext.InputSize().height > aConfig.mInputSize.height) {
      NS_WARNING("Created a downscaler, but height is larger");
      return NS_ERROR_INVALID_ARG;
    }
    if (aConfig.mInputSize.width <= 0 || aConfig.mInputSize.height <= 0) {
      NS_WARNING("Invalid input size for DownscalingFilter");
      return NS_ERROR_INVALID_ARG;
    }

    mInputSize = aConfig.mInputSize;
    gfx::IntSize outputSize = mNext.InputSize();
    mScale = gfxSize(double(mInputSize.width) / outputSize.width,
                     double(mInputSize.height) / outputSize.height);
    mHasAlpha = aConfig.mFormat == gfx::SurfaceFormat::B8G8R8A8;

    ReleaseWindow();

    auto resizeMethod = skia::ImageOperations::RESIZE_LANCZOS3;

    skia::resize::ComputeFilters(resizeMethod,
                                 mInputSize.width, outputSize.width,
                                 0, outputSize.width,
                                 mXFilter.get());

    skia::resize::ComputeFilters(resizeMethod,
                                 mInputSize.height, outputSize.height,
                                 0, outputSize.height,
                                 mYFilter.get());

    // Allocate the buffer, which contains scanlines of the input image.
    mRowBuffer.reset(new (fallible)
                       uint8_t[PaddedWidthInBytes(mInputSize.width)]);
    if (MOZ_UNLIKELY(!mRowBuffer)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Clear the buffer to avoid writing uninitialized memory to the output.
    memset(mRowBuffer.get(), 0, PaddedWidthInBytes(mInputSize.width));

    // Allocate the window, which contains horizontally downscaled scanlines. (We
    // can store scanlines which are already downscaled because our downscaling
    // filter is separable.)
    mWindowCapacity = mYFilter->max_filter();
    mWindow.reset(new (fallible) uint8_t*[mWindowCapacity]);
    if (MOZ_UNLIKELY(!mWindow)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Allocate the "window" of recent rows that we keep in memory as input for
    // the downscaling code. We intentionally iterate through the entire array
    // even if an allocation fails, to ensure that all the pointers in it are
    // either valid or nullptr. That in turn ensures that ReleaseWindow() can
    // clean up correctly.
    bool anyAllocationFailed = false;
    const uint32_t windowRowSizeInBytes = PaddedWidthInBytes(outputSize.width);
    for (int32_t i = 0; i < mWindowCapacity; ++i) {
      mWindow[i] = new (fallible) uint8_t[windowRowSizeInBytes];
      anyAllocationFailed = anyAllocationFailed || mWindow[i] == nullptr;
    }

    if (MOZ_UNLIKELY(anyAllocationFailed)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    ConfigureFilter(mInputSize, sizeof(uint32_t));
    return NS_OK;
  }

  Maybe<SurfaceInvalidRect> TakeInvalidRect() override
  {
    Maybe<SurfaceInvalidRect> invalidRect = mNext.TakeInvalidRect();

    if (invalidRect) {
      // Compute the input space invalid rect by scaling.
      invalidRect->mInputSpaceRect.ScaleRoundOut(mScale.width, mScale.height);
    }

    return invalidRect;
  }

protected:
  uint8_t* DoResetToFirstRow() override
  {
    mNext.ResetToFirstRow();

    mInputRow = 0;
    mOutputRow = 0;
    mRowsInWindow = 0;

    return GetRowPointer();
  }

  uint8_t* DoAdvanceRow() override
  {
    if (mInputRow >= mInputSize.height) {
      NS_WARNING("Advancing DownscalingFilter past the end of the input");
      return nullptr;
    }

    if (mOutputRow >= mNext.InputSize().height) {
      NS_WARNING("Advancing DownscalingFilter past the end of the output");
      return nullptr;
    }

    int32_t filterOffset = 0;
    int32_t filterLength = 0;
    GetFilterOffsetAndLength(mYFilter, mOutputRow,
                             &filterOffset, &filterLength);

    int32_t inputRowToRead = filterOffset + mRowsInWindow;
    MOZ_ASSERT(mInputRow <= inputRowToRead, "Reading past end of input");
    if (mInputRow == inputRowToRead) {
      skia::ConvolveHorizontally(mRowBuffer.get(), *mXFilter,
                                 mWindow[mRowsInWindow++], mHasAlpha,
                                 supports_sse2() || supports_mmi());
    }

    MOZ_ASSERT(mOutputRow < mNext.InputSize().height,
               "Writing past end of output");

    while (mRowsInWindow == filterLength) {
      DownscaleInputRow();

      if (mOutputRow == mNext.InputSize().height) {
        break;  // We're done.
      }

      GetFilterOffsetAndLength(mYFilter, mOutputRow,
                               &filterOffset, &filterLength);
    }

    mInputRow++;

    return mInputRow < mInputSize.height ? GetRowPointer()
                                         : nullptr;
  }

private:
  uint8_t* GetRowPointer() const { return mRowBuffer.get(); }

  static uint32_t PaddedWidthInBytes(uint32_t aLogicalWidth)
  {
    // Convert from width in BGRA/BGRX pixels to width in bytes, padding by 15
    // to handle overreads by the SIMD code inside Skia.
    return aLogicalWidth * sizeof(uint32_t) + 15;
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

  void DownscaleInputRow()
  {
    typedef skia::ConvolutionFilter1D::Fixed FilterValue;

    MOZ_ASSERT(mOutputRow < mNext.InputSize().height,
               "Writing past end of output");

    int32_t filterOffset = 0;
    int32_t filterLength = 0;
    MOZ_ASSERT(mOutputRow < mYFilter->num_values());
    auto filterValues =
      mYFilter->FilterForValue(mOutputRow, &filterOffset, &filterLength);

    mNext.template WriteUnsafeComputedRow<uint32_t>([&](uint32_t* aRow,
                                                        uint32_t aLength) {
      skia::ConvolveVertically(static_cast<const FilterValue*>(filterValues),
                               filterLength, mWindow.get(), mXFilter->num_values(),
                               reinterpret_cast<uint8_t*>(aRow), mHasAlpha,
                               supports_sse2() || supports_mmi());
    });

    mOutputRow++;

    if (mOutputRow == mNext.InputSize().height) {
      return;  // We're done.
    }

    int32_t newFilterOffset = 0;
    int32_t newFilterLength = 0;
    GetFilterOffsetAndLength(mYFilter, mOutputRow,
                             &newFilterOffset, &newFilterLength);

    int diff = newFilterOffset - filterOffset;
    MOZ_ASSERT(diff >= 0, "Moving backwards in the filter?");

    // Shift the buffer. We're just moving pointers here, so this is cheap.
    mRowsInWindow -= diff;
    mRowsInWindow = std::max(mRowsInWindow, 0);
    for (int32_t i = 0; i < mRowsInWindow; ++i) {
      std::swap(mWindow[i], mWindow[filterLength - mRowsInWindow + i]);
    }
  }

  void ReleaseWindow()
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

  Next mNext;                       /// The next SurfaceFilter in the chain.

  gfx::IntSize mInputSize;          /// The size of the input image.
  gfxSize mScale;                   /// The scale factors in each dimension.
                                    /// Computed from @mInputSize and
                                    /// the next filter's input size.

  UniquePtr<uint8_t[]> mRowBuffer;  /// The buffer into which input is written.
  UniquePtr<uint8_t*[]> mWindow;    /// The last few rows which were written.

  UniquePtr<skia::ConvolutionFilter1D> mXFilter;  /// The Lanczos filter in X.
  UniquePtr<skia::ConvolutionFilter1D> mYFilter;  /// The Lanczos filter in Y.

  int32_t mWindowCapacity;  /// How many rows the window contains.

  int32_t mRowsInWindow;    /// How many rows we've buffered in the window.
  int32_t mInputRow;        /// The current row we're reading. (0-indexed)
  int32_t mOutputRow;       /// The current row we're writing. (0-indexed)

  bool mHasAlpha;           /// If true, the image has transparency.
};

#endif

} // namespace image
} // namespace mozilla

#endif // mozilla_image_DownscalingFilter_h
