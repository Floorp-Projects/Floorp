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

#include <algorithm>
#include <stdint.h>
#include <string.h>

#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/gfx/2D.h"
#include "skia/src/core/SkBlitRow.h"

#include "DownscalingFilter.h"
#include "SurfaceCache.h"
#include "SurfacePipe.h"

namespace mozilla {
namespace image {

//////////////////////////////////////////////////////////////////////////////
// DeinterlacingFilter
//////////////////////////////////////////////////////////////////////////////

template <typename PixelType, typename Next>
class DeinterlacingFilter;

/**
 * A configuration struct for DeinterlacingFilter.
 *
 * The 'PixelType' template parameter should be either uint32_t (for output to a
 * SurfaceSink) or uint8_t (for output to a PalettedSurfaceSink).
 */
template <typename PixelType>
struct DeinterlacingConfig {
  template <typename Next>
  using Filter = DeinterlacingFilter<PixelType, Next>;
  bool mProgressiveDisplay;  /// If true, duplicate rows during deinterlacing
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
class DeinterlacingFilter final : public SurfaceFilter {
 public:
  DeinterlacingFilter()
      : mInputRow(0), mOutputRow(0), mPass(0), mProgressiveDisplay(true) {}

  template <typename... Rest>
  nsresult Configure(const DeinterlacingConfig<PixelType>& aConfig,
                     const Rest&... aRest) {
    nsresult rv = mNext.Configure(aRest...);
    if (NS_FAILED(rv)) {
      return rv;
    }

    gfx::IntSize outputSize = mNext.InputSize();
    mProgressiveDisplay = aConfig.mProgressiveDisplay;

    const uint32_t bufferSize =
        outputSize.width * outputSize.height * sizeof(PixelType);

    // Use the size of the SurfaceCache as a heuristic to avoid gigantic
    // allocations. Even if DownscalingFilter allowed us to allocate space for
    // the output image, the deinterlacing buffer may still be too big, and
    // fallible allocation won't always save us in the presence of overcommit.
    if (!SurfaceCache::CanHold(bufferSize)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

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

  Maybe<SurfaceInvalidRect> TakeInvalidRect() override {
    return mNext.TakeInvalidRect();
  }

 protected:
  uint8_t* DoResetToFirstRow() override {
    mNext.ResetToFirstRow();
    mPass = 0;
    mInputRow = 0;
    mOutputRow = InterlaceOffset(mPass);
    return GetRowPointer(mOutputRow);
  }

  uint8_t* DoAdvanceRow() override {
    if (mPass >= 4) {
      return nullptr;  // We already finished all passes.
    }
    if (mInputRow >= InputSize().height) {
      return nullptr;  // We already got all the input rows we expect.
    }

    // Duplicate from the first Haeberli row to the remaining Haeberli rows
    // within the buffer.
    DuplicateRows(
        HaeberliOutputStartRow(mPass, mProgressiveDisplay, mOutputRow),
        HaeberliOutputUntilRow(mPass, mProgressiveDisplay, InputSize(),
                               mOutputRow));

    // Write the current set of Haeberli rows (which contains the current row)
    // to the next stage in the pipeline.
    OutputRows(HaeberliOutputStartRow(mPass, mProgressiveDisplay, mOutputRow),
               HaeberliOutputUntilRow(mPass, mProgressiveDisplay, InputSize(),
                                      mOutputRow));

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

    MOZ_ASSERT(
        HaeberliOutputStartRow(mPass, mProgressiveDisplay, nextOutputRow) >= 0);
    MOZ_ASSERT(HaeberliOutputStartRow(mPass, mProgressiveDisplay,
                                      nextOutputRow) < InputSize().height);
    MOZ_ASSERT(HaeberliOutputStartRow(mPass, mProgressiveDisplay,
                                      nextOutputRow) <= nextOutputRow);

    MOZ_ASSERT(HaeberliOutputUntilRow(mPass, mProgressiveDisplay, InputSize(),
                                      nextOutputRow) >= 0);
    MOZ_ASSERT(HaeberliOutputUntilRow(mPass, mProgressiveDisplay, InputSize(),
                                      nextOutputRow) <= InputSize().height);
    MOZ_ASSERT(HaeberliOutputUntilRow(mPass, mProgressiveDisplay, InputSize(),
                                      nextOutputRow) > nextOutputRow);

    int32_t nextHaeberliOutputRow =
        HaeberliOutputStartRow(mPass, mProgressiveDisplay, nextOutputRow);

    // Copy rows from the buffer until we reach the desired output row.
    if (advancedPass) {
      OutputRows(0, nextHaeberliOutputRow);
    } else {
      OutputRows(HaeberliOutputUntilRow(mPass, mProgressiveDisplay, InputSize(),
                                        mOutputRow),
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
  static uint32_t InterlaceOffset(uint32_t aPass) {
    MOZ_ASSERT(aPass < 4, "Invalid pass");
    static const uint8_t offset[] = {0, 4, 2, 1};
    return offset[aPass];
  }

  static uint32_t InterlaceStride(uint32_t aPass) {
    MOZ_ASSERT(aPass < 4, "Invalid pass");
    static const uint8_t stride[] = {8, 8, 4, 2};
    return stride[aPass];
  }

  static int32_t HaeberliOutputStartRow(uint32_t aPass,
                                        bool aProgressiveDisplay,
                                        int32_t aOutputRow) {
    MOZ_ASSERT(aPass < 4, "Invalid pass");
    static const uint8_t firstRowOffset[] = {3, 1, 0, 0};

    if (aProgressiveDisplay) {
      return std::max(aOutputRow - firstRowOffset[aPass], 0);
    } else {
      return aOutputRow;
    }
  }

  static int32_t HaeberliOutputUntilRow(uint32_t aPass,
                                        bool aProgressiveDisplay,
                                        const gfx::IntSize& aInputSize,
                                        int32_t aOutputRow) {
    MOZ_ASSERT(aPass < 4, "Invalid pass");
    static const uint8_t lastRowOffset[] = {4, 2, 1, 0};

    if (aProgressiveDisplay) {
      return std::min(aOutputRow + lastRowOffset[aPass],
                      aInputSize.height - 1) +
             1;  // Add one because this is an open interval on the right.
    } else {
      return aOutputRow + 1;
    }
  }

  void DuplicateRows(int32_t aStart, int32_t aUntil) {
    MOZ_ASSERT(aStart >= 0);
    MOZ_ASSERT(aUntil >= 0);

    if (aUntil <= aStart || aStart >= InputSize().height) {
      return;
    }

    // The source row is the first row in the range.
    const uint8_t* sourceRowPointer = GetRowPointer(aStart);

    // We duplicate the source row into each subsequent row in the range.
    for (int32_t destRow = aStart + 1; destRow < aUntil; ++destRow) {
      uint8_t* destRowPointer = GetRowPointer(destRow);
      memcpy(destRowPointer, sourceRowPointer,
             InputSize().width * sizeof(PixelType));
    }
  }

  void OutputRows(int32_t aStart, int32_t aUntil) {
    MOZ_ASSERT(aStart >= 0);
    MOZ_ASSERT(aUntil >= 0);

    if (aUntil <= aStart || aStart >= InputSize().height) {
      return;
    }

    for (int32_t rowToOutput = aStart; rowToOutput < aUntil; ++rowToOutput) {
      mNext.WriteBuffer(
          reinterpret_cast<PixelType*>(GetRowPointer(rowToOutput)));
    }
  }

  uint8_t* GetRowPointer(uint32_t aRow) const {
    uint32_t offset = aRow * InputSize().width * sizeof(PixelType);
    MOZ_ASSERT(
        offset < InputSize().width * InputSize().height * sizeof(PixelType),
        "Start of row is outside of image");
    MOZ_ASSERT(offset + InputSize().width * sizeof(PixelType) <=
                   InputSize().width * InputSize().height * sizeof(PixelType),
               "End of row is outside of image");
    return mBuffer.get() + offset;
  }

  Next mNext;  /// The next SurfaceFilter in the chain.

  UniquePtr<uint8_t[]> mBuffer;  /// The buffer used to store reordered rows.
  int32_t mInputRow;             /// The current row we're reading. (0-indexed)
  int32_t mOutputRow;            /// The current row we're writing. (0-indexed)
  uint8_t mPass;                 /// Which pass we're on. (0-indexed)
  bool mProgressiveDisplay;      /// If true, duplicate rows to optimize for
                                 /// progressive display.
};

//////////////////////////////////////////////////////////////////////////////
// BlendAnimationFilter
//////////////////////////////////////////////////////////////////////////////

template <typename Next>
class BlendAnimationFilter;

/**
 * A configuration struct for BlendAnimationFilter.
 */
struct BlendAnimationConfig {
  template <typename Next>
  using Filter = BlendAnimationFilter<Next>;
  Decoder* mDecoder;  /// The decoder producing the animation.
};

/**
 * BlendAnimationFilter turns a partial image as part of an animation into a
 * complete frame given its frame rect, blend method, and the base frame's
 * data buffer, frame rect and disposal method. Any excess data caused by a
 * frame rect not being contained by the output size will be discarded.
 *
 * The base frame is an already produced complete frame from the animation.
 * It may be any previous frame depending on the disposal method, although
 * most often it will be the immediate previous frame to the current we are
 * generating.
 *
 * The 'Next' template parameter specifies the next filter in the chain.
 */
template <typename Next>
class BlendAnimationFilter final : public SurfaceFilter {
 public:
  BlendAnimationFilter()
      : mRow(0),
        mRowLength(0),
        mRecycleRow(0),
        mRecycleRowMost(0),
        mRecycleRowOffset(0),
        mRecycleRowLength(0),
        mClearRow(0),
        mClearRowMost(0),
        mClearPrefixLength(0),
        mClearInfixOffset(0),
        mClearInfixLength(0),
        mClearPostfixOffset(0),
        mClearPostfixLength(0),
        mOverProc(nullptr),
        mBaseFrameStartPtr(nullptr),
        mBaseFrameRowPtr(nullptr) {}

  template <typename... Rest>
  nsresult Configure(const BlendAnimationConfig& aConfig,
                     const Rest&... aRest) {
    nsresult rv = mNext.Configure(aRest...);
    if (NS_FAILED(rv)) {
      return rv;
    }

    imgFrame* currentFrame = aConfig.mDecoder->GetCurrentFrame();
    if (!currentFrame) {
      MOZ_ASSERT_UNREACHABLE("Decoder must have current frame!");
      return NS_ERROR_FAILURE;
    }

    mFrameRect = mUnclampedFrameRect = currentFrame->GetBlendRect();
    gfx::IntSize outputSize = mNext.InputSize();
    mRowLength = outputSize.width * sizeof(uint32_t);

    // Forbid frame rects with negative size.
    if (mUnclampedFrameRect.width < 0 || mUnclampedFrameRect.height < 0) {
      return NS_ERROR_FAILURE;
    }

    // Clamp mFrameRect to the output size.
    gfx::IntRect outputRect(0, 0, outputSize.width, outputSize.height);
    mFrameRect = mFrameRect.Intersect(outputRect);
    bool fullFrame = outputRect.IsEqualEdges(mFrameRect);

    // If there's no intersection, |mFrameRect| will be an empty rect positioned
    // at the maximum of |inputRect|'s and |aFrameRect|'s coordinates, which is
    // not what we want. Force it to (0, 0) sized 0 x 0 in that case.
    if (mFrameRect.IsEmpty()) {
      mFrameRect.SetRect(0, 0, 0, 0);
    }

    BlendMethod blendMethod = currentFrame->GetBlendMethod();
    switch (blendMethod) {
      default:
        blendMethod = BlendMethod::SOURCE;
        MOZ_FALLTHROUGH_ASSERT("Unexpected blend method!");
      case BlendMethod::SOURCE:
        // Default, overwrites base frame data (if any) with new.
        break;
      case BlendMethod::OVER:
        // OVER only has an impact on the output if we have new data to blend
        // with.
        if (mFrameRect.IsEmpty()) {
          blendMethod = BlendMethod::SOURCE;
        }
        break;
    }

    // Determine what we need to clear and what we need to copy. If this frame
    // is a full frame and uses source blending, there is no need to consider
    // the disposal method of the previous frame.
    gfx::IntRect dirtyRect(outputRect);
    gfx::IntRect clearRect;
    if (!fullFrame || blendMethod != BlendMethod::SOURCE) {
      const RawAccessFrameRef& restoreFrame =
          aConfig.mDecoder->GetRestoreFrameRef();
      if (restoreFrame) {
        MOZ_ASSERT(restoreFrame->GetSize() == outputSize);
        MOZ_ASSERT(restoreFrame->IsFinished());

        // We can safely use this pointer without holding a RawAccessFrameRef
        // because the decoder will keep it alive for us.
        mBaseFrameStartPtr = restoreFrame.Data();
        MOZ_ASSERT(mBaseFrameStartPtr);

        gfx::IntRect restoreBlendRect = restoreFrame->GetBoundedBlendRect();
        gfx::IntRect restoreDirtyRect = aConfig.mDecoder->GetRestoreDirtyRect();
        switch (restoreFrame->GetDisposalMethod()) {
          default:
          case DisposalMethod::RESTORE_PREVIOUS:
            MOZ_FALLTHROUGH_ASSERT("Unexpected DisposalMethod");
          case DisposalMethod::NOT_SPECIFIED:
          case DisposalMethod::KEEP:
            dirtyRect = mFrameRect.Union(restoreDirtyRect);
            break;
          case DisposalMethod::CLEAR:
            // We only need to clear if the rect is outside the frame rect (i.e.
            // overwrites a non-overlapping area) or the blend method may cause
            // us to combine old data and new.
            if (!mFrameRect.Contains(restoreBlendRect) ||
                blendMethod == BlendMethod::OVER) {
              clearRect = restoreBlendRect;
            }

            // If we are clearing the whole frame, we do not need to retain a
            // reference to the base frame buffer.
            if (outputRect.IsEqualEdges(clearRect)) {
              mBaseFrameStartPtr = nullptr;
            } else {
              dirtyRect = mFrameRect.Union(restoreDirtyRect).Union(clearRect);
            }
            break;
        }
      } else if (!fullFrame) {
        // This must be the first frame, clear everything.
        clearRect = outputRect;
      }
    }

    // We may be able to reuse parts of our underlying buffer that we are
    // writing the new frame to. The recycle rect gives us the invalidation
    // region which needs to be copied from the restore frame.
    const gfx::IntRect& recycleRect = aConfig.mDecoder->GetRecycleRect();
    mRecycleRow = recycleRect.y;
    mRecycleRowMost = recycleRect.YMost();
    mRecycleRowOffset = recycleRect.x * sizeof(uint32_t);
    mRecycleRowLength = recycleRect.width * sizeof(uint32_t);

    if (!clearRect.IsEmpty()) {
      // The clear rect interacts with the recycle rect because we need to copy
      // the prefix and postfix data from the base frame. The one thing we do
      // know is that the infix area is always cleared explicitly.
      mClearRow = clearRect.y;
      mClearRowMost = clearRect.YMost();
      mClearInfixOffset = clearRect.x * sizeof(uint32_t);
      mClearInfixLength = clearRect.width * sizeof(uint32_t);

      // The recycle row offset is where we need to begin copying base frame
      // data for a row. If this offset begins after or at the clear infix
      // offset, then there is no prefix data at all.
      if (mClearInfixOffset > mRecycleRowOffset) {
        mClearPrefixLength = mClearInfixOffset - mRecycleRowOffset;
      }

      // Similar to the prefix, if the postfix offset begins outside the recycle
      // rect, then we know we already have all the data we need.
      mClearPostfixOffset = mClearInfixOffset + mClearInfixLength;
      size_t recycleRowEndOffset = mRecycleRowOffset + mRecycleRowLength;
      if (mClearPostfixOffset < recycleRowEndOffset) {
        mClearPostfixLength = recycleRowEndOffset - mClearPostfixOffset;
      }
    }

    // The dirty rect, or delta between the current frame and the previous frame
    // (chronologically, not necessarily the restore frame) is the last
    // animation parameter we need to initialize the new frame with.
    currentFrame->SetDirtyRect(dirtyRect);

    if (!mBaseFrameStartPtr) {
      // Switch to SOURCE if no base frame to ensure we don't allocate an
      // intermediate buffer below. OVER does nothing without the base frame
      // data.
      blendMethod = BlendMethod::SOURCE;
    }

    // Skia provides arch-specific accelerated methods to perform blending.
    // Note that this is an internal Skia API and may be prone to change,
    // but we avoid the overhead of setting up Skia objects.
    if (blendMethod == BlendMethod::OVER) {
      mOverProc = SkBlitRow::Factory32(SkBlitRow::kSrcPixelAlpha_Flag32);
      MOZ_ASSERT(mOverProc);
    }

    // We don't need an intermediate buffer unless the unclamped frame rect
    // width is larger than the clamped frame rect width. In that case, the
    // caller will end up writing data that won't end up in the final image at
    // all, and we'll need a buffer to give that data a place to go.
    if (mFrameRect.width < mUnclampedFrameRect.width || mOverProc) {
      mBuffer.reset(new (fallible)
                        uint8_t[mUnclampedFrameRect.width * sizeof(uint32_t)]);
      if (MOZ_UNLIKELY(!mBuffer)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      memset(mBuffer.get(), 0, mUnclampedFrameRect.width * sizeof(uint32_t));
    }

    ConfigureFilter(mUnclampedFrameRect.Size(), sizeof(uint32_t));
    return NS_OK;
  }

  Maybe<SurfaceInvalidRect> TakeInvalidRect() override {
    return mNext.TakeInvalidRect();
  }

 protected:
  uint8_t* DoResetToFirstRow() override {
    uint8_t* rowPtr = mNext.ResetToFirstRow();
    if (rowPtr == nullptr) {
      mRow = mFrameRect.YMost();
      return nullptr;
    }

    mRow = 0;
    mBaseFrameRowPtr = mBaseFrameStartPtr;

    while (mRow < mFrameRect.y) {
      WriteBaseFrameRow();
      AdvanceRowOutsideFrameRect();
    }

    // We're at the beginning of the frame rect now, so return if we're either
    // ready for input or we're already done.
    rowPtr = mBuffer ? mBuffer.get() : mNext.CurrentRowPointer();
    if (!mFrameRect.IsEmpty() || rowPtr == nullptr) {
      // Note that the pointer we're returning is for the next row we're
      // actually going to write to, but we may discard writes before that point
      // if mRow < mFrameRect.y.
      mRow = mUnclampedFrameRect.y;
      WriteBaseFrameRow();
      return AdjustRowPointer(rowPtr);
    }

    // We've finished the region specified by the frame rect, but the frame rect
    // is empty, so we need to output the rest of the image immediately. Advance
    // to the end of the next pipeline stage's buffer, outputting rows that are
    // copied from the base frame and/or cleared.
    WriteBaseFrameRowsUntilComplete();

    mRow = mFrameRect.YMost();
    return nullptr;  // We're done.
  }

  uint8_t* DoAdvanceRow() override {
    uint8_t* rowPtr = nullptr;

    const int32_t currentRow = mRow;
    mRow++;

    // The unclamped frame rect has a negative offset which means -y rows from
    // the decoder need to be discarded before we advance properly.
    if (currentRow >= 0 && mBaseFrameRowPtr) {
      mBaseFrameRowPtr += mRowLength;
    }

    if (currentRow < mFrameRect.y) {
      // This row is outside of the frame rect, so just drop it on the floor.
      rowPtr = mBuffer ? mBuffer.get() : mNext.CurrentRowPointer();
      return AdjustRowPointer(rowPtr);
    } else if (NS_WARN_IF(currentRow >= mFrameRect.YMost())) {
      return nullptr;
    }

    // If we had to buffer, merge the data into the row. Otherwise we had the
    // decoder write directly to the next stage's buffer.
    if (mBuffer) {
      int32_t width = mFrameRect.width;
      uint32_t* dst = reinterpret_cast<uint32_t*>(mNext.CurrentRowPointer());
      uint32_t* src = reinterpret_cast<uint32_t*>(mBuffer.get()) -
                      std::min(mUnclampedFrameRect.x, 0);
      dst += mFrameRect.x;
      if (mOverProc) {
        mOverProc(dst, src, width, 0xFF);
      } else {
        memcpy(dst, src, width * sizeof(uint32_t));
      }
      rowPtr = mNext.AdvanceRow() ? mBuffer.get() : nullptr;
    } else {
      MOZ_ASSERT(!mOverProc);
      rowPtr = mNext.AdvanceRow();
    }

    // If there's still more data coming or we're already done, just adjust the
    // pointer and return.
    if (mRow < mFrameRect.YMost() || rowPtr == nullptr) {
      WriteBaseFrameRow();
      return AdjustRowPointer(rowPtr);
    }

    // We've finished the region specified by the frame rect. Advance to the end
    // of the next pipeline stage's buffer, outputting rows that are copied from
    // the base frame and/or cleared.
    WriteBaseFrameRowsUntilComplete();

    return nullptr;  // We're done.
  }

 private:
  void WriteBaseFrameRowsUntilComplete() {
    do {
      WriteBaseFrameRow();
    } while (AdvanceRowOutsideFrameRect());
  }

  void WriteBaseFrameRow() {
    uint8_t* dest = mNext.CurrentRowPointer();
    if (!dest) {
      return;
    }

    // No need to copy pixels from the base frame for rows that will not change
    // between the recycled frame and the new frame.
    bool needBaseFrame = mRow >= mRecycleRow && mRow < mRecycleRowMost;

    if (!mBaseFrameRowPtr) {
      // No base frame, so we are clearing everything.
      if (needBaseFrame) {
        memset(dest + mRecycleRowOffset, 0, mRecycleRowLength);
      }
    } else if (mClearRow <= mRow && mClearRowMost > mRow) {
      // We have a base frame, but we are inside the area to be cleared.
      // Only copy the data we need from the source.
      if (needBaseFrame) {
        memcpy(dest + mRecycleRowOffset, mBaseFrameRowPtr + mRecycleRowOffset,
               mClearPrefixLength);
        memcpy(dest + mClearPostfixOffset,
               mBaseFrameRowPtr + mClearPostfixOffset, mClearPostfixLength);
      }
      memset(dest + mClearInfixOffset, 0, mClearInfixLength);
    } else if (needBaseFrame) {
      memcpy(dest + mRecycleRowOffset, mBaseFrameRowPtr + mRecycleRowOffset,
             mRecycleRowLength);
    }
  }

  bool AdvanceRowOutsideFrameRect() {
    // The unclamped frame rect may have a negative offset however we should
    // never be advancing the row via this path (otherwise mBaseFrameRowPtr
    // will be wrong.
    MOZ_ASSERT(mRow >= 0);
    MOZ_ASSERT(mRow < mFrameRect.y || mRow >= mFrameRect.YMost());

    mRow++;
    if (mBaseFrameRowPtr) {
      mBaseFrameRowPtr += mRowLength;
    }

    return mNext.AdvanceRow() != nullptr;
  }

  uint8_t* AdjustRowPointer(uint8_t* aNextRowPointer) const {
    if (mBuffer) {
      MOZ_ASSERT(aNextRowPointer == mBuffer.get() ||
                 aNextRowPointer == nullptr);
      return aNextRowPointer;  // No adjustment needed for an intermediate
                               // buffer.
    }

    if (mFrameRect.IsEmpty() || mRow >= mFrameRect.YMost() ||
        aNextRowPointer == nullptr) {
      return nullptr;  // Nothing left to write.
    }

    MOZ_ASSERT(!mOverProc);
    return aNextRowPointer + mFrameRect.x * sizeof(uint32_t);
  }

  Next mNext;  /// The next SurfaceFilter in the chain.

  gfx::IntRect mFrameRect;  /// The surface subrect which contains data,
                            /// clamped to the image size.
  gfx::IntRect mUnclampedFrameRect;  /// The frame rect before clamping.
  UniquePtr<uint8_t[]> mBuffer;      /// The intermediate buffer, if one is
                                     /// necessary because the frame rect width
  /// is larger than the image's logical width.
  int32_t mRow;              /// The row in unclamped frame rect space
                             /// that we're currently writing.
  size_t mRowLength;         /// Length in bytes of a row that is the input
                             /// for the next filter.
  int32_t mRecycleRow;       /// The starting row of the recycle rect.
  int32_t mRecycleRowMost;   /// The ending row of the recycle rect.
  size_t mRecycleRowOffset;  /// Row offset in bytes of the recycle rect.
  size_t mRecycleRowLength;  /// Row length in bytes of the recycle rect.

  /// The frame area to clear before blending the current frame.
  int32_t mClearRow;           /// The starting row of the clear rect.
  int32_t mClearRowMost;       /// The ending row of the clear rect.
  size_t mClearPrefixLength;   /// Row length in bytes of clear prefix.
  size_t mClearInfixOffset;    /// Row offset in bytes of clear area.
  size_t mClearInfixLength;    /// Row length in bytes of clear area.
  size_t mClearPostfixOffset;  /// Row offset in bytes of clear postfix.
  size_t mClearPostfixLength;  /// Row length in bytes of clear postfix.

  SkBlitRow::Proc32 mOverProc;  /// Function pointer to perform over blending.
  const uint8_t*
      mBaseFrameStartPtr;           /// Starting row pointer to the base frame
                                    /// data from which we copy pixel data from.
  const uint8_t* mBaseFrameRowPtr;  /// Current row pointer to the base frame
                                    /// data.
};

//////////////////////////////////////////////////////////////////////////////
// RemoveFrameRectFilter
//////////////////////////////////////////////////////////////////////////////

template <typename Next>
class RemoveFrameRectFilter;

/**
 * A configuration struct for RemoveFrameRectFilter.
 */
struct RemoveFrameRectConfig {
  template <typename Next>
  using Filter = RemoveFrameRectFilter<Next>;
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
class RemoveFrameRectFilter final : public SurfaceFilter {
 public:
  RemoveFrameRectFilter() : mRow(0) {}

  template <typename... Rest>
  nsresult Configure(const RemoveFrameRectConfig& aConfig,
                     const Rest&... aRest) {
    nsresult rv = mNext.Configure(aRest...);
    if (NS_FAILED(rv)) {
      return rv;
    }

    mFrameRect = mUnclampedFrameRect = aConfig.mFrameRect;
    gfx::IntSize outputSize = mNext.InputSize();

    // Forbid frame rects with negative size.
    if (aConfig.mFrameRect.Width() < 0 || aConfig.mFrameRect.Height() < 0) {
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
    if (mFrameRect.Width() < mUnclampedFrameRect.Width()) {
      mBuffer.reset(new (
          fallible) uint8_t[mUnclampedFrameRect.Width() * sizeof(uint32_t)]);
      if (MOZ_UNLIKELY(!mBuffer)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      memset(mBuffer.get(), 0, mUnclampedFrameRect.Width() * sizeof(uint32_t));
    }

    ConfigureFilter(mUnclampedFrameRect.Size(), sizeof(uint32_t));
    return NS_OK;
  }

  Maybe<SurfaceInvalidRect> TakeInvalidRect() override {
    return mNext.TakeInvalidRect();
  }

 protected:
  uint8_t* DoResetToFirstRow() override {
    uint8_t* rowPtr = mNext.ResetToFirstRow();
    if (rowPtr == nullptr) {
      mRow = mFrameRect.YMost();
      return nullptr;
    }

    mRow = mUnclampedFrameRect.Y();

    // Advance the next pipeline stage to the beginning of the frame rect,
    // outputting blank rows.
    if (mFrameRect.Y() > 0) {
      for (int32_t rowToOutput = 0; rowToOutput < mFrameRect.Y();
           ++rowToOutput) {
        mNext.WriteEmptyRow();
      }
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
    while (mNext.WriteEmptyRow() == WriteState::NEED_MORE_DATA) {
    }

    mRow = mFrameRect.YMost();
    return nullptr;  // We're done.
  }

  uint8_t* DoAdvanceRow() override {
    uint8_t* rowPtr = nullptr;

    const int32_t currentRow = mRow;
    mRow++;

    if (currentRow < mFrameRect.Y()) {
      // This row is outside of the frame rect, so just drop it on the floor.
      rowPtr = mBuffer ? mBuffer.get() : mNext.CurrentRowPointer();
      return AdjustRowPointer(rowPtr);
    } else if (currentRow >= mFrameRect.YMost()) {
      NS_WARNING("RemoveFrameRectFilter: Advancing past end of frame rect");
      return nullptr;
    }

    // If we had to buffer, copy the data. Otherwise, just advance the row.
    if (mBuffer) {
      // We write from the beginning of the buffer unless
      // |mUnclampedFrameRect.x| is negative; if that's the case, we have to
      // skip the portion of the unclamped frame rect that's outside the row.
      uint32_t* source = reinterpret_cast<uint32_t*>(mBuffer.get()) -
                         std::min(mUnclampedFrameRect.X(), 0);

      // We write |mFrameRect.width| columns starting at |mFrameRect.x|; we've
      // already clamped these values to the size of the output, so we don't
      // have to worry about bounds checking here (though WriteBuffer() will do
      // it for us in any case).
      WriteState state =
          mNext.WriteBuffer(source, mFrameRect.X(), mFrameRect.Width());

      rowPtr = state == WriteState::NEED_MORE_DATA ? mBuffer.get() : nullptr;
    } else {
      rowPtr = mNext.AdvanceRow();
    }

    // If there's still more data coming or we're already done, just adjust the
    // pointer and return.
    if (mRow < mFrameRect.YMost() || rowPtr == nullptr) {
      return AdjustRowPointer(rowPtr);
    }

    // We've finished the region specified by the frame rect. Advance to the end
    // of the next pipeline stage's buffer, outputting blank rows.
    while (mNext.WriteEmptyRow() == WriteState::NEED_MORE_DATA) {
    }

    mRow = mFrameRect.YMost();
    return nullptr;  // We're done.
  }

 private:
  uint8_t* AdjustRowPointer(uint8_t* aNextRowPointer) const {
    if (mBuffer) {
      MOZ_ASSERT(aNextRowPointer == mBuffer.get() ||
                 aNextRowPointer == nullptr);
      return aNextRowPointer;  // No adjustment needed for an intermediate
                               // buffer.
    }

    if (mFrameRect.IsEmpty() || mRow >= mFrameRect.YMost() ||
        aNextRowPointer == nullptr) {
      return nullptr;  // Nothing left to write.
    }

    return aNextRowPointer + mFrameRect.X() * sizeof(uint32_t);
  }

  Next mNext;  /// The next SurfaceFilter in the chain.

  gfx::IntRect mFrameRect;  /// The surface subrect which contains data,
                            /// clamped to the image size.
  gfx::IntRect mUnclampedFrameRect;  /// The frame rect before clamping.
  UniquePtr<uint8_t[]> mBuffer;      /// The intermediate buffer, if one is
                                     /// necessary because the frame rect width
  /// is larger than the image's logical width.
  int32_t mRow;  /// The row in unclamped frame rect space
                 /// that we're currently writing.
};

//////////////////////////////////////////////////////////////////////////////
// ADAM7InterpolatingFilter
//////////////////////////////////////////////////////////////////////////////

template <typename Next>
class ADAM7InterpolatingFilter;

/**
 * A configuration struct for ADAM7InterpolatingFilter.
 */
struct ADAM7InterpolatingConfig {
  template <typename Next>
  using Filter = ADAM7InterpolatingFilter<Next>;
};

/**
 * ADAM7InterpolatingFilter performs bilinear interpolation over an ADAM7
 * interlaced image.
 *
 * ADAM7 breaks up the image into 8x8 blocks. On each of the 7 passes, a new set
 * of pixels in each block receives their final values, according to the
 * following pattern:
 *
 *    1 6 4 6 2 6 4 6
 *    7 7 7 7 7 7 7 7
 *    5 6 5 6 5 6 5 6
 *    7 7 7 7 7 7 7 7
 *    3 6 4 6 3 6 4 6
 *    7 7 7 7 7 7 7 7
 *    5 6 5 6 5 6 5 6
 *    7 7 7 7 7 7 7 7
 *
 * When rendering the pixels that have not yet received their final values, we
 * can get much better intermediate results if we interpolate between
 * the pixels we *have* gotten so far. This filter performs bilinear
 * interpolation by first performing linear interpolation horizontally for each
 * "important" row (which we'll define as a row that has received any pixels
 * with final values at all) and then performing linear interpolation vertically
 * to produce pixel values for rows which aren't important on the current pass.
 *
 * Note that this filter totally ignores the data which is written to rows which
 * aren't important on the current pass! It's fine to write nothing at all for
 * these rows, although doing so won't cause any harm.
 *
 * XXX(seth): In bug 1280552 we'll add a SIMD implementation for this filter.
 *
 * The 'Next' template parameter specifies the next filter in the chain.
 */
template <typename Next>
class ADAM7InterpolatingFilter final : public SurfaceFilter {
 public:
  ADAM7InterpolatingFilter()
      : mPass(0)  // The current pass, in the range 1..7. Starts at 0 so that
                  // DoResetToFirstRow() doesn't have to special case the first
                  // pass.
        ,
        mRow(0) {}

  template <typename... Rest>
  nsresult Configure(const ADAM7InterpolatingConfig& aConfig,
                     const Rest&... aRest) {
    nsresult rv = mNext.Configure(aRest...);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // We have two intermediate buffers, one for the previous row with final
    // pixel values and one for the row that the previous filter in the chain is
    // currently writing to.
    size_t inputWidthInBytes = mNext.InputSize().width * sizeof(uint32_t);
    mPreviousRow.reset(new (fallible) uint8_t[inputWidthInBytes]);
    if (MOZ_UNLIKELY(!mPreviousRow)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    mCurrentRow.reset(new (fallible) uint8_t[inputWidthInBytes]);
    if (MOZ_UNLIKELY(!mCurrentRow)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    memset(mPreviousRow.get(), 0, inputWidthInBytes);
    memset(mCurrentRow.get(), 0, inputWidthInBytes);

    ConfigureFilter(mNext.InputSize(), sizeof(uint32_t));
    return NS_OK;
  }

  Maybe<SurfaceInvalidRect> TakeInvalidRect() override {
    return mNext.TakeInvalidRect();
  }

 protected:
  uint8_t* DoResetToFirstRow() override {
    mRow = 0;
    mPass = std::min(mPass + 1, 7);

    uint8_t* rowPtr = mNext.ResetToFirstRow();
    if (mPass == 7) {
      // Short circuit this filter on the final pass, since all pixels have
      // their final values at that point.
      return rowPtr;
    }

    return mCurrentRow.get();
  }

  uint8_t* DoAdvanceRow() override {
    MOZ_ASSERT(0 < mPass && mPass <= 7, "Invalid pass");

    int32_t currentRow = mRow;
    ++mRow;

    if (mPass == 7) {
      // On the final pass we short circuit this filter totally.
      return mNext.AdvanceRow();
    }

    const int32_t lastImportantRow =
        LastImportantRow(InputSize().height, mPass);
    if (currentRow > lastImportantRow) {
      return nullptr;  // This pass is already complete.
    }

    if (!IsImportantRow(currentRow, mPass)) {
      // We just ignore whatever the caller gives us for these rows. We'll
      // interpolate them in later.
      return mCurrentRow.get();
    }

    // This is an important row. We need to perform horizontal interpolation for
    // these rows.
    InterpolateHorizontally(mCurrentRow.get(), InputSize().width, mPass);

    // Interpolate vertically between the previous important row and the current
    // important row. We skip this if the current row is 0 (which is always an
    // important row), because in that case there is no previous important row
    // to interpolate with.
    if (currentRow != 0) {
      InterpolateVertically(mPreviousRow.get(), mCurrentRow.get(), mPass,
                            mNext);
    }

    // Write out the current row itself, which, being an important row, does not
    // need vertical interpolation.
    uint32_t* currentRowAsPixels =
        reinterpret_cast<uint32_t*>(mCurrentRow.get());
    mNext.WriteBuffer(currentRowAsPixels);

    if (currentRow == lastImportantRow) {
      // This is the last important row, which completes this pass. Note that
      // for very small images, this may be the first row! Since there won't be
      // another important row, there's nothing to interpolate with vertically,
      // so we just duplicate this row until the end of the image.
      while (mNext.WriteBuffer(currentRowAsPixels) ==
             WriteState::NEED_MORE_DATA) {
      }

      // All of the remaining rows in the image were determined above, so we're
      // done.
      return nullptr;
    }

    // The current row is now the previous important row; save it.
    Swap(mPreviousRow, mCurrentRow);

    MOZ_ASSERT(mRow < InputSize().height,
               "Reached the end of the surface without "
               "hitting the last important row?");

    return mCurrentRow.get();
  }

 private:
  static void InterpolateVertically(uint8_t* aPreviousRow, uint8_t* aCurrentRow,
                                    uint8_t aPass, SurfaceFilter& aNext) {
    const float* weights = InterpolationWeights(ImportantRowStride(aPass));

    // We need to interpolate vertically to generate the rows between the
    // previous important row and the next one. Recall that important rows are
    // rows which contain at least some final pixels; see
    // InterpolateHorizontally() for some additional explanation as to what that
    // means. Note that we've already written out the previous important row, so
    // we start the iteration at 1.
    for (int32_t outRow = 1; outRow < ImportantRowStride(aPass); ++outRow) {
      const float weight = weights[outRow];

      // We iterate through the previous and current important row every time we
      // write out an interpolated row, so we need to copy the pointers.
      uint8_t* prevRowBytes = aPreviousRow;
      uint8_t* currRowBytes = aCurrentRow;

      // Write out the interpolated pixels. Interpolation is componentwise.
      aNext.template WritePixelsToRow<uint32_t>([&] {
        uint32_t pixel = 0;
        auto* component = reinterpret_cast<uint8_t*>(&pixel);
        *component++ =
            InterpolateByte(*prevRowBytes++, *currRowBytes++, weight);
        *component++ =
            InterpolateByte(*prevRowBytes++, *currRowBytes++, weight);
        *component++ =
            InterpolateByte(*prevRowBytes++, *currRowBytes++, weight);
        *component++ =
            InterpolateByte(*prevRowBytes++, *currRowBytes++, weight);
        return AsVariant(pixel);
      });
    }
  }

  static void InterpolateHorizontally(uint8_t* aRow, int32_t aWidth,
                                      uint8_t aPass) {
    // Collect the data we'll need to perform horizontal interpolation. The
    // terminology here bears some explanation: a "final pixel" is a pixel which
    // has received its final value. On each pass, a new set of pixels receives
    // their final value; see the diagram above of the 8x8 pattern that ADAM7
    // uses. Any pixel which hasn't received its final value on this pass
    // derives its value from either horizontal or vertical interpolation
    // instead.
    const size_t finalPixelStride = FinalPixelStride(aPass);
    const size_t finalPixelStrideBytes = finalPixelStride * sizeof(uint32_t);
    const size_t lastFinalPixel = LastFinalPixel(aWidth, aPass);
    const size_t lastFinalPixelBytes = lastFinalPixel * sizeof(uint32_t);
    const float* weights = InterpolationWeights(finalPixelStride);

    // Interpolate blocks of pixels which lie between two final pixels.
    // Horizontal interpolation is done in place, as we'll need the results
    // later when we vertically interpolate.
    for (size_t blockBytes = 0; blockBytes < lastFinalPixelBytes;
         blockBytes += finalPixelStrideBytes) {
      uint8_t* finalPixelA = aRow + blockBytes;
      uint8_t* finalPixelB = aRow + blockBytes + finalPixelStrideBytes;

      MOZ_ASSERT(finalPixelA < aRow + aWidth * sizeof(uint32_t),
                 "Running off end of buffer");
      MOZ_ASSERT(finalPixelB < aRow + aWidth * sizeof(uint32_t),
                 "Running off end of buffer");

      // Interpolate the individual pixels componentwise. Note that we start
      // iteration at 1 since we don't need to apply any interpolation to the
      // first pixel in the block, which has its final value.
      for (size_t pixelIndex = 1; pixelIndex < finalPixelStride; ++pixelIndex) {
        const float weight = weights[pixelIndex];
        uint8_t* pixel = aRow + blockBytes + pixelIndex * sizeof(uint32_t);

        MOZ_ASSERT(pixel < aRow + aWidth * sizeof(uint32_t),
                   "Running off end of buffer");

        for (size_t component = 0; component < sizeof(uint32_t); ++component) {
          pixel[component] = InterpolateByte(finalPixelA[component],
                                             finalPixelB[component], weight);
        }
      }
    }

    // For the pixels after the last final pixel in the row, there isn't a
    // second final pixel to interpolate with, so just duplicate.
    uint32_t* rowPixels = reinterpret_cast<uint32_t*>(aRow);
    uint32_t pixelToDuplicate = rowPixels[lastFinalPixel];
    for (int32_t pixelIndex = lastFinalPixel + 1; pixelIndex < aWidth;
         ++pixelIndex) {
      MOZ_ASSERT(pixelIndex < aWidth, "Running off end of buffer");
      rowPixels[pixelIndex] = pixelToDuplicate;
    }
  }

  static uint8_t InterpolateByte(uint8_t aByteA, uint8_t aByteB,
                                 float aWeight) {
    return uint8_t(aByteA * aWeight + aByteB * (1.0f - aWeight));
  }

  static int32_t ImportantRowStride(uint8_t aPass) {
    MOZ_ASSERT(0 < aPass && aPass <= 7, "Invalid pass");

    // The stride between important rows for each pass, with a dummy value for
    // the nonexistent pass 0.
    static int32_t strides[] = {1, 8, 8, 4, 4, 2, 2, 1};

    return strides[aPass];
  }

  static bool IsImportantRow(int32_t aRow, uint8_t aPass) {
    MOZ_ASSERT(aRow >= 0);

    // Whether the row is important comes down to divisibility by the stride for
    // this pass, which is always a power of 2, so we can check using a mask.
    int32_t mask = ImportantRowStride(aPass) - 1;
    return (aRow & mask) == 0;
  }

  static int32_t LastImportantRow(int32_t aHeight, uint8_t aPass) {
    MOZ_ASSERT(aHeight > 0);

    // We can find the last important row using the same mask trick as above.
    int32_t lastRow = aHeight - 1;
    int32_t mask = ImportantRowStride(aPass) - 1;
    return lastRow - (lastRow & mask);
  }

  static size_t FinalPixelStride(uint8_t aPass) {
    MOZ_ASSERT(0 < aPass && aPass <= 7, "Invalid pass");

    // The stride between the final pixels in important rows for each pass, with
    // a dummy value for the nonexistent pass 0.
    static size_t strides[] = {1, 8, 4, 4, 2, 2, 1, 1};

    return strides[aPass];
  }

  static size_t LastFinalPixel(int32_t aWidth, uint8_t aPass) {
    MOZ_ASSERT(aWidth >= 0);

    // Again, we can use the mask trick above to find the last important pixel.
    int32_t lastColumn = aWidth - 1;
    size_t mask = FinalPixelStride(aPass) - 1;
    return lastColumn - (lastColumn & mask);
  }

  static const float* InterpolationWeights(int32_t aStride) {
    // Precalculated interpolation weights. These are used to interpolate
    // between final pixels or between important rows. Although no interpolation
    // is actually applied to the previous final pixel or important row value,
    // the arrays still start with 1.0f, which is always skipped, primarily
    // because otherwise |stride1Weights| would have zero elements.
    static float stride8Weights[] = {1.0f,     7 / 8.0f, 6 / 8.0f, 5 / 8.0f,
                                     4 / 8.0f, 3 / 8.0f, 2 / 8.0f, 1 / 8.0f};
    static float stride4Weights[] = {1.0f, 3 / 4.0f, 2 / 4.0f, 1 / 4.0f};
    static float stride2Weights[] = {1.0f, 1 / 2.0f};
    static float stride1Weights[] = {1.0f};

    switch (aStride) {
      case 8:
        return stride8Weights;
      case 4:
        return stride4Weights;
      case 2:
        return stride2Weights;
      case 1:
        return stride1Weights;
      default:
        MOZ_CRASH();
    }
  }

  Next mNext;  /// The next SurfaceFilter in the chain.

  UniquePtr<uint8_t[]>
      mPreviousRow;  /// The last important row (i.e., row with
                     /// final pixel values) that got written to.
  UniquePtr<uint8_t[]> mCurrentRow;  /// The row that's being written to right
                                     /// now.
  uint8_t mPass;                     /// Which ADAM7 pass we're on. Valid passes
                                     /// are 1..7 during processing and 0 prior
                                     /// to configuraiton.
  int32_t mRow;                      /// The row we're currently reading.
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_SurfaceFilters_h
