/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This header contains various SurfaceFilter implementations that apply
 * transformations to image data, for usage with SurfacePipe.
 */

#ifndef mozilla_image_SurfaceFilters_h
#define mozilla_image_SurfaceFilters_h

#include <stdint.h>
#include <string.h>

#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/gfx/2D.h"

#include "DownscalingFilter.h"
#include "SurfacePipe.h"

namespace mozilla {
namespace image {

//////////////////////////////////////////////////////////////////////////////
// DeinterlacingFilter
//////////////////////////////////////////////////////////////////////////////

template <typename PixelType, typename Next> class DeinterlacingFilter;

/**
 * A configuration struct for DeinterlacingFilter.
 *
 * The 'PixelType' template parameter should be either uint32_t (for output to a
 * SurfaceSink) or uint8_t (for output to a PalettedSurfaceSink).
 */
template <typename PixelType>
struct DeinterlacingConfig
{
  template <typename Next> using Filter = DeinterlacingFilter<PixelType, Next>;
  bool mProgressiveDisplay; /// If true, duplicate rows during deinterlacing
                            /// to make progressive display look better, at
                            /// the cost of some performance.
};

/**
 * DeinterlacingFilter performs deinterlacing by reordering the rows that are
 * written to it.
 *
 * The 'PixelType' template parameter should be either uint32_t (for output to a
 * SurfaceSink) or uint8_t (for output to a PalettedSurfaceSink).
 *
 * The 'Next' template parameter specifies the next filter in the chain.
 */
template <typename PixelType, typename Next>
class DeinterlacingFilter final : public SurfaceFilter
{
public:
  DeinterlacingFilter()
    : mInputRow(0)
    , mOutputRow(0)
    , mPass(0)
    , mProgressiveDisplay(true)
  { }

