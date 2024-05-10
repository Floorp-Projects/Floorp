/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "imgTools.h"

#include "DecodePool.h"
#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/Document.h"
#include "nsError.h"
#include "imgLoader.h"
#include "imgICache.h"
#include "imgIContainer.h"
#include "imgIEncoder.h"
#include "nsComponentManagerUtils.h"
#include "nsNetUtil.h"  // for NS_NewBufferedInputStream
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsContentUtils.h"
#include "nsProxyRelease.h"
#include "nsIStreamListener.h"
#include "ImageFactory.h"
#include "Image.h"
#include "IProgressObserver.h"
#include "ScriptedNotificationObserver.h"
#include "imgIScriptedNotificationObserver.h"
#include "gfxPlatform.h"
#include "js/ArrayBuffer.h"
#include "js/RootingAPI.h"  // JS::{Handle,Rooted}
#include "js/Value.h"       // JS::Value
#include "Orientation.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace image {

namespace {

static nsresult sniff_mimetype_callback(nsIInputStream* in, void* data,
                                        const char* fromRawSegment,
                                        uint32_t toOffset, uint32_t count,
                                        uint32_t* writeCount) {
  nsCString* mimeType = static_cast<nsCString*>(data);
  MOZ_ASSERT(mimeType, "mimeType is null!");

  if (count > 0) {
    imgLoader::GetMimeTypeFromContent(fromRawSegment, count, *mimeType);
  }

  *writeCount = 0;
  return NS_ERROR_FAILURE;
}

class ImageDecoderListener final : public nsIStreamListener,
                                   public IProgressObserver,
                                   public imgIContainer {
 public:
  NS_DECL_ISUPPORTS

  ImageDecoderListener(nsIURI* aURI, imgIContainerCallback* aCallback,
                       imgINotificationObserver* aObserver)
      : mURI(aURI),
        mImage(nullptr),
        mCallback(aCallback),
        mObserver(aObserver) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD
  OnDataAvailable(nsIRequest* aRequest, nsIInputStream* aInputStream,
                  uint64_t aOffset, uint32_t aCount) override {
    if (!mImage) {
      nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);

      nsCString mimeType;
      channel->GetContentType(mimeType);

      if (aInputStream) {
        // Look at the first few bytes and see if we can tell what the data is
        // from that since servers tend to lie. :(
        uint32_t unused;
        aInputStream->ReadSegments(sniff_mimetype_callback, &mimeType, aCount,
                                   &unused);
      }

      RefPtr<ProgressTracker> tracker = new ProgressTracker();
      if (mObserver) {
        tracker->AddObserver(this);
      }

      mImage = ImageFactory::CreateImage(channel, tracker, mimeType, mURI,
                                         /* aIsMultiPart */ false, 0);

      if (mImage->HasError()) {
        return NS_ERROR_FAILURE;
      }
    }

    return mImage->OnImageDataAvailable(aRequest, aInputStream, aOffset,
                                        aCount);
  }

  NS_IMETHOD
  OnStartRequest(nsIRequest* aRequest) override { return NS_OK; }

  NS_IMETHOD
  OnStopRequest(nsIRequest* aRequest, nsresult aStatus) override {
    // Encouter a fetch error, or no data could be fetched.
    if (!mImage || NS_FAILED(aStatus)) {
      mCallback->OnImageReady(nullptr, mImage ? aStatus : NS_ERROR_FAILURE);
      return NS_OK;
    }

    mImage->OnImageDataComplete(aRequest, aStatus, true);
    nsCOMPtr<imgIContainer> container = this;
    mCallback->OnImageReady(container, aStatus);
    return NS_OK;
  }

  virtual void Notify(int32_t aType,
                      const nsIntRect* aRect = nullptr) override {
    if (mObserver) {
      mObserver->Notify(nullptr, aType, aRect);
    }
  }

  virtual void OnLoadComplete(bool aLastPart) override {}

  // Other notifications are ignored.
  virtual void SetHasImage() override {}
  virtual bool NotificationsDeferred() const override { return false; }
  virtual void MarkPendingNotify() override {}
  virtual void ClearPendingNotify() override {}

  // imgIContainer
  NS_FORWARD_IMGICONTAINER(mImage->)

 private:
  virtual ~ImageDecoderListener() = default;

  nsCOMPtr<nsIURI> mURI;
  RefPtr<image::Image> mImage;
  nsCOMPtr<imgIContainerCallback> mCallback;
  nsCOMPtr<imgINotificationObserver> mObserver;
};

NS_IMPL_ISUPPORTS(ImageDecoderListener, nsIStreamListener, imgIContainer)

