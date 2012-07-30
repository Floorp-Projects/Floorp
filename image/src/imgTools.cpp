/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "imgTools.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "ImageErrors.h"
#include "imgIContainer.h"
#include "imgIEncoder.h"
#include "imgIDecoderObserver.h"
#include "imgIContainerObserver.h"
#include "gfxContext.h"
#include "nsStringStream.h"
#include "nsComponentManagerUtils.h"
#include "nsWeakReference.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsStreamUtils.h"
#include "nsNetUtil.h"
#include "RasterImage.h"

using namespace mozilla::image;

/* ========== imgITools implementation ========== */



NS_IMPL_ISUPPORTS1(imgTools, imgITools)

imgTools::imgTools()
{
  /* member initializers and constructor code */
}

imgTools::~imgTools()
{
  /* destructor code */
}


NS_IMETHODIMP imgTools::DecodeImageData(nsIInputStream* aInStr,
                                        const nsACString& aMimeType,
                                        imgIContainer **aContainer)
{
  nsresult rv;
  RasterImage* image;  // convenience alias for *aContainer

  NS_ENSURE_ARG_POINTER(aInStr);

  // If the caller didn't provide an imgIContainer, create one.
  if (*aContainer) {
    NS_ABORT_IF_FALSE((*aContainer)->GetType() == imgIContainer::TYPE_RASTER,
                      "wrong type of imgIContainer for decoding into");
    image = static_cast<RasterImage*>(*aContainer);
  } else {
    *aContainer = image = new RasterImage();
    NS_ADDREF(image);
  }

  // Initialize the Image. If we're using the one from the caller, we
  // require that it not be initialized.
  nsCString mimeType(aMimeType);
  rv = image->Init(nullptr, mimeType.get(), "<unknown>", Image::INIT_FLAG_NONE);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> inStream = aInStr;
  if (!NS_InputStreamIsBuffered(aInStr)) {
    nsCOMPtr<nsIInputStream> bufStream;
    rv = NS_NewBufferedInputStream(getter_AddRefs(bufStream), aInStr, 1024);
    if (NS_SUCCEEDED(rv))
      inStream = bufStream;
  }

  // Figure out how much data we've been passed
  PRUint32 length;
  rv = inStream->Available(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  // Send the source data to the Image. WriteToRasterImage always
  // consumes everything it gets if it doesn't run out of memory.
  PRUint32 bytesRead;
  rv = inStream->ReadSegments(RasterImage::WriteToRasterImage,
                              static_cast<void*>(image),
                              length, &bytesRead);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ABORT_IF_FALSE(bytesRead == length || image->HasError(),
  "WriteToRasterImage should consume everything or the image must be in error!");

  // Let the Image know we've sent all the data
  rv = image->SourceDataComplete();
  NS_ENSURE_SUCCESS(rv, rv);

  // All done
  return NS_OK;
}


NS_IMETHODIMP imgTools::EncodeImage(imgIContainer *aContainer,
                                    const nsACString& aMimeType,
                                    const nsAString& aOutputOptions,
                                    nsIInputStream **aStream)
{
  nsresult rv;

  // Use frame 0 from the image container.
  nsRefPtr<gfxImageSurface> frame;
  rv = GetFirstImageFrame(aContainer, getter_AddRefs(frame));
  NS_ENSURE_SUCCESS(rv, rv);

  return EncodeImageData(frame, aMimeType, aOutputOptions, aStream);
}

NS_IMETHODIMP imgTools::EncodeScaledImage(imgIContainer *aContainer,
                                          const nsACString& aMimeType,
                                          PRInt32 aScaledWidth,
                                          PRInt32 aScaledHeight,
                                          const nsAString& aOutputOptions,
                                          nsIInputStream **aStream)
{
  NS_ENSURE_ARG(aScaledWidth >= 0 && aScaledHeight >= 0);

  // If no scaled size is specified, we'll just encode the image at its
  // original size (no scaling).
  if (aScaledWidth == 0 && aScaledHeight == 0) {
    return EncodeImage(aContainer, aMimeType, aOutputOptions, aStream);
  }

  // Use frame 0 from the image container.
  nsRefPtr<gfxImageSurface> frame;
  nsresult rv = GetFirstImageFrame(aContainer, getter_AddRefs(frame));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 frameWidth = frame->Width(), frameHeight = frame->Height();

  // If the given width or height is zero we'll replace it with the image's
  // original dimensions.
  if (aScaledWidth == 0) {
    aScaledWidth = frameWidth;
  } else if (aScaledHeight == 0) {
    aScaledHeight = frameHeight;
  }

  // Create a temporary image surface
  nsRefPtr<gfxImageSurface> dest = new gfxImageSurface(gfxIntSize(aScaledWidth, aScaledHeight),
                                                       gfxASurface::ImageFormatARGB32);
  gfxContext ctx(dest);

  // Set scaling
  gfxFloat sw = (double) aScaledWidth / frameWidth;
  gfxFloat sh = (double) aScaledHeight / frameHeight;
  ctx.Scale(sw, sh);

  // Paint a scaled image
  ctx.SetOperator(gfxContext::OPERATOR_SOURCE);
  ctx.SetSource(frame);
  ctx.Paint();

  return EncodeImageData(dest, aMimeType, aOutputOptions, aStream);
}

NS_IMETHODIMP imgTools::EncodeCroppedImage(imgIContainer *aContainer,
                                           const nsACString& aMimeType,
                                           PRInt32 aOffsetX,
                                           PRInt32 aOffsetY,
                                           PRInt32 aWidth,
                                           PRInt32 aHeight,
                                           const nsAString& aOutputOptions,
                                           nsIInputStream **aStream)
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
  nsRefPtr<gfxImageSurface> frame;
  nsresult rv = GetFirstImageFrame(aContainer, getter_AddRefs(frame));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 frameWidth = frame->Width(), frameHeight = frame->Height();

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

  // Create a temporary image surface
  nsRefPtr<gfxImageSurface> dest = new gfxImageSurface(gfxIntSize(aWidth, aHeight),
                                                       gfxASurface::ImageFormatARGB32);
  gfxContext ctx(dest);

  // Set translate
  ctx.Translate(gfxPoint(-aOffsetX, -aOffsetY));

  // Paint a scaled image
  ctx.SetOperator(gfxContext::OPERATOR_SOURCE);
  ctx.SetSource(frame);
  ctx.Paint();

  return EncodeImageData(dest, aMimeType, aOutputOptions, aStream);
}

