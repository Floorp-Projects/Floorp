/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthnTransactionParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/StaticPrefs_security.h"

#include "nsThreadUtils.h"
#include "WebAuthnArgs.h"

namespace mozilla::dom {

void WebAuthnTransactionParent::CompleteTransaction() {
  if (mTransactionId.isSome()) {
    if (mRegisterPromiseRequest.Exists()) {
      mRegisterPromiseRequest.Complete();
    }
    if (mSignPromiseRequest.Exists()) {
      mSignPromiseRequest.Complete();
    }
    if (mWebAuthnService) {
      // We have to do this to work around Bug 1864526.
      mWebAuthnService->Cancel(mTransactionId.ref());
    }
    mTransactionId.reset();
  }
}

void WebAuthnTransactionParent::DisconnectTransaction() {
  mTransactionId.reset();
  mRegisterPromiseRequest.DisconnectIfExists();
  mSignPromiseRequest.DisconnectIfExists();
  if (mWebAuthnService) {
    mWebAuthnService->Reset();
  }
}

mozilla::ipc::IPCResult WebAuthnTransactionParent::RecvRequestRegister(
    const uint64_t& aTransactionId,
    const WebAuthnMakeCredentialInfo& aTransactionInfo) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  if (!mWebAuthnService) {
    mWebAuthnService = do_GetService("@mozilla.org/webauthn/service;1");
    if (!mWebAuthnService) {
      return IPC_FAIL_NO_REASON(this);
    }
  }

  // If there's an ongoing transaction, abort it.
  if (mTransactionId.isSome()) {
    DisconnectTransaction();
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
            CompleteTransaction();

            nsCString clientData;
            nsresult rv = aValue->GetClientDataJSON(clientData);
            if (rv == NS_ERROR_NOT_AVAILABLE) {
              clientData = inputClientData;
            } else if (NS_FAILED(rv)) {
              Unused << parent->SendAbort(aTransactionId,
                                          NS_ERROR_DOM_NOT_ALLOWED_ERR);
              return;
            }

            nsTArray<uint8_t> attObj;
            rv = aValue->GetAttestationObject(attObj);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              Unused << parent->SendAbort(aTransactionId,
                                          NS_ERROR_DOM_NOT_ALLOWED_ERR);
              return;
            }

            nsTArray<uint8_t> credentialId;
            rv = aValue->GetCredentialId(credentialId);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              Unused << parent->SendAbort(aTransactionId,
                                          NS_ERROR_DOM_NOT_ALLOWED_ERR);
              return;
            }

            nsTArray<nsString> transports;
            rv = aValue->GetTransports(transports);
            if (NS_WARN_IF(NS_FAILED(rv))) {
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
                Unused << parent->SendAbort(aTransactionId,
                                            NS_ERROR_DOM_NOT_ALLOWED_ERR);
                return;
              }
              extensions.AppendElement(
                  WebAuthnExtensionResultCredProps(credPropsRk));
            }

            bool hmacCreateSecret;
            rv = aValue->GetHmacCreateSecret(&hmacCreateSecret);
            if (rv != NS_ERROR_NOT_AVAILABLE) {
              if (NS_WARN_IF(NS_FAILED(rv))) {
                Unused << parent->SendAbort(aTransactionId,
                                            NS_ERROR_DOM_NOT_ALLOWED_ERR);
                return;
              }
              extensions.AppendElement(
                  WebAuthnExtensionResultHmacSecret(hmacCreateSecret));
            }

            WebAuthnMakeCredentialResult result(
                clientData, attObj, credentialId, transports, extensions,
                authenticatorAttachment);

            Unused << parent->SendConfirmRegister(aTransactionId, result);
          },
          [this, parent, aTransactionId](
              const WebAuthnRegisterPromise::RejectValueType aValue) {
            CompleteTransaction();
            Unused << parent->SendAbort(aTransactionId, aValue);
          })
      ->Track(mRegisterPromiseRequest);

  uint64_t browsingContextId = aTransactionInfo.BrowsingContextId();
  RefPtr<WebAuthnRegisterArgs> args(new WebAuthnRegisterArgs(aTransactionInfo));

  nsresult rv = mWebAuthnService->MakeCredential(
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

  if (!mWebAuthnService) {
    mWebAuthnService = do_GetService("@mozilla.org/webauthn/service;1");
    if (!mWebAuthnService) {
      return IPC_FAIL_NO_REASON(this);
    }
  }

  if (mTransactionId.isSome()) {
    DisconnectTransaction();
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
            CompleteTransaction();

            nsCString clientData;
            nsresult rv = aValue->GetClientDataJSON(clientData);
            if (rv == NS_ERROR_NOT_AVAILABLE) {
              clientData = inputClientData;
            } else if (NS_FAILED(rv)) {
              Unused << parent->SendAbort(aTransactionId,
                                          NS_ERROR_DOM_NOT_ALLOWED_ERR);
              return;
            }

            nsTArray<uint8_t> credentialId;
            rv = aValue->GetCredentialId(credentialId);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              Unused << parent->SendAbort(aTransactionId,
                                          NS_ERROR_DOM_NOT_ALLOWED_ERR);
              return;
            }

            nsTArray<uint8_t> signature;
            rv = aValue->GetSignature(signature);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              Unused << parent->SendAbort(aTransactionId,
                                          NS_ERROR_DOM_NOT_ALLOWED_ERR);
              return;
            }

            nsTArray<uint8_t> authenticatorData;
            rv = aValue->GetAuthenticatorData(authenticatorData);
            if (NS_WARN_IF(NS_FAILED(rv))) {
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
                Unused << parent->SendAbort(aTransactionId,
                                            NS_ERROR_DOM_NOT_ALLOWED_ERR);
                return;
              }
              extensions.AppendElement(WebAuthnExtensionResultAppId(usedAppId));
            }

            WebAuthnGetAssertionResult result(
                clientData, credentialId, signature, authenticatorData,
                extensions, userHandle, authenticatorAttachment);

            Unused << parent->SendConfirmSign(aTransactionId, result);
          },
          [this, parent,
           aTransactionId](const WebAuthnSignPromise::RejectValueType aValue) {
            CompleteTransaction();
            Unused << parent->SendAbort(aTransactionId, aValue);
          })
      ->Track(mSignPromiseRequest);

  RefPtr<WebAuthnSignArgs> args(new WebAuthnSignArgs(aTransactionInfo));

  nsresult rv = mWebAuthnService->GetAssertion(
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

  if (mTransactionId.isNothing() ||
      !MOZ_IS_VALID(aTransactionId, mTransactionId.ref() == aTransactionId)) {
    return IPC_OK();
  }

  DisconnectTransaction();
  return IPC_OK();
}

