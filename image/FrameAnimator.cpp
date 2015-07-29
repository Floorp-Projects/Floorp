/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FrameAnimator.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"
#include "imgIContainer.h"
#include "LookupResult.h"
#include "MainThreadUtils.h"
#include "RasterImage.h"

#include "pixman.h"

namespace mozilla {

using namespace gfx;

namespace image {

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
  for (uint32_t i = 0; i < mImage->GetNumFrames(); ++i) {
    int32_t timeout = GetTimeoutForFrame(i);
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
  int32_t timeout =
    GetTimeoutForFrame(mCurrentAnimationFrameIndex);

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
  RawAccessFrameRef nextFrame = GetRawFrame(nextFrameIndex);

  // If we're done decoding, we know we've got everything we're going to get.
  // If we aren't, we only display fully-downloaded frames; everything else
  // gets delayed.
  bool canDisplay = mDoneDecoding ||
                    (nextFrame && nextFrame->IsImageComplete());

  if (!canDisplay) {
    // Uh oh, the frame we want to show is currently being decoded (partial)
    // Wait until the next refresh driver tick and try again
    return ret;
  }

  // If we're done decoding the next frame, go ahead and display it now and
  // reinit with the next frame's delay time.
  if (mImage->GetNumFrames() == nextFrameIndex) {
    // End of an animation loop...

    // If we are not looping forever, initialize the loop counter
    if (mLoopRemainingCount < 0 && LoopCount() >= 0) {
      mLoopRemainingCount = LoopCount();
    }

    // If animation mode is "loop once", or we're at end of loop counter,
    // it's time to stop animating.
    if (mAnimationMode == imgIContainer::kLoopOnceAnimMode ||
        mLoopRemainingCount == 0) {
      ret.animationFinished = true;
    }

    nextFrameIndex = 0;

    if (mLoopRemainingCount > 0) {
      mLoopRemainingCount--;
    }

    // If we're done, exit early.
    if (ret.animationFinished) {
      return ret;
    }
  }

  timeout = GetTimeoutForFrame(nextFrameIndex);

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
      nextFrame = GetRawFrame(nextFrameIndex);
    }

