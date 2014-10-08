/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageEncoder.h"
#include "mozilla/dom/CanvasRenderingContext2D.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SyncRunnable.h"
#include "gfxUtils.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

// This class should be placed inside GetBRGADataSourceSurfaceSync(). However,
// due to B2G ICS uses old complier (C++98/03) which forbids local class as
// template parameter, we need to move this class outside.
class SurfaceHelper : public nsRunnable {
public:
  explicit SurfaceHelper(TemporaryRef<layers::Image> aImage) : mImage(aImage) {}

  // It retrieves a SourceSurface reference and convert color format on main
  // thread and passes DataSourceSurface to caller thread.
  NS_IMETHOD Run() {
    // It guarantees the reference will be released on main thread.
    nsCountedRef<nsMainThreadSourceSurfaceRef> surface;
    surface.own(mImage->GetAsSourceSurface().drop());

    if (surface->GetFormat() == gfx::SurfaceFormat::B8G8R8A8) {
      mDataSourceSurface = surface->GetDataSurface();
    } else {
      mDataSourceSurface = gfxUtils::
        CopySurfaceToDataSourceSurfaceWithFormat(surface,
                                                 gfx::SurfaceFormat::B8G8R8A8);
    }
    return NS_OK;
  }

  TemporaryRef<gfx::DataSourceSurface> GetDataSurfaceSafe() {
    nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
    MOZ_ASSERT(mainThread);
    SyncRunnable::DispatchToThread(mainThread, this, false);

    return mDataSourceSurface.forget();
  }

private:
  RefPtr<layers::Image> mImage;
  RefPtr<gfx::DataSourceSurface> mDataSourceSurface;
};

// This function returns a DataSourceSurface in B8G8R8A8 format.
// It uses SourceSurface to do format convert. Because most SourceSurface in
// image formats should be referenced or dereferenced on main thread, it uses a
// sync class SurfaceHelper to retrieve SourceSurface and convert to B8G8R8A8 on
// main thread.
TemporaryRef<DataSourceSurface>
GetBRGADataSourceSurfaceSync(TemporaryRef<layers::Image> aImage)
{
  nsRefPtr<SurfaceHelper> helper = new SurfaceHelper(aImage);
  return helper->GetDataSurfaceSafe();
}

class EncodingCompleteEvent : public nsRunnable
{
  virtual ~EncodingCompleteEvent() {}

public:
  NS_DECL_ISUPPORTS_INHERITED

  EncodingCompleteEvent(nsIThread* aEncoderThread,
                        EncodeCompleteCallback* aEncodeCompleteCallback)
    : mImgSize(0)
    , mType()
    , mImgData(nullptr)
    , mEncoderThread(aEncoderThread)
    , mEncodeCompleteCallback(aEncodeCompleteCallback)
    , mFailed(false)
  {}

  NS_IMETHOD Run()
  {
    nsresult rv = NS_OK;
    MOZ_ASSERT(NS_IsMainThread());

    if (!mFailed) {
      // The correct parentObject has to be set by the mEncodeCompleteCallback.
      nsRefPtr<File> blob =
        File::CreateMemoryFile(nullptr, mImgData, mImgSize, mType);
      MOZ_ASSERT(blob);

      rv = mEncodeCompleteCallback->ReceiveBlob(blob.forget());
    }

    mEncodeCompleteCallback = nullptr;

    mEncoderThread->Shutdown();
    return rv;
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
  nsCOMPtr<nsIThread> mEncoderThread;
  nsRefPtr<EncodeCompleteCallback> mEncodeCompleteCallback;
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
                   layers::Image* aImage,
                   imgIEncoder* aEncoder,
                   EncodingCompleteEvent* aEncodingCompleteEvent,
                   int32_t aFormat,
                   const nsIntSize aSize,
                   bool aUsingCustomOptions)
    : mType(aType)
    , mOptions(aOptions)
    , mImageBuffer(aImageBuffer)
    , mImage(aImage)
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
                                                    mImage,
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
                                             mImage,
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
  nsRefPtr<layers::Image> mImage;
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

  return ExtractDataInternal(aType, aOptions, nullptr, 0, aSize, nullptr,
                             aContext, aStream, encoder);
}


