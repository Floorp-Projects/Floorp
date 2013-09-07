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
  NS_DECL_THREADSAFE_ISUPPORTS

  EncodingCompleteEvent(JSContext* aJSContext,
                        nsIThread* aEncoderThread,
                        nsIFileCallback* aCallback)
    : mImgSize(0)
    , mType()
    , mImgData(nullptr)
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
  uint64_t mImgSize;
  nsAutoString mType;
  void* mImgData;
  JSContext* mJSContext;
  nsCOMPtr<nsIThread> mEncoderThread;
  nsCOMPtr<nsIFileCallback> mCallback;
};

NS_IMPL_ISUPPORTS1(EncodingCompleteEvent, nsIRunnable);

class EncodingRunnable : public nsRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  EncodingRunnable(const nsAString& aType,
                   const nsAString& aOptions,
                   uint8_t* aImageBuffer,
                   imgIEncoder* aEncoder,
                   nsIThread* aOriginThread,
                   EncodingCompleteEvent* aEncodingCompleteEvent,
                   int32_t aFormat,
                   const nsIntSize aSize,
                   bool aUsingCustomOptions)
    : mType(aType)
    , mOptions(aOptions)
    , mImageBuffer(aImageBuffer)
    , mEncoder(aEncoder)
    , mOriginThread(aOriginThread)
    , mEncodingCompleteEvent(aEncodingCompleteEvent)
    , mFormat(aFormat)
    , mSize(aSize)
    , mUsingCustomOptions(aUsingCustomOptions)
  {}
  virtual ~EncodingRunnable() {}

  NS_IMETHOD Run()
  {
    nsCOMPtr<nsIInputStream> stream;
    nsresult rv = ImageEncoder::ExtractDataInternal(mType,
                                                    mOptions,
                                                    mImageBuffer,
                                                    mFormat,
                                                    mSize,
                                                    nullptr,
                                                    getter_AddRefs(stream),
                                                    mEncoder);

    // If there are unrecognized custom parse options, we should fall back to
    // the default values for the encoder without any options at all.
    if (rv == NS_ERROR_INVALID_ARG && mUsingCustomOptions) {
      rv = ImageEncoder::ExtractDataInternal(mType,
                                             EmptyString(),
                                             mImageBuffer,
                                             mFormat,
                                             mSize,
                                             nullptr,
                                             getter_AddRefs(stream),
                                             mEncoder);
    }
    NS_ENSURE_SUCCESS(rv, rv);

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
  nsCOMPtr<imgIEncoder> mEncoder;
  nsCOMPtr<nsIThread> mOriginThread;
  nsRefPtr<EncodingCompleteEvent> mEncodingCompleteEvent;
  int32_t mFormat;
  const nsIntSize mSize;
  bool mUsingCustomOptions;
};

NS_IMPL_ISUPPORTS1(EncodingRunnable, nsIRunnable)

/* static */
nsresult
ImageEncoder::ExtractData(nsAString& aType,
                          const nsAString& aOptions,
                          const nsIntSize aSize,
                          nsICanvasRenderingContextInternal* aContext,
                          nsIInputStream** aStream)
{
  nsCOMPtr<imgIEncoder> encoder = ImageEncoder::GetImageEncoder(aType);
  if (!encoder) {
    return NS_IMAGELIB_ERROR_NO_ENCODER;
  }

  return ExtractDataInternal(aType, aOptions, nullptr, 0, aSize, aContext,
                             aStream, encoder);
}

