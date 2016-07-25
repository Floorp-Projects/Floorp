/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MMICall.h"

#include "mozilla/dom/MMICallBinding.h"
#include "nsIGlobalObject.h"

using namespace mozilla::dom;
using mozilla::ErrorResult;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MMICall, mWindow)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MMICall)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MMICall)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MMICall)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

MMICall::MMICall(nsPIDOMWindowInner* aWindow, const nsAString& aServiceCode)
  : mWindow(aWindow), mServiceCode(aServiceCode)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mWindow);
  if (!global) {
    return;
  }

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(global, rv);
  if (rv.Failed()) {
    rv.SuppressException();
    return;
  }

  mPromise = promise;
}

MMICall::~MMICall()
{
}

nsPIDOMWindowInner*
MMICall::GetParentObject() const
{
  return mWindow;
}

JSObject*
MMICall::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MMICallBinding::Wrap(aCx, this, aGivenProto);
}

void
MMICall::NotifyResult(JS::Handle<JS::Value> aResult)
{
  if (!mPromise) {
    return;
  }

  mPromise->MaybeResolve(aResult);
}

// WebIDL

already_AddRefed<Promise>
MMICall::GetResult(ErrorResult& aRv)
{
  if (!mPromise) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<Promise> promise = mPromise;
  return promise.forget();
}
