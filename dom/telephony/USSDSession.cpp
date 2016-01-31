/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
using mozilla::ErrorResult;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(USSDSession, mWindow)
NS_IMPL_CYCLE_COLLECTING_ADDREF(USSDSession)
NS_IMPL_CYCLE_COLLECTING_RELEASE(USSDSession)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(USSDSession)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

USSDSession::USSDSession(nsPIDOMWindowInner* aWindow,
                         nsITelephonyService* aService,
                         uint32_t aServiceId)
  : mWindow(aWindow), mService(aService), mServiceId(aServiceId)
{
}

USSDSession::~USSDSession()
{
}

nsPIDOMWindowInner*
USSDSession::GetParentObject() const
{
  return mWindow;
}

JSObject*
USSDSession::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return USSDSessionBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise>
USSDSession::CreatePromise(ErrorResult& aRv)
{
  if (!mService) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mWindow);
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  return promise.forget();
}

// WebIDL

already_AddRefed<USSDSession>
USSDSession::Constructor(const GlobalObject& aGlobal, uint32_t aServiceId,
                         ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal.GetAsSupports());
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

  RefPtr<USSDSession> session = new USSDSession(window, ril, aServiceId);
  return session.forget();
}

already_AddRefed<Promise>
USSDSession::Send(const nsAString& aUssd, ErrorResult& aRv)
{
  RefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);

  nsresult rv = mService->SendUSSD(mServiceId, aUssd, callback);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
  }

  return promise.forget();
}

already_AddRefed<Promise>
USSDSession::Cancel(ErrorResult& aRv)
{
  RefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);

  nsresult rv = mService->CancelUSSD(mServiceId, callback);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
  }

  return promise.forget();
}
