/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_FrameAnimator_h
#define mozilla_image_FrameAnimator_h

#include "mozilla/Maybe.h"
#include "mozilla/StaticPrefs_image.h"
#include "mozilla/TimeStamp.h"
#include "gfxTypes.h"
#include "imgFrame.h"
#include "nsCOMPtr.h"
#include "nsRect.h"
#include "SurfaceCache.h"

namespace mozilla {
namespace image {

class RasterImage;
class DrawableSurface;

class AnimationState {
 public:
  explicit AnimationState(uint16_t aAnimationMode)
      : mFrameCount(0),
        mCurrentAnimationFrameIndex(0),
        mLoopRemainingCount(-1),
        mLoopCount(-1),
        mFirstFrameTimeout(FrameTimeout::FromRawMilliseconds(0)),
        mAnimationMode(aAnimationMode),
        mHasBeenDecoded(false),
        mHasRequestedDecode(false),
        mIsCurrentlyDecoded(false),
        mCompositedFrameInvalid(false),
        mCompositedFrameRequested(false),
        mDiscarded(false) {}

  /**
   * Call this whenever a decode completes, a decode starts, or the image is
   * discarded. It will update the internal state. Specifically mDiscarded,
   * mCompositedFrameInvalid, and mIsCurrentlyDecoded. If aAllowInvalidation
   * is true then returns a rect to invalidate.
   */
  const gfx::IntRect UpdateState(bool aAnimationFinished, RasterImage* aImage,
                                 const gfx::IntSize& aSize,
                                 bool aAllowInvalidation = true);

 private:
  const gfx::IntRect UpdateStateInternal(LookupResult& aResult,
                                         bool aAnimationFinished,
                                         const gfx::IntSize& aSize,
                                         bool aAllowInvalidation = true);

 public:
  /**
   * Call when a decode of this image has been completed.
   */
  void NotifyDecodeComplete();

  /**
   * Returns true if this image has been fully decoded before.
   */
  bool GetHasBeenDecoded() { return mHasBeenDecoded; }

  /**
   * Returns true if this image has ever requested a decode before.
   */
  bool GetHasRequestedDecode() { return mHasRequestedDecode; }

  /**
   * Returns true if this image has been discarded and a decoded has not yet
   * been created to redecode it.
   */
  bool IsDiscarded() { return mDiscarded; }

  /**
   * Sets the composited frame as valid or invalid.
   */
  void SetCompositedFrameInvalid(bool aInvalid) {
    MOZ_ASSERT(!aInvalid ||
               StaticPrefs::image_mem_animated_discardable_AtStartup());
    mCompositedFrameInvalid = aInvalid;
  }

  /**
   * Returns whether the composited frame is valid to draw to the screen.
   */
  bool GetCompositedFrameInvalid() { return mCompositedFrameInvalid; }

  /**
   * Returns whether the image is currently full decoded..
   */
  bool GetIsCurrentlyDecoded() { return mIsCurrentlyDecoded; }

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
   * Set the animation frame time to @aTime if we are configured to stop the
   * animation when not visible and aTime is later than the current time.
   * Returns true if the time was updated, else false.
   */
  bool MaybeAdvanceAnimationFrameTime(const TimeStamp& aTime);

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
  void SetFirstFrameTimeout(FrameTimeout aTimeout) {
    mFirstFrameTimeout = aTimeout;
  }
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

  /**
   * The following four bools (mHasBeenDecoded, mIsCurrentlyDecoded,
   * mCompositedFrameInvalid, mDiscarded) track the state of the image with
   * regards to decoding. They all start out false, including mDiscarded,
   * because we want to treat being discarded differently from "not yet decoded
   * for the first time".
   *
   * (When we are decoding the image for the first time we want to show the
   * image at the speed of data coming in from the network or the speed
   * specified in the image file, whichever is slower. But when redecoding we
   * want to show nothing until the frame for the current time has been
   * decoded. The prevents the user from seeing the image "fast forward"
   * to the expected spot.)
   *
   * When the image is decoded for the first time mHasBeenDecoded and
   * mIsCurrentlyDecoded get set to true. When the image is discarded
   * mIsCurrentlyDecoded gets set to false, and mCompositedFrameInvalid
   * & mDiscarded get set to true. When we create a decoder to redecode the
   * image mDiscarded gets set to false. mCompositedFrameInvalid gets set to
   * false when we are able to advance to the frame that should be showing
   * for the current time. mIsCurrentlyDecoded gets set to true when the
   * redecode finishes.
   */

