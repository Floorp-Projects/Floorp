/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// #include "ImageLogging.h"
#include "nsCRT.h"
#include "nsWebPEncoder.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "prprf.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/UniquePtrExtensions.h"

using namespace mozilla;

// static LazyLogModule sWEBPEncoderLog("WEBPEncoder");

NS_IMPL_ISUPPORTS(nsWebPEncoder, imgIEncoder, nsIInputStream,
                  nsIAsyncInputStream)

nsWebPEncoder::nsWebPEncoder()
    : mFinished(false),
      mImageBuffer(nullptr),
      mImageBufferSize(0),
      mImageBufferUsed(0),
      mImageBufferReadPoint(0),
      mCallback(nullptr),
      mCallbackTarget(nullptr),
      mNotifyThreshold(0),
      mReentrantMonitor("nsWebPEncoder.mReentrantMonitor") {}

nsWebPEncoder::~nsWebPEncoder() {
  if (mImageBuffer) {
    WebPFree(mImageBuffer);
    mImageBuffer = nullptr;
    mImageBufferSize = 0;
    mImageBufferUsed = 0;
    mImageBufferReadPoint = 0;
  }
}

// nsWebPEncoder::InitFromData
//
//    One output option is supported: "quality=X" where X is an integer in the
//    range 0-100. Higher values for X give better quality.

