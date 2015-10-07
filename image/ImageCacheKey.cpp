/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageCacheKey.h"

#include "mozilla/Move.h"
#include "File.h"
#include "ImageURL.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsString.h"

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

  nsRefPtr<BlobImpl> blob;
  if (NS_SUCCEEDED(NS_GetBlobForBlobURISpec(spec, getter_AddRefs(blob))) &&
      blob) {
    return Some(blob->GetSerialNumber());
  }

  return Nothing();
}

ImageCacheKey::ImageCacheKey(nsIURI* aURI)
  : mURI(new ImageURL(aURI))
  , mIsChrome(URISchemeIs(mURI, "chrome"))
{
  MOZ_ASSERT(NS_IsMainThread());

  if (URISchemeIs(mURI, "blob")) {
    mBlobSerial = BlobSerial(mURI);
  }

  mHash = ComputeHash(mURI, mBlobSerial);
}

ImageCacheKey::ImageCacheKey(ImageURL* aURI)
  : mURI(aURI)
  , mIsChrome(URISchemeIs(mURI, "chrome"))
{
  MOZ_ASSERT(aURI);

  if (URISchemeIs(mURI, "blob")) {
    mBlobSerial = BlobSerial(mURI);
  }

  mHash = ComputeHash(mURI, mBlobSerial);
}

ImageCacheKey::ImageCacheKey(const ImageCacheKey& aOther)
  : mURI(aOther.mURI)
  , mBlobSerial(aOther.mBlobSerial)
  , mHash(aOther.mHash)
  , mIsChrome(aOther.mIsChrome)
{ }

ImageCacheKey::ImageCacheKey(ImageCacheKey&& aOther)
  : mURI(Move(aOther.mURI))
  , mBlobSerial(Move(aOther.mBlobSerial))
  , mHash(aOther.mHash)
  , mIsChrome(aOther.mIsChrome)
{ }

bool
ImageCacheKey::operator==(const ImageCacheKey& aOther) const
{
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

/* static */ uint32_t
ImageCacheKey::ComputeHash(ImageURL* aURI,
                           const Maybe<uint64_t>& aBlobSerial)
{
  // Since we frequently call Hash() several times in a row on the same
  // ImageCacheKey, as an optimization we compute our hash once and store it.

  if (aBlobSerial) {
    // For blob URIs, we hash the serial number of the underlying blob, so that
    // different blob URIs which point to the same blob share a cache entry. We
    // also include the ref portion of the URI to support -moz-samplesize, which
    // requires us to create different Image objects even if the source data is
    // the same.
    nsAutoCString ref;
    aURI->GetRef(ref);
    return HashGeneric(*aBlobSerial, HashString(ref));
  }

  // For non-blob URIs, we hash the URI spec.
  nsAutoCString spec;
  aURI->GetSpec(spec);
  return HashString(spec);
}

} // namespace image
} // namespace mozilla
