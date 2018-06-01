/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* thread-safe container of information for resolving url values */

#ifndef mozilla_URLExtraData_h
#define mozilla_URLExtraData_h

#include "mozilla/dom/URL.h"
#include "mozilla/Move.h"
#include "mozilla/StaticPtr.h"

#include "nsCOMPtr.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"

namespace mozilla {

struct URLExtraData
{
  URLExtraData(already_AddRefed<nsIURI> aBaseURI,
               already_AddRefed<nsIURI> aReferrer,
               already_AddRefed<nsIPrincipal> aPrincipal)
    : mBaseURI(std::move(aBaseURI))
    , mReferrer(std::move(aReferrer))
    , mPrincipal(std::move(aPrincipal))
      // When we hold the URI data of a style sheet, mReferrer is always
      // equal to the sheet URI.
    , mIsChrome(mReferrer ? dom::IsChromeURI(mReferrer) : false)
  {
    MOZ_ASSERT(mBaseURI);
  }

  URLExtraData(nsIURI* aBaseURI, nsIURI* aReferrer, nsIPrincipal* aPrincipal)
    : URLExtraData(do_AddRef(aBaseURI),
                   do_AddRef(aReferrer),
                   do_AddRef(aPrincipal)) {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(URLExtraData)

  nsIURI* BaseURI() const { return mBaseURI; }
  nsIURI* GetReferrer() const { return mReferrer; }
  nsIPrincipal* GetPrincipal() const { return mPrincipal; }

  static URLExtraData* Dummy() {
    MOZ_ASSERT(sDummy);
    return sDummy;
  }
  static void InitDummy();
  static void ReleaseDummy();

private:
  ~URLExtraData();

  nsCOMPtr<nsIURI> mBaseURI;
  nsCOMPtr<nsIURI> mReferrer;
  nsCOMPtr<nsIPrincipal> mPrincipal;

  // True if mReferrer is a chrome:// URI.
  const bool mIsChrome;

  static StaticRefPtr<URLExtraData> sDummy;
};

} // namespace mozilla

#endif // mozilla_URLExtraData_h
