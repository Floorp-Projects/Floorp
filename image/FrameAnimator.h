/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_FrameAnimator_h
#define mozilla_image_FrameAnimator_h

#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TimeStamp.h"
#include "gfxTypes.h"
#include "imgFrame.h"
#include "nsCOMPtr.h"
#include "nsRect.h"
#include "SurfaceCache.h"

namespace mozilla {
namespace image {

class RasterImage;

class AnimationState
{
public:
  explicit AnimationState(uint16_t aAnimationMode)
    : mFrameCount(0)
    , mCurrentAnimationFrameIndex(0)
    , mLoopRemainingCount(-1)
    , mLoopCount(-1)
    , mFirstFrameTimeout(FrameTimeout::FromRawMilliseconds(0))
    , mAnimationMode(aAnimationMode)
    , mDoneDecoding(false)
  { }

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

  /// Update the number of frames of animation this image is known to have.
  void UpdateKnownFrameCount(uint32_t aFrameCount);

  /// @return the number of frames of animation we know about so far.
  uint32_t KnownFrameCount() const { return mFrameCount; }

  /// @return the number of frames this animation has, if we know for sure.
  /// (In other words, if decoding is finished.) Otherwise, returns Nothing().
  Maybe<uint32_t> FrameCount() const;

  /**
   * Get or set the area of the image to invalidate when we loop around to the
   * first frame.
   */
  void SetFirstFrameRefreshArea(const gfx::IntRect& aRefreshArea);
  gfx::IntRect FirstFrameRefreshArea() const { return mFirstFrameRefreshArea; }

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

  /*
   * Set number of times to loop the image.
   * @note -1 means loop forever.
   */
  void SetLoopCount(int32_t aLoopCount) { mLoopCount = aLoopCount; }
  int32_t LoopCount() const { return mLoopCount; }

  /// Set the @aLength of a single loop through this image.
  void SetLoopLength(FrameTimeout aLength) { mLoopLength = Some(aLength); }

  /**
   * @return the length of a single loop of this image. If this image is not
   * finished decoding, is not animated, or it is animated but does not loop,
   * returns FrameTimeout::Forever().
   */
  FrameTimeout LoopLength() const;

  /*
   * Get or set the timeout for the first frame. This is used to allow animation
   * scheduling even before a full decode runs for this image.
   */
  void SetFirstFrameTimeout(FrameTimeout aTimeout) { mFirstFrameTimeout = aTimeout; }
  FrameTimeout FirstFrameTimeout() const { return mFirstFrameTimeout; }

private:
  friend class FrameAnimator;

  //! Area of the first frame that needs to be redrawn on subsequent loops.
  gfx::IntRect mFirstFrameRefreshArea;

  //! the time that the animation advanced to the current frame
  TimeStamp mCurrentAnimationFrameTime;

  //! The number of frames of animation this image has.
  uint32_t mFrameCount;

  //! The current frame index we're on, in the range [0, mFrameCount).
  uint32_t mCurrentAnimationFrameIndex;

  //! number of loops remaining before animation stops (-1 no stop)
  int32_t mLoopRemainingCount;

  //! The total number of loops for the image.
  int32_t mLoopCount;

  //! The length of a single loop through this image.
  Maybe<FrameTimeout> mLoopLength;

  //! The timeout for the first frame of this image.
  FrameTimeout mFirstFrameTimeout;

  //! The animation mode of this image. Constants defined in imgIContainer.
  uint16_t mAnimationMode;

  //! Whether this image is done being decoded.
  bool mDoneDecoding;
};

/**
 * RefreshResult is used to let callers know how the state of the animation
 * changed during a call to FrameAnimator::RequestRefresh().
 */
struct RefreshResult
{
  RefreshResult()
    : mFrameAdvanced(false)
    , mAnimationFinished(false)
  { }

