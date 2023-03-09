/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* thread-safe container of information for resolving url values */

#ifndef mozilla_URLExtraData_h
#define mozilla_URLExtraData_h

#include <utility>

#include "mozilla/StaticPtr.h"
#include "mozilla/UserAgentStyleSheetID.h"
#include "nsCOMPtr.h"
#include "nsIPrincipal.h"
#include "nsIReferrerInfo.h"
#include "nsIURI.h"

namespace mozilla {

struct URLExtraData {
  static bool ChromeRulesEnabled(nsIURI* aURI);

  URLExtraData(already_AddRefed<nsIURI> aBaseURI,
               already_AddRefed<nsIReferrerInfo> aReferrerInfo,
               already_AddRefed<nsIPrincipal> aPrincipal)
      : mBaseURI(std::move(aBaseURI)),
        mReferrerInfo(std::move(aReferrerInfo)),
        mPrincipal(std::move(aPrincipal)) {
    MOZ_ASSERT(mBaseURI);
    MOZ_ASSERT(mPrincipal);
    MOZ_ASSERT(mReferrerInfo);
    // When we hold the URI data of a style sheet, referrer is always
    // equal to the sheet URI.
    nsCOMPtr<nsIURI> referrer = mReferrerInfo->GetOriginalReferrer();
    mChromeRulesEnabled = ChromeRulesEnabled(referrer);
  }

  URLExtraData(nsIURI* aBaseURI, nsIReferrerInfo* aReferrerInfo,
               nsIPrincipal* aPrincipal)
      : URLExtraData(do_AddRef(aBaseURI), do_AddRef(aReferrerInfo),
                     do_AddRef(aPrincipal)) {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(URLExtraData)

  nsIURI* BaseURI() const { return mBaseURI; }
  nsIReferrerInfo* ReferrerInfo() const { return mReferrerInfo; }
  nsIPrincipal* Principal() const { return mPrincipal; }

  bool ChromeRulesEnabled() const { return mChromeRulesEnabled; }

  static URLExtraData* Dummy() {
    MOZ_ASSERT(sDummy);
    return sDummy;
  }

  static URLExtraData* DummyChrome() {
    MOZ_ASSERT(sDummyChrome);
    return sDummyChrome;
  }

  static void Init();
  static void Shutdown();

  // URLExtraData objects that shared style sheets use a sheet ID index to
  // refer to.
  static StaticRefPtr<URLExtraData>
      sShared[size_t(UserAgentStyleSheetID::Count)];

 private:
  ~URLExtraData();

  nsCOMPtr<nsIURI> mBaseURI;
  nsCOMPtr<nsIReferrerInfo> mReferrerInfo;
  nsCOMPtr<nsIPrincipal> mPrincipal;

  bool mChromeRulesEnabled;

  static StaticRefPtr<URLExtraData> sDummy;
  static StaticRefPtr<URLExtraData> sDummyChrome;
};

}  // namespace mozilla

#endif  // mozilla_URLExtraData_h
