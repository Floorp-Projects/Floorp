/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageEncoder.h"
#include "mozilla/dom/CanvasRenderingContext2D.h"
#include "mozilla/dom/GeneratePlaceholderCanvasData.h"
#include "mozilla/dom/MemoryBlobImpl.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/layers/CanvasRenderer.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/Unused.h"
#include "gfxUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "nsXPCOMCIDInternal.h"
#include "YCbCrUtils.h"

using namespace mozilla::gfx;

namespace mozilla::dom {

// This class should be placed inside GetBRGADataSourceSurfaceSync(). However,
// due to B2G ICS uses old complier (C++98/03) which forbids local class as
// template parameter, we need to move this class outside.
class SurfaceHelper : public Runnable {
 public:
  explicit SurfaceHelper(already_AddRefed<layers::Image> aImage)
      : Runnable("SurfaceHelper"), mImage(aImage) {}

  // It retrieves a SourceSurface reference and convert color format on main
  // thread and passes DataSourceSurface to caller thread.
  NS_IMETHOD Run() override {
    RefPtr<gfx::SourceSurface> surface = mImage->GetAsSourceSurface();

    if (surface->GetFormat() == gfx::SurfaceFormat::B8G8R8A8) {
      mDataSourceSurface = surface->GetDataSurface();
    } else {
      mDataSourceSurface = gfxUtils::CopySurfaceToDataSourceSurfaceWithFormat(
          surface, gfx::SurfaceFormat::B8G8R8A8);
    }

    // It guarantees the reference will be released on main thread.
    NS_ReleaseOnMainThread("SurfaceHelper::surface", surface.forget());
    return NS_OK;
  }

  already_AddRefed<gfx::DataSourceSurface> GetDataSurfaceSafe() {
    nsCOMPtr<nsIEventTarget> mainTarget = GetMainThreadEventTarget();
    MOZ_ASSERT(mainTarget);
    SyncRunnable::DispatchToThread(mainTarget, this, false);

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
already_AddRefed<DataSourceSurface> GetBRGADataSourceSurfaceSync(
    already_AddRefed<layers::Image> aImage) {
  RefPtr<SurfaceHelper> helper = new SurfaceHelper(std::move(aImage));
  return helper->GetDataSurfaceSafe();
}

class EncodingCompleteEvent final : public DiscardableRunnable {
  virtual ~EncodingCompleteEvent() = default;

 public:
  explicit EncodingCompleteEvent(
      EncodeCompleteCallback* aEncodeCompleteCallback)
      : DiscardableRunnable("EncodingCompleteEvent"),
        mImgSize(0),
        mType(),
        mImgData(nullptr),
        mEncodeCompleteCallback(aEncodeCompleteCallback),
        mFailed(false) {
    if (!NS_IsMainThread() && IsCurrentThreadRunningWorker()) {
      mCreationEventTarget = GetCurrentEventTarget();
    } else {
      mCreationEventTarget = GetMainThreadEventTarget();
    }
  }

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY until Runnable::Run is MOZ_CAN_RUN_SCRIPT.  See
  // bug 1535398.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  NS_IMETHOD Run() override {
    nsresult rv = NS_OK;

    // We want to null out mEncodeCompleteCallback no matter what.
    RefPtr<EncodeCompleteCallback> callback(std::move(mEncodeCompleteCallback));
    if (!mFailed) {
      RefPtr<BlobImpl> blobImpl = new MemoryBlobImpl(mImgData, mImgSize, mType);
      rv = callback->ReceiveBlobImpl(blobImpl.forget());
    } else {
      rv = callback->ReceiveBlobImpl(nullptr);
    }

    return rv;
  }

  void SetMembers(void* aImgData, uint64_t aImgSize,
                  const nsAutoString& aType) {
    mImgData = aImgData;
    mImgSize = aImgSize;
    mType = aType;
  }

  void SetFailed() { mFailed = true; }

  nsIEventTarget* GetCreationThreadEventTarget() {
    return mCreationEventTarget;
  }

  bool CanBeDeletedOnAnyThread() {
    return !mEncodeCompleteCallback ||
           mEncodeCompleteCallback->CanBeDeletedOnAnyThread();
  }

 private:
  uint64_t mImgSize;
  nsAutoString mType;
  void* mImgData;
  nsCOMPtr<nsIEventTarget> mCreationEventTarget;
  RefPtr<EncodeCompleteCallback> mEncodeCompleteCallback;
  bool mFailed;
};

class EncodingRunnable : public Runnable {
  virtual ~EncodingRunnable() = default;

 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(EncodingRunnable, Runnable)

