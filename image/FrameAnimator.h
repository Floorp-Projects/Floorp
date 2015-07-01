/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_FrameAnimator_h
#define mozilla_image_FrameAnimator_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/TimeStamp.h"
#include "gfx2DGlue.h"
#include "gfxTypes.h"
#include "imgFrame.h"
#include "nsCOMPtr.h"
#include "nsRect.h"
#include "SurfaceCache.h"

namespace mozilla {
namespace image {

class RasterImage;

class FrameAnimator
{
public:
  FrameAnimator(RasterImage* aImage,
                gfx::IntSize aSize,
                uint16_t aAnimationMode)
    : mImage(aImage)
    , mSize(aSize)
    , mCurrentAnimationFrameIndex(0)
    , mLoopRemainingCount(-1)
    , mLastCompositedFrameIndex(-1)
    , mLoopCount(-1)
    , mAnimationMode(aAnimationMode)
    , mDoneDecoding(false)
  { }

  /**
   * Return value from RequestRefresh. Tells callers what happened in that call
   * to RequestRefresh.
   */
  struct RefreshResult
  {
    // The dirty rectangle to be re-drawn after this RequestRefresh().
    nsIntRect dirtyRect;

    // Whether any frame changed, and hence the dirty rect was set.
    bool frameAdvanced : 1;

    // Whether the animation has finished playing.
    bool animationFinished : 1;

    // Whether an error has occurred when trying to advance a frame. Note that
    // errors do not, on their own, end the animation.
    bool error : 1;

    RefreshResult()
      : frameAdvanced(false)
      , animationFinished(false)
      , error(false)
    { }

    void Accumulate(const RefreshResult& other)
    {
      frameAdvanced = frameAdvanced || other.frameAdvanced;
      animationFinished = animationFinished || other.animationFinished;
      error = error || other.error;
      dirtyRect = dirtyRect.Union(other.dirtyRect);
    }
  };

  /**
   * Re-evaluate what frame we're supposed to be on, and do whatever blending
   * is necessary to get us to that frame.
   *
   * Returns the result of that blending, including whether the current frame
   * changed and what the resulting dirty rectangle is.
   */
  RefreshResult RequestRefresh(const TimeStamp& aTime);

  /**
   * Call when this image is finished decoding so we know that there aren't any
   * more frames coming.
   */
  void SetDoneDecoding(bool aDone);

  /**
   * Call when you need to re-start animating. Ensures we start from the first
   * frame.
   */
  void ResetAnimation();

  /**
   * The animation mode of the image.
   *
   * Constants defined in imgIContainer.idl.
   */
  void SetAnimationMode(uint16_t aAnimationMode);

  /**
   * Union the area to refresh when we loop around to the first frame with this
   * rect.
   */
  void UnionFirstFrameRefreshArea(const nsIntRect& aRect);

  /**
   * If the animation frame time has not yet been set, set it to
   * TimeStamp::Now().
   */
  void InitAnimationFrameTimeIfNecessary();

  /**
   * Set the animation frame time to @aTime.
   */
  void SetAnimationFrameTime(const TimeStamp& aTime);

  /**
   * The current frame we're on, from 0 to (numFrames - 1).
   */
  uint32_t GetCurrentAnimationFrameIndex() const;

  /**
   * Get the area we refresh when we loop around to the first frame.
   */
  nsIntRect GetFirstFrameRefreshArea() const;

  /**
   * If we have a composited frame for @aFrameNum, returns it. Otherwise,
   * returns an empty LookupResult. It is an error to call this method with
   * aFrameNum == 0, because the first frame is never composited.
   */
  LookupResult GetCompositedFrame(uint32_t aFrameNum);

  /*
   * Returns the frame's adjusted timeout. If the animation loops and the
   * timeout falls in between a certain range then the timeout is adjusted so
   * that it's never 0. If the animation does not loop then no adjustments are
   * made.
   */
  int32_t GetTimeoutForFrame(uint32_t aFrameNum) const;

  /*
   * Set number of times to loop the image.
   * @note -1 means loop forever.
   */
  void SetLoopCount(int32_t aLoopCount) { mLoopCount = aLoopCount; }
  int32_t LoopCount() const { return mLoopCount; }

  /**
   * Collect an accounting of the memory occupied by the compositing surfaces we
   * use during animation playback. All of the actual animation frames are
   * stored in the SurfaceCache, so we don't need to report them here.
   */
  void CollectSizeOfCompositingSurfaces(nsTArray<SurfaceMemoryCounter>& aCounters,
                                        MallocSizeOf aMallocSizeOf) const;

private: // methods
  /**
   * Gets the length of a single loop of this image, in milliseconds.
   *
   * If this image is not finished decoding, is not animated, or it is animated
   * but does not loop, returns -1. Can return 0 in the case of an animated
   * image that has a 0ms delay between its frames and does not loop.
   */
  int32_t GetSingleLoopTime() const;

