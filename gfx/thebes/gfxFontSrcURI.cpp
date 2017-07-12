/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxFontSrcURI.h"

#include "nsIProtocolHandler.h"
#include "nsProxyRelease.h"
#include "nsNetUtil.h"
#include "nsSimpleURI.h"
#include "nsURIHashKey.h"

static bool
HasFlag(nsIURI* aURI, uint32_t aFlag)
{
  nsresult rv;
  bool value = false;
  rv = NS_URIChainHasFlags(aURI, aFlag, &value);
  return NS_SUCCEEDED(rv) && value;
}

gfxFontSrcURI::gfxFontSrcURI(nsIURI* aURI)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aURI);

  mURI = aURI;

  // If we have a data: URI, we know that it is backed by an nsSimpleURI,
  // and that we don't need to serialize it ahead of time.
  nsCString scheme;
  mURI->GetScheme(scheme);

  if (scheme.EqualsLiteral("data")) {
    // We know that nsSimpleURI::From returns us a pointer to the same object,
    // and we hold a strong reference to the object in mURI, so no need to
    // hold it strongly here as well.  (And we'd have to NS_ReleaseOnMainThread
    // it in our destructor anyway.)
    RefPtr<mozilla::net::nsSimpleURI> simpleURI =
      mozilla::net::nsSimpleURI::From(aURI);
    mSimpleURI = simpleURI;

    NS_ASSERTION(mSimpleURI, "Why aren't our data: URLs backed by nsSimpleURI?");
  } else {
    mSimpleURI = nullptr;
  }

  if (!mSimpleURI) {
    mURI->GetSpec(mSpec);
  }

  mHash = nsURIHashKey::HashKey(mURI);

  mInheritsSecurityContext =
    HasFlag(aURI, nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT);
  mSyncLoadIsOK =
    HasFlag(aURI, nsIProtocolHandler::URI_SYNC_LOAD_IS_OK);
}

gfxFontSrcURI::~gfxFontSrcURI()
{
  NS_ReleaseOnMainThread("gfxFontSrcURI::mURI", mURI.forget());
}

bool
gfxFontSrcURI::Equals(gfxFontSrcURI* aOther)
{
  if (mSimpleURI) {
    if (aOther->mSimpleURI) {
      return mSimpleURI->Equals(aOther->mSimpleURI);
    }

    // The two URIs are probably different.  Do a quick check on the
    // schemes before deciding to serialize mSimpleURI (which might be
    // quite large).
    {
      nsCString thisScheme;
      mSimpleURI->GetScheme(thisScheme);

      nsCString otherScheme;
      if (!StringBeginsWith(aOther->mSpec, thisScheme,
                            nsDefaultCStringComparator())) {
        return false;
      }
    }

    nsCString thisSpec;
    mSimpleURI->GetSpec(thisSpec);
    return thisSpec == aOther->mSpec;
  }

  if (aOther->mSimpleURI) {
    return aOther->Equals(this);
  }

  return mSpec == aOther->mSpec;
}

nsresult
gfxFontSrcURI::GetSpec(nsACString& aResult)
{
  if (mSimpleURI) {
    return mSimpleURI->GetSpec(aResult);
  }

  aResult = mSpec;
  return NS_OK;
}

nsCString
gfxFontSrcURI::GetSpecOrDefault()
{
  if (mSimpleURI) {
    return mSimpleURI->GetSpecOrDefault();
  }

  return mSpec;
}
