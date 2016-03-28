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

namespace mozilla {
class CSSStyleSheet;
} // namespace mozilla
class nsCSSRuleProcessor;
class nsIPrincipal;
class nsIURI;

namespace mozilla {

/**
 * Superclass for data common to CSSStyleSheetInner and ServoStyleSheet.
 */
class StyleSheetInfo
{
public:
  friend class mozilla::CSSStyleSheet;
  friend class ::nsCSSRuleProcessor;
  typedef net::ReferrerPolicy ReferrerPolicy;

  StyleSheetInfo(CORSMode aCORSMode,
                 ReferrerPolicy aReferrerPolicy,
                 const dom::SRIMetadata& aIntegrity);
  StyleSheetInfo(const StyleSheetInfo& aCopy);

  nsIURI* GetSheetURI() const { return mSheetURI; }
  nsIURI* GetOriginalURI() const { return mOriginalSheetURI; }
  nsIURI* GetBaseURI() const { return mBaseURI; }
  void SetURIs(nsIURI* aSheetURI, nsIURI* aOriginalSheetURI, nsIURI* aBaseURI);

  // Whether the sheet is for an inline <style> element.
  bool IsInline() const { return !mOriginalSheetURI; }

  nsIPrincipal* Principal() const { return mPrincipal; }
  void SetPrincipal(nsIPrincipal* aPrincipal);

  CORSMode GetCORSMode() const { return mCORSMode; }
  net::ReferrerPolicy GetReferrerPolicy() const { return mReferrerPolicy; }
  void GetIntegrity(dom::SRIMetadata& aResult) const { aResult = mIntegrity; }

protected:
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

  friend class StyleSheet;
};

} // namespace mozilla

#endif // mozilla_StyleSheetInfo_h
