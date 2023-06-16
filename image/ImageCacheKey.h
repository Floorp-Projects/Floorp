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

enum CORSMode : uint8_t;

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
  ImageCacheKey(nsIURI*, CORSMode, const OriginAttributes&, dom::Document*);

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

  CORSMode GetCORSMode() const { return mCORSMode; }

  const OriginAttributes& OriginAttributesRef() const {
    return mOriginAttributes;
  }

  const nsCString& IsolationKeyRef() const { return mIsolationKey; }

  /// A token indicating which service worker controlled document this entry
  /// belongs to, if any.
  void* ControlledDocument() const { return mControlledDocument; }

 private:
  // For ServiceWorker we need to use the document as
  // token for the key. All those exceptions are handled by this method.
  static void* GetSpecialCaseDocumentToken(dom::Document* aDocument);

  // For anti-tracking we need to use an isolation key. It can be the suffix of
  // the PatitionedPrincipal (see StoragePrincipalHelper.h) or the top-level
  // document's base domain. This is handled by this method.
  static nsCString GetIsolationKey(dom::Document* aDocument, nsIURI* aURI);

  void EnsureHash() const;

  nsCOMPtr<nsIURI> mURI;
  const OriginAttributes mOriginAttributes;
  void* mControlledDocument;
  nsCString mIsolationKey;
  mutable Maybe<PLDHashNumber> mHash;
  const CORSMode mCORSMode;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_src_ImageCacheKey_h
