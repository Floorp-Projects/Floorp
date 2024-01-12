/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageCacheKey.h"

#include <utility>

#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/StorageAccess.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StorageAccess.h"
#include "nsContentUtils.h"
#include "nsHashKeys.h"
#include "nsLayoutUtils.h"
#include "nsPrintfCString.h"
#include "nsString.h"

namespace mozilla {

using namespace dom;

namespace image {

ImageCacheKey::ImageCacheKey(nsIURI* aURI, CORSMode aCORSMode,
                             const OriginAttributes& aAttrs,
                             Document* aDocument)
    : mURI(aURI),
      mOriginAttributes(aAttrs),
      mControlledDocument(GetSpecialCaseDocumentToken(aDocument)),
      mIsolationKey(GetIsolationKey(aDocument, aURI)),
      mCORSMode(aCORSMode) {}

ImageCacheKey::ImageCacheKey(const ImageCacheKey& aOther)
    : mURI(aOther.mURI),
      mOriginAttributes(aOther.mOriginAttributes),
      mControlledDocument(aOther.mControlledDocument),
      mIsolationKey(aOther.mIsolationKey),
      mHash(aOther.mHash),
      mCORSMode(aOther.mCORSMode) {}

ImageCacheKey::ImageCacheKey(ImageCacheKey&& aOther)
    : mURI(std::move(aOther.mURI)),
      mOriginAttributes(aOther.mOriginAttributes),
      mControlledDocument(aOther.mControlledDocument),
      mIsolationKey(aOther.mIsolationKey),
      mHash(aOther.mHash),
      mCORSMode(aOther.mCORSMode) {}

bool ImageCacheKey::operator==(const ImageCacheKey& aOther) const {
  // Don't share the image cache between a controlled document and anything
  // else.
  if (mControlledDocument != aOther.mControlledDocument) {
    return false;
  }
  // Don't share the image cache between two top-level documents of different
  // base domains.
  if (!mIsolationKey.Equals(aOther.mIsolationKey,
                            nsCaseInsensitiveCStringComparator)) {
    return false;
  }
  // The origin attributes always have to match.
  if (mOriginAttributes != aOther.mOriginAttributes) {
    return false;
  }

  if (mCORSMode != aOther.mCORSMode) {
    return false;
  }

  // For non-blob URIs, compare the URIs.
  bool equals = false;
  nsresult rv = mURI->Equals(aOther.mURI, &equals);
  return NS_SUCCEEDED(rv) && equals;
}

void ImageCacheKey::EnsureHash() const {
  MOZ_ASSERT(mHash.isNothing());
  PLDHashNumber hash = 0;

  // Since we frequently call Hash() several times in a row on the same
  // ImageCacheKey, as an optimization we compute our hash once and store it.

  nsPrintfCString ptr("%p", mControlledDocument);
  nsAutoCString suffix;
  mOriginAttributes.CreateSuffix(suffix);

  nsAutoCString spec;
  Unused << mURI->GetSpec(spec);
  hash = HashString(spec);

  hash = AddToHash(hash, HashString(suffix), HashString(mIsolationKey),
                   HashString(ptr));
  mHash.emplace(hash);
}

/* static */
void* ImageCacheKey::GetSpecialCaseDocumentToken(Document* aDocument) {
  // Cookie-averse documents can never have storage granted to them.  Since they
  // may not have inner windows, they would require special handling below, so
  // just bail out early here.
  if (!aDocument || aDocument->IsCookieAverse()) {
    return nullptr;
  }

  // For controlled documents, we cast the pointer into a void* to avoid
  // dereferencing it (since we only use it for comparisons).
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (swm && aDocument->GetController().isSome()) {
    return aDocument;
  }

  return nullptr;
}

/* static */
nsCString ImageCacheKey::GetIsolationKey(Document* aDocument, nsIURI* aURI) {
  if (!aDocument || !aDocument->GetInnerWindow()) {
    return ""_ns;
  }

  // Network-state isolation
  if (StaticPrefs::privacy_partition_network_state()) {
    OriginAttributes oa;
    StoragePrincipalHelper::GetOriginAttributesForNetworkState(aDocument, oa);

    nsAutoCString suffix;
    oa.CreateSuffix(suffix);

    return std::move(suffix);
  }

  // If the window is 3rd party resource, let's see if first-party storage
  // access is granted for this image.
  if (AntiTrackingUtils::IsThirdPartyWindow(aDocument->GetInnerWindow(),
                                            nullptr)) {
    uint32_t rejectedReason = 0;
    Unused << rejectedReason;
    return ShouldAllowAccessFor(aDocument->GetInnerWindow(), aURI,
                                &rejectedReason)
               ? ""_ns
               : aDocument->GetBaseDomain();
  }

  // Another scenario is if this image is a 3rd party resource loaded by a
  // first party context. In this case, we should check if the nsIChannel has
  // been marked as tracking resource, but we don't have the channel yet at
  // this point.  The best approach here is to be conservative: if we are sure
  // that the permission is granted, let's return 0. Otherwise, let's make a
  // unique image cache per the top-level document eTLD+1.
  if (!ApproximateAllowAccessForWithoutChannel(aDocument->GetInnerWindow(),
                                               aURI)) {
    // If we are here, the image is a 3rd-party resource loaded by a first-party
    // context. We can just use the document's base domain as the key because it
    // should be the same as the top-level document's base domain.
    return aDocument
        ->GetBaseDomain();  // because we don't have anything better!
  }

  return ""_ns;
}

}  // namespace image
}  // namespace mozilla
