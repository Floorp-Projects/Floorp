
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Decoder.h"

#include "mozilla/gfx/2D.h"
#include "DecodePool.h"
#include "GeckoProfiler.h"
#include "IDecodingTask.h"
#include "ISurfaceProvider.h"
#include "nsProxyRelease.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Telemetry.h"

using mozilla::gfx::IntSize;
using mozilla::gfx::SurfaceFormat;

namespace mozilla {
namespace image {

class MOZ_STACK_CLASS AutoRecordDecoderTelemetry final
{
public:
  explicit AutoRecordDecoderTelemetry(Decoder* aDecoder)
    : mDecoder(aDecoder)
  {
    MOZ_ASSERT(mDecoder);

    // Begin recording telemetry data.
    mStartTime = TimeStamp::Now();
  }

  ~AutoRecordDecoderTelemetry()
  {
    // Finish telemetry.
    mDecoder->mDecodeTime += (TimeStamp::Now() - mStartTime);
  }

private:
  Decoder* mDecoder;
  TimeStamp mStartTime;
};

Decoder::Decoder(RasterImage* aImage)
  : mImageData(nullptr)
  , mImageDataLength(0)
  , mColormap(nullptr)
  , mColormapSize(0)
  , mImage(aImage)
  , mProgress(NoProgress)
  , mFrameCount(0)
  , mDecoderFlags(DefaultDecoderFlags())
  , mSurfaceFlags(DefaultSurfaceFlags())
  , mInitialized(false)
  , mMetadataDecode(false)
  , mInFrame(false)
  , mReachedTerminalState(false)
  , mDecodeDone(false)
  , mError(false)
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

nsresult
Decoder::Init()
{
  // No re-initializing
  MOZ_ASSERT(!mInitialized, "Can't re-initialize a decoder!");

  // All decoders must have a SourceBufferIterator.
  MOZ_ASSERT(mIterator);

  // It doesn't make sense to decode anything but the first frame if we can't
  // store anything in the SurfaceCache, since only the last frame we decode
  // will be retrievable.
  MOZ_ASSERT(ShouldUseSurfaceCache() || IsFirstFrameDecode());

  // Implementation-specific initialization.
  nsresult rv = InitInternal();

  mInitialized = true;

  return rv;
}

nsresult
Decoder::Decode(IResumable* aOnResume /* = nullptr */)
{
  MOZ_ASSERT(mInitialized, "Should be initialized here");
  MOZ_ASSERT(mIterator, "Should have a SourceBufferIterator");

  // If we're already done, don't attempt to keep decoding.
  if (GetDecodeDone()) {
    return HasError() ? NS_ERROR_FAILURE : NS_OK;
  }

  LexerResult lexerResult(TerminalState::FAILURE);
  {
    PROFILER_LABEL("ImageDecoder", "Decode", js::ProfileEntry::Category::GRAPHICS);
    AutoRecordDecoderTelemetry telemetry(this);

    lexerResult =  DoDecode(*mIterator, aOnResume);
  };

  if (lexerResult.is<Yield>()) {
    // We need more data to continue. If @aOnResume was non-null, the
    // SourceBufferIterator will automatically reschedule us. Otherwise, it's up
    // to the caller.
    MOZ_ASSERT(lexerResult.as<Yield>() == Yield::NEED_MORE_DATA);
    return NS_OK;
  }

  // We reached a terminal state; we're now done decoding.
  MOZ_ASSERT(lexerResult.is<TerminalState>());
  mReachedTerminalState = true;

  // If decoding failed, record that fact.
  if (lexerResult.as<TerminalState>() == TerminalState::FAILURE) {
    PostError();
  }

  // Perform final cleanup.
  CompleteDecode();

  return HasError() ? NS_ERROR_FAILURE : NS_OK;
}

bool
Decoder::ShouldSyncDecode(size_t aByteLimit)
{
  MOZ_ASSERT(aByteLimit > 0);
  MOZ_ASSERT(mIterator, "Should have a SourceBufferIterator");

  return mIterator->RemainingBytesIsNoMoreThan(aByteLimit);
}

void
Decoder::CompleteDecode()
{
  // Implementation-specific finalization.
  nsresult rv = BeforeFinishInternal();
  if (NS_FAILED(rv)) {
    PostError();
  }

  rv = HasError() ? FinishWithErrorInternal()
                  : FinishInternal();
  if (NS_FAILED(rv)) {
    PostError();
  }

  // If this was a metadata decode and we never got a size, the decode failed.
  if (IsMetadataDecode() && !HasSize()) {
    PostError();
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

    // Even if we encountered an error, we're still usable if we have at least
    // one complete frame.
    if (GetCompleteFrameCount() > 0) {
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

Maybe<IntSize>
Decoder::GetTargetSize()
{
  return mDownscaler ? Some(mDownscaler->TargetSize()) : Nothing();
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
  if (HasError()) {
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

  NotNull<RefPtr<imgFrame>> frame = WrapNotNull(new imgFrame());
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
    NotNull<RefPtr<ISurfaceProvider>> provider =
      WrapNotNull(new SimpleSurfaceProvider(frame));
    InsertOutcome outcome =
      SurfaceCache::Insert(provider, ImageKey(mImage.get()),
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

nsresult Decoder::InitInternal() { return NS_OK; }
nsresult Decoder::BeforeFinishInternal() { return NS_OK; }
nsresult Decoder::FinishInternal() { return NS_OK; }
nsresult Decoder::FinishWithErrorInternal() { return NS_OK; }

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
                       BlendMethod aBlendMethod /* = BlendMethod::OVER */,
                       const Maybe<nsIntRect>& aBlendRect /* = Nothing() */)
{
  // We should be mid-frame
  MOZ_ASSERT(!IsMetadataDecode(), "Stopping frame during metadata decode");
  MOZ_ASSERT(mInFrame, "Stopping frame when we didn't start one");
  MOZ_ASSERT(mCurrentFrame, "Stopping frame when we don't have one");

  // Update our state
  mInFrame = false;

  mCurrentFrame->Finish(aFrameOpacity, aDisposalMethod, aTimeout,
                        aBlendMethod, aBlendRect);

  mProgress |= FLAG_FRAME_COMPLETE;

  // If we're not sending partial invalidations, then we send an invalidation
  // here when the first frame is complete.
  if (!ShouldSendPartialInvalidations() && mFrameCount == 1) {
    mInvalidRect.UnionRect(mInvalidRect,
                           gfx::IntRect(gfx::IntPoint(0, 0), GetSize()));
  }

  // If we are going to keep decoding we should notify now about the first frame being done.
  if (mImage && mFrameCount == 1 && HasAnimation()) {
    MOZ_ASSERT(HasProgress());
    IDecodingTask::NotifyProgress(WrapNotNull(this));
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
Decoder::PostError()
{
  mError = true;

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