    if (!DoBlend(&ret.dirtyRect, currentFrameIndex,
                               nextFrameIndex)) {
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
      mCurrentAnimationFrameTime +=
        TimeDuration::FromMilliseconds(loops * loopTime);
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
  mLastCompositedFrameIndex = -1;
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

LookupResult
FrameAnimator::GetCompositedFrame(uint32_t aFrameNum)
{
  MOZ_ASSERT(aFrameNum != 0, "First frame is never composited");

  // If we have a composited version of this frame, return that.
  if (mLastCompositedFrameIndex == int32_t(aFrameNum)) {
    return LookupResult(mCompositingFrame->DrawableRef(), MatchType::EXACT);
  }

  // Otherwise return the raw frame. DoBlend is required to ensure that we only
  // hit this case if the frame is not paletted and doesn't require compositing.
  LookupResult result =
    SurfaceCache::Lookup(ImageKey(mImage),
                         RasterSurfaceKey(mSize,
                                          0,  // Default decode flags.
                                          aFrameNum));
  MOZ_ASSERT(!result || !result.DrawableRef()->GetIsPaletted(),
             "About to return a paletted frame");
  return result;
}

int32_t
FrameAnimator::GetTimeoutForFrame(uint32_t aFrameNum) const
{
  RawAccessFrameRef frame = GetRawFrame(aFrameNum);
  if (!frame) {
    NS_WARNING("No frame; called GetTimeoutForFrame too early?");
    return 100;
  }

  AnimationData data = frame->GetAnimationData();

  // Ensure a minimal time between updates so we don't throttle the UI thread.
  // consider 0 == unspecified and make it fast but not too fast.  Unless we
  // have a single loop GIF. See bug 890743, bug 125137, bug 139677, and bug
  // 207059. The behavior of recent IE and Opera versions seems to be:
  // IE 6/Win:
  //   10 - 50ms go 100ms
  //   >50ms go correct speed
  // Opera 7 final/Win:
  //   10ms goes 100ms
  //   >10ms go correct speed
  // It seems that there are broken tools out there that set a 0ms or 10ms
  // timeout when they really want a "default" one.  So munge values in that
  // range.
  if (data.mRawTimeout >= 0 && data.mRawTimeout <= 10) {
    return 100;
  }

  return data.mRawTimeout;
}

static void
DoCollectSizeOfCompositingSurfaces(const RawAccessFrameRef& aSurface,
                                   SurfaceMemoryCounterType aType,
                                   nsTArray<SurfaceMemoryCounter>& aCounters,
                                   MallocSizeOf aMallocSizeOf)
{
  // Concoct a SurfaceKey for this surface.
  SurfaceKey key = RasterSurfaceKey(aSurface->GetImageSize(),
                                    imgIContainer::DECODE_FLAGS_DEFAULT,
                                    /* aFrameNum = */ 0);

  // Create a counter for this surface.
  SurfaceMemoryCounter counter(key, /* aIsLocked = */ true, aType);

  // Extract the surface's memory usage information.
  size_t heap = 0, nonHeap = 0;
  aSurface->AddSizeOfExcludingThis(aMallocSizeOf, heap, nonHeap);
  counter.Values().SetDecodedHeap(heap);
  counter.Values().SetDecodedNonHeap(nonHeap);

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
FrameAnimator::GetRawFrame(uint32_t aFrameNum) const
{
  LookupResult result =
    SurfaceCache::Lookup(ImageKey(mImage),
                         RasterSurfaceKey(mSize,
                                          0,  // Default decode flags.
                                          aFrameNum));
  return result ? result.DrawableRef()->RawAccessRef()
                : RawAccessFrameRef();
}

//******************************************************************************
// DoBlend gets called when the timer for animation get fired and we have to
// update the composited frame of the animation.
bool
FrameAnimator::DoBlend(nsIntRect* aDirtyRect,
                       uint32_t aPrevFrameIndex,
                       uint32_t aNextFrameIndex)
{
  RawAccessFrameRef prevFrame = GetRawFrame(aPrevFrameIndex);
  RawAccessFrameRef nextFrame = GetRawFrame(aNextFrameIndex);

  MOZ_ASSERT(prevFrame && nextFrame, "Should have frames here");

  AnimationData prevFrameData = prevFrame->GetAnimationData();
  if (prevFrameData.mDisposalMethod == DisposalMethod::RESTORE_PREVIOUS &&
      !mCompositingPrevFrame) {
    prevFrameData.mDisposalMethod = DisposalMethod::CLEAR;
  }

  bool isFullPrevFrame = prevFrameData.mRect.x == 0 &&
                         prevFrameData.mRect.y == 0 &&
                         prevFrameData.mRect.width == mSize.width &&
                         prevFrameData.mRect.height == mSize.height;

  // Optimization: DisposeClearAll if the previous frame is the same size as
  //               container and it's clearing itself
  if (isFullPrevFrame &&
      (prevFrameData.mDisposalMethod == DisposalMethod::CLEAR)) {
    prevFrameData.mDisposalMethod = DisposalMethod::CLEAR_ALL;
  }

  AnimationData nextFrameData = nextFrame->GetAnimationData();
  bool isFullNextFrame = nextFrameData.mRect.x == 0 &&
                         nextFrameData.mRect.y == 0 &&
                         nextFrameData.mRect.width == mSize.width &&
                         nextFrameData.mRect.height == mSize.height;

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
      MOZ_ASSERT_UNREACHABLE("Unexpected DisposalMethod");
    case DisposalMethod::NOT_SPECIFIED:
    case DisposalMethod::KEEP:
      *aDirtyRect = nextFrameData.mRect;
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
      aDirtyRect->UnionRect(nextFrameData.mRect, prevFrameData.mRect);
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
    nsRefPtr<imgFrame> newFrame = new imgFrame;
    nsresult rv = newFrame->InitForDecoder(mSize,
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
      if ((prevFrameData.mRect.x >= nextFrameData.mRect.x) &&
          (prevFrameData.mRect.y >= nextFrameData.mRect.y) &&
          (prevFrameData.mRect.x + prevFrameData.mRect.width <=
              nextFrameData.mRect.x + nextFrameData.mRect.width) &&
          (prevFrameData.mRect.y + prevFrameData.mRect.height <=
              nextFrameData.mRect.y + nextFrameData.mRect.height)) {
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
                     prevFrameData.mRect);
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
        MOZ_ASSERT_UNREACHABLE("Unexpected DisposalMethod");
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
                           prevFrameData.mRect,
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
                        prevFrameData.mBlendMethod);
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
      nsRefPtr<imgFrame> newFrame = new imgFrame;
      nsresult rv = newFrame->InitForDecoder(mSize,
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
              nextFrameData.mBlendMethod);

  // Tell the image that it is fully 'downloaded'.
  mCompositingFrame->Finish();

  mLastCompositedFrameIndex = int32_t(aNextFrameIndex);

  return true;
}

//******************************************************************************
// Fill aFrame with black. Does also clears the mask.
void
FrameAnimator::ClearFrame(uint8_t* aFrameData, const nsIntRect& aFrameRect)
{
  if (!aFrameData) {
    return;
  }

  memset(aFrameData, 0, aFrameRect.width * aFrameRect.height * 4);
}

//******************************************************************************
void
FrameAnimator::ClearFrame(uint8_t* aFrameData, const nsIntRect& aFrameRect,
                          const nsIntRect& aRectToClear)
{
  if (!aFrameData || aFrameRect.width <= 0 || aFrameRect.height <= 0 ||
      aRectToClear.width <= 0 || aRectToClear.height <= 0) {
    return;
  }

  nsIntRect toClear = aFrameRect.Intersect(aRectToClear);
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
                              const nsIntRect& aRectSrc,
                              uint8_t* aDataDest,
                              const nsIntRect& aRectDest)
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
FrameAnimator::DrawFrameTo(const uint8_t* aSrcData, const nsIntRect& aSrcRect,
                           uint32_t aSrcPaletteLength, bool aSrcHasAlpha,
                           uint8_t* aDstPixels, const nsIntRect& aDstRect,
                           BlendMethod aBlendMethod)
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
    pixman_image_t* dst =
      pixman_image_create_bits(PIXMAN_a8r8g8b8,
                               aDstRect.width,
                               aDstRect.height,
                               reinterpret_cast<uint32_t*>(aDstPixels),
                               aDstRect.width * 4);

    auto op = aBlendMethod == BlendMethod::SOURCE ? PIXMAN_OP_SRC
                                                  : PIXMAN_OP_OVER;
    pixman_image_composite32(op,
                             src,
                             nullptr,
                             dst,
                             0, 0,
                             0, 0,
                             aSrcRect.x, aSrcRect.y,
                             aSrcRect.width, aSrcRect.height);

    pixman_image_unref(src);
    pixman_image_unref(dst);
  }

  return NS_OK;
}

} // namespace image
} // namespace mozilla
