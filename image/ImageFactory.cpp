/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "mozilla/Likely.h"

#include "nsIHttpChannel.h"
#include "nsIFileChannel.h"
#include "nsIFile.h"
#include "nsMimeTypes.h"
#include "nsIRequest.h"

#include "MultipartImage.h"
#include "RasterImage.h"
#include "VectorImage.h"
#include "Image.h"
#include "nsMediaFragmentURIParser.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"

#include "ImageFactory.h"
#include "gfxPrefs.h"

namespace mozilla {
namespace image {

/*static*/ void
ImageFactory::Initialize()
{ }

static bool
ShouldDownscaleDuringDecode(const nsCString& aMimeType)
{
  return aMimeType.EqualsLiteral(IMAGE_JPEG) ||
         aMimeType.EqualsLiteral(IMAGE_JPG) ||
         aMimeType.EqualsLiteral(IMAGE_PJPEG);
}

static uint32_t
ComputeImageFlags(ImageURL* uri, const nsCString& aMimeType, bool isMultiPart)
{
  nsresult rv;

  // We default to the static globals.
  bool isDiscardable = gfxPrefs::ImageMemDiscardable();
  bool doDecodeImmediately = gfxPrefs::ImageDecodeImmediatelyEnabled();
  bool doDownscaleDuringDecode = gfxPrefs::ImageDownscaleDuringDecodeEnabled();

  // We use the platform APZ value here since we don't have a widget to test.
  // It's safe since this is an optimization, and
  // ImageDecodeOnlyOnDraw is disabled everywhere and will be removed soon.
  bool doDecodeOnlyOnDraw = gfxPrefs::ImageDecodeOnlyOnDrawEnabled() &&
                            gfxPlatform::AsyncPanZoomEnabled();

  // We want UI to be as snappy as possible and not to flicker. Disable
  // discarding and decode-only-on-draw for chrome URLS.
  bool isChrome = false;
  rv = uri->SchemeIs("chrome", &isChrome);
  if (NS_SUCCEEDED(rv) && isChrome) {
    isDiscardable = doDecodeOnlyOnDraw = false;
  }

  // We don't want resources like the "loading" icon to be discardable or
  // decode-only-on-draw either.
  bool isResource = false;
  rv = uri->SchemeIs("resource", &isResource);
  if (NS_SUCCEEDED(rv) && isResource) {
    isDiscardable = doDecodeOnlyOnDraw = false;
  }

  // Downscale-during-decode and decode-only-on-draw are only enabled for
  // certain content types.
  if ((doDownscaleDuringDecode || doDecodeOnlyOnDraw) &&
      !ShouldDownscaleDuringDecode(aMimeType)) {
    doDownscaleDuringDecode = false;
    doDecodeOnlyOnDraw = false;
  }

  // If we're decoding immediately, disable decode-only-on-draw.
  if (doDecodeImmediately) {
    doDecodeOnlyOnDraw = false;
  }

  // For multipart/x-mixed-replace, we basically want a direct channel to the
  // decoder. Disable everything for this case.
  if (isMultiPart) {
    isDiscardable = doDecodeOnlyOnDraw = doDownscaleDuringDecode = false;
  }

  // We have all the information we need.
  uint32_t imageFlags = Image::INIT_FLAG_NONE;
  if (isDiscardable) {
    imageFlags |= Image::INIT_FLAG_DISCARDABLE;
  }
  if (doDecodeOnlyOnDraw) {
    imageFlags |= Image::INIT_FLAG_DECODE_ONLY_ON_DRAW;
  }
  if (doDecodeImmediately) {
    imageFlags |= Image::INIT_FLAG_DECODE_IMMEDIATELY;
  }
  if (isMultiPart) {
    imageFlags |= Image::INIT_FLAG_TRANSIENT;
  }
  if (doDownscaleDuringDecode) {
    imageFlags |= Image::INIT_FLAG_DOWNSCALE_DURING_DECODE;
  }

  return imageFlags;
}

/* static */ already_AddRefed<Image>
ImageFactory::CreateImage(nsIRequest* aRequest,
                          ProgressTracker* aProgressTracker,
                          const nsCString& aMimeType,
                          ImageURL* aURI,
                          bool aIsMultiPart,
                          uint32_t aInnerWindowId)
{
  MOZ_ASSERT(gfxPrefs::SingletonExists(),
             "Pref observers should have been initialized already");

  // Compute the image's initialization flags.
  uint32_t imageFlags = ComputeImageFlags(aURI, aMimeType, aIsMultiPart);

  // Select the type of image to create based on MIME type.
  if (aMimeType.EqualsLiteral(IMAGE_SVG_XML)) {
    return CreateVectorImage(aRequest, aProgressTracker, aMimeType,
                             aURI, imageFlags, aInnerWindowId);
  } else {
    return CreateRasterImage(aRequest, aProgressTracker, aMimeType,
                             aURI, imageFlags, aInnerWindowId);
  }
}

// Marks an image as having an error before returning it. Used with macros like
// NS_ENSURE_SUCCESS, since we guarantee to always return an image even if an
// error occurs, but callers need to be able to tell that this happened.
template <typename T>
static already_AddRefed<Image>
BadImage(nsRefPtr<T>& image)
{
  image->SetHasError();
  return image.forget();
}

/* static */ already_AddRefed<Image>
ImageFactory::CreateAnonymousImage(const nsCString& aMimeType)
{
  nsresult rv;

  nsRefPtr<RasterImage> newImage = new RasterImage();

  nsRefPtr<ProgressTracker> newTracker = new ProgressTracker();
  newTracker->SetImage(newImage);
  newImage->SetProgressTracker(newTracker);

  rv = newImage->Init(aMimeType.get(), Image::INIT_FLAG_SYNC_LOAD);
  NS_ENSURE_SUCCESS(rv, BadImage(newImage));

  return newImage.forget();
}

/* static */ already_AddRefed<MultipartImage>
ImageFactory::CreateMultipartImage(Image* aFirstPart,
                                   ProgressTracker* aProgressTracker)
{
  MOZ_ASSERT(aFirstPart);
  MOZ_ASSERT(aProgressTracker);

  nsRefPtr<MultipartImage> newImage = new MultipartImage(aFirstPart);
  aProgressTracker->SetImage(newImage);
  newImage->SetProgressTracker(aProgressTracker);

  newImage->Init();

  return newImage.forget();
}

int32_t
SaturateToInt32(int64_t val)
{
  if (val > INT_MAX) {
    return INT_MAX;
  }
  if (val < INT_MIN) {
    return INT_MIN;
  }

  return static_cast<int32_t>(val);
}

uint32_t
GetContentSize(nsIRequest* aRequest)
{
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  if (channel) {
    int64_t size;
    nsresult rv = channel->GetContentLength(&size);
    if (NS_SUCCEEDED(rv)) {
      return std::max(SaturateToInt32(size), 0);
    }
  }

  // Use the file size as a size hint for file channels.
  nsCOMPtr<nsIFileChannel> fileChannel(do_QueryInterface(aRequest));
  if (fileChannel) {
    nsCOMPtr<nsIFile> file;
    nsresult rv = fileChannel->GetFile(getter_AddRefs(file));
    if (NS_SUCCEEDED(rv)) {
      int64_t filesize;
      rv = file->GetFileSize(&filesize);
      if (NS_SUCCEEDED(rv)) {
        return std::max(SaturateToInt32(filesize), 0);
      }
    }
  }

  // Fallback - neither http nor file. We'll use dynamic allocation.
  return 0;
}

/* static */ already_AddRefed<Image>
ImageFactory::CreateRasterImage(nsIRequest* aRequest,
                                ProgressTracker* aProgressTracker,
                                const nsCString& aMimeType,
                                ImageURL* aURI,
                                uint32_t aImageFlags,
                                uint32_t aInnerWindowId)
{
  MOZ_ASSERT(aProgressTracker);

  nsresult rv;

  nsRefPtr<RasterImage> newImage = new RasterImage(aURI);
  aProgressTracker->SetImage(newImage);
  newImage->SetProgressTracker(aProgressTracker);

  rv = newImage->Init(aMimeType.get(), aImageFlags);
  NS_ENSURE_SUCCESS(rv, BadImage(newImage));

  newImage->SetInnerWindowID(aInnerWindowId);

  uint32_t len = GetContentSize(aRequest);

  // Pass anything usable on so that the RasterImage can preallocate
  // its source buffer.
  if (len > 0) {
    // Bound by something reasonable
    uint32_t sizeHint = std::min<uint32_t>(len, 20000000);
    rv = newImage->SetSourceSizeHint(sizeHint);
    if (NS_FAILED(rv)) {
      // Flush memory, try to get some back, and try again.
      rv = nsMemory::HeapMinimize(true);
      nsresult rv2 = newImage->SetSourceSizeHint(sizeHint);
      // If we've still failed at this point, things are going downhill.
      if (NS_FAILED(rv) || NS_FAILED(rv2)) {
        NS_WARNING("About to hit OOM in imagelib!");
      }
    }
  }

  nsAutoCString ref;
  aURI->GetRef(ref);
  net::nsMediaFragmentURIParser parser(ref);
  if (parser.HasResolution()) {
    newImage->SetRequestedResolution(parser.GetResolution());
  }

  if (parser.HasSampleSize()) {
      /* Get our principal */
      nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));
      nsCOMPtr<nsIPrincipal> principal;
      if (chan) {
        nsContentUtils::GetSecurityManager()
          ->GetChannelResultPrincipal(chan, getter_AddRefs(principal));
      }

