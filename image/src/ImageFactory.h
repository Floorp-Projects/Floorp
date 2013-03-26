/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IMAGELIB_IMAGEFACTORY_H_
#define MOZILLA_IMAGELIB_IMAGEFACTORY_H_

#include "nsCOMPtr.h"

class nsCString;
class nsIRequest;
class nsIURI;
class imgStatusTracker;

namespace mozilla {
namespace image {

class Image;

class ImageFactory
{
public:
  /**
   * Creates a new image with the given properties.
   *
   * @param aRequest       The associated request.
   * @param aStatusTracker A status tracker for the image to use.
   * @param aMimeType      The mimetype of the image.
   * @param aURI           The URI of the image.
   * @param aIsMultiPart   Whether the image is part of a multipart request.
   * @param aInnerWindowId The window this image belongs to.
   */
  static already_AddRefed<Image> CreateImage(nsIRequest* aRequest,
                                             imgStatusTracker* aStatusTracker,
                                             const nsCString& aMimeType,
                                             nsIURI* aURI,
                                             bool aIsMultiPart,
                                             uint32_t aInnerWindowId);
  /**
   * Creates a new image which isn't associated with a URI or loaded through
   * the usual image loading mechanism.
   *
   * @param aMimeType      The mimetype of the image.
   */
  static already_AddRefed<Image> CreateAnonymousImage(const nsCString& aMimeType);

private:
  // Factory functions that create specific types of image containers.
  static already_AddRefed<Image> CreateRasterImage(nsIRequest* aRequest,
                                                   imgStatusTracker* aStatusTracker,
                                                   const nsCString& aMimeType,
                                                   nsIURI* aURI,
                                                   uint32_t aImageFlags,
                                                   uint32_t aInnerWindowId);

  static already_AddRefed<Image> CreateVectorImage(nsIRequest* aRequest,
                                                   imgStatusTracker* aStatusTracker,
                                                   const nsCString& aMimeType,
                                                   nsIURI* aURI,
                                                   uint32_t aImageFlags,
                                                   uint32_t aInnerWindowId);

  // This is a static factory class, so disallow instantiation.
  virtual ~ImageFactory() = 0;
};

} // namespace image
} // namespace mozilla

#endif // MOZILLA_IMAGELIB_IMAGEFACTORY_H_
