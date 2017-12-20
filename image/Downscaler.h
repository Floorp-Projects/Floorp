/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Downscaler is a high-quality, streaming image downscaler based upon Skia's
 * scaling implementation.
 */

#ifndef mozilla_image_Downscaler_h
#define mozilla_image_Downscaler_h

#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "gfxPoint.h"
#include "nsRect.h"
#ifdef MOZ_ENABLE_SKIA
#include "mozilla/gfx/ConvolutionFilter.h"
#endif

namespace mozilla {
namespace image {

/**
 * DownscalerInvalidRect wraps two invalidation rects: one in terms of the
 * original image size, and one in terms of the target size.
 */
struct DownscalerInvalidRect
{
  nsIntRect mOriginalSizeRect;
  nsIntRect mTargetSizeRect;
};

#ifdef MOZ_ENABLE_SKIA

/**
 * Downscaler is a high-quality, streaming image downscaler based upon Skia's
 * scaling implementation.
 *
 * Decoders can construct a Downscaler once they know their target size, then
 * call BeginFrame() for each frame they decode. They should write a decoded row
 * into the buffer returned by RowBuffer(), and then call CommitRow() to signal
 * that they have finished.
 *

 * Because invalidations need to be computed in terms of the scaled version of
 * the image, Downscaler also tracks them. Decoders can call HasInvalidation()
 * and TakeInvalidRect() instead of tracking invalidations themselves.
 */
class Downscaler
{
public:
  /// Constructs a new Downscaler which to scale to size @aTargetSize.
  explicit Downscaler(const nsIntSize& aTargetSize);
  ~Downscaler();

  const nsIntSize& OriginalSize() const { return mOriginalSize; }
  const nsIntSize& TargetSize() const { return mTargetSize; }
  const nsIntSize FrameSize() const { return nsIntSize(mFrameRect.Width(), mFrameRect.Height()); }
  const gfxSize& Scale() const { return mScale; }

  /**
   * Begins a new frame and reinitializes the Downscaler.
   *
   * @param aOriginalSize The original size of this frame, before scaling.
   * @param aFrameRect The region of  the original image which has data.
   *                   Every pixel outside @aFrameRect is considered blank and
   *                   has zero alpha.
   * @param aOutputBuffer The buffer to which the Downscaler should write its
   *                      output; this is the same buffer where the Decoder
   *                      would write its output when not downscaling during
   *                      decode.
   * @param aHasAlpha Whether or not this frame has an alpha channel.
   *                  Performance is a little better if it doesn't have one.
   * @param aFlipVertically If true, output rows will be written to the output
   *                        buffer in reverse order vertically, which matches
   *                        the way they are stored in some image formats.
   */
  nsresult BeginFrame(const nsIntSize& aOriginalSize,
                      const Maybe<nsIntRect>& aFrameRect,
                      uint8_t* aOutputBuffer,
                      bool aHasAlpha,
                      bool aFlipVertically = false);

  bool IsFrameComplete() const { return mCurrentInLine >= mOriginalSize.height; }

  /// Retrieves the buffer into which the Decoder should write each row.
  uint8_t* RowBuffer()
  {
    return mRowBuffer.get() + mFrameRect.X() * sizeof(uint32_t);
  }

  /// Clears the current row buffer.
  void ClearRow() { ClearRestOfRow(0); }

  /// Clears the current row buffer starting at @aStartingAtCol.
  void ClearRestOfRow(uint32_t aStartingAtCol);

  /// Signals that the decoder has finished writing a row into the row buffer.
  void CommitRow();

  /// Returns true if there is a non-empty invalid rect available.
  bool HasInvalidation() const;

  /// Takes the Downscaler's current invalid rect and resets it.
  DownscalerInvalidRect TakeInvalidRect();

  /**
   * Resets the Downscaler's position in the image, for a new progressive pass
   * over the same frame. Because the same data structures can be reused, this
   * is more efficient than calling BeginFrame.
   */
  void ResetForNextProgressivePass();

private:
  void DownscaleInputLine();
  void ReleaseWindow();
  void SkipToRow(int32_t aRow);

  nsIntSize mOriginalSize;
  nsIntSize mTargetSize;
  nsIntRect mFrameRect;
  gfxSize mScale;

  uint8_t* mOutputBuffer;

  UniquePtr<uint8_t[]> mRowBuffer;
  UniquePtr<uint8_t*[]> mWindow;

  gfx::ConvolutionFilter mXFilter;
  gfx::ConvolutionFilter mYFilter;

  int32_t mWindowCapacity;

  int32_t mLinesInBuffer;
  int32_t mPrevInvalidatedLine;
  int32_t mCurrentOutLine;
  int32_t mCurrentInLine;

  bool mHasAlpha : 1;
  bool mFlipVertically : 1;
};

#else

/**
 * Downscaler requires Skia to work, so we provide a dummy implementation if
 * Skia is disabled that asserts if constructed.
 */

class Downscaler
{
public:
  explicit Downscaler(const nsIntSize&) : mScale(1.0, 1.0)
  {
    MOZ_RELEASE_ASSERT(false, "Skia is not enabled");
  }

  const nsIntSize& OriginalSize() const { return mSize; }
  const nsIntSize& TargetSize() const { return mSize; }
  const gfxSize& Scale() const { return mScale; }

  nsresult BeginFrame(const nsIntSize&, const Maybe<nsIntRect>&, uint8_t*, bool, bool = false)
  {
    return NS_ERROR_FAILURE;
  }

  bool IsFrameComplete() const { return false; }
  uint8_t* RowBuffer() { return nullptr; }
  void ClearRow() { }
  void ClearRestOfRow(uint32_t) { }
  void CommitRow() { }
  bool HasInvalidation() const { return false; }
  DownscalerInvalidRect TakeInvalidRect() { return DownscalerInvalidRect(); }
  void ResetForNextProgressivePass() { }
  const nsIntSize FrameSize() const { return nsIntSize(0, 0); }
private:
  nsIntSize mSize;
  gfxSize mScale;
};

#endif // MOZ_ENABLE_SKIA



} // namespace image
} // namespace mozilla

#endif // mozilla_image_Downscaler_h
