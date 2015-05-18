/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_ImageFactory_h
#define mozilla_image_ImageFactory_h

#include "nsCOMPtr.h"
#include "nsProxyRelease.h"

class nsCString;
class nsIRequest;

namespace mozilla {
namespace image {

class Image;
class ImageURL;
class MultipartImage;
class ProgressTracker;

class ImageFactory
{
public:
  /**
   * Registers vars with Preferences. Should only be called on the main thread.
   */
  static void Initialize();

  /**
   * Creates a new image with the given properties.
   * Can be called on or off the main thread.
   *
   * @param aRequest         The associated request.
   * @param aProgressTracker A status tracker for the image to use.
   * @param aMimeType        The mimetype of the image.
   * @param aURI             The URI of the image.
   * @param aIsMultiPart     Whether the image is part of a multipart request.
   * @param aInnerWindowId   The window this image belongs to.
   */
  static already_AddRefed<Image> CreateImage(nsIRequest* aRequest,
                                             ProgressTracker* aProgressTracker,
                                             const nsCString& aMimeType,
                                             ImageURL* aURI,
                                             bool aIsMultiPart,
                                             uint32_t aInnerWindowId);
  /**
   * Creates a new image which isn't associated with a URI or loaded through
   * the usual image loading mechanism.
   *
   * @param aMimeType      The mimetype of the image.
   */
  static already_AddRefed<Image>
  CreateAnonymousImage(const nsCString& aMimeType);

  /**
   * Creates a new multipart/x-mixed-replace image wrapper, and initializes it
   * with the first part. Subsequent parts should be passed to the existing
   * MultipartImage via MultipartImage::BeginTransitionToPart().
   *
   * @param aFirstPart       An image containing the first part of the multipart
   *                         stream.
   * @param aProgressTracker A progress tracker for the multipart image.
   */
  static already_AddRefed<MultipartImage>
  CreateMultipartImage(Image* aFirstPart, ProgressTracker* aProgressTracker);

private:
  // Factory functions that create specific types of image containers.
  static already_AddRefed<Image>
  CreateRasterImage(nsIRequest* aRequest,
                    ProgressTracker* aProgressTracker,
                    const nsCString& aMimeType,
                    ImageURL* aURI,
                    uint32_t aImageFlags,
                    uint32_t aInnerWindowId);

  static already_AddRefed<Image>
  CreateVectorImage(nsIRequest* aRequest,
                    ProgressTracker* aProgressTracker,
                    const nsCString& aMimeType,
                    ImageURL* aURI,
                    uint32_t aImageFlags,
                    uint32_t aInnerWindowId);

  // This is a static factory class, so disallow instantiation.
  virtual ~ImageFactory() = 0;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_ImageFactory_h
