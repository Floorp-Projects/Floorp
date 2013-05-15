/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageEncoder.h"

#include "mozilla/dom/CanvasRenderingContext2D.h"

namespace mozilla {
namespace dom {

class EncodingCompleteEvent : public nsRunnable
{
public:
  NS_DECL_ISUPPORTS

  EncodingCompleteEvent(JSContext* aJSContext,
                        nsIThread* aEncoderThread,
                        nsIFileCallback* aCallback)
    : mImgData(nullptr)
    , mImgSize(0)
    , mType()
    , mJSContext(aJSContext)
    , mEncoderThread(aEncoderThread)
    , mCallback(aCallback)
  {}
  virtual ~EncodingCompleteEvent() {}

  NS_IMETHOD Run()
  {
    nsRefPtr<nsDOMMemoryFile> blob =
      new nsDOMMemoryFile(mImgData, mImgSize, mType);

    if (mJSContext) {
      JS_updateMallocCounter(mJSContext, mImgSize);
    }
    nsresult rv = mCallback->Receive(blob);
    NS_ENSURE_SUCCESS(rv, rv);

    mEncoderThread->Shutdown();
    return rv;
  }

  void SetMembers(void* aImgData, uint64_t aImgSize, const nsAutoString& aType)
  {
    mImgData = aImgData;
    mImgSize = aImgSize;
    mType = aType;
  }

private:
  void* mImgData;
  uint64_t mImgSize;
  nsAutoString mType;
  JSContext* mJSContext;
  nsCOMPtr<nsIThread> mEncoderThread;
  nsCOMPtr<nsIFileCallback> mCallback;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(EncodingCompleteEvent, nsIRunnable)

class EncodingRunnable : public nsRunnable
{
public:
  NS_DECL_ISUPPORTS

  EncodingRunnable(const nsAString& aType,
                   const nsAString& aOptions,
                   uint8_t* aImageBuffer,
                   const nsIntSize aSize,
                   imgIEncoder* aEncoder,
                   imgIEncoder* aFallbackEncoder,
                   nsIThread* aOriginThread,
                   EncodingCompleteEvent* aEncodingCompleteEvent)
    : mType(aType)
    , mOptions(aOptions)
    , mImageBuffer(aImageBuffer)
    , mSize(aSize)
    , mEncoder(aEncoder)
    , mFallbackEncoder(aFallbackEncoder)
    , mOriginThread(aOriginThread)
    , mEncodingCompleteEvent(aEncodingCompleteEvent)
  {}
  virtual ~EncodingRunnable() {}

