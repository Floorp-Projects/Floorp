/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ThirdPartyUtil_h__
#define ThirdPartyUtil_h__

#include <cstdint>
#include "ErrorList.h"
#include "mozIThirdPartyUtil.h"
#include "mozilla/RefPtr.h"
#include "nsISupports.h"
#include "nsString.h"

class mozIDOMWindowProxy;
class nsEffectiveTLDService;
class nsIChannel;
class nsIPrincipal;
class nsIURI;
class nsPIDOMWindowOuter;

namespace mozilla::dom {
class WindowGlobalParent;
}  // namespace mozilla::dom

class ThirdPartyUtil final : public mozIThirdPartyUtil {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZITHIRDPARTYUTIL

  nsresult Init();

  static void Startup();
  static ThirdPartyUtil* GetInstance();

  nsresult IsThirdPartyGlobal(mozilla::dom::WindowGlobalParent* aWindowGlobal,
                              bool* aResult);

 private:
  ~ThirdPartyUtil();

  bool IsThirdPartyInternal(const nsCString& aFirstDomain,
                            const nsCString& aSecondDomain) {
    // Check strict equality.
    return aFirstDomain != aSecondDomain;
  }
  nsresult IsThirdPartyInternal(const nsCString& aFirstDomain,
                                nsIURI* aSecondURI, bool* aResult);

  nsCString GetBaseDomainFromWindow(nsPIDOMWindowOuter* aWindow);

  RefPtr<nsEffectiveTLDService> mTLDService;
};

#endif