class ImageDecoderHelper final : public Runnable,
                                 public nsIInputStreamCallback {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  ImageDecoderHelper(already_AddRefed<image::Image> aImage,
                     already_AddRefed<nsIInputStream> aInputStream,
                     nsIEventTarget* aEventTarget,
                     imgIContainerCallback* aCallback,
                     nsIEventTarget* aCallbackEventTarget)
      : Runnable("ImageDecoderHelper"),
        mImage(std::move(aImage)),
        mInputStream(std::move(aInputStream)),
        mEventTarget(aEventTarget),
        mCallback(aCallback),
        mCallbackEventTarget(aCallbackEventTarget),
        mStatus(NS_OK) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD
  Run() override {
    // This runnable is dispatched on the Image thread when reading data, but
    // at the end, it goes back to the main-thread in order to complete the
    // operation.
    if (NS_IsMainThread()) {
      // Let the Image know we've sent all the data.
      mImage->OnImageDataComplete(nullptr, mStatus, true);

      RefPtr<ProgressTracker> tracker = mImage->GetProgressTracker();
      tracker->SyncNotifyProgress(FLAG_LOAD_COMPLETE);

      nsCOMPtr<imgIContainer> container;
      if (NS_SUCCEEDED(mStatus)) {
        container = mImage;
      }

      mCallback->OnImageReady(container, mStatus);
      return NS_OK;
    }

    uint64_t length;
    nsresult rv = mInputStream->Available(&length);
    if (rv == NS_BASE_STREAM_CLOSED) {
      return OperationCompleted(NS_OK);
    }

    if (NS_WARN_IF(NS_FAILED(rv))) {
      return OperationCompleted(rv);
    }

    // Nothing else to read, but maybe we just need to wait.
    if (length == 0) {
      nsCOMPtr<nsIAsyncInputStream> asyncInputStream =
          do_QueryInterface(mInputStream);
      if (asyncInputStream) {
        rv = asyncInputStream->AsyncWait(this, 0, 0, mEventTarget);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return OperationCompleted(rv);
        }
        return NS_OK;
      }

      // We really have nothing else to read.
      if (length == 0) {
        return OperationCompleted(NS_OK);
      }
    }

    // Send the source data to the Image.
    rv = mImage->OnImageDataAvailable(nullptr, mInputStream, 0,
                                      uint32_t(length));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return OperationCompleted(rv);
    }

    rv = mEventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return OperationCompleted(rv);
    }

    return NS_OK;
  }

  NS_IMETHOD
  OnInputStreamReady(nsIAsyncInputStream* aAsyncInputStream) override {
    MOZ_ASSERT(!NS_IsMainThread());
    return Run();
  }

  nsresult OperationCompleted(nsresult aStatus) {
    MOZ_ASSERT(!NS_IsMainThread());

    mStatus = aStatus;
    mCallbackEventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
    return NS_OK;
  }

 private:
  ~ImageDecoderHelper() {
    SurfaceCache::ReleaseImageOnMainThread(mImage.forget());
    NS_ReleaseOnMainThread("ImageDecoderHelper::mCallback", mCallback.forget());
  }

  RefPtr<image::Image> mImage;

  nsCOMPtr<nsIInputStream> mInputStream;
  nsCOMPtr<nsIEventTarget> mEventTarget;
  nsCOMPtr<imgIContainerCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mCallbackEventTarget;

  nsresult mStatus;
};

NS_IMPL_ISUPPORTS_INHERITED(ImageDecoderHelper, Runnable,
                            nsIInputStreamCallback)

}  // namespace

/* ========== imgITools implementation ========== */

NS_IMPL_ISUPPORTS(imgTools, imgITools)

imgTools::imgTools() { /* member initializers and constructor code */
}

imgTools::~imgTools() { /* destructor code */
}

NS_IMETHODIMP
imgTools::DecodeImageFromArrayBuffer(JS::Handle<JS::Value> aArrayBuffer,
                                     const nsACString& aMimeType,
                                     JSContext* aCx,
                                     imgIContainer** aContainer) {
  if (!aArrayBuffer.isObject()) {
    return NS_ERROR_FAILURE;
  }

  JS::Rooted<JSObject*> obj(aCx,
                            JS::UnwrapArrayBuffer(&aArrayBuffer.toObject()));
  if (!obj) {
    return NS_ERROR_FAILURE;
  }

  uint8_t* bufferData = nullptr;
  size_t bufferLength = 0;
  bool isSharedMemory = false;

  JS::GetArrayBufferLengthAndData(obj, &bufferLength, &isSharedMemory,
                                  &bufferData);

  // Throw for large ArrayBuffers to prevent truncation.
  if (bufferLength > INT32_MAX) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  return DecodeImageFromBuffer((char*)bufferData, bufferLength, aMimeType,
                               aContainer);
}