mozilla::ipc::IPCResult WebAuthnTransactionParent::RecvRequestIsUVPAA(
    RequestIsUVPAAResolver&& aResolver) {
#ifdef MOZ_WIDGET_ANDROID
  // Try the nsIWebAuthnService. If we're configured for tests we
  // will get a result. Otherwise we expect NS_ERROR_NOT_IMPLEMENTED.
  nsCOMPtr<nsIWebAuthnService> service(
      do_GetService("@mozilla.org/webauthn/service;1"));
  bool available;
  nsresult rv = service->GetIsUVPAA(&available);
  if (NS_SUCCEEDED(rv)) {
    aResolver(available);
    return IPC_OK();
  }

  // Don't consult the platform API if resident key support is disabled.
  if (!StaticPrefs::
          security_webauthn_webauthn_enable_android_fido2_residentkey()) {
    aResolver(false);
    return IPC_OK();
  }

  // The GeckoView implementation of
  // isUserVerifiyingPlatformAuthenticatorAvailable does not block, but we must
  // call it on the main thread. It returns a MozPromise which we can ->Then to
  // call aResolver on the IPDL background thread.
  //
  // Bug 1550788: there is an unnecessary layer of dispatching here: ipdl ->
  // main -> a background thread. Other platforms just do ipdl -> a background
  // thread.
  nsCOMPtr<nsISerialEventTarget> target = GetCurrentSerialEventTarget();
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      __func__, [target, resolver = std::move(aResolver)]() {
        auto result = java::WebAuthnTokenManager::
            WebAuthnIsUserVerifyingPlatformAuthenticatorAvailable();
        auto geckoResult = java::GeckoResult::LocalRef(std::move(result));
        MozPromise<bool, bool, false>::FromGeckoResult(geckoResult)
            ->Then(
                target, __func__,
                [resolver](
                    const MozPromise<bool, bool, false>::ResolveOrRejectValue&
                        aValue) {
                  if (aValue.IsResolve()) {
                    resolver(aValue.ResolveValue());
                  } else {
                    resolver(false);
                  }
                });
      }));
  NS_DispatchToMainThread(runnable.forget());
  return IPC_OK();

#else

  nsCOMPtr<nsISerialEventTarget> target = GetCurrentSerialEventTarget();
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      __func__, [target, resolver = std::move(aResolver)]() {
        bool available;
        nsCOMPtr<nsIWebAuthnService> service(
            do_GetService("@mozilla.org/webauthn/service;1"));
        nsresult rv = service->GetIsUVPAA(&available);
        if (NS_FAILED(rv)) {
          available = false;
        }
        BoolPromise::CreateAndResolve(available, __func__)
            ->Then(target, __func__,
                   [resolver](const BoolPromise::ResolveOrRejectValue& value) {
                     if (value.IsResolve()) {
                       resolver(value.ResolveValue());
                     } else {
                       resolver(false);
                     }
                   });
      }));
  NS_DispatchBackgroundTask(runnable.forget(), NS_DISPATCH_EVENT_MAY_BLOCK);
  return IPC_OK();
#endif
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

  if (mTransactionId.isSome()) {
    DisconnectTransaction();
  }
}

}  // namespace mozilla::dom