  EncodingRunnable(const nsAString& aType, const nsAString& aOptions,
                   UniquePtr<uint8_t[]> aImageBuffer, layers::Image* aImage,
                   imgIEncoder* aEncoder,
                   EncodingCompleteEvent* aEncodingCompleteEvent,
                   int32_t aFormat, const nsIntSize aSize, bool aUsePlaceholder,
                   bool aUsingCustomOptions)
      : Runnable("EncodingRunnable"),
        mType(aType),
        mOptions(aOptions),
        mImageBuffer(std::move(aImageBuffer)),
        mImage(aImage),
        mEncoder(aEncoder),
        mEncodingCompleteEvent(aEncodingCompleteEvent),
        mFormat(aFormat),
        mSize(aSize),
        mUsePlaceholder(aUsePlaceholder),
        mUsingCustomOptions(aUsingCustomOptions) {}

  nsresult ProcessImageData(uint64_t* aImgSize, void** aImgData) {
    nsCOMPtr<nsIInputStream> stream;
    nsresult rv = ImageEncoder::ExtractDataInternal(
        mType, mOptions, mImageBuffer.get(), mFormat, mSize, mUsePlaceholder,
        mImage, nullptr, nullptr, getter_AddRefs(stream), mEncoder);

    // If there are unrecognized custom parse options, we should fall back to
    // the default values for the encoder without any options at all.
    if (rv == NS_ERROR_INVALID_ARG && mUsingCustomOptions) {
      rv = ImageEncoder::ExtractDataInternal(
          mType, u""_ns, mImageBuffer.get(), mFormat, mSize, mUsePlaceholder,
          mImage, nullptr, nullptr, getter_AddRefs(stream), mEncoder);
    }
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_ReadInputStreamToBuffer(stream, aImgData, -1, aImgSize);
    NS_ENSURE_SUCCESS(rv, rv);

    return rv;
  }

  NS_IMETHOD Run() override {
    uint64_t imgSize;
    void* imgData = nullptr;

    nsresult rv = ProcessImageData(&imgSize, &imgData);
    if (NS_FAILED(rv)) {
      mEncodingCompleteEvent->SetFailed();
    } else {
      mEncodingCompleteEvent->SetMembers(imgData, imgSize, mType);
    }
    rv = mEncodingCompleteEvent->GetCreationThreadEventTarget()->Dispatch(
        mEncodingCompleteEvent, nsIThread::DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      if (!mEncodingCompleteEvent->CanBeDeletedOnAnyThread()) {
        // Better to leak than to crash.
        Unused << mEncodingCompleteEvent.forget();
      }
      return rv;
    }

    return rv;
  }

