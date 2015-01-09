
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Decoder.h"

#include "mozilla/gfx/2D.h"
#include "imgIContainer.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "GeckoProfiler.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"

using mozilla::gfx::IntSize;
using mozilla::gfx::SurfaceFormat;

namespace mozilla {
namespace image {

Decoder::Decoder(RasterImage &aImage)
  : mImage(aImage)
  , mProgress(NoProgress)
  , mImageData(nullptr)
  , mColormap(nullptr)
  , mChunkCount(0)
  , mDecodeFlags(0)
  , mBytesDecoded(0)
  , mDecodeDone(false)
  , mDataError(false)
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
}

/*
 * Common implementation of the decoder interface.
 */

void
Decoder::Init()
{
  // No re-initializing
  NS_ABORT_IF_FALSE(!mInitialized, "Can't re-initialize a decoder!");

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
  NS_ABORT_IF_FALSE(!mInitialized, "Can't re-initialize a decoder!");

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
  if (HasDataError())
    return;

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
Decoder::Finish(ShutdownReason aReason)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Implementation-specific finalization
  if (!HasError())
    FinishInternal();

  // If the implementation left us mid-frame, finish that up.
  if (mInFrame && !HasError())
    PostFrameStop();

  // If PostDecodeDone() has not been called, we need to sent teardown
  // notifications.
  if (!IsSizeDecode() && !mDecodeDone) {

    // Log data errors to the error console
    nsCOMPtr<nsIConsoleService> consoleService =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    nsCOMPtr<nsIScriptError> errorObject =
      do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);

    if (consoleService && errorObject && !HasDecoderError()) {
      nsAutoString msg(NS_LITERAL_STRING("Image corrupt or truncated: ") +
                       NS_ConvertUTF8toUTF16(mImage.GetURIString()));

      if (NS_SUCCEEDED(errorObject->InitWithWindowID(
                         msg,
                         NS_ConvertUTF8toUTF16(mImage.GetURIString()),
                         EmptyString(), 0, 0, nsIScriptError::errorFlag,
                         "Image", mImage.InnerWindowID()
                       ))) {
        consoleService->LogMessage(errorObject);
      }
    }

    bool usable = !HasDecoderError();
    if (aReason != ShutdownReason::NOT_NEEDED && !HasDecoderError()) {
      // If we only have a data error, we're usable if we have at least one complete frame.
      if (GetCompleteFrameCount() == 0) {
        usable = false;
      }
    }

    // If we're usable, do exactly what we should have when the decoder
    // completed.
    if (usable) {
      if (mInFrame) {
        PostFrameStop();
      }
      PostDecodeDone();
    } else {
      if (!IsSizeDecode()) {
        mProgress |= FLAG_DECODE_COMPLETE | FLAG_ONLOAD_UNBLOCKED;
      }
      mProgress |= FLAG_HAS_ERROR;
    }
  }

  // Set image metadata before calling DecodingComplete, because
  // DecodingComplete calls Optimize().
  mImageMetadata.SetOnImage(&mImage);

  if (mDecodeDone) {
    MOZ_ASSERT(HasError() || mCurrentFrame, "Should have an error or a frame");
    mImage.DecodingComplete(mCurrentFrame.get());
  }
}

void
Decoder::FinishSharedDecoder()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!HasError()) {
    FinishInternal();
  }
}

