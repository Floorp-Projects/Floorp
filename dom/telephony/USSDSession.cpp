/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/USSDSession.h"

#include "mozilla/dom/USSDSessionBinding.h"
#include "mozilla/dom/telephony/TelephonyCallback.h"
#include "nsIGlobalObject.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla::dom;
using namespace mozilla::dom::telephony;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(USSDSession, mWindow)
NS_IMPL_CYCLE_COLLECTING_ADDREF(USSDSession)
NS_IMPL_CYCLE_COLLECTING_RELEASE(USSDSession)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(USSDSession)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

USSDSession::USSDSession(nsPIDOMWindow* aWindow, nsITelephonyService* aService,
                         uint32_t aServiceId)
  : mWindow(aWindow), mService(aService), mServiceId(aServiceId)
{
}

USSDSession::~USSDSession()
{
}

nsPIDOMWindow*
USSDSession::GetParentObject() const
{
  return mWindow;
}

JSObject*
USSDSession::WrapObject(JSContext* aCx)
{
  return USSDSessionBinding::Wrap(aCx, this);
}

// WebIDL

already_AddRefed<USSDSession>
USSDSession::Constructor(const GlobalObject& aGlobal, uint32_t aServiceId,
                         ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsITelephonyService> ril =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  if (!ril) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<USSDSession> session = new USSDSession(window, ril, aServiceId);
  return session.forget();
}

already_AddRefed<Promise>
USSDSession::Send(const nsAString& aUssd, ErrorResult& aRv)
{
  if (!mService) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mWindow);
  if (!global) {
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);

  nsresult rv = mService->SendUSSD(mServiceId, aUssd, callback);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
  }

  return promise.forget();
}
