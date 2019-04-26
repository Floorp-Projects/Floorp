/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ThirdPartyUtil_h__
#define ThirdPartyUtil_h__

#include "nsCOMPtr.h"
#include "nsString.h"
#include "mozIThirdPartyUtil.h"
#include "nsEffectiveTLDService.h"
#include "mozilla/Attributes.h"

class nsIURI;

class ThirdPartyUtil final : public mozIThirdPartyUtil {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZITHIRDPARTYUTIL

  nsresult Init();

  static void Startup();
  static ThirdPartyUtil* GetInstance();

 private:
  ~ThirdPartyUtil();

  bool IsThirdPartyInternal(const nsCString& aFirstDomain,
                            const nsCString& aSecondDomain) {
    // Check strict equality.
    return aFirstDomain != aSecondDomain;
  }
  nsresult IsThirdPartyInternal(const nsCString& aFirstDomain,
                                nsIURI* aSecondURI, bool* aResult);

  nsCString GetBaseDomainFromWindow(nsPIDOMWindowOuter* aWindow) {
    mozilla::dom::Document* doc = aWindow ? aWindow->GetExtantDoc() : nullptr;

    if (!doc) {
      return EmptyCString();
    }

    return doc->GetBaseDomain();
  }

  RefPtr<nsEffectiveTLDService> mTLDService;
};

#endif
