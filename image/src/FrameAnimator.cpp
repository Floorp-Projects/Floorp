/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FrameAnimator.h"
#include "FrameBlender.h"

#include "imgIContainer.h"

namespace mozilla {
namespace image {

FrameAnimator::FrameAnimator(FrameBlender& aFrameBlender,
                             uint16_t aAnimationMode)
  : mCurrentAnimationFrameIndex(0)
  , mLoopCounter(-1)
  , mFrameBlender(aFrameBlender)
  , mAnimationMode(aAnimationMode)
  , mDoneDecoding(false)
{
}

int32_t
FrameAnimator::GetSingleLoopTime() const
{
  // If we aren't done decoding, we don't know the image's full play time.
  if (!mDoneDecoding) {
    return -1;
  }

  // If we're not looping, a single loop time has no meaning
  if (mAnimationMode != imgIContainer::kNormalAnimMode) {
    return -1;
  }

  uint32_t looptime = 0;
  for (uint32_t i = 0; i < mFrameBlender.GetNumFrames(); ++i) {
    int32_t timeout = mFrameBlender.GetTimeoutForFrame(i);
    if (timeout >= 0) {
      looptime += static_cast<uint32_t>(timeout);
    } else {
      // If we have a frame that never times out, we're probably in an error
      // case, but let's handle it more gracefully.
      NS_WARNING("Negative frame timeout - how did this happen?");
      return -1;
    }
  }

  return looptime;
}

TimeStamp
FrameAnimator::GetCurrentImgFrameEndTime() const
{
  TimeStamp currentFrameTime = mCurrentAnimationFrameTime;
  int32_t timeout = mFrameBlender.GetTimeoutForFrame(mCurrentAnimationFrameIndex);

  if (timeout < 0) {
    // We need to return a sentinel value in this case, because our logic
    // doesn't work correctly if we have a negative timeout value. We use
    // one year in the future as the sentinel because it works with the loop
    // in RequestRefresh() below.
    // XXX(seth): It'd be preferable to make our logic work correctly with
    // negative timeouts.
    return TimeStamp::NowLoRes() +
           TimeDuration::FromMilliseconds(31536000.0);
  }

  TimeDuration durationOfTimeout =
    TimeDuration::FromMilliseconds(static_cast<double>(timeout));
  TimeStamp currentFrameEndTime = currentFrameTime + durationOfTimeout;

  return currentFrameEndTime;
}

FrameAnimator::RefreshResult
FrameAnimator::AdvanceFrame(TimeStamp aTime)
{
  NS_ASSERTION(aTime <= TimeStamp::Now(),
               "Given time appears to be in the future");

  uint32_t currentFrameIndex = mCurrentAnimationFrameIndex;
  uint32_t nextFrameIndex = currentFrameIndex + 1;
  int32_t timeout = 0;

  RefreshResult ret;
  nsRefPtr<imgFrame> nextFrame = mFrameBlender.RawGetFrame(nextFrameIndex);

  // If we're done decoding, we know we've got everything we're going to get.
  // If we aren't, we only display fully-downloaded frames; everything else
  // gets delayed.
  bool canDisplay = mDoneDecoding || (nextFrame && nextFrame->ImageComplete());

  if (!canDisplay) {
    // Uh oh, the frame we want to show is currently being decoded (partial)
    // Wait until the next refresh driver tick and try again
    return ret;
  }

  // If we're done decoding the next frame, go ahead and display it now and
  // reinit with the next frame's delay time.
  if (mFrameBlender.GetNumFrames() == nextFrameIndex) {
    // End of an animation loop...

    // If we are not looping forever, initialize the loop counter
    if (mLoopCounter < 0 && mFrameBlender.GetLoopCount() >= 0) {
      mLoopCounter = mFrameBlender.GetLoopCount();
    }

    // If animation mode is "loop once", or we're at end of loop counter, it's time to stop animating
    if (mAnimationMode == imgIContainer::kLoopOnceAnimMode || mLoopCounter == 0) {
      ret.animationFinished = true;
    }

    nextFrameIndex = 0;

    if (mLoopCounter > 0) {
      mLoopCounter--;
    }

    // If we're done, exit early.
    if (ret.animationFinished) {
      return ret;
    }
  }

  timeout = mFrameBlender.GetTimeoutForFrame(nextFrameIndex);

  // Bad data
  if (timeout < 0) {
    ret.animationFinished = true;
    ret.error = true;
  }

  if (nextFrameIndex == 0) {
    ret.dirtyRect = mFirstFrameRefreshArea;
  } else {
    // Change frame
    if (nextFrameIndex != currentFrameIndex + 1) {
      nextFrame = mFrameBlender.RawGetFrame(nextFrameIndex);
    }

    if (!mFrameBlender.DoBlend(&ret.dirtyRect, currentFrameIndex, nextFrameIndex)) {
      // something went wrong, move on to next
      NS_WARNING("FrameAnimator::AdvanceFrame(): Compositing of frame failed");
      nextFrame->SetCompositingFailed(true);
      mCurrentAnimationFrameTime = GetCurrentImgFrameEndTime();
      mCurrentAnimationFrameIndex = nextFrameIndex;

      ret.error = true;
      return ret;
    }

    nextFrame->SetCompositingFailed(false);
  }

  mCurrentAnimationFrameTime = GetCurrentImgFrameEndTime();

  // If we can get closer to the current time by a multiple of the image's loop
  // time, we should.
  uint32_t loopTime = GetSingleLoopTime();
  if (loopTime > 0) {
    TimeDuration delay = aTime - mCurrentAnimationFrameTime;
    if (delay.ToMilliseconds() > loopTime) {
      // Explicitly use integer division to get the floor of the number of
      // loops.
      uint32_t loops = static_cast<uint32_t>(delay.ToMilliseconds()) / loopTime;
      mCurrentAnimationFrameTime += TimeDuration::FromMilliseconds(loops * loopTime);
    }
  }

  // Set currentAnimationFrameIndex at the last possible moment
  mCurrentAnimationFrameIndex = nextFrameIndex;

  // If we're here, we successfully advanced the frame.
  ret.frameAdvanced = true;

  return ret;
}

FrameAnimator::RefreshResult
FrameAnimator::RequestRefresh(const TimeStamp& aTime)
{
  // only advance the frame if the current time is greater than or
  // equal to the current frame's end time.
  TimeStamp currentFrameEndTime = GetCurrentImgFrameEndTime();

  // By default, an empty RefreshResult.
  RefreshResult ret;

  while (currentFrameEndTime <= aTime) {
    TimeStamp oldFrameEndTime = currentFrameEndTime;

    RefreshResult frameRes = AdvanceFrame(aTime);

    // Accumulate our result for returning to callers.
    ret.Accumulate(frameRes);

    currentFrameEndTime = GetCurrentImgFrameEndTime();

    // if we didn't advance a frame, and our frame end time didn't change,
    // then we need to break out of this loop & wait for the frame(s)
    // to finish downloading
    if (!frameRes.frameAdvanced && (currentFrameEndTime == oldFrameEndTime)) {
      break;
    }
  }

  return ret;
}

void
FrameAnimator::ResetAnimation()
{
  mCurrentAnimationFrameIndex = 0;
}

void
FrameAnimator::SetDoneDecoding(bool aDone)
{
  mDoneDecoding = aDone;
}

void
FrameAnimator::SetAnimationMode(uint16_t aAnimationMode)
{
  mAnimationMode = aAnimationMode;
}

void
FrameAnimator::InitAnimationFrameTimeIfNecessary()
{
  if (mCurrentAnimationFrameTime.IsNull()) {
    mCurrentAnimationFrameTime = TimeStamp::Now();
  }
}

void
FrameAnimator::SetAnimationFrameTime(const TimeStamp& aTime)
{
  mCurrentAnimationFrameTime = aTime;
}

void
FrameAnimator::SetFirstFrameRefreshArea(const nsIntRect& aRect)
{
  mFirstFrameRefreshArea = aRect;
}

void
FrameAnimator::UnionFirstFrameRefreshArea(const nsIntRect& aRect)
{
  mFirstFrameRefreshArea.UnionRect(mFirstFrameRefreshArea, aRect);
}

uint32_t
FrameAnimator::GetCurrentAnimationFrameIndex() const
{
  return mCurrentAnimationFrameIndex;
}

nsIntRect
FrameAnimator::GetFirstFrameRefreshArea() const
{
  return mFirstFrameRefreshArea;
}

} // namespace image
} // namespace mozilla
