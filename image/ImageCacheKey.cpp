/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageCacheKey.h"

#include "mozilla/Move.h"
#include "ImageURL.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsLayoutUtils.h"
#include "nsString.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/workers/ServiceWorkerManager.h"
#include "nsIDocument.h"
#include "nsPrintfCString.h"

namespace mozilla {

using namespace dom;

namespace image {

bool
URISchemeIs(ImageURL* aURI, const char* aScheme)
{
  bool schemeMatches = false;
  if (NS_WARN_IF(NS_FAILED(aURI->SchemeIs(aScheme, &schemeMatches)))) {
    return false;
  }
  return schemeMatches;
}

static Maybe<uint64_t>
BlobSerial(ImageURL* aURI)
{
  nsAutoCString spec;
  aURI->GetSpec(spec);

  RefPtr<BlobImpl> blob;
  if (NS_SUCCEEDED(NS_GetBlobForBlobURISpec(spec, getter_AddRefs(blob))) &&
      blob) {
    return Some(blob->GetSerialNumber());
  }

  return Nothing();
}

ImageCacheKey::ImageCacheKey(nsIURI* aURI,
                             const OriginAttributes& aAttrs,
                             nsIDocument* aDocument,
                             nsresult& aRv)
  : mURI(new ImageURL(aURI, aRv))
  , mOriginAttributes(aAttrs)
  , mControlledDocument(GetControlledDocumentToken(aDocument))
  , mIsChrome(URISchemeIs(mURI, "chrome"))
  , mIsStyloEnabled(nsLayoutUtils::StyloEnabled())
{
  NS_ENSURE_SUCCESS_VOID(aRv);

  MOZ_ASSERT(NS_IsMainThread());

  if (URISchemeIs(mURI, "blob")) {
    mBlobSerial = BlobSerial(mURI);
  }

  mHash = ComputeHash(mURI, mBlobSerial, mOriginAttributes, mControlledDocument,
                      mIsStyloEnabled);
}

ImageCacheKey::ImageCacheKey(ImageURL* aURI,
                             const OriginAttributes& aAttrs,
                             nsIDocument* aDocument)
  : mURI(aURI)
  , mOriginAttributes(aAttrs)
  , mControlledDocument(GetControlledDocumentToken(aDocument))
  , mIsChrome(URISchemeIs(mURI, "chrome"))
  , mIsStyloEnabled(nsLayoutUtils::StyloEnabled())
{
  MOZ_ASSERT(aURI);

  if (URISchemeIs(mURI, "blob")) {
    mBlobSerial = BlobSerial(mURI);
  }

  mHash = ComputeHash(mURI, mBlobSerial, mOriginAttributes, mControlledDocument,
                      mIsStyloEnabled);
}

ImageCacheKey::ImageCacheKey(const ImageCacheKey& aOther)
  : mURI(aOther.mURI)
  , mBlobSerial(aOther.mBlobSerial)
  , mOriginAttributes(aOther.mOriginAttributes)
  , mControlledDocument(aOther.mControlledDocument)
  , mHash(aOther.mHash)
  , mIsChrome(aOther.mIsChrome)
  , mIsStyloEnabled(aOther.mIsStyloEnabled)
{ }

ImageCacheKey::ImageCacheKey(ImageCacheKey&& aOther)
  : mURI(Move(aOther.mURI))
  , mBlobSerial(Move(aOther.mBlobSerial))
  , mOriginAttributes(aOther.mOriginAttributes)
  , mControlledDocument(aOther.mControlledDocument)
  , mHash(aOther.mHash)
  , mIsChrome(aOther.mIsChrome)
  , mIsStyloEnabled(aOther.mIsStyloEnabled)
{ }

bool
ImageCacheKey::operator==(const ImageCacheKey& aOther) const
{
  if (mIsStyloEnabled != aOther.mIsStyloEnabled) {
    return false;
  }
  // Don't share the image cache between a controlled document and anything else.
  if (mControlledDocument != aOther.mControlledDocument) {
    return false;
  }
  // The origin attributes always have to match.
  if (mOriginAttributes != aOther.mOriginAttributes) {
    return false;
  }
  if (mBlobSerial || aOther.mBlobSerial) {
    // If at least one of us has a blob serial, just compare the blob serial and
    // the ref portion of the URIs.
    return mBlobSerial == aOther.mBlobSerial &&
           mURI->HasSameRef(*aOther.mURI);
  }

  // For non-blob URIs, compare the URIs.
  return *mURI == *aOther.mURI;
}

const char*
ImageCacheKey::Spec() const
{
  return mURI->Spec();
}

/* static */ PLDHashNumber
ImageCacheKey::ComputeHash(ImageURL* aURI,
                           const Maybe<uint64_t>& aBlobSerial,
                           const OriginAttributes& aAttrs,
                           void* aControlledDocument,
                           bool aIsStyloEnabled)
{
  // Since we frequently call Hash() several times in a row on the same
  // ImageCacheKey, as an optimization we compute our hash once and store it.

  nsPrintfCString ptr("%p", aControlledDocument);
  nsAutoCString suffix;
  aAttrs.CreateSuffix(suffix);

  return AddToHash(0, aURI->ComputeHash(aBlobSerial),
                   HashString(suffix), HashString(ptr),
                   aIsStyloEnabled);
}

/* static */ void*
ImageCacheKey::GetControlledDocumentToken(nsIDocument* aDocument)
{
  // For non-controlled documents, we just return null.  For controlled
  // documents, we cast the pointer into a void* to avoid dereferencing
  // it (since we only use it for comparisons), and return it.
  void* pointer = nullptr;
  using dom::workers::ServiceWorkerManager;
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (aDocument && swm) {
    ErrorResult rv;
    if (aDocument->GetController().isSome()) {
      pointer = aDocument;
    }
  }
  return pointer;
}

} // namespace image
} // namespace mozilla