  /// Merges another RefreshResult's changes into this RefreshResult.
  void Accumulate(const RefreshResult& aOther)
  {
    mFrameAdvanced = mFrameAdvanced || aOther.mFrameAdvanced;
    mAnimationFinished = mAnimationFinished || aOther.mAnimationFinished;
    mDirtyRect = mDirtyRect.Union(aOther.mDirtyRect);
  }

  // The region of the image that has changed.
  gfx::IntRect mDirtyRect;

  // If true, we changed frames at least once. Note that, due to looping, we
  // could still have ended up on the same frame!
  bool mFrameAdvanced : 1;

  // Whether the animation has finished playing.
  bool mAnimationFinished : 1;
};

class FrameAnimator
{
public:
  FrameAnimator(RasterImage* aImage, const gfx::IntSize& aSize)
    : mImage(aImage)
    , mSize(aSize)
    , mLastCompositedFrameIndex(-1)
  {
     MOZ_COUNT_CTOR(FrameAnimator);
  }

  ~FrameAnimator()
  {
    MOZ_COUNT_DTOR(FrameAnimator);
  }

  /**
   * Re-evaluate what frame we're supposed to be on, and do whatever blending
   * is necessary to get us to that frame.
   *
   * Returns the result of that blending, including whether the current frame
   * changed and what the resulting dirty rectangle is.
   */
  RefreshResult RequestRefresh(AnimationState& aState, const TimeStamp& aTime);

  /**
   * If we have a composited frame for @aFrameNum, returns it. Otherwise,
   * returns an empty LookupResult. It is an error to call this method with
   * aFrameNum == 0, because the first frame is never composited.
   */
  LookupResult GetCompositedFrame(uint32_t aFrameNum);

  /**
   * Collect an accounting of the memory occupied by the compositing surfaces we
   * use during animation playback. All of the actual animation frames are
   * stored in the SurfaceCache, so we don't need to report them here.
   */
  void CollectSizeOfCompositingSurfaces(nsTArray<SurfaceMemoryCounter>& aCounters,
                                        MallocSizeOf aMallocSizeOf) const;

private: // methods
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
  RefreshResult AdvanceFrame(AnimationState& aState, TimeStamp aTime);

  /**
   * Get the @aIndex-th frame in the frame index, ignoring results of blending.
   */
  RawAccessFrameRef GetRawFrame(uint32_t aFrameNum) const;

  /// @return the given frame's timeout.
  FrameTimeout GetTimeoutForFrame(uint32_t aFrameNum) const;

  /**
   * Get the time the frame we're currently displaying is supposed to end.
   *
   * In the error case, returns an "infinity" timestamp.
   */
  TimeStamp GetCurrentImgFrameEndTime(AnimationState& aState) const;

  bool DoBlend(gfx::IntRect* aDirtyRect,
               uint32_t aPrevFrameIndex,
               uint32_t aNextFrameIndex);

  /** Clears an area of <aFrame> with transparent black.
   *
   * @param aFrameData Target Frame data
   * @param aFrameRect The rectangle of the data pointed ot by aFrameData
   *
   * @note Does also clears the transparency mask
   */
  static void ClearFrame(uint8_t* aFrameData, const gfx::IntRect& aFrameRect);

  //! @overload
  static void ClearFrame(uint8_t* aFrameData, const gfx::IntRect& aFrameRect,
                         const gfx::IntRect& aRectToClear);

  //! Copy one frame's image and mask into another
  static bool CopyFrameImage(const uint8_t* aDataSrc, const gfx::IntRect& aRectSrc,
                             uint8_t* aDataDest, const gfx::IntRect& aRectDest);

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
                              const gfx::IntRect& aSrcRect,
                              uint32_t aSrcPaletteLength, bool aSrcHasAlpha,
                              uint8_t* aDstPixels, const gfx::IntRect& aDstRect,
                              BlendMethod aBlendMethod,
                              const Maybe<gfx::IntRect>& aBlendRect);

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

  //! Track the last composited frame for Optimizations (See DoComposite code)
  int32_t mLastCompositedFrameIndex;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_FrameAnimator_h
