/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthnTransactionParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/StaticPrefs_security.h"

#include "nsIWebAuthnService.h"
#include "nsThreadUtils.h"
#include "WebAuthnArgs.h"

#ifdef XP_WIN
#  include "WinWebAuthnManager.h"
#endif

namespace mozilla::dom {

mozilla::ipc::IPCResult WebAuthnTransactionParent::RecvRequestRegister(
    const uint64_t& aTransactionId,
    const WebAuthnMakeCredentialInfo& aTransactionInfo) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

#ifdef XP_WIN
  bool usingTestToken =
      StaticPrefs::security_webauth_webauthn_enable_softtoken();
  if (!usingTestToken && WinWebAuthnManager::AreWebAuthNApisAvailable()) {
    WinWebAuthnManager* mgr = WinWebAuthnManager::Get();
    if (mgr) {
      mgr->Register(this, aTransactionId, aTransactionInfo);
    }
    return IPC_OK();
  }
#endif

  // If there's an ongoing transaction, abort it.
  if (mTransactionId.isSome()) {
    mRegisterPromiseRequest.DisconnectIfExists();
    mSignPromiseRequest.DisconnectIfExists();
    Unused << SendAbort(mTransactionId.ref(), NS_ERROR_DOM_ABORT_ERR);
  }
  mTransactionId = Some(aTransactionId);

  RefPtr<WebAuthnRegisterPromiseHolder> promiseHolder =
      new WebAuthnRegisterPromiseHolder(GetCurrentSerialEventTarget());

  PWebAuthnTransactionParent* parent = this;
  RefPtr<WebAuthnRegisterPromise> promise = promiseHolder->Ensure();
  promise
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [this, parent, aTransactionId,
           inputClientData = aTransactionInfo.ClientDataJSON()](
              const WebAuthnRegisterPromise::ResolveValueType& aValue) {
            mTransactionId.reset();
            mRegisterPromiseRequest.Complete();
            nsCString clientData;
            nsresult rv = aValue->GetClientDataJSON(clientData);
            if (rv == NS_ERROR_NOT_AVAILABLE) {
              clientData = inputClientData;
            } else if (NS_FAILED(rv)) {
              Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                   u"CTAPRegisterAbort"_ns, 1);
              Unused << parent->SendAbort(aTransactionId,
                                          NS_ERROR_DOM_NOT_ALLOWED_ERR);
              return;
            }