  NS_IMETHOD Run()
  {
    bool fellbackToPNG = false;

    nsCOMPtr<nsIInputStream> stream;
    nsresult rv = ImageEncoder::ExtractDataInternal(mType,
                                                    EmptyString(),
                                                    mImageBuffer,
                                                    mSize,
                                                    nullptr,
                                                    getter_AddRefs(stream),
                                                    fellbackToPNG,
                                                    mEncoder,
                                                    mFallbackEncoder);
    NS_ENSURE_SUCCESS(rv, rv);

    if (fellbackToPNG) {
      mType.AssignLiteral("image/png");
    }

    uint64_t imgSize;
    rv = stream->Available(&imgSize);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(imgSize <= UINT32_MAX, NS_ERROR_FILE_TOO_BIG);

    void* imgData = nullptr;
    rv = NS_ReadInputStreamToBuffer(stream, &imgData, imgSize);
    NS_ENSURE_SUCCESS(rv, rv);

    mEncodingCompleteEvent->SetMembers(imgData, imgSize, mType);
    rv = mOriginThread->Dispatch(mEncodingCompleteEvent, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);

    return rv;
  }

private:
  nsAutoString mType;
  nsAutoString mOptions;
  nsAutoArrayPtr<uint8_t> mImageBuffer;
  const nsIntSize mSize;
  nsCOMPtr<imgIEncoder> mEncoder;
  nsCOMPtr<imgIEncoder> mFallbackEncoder;
  nsCOMPtr<nsIThread> mOriginThread;
  nsRefPtr<EncodingCompleteEvent> mEncodingCompleteEvent;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(EncodingRunnable, nsIRunnable)

/* static */
nsresult
ImageEncoder::ExtractData(const nsAString& aType,
                          const nsAString& aOptions,
                          uint8_t* aImageBuffer,
                          const nsIntSize aSize,
                          nsICanvasRenderingContextInternal* aContext,
                          nsIInputStream** aStream,
                          bool& aFellBackToPNG)
{
  nsCOMPtr<imgIEncoder> encoder;
  nsCOMPtr<imgIEncoder> fallbackEncoder;
  nsresult rv = ImageEncoder::GetImageEncoders(aType,
                                               getter_AddRefs(encoder),
                                               getter_AddRefs(fallbackEncoder));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ExtractDataInternal(aType, aOptions, aImageBuffer, aSize, aContext,
                           aStream, aFellBackToPNG, encoder, fallbackEncoder);
  return rv;
}

/* static */
nsresult
ImageEncoder::ExtractDataAsync(const nsAString& aType,
                               const nsAString& aOptions,
                               uint8_t* aImageBuffer,
                               const nsIntSize aSize,
                               nsICanvasRenderingContextInternal* aContext,
                               JSContext* aJSContext,
                               nsIFileCallback* aCallback)
{
  nsCOMPtr<nsIThread> encoderThread;
  nsresult rv = NS_NewThread(getter_AddRefs(encoderThread), nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<EncodingCompleteEvent> completeEvent =
    new EncodingCompleteEvent(aJSContext, encoderThread, aCallback);
  nsCOMPtr<imgIEncoder> encoder;
  nsCOMPtr<imgIEncoder> fallbackEncoder;
  rv = ImageEncoder::GetImageEncoders(aType,
                                      getter_AddRefs(encoder),
                                      getter_AddRefs(fallbackEncoder));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRunnable> event = new EncodingRunnable(aType,
                                                     aOptions,
                                                     aImageBuffer,
                                                     aSize,
                                                     encoder,
                                                     fallbackEncoder,
                                                     NS_GetCurrentThread(),
                                                     completeEvent);
  return encoderThread->Dispatch(event, NS_DISPATCH_NORMAL);
}

/* static */
nsresult
ImageEncoder::ExtractDataInternal(const nsAString& aType,
                                  const nsAString& aOptions,
                                  uint8_t* aImageBuffer,
                                  const nsIntSize aSize,
                                  nsICanvasRenderingContextInternal* aContext,
                                  nsIInputStream** aStream,
                                  bool& aFellBackToPNG,
                                  imgIEncoder* aEncoder,
                                  imgIEncoder* aFallbackEncoder)
{
  nsCOMPtr<nsICanvasRenderingContextInternal> context = aContext;
  nsRefPtr<gfxImageSurface> emptyCanvas;
  nsresult rv;

  nsCOMPtr<nsIInputStream> imgStream;

  nsCOMPtr<imgIEncoder> currentEncoder(aEncoder);
  if (!currentEncoder) {
    aFellBackToPNG = true;
    currentEncoder = aFallbackEncoder;
  }

  // get image bytes
  bool gotImageBytes = false;
  while (!gotImageBytes) {
    if (aImageBuffer) {
      rv = CanvasRenderingContext2D::GetInputStream(
        aSize.width,
        aSize.height,
        aImageBuffer,
        currentEncoder,
        nsPromiseFlatString(aOptions).get(),
        getter_AddRefs(imgStream));
    } else if (context) {
      NS_ConvertUTF16toUTF8 encoderType(aType);
      context->GetInputStream(aFellBackToPNG ? "image/png" : encoderType.get(),
                              nsPromiseFlatString(aOptions).get(),
                              getter_AddRefs(imgStream));
    } else {
      // no context, so we have to encode an empty image
      // note that if we didn't have a current context, the spec says we're
      // supposed to just return transparent black pixels of the canvas
      // dimensions.
      if (!emptyCanvas) {
        emptyCanvas =
          new gfxImageSurface(gfxIntSize(aSize.width, aSize.height),
                              gfxASurface::ImageFormatARGB32);
        if (emptyCanvas->CairoStatus()) {
          return NS_ERROR_INVALID_ARG;
        }
      }
      rv = currentEncoder->InitFromData(emptyCanvas->Data(),
                                        aSize.width * aSize.height * 4,
                                        aSize.width,
                                        aSize.height,
                                        aSize.width * 4,
                                        imgIEncoder::INPUT_FORMAT_HOSTARGB,
                                        aOptions);
      if (NS_SUCCEEDED(rv)) {
        imgStream = do_QueryInterface(currentEncoder);
      }
    }

    if (NS_FAILED(rv) && !aFellBackToPNG) {
      // Try image/png instead.
      // XXX ERRMSG we need to report an error to developers here! (bug 329026)
      aFellBackToPNG = true;
      currentEncoder = aFallbackEncoder;
    } else {
      gotImageBytes = true;
    }
  }

  NS_ENSURE_SUCCESS(rv, rv);

  imgStream.forget(aStream);
  return NS_OK;
}

/* static */
nsresult
ImageEncoder::GetImageEncoders(const nsAString& aType,
                               imgIEncoder** aEncoder,
                               imgIEncoder** aFallbackEncoder)
{
  // Get an image encoder for the media type.
  nsCString encoderCID("@mozilla.org/image/encoder;2?type=");
  NS_ConvertUTF16toUTF8 encoderType(aType);
  encoderCID += encoderType;
  nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(encoderCID.get());
  if (encoder) {
    NS_ADDREF(*aEncoder = encoder);
  }

  // Get the fallback encoder.
  nsCString fallbackEncoderCID("@mozilla.org/image/encoder;2?type=image/png");
  nsCOMPtr<imgIEncoder> fallbackEncoder =
    do_CreateInstance(fallbackEncoderCID.get());
  if (fallbackEncoder) {
    NS_ADDREF(*aFallbackEncoder = fallbackEncoder);
  }

  if (!encoder && !fallbackEncoder) {
    return NS_IMAGELIB_ERROR_NO_ENCODER;
  }
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