NS_IMETHODIMP
imgTools::DecodeImageFromBuffer(const char* aBuffer, uint32_t aSize,
                                const nsACString& aMimeType,
                                imgIContainer** aContainer) {
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_ARG_POINTER(aBuffer);

  // Create a new image container to hold the decoded data.
  nsAutoCString mimeType(aMimeType);
  RefPtr<image::Image> image =
      ImageFactory::CreateAnonymousImage(mimeType, aSize);
  RefPtr<ProgressTracker> tracker = image->GetProgressTracker();

  if (image->HasError()) {
    return NS_ERROR_FAILURE;
  }

  // Let's create a temporary inputStream.
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(
      getter_AddRefs(stream), Span(aBuffer, aSize), NS_ASSIGNMENT_DEPEND);
  NS_ENSURE_SUCCESS(rv, rv);
  MOZ_ASSERT(stream);
  MOZ_ASSERT(NS_InputStreamIsBuffered(stream));

  rv = image->OnImageDataAvailable(nullptr, stream, 0, aSize);
  NS_ENSURE_SUCCESS(rv, rv);

  // Let the Image know we've sent all the data.
  rv = image->OnImageDataComplete(nullptr, NS_OK, true);
  tracker->SyncNotifyProgress(FLAG_LOAD_COMPLETE);
  NS_ENSURE_SUCCESS(rv, rv);

  // All done.
  image.forget(aContainer);
  return NS_OK;
}

NS_IMETHODIMP
imgTools::DecodeImageFromChannelAsync(nsIURI* aURI, nsIChannel* aChannel,
                                      imgIContainerCallback* aCallback,
                                      imgINotificationObserver* aObserver) {
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aCallback);

  RefPtr<ImageDecoderListener> listener =
      new ImageDecoderListener(aURI, aCallback, aObserver);

  return aChannel->AsyncOpen(listener);
}

