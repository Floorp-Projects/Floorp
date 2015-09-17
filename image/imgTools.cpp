/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "imgTools.h"

#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsError.h"
#include "imgLoader.h"
#include "imgICache.h"
#include "imgIContainer.h"
#include "imgIEncoder.h"
#include "nsStreamUtils.h"
#include "nsContentUtils.h"
#include "ImageFactory.h"
#include "Image.h"
#include "ScriptedNotificationObserver.h"
#include "imgIScriptedNotificationObserver.h"
#include "gfxPlatform.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace image {
/* ========== imgITools implementation ========== */



NS_IMPL_ISUPPORTS(imgTools, imgITools)

imgTools::imgTools()
{
  /* member initializers and constructor code */
}

imgTools::~imgTools()
{
  /* destructor code */
}

NS_IMETHODIMP
imgTools::DecodeImageData(nsIInputStream* aInStr,
                          const nsACString& aMimeType,
                          imgIContainer** aContainer)
{
  MOZ_ASSERT(*aContainer == nullptr,
             "Cannot provide an existing image container to DecodeImageData");

  return DecodeImage(aInStr, aMimeType, aContainer);
}

NS_IMETHODIMP
imgTools::DecodeImage(nsIInputStream* aInStr,
                      const nsACString& aMimeType,
                      imgIContainer** aContainer)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;

  NS_ENSURE_ARG_POINTER(aInStr);

  // Create a new image container to hold the decoded data.
  nsAutoCString mimeType(aMimeType);
  nsRefPtr<image::Image> image = ImageFactory::CreateAnonymousImage(mimeType);
  nsRefPtr<ProgressTracker> tracker = image->GetProgressTracker();

  if (image->HasError()) {
    return NS_ERROR_FAILURE;
  }

  // Prepare the input stream.
  nsCOMPtr<nsIInputStream> inStream = aInStr;
  if (!NS_InputStreamIsBuffered(aInStr)) {
    nsCOMPtr<nsIInputStream> bufStream;
    rv = NS_NewBufferedInputStream(getter_AddRefs(bufStream), aInStr, 1024);
    if (NS_SUCCEEDED(rv)) {
      inStream = bufStream;
    }
  }

  // Figure out how much data we've been passed.
  uint64_t length;
  rv = inStream->Available(&length);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(length <= UINT32_MAX, NS_ERROR_FILE_TOO_BIG);

  // Send the source data to the Image.
  rv = image->OnImageDataAvailable(nullptr, nullptr, inStream, 0,
                                   uint32_t(length));
  NS_ENSURE_SUCCESS(rv, rv);

  // Let the Image know we've sent all the data.
  rv = image->OnImageDataComplete(nullptr, nullptr, NS_OK, true);
  tracker->SyncNotifyProgress(FLAG_LOAD_COMPLETE);
  NS_ENSURE_SUCCESS(rv, rv);

  // All done.
  NS_ADDREF(*aContainer = image.get());
  return NS_OK;
}

/**
 * This takes a DataSourceSurface rather than a SourceSurface because some
 * of the callers have a DataSourceSurface and we don't want to call
 * GetDataSurface on such surfaces since that may incure a conversion to
 * SurfaceType::DATA which we don't need.
 */
static nsresult
EncodeImageData(DataSourceSurface* aDataSurface,
                const nsACString& aMimeType,
                const nsAString& aOutputOptions,
                nsIInputStream** aStream)
{
  MOZ_ASSERT(aDataSurface->GetFormat() ==  SurfaceFormat::B8G8R8A8,
             "We're assuming B8G8R8A8");

  // Get an image encoder for the media type
  nsAutoCString encoderCID(
    NS_LITERAL_CSTRING("@mozilla.org/image/encoder;2?type=") + aMimeType);

  nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(encoderCID.get());
  if (!encoder) {
    return NS_IMAGELIB_ERROR_NO_ENCODER;
  }

  DataSourceSurface::MappedSurface map;
  if (!aDataSurface->Map(DataSourceSurface::MapType::READ, &map)) {
    return NS_ERROR_FAILURE;
  }

  IntSize size = aDataSurface->GetSize();
  uint32_t dataLength = map.mStride * size.height;

  // Encode the bitmap
  nsresult rv = encoder->InitFromData(map.mData,
                                      dataLength,
                                      size.width,
                                      size.height,
                                      map.mStride,
                                      imgIEncoder::INPUT_FORMAT_HOSTARGB,
                                      aOutputOptions);
  aDataSurface->Unmap();
  NS_ENSURE_SUCCESS(rv, rv);

  encoder.forget(aStream);
  return NS_OK;
}