NS_IMETHODIMP imgTools::EncodeImageData(gfxImageSurface *aSurface,
                                        const nsACString& aMimeType,
                                        const nsAString& aOutputOptions,
                                        nsIInputStream **aStream)
{
  PRUint8 *bitmapData;
  PRUint32 bitmapDataLength, strideSize;

  // Get an image encoder for the media type
  nsCAutoString encoderCID(
    NS_LITERAL_CSTRING("@mozilla.org/image/encoder;2?type=") + aMimeType);

  nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(encoderCID.get());
  if (!encoder)
    return NS_IMAGELIB_ERROR_NO_ENCODER;

  bitmapData = aSurface->Data();
  if (!bitmapData)
    return NS_ERROR_FAILURE;

  strideSize = aSurface->Stride();

  PRInt32 width = aSurface->Width(), height = aSurface->Height();
  bitmapDataLength = height * strideSize;

  // Encode the bitmap
  nsresult rv = encoder->InitFromData(bitmapData,
                                      bitmapDataLength,
                                      width,
                                      height,
                                      strideSize,
                                      imgIEncoder::INPUT_FORMAT_HOSTARGB,
                                      aOutputOptions);

  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(encoder, aStream);
}

NS_IMETHODIMP imgTools::GetFirstImageFrame(imgIContainer *aContainer,
                                           gfxImageSurface **aSurface)
{
  nsRefPtr<gfxImageSurface> frame;
  nsresult rv = aContainer->CopyFrame(imgIContainer::FRAME_CURRENT, true,
                                      getter_AddRefs(frame));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(frame, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(frame->Width() && frame->Height(), NS_ERROR_FAILURE);

  frame.forget(aSurface);
  return NS_OK;
}
