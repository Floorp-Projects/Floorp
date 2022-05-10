/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerUnregisterCallback.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS(UnregisterCallback, nsIServiceWorkerUnregisterCallback)

UnregisterCallback::UnregisterCallback()
    : mPromise(new GenericPromise::Private(__func__)) {}

UnregisterCallback::UnregisterCallback(GenericPromise::Private* aPromise)
    : mPromise(aPromise) {
  MOZ_DIAGNOSTIC_ASSERT(mPromise);
}

NS_IMETHODIMP
UnregisterCallback::UnregisterSucceeded(bool aState) {
  mPromise->Resolve(aState, __func__);
  return NS_OK;
}

NS_IMETHODIMP
UnregisterCallback::UnregisterFailed() {
  mPromise->Reject(NS_ERROR_DOM_SECURITY_ERR, __func__);
  return NS_OK;
}

RefPtr<GenericPromise> UnregisterCallback::Promise() const { return mPromise; }

}  // namespace mozilla::dom