NS_IMETHODIMP
imgTools::EncodeImage(imgIContainer* aContainer,
                      const nsACString& aMimeType,
                      const nsAString& aOutputOptions,
                      nsIInputStream** aStream)
{
  // Use frame 0 from the image container.
  RefPtr<SourceSurface> frame =
    aContainer->GetFrame(imgIContainer::FRAME_FIRST,
                         imgIContainer::FLAG_SYNC_DECODE);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  RefPtr<DataSourceSurface> dataSurface;

  if (frame->GetFormat() == SurfaceFormat::B8G8R8A8) {
    dataSurface = frame->GetDataSurface();
  } else {
    // Convert format to SurfaceFormat::B8G8R8A8
    dataSurface = gfxUtils::
      CopySurfaceToDataSourceSurfaceWithFormat(frame,
                                               SurfaceFormat::B8G8R8A8);
  }

  NS_ENSURE_TRUE(dataSurface, NS_ERROR_FAILURE);

  return EncodeImageData(dataSurface, aMimeType, aOutputOptions, aStream);
}

NS_IMETHODIMP
imgTools::EncodeScaledImage(imgIContainer* aContainer,
                            const nsACString& aMimeType,
                            int32_t aScaledWidth,
                            int32_t aScaledHeight,
                            const nsAString& aOutputOptions,
                            nsIInputStream** aStream)
{
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
  RefPtr<SourceSurface> frame =
    aContainer->GetFrameAtSize(scaledSize,
                               imgIContainer::FRAME_FIRST,
                               imgIContainer::FLAG_HIGH_QUALITY_SCALING |
                               imgIContainer::FLAG_SYNC_DECODE);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  RefPtr<DataSourceSurface> dataSurface =
    Factory::CreateDataSourceSurface(scaledSize, SurfaceFormat::B8G8R8A8);
  if (NS_WARN_IF(!dataSurface)) {
    return NS_ERROR_FAILURE;
  }

  DataSourceSurface::MappedSurface map;
  if (!dataSurface->Map(DataSourceSurface::MapType::WRITE, &map)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<DrawTarget> dt =
    Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                     map.mData,
                                     dataSurface->GetSize(),
                                     map.mStride,
                                     SurfaceFormat::B8G8R8A8);
  if (!dt) {
    gfxWarning() << "imgTools::EncodeImage failed in CreateDrawTargetForData";
    return NS_ERROR_OUT_OF_MEMORY;
  }

  IntSize frameSize = frame->GetSize();
  dt->DrawSurface(frame,
                  Rect(0, 0, scaledSize.width, scaledSize.height),
                  Rect(0, 0, frameSize.width, frameSize.height),
                  DrawSurfaceOptions(),
                  DrawOptions(1.0f, CompositionOp::OP_SOURCE));

  dataSurface->Unmap();

  return EncodeImageData(dataSurface, aMimeType, aOutputOptions, aStream);
}

NS_IMETHODIMP
imgTools::EncodeCroppedImage(imgIContainer* aContainer,
                             const nsACString& aMimeType,
                             int32_t aOffsetX,
                             int32_t aOffsetY,
                             int32_t aWidth,
                             int32_t aHeight,
                             const nsAString& aOutputOptions,
                             nsIInputStream** aStream)
{
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
  RefPtr<SourceSurface> frame =
    aContainer->GetFrame(imgIContainer::FRAME_FIRST,
                         imgIContainer::FLAG_SYNC_DECODE);
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

  RefPtr<DataSourceSurface> dataSurface =
    Factory::CreateDataSourceSurface(IntSize(aWidth, aHeight),
                                     SurfaceFormat::B8G8R8A8,
                                     /* aZero = */ true);
  if (NS_WARN_IF(!dataSurface)) {
    return NS_ERROR_FAILURE;
  }

  DataSourceSurface::MappedSurface map;
  if (!dataSurface->Map(DataSourceSurface::MapType::WRITE, &map)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<DrawTarget> dt =
    Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                     map.mData,
                                     dataSurface->GetSize(),
                                     map.mStride,
                                     SurfaceFormat::B8G8R8A8);
  if (!dt) {
    gfxWarning() <<
      "imgTools::EncodeCroppedImage failed in CreateDrawTargetForData";
    return NS_ERROR_OUT_OF_MEMORY;
  }
  dt->CopySurface(frame,
                  IntRect(aOffsetX, aOffsetY, aWidth, aHeight),
                  IntPoint(0, 0));

  dataSurface->Unmap();

  return EncodeImageData(dataSurface, aMimeType, aOutputOptions, aStream);
}

NS_IMETHODIMP
imgTools::CreateScriptedObserver(imgIScriptedNotificationObserver* aInner,
                                 imgINotificationObserver** aObserver)
{
  NS_ADDREF(*aObserver = new ScriptedNotificationObserver(aInner));
  return NS_OK;
}

NS_IMETHODIMP
imgTools::GetImgLoaderForDocument(nsIDOMDocument* aDoc, imgILoader** aLoader)
{
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);
  NS_IF_ADDREF(*aLoader = nsContentUtils::GetImgLoaderForDocument(doc));
  return NS_OK;
}

NS_IMETHODIMP
imgTools::GetImgCacheForDocument(nsIDOMDocument* aDoc, imgICache** aCache)
{
  nsCOMPtr<imgILoader> loader;
  nsresult rv = GetImgLoaderForDocument(aDoc, getter_AddRefs(loader));
  NS_ENSURE_SUCCESS(rv, rv);
  return CallQueryInterface(loader, aCache);
}

} // namespace image
} // namespace mozilla
