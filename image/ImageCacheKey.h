/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * ImageCacheKey is the key type for the image cache (see imgLoader.h).
 */

#ifndef mozilla_image_src_ImageCacheKey_h
#define mozilla_image_src_ImageCacheKey_h

#include "mozilla/Maybe.h"

class nsIDOMDocument;
class nsIURI;

namespace mozilla {
namespace image {

class ImageURL;

/**
 * An ImageLib cache entry key.
 *
 * We key the cache on the initial URI (before any redirects), with some
 * canonicalization applied. See ComputeHash() for the details.
 * Controlled documents do not share their cache entries with
 * non-controlled documents, or other controlled documents.
 */
class ImageCacheKey final
{
public:
  ImageCacheKey(nsIURI* aURI, nsIDOMDocument* aDocument);
  ImageCacheKey(ImageURL* aURI, nsIDOMDocument* aDocument);

  ImageCacheKey(const ImageCacheKey& aOther);
  ImageCacheKey(ImageCacheKey&& aOther);

  bool operator==(const ImageCacheKey& aOther) const;
  uint32_t Hash() const { return mHash; }

  /// A weak pointer to the URI spec for this cache entry. For logging only.
  const char* Spec() const;

  /// Is this cache entry for a chrome image?
  bool IsChrome() const { return mIsChrome; }

private:
  static uint32_t ComputeHash(ImageURL* aURI,
                              const Maybe<uint64_t>& aBlobSerial,
                              void* aControlledDocument);
  static void* GetControlledDocumentToken(nsIDOMDocument* aDocument);

  RefPtr<ImageURL> mURI;
  Maybe<uint64_t> mBlobSerial;
  void* mControlledDocument;
  uint32_t mHash;
  bool mIsChrome;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_src_ImageCacheKey_h
