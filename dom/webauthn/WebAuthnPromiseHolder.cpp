/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AppShutdown.h"
#include "WebAuthnPromiseHolder.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS(WebAuthnRegisterPromiseHolder, nsIWebAuthnRegisterPromise);

already_AddRefed<WebAuthnRegisterPromise>
WebAuthnRegisterPromiseHolder::Ensure() {
  return mRegisterPromise.Ensure(__func__);
}

NS_IMETHODIMP
WebAuthnRegisterPromiseHolder::Resolve(nsIWebAuthnRegisterResult* aResult) {
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdownThreads)) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

  // Resolve the promise on its owning thread if Disconnect() has not been
  // called.
  RefPtr<nsIWebAuthnRegisterResult> result(aResult);
  mEventTarget->Dispatch(NS_NewRunnableFunction(
      "WebAuthnRegisterPromiseHolder::Resolve",
      [self = RefPtr{this}, result]() {
        self->mRegisterPromise.ResolveIfExists(result, __func__);
      }));
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterPromiseHolder::Reject(nsresult aResult) {
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdownThreads)) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

  // Reject the promise on its owning thread if Disconnect() has not been
  // called.
  mEventTarget->Dispatch(NS_NewRunnableFunction(
      "WebAuthnRegisterPromiseHolder::Reject",
      [self = RefPtr{this}, aResult]() {
        self->mRegisterPromise.RejectIfExists(aResult, __func__);
      }));
  return NS_OK;
}

NS_IMPL_ISUPPORTS(WebAuthnSignPromiseHolder, nsIWebAuthnSignPromise);

already_AddRefed<WebAuthnSignPromise> WebAuthnSignPromiseHolder::Ensure() {
  return mSignPromise.Ensure(__func__);
}

NS_IMETHODIMP
WebAuthnSignPromiseHolder::Resolve(nsIWebAuthnSignResult* aResult) {
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdownThreads)) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

  // Resolve the promise on its owning thread if Disconnect() has not been
  // called.
  RefPtr<nsIWebAuthnSignResult> result(aResult);
  mEventTarget->Dispatch(NS_NewRunnableFunction(
      "WebAuthnSignPromiseHolder::Resolve", [self = RefPtr{this}, result]() {
        self->mSignPromise.ResolveIfExists(result, __func__);
      }));
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnSignPromiseHolder::Reject(nsresult aResult) {
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdownThreads)) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

  // Reject the promise on its owning thread if Disconnect() has not been
  // called.
  mEventTarget->Dispatch(NS_NewRunnableFunction(
      "WebAuthnSignPromiseHolder::Reject", [self = RefPtr{this}, aResult]() {
        self->mSignPromise.RejectIfExists(aResult, __func__);
      }));

  return NS_OK;
}

}  // namespace mozilla::dom
