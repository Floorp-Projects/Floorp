
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Decoder.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "GeckoProfiler.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"

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
    PostFrameStart();
  }

  // Implementation-specific initialization
  InitInternal();
  mInitialized = true;
}

void
Decoder::Write(const char* aBuffer, uint32_t aCount, DecodeStrategy aStrategy)
{
  PROFILER_LABEL("ImageDecoder", "Write",
    js::ProfileEntry::Category::GRAPHICS);

  MOZ_ASSERT(NS_IsMainThread() || aStrategy == DecodeStrategy::ASYNC);

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

  // Pass the data along to the implementation
  WriteInternal(aBuffer, aCount, aStrategy);

  // If we're a synchronous decoder and we need a new frame to proceed, let's
  // create one and call it again.
  if (aStrategy == DecodeStrategy::SYNC) {
    while (NeedsNewFrame() && !HasDataError()) {
      nsresult rv = AllocateFrame();

      if (NS_SUCCEEDED(rv)) {
        // Use the data we saved when we asked for a new frame.
        WriteInternal(nullptr, 0, aStrategy);
      }

      mNeedsToFlushData = false;
    }
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
  MOZ_ASSERT(NS_IsMainThread());

  mCurrentFrame = mImage.EnsureFrame(mNewFrameData.mFrameNum,
                                     mNewFrameData.mFrameRect,
                                     mDecodeFlags,
                                     mNewFrameData.mFormat,
                                     mNewFrameData.mPaletteDepth,
                                     mCurrentFrame.get());

  if (mCurrentFrame) {
    // Gather the raw pointers the decoders will use.
    mCurrentFrame->GetImageData(&mImageData, &mImageDataLength);
    mCurrentFrame->GetPaletteData(&mColormap, &mColormapSize);

    if (mNewFrameData.mFrameNum == mFrameCount) {
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
void Decoder::WriteInternal(const char* aBuffer, uint32_t aCount, DecodeStrategy aStrategy) { }
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
  mFrameCount++;
  mInFrame = true;

  // If we just became animated, record that fact.
  if (mFrameCount > 1) {
    mIsAnimated = true;
    mProgress |= FLAG_IS_ANIMATED;
  }

  // Decoder implementations should only call this method if they successfully
  // appended the frame to the image. So mFrameCount should always match that
  // reported by the Image.
  MOZ_ASSERT(mFrameCount == mImage.GetNumFrames(),
             "Decoder frame count doesn't match image's!");
}

void
Decoder::PostFrameStop(FrameBlender::FrameAlpha aFrameAlpha /* = FrameBlender::kFrameHasAlpha */,
                       FrameBlender::FrameDisposalMethod aDisposalMethod /* = FrameBlender::kDisposeKeep */,
                       int32_t aTimeout /* = 0 */,
                       FrameBlender::FrameBlendMethod aBlendMethod /* = FrameBlender::kBlendOver */)
{
  // We should be mid-frame
  MOZ_ASSERT(!IsSizeDecode(), "Stopping frame during a size decode");
  MOZ_ASSERT(mInFrame, "Stopping frame when we didn't start one");
  MOZ_ASSERT(mCurrentFrame, "Stopping frame when we don't have one");

  // Update our state
  mInFrame = false;

  if (aFrameAlpha == FrameBlender::kFrameOpaque) {
    mCurrentFrame->SetHasNoAlpha();
  }

  mCurrentFrame->SetFrameDisposalMethod(aDisposalMethod);
  mCurrentFrame->SetRawTimeout(aTimeout);
  mCurrentFrame->SetBlendMethod(aBlendMethod);
  mCurrentFrame->ImageUpdated(mCurrentFrame->GetRect());

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
