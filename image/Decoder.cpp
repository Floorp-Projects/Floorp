/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Decoder.h"

#include "DecodePool.h"
#include "GeckoProfiler.h"
#include "IDecodingTask.h"
#include "ISurfaceProvider.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/Telemetry.h"
#include "nsComponentManagerUtils.h"
#include "nsProxyRelease.h"
#include "nsServiceManagerUtils.h"

using mozilla::gfx::IntPoint;
using mozilla::gfx::IntRect;
using mozilla::gfx::IntSize;
using mozilla::gfx::SurfaceFormat;

namespace mozilla {
namespace image {

class MOZ_STACK_CLASS AutoRecordDecoderTelemetry final {
 public:
  explicit AutoRecordDecoderTelemetry(Decoder* aDecoder) : mDecoder(aDecoder) {
    MOZ_ASSERT(mDecoder);

    // Begin recording telemetry data.
    mStartTime = TimeStamp::Now();
  }

  ~AutoRecordDecoderTelemetry() {
    // Finish telemetry.
    mDecoder->mDecodeTime += (TimeStamp::Now() - mStartTime);
  }

 private:
  Decoder* mDecoder;
  TimeStamp mStartTime;
};

Decoder::Decoder(RasterImage* aImage)
    : mInProfile(nullptr),
      mTransform(nullptr),
      mImageData(nullptr),
      mImageDataLength(0),
      mImage(aImage),
      mFrameRecycler(nullptr),
      mProgress(NoProgress),
      mFrameCount(0),
      mLoopLength(FrameTimeout::Zero()),
      mDecoderFlags(DefaultDecoderFlags()),
      mSurfaceFlags(DefaultSurfaceFlags()),
      mInitialized(false),
      mMetadataDecode(false),
      mHaveExplicitOutputSize(false),
      mInFrame(false),
      mFinishedNewFrame(false),
      mHasFrameToTake(false),
      mReachedTerminalState(false),
      mDecodeDone(false),
      mError(false),
      mShouldReportError(false),
      mFinalizeFrames(true) {}

Decoder::~Decoder() {
  MOZ_ASSERT(mProgress == NoProgress || !mImage,
             "Destroying Decoder without taking all its progress changes");
  MOZ_ASSERT(mInvalidRect.IsEmpty() || !mImage,
             "Destroying Decoder without taking all its invalidations");
  mInitialized = false;

  if (mInProfile) {
    // mTransform belongs to us only if mInProfile is non-null
    if (mTransform) {
      qcms_transform_release(mTransform);
    }
    qcms_profile_release(mInProfile);
  }

  if (mImage && !NS_IsMainThread()) {
    // Dispatch mImage to main thread to prevent it from being destructed by the
    // decode thread.
    NS_ReleaseOnMainThread(mImage.forget());
  }
}

/*
 * Common implementation of the decoder interface.
 */

nsresult Decoder::Init() {
  // No re-initializing
  MOZ_ASSERT(!mInitialized, "Can't re-initialize a decoder!");

  // All decoders must have a SourceBufferIterator.
  MOZ_ASSERT(mIterator);

  // Metadata decoders must not set an output size.
  MOZ_ASSERT_IF(mMetadataDecode, !mHaveExplicitOutputSize);

  // All decoders must be anonymous except for metadata decoders.
  // XXX(seth): Soon that exception will be removed.
  MOZ_ASSERT_IF(mImage, IsMetadataDecode());

  // Implementation-specific initialization.
  nsresult rv = InitInternal();

  mInitialized = true;

  return rv;
}

LexerResult Decoder::Decode(IResumable* aOnResume /* = nullptr */) {
  MOZ_ASSERT(mInitialized, "Should be initialized here");
  MOZ_ASSERT(mIterator, "Should have a SourceBufferIterator");

  // If we're already done, don't attempt to keep decoding.
  if (GetDecodeDone()) {
    return LexerResult(HasError() ? TerminalState::FAILURE
                                  : TerminalState::SUCCESS);
  }

  LexerResult lexerResult(TerminalState::FAILURE);
  {
    AUTO_PROFILER_LABEL_CATEGORY_PAIR(GRAPHICS_ImageDecoding);
    AutoRecordDecoderTelemetry telemetry(this);

    lexerResult = DoDecode(*mIterator, aOnResume);
  };

  if (lexerResult.is<Yield>()) {
    // We either need more data to continue (in which case either @aOnResume or
    // the caller will reschedule us to run again later), or the decoder is
    // yielding to allow the caller access to some intermediate output.
    return lexerResult;
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

  return LexerResult(HasError() ? TerminalState::FAILURE
                                : TerminalState::SUCCESS);
}

LexerResult Decoder::TerminateFailure() {
  PostError();

  // Perform final cleanup if need be.
  if (!mReachedTerminalState) {
    mReachedTerminalState = true;
    CompleteDecode();
  }

  return LexerResult(TerminalState::FAILURE);
}

bool Decoder::ShouldSyncDecode(size_t aByteLimit) {
  MOZ_ASSERT(aByteLimit > 0);
  MOZ_ASSERT(mIterator, "Should have a SourceBufferIterator");

  return mIterator->RemainingBytesIsNoMoreThan(aByteLimit);
}

void Decoder::CompleteDecode() {
  // Implementation-specific finalization.
  nsresult rv = BeforeFinishInternal();
  if (NS_FAILED(rv)) {
    PostError();
  }

  rv = HasError() ? FinishWithErrorInternal() : FinishInternal();
  if (NS_FAILED(rv)) {
    PostError();
  }

  if (IsMetadataDecode()) {
    // If this was a metadata decode and we never got a size, the decode failed.
    if (!HasSize()) {
      PostError();
    }
    return;
  }

  // If the implementation left us mid-frame, finish that up. Note that it may
  // have left us transparent.
  if (mInFrame) {
    PostHasTransparency();
    PostFrameStop();
  }

  // If PostDecodeDone() has not been called, we may need to send teardown
  // notifications if it is unrecoverable.
  if (!mDecodeDone) {
    // We should always report an error to the console in this case.
    mShouldReportError = true;

    if (GetCompleteFrameCount() > 0) {
      // We're usable if we have at least one complete frame, so do exactly
      // what we should have when the decoder completed.
      PostHasTransparency();
      PostDecodeDone();
    } else {
      // We're not usable. Record some final progress indicating the error.
      mProgress |= FLAG_DECODE_COMPLETE | FLAG_HAS_ERROR;
    }
  }

  if (mDecodeDone) {
    MOZ_ASSERT(HasError() || mCurrentFrame, "Should have an error or a frame");

    // If this image wasn't animated and isn't a transient image, mark its frame
    // as optimizable. We don't support optimizing animated images and
    // optimizing transient images isn't worth it.
    if (!HasAnimation() &&
        !(mDecoderFlags & DecoderFlags::IMAGE_IS_TRANSIENT) && mCurrentFrame) {
      mCurrentFrame->SetOptimizable();
    }
  }
}

void Decoder::SetOutputSize(const gfx::IntSize& aSize) {
  mOutputSize = Some(aSize);
  mHaveExplicitOutputSize = true;
}

Maybe<gfx::IntSize> Decoder::ExplicitOutputSize() const {
  MOZ_ASSERT_IF(mHaveExplicitOutputSize, mOutputSize);
  return mHaveExplicitOutputSize ? mOutputSize : Nothing();
}

Maybe<uint32_t> Decoder::TakeCompleteFrameCount() {
  const bool finishedNewFrame = mFinishedNewFrame;
  mFinishedNewFrame = false;
  return finishedNewFrame ? Some(GetCompleteFrameCount()) : Nothing();
}

DecoderFinalStatus Decoder::FinalStatus() const {
  return DecoderFinalStatus(IsMetadataDecode(), GetDecodeDone(), HasError(),
                            ShouldReportError());
}

DecoderTelemetry Decoder::Telemetry() const {
  MOZ_ASSERT(mIterator);
  return DecoderTelemetry(SpeedHistogram(),
                          mIterator ? mIterator->ByteCount() : 0,
                          mIterator ? mIterator->ChunkCount() : 0, mDecodeTime);
}

nsresult Decoder::AllocateFrame(const gfx::IntSize& aOutputSize,
                                gfx::SurfaceFormat aFormat,
                                const Maybe<AnimationParams>& aAnimParams) {
  mCurrentFrame = AllocateFrameInternal(aOutputSize, aFormat, aAnimParams,
                                        std::move(mCurrentFrame));

  if (mCurrentFrame) {
    mHasFrameToTake = true;

    // Gather the raw pointers the decoders will use.
    mCurrentFrame->GetImageData(&mImageData, &mImageDataLength);

    // We should now be on |aFrameNum|. (Note that we're comparing the frame
    // number, which is zero-based, with the frame count, which is one-based.)
    MOZ_ASSERT_IF(aAnimParams, aAnimParams->mFrameNum + 1 == mFrameCount);

    // If we're past the first frame, PostIsAnimated() should've been called.
    MOZ_ASSERT_IF(mFrameCount > 1, HasAnimation());

    // Update our state to reflect the new frame.
    MOZ_ASSERT(!mInFrame, "Starting new frame but not done with old one!");
    mInFrame = true;
  }

  return mCurrentFrame ? NS_OK : NS_ERROR_FAILURE;
}

RawAccessFrameRef Decoder::AllocateFrameInternal(
    const gfx::IntSize& aOutputSize, SurfaceFormat aFormat,
    const Maybe<AnimationParams>& aAnimParams,
    RawAccessFrameRef&& aPreviousFrame) {
  if (HasError()) {
    return RawAccessFrameRef();
  }

  uint32_t frameNum = aAnimParams ? aAnimParams->mFrameNum : 0;
  if (frameNum != mFrameCount) {
    MOZ_ASSERT_UNREACHABLE("Allocating frames out of order");
    return RawAccessFrameRef();
  }

  if (aOutputSize.width <= 0 || aOutputSize.height <= 0) {
    NS_WARNING("Trying to add frame with zero or negative size");
    return RawAccessFrameRef();
  }

  if (frameNum == 1) {
    MOZ_ASSERT(aPreviousFrame, "Must provide a previous frame when animated");
    aPreviousFrame->SetRawAccessOnly();
  }

  if (frameNum > 0) {
    if (aPreviousFrame->GetDisposalMethod() !=
        DisposalMethod::RESTORE_PREVIOUS) {
      // If the new restore frame is the direct previous frame, then we know
      // the dirty rect is composed only of the current frame's blend rect and
      // the restore frame's clear rect (if applicable) which are handled in
      // filters.
      mRestoreFrame = std::move(aPreviousFrame);
      mRestoreDirtyRect.SetBox(0, 0, 0, 0);
    } else {
      // We only need the previous frame's dirty rect, because while there may
      // have been several frames between us and mRestoreFrame, the only areas
      // that changed are the restore frame's clear rect, the current frame
      // blending rect, and the previous frame's blending rect. All else is
      // forgotten due to us restoring the same frame again.
      mRestoreDirtyRect = aPreviousFrame->GetBoundedBlendRect();
    }
  }

  RawAccessFrameRef ref;

  // If we have a frame recycler, it must be for an animated image producing
  // full frames. If the higher layers are discarding frames because of the
  // memory footprint, then the recycler will allow us to reuse the buffers.
  // Each frame should be the same size and have mostly the same properties.
  if (mFrameRecycler) {
    MOZ_ASSERT(aAnimParams);

    ref = mFrameRecycler->RecycleFrame(mRecycleRect);
    if (ref) {
      // If the recycled frame is actually the current restore frame, we cannot
      // use it. If the next restore frame is the new frame we are creating, in
      // theory we could reuse it, but we would need to store the restore frame
      // animation parameters elsewhere. For now we just drop it.
      bool blocked = ref.get() == mRestoreFrame.get();
      if (!blocked) {
        blocked = NS_FAILED(ref->InitForDecoderRecycle(aAnimParams.ref()));
      }

      if (blocked) {
        ref.reset();
      }
    }
  }

  // Either the recycler had nothing to give us, or we don't have a recycler.
  // Produce a new frame to store the data.
  if (!ref) {
    // There is no underlying data to reuse, so reset the recycle rect to be
    // the full frame, to ensure the restore frame is fully copied.
    mRecycleRect = IntRect(IntPoint(0, 0), aOutputSize);

    bool nonPremult = bool(mSurfaceFlags & SurfaceFlags::NO_PREMULTIPLY_ALPHA);
    auto frame = MakeNotNull<RefPtr<imgFrame>>();
    if (NS_FAILED(frame->InitForDecoder(aOutputSize, aFormat, nonPremult,
                                        aAnimParams, bool(mFrameRecycler)))) {
      NS_WARNING("imgFrame::Init should succeed");
      return RawAccessFrameRef();
    }

    ref = frame->RawAccessRef();
    if (!ref) {
      frame->Abort();
      return RawAccessFrameRef();
    }

    if (frameNum > 0) {
      frame->SetRawAccessOnly();
    }
  }

  mFrameCount++;

  return ref;
}

/*
 * Hook stubs. Override these as necessary in decoder implementations.
 */

nsresult Decoder::InitInternal() { return NS_OK; }
nsresult Decoder::BeforeFinishInternal() { return NS_OK; }
nsresult Decoder::FinishInternal() { return NS_OK; }

nsresult Decoder::FinishWithErrorInternal() {
  MOZ_ASSERT(!mInFrame);
  return NS_OK;
}

/*
 * Progress Notifications
 */

void Decoder::PostSize(int32_t aWidth, int32_t aHeight,
                       Orientation aOrientation /* = Orientation()*/) {
  // Validate.
  MOZ_ASSERT(aWidth >= 0, "Width can't be negative!");
  MOZ_ASSERT(aHeight >= 0, "Height can't be negative!");

  // Set our intrinsic size.
  mImageMetadata.SetSize(aWidth, aHeight, aOrientation);

  // Verify it is the expected size, if given. Note that this is only used by
  // the ICO decoder for embedded image types, so only its subdecoders are
  // required to handle failures in PostSize.
  if (!IsExpectedSize()) {
    PostError();
    return;
  }

  // Set our output size if it's not already set.
  if (!mOutputSize) {
    mOutputSize = Some(IntSize(aWidth, aHeight));
  }

  MOZ_ASSERT(mOutputSize->width <= aWidth && mOutputSize->height <= aHeight,
             "Output size will result in upscaling");

  // Record this notification.
  mProgress |= FLAG_SIZE_AVAILABLE;
}

void Decoder::PostHasTransparency() { mProgress |= FLAG_HAS_TRANSPARENCY; }

void Decoder::PostIsAnimated(FrameTimeout aFirstFrameTimeout) {
  mProgress |= FLAG_IS_ANIMATED;
  mImageMetadata.SetHasAnimation();
  mImageMetadata.SetFirstFrameTimeout(aFirstFrameTimeout);
}

void Decoder::PostFrameStop(Opacity aFrameOpacity) {
  // We should be mid-frame
  MOZ_ASSERT(!IsMetadataDecode(), "Stopping frame during metadata decode");
  MOZ_ASSERT(mInFrame, "Stopping frame when we didn't start one");
  MOZ_ASSERT(mCurrentFrame, "Stopping frame when we don't have one");

  // Update our state.
  mInFrame = false;
  mFinishedNewFrame = true;

  mCurrentFrame->Finish(aFrameOpacity, mFinalizeFrames);

  mProgress |= FLAG_FRAME_COMPLETE;

  mLoopLength += mCurrentFrame->GetTimeout();

  if (mFrameCount == 1) {
    // If we're not sending partial invalidations, then we send an invalidation
    // here when the first frame is complete.
    if (!ShouldSendPartialInvalidations()) {
      mInvalidRect.UnionRect(mInvalidRect, IntRect(IntPoint(), Size()));
    }

    // If we dispose of the first frame by clearing it, then the first frame's
    // refresh area is all of itself. RESTORE_PREVIOUS is invalid (assumed to
    // be DISPOSE_CLEAR).
    switch (mCurrentFrame->GetDisposalMethod()) {
      default:
        MOZ_FALLTHROUGH_ASSERT("Unexpected DisposalMethod");
      case DisposalMethod::CLEAR:
      case DisposalMethod::CLEAR_ALL:
      case DisposalMethod::RESTORE_PREVIOUS:
        mFirstFrameRefreshArea = IntRect(IntPoint(), Size());
        break;
      case DisposalMethod::KEEP:
      case DisposalMethod::NOT_SPECIFIED:
        break;
    }
  } else {
    // Some GIFs are huge but only have a small area that they animate. We only
    // need to refresh that small area when frame 0 comes around again.
    mFirstFrameRefreshArea.UnionRect(mFirstFrameRefreshArea,
                                     mCurrentFrame->GetBoundedBlendRect());
  }
}

void Decoder::PostInvalidation(const gfx::IntRect& aRect,
                               const Maybe<gfx::IntRect>& aRectAtOutputSize
                               /* = Nothing() */) {
  // We should be mid-frame
  MOZ_ASSERT(mInFrame, "Can't invalidate when not mid-frame!");
  MOZ_ASSERT(mCurrentFrame, "Can't invalidate when not mid-frame!");

  // Record this invalidation, unless we're not sending partial invalidations
  // or we're past the first frame.
  if (ShouldSendPartialInvalidations() && mFrameCount == 1) {
    mInvalidRect.UnionRect(mInvalidRect, aRect);
    mCurrentFrame->ImageUpdated(aRectAtOutputSize.valueOr(aRect));
  }
}

void Decoder::PostDecodeDone(int32_t aLoopCount /* = 0 */) {
  MOZ_ASSERT(!IsMetadataDecode(), "Done with decoding in metadata decode");
  MOZ_ASSERT(!mInFrame, "Can't be done decoding if we're mid-frame!");
  MOZ_ASSERT(!mDecodeDone, "Decode already done!");
  mDecodeDone = true;

  mImageMetadata.SetLoopCount(aLoopCount);

  // Some metadata that we track should take into account every frame in the
  // image. If this is a first-frame-only decode, our accumulated loop length
  // and first frame refresh area only includes the first frame, so it's not
  // correct and we don't record it.
  if (!IsFirstFrameDecode()) {
    mImageMetadata.SetLoopLength(mLoopLength);
    mImageMetadata.SetFirstFrameRefreshArea(mFirstFrameRefreshArea);
  }

  mProgress |= FLAG_DECODE_COMPLETE;
}

void Decoder::PostError() {
  mError = true;

  if (mInFrame) {
    MOZ_ASSERT(mCurrentFrame);
    MOZ_ASSERT(mFrameCount > 0);
    mCurrentFrame->Abort();
    mInFrame = false;
    --mFrameCount;
    mHasFrameToTake = false;
  }
}

}  // namespace image
}  // namespace mozilla
