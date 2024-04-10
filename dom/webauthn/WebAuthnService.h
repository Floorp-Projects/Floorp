/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnService_h_
#define mozilla_dom_WebAuthnService_h_

#include "nsIWebAuthnService.h"
#include "AuthrsBridge_ffi.h"
#include "mozilla/dom/WebAuthnPromiseHolder.h"

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

  WebAuthnService()
      : mTransactionState(Nothing(), "WebAuthnService::mTransactionState") {
    Unused << authrs_service_constructor(getter_AddRefs(mAuthrsService));
#if defined(XP_WIN)
    if (WinWebAuthnService::AreWebAuthNApisAvailable()) {
      mPlatformService = new WinWebAuthnService();
    } else {
      mPlatformService = mAuthrsService;
    }
#elif defined(MOZ_WIDGET_ANDROID)
    mPlatformService = new AndroidWebAuthnService();
#elif defined(XP_MACOSX)
    if (__builtin_available(macos 13.3, *)) {
      mPlatformService = NewMacOSWebAuthnServiceIfAvailable();
    }
    if (!mPlatformService) {
      mPlatformService = mAuthrsService;
    }
#else
    mPlatformService = mAuthrsService;
#endif
  }

 private:
  ~WebAuthnService() = default;

  struct TransactionState {
    nsCOMPtr<nsIWebAuthnService> service;
    uint64_t transactionId;
    Maybe<nsCOMPtr<nsIWebAuthnRegisterPromise>> parentRegisterPromise;
    Maybe<nsCOMPtr<nsIWebAuthnRegisterResult>> registerResult;
    MozPromiseRequestHolder<WebAuthnRegisterPromise> childRegisterRequest;
  };
  using TransactionStateMutex = DataMutex<Maybe<TransactionState>>;
  TransactionStateMutex mTransactionState;

  void ShowAttestationConsentPrompt(const nsString& aOrigin,
                                    uint64_t aTransactionId,
                                    uint64_t aBrowsingContextId);
  void ResetLocked(const TransactionStateMutex::AutoLock& aGuard);

  nsIWebAuthnService* DefaultService() {
    if (StaticPrefs::security_webauth_webauthn_enable_softtoken()) {
      return mAuthrsService;
    }
    return mPlatformService;
  }

  nsIWebAuthnService* AuthrsService() { return mAuthrsService; }

  nsIWebAuthnService* SelectedService() {
    auto guard = mTransactionState.Lock();
    if (guard->isSome()) {
      return guard->ref().service;
    }
    return DefaultService();
  }

  nsCOMPtr<nsIWebAuthnService> mAuthrsService;
  nsCOMPtr<nsIWebAuthnService> mPlatformService;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_WebAuthnService_h_