/* static */
nsresult
ImageEncoder::ExtractDataFromLayersImageAsync(nsAString& aType,
                                              const nsAString& aOptions,
                                              bool aUsingCustomOptions,
                                              layers::Image* aImage,
                                              EncodeCompleteCallback* aEncodeCallback)
{
  nsCOMPtr<imgIEncoder> encoder = ImageEncoder::GetImageEncoder(aType);
  if (!encoder) {
    return NS_IMAGELIB_ERROR_NO_ENCODER;
  }

  nsCOMPtr<nsIThread> encoderThread;
  nsresult rv = NS_NewThread(getter_AddRefs(encoderThread), nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<EncodingCompleteEvent> completeEvent =
    new EncodingCompleteEvent(encoderThread, aEncodeCallback);

  nsIntSize size(aImage->GetSize().width, aImage->GetSize().height);
  nsCOMPtr<nsIRunnable> event = new EncodingRunnable(aType,
                                                     aOptions,
                                                     nullptr,
                                                     aImage,
                                                     encoder,
                                                     completeEvent,
                                                     imgIEncoder::INPUT_FORMAT_HOSTARGB,
                                                     size,
                                                     aUsingCustomOptions);
  return encoderThread->Dispatch(event, NS_DISPATCH_NORMAL);
}

/* static */
nsresult
ImageEncoder::ExtractDataAsync(nsAString& aType,
                               const nsAString& aOptions,
                               bool aUsingCustomOptions,
                               uint8_t* aImageBuffer,
                               int32_t aFormat,
                               const nsIntSize aSize,
                               EncodeCompleteCallback* aEncodeCallback)
{
  nsCOMPtr<imgIEncoder> encoder = ImageEncoder::GetImageEncoder(aType);
  if (!encoder) {
    return NS_IMAGELIB_ERROR_NO_ENCODER;
  }

  nsCOMPtr<nsIThread> encoderThread;
  nsresult rv = NS_NewThread(getter_AddRefs(encoderThread), nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<EncodingCompleteEvent> completeEvent =
    new EncodingCompleteEvent(encoderThread, aEncodeCallback);

  nsCOMPtr<nsIRunnable> event = new EncodingRunnable(aType,
                                                     aOptions,
                                                     aImageBuffer,
                                                     nullptr,
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
                                  layers::Image* aImage,
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
  } else if (aImage) {
    // It is safe to convert PlanarYCbCr format from YUV to RGB off-main-thread.
    // Other image formats could have problem to convert format off-main-thread.
    // So here it uses a help function GetBRGADataSourceSurfaceSync() to convert
    // format on main thread.
    if (aImage->GetFormat() == ImageFormat::PLANAR_YCBCR) {
      nsTArray<uint8_t> data;
      layers::PlanarYCbCrImage* ycbcrImage = static_cast<layers::PlanarYCbCrImage*> (aImage);
      gfxImageFormat format = gfxImageFormat::ARGB32;
      uint32_t stride = GetAlignedStride<16>(aSize.width * 4);
      size_t length = BufferSizeFromStrideAndHeight(stride, aSize.height);
      data.SetCapacity(length);

      gfxUtils::ConvertYCbCrToRGB(*ycbcrImage->GetData(),
                                  format,
                                  aSize,
                                  data.Elements(),
                                  stride);

      rv = aEncoder->InitFromData(data.Elements(),
                                  aSize.width * aSize.height * 4,
                                  aSize.width,
                                  aSize.height,
                                  aSize.width * 4,
                                  imgIEncoder::INPUT_FORMAT_HOSTARGB,
                                  aOptions);
    } else {
      RefPtr<gfx::DataSourceSurface> dataSurface;
      dataSurface = GetBRGADataSourceSurfaceSync(aImage);

      DataSourceSurface::MappedSurface map;
      if (!dataSurface->Map(gfx::DataSourceSurface::MapType::READ, &map)) {
        return NS_ERROR_INVALID_ARG;
      }
      rv = aEncoder->InitFromData(map.mData,
                                  aSize.width * aSize.height * 4,
                                  aSize.width,
                                  aSize.height,
                                  aSize.width * 4,
                                  imgIEncoder::INPUT_FORMAT_HOSTARGB,
                                  aOptions);
      dataSurface->Unmap();
    }

    if (NS_SUCCEEDED(rv)) {
      imgStream = do_QueryInterface(aEncoder);
    }
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