  template <typename... Rest>
  nsresult Configure(const DeinterlacingConfig<PixelType>& aConfig, Rest... aRest)
  {
    nsresult rv = mNext.Configure(aRest...);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (sizeof(PixelType) == 1 && !mNext.IsValidPalettedPipe()) {
      NS_WARNING("Paletted DeinterlacingFilter used with non-paletted pipe?");
      return NS_ERROR_INVALID_ARG;
    }
    if (sizeof(PixelType) == 4 && mNext.IsValidPalettedPipe()) {
      NS_WARNING("Non-paletted DeinterlacingFilter used with paletted pipe?");
      return NS_ERROR_INVALID_ARG;
    }

    gfx::IntSize outputSize = mNext.InputSize();
    mProgressiveDisplay = aConfig.mProgressiveDisplay;

    const uint32_t bufferSize = outputSize.width *
                                outputSize.height *
                                sizeof(PixelType);

    // Allocate the buffer, which contains deinterlaced scanlines of the image.
    // The buffer is necessary so that we can output rows which have already
    // been deinterlaced again on subsequent passes. Since a later stage in the
    // pipeline may be transforming the rows it receives (for example, by
    // downscaling them), the rows may no longer exist in their original form on
    // the surface itself.
    mBuffer.reset(new (fallible) uint8_t[bufferSize]);
    if (MOZ_UNLIKELY(!mBuffer)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Clear the buffer to avoid writing uninitialized memory to the output.
    memset(mBuffer.get(), 0, bufferSize);

    ConfigureFilter(outputSize, sizeof(PixelType));
    return NS_OK;
  }

  bool IsValidPalettedPipe() const override
  {
    return sizeof(PixelType) == 1 && mNext.IsValidPalettedPipe();
  }

  Maybe<SurfaceInvalidRect> TakeInvalidRect() override
  {
    return mNext.TakeInvalidRect();
  }

protected:
  uint8_t* DoResetToFirstRow() override
  {
    mNext.ResetToFirstRow();
    mPass = 0;
    mInputRow = 0;
    mOutputRow = InterlaceOffset(mPass);
    return GetRowPointer(mOutputRow);
  }

  uint8_t* DoAdvanceRow() override
  {
    if (mPass >= 4) {
      return nullptr;  // We already finished all passes.
    }
    if (mInputRow >= InputSize().height) {
      return nullptr;  // We already got all the input rows we expect.
    }

    // Duplicate from the first Haeberli row to the remaining Haeberli rows
    // within the buffer.
    DuplicateRows(HaeberliOutputStartRow(mPass, mProgressiveDisplay, mOutputRow),
                  HaeberliOutputUntilRow(mPass, mProgressiveDisplay,
                                         InputSize(), mOutputRow));

    // Write the current set of Haeberli rows (which contains the current row)
    // to the next stage in the pipeline.
    OutputRows(HaeberliOutputStartRow(mPass, mProgressiveDisplay, mOutputRow),
               HaeberliOutputUntilRow(mPass, mProgressiveDisplay,
                                      InputSize(), mOutputRow));

    // Determine which output row the next input row corresponds to.
    bool advancedPass = false;
    uint32_t stride = InterlaceStride(mPass);
    int32_t nextOutputRow = mOutputRow + stride;
    while (nextOutputRow >= InputSize().height) {
      // Copy any remaining rows from the buffer.
      if (!advancedPass) {
        OutputRows(HaeberliOutputUntilRow(mPass, mProgressiveDisplay,
                                          InputSize(), mOutputRow),
                   InputSize().height);
      }

      // We finished the current pass; advance to the next one.
      mPass++;
      if (mPass >= 4) {
        return nullptr;  // Finished all passes.
      }

      // Tell the next pipeline stage that we're starting the next pass.
      mNext.ResetToFirstRow();

      // Update our state to reflect the pass change.
      advancedPass = true;
      stride = InterlaceStride(mPass);
      nextOutputRow = InterlaceOffset(mPass);
    }

    MOZ_ASSERT(nextOutputRow >= 0);
    MOZ_ASSERT(nextOutputRow < InputSize().height);

    MOZ_ASSERT(HaeberliOutputStartRow(mPass, mProgressiveDisplay,
                                      nextOutputRow) >= 0);
    MOZ_ASSERT(HaeberliOutputStartRow(mPass, mProgressiveDisplay,
                                      nextOutputRow) < InputSize().height);
    MOZ_ASSERT(HaeberliOutputStartRow(mPass, mProgressiveDisplay,
                                      nextOutputRow) <= nextOutputRow);

    MOZ_ASSERT(HaeberliOutputUntilRow(mPass, mProgressiveDisplay,
                                      InputSize(), nextOutputRow) >= 0);
    MOZ_ASSERT(HaeberliOutputUntilRow(mPass, mProgressiveDisplay,
                                      InputSize(), nextOutputRow)
                 <= InputSize().height);
    MOZ_ASSERT(HaeberliOutputUntilRow(mPass, mProgressiveDisplay,
                                      InputSize(), nextOutputRow)
                 > nextOutputRow);

    int32_t nextHaeberliOutputRow =
      HaeberliOutputStartRow(mPass, mProgressiveDisplay, nextOutputRow);

    // Copy rows from the buffer until we reach the desired output row.
    if (advancedPass) {
      OutputRows(0, nextHaeberliOutputRow);
    } else {
      OutputRows(HaeberliOutputUntilRow(mPass, mProgressiveDisplay,
                                        InputSize(), mOutputRow),
                 nextHaeberliOutputRow);
    }

    // Update our position within the buffer.
    mInputRow++;
    mOutputRow = nextOutputRow;

    // We'll actually write to the first Haeberli output row, then copy it until
    // we reach the last Haeberli output row. The assertions above make sure
    // this always includes mOutputRow.
    return GetRowPointer(nextHaeberliOutputRow);
  }

private:
  static uint32_t InterlaceOffset(uint32_t aPass)
  {
    MOZ_ASSERT(aPass < 4, "Invalid pass");
    static const uint8_t offset[] = { 0, 4, 2, 1 };
    return offset[aPass];
  }

  static uint32_t InterlaceStride(uint32_t aPass)
  {
    MOZ_ASSERT(aPass < 4, "Invalid pass");
    static const uint8_t stride[] = { 8, 8, 4, 2 };
    return stride[aPass];
  }

  static int32_t HaeberliOutputStartRow(uint32_t aPass,
                                        bool aProgressiveDisplay,
                                        int32_t aOutputRow)
  {
    MOZ_ASSERT(aPass < 4, "Invalid pass");
    static const uint8_t firstRowOffset[] = { 3, 1, 0, 0 };

    if (aProgressiveDisplay) {
      return std::max(aOutputRow - firstRowOffset[aPass], 0);
    } else {
      return aOutputRow;
    }
  }

  static int32_t HaeberliOutputUntilRow(uint32_t aPass,
                                        bool aProgressiveDisplay,
                                        const gfx::IntSize& aInputSize,
                                        int32_t aOutputRow)
  {
    MOZ_ASSERT(aPass < 4, "Invalid pass");
    static const uint8_t lastRowOffset[] = { 4, 2, 1, 0 };

    if (aProgressiveDisplay) {
      return std::min(aOutputRow + lastRowOffset[aPass],
                      aInputSize.height - 1)
             + 1;  // Add one because this is an open interval on the right.
    } else {
      return aOutputRow + 1;
    }
  }

  void DuplicateRows(int32_t aStart, int32_t aUntil)
  {
    MOZ_ASSERT(aStart >= 0);
    MOZ_ASSERT(aUntil >= 0);

    if (aUntil <= aStart || aStart >= InputSize().height) {
      return;
    }

    // The source row is the first row in the range.
    const uint8_t* sourceRowPointer = GetRowPointer(aStart);

    // We duplicate the source row into each subsequent row in the range.
    for (int32_t destRow = aStart + 1 ; destRow < aUntil ; ++destRow) {
      uint8_t* destRowPointer = GetRowPointer(destRow);
      memcpy(destRowPointer, sourceRowPointer, InputSize().width * sizeof(PixelType));
    }
  }

  void OutputRows(int32_t aStart, int32_t aUntil)
  {
    MOZ_ASSERT(aStart >= 0);
    MOZ_ASSERT(aUntil >= 0);

    if (aUntil <= aStart || aStart >= InputSize().height) {
      return;
    }

    int32_t rowToOutput = aStart;
    mNext.template WriteRows<PixelType>([&](PixelType* aRow, uint32_t aLength) {
      const uint8_t* rowToOutputPointer = GetRowPointer(rowToOutput);
      memcpy(aRow, rowToOutputPointer, aLength * sizeof(PixelType));

      rowToOutput++;
      return rowToOutput >= aUntil ? Some(WriteState::NEED_MORE_DATA)
                                   : Nothing();
    });
  }

  uint8_t* GetRowPointer(uint32_t aRow) const
  {
    uint32_t offset = aRow * InputSize().width * sizeof(PixelType);
    MOZ_ASSERT(offset < InputSize().width * InputSize().height * sizeof(PixelType),
               "Start of row is outside of image");
    MOZ_ASSERT(offset + InputSize().width * sizeof(PixelType)
                 <= InputSize().width * InputSize().height * sizeof(PixelType),
               "End of row is outside of image");
    return mBuffer.get() + offset;
  }

  Next mNext;                    /// The next SurfaceFilter in the chain.

  UniquePtr<uint8_t[]> mBuffer;  /// The buffer used to store reordered rows.
  int32_t mInputRow;             /// The current row we're reading. (0-indexed)
  int32_t mOutputRow;            /// The current row we're writing. (0-indexed)
  uint8_t mPass;                 /// Which pass we're on. (0-indexed)
  bool mProgressiveDisplay;      /// If true, duplicate rows to optimize for
                                 /// progressive display.
};


//////////////////////////////////////////////////////////////////////////////
// RemoveFrameRectFilter
//////////////////////////////////////////////////////////////////////////////

template <typename Next> class RemoveFrameRectFilter;

/**
 * A configuration struct for RemoveFrameRectFilter.
 */
struct RemoveFrameRectConfig
{
  template <typename Next> using Filter = RemoveFrameRectFilter<Next>;
  gfx::IntRect mFrameRect;  /// The surface subrect which contains data.
};

/**
 * RemoveFrameRectFilter turns an image with a frame rect that does not match
 * its logical size into an image with no frame rect. It does this by writing
 * transparent pixels into any padding regions and throwing away excess data.
 *
 * The 'Next' template parameter specifies the next filter in the chain.
 */
template <typename Next>
class RemoveFrameRectFilter final : public SurfaceFilter
{
public:
  RemoveFrameRectFilter()
    : mRow(0)
  { }