  /**
   * Advances the animation. Typically, this will advance a single frame, but it
   * may advance multiple frames. This may happen if we have infrequently
   * "ticking" refresh drivers (e.g. in background tabs), or extremely short-
   * lived animation frames.
   *
   * @param aTime the time that the animation should advance to. This will
   *              typically be <= TimeStamp::Now().
   *
   * @returns a RefreshResult that shows whether the frame was successfully
   *          advanced, and its resulting dirty rect.
   */
  RefreshResult AdvanceFrame(TimeStamp aTime);

  /**
   * Get the time the frame we're currently displaying is supposed to end.
   *
   * In the error case, returns an "infinity" timestamp.
   */
  TimeStamp GetCurrentImgFrameEndTime() const;

  bool DoBlend(nsIntRect* aDirtyRect, uint32_t aPrevFrameIndex,
               uint32_t aNextFrameIndex);

  /**
   * Get the @aIndex-th frame in the frame index, ignoring results of blending.
   */
  RawAccessFrameRef GetRawFrame(uint32_t aFrameNum) const;

  /** Clears an area of <aFrame> with transparent black.
   *
   * @param aFrameData Target Frame data
   * @param aFrameRect The rectangle of the data pointed ot by aFrameData
   *
   * @note Does also clears the transparency mask
   */
  static void ClearFrame(uint8_t* aFrameData, const nsIntRect& aFrameRect);

  //! @overload
  static void ClearFrame(uint8_t* aFrameData, const nsIntRect& aFrameRect,
                         const nsIntRect& aRectToClear);

  //! Copy one frame's image and mask into another
  static bool CopyFrameImage(const uint8_t* aDataSrc, const nsIntRect& aRectSrc,
                             uint8_t* aDataDest, const nsIntRect& aRectDest);

  /**
   * Draws one frame's image to into another, at the position specified by
   * aSrcRect.
   *
   * @aSrcData the raw data of the current frame being drawn
   * @aSrcRect the size of the source frame, and the position of that frame in
   *           the composition frame
   * @aSrcPaletteLength the length (in bytes) of the palette at the beginning
   *                    of the source data (0 if image is not paletted)
   * @aSrcHasAlpha whether the source data represents an image with alpha
   * @aDstPixels the raw data of the composition frame where the current frame
   *             is drawn into (32-bit ARGB)
   * @aDstRect the size of the composition frame
   * @aBlendMethod the blend method for how to blend src on the composition
   * frame.
   */
  static nsresult DrawFrameTo(const uint8_t* aSrcData,
                              const nsIntRect& aSrcRect,
                              uint32_t aSrcPaletteLength, bool aSrcHasAlpha,
                              uint8_t* aDstPixels, const nsIntRect& aDstRect,
                              BlendMethod aBlendMethod);

private: // data
  //! A weak pointer to our owning image.
  RasterImage* mImage;

  //! The intrinsic size of the image.
  gfx::IntSize mSize;

  /** For managing blending of frames
   *
   * Some animations will use the compositingFrame to composite images
   * and just hand this back to the caller when it is time to draw the frame.
   * NOTE: When clearing compositingFrame, remember to set
   *       lastCompositedFrameIndex to -1.  Code assume that if
   *       lastCompositedFrameIndex >= 0 then compositingFrame exists.
   */
  RawAccessFrameRef mCompositingFrame;

  /** the previous composited frame, for DISPOSE_RESTORE_PREVIOUS
   *
   * The Previous Frame (all frames composited up to the current) needs to be
   * stored in cases where the image specifies it wants the last frame back
   * when it's done with the current frame.
   */
  RawAccessFrameRef mCompositingPrevFrame;

  //! Area of the first frame that needs to be redrawn on subsequent loops.
  nsIntRect mFirstFrameRefreshArea;

  //! the time that the animation advanced to the current frame
  TimeStamp mCurrentAnimationFrameTime;

  //! The current frame index we're on. 0 to (numFrames - 1).
  uint32_t mCurrentAnimationFrameIndex;

  //! number of loops remaining before animation stops (-1 no stop)
  int32_t mLoopRemainingCount;

  //! Track the last composited frame for Optimizations (See DoComposite code)
  int32_t mLastCompositedFrameIndex;

  //! The total number of loops for the image.
  int32_t mLoopCount;

  //! The animation mode of this image. Constants defined in imgIContainer.
  uint16_t mAnimationMode;

  //! Whether this image is done being decoded.
  bool mDoneDecoding;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_FrameAnimator_h