  //! Whether this image has been decoded at least once.
  bool mHasBeenDecoded;

  //! Whether this image has ever requested a decode.
  bool mHasRequestedDecode;

  //! Whether this image is currently fully decoded.
  bool mIsCurrentlyDecoded;

  //! Whether the composited frame is valid to draw to the screen, note that
  //! the composited frame can exist and be filled with image data but not
  //! valid to draw to the screen.
  bool mCompositedFrameInvalid;

  //! Whether the composited frame was requested from the animator since the
  //! last time we advanced the animation.
  bool mCompositedFrameRequested;

  //! Whether this image is currently discarded. Only set to true after the
  //! image has been decoded at least once.
  bool mDiscarded;
};

/**
 * RefreshResult is used to let callers know how the state of the animation
 * changed during a call to FrameAnimator::RequestRefresh().
 */
struct RefreshResult {
  RefreshResult() : mFrameAdvanced(false), mAnimationFinished(false) {}

  /// Merges another RefreshResult's changes into this RefreshResult.
  void Accumulate(const RefreshResult& aOther) {
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

class FrameAnimator {
 public:
  FrameAnimator(RasterImage* aImage, const gfx::IntSize& aSize)
      : mImage(aImage), mSize(aSize) {
    MOZ_COUNT_CTOR(FrameAnimator);
  }

  MOZ_COUNTED_DTOR(FrameAnimator)

  /**
   * Call when you need to re-start animating. Ensures we start from the first
   * frame.
   */
  void ResetAnimation(AnimationState& aState);

  /**
   * Re-evaluate what frame we're supposed to be on, and do whatever blending
   * is necessary to get us to that frame.
   *
   * Returns the result of that blending, including whether the current frame
   * changed and what the resulting dirty rectangle is.
   */
  RefreshResult RequestRefresh(AnimationState& aState, const TimeStamp& aTime,
                               bool aAnimationFinished);

  /**
   * Get the full frame for the current frame of the animation (it may or may
   * not have required compositing). It may not be available because it hasn't
   * been decoded yet, in which case we return an empty LookupResult.
   */
  LookupResult GetCompositedFrame(AnimationState& aState, bool aMarkUsed);

 private:  // methods
  /**
   * Advances the animation. Typically, this will advance a single frame, but it
   * may advance multiple frames. This may happen if we have infrequently
   * "ticking" refresh drivers (e.g. in background tabs), or extremely short-
   * lived animation frames.
   *
   * @param aTime the time that the animation should advance to. This will
   *              typically be <= TimeStamp::Now().
   *
   * @param aCurrentFrame the currently displayed frame of the animation. If
   *                      we advance, it will replace aCurrentFrame with the
   *                      new current frame we advanced to.
   *
   * @returns a RefreshResult that shows whether the frame was successfully
   *          advanced, and its resulting dirty rect.
   */
  RefreshResult AdvanceFrame(AnimationState& aState, DrawableSurface& aFrames,
                             RefPtr<imgFrame>& aCurrentFrame, TimeStamp aTime);

  /**
   * Get the time the frame we're currently displaying is supposed to end.
   *
   * In the error case (like if the requested frame is not currently
   * decoded), returns None().
   */
  TimeStamp GetCurrentImgFrameEndTime(AnimationState& aState,
                                      FrameTimeout aCurrentTimeout) const;

 private:  // data
  //! A weak pointer to our owning image.
  RasterImage* mImage;

  //! The intrinsic size of the image.
  gfx::IntSize mSize;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_FrameAnimator_h
