
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Decoder.h"
#include "nsIServiceManager.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "GeckoProfiler.h"

namespace mozilla {
namespace image {

Decoder::Decoder(RasterImage &aImage)
  : mImage(aImage)
  , mCurrentFrame(nullptr)
  , mImageData(nullptr)
  , mColormap(nullptr)
  , mDecodeFlags(0)
  , mDecodeDone(false)
  , mDataError(false)
  , mFrameCount(0)
  , mFailCode(NS_OK)
  , mNeedsNewFrame(false)
  , mInitialized(false)
  , mSizeDecode(false)
  , mInFrame(false)
  , mIsAnimated(false)
  , mSynchronous(false)
{
}

Decoder::~Decoder()
{
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
  NS_ABORT_IF_FALSE(mObserver, "Need an observer!");

  // Fire OnStartDecode at init time to support bug 512435.
  if (!IsSizeDecode())
      mObserver->OnStartDecode();

  // Implementation-specific initialization
  InitInternal();

  mInitialized = true;
}

// Initializes a decoder whose image and observer is already being used by a
// parent decoder
void
Decoder::InitSharedDecoder(uint8_t* imageData, uint32_t imageDataLength,
                           uint32_t* colormap, uint32_t colormapSize,
                           imgFrame* currentFrame)
{
  // No re-initializing
  NS_ABORT_IF_FALSE(!mInitialized, "Can't re-initialize a decoder!");
  NS_ABORT_IF_FALSE(mObserver, "Need an observer!");

  mImageData = imageData;
  mImageDataLength = imageDataLength;
  mColormap = colormap;
  mColormapSize = colormapSize;
  mCurrentFrame = currentFrame;
  // We have all the frame data, so we've started the frame.
  if (!IsSizeDecode()) {
    PostFrameStart();
  }

  // Implementation-specific initialization
  InitInternal();
  mInitialized = true;
}

void
Decoder::Write(const char* aBuffer, uint32_t aCount)
{
  PROFILER_LABEL("ImageDecoder", "Write");

  // We're strict about decoder errors
  NS_ABORT_IF_FALSE(!HasDecoderError(),
                    "Not allowed to make more decoder calls after error!");

  // If a data error occured, just ignore future data
  if (HasDataError())
    return;

  if (IsSizeDecode() && HasSize()) {
    // More data came in since we found the size. We have nothing to do here.
    return;
  }

  // Pass the data along to the implementation
  WriteInternal(aBuffer, aCount);

  // If we're a synchronous decoder and we need a new frame to proceed, let's
  // create one and call it again.
  while (mSynchronous && NeedsNewFrame() && !HasDataError()) {
    nsresult rv = AllocateFrame();

    if (NS_SUCCEEDED(rv)) {
      // Tell the decoder to use the data it saved when it asked for a new frame.
      WriteInternal(nullptr, 0);
    }
  }
}

void
Decoder::Finish(RasterImage::eShutdownIntent aShutdownIntent)
{
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

    bool usable = true;
    if (aShutdownIntent != RasterImage::eShutdownIntent_NotNeeded && !HasDecoderError()) {
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
      if (mObserver) {
        mObserver->OnStopDecode(NS_ERROR_FAILURE);
      }
    }
  }

  // Set image metadata before calling DecodingComplete, because DecodingComplete calls Optimize().
  mImageMetadata.SetOnImage(&mImage);

