/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FrameAnimator.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"
#include "mozilla/CheckedInt.h"
#include "imgIContainer.h"
#include "LookupResult.h"
#include "MainThreadUtils.h"
#include "RasterImage.h"
#include "gfxPrefs.h"

#include "pixman.h"
#include <algorithm>

namespace mozilla {

using namespace gfx;

namespace image {

///////////////////////////////////////////////////////////////////////////////
// AnimationState implementation.
///////////////////////////////////////////////////////////////////////////////

const gfx::IntRect
AnimationState::UpdateState(bool aAnimationFinished,
                            RasterImage *aImage,
                            const gfx::IntSize& aSize,
                            bool aAllowInvalidation /* = true */)
{
  LookupResult result =
    SurfaceCache::Lookup(ImageKey(aImage),
                         RasterSurfaceKey(aSize,
                                          DefaultSurfaceFlags(),
                                          PlaybackType::eAnimated));

  return UpdateStateInternal(result, aAnimationFinished, aSize, aAllowInvalidation);
}

const gfx::IntRect
AnimationState::UpdateStateInternal(LookupResult& aResult,
                                    bool aAnimationFinished,
                                    const gfx::IntSize& aSize,
                                    bool aAllowInvalidation /* = true */)
{
  // Update mDiscarded and mIsCurrentlyDecoded.
  if (aResult.Type() == MatchType::NOT_FOUND) {
    // no frames, we've either been discarded, or never been decoded before.
    mDiscarded = mHasBeenDecoded;
    mIsCurrentlyDecoded = false;
  } else if (aResult.Type() == MatchType::PENDING) {
    // no frames yet, but a decoder is or will be working on it.
    mDiscarded = false;
    mIsCurrentlyDecoded = false;
  } else {
    MOZ_ASSERT(aResult.Type() == MatchType::EXACT);
    mDiscarded = false;

    // If mHasBeenDecoded is true then we know the true total frame count and
    // we can use it to determine if we have all the frames now so we know if
    // we are currently fully decoded.
    // If mHasBeenDecoded is false then we'll get another UpdateState call
    // when the decode finishes.
    if (mHasBeenDecoded) {
      Maybe<uint32_t> frameCount = FrameCount();
      MOZ_ASSERT(frameCount.isSome());
      if (NS_SUCCEEDED(aResult.Surface().Seek(*frameCount - 1)) &&
          aResult.Surface()->IsFinished()) {
        mIsCurrentlyDecoded = true;
      } else {
        mIsCurrentlyDecoded = false;
      }
    }
  }

  gfx::IntRect ret;

  if (aAllowInvalidation) {
    // Update the value of mCompositedFrameInvalid.
    if (mIsCurrentlyDecoded || aAnimationFinished) {
      // Animated images that have finished their animation (ie because it is a
      // finite length animation) don't have RequestRefresh called on them, and so
      // mCompositedFrameInvalid would never get cleared. We clear it here (and
      // also in RasterImage::Decode when we create a decoder for an image that
      // has finished animated so it can display sooner than waiting until the
      // decode completes). We also do it if we are fully decoded. This is safe
      // to do for images that aren't finished animating because before we paint
      // the refresh driver will call into us to advance to the correct frame,
      // and that will succeed because we have all the frames.
      if (mCompositedFrameInvalid) {
        // Invalidate if we are marking the composited frame valid.
        ret.SizeTo(aSize);
      }
      mCompositedFrameInvalid = false;
    } else if (aResult.Type() == MatchType::NOT_FOUND ||
               aResult.Type() == MatchType::PENDING) {
      if (mHasBeenDecoded) {
        MOZ_ASSERT(gfxPrefs::ImageMemAnimatedDiscardable());
        mCompositedFrameInvalid = true;
      }
    }
    // Otherwise don't change the value of mCompositedFrameInvalid, it will be
    // updated by RequestRefresh.
  }

  return ret;
}

void
AnimationState::NotifyDecodeComplete()
{
  mHasBeenDecoded = true;
}

void
AnimationState::ResetAnimation()
{
  mCurrentAnimationFrameIndex = 0;
}

void
AnimationState::SetAnimationMode(uint16_t aAnimationMode)
{
  mAnimationMode = aAnimationMode;
}

void
AnimationState::UpdateKnownFrameCount(uint32_t aFrameCount)
{
  if (aFrameCount <= mFrameCount) {
    // Nothing to do. Since we can redecode animated images, we may see the same
    // sequence of updates replayed again, so seeing a smaller frame count than
    // what we already know about doesn't indicate an error.
    return;
  }

  MOZ_ASSERT(!mHasBeenDecoded, "Adding new frames after decoding is finished?");
  MOZ_ASSERT(aFrameCount <= mFrameCount + 1, "Skipped a frame?");

  mFrameCount = aFrameCount;
}

Maybe<uint32_t>
AnimationState::FrameCount() const
{
  return mHasBeenDecoded ? Some(mFrameCount) : Nothing();
}

void
AnimationState::SetFirstFrameRefreshArea(const IntRect& aRefreshArea)
{
  mFirstFrameRefreshArea = aRefreshArea;
}

void
AnimationState::InitAnimationFrameTimeIfNecessary()
{
  if (mCurrentAnimationFrameTime.IsNull()) {
    mCurrentAnimationFrameTime = TimeStamp::Now();
  }
}

void
AnimationState::SetAnimationFrameTime(const TimeStamp& aTime)
{
  mCurrentAnimationFrameTime = aTime;
}

uint32_t
AnimationState::GetCurrentAnimationFrameIndex() const
{
  return mCurrentAnimationFrameIndex;
}

FrameTimeout
AnimationState::LoopLength() const
{
  // If we don't know the loop length yet, we have to treat it as infinite.
  if (!mLoopLength) {
    return FrameTimeout::Forever();
  }

  MOZ_ASSERT(mHasBeenDecoded, "We know the loop length but decoding isn't done?");

  // If we're not looping, a single loop time has no meaning.
  if (mAnimationMode != imgIContainer::kNormalAnimMode) {
    return FrameTimeout::Forever();
  }

  return *mLoopLength;
}


///////////////////////////////////////////////////////////////////////////////
// FrameAnimator implementation.
///////////////////////////////////////////////////////////////////////////////

Maybe<TimeStamp>
FrameAnimator::GetCurrentImgFrameEndTime(AnimationState& aState,
                                         DrawableSurface& aFrames) const
{
  TimeStamp currentFrameTime = aState.mCurrentAnimationFrameTime;
  Maybe<FrameTimeout> timeout =
    GetTimeoutForFrame(aState, aFrames, aState.mCurrentAnimationFrameIndex);

  if (timeout.isNothing()) {
    MOZ_ASSERT(aState.GetHasBeenDecoded() && !aState.GetIsCurrentlyDecoded());
    return Nothing();
  }

  if (*timeout == FrameTimeout::Forever()) {
    // We need to return a sentinel value in this case, because our logic
    // doesn't work correctly if we have an infinitely long timeout. We use one
    // year in the future as the sentinel because it works with the loop in
    // RequestRefresh() below.
    // XXX(seth): It'd be preferable to make our logic work correctly with
    // infinitely long timeouts.
    return Some(TimeStamp::NowLoRes() +
                TimeDuration::FromMilliseconds(31536000.0));
  }

  TimeDuration durationOfTimeout =
    TimeDuration::FromMilliseconds(double(timeout->AsMilliseconds()));
  TimeStamp currentFrameEndTime = currentFrameTime + durationOfTimeout;

  return Some(currentFrameEndTime);
}

RefreshResult
FrameAnimator::AdvanceFrame(AnimationState& aState,
                            DrawableSurface& aFrames,
                            TimeStamp aTime)
{
  NS_ASSERTION(aTime <= TimeStamp::Now(),
               "Given time appears to be in the future");
  PROFILER_LABEL_FUNC(js::ProfileEntry::Category::GRAPHICS);

  RefreshResult ret;

  // Determine what the next frame is, taking into account looping.
  uint32_t currentFrameIndex = aState.mCurrentAnimationFrameIndex;
  uint32_t nextFrameIndex = currentFrameIndex + 1;

  // Check if we're at the end of the loop. (FrameCount() returns Nothing() if
  // we don't know the total count yet.)
  if (aState.FrameCount() == Some(nextFrameIndex)) {
    // If we are not looping forever, initialize the loop counter
    if (aState.mLoopRemainingCount < 0 && aState.LoopCount() >= 0) {
      aState.mLoopRemainingCount = aState.LoopCount();
    }

    // If animation mode is "loop once", or we're at end of loop counter,
    // it's time to stop animating.
    if (aState.mAnimationMode == imgIContainer::kLoopOnceAnimMode ||
        aState.mLoopRemainingCount == 0) {
      ret.mAnimationFinished = true;
    }

    nextFrameIndex = 0;

    if (aState.mLoopRemainingCount > 0) {
      aState.mLoopRemainingCount--;
    }

    // If we're done, exit early.
    if (ret.mAnimationFinished) {
      return ret;
    }
  }

  if (nextFrameIndex >= aState.KnownFrameCount()) {
    // We've already advanced to the last decoded frame, nothing more we can do.
    // We're blocked by network/decoding from displaying the animation at the
    // rate specified, so that means the frame we are displaying (the latest
    // available) is the frame we want to be displaying at this time. So we
    // update the current animation time. If we didn't update the current
    // animation time then it could lag behind, which would indicate that we are
    // behind in the animation and should try to catch up. When we are done
    // decoding (and thus can loop around back to the start of the animation) we
    // would then jump to a random point in the animation to try to catch up.
    // But we were never behind in the animation.
    aState.mCurrentAnimationFrameTime = aTime;
    return ret;
  }

  // There can be frames in the surface cache with index >= KnownFrameCount()
  // which GetRawFrame() can access because an async decoder has decoded them,
  // but which AnimationState doesn't know about yet because we haven't received
  // the appropriate notification on the main thread. Make sure we stay in sync
  // with AnimationState.
  MOZ_ASSERT(nextFrameIndex < aState.KnownFrameCount());
  RawAccessFrameRef nextFrame = GetRawFrame(aFrames, nextFrameIndex);

  // We should always check to see if we have the next frame even if we have
  // previously finished decoding. If we needed to redecode (e.g. due to a draw
  // failure) we would have discarded all the old frames and may not yet have
  // the new ones.
  if (!nextFrame || !nextFrame->IsFinished()) {
    // Uh oh, the frame we want to show is currently being decoded (partial)
    // Wait until the next refresh driver tick and try again
    return ret;
  }

  Maybe<FrameTimeout> nextFrameTimeout = GetTimeoutForFrame(aState, aFrames, nextFrameIndex);
  // GetTimeoutForFrame can only return none if frame doesn't exist,
  // but we just got it above.
  MOZ_ASSERT(nextFrameTimeout.isSome());
  if (*nextFrameTimeout == FrameTimeout::Forever()) {
    ret.mAnimationFinished = true;
  }

  if (nextFrameIndex == 0) {
    ret.mDirtyRect = aState.FirstFrameRefreshArea();
  } else {
    MOZ_ASSERT(nextFrameIndex == currentFrameIndex + 1);

    // Change frame
    if (!DoBlend(aFrames, &ret.mDirtyRect, currentFrameIndex, nextFrameIndex)) {
      // something went wrong, move on to next
      NS_WARNING("FrameAnimator::AdvanceFrame(): Compositing of frame failed");
      nextFrame->SetCompositingFailed(true);
      Maybe<TimeStamp> currentFrameEndTime = GetCurrentImgFrameEndTime(aState, aFrames);
      MOZ_ASSERT(currentFrameEndTime.isSome());
      aState.mCurrentAnimationFrameTime = *currentFrameEndTime;
      aState.mCurrentAnimationFrameIndex = nextFrameIndex;

      return ret;
    }

    nextFrame->SetCompositingFailed(false);
  }

  Maybe<TimeStamp> currentFrameEndTime = GetCurrentImgFrameEndTime(aState, aFrames);
  MOZ_ASSERT(currentFrameEndTime.isSome());
  aState.mCurrentAnimationFrameTime = *currentFrameEndTime;

  // If we can get closer to the current time by a multiple of the image's loop
  // time, we should. We can only do this if we're done decoding; otherwise, we
  // don't know the full loop length, and LoopLength() will have to return
  // FrameTimeout::Forever(). We also skip this for images with a finite loop
  // count if we have initialized mLoopRemainingCount (it only gets initialized
  // after one full loop).
  FrameTimeout loopTime = aState.LoopLength();
  if (loopTime != FrameTimeout::Forever() &&
      (aState.LoopCount() < 0 || aState.mLoopRemainingCount >= 0)) {
    TimeDuration delay = aTime - aState.mCurrentAnimationFrameTime;
    if (delay.ToMilliseconds() > loopTime.AsMilliseconds()) {
      // Explicitly use integer division to get the floor of the number of
      // loops.
      uint64_t loops = static_cast<uint64_t>(delay.ToMilliseconds())
                     / loopTime.AsMilliseconds();

      // If we have a finite loop count limit the number of loops we advance.
      if (aState.mLoopRemainingCount >= 0) {
        MOZ_ASSERT(aState.LoopCount() >= 0);
        loops = std::min(loops, CheckedUint64(aState.mLoopRemainingCount).value());
      }

      aState.mCurrentAnimationFrameTime +=
        TimeDuration::FromMilliseconds(loops * loopTime.AsMilliseconds());

      if (aState.mLoopRemainingCount >= 0) {
        MOZ_ASSERT(loops <= CheckedUint64(aState.mLoopRemainingCount).value());
        aState.mLoopRemainingCount -= CheckedInt32(loops).value();
      }
    }
  }

  // Set currentAnimationFrameIndex at the last possible moment
  aState.mCurrentAnimationFrameIndex = nextFrameIndex;

  // If we're here, we successfully advanced the frame.
  ret.mFrameAdvanced = true;

  return ret;
}

RefreshResult
FrameAnimator::RequestRefresh(AnimationState& aState,
                              const TimeStamp& aTime,
                              bool aAnimationFinished)
{
  // By default, an empty RefreshResult.
  RefreshResult ret;

  if (aState.IsDiscarded()) {
    return ret;
  }

  // Get the animation frames once now, and pass them down to callees because
  // the surface could be discarded at anytime on a different thread. This is
  // must easier to reason about then trying to write code that is safe to
  // having the surface disappear at anytime.
  LookupResult result =
    SurfaceCache::Lookup(ImageKey(mImage),
                         RasterSurfaceKey(mSize,
                                          DefaultSurfaceFlags(),
                                          PlaybackType::eAnimated));

  ret.mDirtyRect = aState.UpdateStateInternal(result, aAnimationFinished, mSize);
  if (aState.IsDiscarded() || !result) {
    if (!ret.mDirtyRect.IsEmpty()) {
      ret.mFrameAdvanced = true;
    }
    return ret;
  }

  // only advance the frame if the current time is greater than or
  // equal to the current frame's end time.
  Maybe<TimeStamp> currentFrameEndTime =
    GetCurrentImgFrameEndTime(aState, result.Surface());
  if (currentFrameEndTime.isNothing()) {
    MOZ_ASSERT(gfxPrefs::ImageMemAnimatedDiscardable());
    MOZ_ASSERT(aState.GetHasBeenDecoded() && !aState.GetIsCurrentlyDecoded());
    MOZ_ASSERT(aState.mCompositedFrameInvalid);
    // Nothing we can do but wait for our previous current frame to be decoded
    // again so we can determine what to do next.
    return ret;
  }

  while (*currentFrameEndTime <= aTime) {
    TimeStamp oldFrameEndTime = *currentFrameEndTime;

    RefreshResult frameRes = AdvanceFrame(aState, result.Surface(), aTime);

    // Accumulate our result for returning to callers.
    ret.Accumulate(frameRes);

    currentFrameEndTime = GetCurrentImgFrameEndTime(aState, result.Surface());
    // AdvanceFrame can't advance to a frame that doesn't exist yet.
    MOZ_ASSERT(currentFrameEndTime.isSome());

    // If we didn't advance a frame, and our frame end time didn't change,
    // then we need to break out of this loop & wait for the frame(s)
    // to finish downloading.
    if (!frameRes.mFrameAdvanced && (*currentFrameEndTime == oldFrameEndTime)) {
      break;
    }
  }

  // Advanced to the correct frame, the composited frame is now valid to be drawn.
  if (*currentFrameEndTime > aTime) {
    aState.mCompositedFrameInvalid = false;
    ret.mDirtyRect = IntRect(IntPoint(0,0), mSize);
  }

  MOZ_ASSERT(!aState.mIsCurrentlyDecoded || !aState.mCompositedFrameInvalid);

  return ret;
}

LookupResult
FrameAnimator::GetCompositedFrame(AnimationState& aState)
{
  LookupResult result =
    SurfaceCache::Lookup(ImageKey(mImage),
                         RasterSurfaceKey(mSize,
                                          DefaultSurfaceFlags(),
                                          PlaybackType::eAnimated));

  if (aState.mCompositedFrameInvalid) {
    MOZ_ASSERT(gfxPrefs::ImageMemAnimatedDiscardable());
    MOZ_ASSERT(aState.GetHasBeenDecoded());
    MOZ_ASSERT(!aState.GetIsCurrentlyDecoded());
    if (result.Type() == MatchType::NOT_FOUND) {
      return result;
    }
    return LookupResult(MatchType::PENDING);
  }

  // If we have a composited version of this frame, return that.
  if (mLastCompositedFrameIndex >= 0 &&
      (uint32_t(mLastCompositedFrameIndex) == aState.mCurrentAnimationFrameIndex)) {
    return LookupResult(DrawableSurface(mCompositingFrame->DrawableRef()),
                        MatchType::EXACT);
  }

  // Otherwise return the raw frame. DoBlend is required to ensure that we only
  // hit this case if the frame is not paletted and doesn't require compositing.
  if (!result) {
    return result;
  }

  // Seek to the appropriate frame. If seeking fails, it means that we couldn't
  // get the frame we're looking for; treat this as if the lookup failed.
  if (NS_FAILED(result.Surface().Seek(aState.mCurrentAnimationFrameIndex))) {
    if (result.Type() == MatchType::NOT_FOUND) {
      return result;
    }
    return LookupResult(MatchType::PENDING);
  }

  MOZ_ASSERT(!result.Surface()->GetIsPaletted(),
             "About to return a paletted frame");

  return result;
}

Maybe<FrameTimeout>
FrameAnimator::GetTimeoutForFrame(AnimationState& aState,
                                  DrawableSurface& aFrames,
                                  uint32_t aFrameNum) const
{
  RawAccessFrameRef frame = GetRawFrame(aFrames, aFrameNum);
  if (frame) {
    AnimationData data = frame->GetAnimationData();
    return Some(data.mTimeout);
  }

  MOZ_ASSERT(aState.mHasBeenDecoded && !aState.mIsCurrentlyDecoded);
  return Nothing();
}

static void
DoCollectSizeOfCompositingSurfaces(const RawAccessFrameRef& aSurface,
                                   SurfaceMemoryCounterType aType,
                                   nsTArray<SurfaceMemoryCounter>& aCounters,
                                   MallocSizeOf aMallocSizeOf)
{
  // Concoct a SurfaceKey for this surface.
  SurfaceKey key = RasterSurfaceKey(aSurface->GetImageSize(),
                                    DefaultSurfaceFlags(),
                                    PlaybackType::eStatic);

  // Create a counter for this surface.
  SurfaceMemoryCounter counter(key, /* aIsLocked = */ true, aType);

  // Extract the surface's memory usage information.
  size_t heap = 0, nonHeap = 0, handles = 0;
  aSurface->AddSizeOfExcludingThis(aMallocSizeOf, heap, nonHeap, handles);
  counter.Values().SetDecodedHeap(heap);
  counter.Values().SetDecodedNonHeap(nonHeap);
  counter.Values().SetSharedHandles(handles);

  // Record it.
  aCounters.AppendElement(counter);
}

void
FrameAnimator::CollectSizeOfCompositingSurfaces(
    nsTArray<SurfaceMemoryCounter>& aCounters,
    MallocSizeOf aMallocSizeOf) const
{
  if (mCompositingFrame) {
    DoCollectSizeOfCompositingSurfaces(mCompositingFrame,
                                       SurfaceMemoryCounterType::COMPOSITING,
                                       aCounters,
                                       aMallocSizeOf);
  }

  if (mCompositingPrevFrame) {
    DoCollectSizeOfCompositingSurfaces(mCompositingPrevFrame,
                                       SurfaceMemoryCounterType::COMPOSITING_PREV,
                                       aCounters,
                                       aMallocSizeOf);
  }
}

RawAccessFrameRef
FrameAnimator::GetRawFrame(DrawableSurface& aFrames, uint32_t aFrameNum) const
{
  // Seek to the frame we want. If seeking fails, it means we couldn't get the
  // frame we're looking for, so we bail here to avoid returning the wrong frame
  // to the caller.
  if (NS_FAILED(aFrames.Seek(aFrameNum))) {
    return RawAccessFrameRef();  // Not available yet.
  }

  return aFrames->RawAccessRef();
}

//******************************************************************************
// DoBlend gets called when the timer for animation get fired and we have to
// update the composited frame of the animation.
bool
FrameAnimator::DoBlend(DrawableSurface& aFrames,
                       IntRect* aDirtyRect,
                       uint32_t aPrevFrameIndex,
                       uint32_t aNextFrameIndex)
{
  RawAccessFrameRef prevFrame = GetRawFrame(aFrames, aPrevFrameIndex);
  RawAccessFrameRef nextFrame = GetRawFrame(aFrames, aNextFrameIndex);

  MOZ_ASSERT(prevFrame && nextFrame, "Should have frames here");

  AnimationData prevFrameData = prevFrame->GetAnimationData();
  if (prevFrameData.mDisposalMethod == DisposalMethod::RESTORE_PREVIOUS &&
      !mCompositingPrevFrame) {
    prevFrameData.mDisposalMethod = DisposalMethod::CLEAR;
  }

  IntRect prevRect = prevFrameData.mBlendRect
                   ? prevFrameData.mRect.Intersect(*prevFrameData.mBlendRect)
                   : prevFrameData.mRect;

  bool isFullPrevFrame = prevRect.x == 0 && prevRect.y == 0 &&
                         prevRect.width == mSize.width &&
                         prevRect.height == mSize.height;

  // Optimization: DisposeClearAll if the previous frame is the same size as
  //               container and it's clearing itself
  if (isFullPrevFrame &&
      (prevFrameData.mDisposalMethod == DisposalMethod::CLEAR)) {
    prevFrameData.mDisposalMethod = DisposalMethod::CLEAR_ALL;
  }

  AnimationData nextFrameData = nextFrame->GetAnimationData();

  IntRect nextRect = nextFrameData.mBlendRect
                   ? nextFrameData.mRect.Intersect(*nextFrameData.mBlendRect)
                   : nextFrameData.mRect;

  bool isFullNextFrame = nextRect.x == 0 && nextRect.y == 0 &&
                         nextRect.width == mSize.width &&
                         nextRect.height == mSize.height;

  if (!nextFrame->GetIsPaletted()) {
    // Optimization: Skip compositing if the previous frame wants to clear the
    //               whole image
    if (prevFrameData.mDisposalMethod == DisposalMethod::CLEAR_ALL) {
      aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
      return true;
    }

    // Optimization: Skip compositing if this frame is the same size as the
    //               container and it's fully drawing over prev frame (no alpha)
    if (isFullNextFrame &&
        (nextFrameData.mDisposalMethod != DisposalMethod::RESTORE_PREVIOUS) &&
        !nextFrameData.mHasAlpha) {
      aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
      return true;
    }
  }

  // Calculate area that needs updating
  switch (prevFrameData.mDisposalMethod) {
    default:
      MOZ_FALLTHROUGH_ASSERT("Unexpected DisposalMethod");
    case DisposalMethod::NOT_SPECIFIED:
    case DisposalMethod::KEEP:
      *aDirtyRect = nextRect;
      break;

    case DisposalMethod::CLEAR_ALL:
      // Whole image container is cleared
      aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
      break;

    case DisposalMethod::CLEAR:
      // Calc area that needs to be redrawn (the combination of previous and
      // this frame)
      // XXX - This could be done with multiple framechanged calls
      //       Having prevFrame way at the top of the image, and nextFrame
      //       way at the bottom, and both frames being small, we'd be
      //       telling framechanged to refresh the whole image when only two
      //       small areas are needed.
      aDirtyRect->UnionRect(nextRect, prevRect);
      break;

    case DisposalMethod::RESTORE_PREVIOUS:
      aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
      break;
  }

  // Optimization:
  //   Skip compositing if the last composited frame is this frame
  //   (Only one composited frame was made for this animation.  Example:
  //    Only Frame 3 of a 10 frame image required us to build a composite frame
  //    On the second loop, we do not need to rebuild the frame
  //    since it's still sitting in compositingFrame)
  if (mLastCompositedFrameIndex == int32_t(aNextFrameIndex)) {
    return true;
  }

  bool needToBlankComposite = false;

  // Create the Compositing Frame
  if (!mCompositingFrame) {
    RefPtr<imgFrame> newFrame = new imgFrame;
    nsresult rv = newFrame->InitForAnimator(mSize,
                                            SurfaceFormat::B8G8R8A8);
    if (NS_FAILED(rv)) {
      mCompositingFrame.reset();
      return false;
    }
    mCompositingFrame = newFrame->RawAccessRef();
    needToBlankComposite = true;
  } else if (int32_t(aNextFrameIndex) != mLastCompositedFrameIndex+1) {

    // If we are not drawing on top of last composited frame,
    // then we are building a new composite frame, so let's clear it first.
    needToBlankComposite = true;
  }

  AnimationData compositingFrameData = mCompositingFrame->GetAnimationData();

  // More optimizations possible when next frame is not transparent
  // But if the next frame has DisposalMethod::RESTORE_PREVIOUS,
  // this "no disposal" optimization is not possible,
  // because the frame in "after disposal operation" state
  // needs to be stored in compositingFrame, so it can be
  // copied into compositingPrevFrame later.
  bool doDisposal = true;
  if (!nextFrameData.mHasAlpha &&
      nextFrameData.mDisposalMethod != DisposalMethod::RESTORE_PREVIOUS) {
    if (isFullNextFrame) {
      // Optimization: No need to dispose prev.frame when
      // next frame is full frame and not transparent.
      doDisposal = false;
      // No need to blank the composite frame
      needToBlankComposite = false;
    } else {
      if ((prevRect.x >= nextRect.x) && (prevRect.y >= nextRect.y) &&
          (prevRect.x + prevRect.width <= nextRect.x + nextRect.width) &&
          (prevRect.y + prevRect.height <= nextRect.y + nextRect.height)) {
        // Optimization: No need to dispose prev.frame when
        // next frame fully overlaps previous frame.
        doDisposal = false;
      }
    }
  }

  if (doDisposal) {
    // Dispose of previous: clear, restore, or keep (copy)
    switch (prevFrameData.mDisposalMethod) {
      case DisposalMethod::CLEAR:
        if (needToBlankComposite) {
          // If we just created the composite, it could have anything in its
          // buffer. Clear whole frame
          ClearFrame(compositingFrameData.mRawData,
                     compositingFrameData.mRect);
        } else {
          // Only blank out previous frame area (both color & Mask/Alpha)
          ClearFrame(compositingFrameData.mRawData,
                     compositingFrameData.mRect,
                     prevRect);
        }
        break;

      case DisposalMethod::CLEAR_ALL:
        ClearFrame(compositingFrameData.mRawData,
                   compositingFrameData.mRect);
        break;

      case DisposalMethod::RESTORE_PREVIOUS:
        // It would be better to copy only the area changed back to
        // compositingFrame.
        if (mCompositingPrevFrame) {
          AnimationData compositingPrevFrameData =
            mCompositingPrevFrame->GetAnimationData();

          CopyFrameImage(compositingPrevFrameData.mRawData,
                         compositingPrevFrameData.mRect,
                         compositingFrameData.mRawData,
                         compositingFrameData.mRect);

          // destroy only if we don't need it for this frame's disposal
          if (nextFrameData.mDisposalMethod !=
              DisposalMethod::RESTORE_PREVIOUS) {
            mCompositingPrevFrame.reset();
          }
        } else {
          ClearFrame(compositingFrameData.mRawData,
                     compositingFrameData.mRect);
        }
        break;

      default:
        MOZ_FALLTHROUGH_ASSERT("Unexpected DisposalMethod");
      case DisposalMethod::NOT_SPECIFIED:
      case DisposalMethod::KEEP:
        // Copy previous frame into compositingFrame before we put the new
        // frame on top
        // Assumes that the previous frame represents a full frame (it could be
        // smaller in size than the container, as long as the frame before it
        // erased itself)
        // Note: Frame 1 never gets into DoBlend(), so (aNextFrameIndex - 1)
        // will always be a valid frame number.
        if (mLastCompositedFrameIndex != int32_t(aNextFrameIndex - 1)) {
          if (isFullPrevFrame && !prevFrame->GetIsPaletted()) {
            // Just copy the bits
            CopyFrameImage(prevFrameData.mRawData,
                           prevRect,
                           compositingFrameData.mRawData,
                           compositingFrameData.mRect);
          } else {
            if (needToBlankComposite) {
              // Only blank composite when prev is transparent or not full.
              if (prevFrameData.mHasAlpha || !isFullPrevFrame) {
                ClearFrame(compositingFrameData.mRawData,
                           compositingFrameData.mRect);
              }
            }
            DrawFrameTo(prevFrameData.mRawData, prevFrameData.mRect,
                        prevFrameData.mPaletteDataLength,
                        prevFrameData.mHasAlpha,
                        compositingFrameData.mRawData,
                        compositingFrameData.mRect,
                        prevFrameData.mBlendMethod,
                        prevFrameData.mBlendRect);
          }
        }
    }
  } else if (needToBlankComposite) {
    // If we just created the composite, it could have anything in its
    // buffers. Clear them
    ClearFrame(compositingFrameData.mRawData,
               compositingFrameData.mRect);
  }

  // Check if the frame we are composing wants the previous image restored after
  // it is done. Don't store it (again) if last frame wanted its image restored
  // too
  if ((nextFrameData.mDisposalMethod == DisposalMethod::RESTORE_PREVIOUS) &&
      (prevFrameData.mDisposalMethod != DisposalMethod::RESTORE_PREVIOUS)) {
    // We are storing the whole image.
    // It would be better if we just stored the area that nextFrame is going to
    // overwrite.
    if (!mCompositingPrevFrame) {
      RefPtr<imgFrame> newFrame = new imgFrame;
      nsresult rv = newFrame->InitForAnimator(mSize,
                                              SurfaceFormat::B8G8R8A8);
      if (NS_FAILED(rv)) {
        mCompositingPrevFrame.reset();
        return false;
      }

      mCompositingPrevFrame = newFrame->RawAccessRef();
    }

    AnimationData compositingPrevFrameData =
      mCompositingPrevFrame->GetAnimationData();

    CopyFrameImage(compositingFrameData.mRawData,
                   compositingFrameData.mRect,
                   compositingPrevFrameData.mRawData,
                   compositingPrevFrameData.mRect);

    mCompositingPrevFrame->Finish();
  }

  // blit next frame into it's correct spot
  DrawFrameTo(nextFrameData.mRawData, nextFrameData.mRect,
              nextFrameData.mPaletteDataLength,
              nextFrameData.mHasAlpha,
              compositingFrameData.mRawData,
              compositingFrameData.mRect,
              nextFrameData.mBlendMethod,
              nextFrameData.mBlendRect);

  // Tell the image that it is fully 'downloaded'.
  mCompositingFrame->Finish();

  mLastCompositedFrameIndex = int32_t(aNextFrameIndex);

  return true;
}

//******************************************************************************
// Fill aFrame with black. Does also clears the mask.
void
FrameAnimator::ClearFrame(uint8_t* aFrameData, const IntRect& aFrameRect)
{
  if (!aFrameData) {
    return;
  }

  memset(aFrameData, 0, aFrameRect.width * aFrameRect.height * 4);
}

//******************************************************************************
void
FrameAnimator::ClearFrame(uint8_t* aFrameData, const IntRect& aFrameRect,
                          const IntRect& aRectToClear)
{
  if (!aFrameData || aFrameRect.width <= 0 || aFrameRect.height <= 0 ||
      aRectToClear.width <= 0 || aRectToClear.height <= 0) {
    return;
  }

  IntRect toClear = aFrameRect.Intersect(aRectToClear);
  if (toClear.IsEmpty()) {
    return;
  }

  uint32_t bytesPerRow = aFrameRect.width * 4;
  for (int row = toClear.y; row < toClear.y + toClear.height; ++row) {
    memset(aFrameData + toClear.x * 4 + row * bytesPerRow, 0,
           toClear.width * 4);
  }
}

//******************************************************************************
// Whether we succeed or fail will not cause a crash, and there's not much
// we can do about a failure, so there we don't return a nsresult
bool
FrameAnimator::CopyFrameImage(const uint8_t* aDataSrc,
                              const IntRect& aRectSrc,
                              uint8_t* aDataDest,
                              const IntRect& aRectDest)
{
  uint32_t dataLengthSrc = aRectSrc.width * aRectSrc.height * 4;
  uint32_t dataLengthDest = aRectDest.width * aRectDest.height * 4;

  if (!aDataDest || !aDataSrc || dataLengthSrc != dataLengthDest) {
    return false;
  }

  memcpy(aDataDest, aDataSrc, dataLengthDest);

  return true;
}

nsresult
FrameAnimator::DrawFrameTo(const uint8_t* aSrcData, const IntRect& aSrcRect,
                           uint32_t aSrcPaletteLength, bool aSrcHasAlpha,
                           uint8_t* aDstPixels, const IntRect& aDstRect,
                           BlendMethod aBlendMethod, const Maybe<IntRect>& aBlendRect)
{
  NS_ENSURE_ARG_POINTER(aSrcData);
  NS_ENSURE_ARG_POINTER(aDstPixels);

  // According to both AGIF and APNG specs, offsets are unsigned
  if (aSrcRect.x < 0 || aSrcRect.y < 0) {
    NS_WARNING("FrameAnimator::DrawFrameTo: negative offsets not allowed");
    return NS_ERROR_FAILURE;
  }
  // Outside the destination frame, skip it
  if ((aSrcRect.x > aDstRect.width) || (aSrcRect.y > aDstRect.height)) {
    return NS_OK;
  }

  if (aSrcPaletteLength) {
    // Larger than the destination frame, clip it
    int32_t width = std::min(aSrcRect.width, aDstRect.width - aSrcRect.x);
    int32_t height = std::min(aSrcRect.height, aDstRect.height - aSrcRect.y);

    // The clipped image must now fully fit within destination image frame
    NS_ASSERTION((aSrcRect.x >= 0) && (aSrcRect.y >= 0) &&
                 (aSrcRect.x + width <= aDstRect.width) &&
                 (aSrcRect.y + height <= aDstRect.height),
                "FrameAnimator::DrawFrameTo: Invalid aSrcRect");

    // clipped image size may be smaller than source, but not larger
    NS_ASSERTION((width <= aSrcRect.width) && (height <= aSrcRect.height),
      "FrameAnimator::DrawFrameTo: source must be smaller than dest");

    // Get pointers to image data
    const uint8_t* srcPixels = aSrcData + aSrcPaletteLength;
    uint32_t* dstPixels = reinterpret_cast<uint32_t*>(aDstPixels);
    const uint32_t* colormap = reinterpret_cast<const uint32_t*>(aSrcData);

    // Skip to the right offset
    dstPixels += aSrcRect.x + (aSrcRect.y * aDstRect.width);
    if (!aSrcHasAlpha) {
      for (int32_t r = height; r > 0; --r) {
        for (int32_t c = 0; c < width; c++) {
          dstPixels[c] = colormap[srcPixels[c]];
        }
        // Go to the next row in the source resp. destination image
        srcPixels += aSrcRect.width;
        dstPixels += aDstRect.width;
      }
    } else {
      for (int32_t r = height; r > 0; --r) {
        for (int32_t c = 0; c < width; c++) {
          const uint32_t color = colormap[srcPixels[c]];
          if (color) {
            dstPixels[c] = color;
          }
        }
        // Go to the next row in the source resp. destination image
        srcPixels += aSrcRect.width;
        dstPixels += aDstRect.width;
      }
    }
  } else {
    pixman_image_t* src =
      pixman_image_create_bits(
          aSrcHasAlpha ? PIXMAN_a8r8g8b8 : PIXMAN_x8r8g8b8,
          aSrcRect.width, aSrcRect.height,
          reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(aSrcData)),
          aSrcRect.width * 4);
    if (!src) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    pixman_image_t* dst =
      pixman_image_create_bits(PIXMAN_a8r8g8b8,
                               aDstRect.width,
                               aDstRect.height,
                               reinterpret_cast<uint32_t*>(aDstPixels),
                               aDstRect.width * 4);
    if (!dst) {
      pixman_image_unref(src);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // XXX(seth): This is inefficient but we'll remove it quite soon when we
    // move frame compositing into SurfacePipe. For now we need this because
    // RemoveFrameRectFilter has transformed PNG frames with frame rects into
    // imgFrame's with no frame rects, but with a region of 0 alpha where the
    // frame rect should be. This works really nicely if we're using
    // BlendMethod::OVER, but BlendMethod::SOURCE will result in that frame rect
    // area overwriting the previous frame, which makes the animation look
    // wrong. This quick hack fixes that by first compositing the whle new frame
    // with BlendMethod::OVER, and then recopying the area that uses
    // BlendMethod::SOURCE if needed. To make this work, the decoder has to
    // provide a "blend rect" that tells us where to do this. This is just the
    // frame rect, but hidden in a way that makes it invisible to most of the
    // system, so we can keep eliminating dependencies on it.
    auto op = aBlendMethod == BlendMethod::SOURCE ? PIXMAN_OP_SRC
                                                  : PIXMAN_OP_OVER;

    if (aBlendMethod == BlendMethod::OVER || !aBlendRect ||
        (aBlendMethod == BlendMethod::SOURCE && aSrcRect.IsEqualEdges(*aBlendRect))) {
      // We don't need to do anything clever. (Or, in the case where no blend
      // rect was specified, we can't.)
      pixman_image_composite32(op,
                               src,
                               nullptr,
                               dst,
                               0, 0,
                               0, 0,
                               aSrcRect.x, aSrcRect.y,
                               aSrcRect.width, aSrcRect.height);
    } else {
      // We need to do the OVER followed by SOURCE trick above.
      pixman_image_composite32(PIXMAN_OP_OVER,
                               src,
                               nullptr,
                               dst,
                               0, 0,
                               0, 0,
                               aSrcRect.x, aSrcRect.y,
                               aSrcRect.width, aSrcRect.height);
      pixman_image_composite32(PIXMAN_OP_SRC,
                               src,
                               nullptr,
                               dst,
                               aBlendRect->x, aBlendRect->y,
                               0, 0,
                               aBlendRect->x, aBlendRect->y,
                               aBlendRect->width, aBlendRect->height);
    }

    pixman_image_unref(src);
    pixman_image_unref(dst);
  }

  return NS_OK;
}

} // namespace image
} // namespace mozilla