      if ((principal &&
           principal->GetAppStatus() == nsIPrincipal::APP_STATUS_CERTIFIED) ||
          gfxPrefs::ImageMozSampleSizeEnabled()) {
        newImage->SetRequestedSampleSize(parser.GetSampleSize());
      }
  }

  return newImage.forget();
}

/* static */ already_AddRefed<Image>
ImageFactory::CreateVectorImage(nsIRequest* aRequest,
                                ProgressTracker* aProgressTracker,
                                const nsCString& aMimeType,
                                ImageURL* aURI,
                                uint32_t aImageFlags,
                                uint32_t aInnerWindowId)
{
  MOZ_ASSERT(aProgressTracker);

  nsresult rv;

  nsRefPtr<VectorImage> newImage = new VectorImage(aURI);
  aProgressTracker->SetImage(newImage);
  newImage->SetProgressTracker(aProgressTracker);

  rv = newImage->Init(aMimeType.get(), aImageFlags);
  NS_ENSURE_SUCCESS(rv, BadImage(newImage));

  newImage->SetInnerWindowID(aInnerWindowId);

  rv = newImage->OnStartRequest(aRequest, nullptr);
  NS_ENSURE_SUCCESS(rv, BadImage(newImage));

  return newImage.forget();
}

} // namespace image
} // namespace mozilla
