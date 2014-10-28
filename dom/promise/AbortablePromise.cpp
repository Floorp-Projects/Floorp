/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AbortablePromise.h"

#include "mozilla/dom/AbortablePromiseBinding.h"
#include "mozilla/dom/PromiseNativeAbortCallback.h"
#include "mozilla/ErrorResult.h"
#include "nsThreadUtils.h"
#include "PromiseCallback.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(PromiseNativeAbortCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PromiseNativeAbortCallback)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PromiseNativeAbortCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTION_0(PromiseNativeAbortCallback)

NS_IMPL_ADDREF_INHERITED(AbortablePromise, Promise)
NS_IMPL_RELEASE_INHERITED(AbortablePromise, Promise)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(AbortablePromise)
NS_INTERFACE_MAP_END_INHERITING(Promise)
NS_IMPL_CYCLE_COLLECTION_INHERITED(AbortablePromise, Promise, mAbortCallback)

AbortablePromise::AbortablePromise(nsIGlobalObject* aGlobal,
                                   PromiseNativeAbortCallback& aAbortCallback)
  : Promise(aGlobal)
  , mAbortCallback(&aAbortCallback)
{
}

AbortablePromise::AbortablePromise(nsIGlobalObject* aGlobal)
  : Promise(aGlobal)
{
}

AbortablePromise::~AbortablePromise()
{
}

/* static */ already_AddRefed<AbortablePromise>
AbortablePromise::Create(nsIGlobalObject* aGlobal,
                         PromiseNativeAbortCallback& aAbortCallback,
                         ErrorResult& aRv)
{
  nsRefPtr<AbortablePromise> p = new AbortablePromise(aGlobal, aAbortCallback);
  p->CreateWrapper(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  return p.forget();
}

JSObject*
AbortablePromise::WrapObject(JSContext* aCx)
{
  return MozAbortablePromiseBinding::Wrap(aCx, this);
}

/* static */ already_AddRefed<AbortablePromise>
AbortablePromise::Constructor(const GlobalObject& aGlobal, PromiseInit& aInit,
                              AbortCallback& aAbortCallback, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global;
  global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<AbortablePromise> promise = new AbortablePromise(global);
  promise->CreateWrapper(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  promise->CallInitFunction(aGlobal, aInit, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  promise->mAbortCallback = &aAbortCallback;

  return promise.forget();
}

void
AbortablePromise::Abort()
{
  if (IsPending()) {
    return;
  }
  MaybeReject(NS_ERROR_ABORT);

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &AbortablePromise::DoAbort);
  Promise::DispatchToMicroTask(runnable);
}

void
AbortablePromise::DoAbort()
{
  if (mAbortCallback.HasWebIDLCallback()) {
    ErrorResult rv;
    mAbortCallback.GetWebIDLCallback()->Call(rv);
    return;
  }
  mAbortCallback.GetXPCOMCallback()->Call();
}

} // namespace dom
} // namespace mozilla