            nsTArray<uint8_t> attObj;
            rv = aValue->GetAttestationObject(attObj);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                   u"CTAPRegisterAbort"_ns, 1);
              Unused << parent->SendAbort(aTransactionId,
                                          NS_ERROR_DOM_NOT_ALLOWED_ERR);
              return;
            }

            nsTArray<uint8_t> credentialId;
            rv = aValue->GetCredentialId(credentialId);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                   u"CTAPRegisterAbort"_ns, 1);
              Unused << parent->SendAbort(aTransactionId,
                                          NS_ERROR_DOM_NOT_ALLOWED_ERR);
              return;
            }

            nsTArray<nsString> transports;
            rv = aValue->GetTransports(transports);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                   u"CTAPRegisterAbort"_ns, 1);
              Unused << parent->SendAbort(aTransactionId,
                                          NS_ERROR_DOM_NOT_ALLOWED_ERR);
              return;
            }

            Maybe<nsString> authenticatorAttachment;
            nsString maybeAuthenticatorAttachment;
            rv = aValue->GetAuthenticatorAttachment(
                maybeAuthenticatorAttachment);
            if (rv != NS_ERROR_NOT_AVAILABLE) {
              if (NS_WARN_IF(NS_FAILED(rv))) {
                Telemetry::ScalarAdd(
                    Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                    u"CTAPRegisterAbort"_ns, 1);
                Unused << parent->SendAbort(aTransactionId,
                                            NS_ERROR_DOM_NOT_ALLOWED_ERR);
                return;
              }
              authenticatorAttachment = Some(maybeAuthenticatorAttachment);
            }

            nsTArray<WebAuthnExtensionResult> extensions;
            bool credPropsRk;
            rv = aValue->GetCredPropsRk(&credPropsRk);
            if (rv != NS_ERROR_NOT_AVAILABLE) {
              if (NS_WARN_IF(NS_FAILED(rv))) {
                Telemetry::ScalarAdd(
                    Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                    u"CTAPRegisterAbort"_ns, 1);
                Unused << parent->SendAbort(aTransactionId,
                                            NS_ERROR_DOM_NOT_ALLOWED_ERR);
                return;
              }
              extensions.AppendElement(
                  WebAuthnExtensionResultCredProps(credPropsRk));
            }

            WebAuthnMakeCredentialResult result(
                clientData, attObj, credentialId, transports, extensions,
                authenticatorAttachment);

            Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                 u"CTAPRegisterFinish"_ns, 1);
            Unused << parent->SendConfirmRegister(aTransactionId, result);
          },
          [this, parent, aTransactionId](
              const WebAuthnRegisterPromise::RejectValueType aValue) {
            mTransactionId.reset();
            mRegisterPromiseRequest.Complete();
            Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                 u"CTAPRegisterAbort"_ns, 1);
            Unused << parent->SendAbort(aTransactionId, aValue);
          })
      ->Track(mRegisterPromiseRequest);

  nsCOMPtr<nsIWebAuthnService> webauthnService(
      do_GetService("@mozilla.org/webauthn/service;1"));

  uint64_t browsingContextId = aTransactionInfo.BrowsingContextId();
  RefPtr<WebAuthnRegisterArgs> args(new WebAuthnRegisterArgs(aTransactionInfo));

  nsresult rv = webauthnService->MakeCredential(
      aTransactionId, browsingContextId, args, promiseHolder);
  if (NS_FAILED(rv)) {
    promiseHolder->Reject(NS_ERROR_DOM_NOT_ALLOWED_ERR);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult WebAuthnTransactionParent::RecvRequestSign(
    const uint64_t& aTransactionId,
    const WebAuthnGetAssertionInfo& aTransactionInfo) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

#ifdef XP_WIN
  bool usingTestToken =
      StaticPrefs::security_webauth_webauthn_enable_softtoken();
  if (!usingTestToken && WinWebAuthnManager::AreWebAuthNApisAvailable()) {
    WinWebAuthnManager* mgr = WinWebAuthnManager::Get();
    if (mgr) {
      mgr->Sign(this, aTransactionId, aTransactionInfo);
    }
    return IPC_OK();
  }
#endif

  if (mTransactionId.isSome()) {
    mRegisterPromiseRequest.DisconnectIfExists();
    mSignPromiseRequest.DisconnectIfExists();
    Unused << SendAbort(mTransactionId.ref(), NS_ERROR_DOM_ABORT_ERR);
  }
  mTransactionId = Some(aTransactionId);

  RefPtr<WebAuthnSignPromiseHolder> promiseHolder =
      new WebAuthnSignPromiseHolder(GetCurrentSerialEventTarget());

  PWebAuthnTransactionParent* parent = this;
  RefPtr<WebAuthnSignPromise> promise = promiseHolder->Ensure();
  promise
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [this, parent, aTransactionId,
           inputClientData = aTransactionInfo.ClientDataJSON()](
              const WebAuthnSignPromise::ResolveValueType& aValue) {
            mTransactionId.reset();
            mSignPromiseRequest.Complete();

            nsCString clientData;
            nsresult rv = aValue->GetClientDataJSON(clientData);
            if (rv == NS_ERROR_NOT_AVAILABLE) {
              clientData = inputClientData;
            } else if (NS_FAILED(rv)) {
              Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                   u"CTAPRegisterAbort"_ns, 1);
              Unused << parent->SendAbort(aTransactionId,
                                          NS_ERROR_DOM_NOT_ALLOWED_ERR);
              return;
            }

            nsTArray<uint8_t> credentialId;
            rv = aValue->GetCredentialId(credentialId);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                   u"CTAPSignAbort"_ns, 1);
              Unused << parent->SendAbort(aTransactionId,
                                          NS_ERROR_DOM_NOT_ALLOWED_ERR);
              return;
            }

            nsTArray<uint8_t> signature;
            rv = aValue->GetSignature(signature);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                   u"CTAPSignAbort"_ns, 1);
              Unused << parent->SendAbort(aTransactionId,
                                          NS_ERROR_DOM_NOT_ALLOWED_ERR);
              return;
            }

            nsTArray<uint8_t> authenticatorData;
            rv = aValue->GetAuthenticatorData(authenticatorData);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                   u"CTAPSignAbort"_ns, 1);
              Unused << parent->SendAbort(aTransactionId,
                                          NS_ERROR_DOM_NOT_ALLOWED_ERR);
              return;
            }

            nsTArray<uint8_t> userHandle;
            Unused << aValue->GetUserHandle(userHandle);  // optional

            Maybe<nsString> authenticatorAttachment;
            nsString maybeAuthenticatorAttachment;
            rv = aValue->GetAuthenticatorAttachment(
                maybeAuthenticatorAttachment);
            if (rv != NS_ERROR_NOT_AVAILABLE) {
              if (NS_WARN_IF(NS_FAILED(rv))) {
                Telemetry::ScalarAdd(
                    Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                    u"CTAPSignAbort"_ns, 1);
                Unused << parent->SendAbort(aTransactionId,
                                            NS_ERROR_DOM_NOT_ALLOWED_ERR);
                return;
              }
              authenticatorAttachment = Some(maybeAuthenticatorAttachment);
            }

            nsTArray<WebAuthnExtensionResult> extensions;
            bool usedAppId;
            rv = aValue->GetUsedAppId(&usedAppId);
            if (rv != NS_ERROR_NOT_AVAILABLE) {
              if (NS_FAILED(rv)) {
                Telemetry::ScalarAdd(
                    Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                    u"CTAPSignAbort"_ns, 1);
                Unused << parent->SendAbort(aTransactionId,
                                            NS_ERROR_DOM_NOT_ALLOWED_ERR);
                return;
              }
              extensions.AppendElement(WebAuthnExtensionResultAppId(usedAppId));
            }

            WebAuthnGetAssertionResult result(
                clientData, credentialId, signature, authenticatorData,
                extensions, userHandle, authenticatorAttachment);

            Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                 u"CTAPSignFinish"_ns, 1);
            Unused << parent->SendConfirmSign(aTransactionId, result);
          },
          [this, parent,
           aTransactionId](const WebAuthnSignPromise::RejectValueType aValue) {
            mTransactionId.reset();
            mSignPromiseRequest.Complete();
            Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                 u"CTAPSignAbort"_ns, 1);
            Unused << parent->SendAbort(aTransactionId, aValue);
          })
      ->Track(mSignPromiseRequest);

  RefPtr<WebAuthnSignArgs> args(new WebAuthnSignArgs(aTransactionInfo));

  nsCOMPtr<nsIWebAuthnService> webauthnService(
      do_GetService("@mozilla.org/webauthn/service;1"));
  nsresult rv = webauthnService->GetAssertion(
      aTransactionId, aTransactionInfo.BrowsingContextId(), args,
      promiseHolder);
  if (NS_FAILED(rv)) {
    promiseHolder->Reject(NS_ERROR_DOM_NOT_ALLOWED_ERR);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult WebAuthnTransactionParent::RecvRequestCancel(
    const Tainted<uint64_t>& aTransactionId) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

#ifdef XP_WIN
  if (WinWebAuthnManager::AreWebAuthNApisAvailable()) {
    WinWebAuthnManager* mgr = WinWebAuthnManager::Get();
    if (mgr) {
      mgr->Cancel(this, aTransactionId);
    }
  }
  // fall through in case the virtual token was used.
#endif

  if (mTransactionId.isNothing() ||
      !MOZ_IS_VALID(aTransactionId, mTransactionId.ref() == aTransactionId)) {
    return IPC_OK();
  }

  mRegisterPromiseRequest.DisconnectIfExists();
  mSignPromiseRequest.DisconnectIfExists();
  mTransactionId.reset();

  nsCOMPtr<nsIWebAuthnService> webauthnService(
      do_GetService("@mozilla.org/webauthn/service;1"));
  if (webauthnService) {
    webauthnService->Reset();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult WebAuthnTransactionParent::RecvDestroyMe() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  // The child was disconnected from the WebAuthnManager instance and will send
  // no further messages. It is kept alive until we delete it explicitly.

  // The child should have cancelled any active transaction. This means
  // we expect no more messages to the child. We'll crash otherwise.

  // The IPC roundtrip is complete. No more messages, hopefully.
  IProtocol* mgr = Manager();
  if (!Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }

  return IPC_OK();
}

void WebAuthnTransactionParent::ActorDestroy(ActorDestroyReason aWhy) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  // Called either by Send__delete__() in RecvDestroyMe() above, or when
  // the channel disconnects. Ensure the token manager forgets about us.

#ifdef XP_WIN
  if (WinWebAuthnManager::AreWebAuthNApisAvailable()) {
    WinWebAuthnManager* mgr = WinWebAuthnManager::Get();
    if (mgr) {
      mgr->MaybeClearTransaction(this);
    }
  }
  // fall through in case the virtual token was used.
#endif

  if (mTransactionId.isSome()) {
    mRegisterPromiseRequest.DisconnectIfExists();
    mSignPromiseRequest.DisconnectIfExists();
    mTransactionId.reset();

    nsCOMPtr<nsIWebAuthnService> webauthnService(
        do_GetService("@mozilla.org/webauthn/service;1"));
    if (webauthnService) {
      webauthnService->Reset();
    }
  }
}

}  // namespace mozilla::dom