nsresult
Decoder::AllocateFrame()
{
  MOZ_ASSERT(mNeedsNewFrame);

  mCurrentFrame = EnsureFrame(mNewFrameData.mFrameNum,
                              mNewFrameData.mFrameRect,
                              mDecodeFlags,
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
    return InternalAddFrame(aFrameNum, aFrameRect, aDecodeFlags, aFormat,
                            aPaletteDepth, aPreviousFrame);
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

  // Remove the old frame from the SurfaceCache.
  IntSize prevFrameSize = aPreviousFrame->GetImageSize();
  SurfaceCache::RemoveSurface(ImageKey(&mImage),
                              RasterSurfaceKey(prevFrameSize, aDecodeFlags, 0));
  mFrameCount = 0;
  mInFrame = false;

  // Add the new frame as usual.
  return InternalAddFrame(aFrameNum, aFrameRect, aDecodeFlags, aFormat,
                          aPaletteDepth, nullptr);
}

RawAccessFrameRef
Decoder::InternalAddFrame(uint32_t aFrameNum,
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

  MOZ_ASSERT(mImageMetadata.HasSize());
  nsIntSize imageSize(mImageMetadata.GetWidth(), mImageMetadata.GetHeight());
  if (imageSize.width <= 0 || imageSize.height <= 0 ||
      aFrameRect.width <= 0 || aFrameRect.height <= 0) {
    NS_WARNING("Trying to add frame with zero or negative size");
    return RawAccessFrameRef();
  }

  if (!SurfaceCache::CanHold(imageSize.ToIntSize())) {
    NS_WARNING("Trying to add frame that's too large for the SurfaceCache");
    return RawAccessFrameRef();
  }

  nsRefPtr<imgFrame> frame = new imgFrame();
  bool nonPremult =
    aDecodeFlags & imgIContainer::FLAG_DECODE_NO_PREMULTIPLY_ALPHA;
  if (NS_FAILED(frame->InitForDecoder(imageSize, aFrameRect, aFormat,
                                      aPaletteDepth, nonPremult))) {
    NS_WARNING("imgFrame::Init should succeed");
    return RawAccessFrameRef();
  }

  RawAccessFrameRef ref = frame->RawAccessRef();
  if (!ref) {
    return RawAccessFrameRef();
  }

  bool succeeded =
    SurfaceCache::Insert(frame, ImageKey(&mImage),
                         RasterSurfaceKey(imageSize.ToIntSize(),
                                          aDecodeFlags,
                                          aFrameNum),
                         Lifetime::Persistent);
  if (!succeeded) {
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
  mImage.OnAddedFrame(mFrameCount, refreshArea);

  return ref;
}

void
Decoder::SetSizeOnImage()
{
  MOZ_ASSERT(mImageMetadata.HasSize(), "Should have size");
  MOZ_ASSERT(mImageMetadata.HasOrientation(), "Should have orientation");

  mImage.SetSize(mImageMetadata.GetWidth(),
                 mImageMetadata.GetHeight(),
                 mImageMetadata.GetOrientation());
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
  NS_ABORT_IF_FALSE(aWidth >= 0, "Width can't be negative!");
  NS_ABORT_IF_FALSE(aHeight >= 0, "Height can't be negative!");

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
  NS_ABORT_IF_FALSE(!mInFrame, "Starting new frame but not done with old one!");

  // Update our state to reflect the new frame
  mInFrame = true;

  // If we just became animated, record that fact.
  if (mFrameCount > 1) {
    mIsAnimated = true;
    mProgress |= FLAG_IS_ANIMATED;
  }
}

void
Decoder::PostFrameStop(Opacity aFrameOpacity /* = Opacity::TRANSPARENT */,
                       DisposalMethod aDisposalMethod /* = DisposalMethod::KEEP */,
                       int32_t aTimeout /* = 0 */,
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
}

void
Decoder::PostInvalidation(nsIntRect& aRect)
{
  // We should be mid-frame
  NS_ABORT_IF_FALSE(mInFrame, "Can't invalidate when not mid-frame!");
  NS_ABORT_IF_FALSE(mCurrentFrame, "Can't invalidate when not mid-frame!");

  // Account for the new region
  mInvalidRect.UnionRect(mInvalidRect, aRect);
  mCurrentFrame->ImageUpdated(aRect);
}

void
Decoder::PostDecodeDone(int32_t aLoopCount /* = 0 */)
{
  NS_ABORT_IF_FALSE(!IsSizeDecode(), "Can't be done with decoding with size decode!");
  NS_ABORT_IF_FALSE(!mInFrame, "Can't be done decoding if we're mid-frame!");
  NS_ABORT_IF_FALSE(!mDecodeDone, "Decode already done!");
  mDecodeDone = true;

  mImageMetadata.SetLoopCount(aLoopCount);

  mProgress |= FLAG_DECODE_COMPLETE;
}

void
Decoder::PostDataError()
{
  mDataError = true;
}

void
Decoder::PostDecoderError(nsresult aFailureCode)
{
  NS_ABORT_IF_FALSE(NS_FAILED(aFailureCode), "Not a failure code!");

  mFailCode = aFailureCode;

  // XXXbholley - we should report the image URI here, but imgContainer
  // needs to know its URI first
  NS_WARNING("Image decoding error - This is probably a bug!");
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

} // namespace image
} // namespace mozilla
