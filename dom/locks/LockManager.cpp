/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/LockManager.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/LockManagerBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Promise-inl.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "nsIRunnable.h"
#include "nsIDUtils.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(LockManager)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(LockManager)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner, mHeldLockSet)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(LockManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner, mHeldLockSet)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(LockManager)

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

void LockManager::Shutdown() {
  mQueueMap.Clear();
  // TODO: release the remaining locks and requests made in this instance when
  // shared lock manager is implemented
}

static bool HasStorageAccess(nsIGlobalObject* aGlobal) {
  StorageAccess access;

  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal);
    if (NS_WARN_IF(!window)) {
      return true;
    }

    access = StorageAllowedForWindow(window);
  } else {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    access = workerPrivate->StorageAccess();
  }

  return access > StorageAccess::eDeny;
}

static nsString GetClientId(nsIGlobalObject* aGlobal) {
  return NSID_TrimBracketsUTF16(aGlobal->GetClientInfo()->Id());
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
      aRv.ThrowAbortError("The lock request is aborted");
      return false;
    }
  }
  return true;
}

// XXX: should be MOZ_CAN_RUN_SCRIPT, but not sure how to call it from closures
MOZ_CAN_RUN_SCRIPT_BOUNDARY static void RunCallbackAndSettlePromise(
    LockGrantedCallback& aCallback, mozilla::dom::Lock* lock,
    Promise& aPromise) {
  ErrorResult rv;
  if (RefPtr<Promise> result = aCallback.Call(
          lock, rv, nullptr, CallbackObject::eRethrowExceptions)) {
    aPromise.MaybeResolve(result);
  } else if (rv.Failed() && !rv.IsUncatchableException()) {
    aPromise.MaybeReject(std::move(rv));
  } else {
    aPromise.MaybeResolveWithUndefined();
  }
  // This is required even with no failure. IgnoredErrorResult is not an option
  // since MaybeReject does not accept it.
  rv.WouldReportJSException();
  MOZ_ASSERT(!rv.Failed());
}

already_AddRefed<Promise> LockManager::Request(const nsAString& name,
                                               LockGrantedCallback& callback,
                                               ErrorResult& aRv) {
  return Request(name, LockOptions(), callback, aRv);
};
already_AddRefed<Promise> LockManager::Request(const nsAString& name,
                                               const LockOptions& options,
                                               LockGrantedCallback& callback,
                                               ErrorResult& aRv) {
  if (!HasStorageAccess(mOwner)) {
    // Step 4: If origin is an opaque origin, then return a promise rejected
    // with a "SecurityError" DOMException.
    // But per https://wicg.github.io/web-locks/#lock-managers this really means
    // whether it has storage access.
    aRv.ThrowSecurityError("request() is not allowed in this context");
    return nullptr;
  }
  if (!ValidateRequestArguments(name, options, aRv)) {
    return nullptr;
  }

  if (options.mSignal.WasPassed()) {
    aRv.ThrowNotSupportedError("AbortSignal support is not implemented yet");
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(mOwner, aRv);
  LockRequest request = {nsString(name), options.mMode, promise, &callback};

  nsCOMPtr<nsIRunnable> runCallback(NS_NewRunnableFunction(
      "RequestLock",
      [self = RefPtr<LockManager>(this), request, options]() -> void {
        nsTArray<mozilla::dom::LockRequest>& queue =
            self->mQueueMap.LookupOrInsert(request.mName);
        if (options.mSteal) {
          self->mHeldLockSet.RemoveIf([&request](Lock* lock) {
            if (lock->mName == request.mName) {
              lock->mReleasedPromise->MaybeRejectWithAbortError(
                  "The lock request is aborted");
              return true;
            }
            return false;
          });
          queue.InsertElementAt(0, request);
        } else if (options.mIfAvailable &&
                   !self->IsGrantableRequest(request, queue)) {
          // TODO: this must be asynchronous! "enqueue the following steps"
          RunCallbackAndSettlePromise(*request.mCallback, nullptr,
                                      *request.mPromise);
          return;
        } else {
          queue.AppendElement(request);
        }
        self->ProcessRequestQueue(queue);
      }));
  Unused << NS_DispatchToCurrentThreadQueue(runCallback.forget(),
                                            EventQueuePriority::Idle);
  // TODO: AbortSignal support
  // if (options.mSignal.WasPassed()) {
  //   options.mSignal.Value()
  // }
  return promise.forget();
};

already_AddRefed<Promise> LockManager::Query(ErrorResult& aRv) {
  RefPtr<Promise> promise = Promise::Create(mOwner, aRv);

  // TODO: this should be retrieved via IPC

  LockManagerSnapshot snapshot;
  snapshot.mHeld.Construct();
  snapshot.mPending.Construct();
  for (const auto& queueMapEntry : mQueueMap) {
    for (const LockRequest& request : queueMapEntry.GetData()) {
      LockInfo info;
      info.mMode.Construct(request.mMode);
      info.mName.Construct(request.mName);
      info.mClientId.Construct(
          GetClientId(request.mPromise->GetGlobalObject()));
      if (!snapshot.mPending.Value().AppendElement(info, mozilla::fallible)) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return nullptr;
      };
    }
  }
  for (const Lock* lock : mHeldLockSet) {
    LockInfo info;
    info.mMode.Construct(lock->mMode);
    info.mName.Construct(lock->mName);
    info.mClientId.Construct(GetClientId(lock->mOwner));
    if (!snapshot.mHeld.Value().AppendElement(info, mozilla::fallible)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    };
  }
  promise->MaybeResolve(snapshot);
  return promise.forget();
};