/* static */
nsresult
ImageEncoder::ExtractDataAsync(nsAString& aType,
                               const nsAString& aOptions,
                               bool aUsingCustomOptions,
                               uint8_t* aImageBuffer,
                               int32_t aFormat,
                               const nsIntSize aSize,
                               nsICanvasRenderingContextInternal* aContext,
                               JSContext* aJSContext,
                               nsIFileCallback* aCallback)
{
  nsCOMPtr<imgIEncoder> encoder = ImageEncoder::GetImageEncoder(aType);
  if (!encoder) {
    return NS_IMAGELIB_ERROR_NO_ENCODER;
  }

  nsCOMPtr<nsIThread> encoderThread;
  nsresult rv = NS_NewThread(getter_AddRefs(encoderThread), nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<EncodingCompleteEvent> completeEvent =
    new EncodingCompleteEvent(aJSContext, encoderThread, aCallback);

  nsCOMPtr<nsIRunnable> event = new EncodingRunnable(aType,
                                                     aOptions,
                                                     aImageBuffer,
                                                     encoder,
                                                     NS_GetCurrentThread(),
                                                     completeEvent,
                                                     aFormat,
                                                     aSize,
                                                     aUsingCustomOptions);
  return encoderThread->Dispatch(event, NS_DISPATCH_NORMAL);
}

/*static*/ nsresult
ImageEncoder::GetInputStream(int32_t aWidth,
                             int32_t aHeight,
                             uint8_t* aImageBuffer,
                             int32_t aFormat,
                             imgIEncoder* aEncoder,
                             const PRUnichar* aEncoderOptions,
                             nsIInputStream** aStream)
{
  nsresult rv =
    aEncoder->InitFromData(aImageBuffer,
                           aWidth * aHeight * 4, aWidth, aHeight, aWidth * 4,
                           aFormat,
                           nsDependentString(aEncoderOptions));
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(aEncoder, aStream);
}

/* static */
nsresult
ImageEncoder::ExtractDataInternal(const nsAString& aType,
                                  const nsAString& aOptions,
                                  uint8_t* aImageBuffer,
                                  int32_t aFormat,
                                  const nsIntSize aSize,
                                  nsICanvasRenderingContextInternal* aContext,
                                  nsIInputStream** aStream,
                                  imgIEncoder* aEncoder)
{
  nsCOMPtr<nsIInputStream> imgStream;

  // get image bytes
  nsresult rv;
  if (aImageBuffer) {
    rv = ImageEncoder::GetInputStream(
      aSize.width,
      aSize.height,
      aImageBuffer,
      aFormat,
      aEncoder,
      nsPromiseFlatString(aOptions).get(),
      getter_AddRefs(imgStream));
  } else if (aContext) {
    NS_ConvertUTF16toUTF8 encoderType(aType);
    rv = aContext->GetInputStream(encoderType.get(),
                                  nsPromiseFlatString(aOptions).get(),
                                  getter_AddRefs(imgStream));
  } else {
    // no context, so we have to encode an empty image
    // note that if we didn't have a current context, the spec says we're
    // supposed to just return transparent black pixels of the canvas
    // dimensions.
    nsRefPtr<gfxImageSurface> emptyCanvas =
      new gfxImageSurface(gfxIntSize(aSize.width, aSize.height),
                          gfxASurface::ImageFormatARGB32);
    if (emptyCanvas->CairoStatus()) {
      return NS_ERROR_INVALID_ARG;
    }
    rv = aEncoder->InitFromData(emptyCanvas->Data(),
                                aSize.width * aSize.height * 4,
                                aSize.width,
                                aSize.height,
                                aSize.width * 4,
                                imgIEncoder::INPUT_FORMAT_HOSTARGB,
                                aOptions);
    if (NS_SUCCEEDED(rv)) {
      imgStream = do_QueryInterface(aEncoder);
    }
  }
  NS_ENSURE_SUCCESS(rv, rv);

  imgStream.forget(aStream);
  return rv;
}

/* static */
already_AddRefed<imgIEncoder>
ImageEncoder::GetImageEncoder(nsAString& aType)
{
  // Get an image encoder for the media type.
  nsCString encoderCID("@mozilla.org/image/encoder;2?type=");
  NS_ConvertUTF16toUTF8 encoderType(aType);
  encoderCID += encoderType;
  nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(encoderCID.get());

  if (!encoder && aType != NS_LITERAL_STRING("image/png")) {
    // Unable to create an encoder instance of the specified type. Falling back
    // to PNG.
    aType.AssignLiteral("image/png");
    nsCString PNGEncoderCID("@mozilla.org/image/encoder;2?type=image/png");
    encoder = do_CreateInstance(PNGEncoderCID.get());
  }

  return encoder.forget();
}

} // namespace dom
} // namespace mozilla
