/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageEncoder.h"
#include "mozilla/dom/CanvasRenderingContext2D.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/RefPtr.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

class EncodingCompleteEvent : public nsRunnable
{
  virtual ~EncodingCompleteEvent() {}

public:
  NS_DECL_ISUPPORTS_INHERITED

  EncodingCompleteEvent(nsIGlobalObject* aGlobal,
                        nsIThread* aEncoderThread,
                        FileCallback& aCallback)
    : mImgSize(0)
    , mType()
    , mImgData(nullptr)
    , mGlobal(aGlobal)
    , mEncoderThread(aEncoderThread)
    , mCallback(&aCallback)
    , mFailed(false)
  {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    mozilla::ErrorResult rv;

    if (!mFailed) {
      nsRefPtr<DOMFile> blob =
        DOMFile::CreateMemoryFile(mImgData, mImgSize, mType);

      {
        AutoJSAPI jsapi;
        jsapi.Init(mGlobal);
        JS_updateMallocCounter(jsapi.cx(), mImgSize);
      }

      mCallback->Call(blob, rv);
    }

    // These members aren't thread-safe. We're making sure that they're being
    // released on the main thread here. Otherwise, they could be getting
    // released by EncodingRunnable's destructor on the encoding thread
    // (bug 916128).
    mGlobal = nullptr;
    mCallback = nullptr;

    mEncoderThread->Shutdown();
    return rv.ErrorCode();
  }

  void SetMembers(void* aImgData, uint64_t aImgSize, const nsAutoString& aType)
  {
    mImgData = aImgData;
    mImgSize = aImgSize;
    mType = aType;
  }

  void SetFailed()
  {
    mFailed = true;
  }

private:
  uint64_t mImgSize;
  nsAutoString mType;
  void* mImgData;
  nsCOMPtr<nsIGlobalObject> mGlobal;
  nsCOMPtr<nsIThread> mEncoderThread;
  nsRefPtr<FileCallback> mCallback;
  bool mFailed;
};

NS_IMPL_ISUPPORTS_INHERITED0(EncodingCompleteEvent, nsRunnable);

class EncodingRunnable : public nsRunnable
{
  virtual ~EncodingRunnable() {}

public:
  NS_DECL_ISUPPORTS_INHERITED

  EncodingRunnable(const nsAString& aType,
                   const nsAString& aOptions,
                   uint8_t* aImageBuffer,
                   imgIEncoder* aEncoder,
                   EncodingCompleteEvent* aEncodingCompleteEvent,
                   int32_t aFormat,
                   const nsIntSize aSize,
                   bool aUsingCustomOptions)
    : mType(aType)
    , mOptions(aOptions)
    , mImageBuffer(aImageBuffer)
    , mEncoder(aEncoder)
    , mEncodingCompleteEvent(aEncodingCompleteEvent)
    , mFormat(aFormat)
    , mSize(aSize)
    , mUsingCustomOptions(aUsingCustomOptions)
  {}

  nsresult ProcessImageData(uint64_t* aImgSize, void** aImgData)
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

    rv = stream->Available(aImgSize);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(*aImgSize <= UINT32_MAX, NS_ERROR_FILE_TOO_BIG);

    rv = NS_ReadInputStreamToBuffer(stream, aImgData, *aImgSize);
    NS_ENSURE_SUCCESS(rv, rv);

    return rv;
  }

  NS_IMETHOD Run()
  {
    uint64_t imgSize;
    void* imgData = nullptr;

    nsresult rv = ProcessImageData(&imgSize, &imgData);
    if (NS_FAILED(rv)) {
      mEncodingCompleteEvent->SetFailed();
    } else {
      mEncodingCompleteEvent->SetMembers(imgData, imgSize, mType);
    }
    rv = NS_DispatchToMainThread(mEncodingCompleteEvent);
    if (NS_FAILED(rv)) {
      // Better to leak than to crash.
      mEncodingCompleteEvent.forget();
      return rv;
    }

    return rv;
  }

private:
  nsAutoString mType;
  nsAutoString mOptions;
  nsAutoArrayPtr<uint8_t> mImageBuffer;
  nsCOMPtr<imgIEncoder> mEncoder;
  nsRefPtr<EncodingCompleteEvent> mEncodingCompleteEvent;
  int32_t mFormat;
  const nsIntSize mSize;
  bool mUsingCustomOptions;
};

NS_IMPL_ISUPPORTS_INHERITED0(EncodingRunnable, nsRunnable);

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
                               nsIGlobalObject* aGlobal,
                               FileCallback& aCallback)
{
  MOZ_ASSERT(aGlobal);

  nsCOMPtr<imgIEncoder> encoder = ImageEncoder::GetImageEncoder(aType);
  if (!encoder) {
    return NS_IMAGELIB_ERROR_NO_ENCODER;
  }

  nsCOMPtr<nsIThread> encoderThread;
  nsresult rv = NS_NewThread(getter_AddRefs(encoderThread), nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<EncodingCompleteEvent> completeEvent =
    new EncodingCompleteEvent(aGlobal, encoderThread, aCallback);

  nsCOMPtr<nsIRunnable> event = new EncodingRunnable(aType,
                                                     aOptions,
                                                     aImageBuffer,
                                                     encoder,
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
                             const char16_t* aEncoderOptions,
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
  if (aSize.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

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
    RefPtr<DataSourceSurface> emptyCanvas =
      Factory::CreateDataSourceSurfaceWithStride(IntSize(aSize.width, aSize.height),
                                                 SurfaceFormat::B8G8R8A8,
                                                 4 * aSize.width, true);
    if (NS_WARN_IF(!emptyCanvas)) {
      return NS_ERROR_INVALID_ARG;
    }

    DataSourceSurface::MappedSurface map;
    if (!emptyCanvas->Map(DataSourceSurface::MapType::WRITE, &map)) {
      return NS_ERROR_INVALID_ARG;
    }
    rv = aEncoder->InitFromData(map.mData,
                                aSize.width * aSize.height * 4,
                                aSize.width,
                                aSize.height,
                                aSize.width * 4,
                                imgIEncoder::INPUT_FORMAT_HOSTARGB,
                                aOptions);
    emptyCanvas->Unmap();
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
