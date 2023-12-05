/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnService_h_
#define mozilla_dom_WebAuthnService_h_

#include "nsIWebAuthnService.h"
#include "AuthrsBridge_ffi.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "AndroidWebAuthnService.h"
#endif

#ifdef XP_MACOSX
#  include "MacOSWebAuthnService.h"
#endif

#ifdef XP_WIN
#  include "WinWebAuthnService.h"
#endif

namespace mozilla::dom {

already_AddRefed<nsIWebAuthnService> NewWebAuthnService();

class WebAuthnService final : public nsIWebAuthnService {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWEBAUTHNSERVICE

  WebAuthnService() {
    Unused << authrs_service_constructor(getter_AddRefs(mTestService));
#if defined(XP_WIN)
    if (WinWebAuthnService::AreWebAuthNApisAvailable()) {
      mPlatformService = new WinWebAuthnService();
    } else {
      mPlatformService = mTestService;
    }
#elif defined(MOZ_WIDGET_ANDROID)
    mPlatformService = new AndroidWebAuthnService();
#elif defined(XP_MACOSX)
    if (__builtin_available(macos 13.3, *)) {
      mPlatformService = NewMacOSWebAuthnServiceIfAvailable();
    }
    if (!mPlatformService) {
      mPlatformService = mTestService;
    }
#else
    mPlatformService = mTestService;
#endif
  }

 private:
  ~WebAuthnService() = default;

  nsCOMPtr<nsIWebAuthnService> mPlatformService;
  nsCOMPtr<nsIWebAuthnService> mTestService;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_WebAuthnService_h_