NS_IMETHODIMP
nsWebPEncoder::InitFromData(const uint8_t* aData,
                            uint32_t aLength,  // (unused, req'd by JS)
                            uint32_t aWidth, uint32_t aHeight, uint32_t aStride,
                            uint32_t aInputFormat,
                            const nsAString& aOutputOptions) {
  NS_ENSURE_ARG(aData);

  // validate input format
  if (aInputFormat != INPUT_FORMAT_RGB && aInputFormat != INPUT_FORMAT_RGBA &&
      aInputFormat != INPUT_FORMAT_HOSTARGB)
    return NS_ERROR_INVALID_ARG;

  // Stride is the padded width of each row, so it better be longer (I'm afraid
  // people will not understand what stride means, so check it well)
  if ((aInputFormat == INPUT_FORMAT_RGB && aStride < aWidth * 3) ||
      ((aInputFormat == INPUT_FORMAT_RGBA ||
        aInputFormat == INPUT_FORMAT_HOSTARGB) &&
       aStride < aWidth * 4)) {
    NS_WARNING("Invalid stride for InitFromData");
    return NS_ERROR_INVALID_ARG;
  }

  // can't initialize more than once
  if (mImageBuffer != nullptr) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  // options: we only have one option so this is easy
  int quality = 92;
  if (aOutputOptions.Length() > 0) {
    // have options string
    const nsString qualityPrefix(u"quality="_ns);
    if (aOutputOptions.Length() > qualityPrefix.Length() &&
        StringBeginsWith(aOutputOptions, qualityPrefix)) {
      // have quality string
      nsCString value = NS_ConvertUTF16toUTF8(
          Substring(aOutputOptions, qualityPrefix.Length()));
      int newquality = -1;
      if (PR_sscanf(value.get(), "%d", &newquality) == 1) {
        if (newquality >= 0 && newquality <= 100) {
          quality = newquality;
        } else {
          NS_WARNING(
              "Quality value out of range, should be 0-100,"
              " using default");
        }
      } else {
        NS_WARNING(
            "Quality value invalid, should be integer 0-100,"
            " using default");
      }
    } else {
      return NS_ERROR_INVALID_ARG;
    }
  }

  size_t size = 0;

  CheckedInt32 width = CheckedInt32(aWidth);
  CheckedInt32 height = CheckedInt32(aHeight);
  CheckedInt32 stride = CheckedInt32(aStride);
  if (!width.isValid() || !height.isValid() || !stride.isValid() ||
      !(CheckedUint32(aStride) * CheckedUint32(aHeight)).isValid()) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aInputFormat == INPUT_FORMAT_RGB) {
    size = quality == 100
               ? WebPEncodeLosslessRGB(aData, width.value(), height.value(),
                                       stride.value(), &mImageBuffer)
               : WebPEncodeRGB(aData, width.value(), height.value(),
                               stride.value(), quality, &mImageBuffer);
  } else if (aInputFormat == INPUT_FORMAT_RGBA) {
    size = quality == 100
               ? WebPEncodeLosslessRGBA(aData, width.value(), height.value(),
                                        stride.value(), &mImageBuffer)
               : WebPEncodeRGBA(aData, width.value(), height.value(),
                                stride.value(), quality, &mImageBuffer);
  } else if (aInputFormat == INPUT_FORMAT_HOSTARGB) {
    UniquePtr<uint8_t[]> aDest =
        MakeUniqueFallible<uint8_t[]>(aStride * aHeight);
    if (NS_WARN_IF(!aDest)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t y = 0; y < aHeight; y++) {
      for (uint32_t x = 0; x < aWidth; x++) {
        const uint32_t& pixelIn =
            ((const uint32_t*)(aData))[y * aStride / 4 + x];
        uint8_t* pixelOut = &aDest[y * aStride + x * 4];

        uint8_t alpha = (pixelIn & 0xff000000) >> 24;
        pixelOut[3] = alpha;
        if (alpha == 255) {
          pixelOut[0] = (pixelIn & 0xff0000) >> 16;
          pixelOut[1] = (pixelIn & 0x00ff00) >> 8;
          pixelOut[2] = (pixelIn & 0x0000ff);
        } else if (alpha == 0) {
          pixelOut[0] = pixelOut[1] = pixelOut[2] = 0;
        } else {
          pixelOut[0] =
              (((pixelIn & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
          pixelOut[1] = (((pixelIn & 0x00ff00) >> 8) * 255 + alpha / 2) / alpha;
          pixelOut[2] = (((pixelIn & 0x0000ff)) * 255 + alpha / 2) / alpha;
        }
      }
    }

    size =
        quality == 100
            ? WebPEncodeLosslessRGBA(aDest.get(), width.value(), height.value(),
                                     stride.value(), &mImageBuffer)
            : WebPEncodeRGBA(aDest.get(), width.value(), height.value(),
                             stride.value(), quality, &mImageBuffer);
  }

  mFinished = true;

  if (size == 0) {
    return NS_ERROR_FAILURE;
  }

  mImageBufferUsed = size;

  return NS_OK;
}

// nsWebPEncoder::StartImageEncode
//
//
// See ::InitFromData for other info.
NS_IMETHODIMP
nsWebPEncoder::StartImageEncode(uint32_t aWidth, uint32_t aHeight,
                                uint32_t aInputFormat,
                                const nsAString& aOutputOptions) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Returns the number of bytes in the image buffer used.
NS_IMETHODIMP
nsWebPEncoder::GetImageBufferUsed(uint32_t* aOutputSize) {
  NS_ENSURE_ARG_POINTER(aOutputSize);
  *aOutputSize = mImageBufferUsed;
  return NS_OK;
}

// Returns a pointer to the start of the image buffer
NS_IMETHODIMP
nsWebPEncoder::GetImageBuffer(char** aOutputBuffer) {
  NS_ENSURE_ARG_POINTER(aOutputBuffer);
  *aOutputBuffer = reinterpret_cast<char*>(mImageBuffer);
  return NS_OK;
}

NS_IMETHODIMP
nsWebPEncoder::AddImageFrame(const uint8_t* aData,
                             uint32_t aLength,  // (unused, req'd by JS)
                             uint32_t aWidth, uint32_t aHeight,
                             uint32_t aStride, uint32_t aInputFormat,
                             const nsAString& aFrameOptions) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebPEncoder::EndImageEncode() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
nsWebPEncoder::Close() {
  if (mImageBuffer) {
    WebPFree(mImageBuffer);
    mImageBuffer = nullptr;
    mImageBufferSize = 0;
    mImageBufferUsed = 0;
    mImageBufferReadPoint = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebPEncoder::Available(uint64_t* _retval) {
  if (!mImageBuffer) {
    return NS_BASE_STREAM_CLOSED;
  }

  *_retval = mImageBufferUsed - mImageBufferReadPoint;
  return NS_OK;
}

NS_IMETHODIMP
nsWebPEncoder::StreamStatus() {
  return mImageBuffer ? NS_OK : NS_BASE_STREAM_CLOSED;
}

NS_IMETHODIMP
nsWebPEncoder::Read(char* aBuf, uint32_t aCount, uint32_t* _retval) {
  return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, _retval);
}

NS_IMETHODIMP
nsWebPEncoder::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                            uint32_t aCount, uint32_t* _retval) {
  // Avoid another thread reallocing the buffer underneath us
  ReentrantMonitorAutoEnter autoEnter(mReentrantMonitor);

  uint32_t maxCount = mImageBufferUsed - mImageBufferReadPoint;
  if (maxCount == 0) {
    *_retval = 0;
    return mFinished ? NS_OK : NS_BASE_STREAM_WOULD_BLOCK;
  }

  if (aCount > maxCount) {
    aCount = maxCount;
  }
  nsresult rv = aWriter(
      this, aClosure,
      reinterpret_cast<const char*>(mImageBuffer + mImageBufferReadPoint), 0,
      aCount, _retval);
  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(*_retval <= aCount, "bad write count");
    mImageBufferReadPoint += *_retval;
  }

  // errors returned from the writer end here!
  return NS_OK;
}

NS_IMETHODIMP
nsWebPEncoder::IsNonBlocking(bool* _retval) {
  *_retval = true;
  return NS_OK;
}

NS_IMETHODIMP
nsWebPEncoder::AsyncWait(nsIInputStreamCallback* aCallback, uint32_t aFlags,
                         uint32_t aRequestedCount, nsIEventTarget* aTarget) {
  if (aFlags != 0) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (mCallback || mCallbackTarget) {
    return NS_ERROR_UNEXPECTED;
  }

  mCallbackTarget = aTarget;
  // 0 means "any number of bytes except 0"
  mNotifyThreshold = aRequestedCount;
  if (!aRequestedCount) {
    mNotifyThreshold = 1024;  // 1 KB seems good.  We don't want to
                              // notify incessantly
  }

  // We set the callback absolutely last, because NotifyListener uses it to
  // determine if someone needs to be notified.  If we don't set it last,
  // NotifyListener might try to fire off a notification to a null target
  // which will generally cause non-threadsafe objects to be used off the
  // main thread
  mCallback = aCallback;

  // What we are being asked for may be present already
  NotifyListener();
  return NS_OK;
}

NS_IMETHODIMP
nsWebPEncoder::CloseWithStatus(nsresult aStatus) { return Close(); }

void nsWebPEncoder::NotifyListener() {
  // We might call this function on multiple threads (any threads that call
  // AsyncWait and any that do encoding) so we lock to avoid notifying the
  // listener twice about the same data (which generally leads to a truncated
  // image).
  ReentrantMonitorAutoEnter autoEnter(mReentrantMonitor);

  if (mCallback &&
      (mImageBufferUsed - mImageBufferReadPoint >= mNotifyThreshold ||
       mFinished)) {
    nsCOMPtr<nsIInputStreamCallback> callback;
    if (mCallbackTarget) {
      callback = NS_NewInputStreamReadyEvent("nsWebPEncoder::NotifyListener",
                                             mCallback, mCallbackTarget);
    } else {
      callback = mCallback;
    }

    NS_ASSERTION(callback, "Shouldn't fail to make the callback");
    // Null the callback first because OnInputStreamReady could reenter
    // AsyncWait
    mCallback = nullptr;
    mCallbackTarget = nullptr;
    mNotifyThreshold = 0;

    callback->OnInputStreamReady(this);
  }
}
