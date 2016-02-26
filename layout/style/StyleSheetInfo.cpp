/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StyleSheetInfo.h"

#include "nsIURI.h"
#include "nsNullPrincipal.h"

namespace mozilla {

using namespace mozilla::dom;

StyleSheetInfo::StyleSheetInfo(CORSMode aCORSMode,
                               ReferrerPolicy aReferrerPolicy,
                               const SRIMetadata& aIntegrity)
  : mPrincipal(nsNullPrincipal::Create())
  , mCORSMode(aCORSMode)
  , mReferrerPolicy(aReferrerPolicy)
  , mIntegrity(aIntegrity)
  , mComplete(false)
#ifdef DEBUG
  , mPrincipalSet(false)
#endif
{
  if (!mPrincipal) {
    NS_RUNTIMEABORT("nsNullPrincipal::Init failed");
  }
}

StyleSheetInfo::StyleSheetInfo(const StyleSheetInfo& aCopy)
  : mSheetURI(aCopy.mSheetURI)
  , mOriginalSheetURI(aCopy.mOriginalSheetURI)
  , mBaseURI(aCopy.mBaseURI)
  , mPrincipal(aCopy.mPrincipal)
  , mCORSMode(aCopy.mCORSMode)
  , mReferrerPolicy(aCopy.mReferrerPolicy)
  , mIntegrity(aCopy.mIntegrity)
  , mComplete(aCopy.mComplete)
#ifdef DEBUG
  , mPrincipalSet(aCopy.mPrincipalSet)
#endif
{
}

void
StyleSheetInfo::SetURIs(nsIURI* aSheetURI,
                        nsIURI* aOriginalSheetURI,
                        nsIURI* aBaseURI)
{
  NS_PRECONDITION(aSheetURI && aBaseURI, "null ptr");

  mSheetURI = aSheetURI;
  mOriginalSheetURI = aOriginalSheetURI;
  mBaseURI = aBaseURI;
}

void
StyleSheetInfo::SetPrincipal(nsIPrincipal* aPrincipal)
{
  NS_PRECONDITION(!mPrincipalSet, "Should only set principal once");

  if (aPrincipal) {
    mPrincipal = aPrincipal;
#ifdef DEBUG
    mPrincipalSet = true;
#endif
  }
}

} // namespace mozilla