  if (mDecodeDone) {
    mImage.DecodingComplete();
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
Decoder::AllocateFrame()
{
  MOZ_ASSERT(mNeedsNewFrame);
  MOZ_ASSERT(NS_IsMainThread());

  MarkFrameDirty();

  nsresult rv;
  imgFrame* frame = nullptr;
  if (mNewFrameData.mPaletteDepth) {
    rv = mImage.EnsureFrame(mNewFrameData.mFrameNum, mNewFrameData.mOffsetX,
                            mNewFrameData.mOffsetY, mNewFrameData.mWidth,
                            mNewFrameData.mHeight, mNewFrameData.mFormat,
                            mNewFrameData.mPaletteDepth,
                            &mImageData, &mImageDataLength,
                            &mColormap, &mColormapSize, &frame);
  } else {
    rv = mImage.EnsureFrame(mNewFrameData.mFrameNum, mNewFrameData.mOffsetX,
                            mNewFrameData.mOffsetY, mNewFrameData.mWidth,
                            mNewFrameData.mHeight, mNewFrameData.mFormat,
                            &mImageData, &mImageDataLength, &frame);
  }

  if (NS_SUCCEEDED(rv)) {
    mCurrentFrame = frame;
  } else {
    mCurrentFrame = nullptr;
  }

  // Notify if appropriate
  if (NS_SUCCEEDED(rv) && mNewFrameData.mFrameNum == mFrameCount) {
    PostFrameStart();
  } else if (NS_FAILED(rv)) {
    PostDataError();
  }

  // Mark ourselves as not needing another frame before talking to anyone else
  // so they can tell us if they need yet another.
  mNeedsNewFrame = false;

  return rv;
}

void
Decoder::FlushInvalidations()
{
  NS_ABORT_IF_FALSE(!HasDecoderError(),
                    "Not allowed to make more decoder calls after error!");

  // If we've got an empty invalidation rect, we have nothing to do
  if (mInvalidRect.IsEmpty())
    return;

  if (mObserver) {
#ifdef XP_MACOSX
    // Bug 703231
    // Because of high quality down sampling on mac we show scan lines while decoding.
    // Bypass this problem by redrawing the border.
    if (mImageMetadata.HasSize()) {
      nsIntRect mImageBound(0, 0, mImageMetadata.GetWidth(), mImageMetadata.GetHeight());

      mInvalidRect.Inflate(1);
      mInvalidRect = mInvalidRect.Intersect(mImageBound);
    }
#endif
    mObserver->FrameChanged(&mInvalidRect);
  }

  // Clear the invalidation rectangle
  mInvalidRect.SetEmpty();
}

void
Decoder::SetSizeOnImage()
{
  mImage.SetSize(mImageMetadata.GetWidth(), mImageMetadata.GetHeight());
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
Decoder::PostSize(int32_t aWidth, int32_t aHeight)
{
  // Validate
  NS_ABORT_IF_FALSE(aWidth >= 0, "Width can't be negative!");
  NS_ABORT_IF_FALSE(aHeight >= 0, "Height can't be negative!");

  // Tell the image
  mImageMetadata.SetSize(aWidth, aHeight);

  // Notify the observer
  if (mObserver)
    mObserver->OnStartContainer();
}

void
Decoder::PostFrameStart()
{
  // We shouldn't already be mid-frame
  NS_ABORT_IF_FALSE(!mInFrame, "Starting new frame but not done with old one!");

  // We should take care of any invalidation region when wrapping up the
  // previous frame
  NS_ABORT_IF_FALSE(mInvalidRect.IsEmpty(),
                    "Start image frame with non-empty invalidation region!");

  // Update our state to reflect the new frame
  mFrameCount++;
  mInFrame = true;

  // Decoder implementations should only call this method if they successfully
  // appended the frame to the image. So mFrameCount should always match that
  // reported by the Image.
  NS_ABORT_IF_FALSE(mFrameCount == mImage.GetNumFrames(),
                    "Decoder frame count doesn't match image's!");

  // Fire notifications
  if (mObserver) {
    mObserver->OnStartFrame();
  }
}

void
Decoder::PostFrameStop(FrameBlender::FrameAlpha aFrameAlpha /* = FrameBlender::kFrameHasAlpha */,
                       FrameBlender::FrameDisposalMethod aDisposalMethod /* = FrameBlender::kDisposeKeep */,
                       int32_t aTimeout /* = 0 */,
                       FrameBlender::FrameBlendMethod aBlendMethod /* = FrameBlender::kBlendOver */)
{
  // We should be mid-frame
  NS_ABORT_IF_FALSE(mInFrame, "Stopping frame when we didn't start one!");
  NS_ABORT_IF_FALSE(mCurrentFrame, "Stopping frame when we don't have one!");

  // Update our state
  mInFrame = false;

  if (aFrameAlpha == FrameBlender::kFrameOpaque) {
    mCurrentFrame->SetHasNoAlpha();
  }

  mCurrentFrame->SetFrameDisposalMethod(aDisposalMethod);
  mCurrentFrame->SetTimeout(aTimeout);
  mCurrentFrame->SetBlendMethod(aBlendMethod);
  mCurrentFrame->ImageUpdated(mCurrentFrame->GetRect());

  // Flush any invalidations before we finish the frame
  FlushInvalidations();

  // Fire notifications
  if (mObserver) {
    mObserver->OnStopFrame();
    if (mFrameCount > 1 && !mIsAnimated) {
      mIsAnimated = true;
      mObserver->OnImageIsAnimated();
    }
  }
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
  mImageMetadata.SetIsNonPremultiplied(GetDecodeFlags() & DECODER_NO_PREMULTIPLY_ALPHA);

  if (mObserver) {
    mObserver->OnStopDecode(NS_OK);
  }
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
                      gfxASurface::gfxImageFormat format,
                      uint8_t palette_depth /* = 0 */)
{
  // Decoders should never call NeedNewFrame without yielding back to Write().
  MOZ_ASSERT(!mNeedsNewFrame);

  // We don't want images going back in time or skipping frames.
  MOZ_ASSERT(framenum == mFrameCount || framenum == (mFrameCount - 1));

  mNewFrameData = NewFrameData(framenum, x_offset, y_offset, width, height, format, palette_depth);
  mNeedsNewFrame = true;
}

void
Decoder::MarkFrameDirty()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mCurrentFrame) {
    mCurrentFrame->ApplyDirtToSurfaces();
  }
}

} // namespace image
} // namespace mozilla
