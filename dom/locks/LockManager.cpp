/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/LockManager.h"
#include "mozilla/dom/AutoEntryScript.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/locks/LockManagerChild.h"
#include "mozilla/dom/locks/LockRequestChild.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/LockManagerBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/locks/PLockManager.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(LockManager, mOwner)
NS_IMPL_CYCLE_COLLECTING_ADDREF(LockManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(LockManager)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(LockManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* LockManager::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return LockManager_Binding::Wrap(aCx, this, aGivenProto);
}

LockManager::LockManager(nsIGlobalObject* aGlobal) : mOwner(aGlobal) {
  Maybe<ClientInfo> clientInfo = aGlobal->GetClientInfo();
  if (!clientInfo) {
    // Pass the nonworking object and let request()/query() throw.
    return;
  }

  nsCOMPtr<nsIPrincipal> principal =
      clientInfo->GetPrincipal().unwrapOr(nullptr);
  if (!principal || !principal->GetIsContentPrincipal()) {
    // Same, the methods will throw instead of the constructor.
    return;
  }

  mozilla::ipc::PBackgroundChild* backgroundActor =
      mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();
  mActor = new locks::LockManagerChild(aGlobal);

  if (!backgroundActor->SendPLockManagerConstructor(
          mActor, WrapNotNull(principal), clientInfo->Id())) {
    // Failed to construct the actor. Pass the nonworking object and let the
    // methods throw.
    mActor = nullptr;
    return;
  }
}

already_AddRefed<LockManager> LockManager::Create(nsIGlobalObject& aGlobal) {
  RefPtr<LockManager> manager = new LockManager(&aGlobal);

  if (!NS_IsMainThread()) {
    // Grabbing WorkerRef may fail and that will cause the methods throw later.
    manager->mWorkerRef =
        WeakWorkerRef::Create(GetCurrentThreadWorkerPrivate(), [manager]() {
          // Others may grab a strong reference and block immediate destruction.
          // Shutdown early as we don't have to wait for them.
          manager->Shutdown();
          manager->mWorkerRef = nullptr;
        });
  }

  return manager.forget();
}

static bool ValidateRequestArguments(const nsAString& name,
                                     const LockOptions& options,
                                     ErrorResult& aRv) {
  if (name.Length() > 0 && name.First() == u'-') {
    aRv.ThrowNotSupportedError("Names starting with `-` are reserved");
    return false;
  }
  if (options.mSteal) {
    if (options.mIfAvailable) {
      aRv.ThrowNotSupportedError(
          "`steal` and `ifAvailable` cannot be used together");
      return false;
    }
    if (options.mMode != LockMode::Exclusive) {
      aRv.ThrowNotSupportedError(
          "`steal` is only supported for exclusive lock requests");
      return false;
    }
  }
  if (options.mSignal.WasPassed()) {
    if (options.mSteal) {
      aRv.ThrowNotSupportedError(
          "`steal` and `signal` cannot be used together");
      return false;
    }
    if (options.mIfAvailable) {
      aRv.ThrowNotSupportedError(
          "`ifAvailable` and `signal` cannot be used together");
      return false;
    }
    if (options.mSignal.Value().Aborted()) {
      AutoJSAPI jsapi;
      if (!jsapi.Init(options.mSignal.Value().GetParentObject())) {
        aRv.ThrowNotSupportedError("Signal's realm isn't active anymore.");
        return false;
      }

      JSContext* cx = jsapi.cx();
      JS::Rooted<JS::Value> reason(cx);
      options.mSignal.Value().GetReason(cx, &reason);
      aRv.MightThrowJSException();
      aRv.ThrowJSException(cx, reason);
      return false;
    }
  }
  return true;
}

already_AddRefed<Promise> LockManager::Request(const nsAString& aName,
                                               LockGrantedCallback& aCallback,
                                               ErrorResult& aRv) {
  return Request(aName, LockOptions(), aCallback, aRv);
};
already_AddRefed<Promise> LockManager::Request(const nsAString& aName,
                                               const LockOptions& aOptions,
                                               LockGrantedCallback& aCallback,
                                               ErrorResult& aRv) {
  if (!mOwner->GetClientInfo()) {
    // We do have nsPIDOMWindowInner::IsFullyActive for this kind of check,
    // but this should be sufficient here as unloaded iframe is the only
    // non-fully-active case that Web Locks should worry about (since it does
    // not enter bfcache).
    aRv.ThrowInvalidStateError(
        "The document of the lock manager is not fully active");
    return nullptr;
  }

  const StorageAccess access = mOwner->GetStorageAccess();
  bool allowed =
      access > StorageAccess::eDeny ||
      (StaticPrefs::
           privacy_partition_always_partition_third_party_non_cookie_storage() &&
       ShouldPartitionStorage(access));
  if (!allowed) {
    // Step 4: If origin is an opaque origin, then return a promise rejected
    // with a "SecurityError" DOMException.
    // But per https://w3c.github.io/web-locks/#lock-managers this really means
    // whether it has storage access.
    aRv.ThrowSecurityError("request() is not allowed in this context");
    return nullptr;
  }

  if (!mActor) {
    aRv.ThrowNotSupportedError(
        "Web Locks API is not enabled for this kind of document");
    return nullptr;
  }

  if (!NS_IsMainThread() && !mWorkerRef) {
    aRv.ThrowInvalidStateError("request() is not allowed at this point");
    return nullptr;
  }

  if (!ValidateRequestArguments(aName, aOptions, aRv)) {
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(mOwner, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  mActor->RequestLock({nsString(aName), promise, &aCallback}, aOptions);
  return promise.forget();
};

already_AddRefed<Promise> LockManager::Query(ErrorResult& aRv) {
  if (!mOwner->GetClientInfo()) {
    aRv.ThrowInvalidStateError(
        "The document of the lock manager is not fully active");
    return nullptr;
  }

  if (mOwner->GetStorageAccess() <= StorageAccess::eDeny) {
    aRv.ThrowSecurityError("query() is not allowed in this context");
    return nullptr;
  }

  if (!mActor) {
    aRv.ThrowNotSupportedError(
        "Web Locks API is not enabled for this kind of document");
    return nullptr;
  }

  if (!NS_IsMainThread() && !mWorkerRef) {
    aRv.ThrowInvalidStateError("query() is not allowed at this point");
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(mOwner, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  mActor->SendQuery()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [promise](locks::LockManagerChild::QueryPromise::ResolveOrRejectValue&&
                    aResult) {
        if (aResult.IsResolve()) {
          promise->MaybeResolve(aResult.ResolveValue());
        } else {
          promise->MaybeRejectWithUnknownError("Query failed");
        }
      });
  return promise.forget();
};

void LockManager::Shutdown() {
  if (mActor) {
    locks::PLockManagerChild::Send__delete__(mActor);
    mActor = nullptr;
  }
}

}  // namespace mozilla::dom
