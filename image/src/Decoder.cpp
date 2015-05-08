
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Decoder.h"

#include "mozilla/gfx/2D.h"
#include "DecodePool.h"
#include "GeckoProfiler.h"
#include "imgIContainer.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsProxyRelease.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Telemetry.h"

using mozilla::gfx::IntSize;
using mozilla::gfx::SurfaceFormat;

namespace mozilla {
namespace image {

Decoder::Decoder(RasterImage* aImage)
  : mImage(aImage)
  , mProgress(NoProgress)
  , mImageData(nullptr)
  , mColormap(nullptr)
  , mChunkCount(0)
  , mFlags(0)
  , mBytesDecoded(0)
  , mSendPartialInvalidations(false)
  , mDataDone(false)
  , mDecodeDone(false)
  , mDataError(false)
  , mDecodeAborted(false)
  , mShouldReportError(false)
  , mImageIsTransient(false)
  , mImageIsLocked(false)
  , mFrameCount(0)
  , mFailCode(NS_OK)
  , mNeedsNewFrame(false)
  , mNeedsToFlushData(false)
  , mInitialized(false)
  , mSizeDecode(false)
  , mInFrame(false)
  , mIsAnimated(false)
{ }

Decoder::~Decoder()
{
  MOZ_ASSERT(mProgress == NoProgress,
             "Destroying Decoder without taking all its progress changes");
  MOZ_ASSERT(mInvalidRect.IsEmpty(),
             "Destroying Decoder without taking all its invalidations");
  mInitialized = false;

  if (!NS_IsMainThread()) {
    // Dispatch mImage to main thread to prevent it from being destructed by the
    // decode thread.
    nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
    NS_WARN_IF_FALSE(mainThread, "Couldn't get the main thread!");
    if (mainThread) {
      // Handle ambiguous nsISupports inheritance.
      RasterImage* rawImg = nullptr;
      mImage.swap(rawImg);
      DebugOnly<nsresult> rv =
        NS_ProxyRelease(mainThread, NS_ISUPPORTS_CAST(ImageResource*, rawImg));
      MOZ_ASSERT(NS_SUCCEEDED(rv), "Failed to proxy release to main thread");
    }
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

  // Fire OnStartDecode at init time to support bug 512435.
  if (!IsSizeDecode()) {
      mProgress |= FLAG_DECODE_STARTED | FLAG_ONLOAD_BLOCKED;
  }

  // Implementation-specific initialization
  InitInternal();

  mInitialized = true;
}

// Initializes a decoder whose image and observer is already being used by a
// parent decoder
void
Decoder::InitSharedDecoder(uint8_t* aImageData, uint32_t aImageDataLength,
                           uint32_t* aColormap, uint32_t aColormapSize,
                           RawAccessFrameRef&& aFrameRef)
{
  // No re-initializing
  MOZ_ASSERT(!mInitialized, "Can't re-initialize a decoder!");

  mImageData = aImageData;
  mImageDataLength = aImageDataLength;
  mColormap = aColormap;
  mColormapSize = aColormapSize;
  mCurrentFrame = Move(aFrameRef);

  // We have all the frame data, so we've started the frame.
  if (!IsSizeDecode()) {
    mFrameCount++;
    PostFrameStart();
  }

  // Implementation-specific initialization
  InitInternal();
  mInitialized = true;
}

nsresult
Decoder::Decode()
{
  MOZ_ASSERT(mInitialized, "Should be initialized here");
  MOZ_ASSERT(mIterator, "Should have a SourceBufferIterator");

  // We keep decoding chunks until the decode completes or there are no more
  // chunks available.
  while (!GetDecodeDone() && !HasError()) {
    auto newState = mIterator->AdvanceOrScheduleResume(this);

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

  nsCOMPtr<nsIEventTarget> target = decodePool->GetEventTarget();
  if (MOZ_UNLIKELY(!target)) {
    // We're shutting down and the DecodePool's thread pool has been destroyed.
    return;
  }

  nsCOMPtr<nsIRunnable> worker = decodePool->CreateDecodeWorker(this);
  target->Dispatch(worker, nsIEventTarget::DISPATCH_NORMAL);
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

  // We're strict about decoder errors
  MOZ_ASSERT(!HasDecoderError(),
             "Not allowed to make more decoder calls after error!");

  // Begin recording telemetry data.
  TimeStamp start = TimeStamp::Now();
  mChunkCount++;

  // Keep track of the total number of bytes written.
  mBytesDecoded += aCount;

  // If we're flushing data, clear the flag.
  if (aBuffer == nullptr && aCount == 0) {
    MOZ_ASSERT(mNeedsToFlushData, "Flushing when we don't need to");
    mNeedsToFlushData = false;
  }

  // If a data error occured, just ignore future data.
  if (HasDataError()) {
    return;
  }

  if (IsSizeDecode() && HasSize()) {
    // More data came in since we found the size. We have nothing to do here.
    return;
  }

  MOZ_ASSERT(!NeedsNewFrame() || HasDataError(),
             "Should not need a new frame before writing anything");
  MOZ_ASSERT(!NeedsToFlushData() || HasDataError(),
             "Should not need to flush data before writing anything");

  // Pass the data along to the implementation.
  WriteInternal(aBuffer, aCount);

  // If we need a new frame to proceed, let's create one and call it again.
  while (NeedsNewFrame() && !HasDataError()) {
    MOZ_ASSERT(!IsSizeDecode(), "Shouldn't need new frame for size decode");

    nsresult rv = AllocateFrame();

    if (NS_SUCCEEDED(rv)) {
      // Use the data we saved when we asked for a new frame.
      WriteInternal(nullptr, 0);
    }

    mNeedsToFlushData = false;
  }

  // Finish telemetry.
  mDecodeTime += (TimeStamp::Now() - start);
}

void
Decoder::CompleteDecode()
{
  // Implementation-specific finalization
  if (!HasError()) {
    FinishInternal();
  }

  // If the implementation left us mid-frame, finish that up.
  if (mInFrame && !HasError()) {
    PostFrameStop();
  }

  // If PostDecodeDone() has not been called, and this decoder wasn't aborted
  // early because of low-memory conditions or losing a race with another
  // decoder, we need to send teardown notifications (and report an error to the
  // console later).
  if (!IsSizeDecode() && !mDecodeDone && !WasAborted()) {
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
      if (!IsSizeDecode()) {
        mProgress |= FLAG_DECODE_COMPLETE | FLAG_ONLOAD_UNBLOCKED;
      }
      mProgress |= FLAG_HAS_ERROR;
    }
  }
}

void
Decoder::Finish()
{
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(HasError() || !mInFrame, "Finishing while we're still in a frame");

  // If we detected an error in CompleteDecode(), log it to the error console.
  if (mShouldReportError && !WasAborted()) {
    nsCOMPtr<nsIConsoleService> consoleService =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    nsCOMPtr<nsIScriptError> errorObject =
      do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);

    if (consoleService && errorObject && !HasDecoderError()) {
      nsAutoString msg(NS_LITERAL_STRING("Image corrupt or truncated: ") +
                       NS_ConvertUTF8toUTF16(mImage->GetURIString()));

      if (NS_SUCCEEDED(errorObject->InitWithWindowID(
                         msg,
                         NS_ConvertUTF8toUTF16(mImage->GetURIString()),
                         EmptyString(), 0, 0, nsIScriptError::errorFlag,
                         "Image", mImage->InnerWindowID()
                       ))) {
        consoleService->LogMessage(errorObject);
      }
    }
  }

  // Set image metadata before calling DecodingComplete, because
  // DecodingComplete calls Optimize().
  mImageMetadata.SetOnImage(mImage);

  if (HasSize()) {
    SetSizeOnImage();
  }

  if (mDecodeDone && !IsSizeDecode()) {
    MOZ_ASSERT(HasError() || mCurrentFrame, "Should have an error or a frame");

    // If this image wasn't animated and isn't a transient image, mark its frame
    // as optimizable. We don't support optimizing animated images and
    // optimizing transient images isn't worth it.
    if (!mIsAnimated && !mImageIsTransient && mCurrentFrame) {
      mCurrentFrame->SetOptimizable();
    }

    mImage->OnDecodingComplete();
  }
}

void
Decoder::FinishSharedDecoder()
{
  if (!HasError()) {
    FinishInternal();
  }
}

nsresult
Decoder::AllocateFrame(const nsIntSize& aTargetSize /* = nsIntSize() */)
{
  MOZ_ASSERT(mNeedsNewFrame);

  nsIntSize targetSize = aTargetSize;
  if (targetSize == nsIntSize()) {
    MOZ_ASSERT(HasSize());
    targetSize = mImageMetadata.GetSize();
  }

  mCurrentFrame = EnsureFrame(mNewFrameData.mFrameNum,
                              targetSize,
                              mNewFrameData.mFrameRect,
                              GetDecodeFlags(),
                              mNewFrameData.mFormat,
                              mNewFrameData.mPaletteDepth,
                              mCurrentFrame.get());

  if (mCurrentFrame) {
    // Gather the raw pointers the decoders will use.
    mCurrentFrame->GetImageData(&mImageData, &mImageDataLength);
    mCurrentFrame->GetPaletteData(&mColormap, &mColormapSize);

    if (mNewFrameData.mFrameNum + 1 == mFrameCount) {
      PostFrameStart();
    }
  } else {
    PostDataError();
  }

  // Mark ourselves as not needing another frame before talking to anyone else
  // so they can tell us if they need yet another.
  mNeedsNewFrame = false;

  // If we've received any data at all, we may have pending data that needs to
  // be flushed now that we have a frame to decode into.
  if (mBytesDecoded > 0) {
    mNeedsToFlushData = true;
  }

  return mCurrentFrame ? NS_OK : NS_ERROR_FAILURE;
}

RawAccessFrameRef
Decoder::EnsureFrame(uint32_t aFrameNum,
                     const nsIntSize& aTargetSize,
                     const nsIntRect& aFrameRect,
                     uint32_t aDecodeFlags,
                     SurfaceFormat aFormat,
                     uint8_t aPaletteDepth,
                     imgFrame* aPreviousFrame)
{
  if (mDataError || NS_FAILED(mFailCode)) {
    return RawAccessFrameRef();
  }

  MOZ_ASSERT(aFrameNum <= mFrameCount, "Invalid frame index!");
  if (aFrameNum > mFrameCount) {
    return RawAccessFrameRef();
  }

  // Adding a frame that doesn't already exist. This is the normal case.
  if (aFrameNum == mFrameCount) {
    return InternalAddFrame(aFrameNum, aTargetSize, aFrameRect, aDecodeFlags,
                            aFormat, aPaletteDepth, aPreviousFrame);
  }

  // We're replacing a frame. It must be the first frame; there's no reason to
  // ever replace any other frame, since the first frame is the only one we
  // speculatively allocate without knowing what the decoder really needs.
  // XXX(seth): I'm not convinced there's any reason to support this at all. We
  // should figure out how to avoid triggering this and rip it out.
  MOZ_ASSERT(aFrameNum == 0, "Replacing a frame other than the first?");
  MOZ_ASSERT(mFrameCount == 1, "Should have only one frame");
  MOZ_ASSERT(aPreviousFrame, "Need the previous frame to replace");
  if (aFrameNum != 0 || !aPreviousFrame || mFrameCount != 1) {
    return RawAccessFrameRef();
  }

  MOZ_ASSERT(!aPreviousFrame->GetRect().IsEqualEdges(aFrameRect) ||
             aPreviousFrame->GetFormat() != aFormat ||
             aPreviousFrame->GetPaletteDepth() != aPaletteDepth,
             "Replacing first frame with the same kind of frame?");

  // Reset our state.
  mInFrame = false;
  RawAccessFrameRef ref = Move(mCurrentFrame);

  MOZ_ASSERT(ref, "No ref to current frame?");

  // Reinitialize the old frame.
  nsIntSize oldSize = aPreviousFrame->GetImageSize();
  bool nonPremult =
    aDecodeFlags & imgIContainer::FLAG_DECODE_NO_PREMULTIPLY_ALPHA;
  if (NS_FAILED(aPreviousFrame->ReinitForDecoder(oldSize, aFrameRect, aFormat,
                                                 aPaletteDepth, nonPremult))) {
    NS_WARNING("imgFrame::ReinitForDecoder should succeed");
    mFrameCount = 0;
    aPreviousFrame->Abort();
    return RawAccessFrameRef();
  }

  return ref;
}

RawAccessFrameRef
Decoder::InternalAddFrame(uint32_t aFrameNum,
                          const nsIntSize& aTargetSize,
                          const nsIntRect& aFrameRect,
                          uint32_t aDecodeFlags,
                          SurfaceFormat aFormat,
                          uint8_t aPaletteDepth,
                          imgFrame* aPreviousFrame)
{
  MOZ_ASSERT(aFrameNum <= mFrameCount, "Invalid frame index!");
  if (aFrameNum > mFrameCount) {
    return RawAccessFrameRef();
  }

  if (aTargetSize.width <= 0 || aTargetSize.height <= 0 ||
      aFrameRect.width <= 0 || aFrameRect.height <= 0) {
    NS_WARNING("Trying to add frame with zero or negative size");
    return RawAccessFrameRef();
  }

  const uint32_t bytesPerPixel = aPaletteDepth == 0 ? 4 : 1;
  if (!SurfaceCache::CanHold(aFrameRect.Size(), bytesPerPixel)) {
    NS_WARNING("Trying to add frame that's too large for the SurfaceCache");
    return RawAccessFrameRef();
  }

  nsRefPtr<imgFrame> frame = new imgFrame();
  bool nonPremult =
    aDecodeFlags & imgIContainer::FLAG_DECODE_NO_PREMULTIPLY_ALPHA;
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

  InsertOutcome outcome =
    SurfaceCache::Insert(frame, ImageKey(mImage.get()),
                         RasterSurfaceKey(aTargetSize,
                                          aDecodeFlags,
                                          aFrameNum),
                         Lifetime::Persistent);
  if (outcome == InsertOutcome::FAILURE) {
    // We couldn't insert the surface, almost certainly due to low memory. We
    // treat this as a permanent error to help the system recover; otherwise, we
    // might just end up attempting to decode this image again immediately.
    ref->Abort();
    return RawAccessFrameRef();
  } else if (outcome == InsertOutcome::FAILURE_ALREADY_PRESENT) {
    // Another decoder beat us to decoding this frame. We abort this decoder
    // rather than treat this as a real error.
    mDecodeAborted = true;
    ref->Abort();
    return RawAccessFrameRef();
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
  mImage->OnAddedFrame(mFrameCount, refreshArea);

  return ref;
}

void
Decoder::SetSizeOnImage()
{
  MOZ_ASSERT(mImageMetadata.HasSize(), "Should have size");
  MOZ_ASSERT(mImageMetadata.HasOrientation(), "Should have orientation");

  nsresult rv = mImage->SetSize(mImageMetadata.GetWidth(),
                                mImageMetadata.GetHeight(),
                                mImageMetadata.GetOrientation());
  if (NS_FAILED(rv)) {
    PostResizeError();
  }
}

/*
 * Hook stubs. Override these as necessary in decoder implementations.
 */

void Decoder::InitInternal() { }
void Decoder::WriteInternal(const char* aBuffer, uint32_t aCount) { }
void Decoder::FinishInternal() { }

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
Decoder::PostFrameStart()
{
  // We shouldn't already be mid-frame
  MOZ_ASSERT(!mInFrame, "Starting new frame but not done with old one!");

  // Update our state to reflect the new frame
  mInFrame = true;

  // If we just became animated, record that fact.
  if (mFrameCount > 1) {
    mIsAnimated = true;
    mProgress |= FLAG_IS_ANIMATED;
  }
}

void
Decoder::PostFrameStop(Opacity aFrameOpacity    /* = Opacity::TRANSPARENT */,
                       DisposalMethod aDisposalMethod
                                                /* = DisposalMethod::KEEP */,
                       int32_t aTimeout         /* = 0 */,
                       BlendMethod aBlendMethod /* = BlendMethod::OVER */)
{
  // We should be mid-frame
  MOZ_ASSERT(!IsSizeDecode(), "Stopping frame during a size decode");
  MOZ_ASSERT(mInFrame, "Stopping frame when we didn't start one");
  MOZ_ASSERT(mCurrentFrame, "Stopping frame when we don't have one");

  // Update our state
  mInFrame = false;

  mCurrentFrame->Finish(aFrameOpacity, aDisposalMethod, aTimeout, aBlendMethod);

  mProgress |= FLAG_FRAME_COMPLETE | FLAG_ONLOAD_UNBLOCKED;

  // If we're not sending partial invalidations, then we send an invalidation
  // here when the first frame is complete.
  if (!mSendPartialInvalidations && !mIsAnimated) {
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
  if (mSendPartialInvalidations && !mIsAnimated) {
    mInvalidRect.UnionRect(mInvalidRect, aRect);
    mCurrentFrame->ImageUpdated(aRectAtTargetSize.valueOr(aRect));
  }
}

void
Decoder::PostDecodeDone(int32_t aLoopCount /* = 0 */)
{
  MOZ_ASSERT(!IsSizeDecode(), "Can't be done with decoding with size decode!");
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

void
Decoder::NeedNewFrame(uint32_t framenum, uint32_t x_offset, uint32_t y_offset,
                      uint32_t width, uint32_t height,
                      gfx::SurfaceFormat format,
                      uint8_t palette_depth /* = 0 */)
{
  // Decoders should never call NeedNewFrame without yielding back to Write().
  MOZ_ASSERT(!mNeedsNewFrame);

  // We don't want images going back in time or skipping frames.
  MOZ_ASSERT(framenum == mFrameCount || framenum == (mFrameCount - 1));

  mNewFrameData = NewFrameData(framenum,
                               nsIntRect(x_offset, y_offset, width, height),
                               format, palette_depth);
  mNeedsNewFrame = true;
}

Telemetry::ID
Decoder::SpeedHistogram()
{
  // Use HistogramCount as an invalid Histogram ID.
  return Telemetry::HistogramCount;
}

} // namespace image
} // namespace mozilla
