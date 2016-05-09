
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Decoder.h"

#include "mozilla/gfx/2D.h"
#include "DecodePool.h"
#include "GeckoProfiler.h"
#include "imgIContainer.h"
#include "nsProxyRelease.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Telemetry.h"

using mozilla::gfx::IntSize;
using mozilla::gfx::SurfaceFormat;

namespace mozilla {
namespace image {

Decoder::Decoder(RasterImage* aImage)
  : mImageData(nullptr)
  , mImageDataLength(0)
  , mColormap(nullptr)
  , mColormapSize(0)
  , mImage(aImage)
  , mProgress(NoProgress)
  , mFrameCount(0)
  , mFailCode(NS_OK)
  , mChunkCount(0)
  , mDecoderFlags(DefaultDecoderFlags())
  , mSurfaceFlags(DefaultSurfaceFlags())
  , mBytesDecoded(0)
  , mInitialized(false)
  , mMetadataDecode(false)
  , mInFrame(false)
  , mDataDone(false)
  , mDecodeDone(false)
  , mDataError(false)
  , mDecodeAborted(false)
  , mShouldReportError(false)
{ }

Decoder::~Decoder()
{
  MOZ_ASSERT(mProgress == NoProgress || !mImage,
             "Destroying Decoder without taking all its progress changes");
  MOZ_ASSERT(mInvalidRect.IsEmpty() || !mImage,
             "Destroying Decoder without taking all its invalidations");
  mInitialized = false;

  if (mImage && !NS_IsMainThread()) {
    // Dispatch mImage to main thread to prevent it from being destructed by the
    // decode thread.
    NS_ReleaseOnMainThread(mImage.forget());
  }
}

/*
 * Common implementation of the decoder interface.
 */

void
Decoder::Init()
{
  // No re-initializing
  MOZ_ASSERT(!mInitialized, "Can't re-initialize a decoder!");

  // It doesn't make sense to decode anything but the first frame if we can't
  // store anything in the SurfaceCache, since only the last frame we decode
  // will be retrievable.
  MOZ_ASSERT(ShouldUseSurfaceCache() || IsFirstFrameDecode());

  // Implementation-specific initialization
  InitInternal();

  mInitialized = true;
}

nsresult
Decoder::Decode(IResumable* aOnResume)
{
  MOZ_ASSERT(mInitialized, "Should be initialized here");
  MOZ_ASSERT(mIterator, "Should have a SourceBufferIterator");

  // If no IResumable was provided, default to |this|.
  IResumable* onResume = aOnResume ? aOnResume : this;

  // We keep decoding chunks until the decode completes or there are no more
  // chunks available.
  while (!GetDecodeDone() && !HasError()) {
    auto newState = mIterator->AdvanceOrScheduleResume(onResume);

    if (newState == SourceBufferIterator::WAITING) {
      // We can't continue because the rest of the data hasn't arrived from the
      // network yet. We don't have to do anything special; the
      // SourceBufferIterator will ensure that Decode() gets called again on a
      // DecodePool thread when more data is available.
      return NS_OK;
    }

    if (newState == SourceBufferIterator::COMPLETE) {
      mDataDone = true;

      nsresult finalStatus = mIterator->CompletionStatus();
      if (NS_FAILED(finalStatus)) {
        PostDataError();
      }

      CompleteDecode();
      return finalStatus;
    }

    MOZ_ASSERT(newState == SourceBufferIterator::READY);

    Write(mIterator->Data(), mIterator->Length());
  }

  CompleteDecode();
  return HasError() ? NS_ERROR_FAILURE : NS_OK;
}

void
Decoder::Resume()
{
  DecodePool* decodePool = DecodePool::Singleton();
  MOZ_ASSERT(decodePool);
  decodePool->AsyncDecode(this);
}

bool
Decoder::ShouldSyncDecode(size_t aByteLimit)
{
  MOZ_ASSERT(aByteLimit > 0);
  MOZ_ASSERT(mIterator, "Should have a SourceBufferIterator");

  return mIterator->RemainingBytesIsNoMoreThan(aByteLimit);
}

void
Decoder::Write(const char* aBuffer, uint32_t aCount)
{
  PROFILER_LABEL("ImageDecoder", "Write",
    js::ProfileEntry::Category::GRAPHICS);

  MOZ_ASSERT(aBuffer);
  MOZ_ASSERT(aCount > 0);

  // We're strict about decoder errors
  MOZ_ASSERT(!HasDecoderError(),
             "Not allowed to make more decoder calls after error!");

  // Begin recording telemetry data.
  TimeStamp start = TimeStamp::Now();
  mChunkCount++;

  // Keep track of the total number of bytes written.
  mBytesDecoded += aCount;

  // If a data error occured, just ignore future data.
  if (HasDataError()) {
    return;
  }

  if (IsMetadataDecode() && HasSize()) {
    // More data came in since we found the size. We have nothing to do here.
    return;
  }

  // Pass the data along to the implementation.
  WriteInternal(aBuffer, aCount);

  // Finish telemetry.
  mDecodeTime += (TimeStamp::Now() - start);
}

void
Decoder::CompleteDecode()
{
  // Implementation-specific finalization
  BeforeFinishInternal();
  if (!HasError()) {
    FinishInternal();
  } else {
    FinishWithErrorInternal();
  }

  // If the implementation left us mid-frame, finish that up.
  if (mInFrame && !HasError()) {
    PostFrameStop();
  }

  // If PostDecodeDone() has not been called, and this decoder wasn't aborted
  // early because of low-memory conditions or losing a race with another
  // decoder, we need to send teardown notifications (and report an error to the
  // console later).
  if (!IsMetadataDecode() && !mDecodeDone && !WasAborted()) {
    mShouldReportError = true;

    // If we only have a data error, we're usable if we have at least one
    // complete frame.
    if (!HasDecoderError() && GetCompleteFrameCount() > 0) {
      // We're usable, so do exactly what we should have when the decoder
      // completed.

      // Not writing to the entire frame may have left us transparent.
      PostHasTransparency();

      if (mInFrame) {
        PostFrameStop();
      }
      PostDecodeDone();
    } else {
      // We're not usable. Record some final progress indicating the error.
      if (!IsMetadataDecode()) {
        mProgress |= FLAG_DECODE_COMPLETE;
      }
      mProgress |= FLAG_HAS_ERROR;
    }
  }

  if (mDecodeDone && !IsMetadataDecode()) {
    MOZ_ASSERT(HasError() || mCurrentFrame, "Should have an error or a frame");

    // If this image wasn't animated and isn't a transient image, mark its frame
    // as optimizable. We don't support optimizing animated images and
    // optimizing transient images isn't worth it.
    if (!HasAnimation() &&
        !(mDecoderFlags & DecoderFlags::IMAGE_IS_TRANSIENT) &&
        mCurrentFrame) {
      mCurrentFrame->SetOptimizable();
    }
  }
}

nsresult
Decoder::SetTargetSize(const nsIntSize& aSize)
{
  // Make sure the size is reasonable.
  if (MOZ_UNLIKELY(aSize.width <= 0 || aSize.height <= 0)) {
    return NS_ERROR_FAILURE;
  }

  // Create a downscaler that we'll filter our output through.
  mDownscaler.emplace(aSize);

  return NS_OK;
}

nsresult
Decoder::AllocateFrame(uint32_t aFrameNum,
                       const nsIntSize& aTargetSize,
                       const nsIntRect& aFrameRect,
                       gfx::SurfaceFormat aFormat,
                       uint8_t aPaletteDepth)
{
  mCurrentFrame = AllocateFrameInternal(aFrameNum, aTargetSize, aFrameRect,
                                        aFormat, aPaletteDepth,
                                        mCurrentFrame.get());

  if (mCurrentFrame) {
    // Gather the raw pointers the decoders will use.
    mCurrentFrame->GetImageData(&mImageData, &mImageDataLength);
    mCurrentFrame->GetPaletteData(&mColormap, &mColormapSize);

    if (aFrameNum + 1 == mFrameCount) {
      // If we're past the first frame, PostIsAnimated() should've been called.
      MOZ_ASSERT_IF(mFrameCount > 1, HasAnimation());

      // Update our state to reflect the new frame
      MOZ_ASSERT(!mInFrame, "Starting new frame but not done with old one!");
      mInFrame = true;
    }
  } else {
    PostDataError();
  }

  return mCurrentFrame ? NS_OK : NS_ERROR_FAILURE;
}

RawAccessFrameRef
Decoder::AllocateFrameInternal(uint32_t aFrameNum,
                               const nsIntSize& aTargetSize,
                               const nsIntRect& aFrameRect,
                               SurfaceFormat aFormat,
                               uint8_t aPaletteDepth,
                               imgFrame* aPreviousFrame)
{
  if (mDataError || NS_FAILED(mFailCode)) {
    return RawAccessFrameRef();
  }

  if (aFrameNum != mFrameCount) {
    MOZ_ASSERT_UNREACHABLE("Allocating frames out of order");
    return RawAccessFrameRef();
  }

  if (aTargetSize.width <= 0 || aTargetSize.height <= 0 ||
      aFrameRect.width <= 0 || aFrameRect.height <= 0) {
    NS_WARNING("Trying to add frame with zero or negative size");
    return RawAccessFrameRef();
  }

  const uint32_t bytesPerPixel = aPaletteDepth == 0 ? 4 : 1;
  if (ShouldUseSurfaceCache() &&
      !SurfaceCache::CanHold(aFrameRect.Size(), bytesPerPixel)) {
    NS_WARNING("Trying to add frame that's too large for the SurfaceCache");
    return RawAccessFrameRef();
  }

  RefPtr<imgFrame> frame = new imgFrame();
  bool nonPremult = bool(mSurfaceFlags & SurfaceFlags::NO_PREMULTIPLY_ALPHA);
  if (NS_FAILED(frame->InitForDecoder(aTargetSize, aFrameRect, aFormat,
                                      aPaletteDepth, nonPremult))) {
    NS_WARNING("imgFrame::Init should succeed");
    return RawAccessFrameRef();
  }

  RawAccessFrameRef ref = frame->RawAccessRef();
  if (!ref) {
    frame->Abort();
    return RawAccessFrameRef();
  }

  if (ShouldUseSurfaceCache()) {
    InsertOutcome outcome =
      SurfaceCache::Insert(frame, ImageKey(mImage.get()),
                           RasterSurfaceKey(aTargetSize,
                                            mSurfaceFlags,
                                            aFrameNum));
    if (outcome == InsertOutcome::FAILURE) {
      // We couldn't insert the surface, almost certainly due to low memory. We
      // treat this as a permanent error to help the system recover; otherwise,
      // we might just end up attempting to decode this image again immediately.
      ref->Abort();
      return RawAccessFrameRef();
    } else if (outcome == InsertOutcome::FAILURE_ALREADY_PRESENT) {
      // Another decoder beat us to decoding this frame. We abort this decoder
      // rather than treat this as a real error.
      mDecodeAborted = true;
      ref->Abort();
      return RawAccessFrameRef();
    }
  }

  nsIntRect refreshArea;

  if (aFrameNum == 1) {
    MOZ_ASSERT(aPreviousFrame, "Must provide a previous frame when animated");
    aPreviousFrame->SetRawAccessOnly();

    // If we dispose of the first frame by clearing it, then the first frame's
    // refresh area is all of itself.
    // RESTORE_PREVIOUS is invalid (assumed to be DISPOSE_CLEAR).
    AnimationData previousFrameData = aPreviousFrame->GetAnimationData();
    if (previousFrameData.mDisposalMethod == DisposalMethod::CLEAR ||
        previousFrameData.mDisposalMethod == DisposalMethod::CLEAR_ALL ||
        previousFrameData.mDisposalMethod == DisposalMethod::RESTORE_PREVIOUS) {
      refreshArea = previousFrameData.mRect;
    }
  }

  if (aFrameNum > 0) {
    ref->SetRawAccessOnly();

    // Some GIFs are huge but only have a small area that they animate. We only
    // need to refresh that small area when frame 0 comes around again.
    refreshArea.UnionRect(refreshArea, frame->GetRect());
  }

  mFrameCount++;

  if (mImage) {
    mImage->OnAddedFrame(mFrameCount, refreshArea);
  }

  return ref;
}

/*
 * Hook stubs. Override these as necessary in decoder implementations.
 */

void Decoder::InitInternal() { }
void Decoder::BeforeFinishInternal() { }
void Decoder::FinishInternal() { }
void Decoder::FinishWithErrorInternal() { }

/*
 * Progress Notifications
 */

void
Decoder::PostSize(int32_t aWidth,
                  int32_t aHeight,
                  Orientation aOrientation /* = Orientation()*/)
{
  // Validate
  MOZ_ASSERT(aWidth >= 0, "Width can't be negative!");
  MOZ_ASSERT(aHeight >= 0, "Height can't be negative!");

  // Tell the image
  mImageMetadata.SetSize(aWidth, aHeight, aOrientation);

  // Record this notification.
  mProgress |= FLAG_SIZE_AVAILABLE;
}

void
Decoder::PostHasTransparency()
{
  mProgress |= FLAG_HAS_TRANSPARENCY;
}

void
Decoder::PostIsAnimated(int32_t aFirstFrameTimeout)
{
  mProgress |= FLAG_IS_ANIMATED;
  mImageMetadata.SetHasAnimation();
  mImageMetadata.SetFirstFrameTimeout(aFirstFrameTimeout);
}

void
Decoder::PostFrameStop(Opacity aFrameOpacity
                         /* = Opacity::SOME_TRANSPARENCY */,
                       DisposalMethod aDisposalMethod
                         /* = DisposalMethod::KEEP */,
                       int32_t aTimeout         /* = 0 */,
                       BlendMethod aBlendMethod /* = BlendMethod::OVER */)
{
  // We should be mid-frame
  MOZ_ASSERT(!IsMetadataDecode(), "Stopping frame during metadata decode");
  MOZ_ASSERT(mInFrame, "Stopping frame when we didn't start one");
  MOZ_ASSERT(mCurrentFrame, "Stopping frame when we don't have one");

  // Update our state
  mInFrame = false;

  mCurrentFrame->Finish(aFrameOpacity, aDisposalMethod, aTimeout, aBlendMethod);

  mProgress |= FLAG_FRAME_COMPLETE;

  // If we're not sending partial invalidations, then we send an invalidation
  // here when the first frame is complete.
  if (!ShouldSendPartialInvalidations() && mFrameCount == 1) {
    mInvalidRect.UnionRect(mInvalidRect,
                           gfx::IntRect(gfx::IntPoint(0, 0), GetSize()));
  }
}

void
Decoder::PostInvalidation(const nsIntRect& aRect,
                          const Maybe<nsIntRect>& aRectAtTargetSize
                            /* = Nothing() */)
{
  // We should be mid-frame
  MOZ_ASSERT(mInFrame, "Can't invalidate when not mid-frame!");
  MOZ_ASSERT(mCurrentFrame, "Can't invalidate when not mid-frame!");

  // Record this invalidation, unless we're not sending partial invalidations
  // or we're past the first frame.
  if (ShouldSendPartialInvalidations() && mFrameCount == 1) {
    mInvalidRect.UnionRect(mInvalidRect, aRect);
    mCurrentFrame->ImageUpdated(aRectAtTargetSize.valueOr(aRect));
  }
}

void
Decoder::PostDecodeDone(int32_t aLoopCount /* = 0 */)
{
  MOZ_ASSERT(!IsMetadataDecode(), "Done with decoding in metadata decode");
  MOZ_ASSERT(!mInFrame, "Can't be done decoding if we're mid-frame!");
  MOZ_ASSERT(!mDecodeDone, "Decode already done!");
  mDecodeDone = true;

  mImageMetadata.SetLoopCount(aLoopCount);

  mProgress |= FLAG_DECODE_COMPLETE;
}

void
Decoder::PostDataError()
{
  mDataError = true;

  if (mInFrame && mCurrentFrame) {
    mCurrentFrame->Abort();
  }
}

void
Decoder::PostDecoderError(nsresult aFailureCode)
{
  MOZ_ASSERT(NS_FAILED(aFailureCode), "Not a failure code!");

  mFailCode = aFailureCode;

  // XXXbholley - we should report the image URI here, but imgContainer
  // needs to know its URI first
  NS_WARNING("Image decoding error - This is probably a bug!");

  if (mInFrame && mCurrentFrame) {
    mCurrentFrame->Abort();
  }
}

Telemetry::ID
Decoder::SpeedHistogram()
{
  // Use HistogramCount as an invalid Histogram ID.
  return Telemetry::HistogramCount;
}

} // namespace image
} // namespace mozilla