void LockManager::ReleaseHeldLock(Lock* aLock) {
  MOZ_ASSERT(mHeldLockSet.Contains(aLock), "Lock not held??");
  mHeldLockSet.Remove(aLock);
  if (auto queue = mQueueMap.Lookup(aLock->mName)) {
    ProcessRequestQueue(queue.Data());
    if (queue.Data().IsEmpty()) {
      // Remove if empty, to prevent the queue map from growing forever
      mQueueMap.Remove(aLock->mName);
    }
  }
  // or else, the queue is removed during the previous lock release
}

void LockManager::ProcessRequestQueue(
    nsTArray<mozilla::dom::LockRequest>& aQueue) {
  // use manual loop since range-loop is not safe for mutable arrays
  for (uint32_t i = 0; i < aQueue.Length(); i++) {
    auto& request = aQueue[i];
    if (IsGrantableRequest(request, aQueue)) {
      RefPtr<Lock> lock = new Lock(mOwner, this, request.mName, request.mMode,
                                   request.mPromise, IgnoreErrors() /* ?? */);
      mHeldLockSet.Insert(lock);

      lock->GetWaitingPromise().AppendNativeHandler(lock);

      RefPtr<LockGrantedCallback> callback = request.mCallback;
      aQueue.RemoveElement(request);
      nsCOMPtr<nsIRunnable> runCallback(NS_NewRunnableFunction(
          "RunCallbackAndSettlePromise", [callback, lock]() -> void {
            RunCallbackAndSettlePromise(*callback, lock,
                                        lock->GetWaitingPromise());
          }));
      // TODO: This should go to lock task queue
      Unused << NS_DispatchToCurrentThreadQueue(runCallback.forget(),
                                                EventQueuePriority::Idle);
    }
  }
}

bool LockManager::IsGrantableRequest(
    const LockRequest& aRequest, nsTArray<mozilla::dom::LockRequest>& aQueue) {
  if (HasBlockingHeldLock(aRequest.mName, aRequest.mMode)) {
    return false;
  }
  if (!aQueue.Length()) {
    return true;  // Possible as this check also happens before enqueuing
  }
  return aQueue[0] == aRequest;
}

bool LockManager::HasBlockingHeldLock(const nsString& aName, LockMode aMode) {
  for (const auto& lock : mHeldLockSet) {
    if (lock->mName == aName) {
      if (aMode == LockMode::Exclusive) {
        return true;
      }
      MOZ_ASSERT(aMode == LockMode::Shared);
      if (lock->mMode == LockMode::Exclusive) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace mozilla::dom
