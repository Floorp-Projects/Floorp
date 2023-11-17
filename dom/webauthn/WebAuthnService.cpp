/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StaticPrefs_security.h"
#include "WebAuthnService.h"

namespace mozilla::dom {

already_AddRefed<nsIWebAuthnService> NewWebAuthnService() {
  nsCOMPtr<nsIWebAuthnService> webauthnService(new WebAuthnService());
  return webauthnService.forget();
}

NS_IMPL_ISUPPORTS(WebAuthnService, nsIWebAuthnService)

NS_IMETHODIMP
WebAuthnService::MakeCredential(uint64_t aTransactionId,
                                uint64_t browsingContextId,
                                nsIWebAuthnRegisterArgs* aArgs,
                                nsIWebAuthnRegisterPromise* aPromise) {
  if (StaticPrefs::security_webauth_webauthn_enable_softtoken()) {
    return mTestService->MakeCredential(aTransactionId, browsingContextId,
                                        aArgs, aPromise);
  }
  return mPlatformService->MakeCredential(aTransactionId, browsingContextId,
                                          aArgs, aPromise);
}

NS_IMETHODIMP
WebAuthnService::GetAssertion(uint64_t aTransactionId,
                              uint64_t browsingContextId,
                              nsIWebAuthnSignArgs* aArgs,
                              nsIWebAuthnSignPromise* aPromise) {
  if (StaticPrefs::security_webauth_webauthn_enable_softtoken()) {
    return mTestService->GetAssertion(aTransactionId, browsingContextId, aArgs,
                                      aPromise);
  }
  return mPlatformService->GetAssertion(aTransactionId, browsingContextId,
                                        aArgs, aPromise);
}

NS_IMETHODIMP
WebAuthnService::GetIsUVPAA(bool* aAvailable) {
  if (StaticPrefs::security_webauth_webauthn_enable_softtoken()) {
    return mTestService->GetIsUVPAA(aAvailable);
  }
  return mPlatformService->GetIsUVPAA(aAvailable);
}

NS_IMETHODIMP
WebAuthnService::Reset() {
  if (StaticPrefs::security_webauth_webauthn_enable_softtoken()) {
    return mTestService->Reset();
  }
  return mPlatformService->Reset();
}

NS_IMETHODIMP
WebAuthnService::Cancel(uint64_t aTransactionId) {
  if (StaticPrefs::security_webauth_webauthn_enable_softtoken()) {
    return mTestService->Cancel(aTransactionId);
  }
  return mPlatformService->Cancel(aTransactionId);
}

NS_IMETHODIMP
WebAuthnService::PinCallback(uint64_t aTransactionId, const nsACString& aPin) {
  if (StaticPrefs::security_webauth_webauthn_enable_softtoken()) {
    return mTestService->PinCallback(aTransactionId, aPin);
  }
  return mPlatformService->PinCallback(aTransactionId, aPin);
}

NS_IMETHODIMP
WebAuthnService::ResumeMakeCredential(uint64_t aTransactionId,
                                      bool aForceNoneAttestation) {
  if (StaticPrefs::security_webauth_webauthn_enable_softtoken()) {
    return mTestService->ResumeMakeCredential(aTransactionId,
                                              aForceNoneAttestation);
  }
  return mPlatformService->ResumeMakeCredential(aTransactionId,
                                                aForceNoneAttestation);
}

NS_IMETHODIMP
WebAuthnService::SelectionCallback(uint64_t aTransactionId, uint64_t aIndex) {
  if (StaticPrefs::security_webauth_webauthn_enable_softtoken()) {
    return mTestService->SelectionCallback(aTransactionId, aIndex);
  }
  return mPlatformService->SelectionCallback(aTransactionId, aIndex);
}

NS_IMETHODIMP
WebAuthnService::AddVirtualAuthenticator(
    const nsACString& protocol, const nsACString& transport,
    bool hasResidentKey, bool hasUserVerification, bool isUserConsenting,
    bool isUserVerified, uint64_t* retval) {
  if (StaticPrefs::security_webauth_webauthn_enable_softtoken()) {
    return mTestService->AddVirtualAuthenticator(
        protocol, transport, hasResidentKey, hasUserVerification,
        isUserConsenting, isUserVerified, retval);
  }
  return mPlatformService->AddVirtualAuthenticator(
      protocol, transport, hasResidentKey, hasUserVerification,
      isUserConsenting, isUserVerified, retval);
}

NS_IMETHODIMP
WebAuthnService::RemoveVirtualAuthenticator(uint64_t authenticatorId) {
  if (StaticPrefs::security_webauth_webauthn_enable_softtoken()) {
    return mTestService->RemoveVirtualAuthenticator(authenticatorId);
  }
  return mPlatformService->RemoveVirtualAuthenticator(authenticatorId);
}

NS_IMETHODIMP
WebAuthnService::AddCredential(uint64_t authenticatorId,
                               const nsACString& credentialId,
                               bool isResidentCredential,
                               const nsACString& rpId,
                               const nsACString& privateKey,
                               const nsACString& userHandle,
                               uint32_t signCount) {
  if (StaticPrefs::security_webauth_webauthn_enable_softtoken()) {
    return mTestService->AddCredential(authenticatorId, credentialId,
                                       isResidentCredential, rpId, privateKey,
                                       userHandle, signCount);
  }
  return mPlatformService->AddCredential(authenticatorId, credentialId,
                                         isResidentCredential, rpId, privateKey,
                                         userHandle, signCount);
}

NS_IMETHODIMP
WebAuthnService::GetCredentials(
    uint64_t authenticatorId,
    nsTArray<RefPtr<nsICredentialParameters>>& retval) {
  if (StaticPrefs::security_webauth_webauthn_enable_softtoken()) {
    return mTestService->GetCredentials(authenticatorId, retval);
  }
  return mPlatformService->GetCredentials(authenticatorId, retval);
}

NS_IMETHODIMP
WebAuthnService::RemoveCredential(uint64_t authenticatorId,
                                  const nsACString& credentialId) {
  if (StaticPrefs::security_webauth_webauthn_enable_softtoken()) {
    return mTestService->RemoveCredential(authenticatorId, credentialId);
  }
  return mPlatformService->RemoveCredential(authenticatorId, credentialId);
}

NS_IMETHODIMP
WebAuthnService::RemoveAllCredentials(uint64_t authenticatorId) {
  if (StaticPrefs::security_webauth_webauthn_enable_softtoken()) {
    return mTestService->RemoveAllCredentials(authenticatorId);
  }
  return mPlatformService->RemoveAllCredentials(authenticatorId);
}

NS_IMETHODIMP
WebAuthnService::SetUserVerified(uint64_t authenticatorId,
                                 bool isUserVerified) {
  if (StaticPrefs::security_webauth_webauthn_enable_softtoken()) {
    return mTestService->SetUserVerified(authenticatorId, isUserVerified);
  }
  return mPlatformService->SetUserVerified(authenticatorId, isUserVerified);
}

NS_IMETHODIMP
WebAuthnService::Listen() {
  if (StaticPrefs::security_webauth_webauthn_enable_softtoken()) {
    return mTestService->Listen();
  }
  return mPlatformService->Listen();
}

NS_IMETHODIMP
WebAuthnService::RunCommand(const nsACString& cmd) {
  if (StaticPrefs::security_webauth_webauthn_enable_softtoken()) {
    return mTestService->RunCommand(cmd);
  }
  return mPlatformService->RunCommand(cmd);
}

}  // namespace mozilla::dom
