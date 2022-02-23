/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Lock.h"
#include "mozilla/dom/LockBinding.h"
#include "mozilla/dom/LockManager.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/locks/LockRequestChild.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Lock, mOwner, mWaitingPromise,
                                      mReleasedPromise)
NS_IMPL_CYCLE_COLLECTING_ADDREF(Lock)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Lock)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Lock)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Lock::Lock(nsIGlobalObject* aGlobal,
           const WeakPtr<locks::LockRequestChild>& aLockRequestChild,
           const nsString& aName, LockMode aMode,
           const RefPtr<Promise>& aReleasedPromise, ErrorResult& aRv)
    : mOwner(aGlobal),
      mLockRequestChild(aLockRequestChild),
      mName(aName),
      mMode(aMode),
      mWaitingPromise(Promise::Create(aGlobal, aRv)),
      mReleasedPromise(aReleasedPromise) {
  MOZ_ASSERT(mLockRequestChild);
  MOZ_ASSERT(aReleasedPromise);
}

JSObject* Lock::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return Lock_Binding::Wrap(aCx, this, aGivenProto);
}

void Lock::GetName(nsString& aRetVal) const { aRetVal = mName; }

LockMode Lock::Mode() const { return mMode; }

Promise& Lock::GetWaitingPromise() {
  MOZ_ASSERT(mWaitingPromise);
  return *mWaitingPromise;
}

void Lock::ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                            ErrorResult& aRv) {
  if (mLockRequestChild) {
    locks::PLockRequestChild::Send__delete__(mLockRequestChild, false);
    mLockRequestChild = nullptr;
  }
  mReleasedPromise->MaybeResolve(aValue);
}

void Lock::RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                            ErrorResult& aRv) {
  if (mLockRequestChild) {
    locks::PLockRequestChild::Send__delete__(mLockRequestChild, false);
    mLockRequestChild = nullptr;
  }
  mReleasedPromise->MaybeReject(aValue);
}

}  // namespace mozilla::dom
