/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_imagelib_FrameAnimator_h_
#define mozilla_imagelib_FrameAnimator_h_

#include "mozilla/TimeStamp.h"
#include "FrameBlender.h"
#include "nsRect.h"

namespace mozilla {
namespace image {

class FrameAnimator
{
public:
  FrameAnimator(FrameBlender& aBlender);

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
    {}

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
  RefreshResult RequestRefresh(const mozilla::TimeStamp& aTime);

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
   * Number of times to loop the image.
   * @note -1 means forever.
   */
  void SetLoopCount(int32_t aLoopCount);

  /**
   * The animation mode of the image.
   *
   * Constants defined in imgIContainer.idl.
   */
  void SetAnimationMode(uint16_t aAnimationMode);

  /**
   * Set the area to refresh when we loop around to the first frame.
   */
  void SetFirstFrameRefreshArea(const nsIntRect& aRect);

  /**
   * Union the area to refresh when we loop around to the first frame with this
   * rect.
   */
  void UnionFirstFrameRefreshArea(const nsIntRect& aRect);

  /**
   * Set the animation frame time to TimeStamp::Now().
   */
  void InitAnimationFrameTime();

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

private: // methods
  /**
   * Gets the length of a single loop of this image, in milliseconds.
   *
   * If this image is not finished decoding, is not animated, or it is animated
   * but does not loop, returns 0.
   */
  uint32_t GetSingleLoopTime() const;

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
  RefreshResult AdvanceFrame(mozilla::TimeStamp aTime);

  /**
   * Get the time the frame we're currently displaying is supposed to end.
   *
   * In the error case, returns an "infinity" timestamp.
   */
  mozilla::TimeStamp GetCurrentImgFrameEndTime() const;

private: // data
  //! Area of the first frame that needs to be redrawn on subsequent loops.
  nsIntRect mFirstFrameRefreshArea;

  //! the time that the animation advanced to the current frame
  TimeStamp mCurrentAnimationFrameTime;

  //! The current frame index we're on. 0 to (numFrames - 1).
  uint32_t mCurrentAnimationFrameIndex;

  //! number of loops remaining before animation stops (-1 no stop)
  int32_t mLoopCount;

  //! All the frames of the image, shared with our owner
  FrameBlender& mFrameBlender;

  //! The animation mode of this image. Constants defined in imgIContainer.
  uint16_t mAnimationMode;

  //! Whether this image is done being decoded.
  bool mDoneDecoding;
};

} // namespace image
} // namespace mozilla

#endif /* mozilla_imagelib_FrameAnimator_h_ */
