/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleSheetInfo_h
#define mozilla_StyleSheetInfo_h

#include "mozilla/dom/SRIMetadata.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "mozilla/CORSMode.h"

#include "nsIURI.h"

namespace mozilla {
class CSSStyleSheet;
} // namespace mozilla
class nsCSSRuleProcessor;
class nsIPrincipal;

namespace mozilla {

/**
 * Struct for data common to CSSStyleSheetInner and ServoStyleSheet.
 */
struct StyleSheetInfo
{
  typedef net::ReferrerPolicy ReferrerPolicy;

  StyleSheetInfo(CORSMode aCORSMode,
                 ReferrerPolicy aReferrerPolicy,
                 const dom::SRIMetadata& aIntegrity);

  nsCOMPtr<nsIURI>       mSheetURI; // for error reports, etc.
  nsCOMPtr<nsIURI>       mOriginalSheetURI;  // for GetHref.  Can be null.
  nsCOMPtr<nsIURI>       mBaseURI; // for resolving relative URIs
  nsCOMPtr<nsIPrincipal> mPrincipal;
  CORSMode               mCORSMode;
  // The Referrer Policy of a stylesheet is used for its child sheets, so it is
  // stored here.
  ReferrerPolicy         mReferrerPolicy;
  dom::SRIMetadata       mIntegrity;
  bool                   mComplete;
#ifdef DEBUG
  bool                   mPrincipalSet;
#endif
};

} // namespace mozilla

#endif // mozilla_StyleSheetInfo_h