  template <typename... Rest>
  nsresult Configure(const RemoveFrameRectConfig& aConfig, Rest... aRest)
  {
    nsresult rv = mNext.Configure(aRest...);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (mNext.IsValidPalettedPipe()) {
      NS_WARNING("RemoveFrameRectFilter used with paletted pipe?");
      return NS_ERROR_INVALID_ARG;
    }

    mFrameRect = mUnclampedFrameRect = aConfig.mFrameRect;
    gfx::IntSize outputSize = mNext.InputSize();

    // Forbid frame rects with negative size.
    if (aConfig.mFrameRect.width < 0 || aConfig.mFrameRect.height < 0) {
      return NS_ERROR_INVALID_ARG;
    }

    // Clamp mFrameRect to the output size.
    gfx::IntRect outputRect(0, 0, outputSize.width, outputSize.height);
    mFrameRect = mFrameRect.Intersect(outputRect);

    // If there's no intersection, |mFrameRect| will be an empty rect positioned
    // at the maximum of |inputRect|'s and |aFrameRect|'s coordinates, which is
    // not what we want. Force it to (0, 0) in that case.
    if (mFrameRect.IsEmpty()) {
      mFrameRect.MoveTo(0, 0);
    }

    // We don't need an intermediate buffer unless the unclamped frame rect
    // width is larger than the clamped frame rect width. In that case, the
    // caller will end up writing data that won't end up in the final image at
    // all, and we'll need a buffer to give that data a place to go.
    if (mFrameRect.width < mUnclampedFrameRect.width) {
      mBuffer.reset(new (fallible) uint8_t[mUnclampedFrameRect.width *
                                           sizeof(uint32_t)]);
      if (MOZ_UNLIKELY(!mBuffer)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      memset(mBuffer.get(), 0, mUnclampedFrameRect.width * sizeof(uint32_t));
    }

    ConfigureFilter(mUnclampedFrameRect.Size(), sizeof(uint32_t));
    return NS_OK;
  }

  Maybe<SurfaceInvalidRect> TakeInvalidRect() override
  {
    return mNext.TakeInvalidRect();
  }

protected:
  uint8_t* DoResetToFirstRow() override
  {
    uint8_t* rowPtr = mNext.ResetToFirstRow();
    if (rowPtr == nullptr) {
      mRow = mFrameRect.YMost();
      return nullptr;
    }

    mRow = mUnclampedFrameRect.y;

    // Advance the next pipeline stage to the beginning of the frame rect,
    // outputting blank rows.
    if (mFrameRect.y > 0) {
      int32_t rowsToWrite = mFrameRect.y;
      mNext.template WriteRows<uint32_t>([&](uint32_t* aRow, uint32_t aLength)
                                           -> Maybe<WriteState> {
        memset(aRow, 0, aLength * sizeof(uint32_t));
        rowsToWrite--;
        return rowsToWrite > 0 ? Nothing()
                               : Some(WriteState::NEED_MORE_DATA);
      });
    }

    // We're at the beginning of the frame rect now, so return if we're either
    // ready for input or we're already done.
    rowPtr = mBuffer ? mBuffer.get() : mNext.CurrentRowPointer();
    if (!mFrameRect.IsEmpty() || rowPtr == nullptr) {
      // Note that the pointer we're returning is for the next row we're
      // actually going to write to, but we may discard writes before that point
      // if mRow < mFrameRect.y.
      return AdjustRowPointer(rowPtr);
    }

    // We've finished the region specified by the frame rect, but the frame rect
    // is empty, so we need to output the rest of the image immediately. Advance
    // to the end of the next pipeline stage's buffer, outputting blank rows.
    mNext.template WriteRows<uint32_t>([](uint32_t* aRow, uint32_t aLength) {
      memset(aRow, 0, aLength * sizeof(uint32_t));
      return Nothing();
    });

    mRow = mFrameRect.YMost();
    return nullptr;  // We're done.
  }

  uint8_t* DoAdvanceRow() override
  {
    uint8_t* rowPtr = nullptr;

    const int32_t currentRow = mRow;
    mRow++;

    if (currentRow < mFrameRect.y) {
      // This row is outside of the frame rect, so just drop it on the floor.
      rowPtr = mBuffer ? mBuffer.get() : mNext.CurrentRowPointer();
      return AdjustRowPointer(rowPtr);
    } else if (currentRow >= mFrameRect.YMost()) {
      NS_WARNING("RemoveFrameRectFilter: Advancing past end of frame rect");
      return nullptr;
    }

    // If we had to buffer, copy the data. Otherwise, just advance the row.
    if (mBuffer) {
      mNext.template WriteRows<uint32_t>([&](uint32_t* aRow, uint32_t aLength) {
        // Clear the part of the row before the clamped frame rect.
        MOZ_ASSERT(mFrameRect.x >= 0);
        MOZ_ASSERT(uint32_t(mFrameRect.x) < aLength);
        memset(aRow, 0, mFrameRect.x * sizeof(uint32_t));

        // Write the part of the row that's inside the clamped frame rect.
        MOZ_ASSERT(mFrameRect.width >= 0);
        aRow += mFrameRect.x;
        aLength -= std::min(aLength, uint32_t(mFrameRect.x));
        uint32_t toWrite = std::min(aLength, uint32_t(mFrameRect.width));
        uint8_t* source = mBuffer.get() -
                          std::min(mUnclampedFrameRect.x, 0) * sizeof(uint32_t);
        MOZ_ASSERT(source >= mBuffer.get());
        MOZ_ASSERT(source + toWrite * sizeof(uint32_t)
                     <= mBuffer.get() + mUnclampedFrameRect.width * sizeof(uint32_t));
        memcpy(aRow, source, toWrite * sizeof(uint32_t));

        // Clear the part of the row after the clamped frame rect.
        aRow += toWrite;
        aLength -= std::min(aLength, toWrite);
        memset(aRow, 0, aLength * sizeof(uint32_t));

        return Some(WriteState::NEED_MORE_DATA);
      });

      rowPtr = mBuffer.get();
    } else {
      rowPtr = mNext.AdvanceRow();
    }

    // If there's still more data coming, just adjust the pointer and return.
    if (mRow < mFrameRect.YMost() || rowPtr == nullptr) {
      return AdjustRowPointer(rowPtr);
    }

    // We've finished the region specified by the frame rect. Advance to the end
    // of the next pipeline stage's buffer, outputting blank rows.
    mNext.template WriteRows<uint32_t>([](uint32_t* aRow, uint32_t aLength) {
      memset(aRow, 0, aLength * sizeof(uint32_t));
      return Nothing();
    });

    mRow = mFrameRect.YMost();
    return nullptr;  // We're done.
  }

private:
  uint8_t* AdjustRowPointer(uint8_t* aNextRowPointer) const
  {
    if (mBuffer) {
      MOZ_ASSERT(aNextRowPointer == mBuffer.get());
      return aNextRowPointer;  // No adjustment needed for an intermediate buffer.
    }

    if (mFrameRect.IsEmpty() ||
        mRow >= mFrameRect.YMost() ||
        aNextRowPointer == nullptr) {
      return nullptr;  // Nothing left to write.
    }

    return aNextRowPointer + mFrameRect.x * sizeof(uint32_t);
  }

  Next mNext;                        /// The next SurfaceFilter in the chain.

  gfx::IntRect mFrameRect;           /// The surface subrect which contains data,
                                     /// clamped to the image size.
  gfx::IntRect mUnclampedFrameRect;  /// The frame rect before clamping.
  UniquePtr<uint8_t[]> mBuffer;      /// The intermediate buffer, if one is
                                     /// necessary because the frame rect width
                                     /// is larger than the image's logical width.
  int32_t  mRow;                     /// The row in unclamped frame rect space
                                     /// that we're currently writing.
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_SurfaceFilters_h
