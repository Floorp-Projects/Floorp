/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * ImageCacheKey is the key type for the image cache (see imgLoader.h).
 */

#ifndef mozilla_image_src_ImageCacheKey_h
#define mozilla_image_src_ImageCacheKey_h

#include "mozilla/BasePrincipal.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "PLDHashTable.h"

class nsIURI;

namespace mozilla {
namespace image {

/**
 * An ImageLib cache entry key.
 *
 * We key the cache on the initial URI (before any redirects), with some
 * canonicalization applied. See ComputeHash() for the details.
 * Controlled documents do not share their cache entries with
 * non-controlled documents, or other controlled documents.
 */
class ImageCacheKey final {
 public:
  ImageCacheKey(nsIURI* aURI, const OriginAttributes& aAttrs,
                dom::Document* aDocument);

  ImageCacheKey(const ImageCacheKey& aOther);
  ImageCacheKey(ImageCacheKey&& aOther);

  bool operator==(const ImageCacheKey& aOther) const;
  PLDHashNumber Hash() const {
    if (MOZ_UNLIKELY(mHash.isNothing())) {
      EnsureHash();
    }
    return mHash.value();
  }

  /// A weak pointer to the URI.
  nsIURI* URI() const { return mURI; }

  const OriginAttributes& OriginAttributesRef() const {
    return mOriginAttributes;
  }

  /// Is this cache entry for a chrome image?
  bool IsChrome() const { return mIsChrome; }

  /// A token indicating which service worker controlled document this entry
  /// belongs to, if any.
  void* ControlledDocument() const { return mControlledDocument; }

 private:
  // For ServiceWorker we need to use the document as
  // token for the key. All those exceptions are handled by this method.
  static void* GetSpecialCaseDocumentToken(dom::Document* aDocument);

  // For anti-tracking we need to use the top-level document's base domain for
  // the key. This is handled by this method.
  static nsCString GetTopLevelBaseDomain(dom::Document* aDocument,
                                         nsIURI* aURI);

  void EnsureHash() const;
  void EnsureBlobRef() const;

  nsCOMPtr<nsIURI> mURI;
  Maybe<uint64_t> mBlobSerial;
  mutable nsCString mBlobRef;
  OriginAttributes mOriginAttributes;
  void* mControlledDocument;
  nsCString mTopLevelBaseDomain;
  mutable Maybe<PLDHashNumber> mHash;
  bool mIsChrome;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_src_ImageCacheKey_h