 private:
  nsAutoString mType;
  nsAutoString mOptions;
  UniquePtr<uint8_t[]> mImageBuffer;
  RefPtr<layers::Image> mImage;
  nsCOMPtr<imgIEncoder> mEncoder;
  RefPtr<EncodingCompleteEvent> mEncodingCompleteEvent;
  int32_t mFormat;
  const nsIntSize mSize;
  bool mUsePlaceholder;
  bool mUsingCustomOptions;
};

/* static */
nsresult ImageEncoder::ExtractData(nsAString& aType, const nsAString& aOptions,
                                   const nsIntSize aSize, bool aUsePlaceholder,
                                   nsICanvasRenderingContextInternal* aContext,
                                   layers::CanvasRenderer* aRenderer,
                                   nsIInputStream** aStream) {
  nsCOMPtr<imgIEncoder> encoder = ImageEncoder::GetImageEncoder(aType);
  if (!encoder) {
    return NS_IMAGELIB_ERROR_NO_ENCODER;
  }

  return ExtractDataInternal(aType, aOptions, nullptr, 0, aSize,
                             aUsePlaceholder, nullptr, aContext, aRenderer,
                             aStream, encoder);
}

/* static */
nsresult ImageEncoder::ExtractDataFromLayersImageAsync(
    nsAString& aType, const nsAString& aOptions, bool aUsingCustomOptions,
    layers::Image* aImage, bool aUsePlaceholder,
    EncodeCompleteCallback* aEncodeCallback) {
  nsCOMPtr<imgIEncoder> encoder = ImageEncoder::GetImageEncoder(aType);
  if (!encoder) {
    return NS_IMAGELIB_ERROR_NO_ENCODER;
  }

  RefPtr<EncodingCompleteEvent> completeEvent =
      new EncodingCompleteEvent(aEncodeCallback);

  nsIntSize size(aImage->GetSize().width, aImage->GetSize().height);
  nsCOMPtr<nsIRunnable> event =
      new EncodingRunnable(aType, aOptions, nullptr, aImage, encoder,
                           completeEvent, imgIEncoder::INPUT_FORMAT_HOSTARGB,
                           size, aUsePlaceholder, aUsingCustomOptions);
  return NS_DispatchBackgroundTask(event.forget());
}

/* static */
nsresult ImageEncoder::ExtractDataAsync(
    nsAString& aType, const nsAString& aOptions, bool aUsingCustomOptions,
    UniquePtr<uint8_t[]> aImageBuffer, int32_t aFormat, const nsIntSize aSize,
    bool aUsePlaceholder, EncodeCompleteCallback* aEncodeCallback) {
  nsCOMPtr<imgIEncoder> encoder = ImageEncoder::GetImageEncoder(aType);
  if (!encoder) {
    return NS_IMAGELIB_ERROR_NO_ENCODER;
  }

  RefPtr<EncodingCompleteEvent> completeEvent =
      new EncodingCompleteEvent(aEncodeCallback);

  nsCOMPtr<nsIRunnable> event = new EncodingRunnable(
      aType, aOptions, std::move(aImageBuffer), nullptr, encoder, completeEvent,
      aFormat, aSize, aUsePlaceholder, aUsingCustomOptions);
  return NS_DispatchBackgroundTask(event.forget());
}

/*static*/
nsresult ImageEncoder::GetInputStream(int32_t aWidth, int32_t aHeight,
                                      uint8_t* aImageBuffer, int32_t aFormat,
                                      imgIEncoder* aEncoder,
                                      const nsAString& aEncoderOptions,
                                      nsIInputStream** aStream) {
  nsresult rv =
      aEncoder->InitFromData(aImageBuffer, aWidth * aHeight * 4, aWidth,
                             aHeight, aWidth * 4, aFormat, aEncoderOptions);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<imgIEncoder> encoder(aEncoder);
  encoder.forget(aStream);
  return NS_OK;
}

/* static */
nsresult ImageEncoder::ExtractDataInternal(
    const nsAString& aType, const nsAString& aOptions, uint8_t* aImageBuffer,
    int32_t aFormat, const nsIntSize aSize, bool aUsePlaceholder,
    layers::Image* aImage, nsICanvasRenderingContextInternal* aContext,
    layers::CanvasRenderer* aRenderer, nsIInputStream** aStream,
    imgIEncoder* aEncoder) {
  if (aSize.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIInputStream> imgStream;

  // get image bytes
  nsresult rv;
  if (aImageBuffer && !aUsePlaceholder) {
    if (BufferSizeFromDimensions(aSize.width, aSize.height, 4) == 0) {
      return NS_ERROR_INVALID_ARG;
    }

    rv = ImageEncoder::GetInputStream(aSize.width, aSize.height, aImageBuffer,
                                      aFormat, aEncoder, aOptions,
                                      getter_AddRefs(imgStream));
  } else if (aContext && !aUsePlaceholder) {
    NS_ConvertUTF16toUTF8 encoderType(aType);
    rv = aContext->GetInputStream(encoderType.get(), aOptions,
                                  getter_AddRefs(imgStream));
  } else if (aRenderer && !aUsePlaceholder) {
    MOZ_CRASH("unused?");
    const NS_ConvertUTF16toUTF8 encoderType(aType);
    if (BufferSizeFromDimensions(aSize.width, aSize.height, 4) == 0) {
      return NS_ERROR_INVALID_ARG;
    }

    const auto snapshot = aRenderer->BorrowSnapshot();
    if (!snapshot) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    const RefPtr<DataSourceSurface> data = snapshot->mSurf->GetDataSurface();
    if (!data) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    {
      DataSourceSurface::MappedSurface map;
      if (!data->Map(gfx::DataSourceSurface::MapType::READ, &map)) {
        return NS_ERROR_INVALID_ARG;
      }
      rv = aEncoder->InitFromData(map.mData, aSize.width * aSize.height * 4,
                                  aSize.width, aSize.height, aSize.width * 4,
                                  imgIEncoder::INPUT_FORMAT_HOSTARGB, aOptions);
      data->Unmap();
    }
  } else if (aImage && !aUsePlaceholder) {
    // It is safe to convert PlanarYCbCr format from YUV to RGB off-main-thread.
    // Other image formats could have problem to convert format off-main-thread.
    // So here it uses a help function GetBRGADataSourceSurfaceSync() to convert
    // format on main thread.
    if (aImage->GetFormat() == ImageFormat::PLANAR_YCBCR) {
      nsTArray<uint8_t> data;
      layers::PlanarYCbCrImage* ycbcrImage =
          static_cast<layers::PlanarYCbCrImage*>(aImage);
      gfxImageFormat format = SurfaceFormat::A8R8G8B8_UINT32;
      uint32_t stride = GetAlignedStride<16>(aSize.width, 4);
      size_t length = BufferSizeFromStrideAndHeight(stride, aSize.height);
      if (length == 0) {
        return NS_ERROR_INVALID_ARG;
      }
      data.SetCapacity(length);

      ConvertYCbCrToRGB(*ycbcrImage->GetData(), format, aSize, data.Elements(),
                        stride);

      rv = aEncoder->InitFromData(data.Elements(),
                                  aSize.width * aSize.height * 4, aSize.width,
                                  aSize.height, aSize.width * 4,
                                  imgIEncoder::INPUT_FORMAT_HOSTARGB, aOptions);
    } else {
      if (BufferSizeFromDimensions(aSize.width, aSize.height, 4) == 0) {
        return NS_ERROR_INVALID_ARG;
      }

      RefPtr<gfx::DataSourceSurface> dataSurface;
      RefPtr<layers::Image> image(aImage);
      dataSurface = GetBRGADataSourceSurfaceSync(image.forget());

      DataSourceSurface::MappedSurface map;
      if (!dataSurface->Map(gfx::DataSourceSurface::MapType::READ, &map)) {
        return NS_ERROR_INVALID_ARG;
      }
      rv = aEncoder->InitFromData(map.mData, aSize.width * aSize.height * 4,
                                  aSize.width, aSize.height, aSize.width * 4,
                                  imgIEncoder::INPUT_FORMAT_HOSTARGB, aOptions);
      dataSurface->Unmap();
    }

    if (NS_SUCCEEDED(rv)) {
      imgStream = aEncoder;
    }
  } else {
    if (BufferSizeFromDimensions(aSize.width, aSize.height, 4) == 0) {
      return NS_ERROR_INVALID_ARG;
    }

    // no context, so we have to encode an empty image
    // note that if we didn't have a current context, the spec says we're
    // supposed to just return transparent black pixels of the canvas
    // dimensions.
    RefPtr<DataSourceSurface> emptyCanvas =
        Factory::CreateDataSourceSurfaceWithStride(
            IntSize(aSize.width, aSize.height), SurfaceFormat::B8G8R8A8,
            4 * aSize.width, true);
    if (NS_WARN_IF(!emptyCanvas)) {
      return NS_ERROR_INVALID_ARG;
    }

    DataSourceSurface::MappedSurface map;
    if (!emptyCanvas->Map(DataSourceSurface::MapType::WRITE, &map)) {
      return NS_ERROR_INVALID_ARG;
    }
    if (aUsePlaceholder) {
      auto size = 4 * aSize.width * aSize.height;
      auto* data = map.mData;
      GeneratePlaceholderCanvasData(size, data);
    }
    rv = aEncoder->InitFromData(map.mData, aSize.width * aSize.height * 4,
                                aSize.width, aSize.height, aSize.width * 4,
                                imgIEncoder::INPUT_FORMAT_HOSTARGB, aOptions);
    emptyCanvas->Unmap();
    if (NS_SUCCEEDED(rv)) {
      imgStream = aEncoder;
    }
  }
  NS_ENSURE_SUCCESS(rv, rv);

  imgStream.forget(aStream);
  return rv;
}

/* static */
already_AddRefed<imgIEncoder> ImageEncoder::GetImageEncoder(nsAString& aType) {
  // Get an image encoder for the media type.
  nsCString encoderCID("@mozilla.org/image/encoder;2?type=");
  NS_ConvertUTF16toUTF8 encoderType(aType);
  encoderCID += encoderType;
  nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(encoderCID.get());

  if (!encoder && aType != u"image/png"_ns) {
    // Unable to create an encoder instance of the specified type. Falling back
    // to PNG.
    aType.AssignLiteral("image/png");
    nsCString PNGEncoderCID("@mozilla.org/image/encoder;2?type=image/png");
    encoder = do_CreateInstance(PNGEncoderCID.get());
  }

  return encoder.forget();
}

}  // namespace mozilla::dom