NS_IMETHODIMP
imgTools::DecodeImageAsync(nsIInputStream* aInStr, const nsACString& aMimeType,
                           imgIContainerCallback* aCallback,
                           nsIEventTarget* aEventTarget) {
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_ARG_POINTER(aInStr);
  NS_ENSURE_ARG_POINTER(aCallback);
  NS_ENSURE_ARG_POINTER(aEventTarget);

  nsresult rv;

  // Let's continuing the reading on a separate thread.
  DecodePool* decodePool = DecodePool::Singleton();
  MOZ_ASSERT(decodePool);

  RefPtr<nsIEventTarget> target = decodePool->GetIOEventTarget();
  NS_ENSURE_TRUE(target, NS_ERROR_FAILURE);

  // Prepare the input stream.
  nsCOMPtr<nsIInputStream> stream = aInStr;
  if (!NS_InputStreamIsBuffered(aInStr)) {
    nsCOMPtr<nsIInputStream> bufStream;
    rv = NS_NewBufferedInputStream(getter_AddRefs(bufStream), stream.forget(),
                                   1024);
    NS_ENSURE_SUCCESS(rv, rv);
    stream = std::move(bufStream);
  }

  // Create a new image container to hold the decoded data.
  nsAutoCString mimeType(aMimeType);
  RefPtr<image::Image> image = ImageFactory::CreateAnonymousImage(mimeType, 0);

  // Already an error?
  if (image->HasError()) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<ImageDecoderHelper> helper = new ImageDecoderHelper(
      image.forget(), stream.forget(), target, aCallback, aEventTarget);
  rv = target->Dispatch(helper.forget(), NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

/**
 * This takes a DataSourceSurface rather than a SourceSurface because some
 * of the callers have a DataSourceSurface and we don't want to call
 * GetDataSurface on such surfaces since that may incur a conversion to
 * SurfaceType::DATA which we don't need.
 */
static nsresult EncodeImageData(DataSourceSurface* aDataSurface,
                                DataSourceSurface::ScopedMap& aMap,
                                const nsACString& aMimeType,
                                const nsAString& aOutputOptions,
                                nsIInputStream** aStream) {
  MOZ_ASSERT(aDataSurface->GetFormat() == SurfaceFormat::B8G8R8A8 ||
                 aDataSurface->GetFormat() == SurfaceFormat::B8G8R8X8,
             "We're assuming B8G8R8A8/X8");

  // Get an image encoder for the media type
  nsAutoCString encoderCID("@mozilla.org/image/encoder;2?type="_ns + aMimeType);

  nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(encoderCID.get());
  if (!encoder) {
    return NS_IMAGELIB_ERROR_NO_ENCODER;
  }

  IntSize size = aDataSurface->GetSize();
  uint32_t dataLength = aMap.GetStride() * size.height;

  // Encode the bitmap
  nsresult rv = encoder->InitFromData(
      aMap.GetData(), dataLength, size.width, size.height, aMap.GetStride(),
      imgIEncoder::INPUT_FORMAT_HOSTARGB, aOutputOptions);
  NS_ENSURE_SUCCESS(rv, rv);

  encoder.forget(aStream);
  return NS_OK;
}

static nsresult EncodeImageData(DataSourceSurface* aDataSurface,
                                const nsACString& aMimeType,
                                const nsAString& aOutputOptions,
                                nsIInputStream** aStream) {
  DataSourceSurface::ScopedMap map(aDataSurface, DataSourceSurface::READ);
  if (!map.IsMapped()) {
    return NS_ERROR_FAILURE;
  }

  return EncodeImageData(aDataSurface, map, aMimeType, aOutputOptions, aStream);
}

NS_IMETHODIMP
imgTools::EncodeImage(imgIContainer* aContainer, const nsACString& aMimeType,
                      const nsAString& aOutputOptions,
                      nsIInputStream** aStream) {
  // Use frame 0 from the image container.
  RefPtr<SourceSurface> frame = aContainer->GetFrame(
      imgIContainer::FRAME_FIRST,
      imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_ASYNC_NOTIFY);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  RefPtr<DataSourceSurface> dataSurface;

  if (frame->GetFormat() == SurfaceFormat::B8G8R8A8 ||
      frame->GetFormat() == SurfaceFormat::B8G8R8X8) {
    dataSurface = frame->GetDataSurface();
  } else {
    // Convert format to SurfaceFormat::B8G8R8A8
    dataSurface = gfxUtils::CopySurfaceToDataSourceSurfaceWithFormat(
        frame, SurfaceFormat::B8G8R8A8);
  }

  NS_ENSURE_TRUE(dataSurface, NS_ERROR_FAILURE);

  return EncodeImageData(dataSurface, aMimeType, aOutputOptions, aStream);
}

NS_IMETHODIMP
imgTools::EncodeScaledImage(imgIContainer* aContainer,
                            const nsACString& aMimeType, int32_t aScaledWidth,
                            int32_t aScaledHeight,
                            const nsAString& aOutputOptions,
                            nsIInputStream** aStream) {
  NS_ENSURE_ARG(aScaledWidth >= 0 && aScaledHeight >= 0);

  // If no scaled size is specified, we'll just encode the image at its
  // original size (no scaling).
  if (aScaledWidth == 0 && aScaledHeight == 0) {
    return EncodeImage(aContainer, aMimeType, aOutputOptions, aStream);
  }

  // Retrieve the image's size.
  int32_t imageWidth = 0;
  int32_t imageHeight = 0;
  aContainer->GetWidth(&imageWidth);
  aContainer->GetHeight(&imageHeight);

  // If the given width or height is zero we'll replace it with the image's
  // original dimensions.
  IntSize scaledSize(aScaledWidth == 0 ? imageWidth : aScaledWidth,
                     aScaledHeight == 0 ? imageHeight : aScaledHeight);

  // Use frame 0 from the image container.
  RefPtr<SourceSurface> frame = aContainer->GetFrameAtSize(
      scaledSize, imgIContainer::FRAME_FIRST,
      imgIContainer::FLAG_HIGH_QUALITY_SCALING |
          imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_ASYNC_NOTIFY);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  // If the given surface is the right size/format, we can encode it directly.
  if (scaledSize == frame->GetSize() &&
      (frame->GetFormat() == SurfaceFormat::B8G8R8A8 ||
       frame->GetFormat() == SurfaceFormat::B8G8R8X8)) {
    RefPtr<DataSourceSurface> dataSurface = frame->GetDataSurface();
    if (dataSurface) {
      return EncodeImageData(dataSurface, aMimeType, aOutputOptions, aStream);
    }
  }

  // Otherwise we need to scale it using a draw target.
  RefPtr<DataSourceSurface> dataSurface =
      Factory::CreateDataSourceSurface(scaledSize, SurfaceFormat::B8G8R8A8);
  if (NS_WARN_IF(!dataSurface)) {
    return NS_ERROR_FAILURE;
  }

  DataSourceSurface::ScopedMap map(dataSurface, DataSourceSurface::READ_WRITE);
  if (!map.IsMapped()) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<DrawTarget> dt = Factory::CreateDrawTargetForData(
      BackendType::SKIA, map.GetData(), dataSurface->GetSize(), map.GetStride(),
      SurfaceFormat::B8G8R8A8);
  if (!dt) {
    gfxWarning() << "imgTools::EncodeImage failed in CreateDrawTargetForData";
    return NS_ERROR_OUT_OF_MEMORY;
  }

  IntSize frameSize = frame->GetSize();
  dt->DrawSurface(frame, Rect(0, 0, scaledSize.width, scaledSize.height),
                  Rect(0, 0, frameSize.width, frameSize.height),
                  DrawSurfaceOptions(),
                  DrawOptions(1.0f, CompositionOp::OP_SOURCE));

  return EncodeImageData(dataSurface, map, aMimeType, aOutputOptions, aStream);
}

NS_IMETHODIMP
imgTools::EncodeCroppedImage(imgIContainer* aContainer,
                             const nsACString& aMimeType, int32_t aOffsetX,
                             int32_t aOffsetY, int32_t aWidth, int32_t aHeight,
                             const nsAString& aOutputOptions,
                             nsIInputStream** aStream) {
  NS_ENSURE_ARG(aOffsetX >= 0 && aOffsetY >= 0 && aWidth >= 0 && aHeight >= 0);

  // Offsets must be zero when no width and height are given or else we're out
  // of bounds.
  NS_ENSURE_ARG(aWidth + aHeight > 0 || aOffsetX + aOffsetY == 0);

  // If no size is specified then we'll preserve the image's original dimensions
  // and don't need to crop.
  if (aWidth == 0 && aHeight == 0) {
    return EncodeImage(aContainer, aMimeType, aOutputOptions, aStream);
  }

  // Use frame 0 from the image container.
  RefPtr<SourceSurface> frame = aContainer->GetFrame(
      imgIContainer::FRAME_FIRST,
      imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_ASYNC_NOTIFY);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  int32_t frameWidth = frame->GetSize().width;
  int32_t frameHeight = frame->GetSize().height;

  // If the given width or height is zero we'll replace it with the image's
  // original dimensions.
  if (aWidth == 0) {
    aWidth = frameWidth;
  } else if (aHeight == 0) {
    aHeight = frameHeight;
  }

  // Check that the given crop rectangle is within image bounds.
  NS_ENSURE_ARG(frameWidth >= aOffsetX + aWidth &&
                frameHeight >= aOffsetY + aHeight);

  RefPtr<DataSourceSurface> dataSurface = Factory::CreateDataSourceSurface(
      IntSize(aWidth, aHeight), SurfaceFormat::B8G8R8A8,
      /* aZero = */ true);
  if (NS_WARN_IF(!dataSurface)) {
    return NS_ERROR_FAILURE;
  }

  DataSourceSurface::ScopedMap map(dataSurface, DataSourceSurface::READ_WRITE);
  if (!map.IsMapped()) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<DrawTarget> dt = Factory::CreateDrawTargetForData(
      BackendType::SKIA, map.GetData(), dataSurface->GetSize(), map.GetStride(),
      SurfaceFormat::B8G8R8A8);
  if (!dt) {
    gfxWarning()
        << "imgTools::EncodeCroppedImage failed in CreateDrawTargetForData";
    return NS_ERROR_OUT_OF_MEMORY;
  }
  dt->CopySurface(frame, IntRect(aOffsetX, aOffsetY, aWidth, aHeight),
                  IntPoint(0, 0));

  return EncodeImageData(dataSurface, map, aMimeType, aOutputOptions, aStream);
}

NS_IMETHODIMP
imgTools::CreateScriptedObserver(imgIScriptedNotificationObserver* aInner,
                                 imgINotificationObserver** aObserver) {
  NS_ADDREF(*aObserver = new ScriptedNotificationObserver(aInner));
  return NS_OK;
}

NS_IMETHODIMP
imgTools::GetImgLoaderForDocument(dom::Document* aDoc, imgILoader** aLoader) {
  NS_IF_ADDREF(*aLoader = nsContentUtils::GetImgLoaderForDocument(aDoc));
  return NS_OK;
}

NS_IMETHODIMP
imgTools::GetImgCacheForDocument(dom::Document* aDoc, imgICache** aCache) {
  nsCOMPtr<imgILoader> loader;
  nsresult rv = GetImgLoaderForDocument(aDoc, getter_AddRefs(loader));
  NS_ENSURE_SUCCESS(rv, rv);
  return CallQueryInterface(loader, aCache);
}

}  // namespace image
}  // namespace mozilla
