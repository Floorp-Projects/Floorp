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

ImageCacheKey::ImageCacheKey(nsIURI* aURI)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aURI);

  bool isChrome;
  mIsChrome = NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)) && isChrome;

  aURI->GetSpec(mSpec);
  mHash = ComputeHash(mSpec);
}

ImageCacheKey::ImageCacheKey(ImageURL* aURI)
{
  MOZ_ASSERT(aURI);

  bool isChrome;
  mIsChrome = NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)) && isChrome;

  aURI->GetSpec(mSpec);
  mHash = ComputeHash(mSpec);
}

ImageCacheKey::ImageCacheKey(const ImageCacheKey& aOther)
  : mSpec(aOther.mSpec)
  , mHash(aOther.mHash)
  , mIsChrome(aOther.mIsChrome)
{ }

ImageCacheKey::ImageCacheKey(ImageCacheKey&& aOther)
  : mSpec(Move(aOther.mSpec))
  , mHash(aOther.mHash)
  , mIsChrome(aOther.mIsChrome)
{ }

bool
ImageCacheKey::operator==(const ImageCacheKey& aOther) const
{
  return mSpec == aOther.mSpec;
}

/* static */ uint32_t
ImageCacheKey::ComputeHash(const nsACString& aSpec)
{
  // Since we frequently call Hash() several times in a row on the same
  // ImageCacheKey, as an optimization we compute our hash once and store it.
  return HashString(aSpec);
}

} // namespace image
} // namespace mozilla
