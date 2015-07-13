/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"

#include "ServiceWorkerClient.h"
#include "ServiceWorkerClients.h"
#include "ServiceWorkerManager.h"
#include "ServiceWorkerWindowClient.h"

#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::workers;

NS_IMPL_CYCLE_COLLECTING_ADDREF(ServiceWorkerClients)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ServiceWorkerClients)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ServiceWorkerClients, mWorkerScope)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServiceWorkerClients)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ServiceWorkerClients::ServiceWorkerClients(ServiceWorkerGlobalScope* aWorkerScope)
  : mWorkerScope(aWorkerScope)
{
  MOZ_ASSERT(mWorkerScope);
}

JSObject*
ServiceWorkerClients::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ClientsBinding::Wrap(aCx, this, aGivenProto);
}

namespace {

class ResolvePromiseWorkerRunnable final : public WorkerRunnable
{
  nsRefPtr<PromiseWorkerProxy> mPromiseProxy;
  nsTArray<ServiceWorkerClientInfo> mValue;

public:
  ResolvePromiseWorkerRunnable(WorkerPrivate* aWorkerPrivate,
                               PromiseWorkerProxy* aPromiseProxy,
                               nsTArray<ServiceWorkerClientInfo>& aValue)
    : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount),
      mPromiseProxy(aPromiseProxy)
  {
    AssertIsOnMainThread();
    mValue.SwapElements(aValue);
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    Promise* promise = mPromiseProxy->GetWorkerPromise();
    MOZ_ASSERT(promise);

    nsTArray<nsRefPtr<ServiceWorkerClient>> ret;
    for (size_t i = 0; i < mValue.Length(); i++) {
      ret.AppendElement(nsRefPtr<ServiceWorkerClient>(
            new ServiceWorkerWindowClient(promise->GetParentObject(),
                                          mValue.ElementAt(i))));
    }
    promise->MaybeResolve(ret);

    // release the reference on the worker thread.
    mPromiseProxy->CleanUp(aCx);

    return true;
  }
};

class MatchAllRunnable final : public nsRunnable
{
  WorkerPrivate* mWorkerPrivate;
  nsRefPtr<PromiseWorkerProxy> mPromiseProxy;
  nsCString mScope;
public:
  MatchAllRunnable(WorkerPrivate* aWorkerPrivate,
                   PromiseWorkerProxy* aPromiseProxy,
                   const nsCString& aScope)
    : mWorkerPrivate(aWorkerPrivate),
      mPromiseProxy(aPromiseProxy),
      mScope(aScope)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();

    MutexAutoLock lock(mPromiseProxy->GetCleanUpLock());
    if (mPromiseProxy->IsClean()) {
      // Don't resolve the promise if it was already released.
      return NS_OK;
    }

    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    nsTArray<ServiceWorkerClientInfo> result;

    swm->GetAllClients(mWorkerPrivate->GetPrincipal(), mScope, result);
    nsRefPtr<ResolvePromiseWorkerRunnable> r =
      new ResolvePromiseWorkerRunnable(mWorkerPrivate, mPromiseProxy, result);

    AutoSafeJSContext cx;
    if (r->Dispatch(cx)) {
      return NS_OK;
    }

    // Dispatch to worker thread failed because the worker is shutting down.
    // Use a control runnable to release the runnable on the worker thread.
    nsRefPtr<PromiseWorkerProxyControlRunnable> releaseRunnable =
      new PromiseWorkerProxyControlRunnable(mWorkerPrivate, mPromiseProxy);

    if (!releaseRunnable->Dispatch(cx)) {
      NS_RUNTIMEABORT("Failed to dispatch MatchAll promise control runnable.");
    }

    return NS_OK;
  }
};

class ResolveClaimRunnable final : public WorkerRunnable
{
  nsRefPtr<PromiseWorkerProxy> mPromiseProxy;
  nsresult mResult;

public:
  ResolveClaimRunnable(WorkerPrivate* aWorkerPrivate,
                       PromiseWorkerProxy* aPromiseProxy,
                       nsresult aResult)
    : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount)
    , mPromiseProxy(aPromiseProxy)
    , mResult(aResult)
  {
    AssertIsOnMainThread();
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    Promise* promise = mPromiseProxy->GetWorkerPromise();
    MOZ_ASSERT(promise);

    if (NS_SUCCEEDED(mResult)) {
      promise->MaybeResolve(JS::UndefinedHandleValue);
    } else {
      promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    }

    // Release the reference on the worker thread.
    mPromiseProxy->CleanUp(aCx);

    return true;
  }
};

class ClaimRunnable final : public nsRunnable
{
  nsRefPtr<PromiseWorkerProxy> mPromiseProxy;
  nsCString mScope;
  // We grab the ID so we don't have to hold a lock the entire time the claim
  // operation is happening on the main thread.
  uint64_t mServiceWorkerID;

public:
  ClaimRunnable(PromiseWorkerProxy* aPromiseProxy, const nsCString& aScope)
    : mPromiseProxy(aPromiseProxy)
    , mScope(aScope)
    , mServiceWorkerID(aPromiseProxy->GetWorkerPrivate()->ServiceWorkerID())
  {
    MOZ_ASSERT(aPromiseProxy);
  }

  NS_IMETHOD
  Run() override
  {
    MutexAutoLock lock(mPromiseProxy->GetCleanUpLock());
    if (mPromiseProxy->IsClean()) {
      // Don't resolve the promise if it was already released.
      return NS_OK;
    }

    WorkerPrivate* workerPrivate = mPromiseProxy->GetWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    MOZ_ASSERT(swm);

    nsresult rv = swm->ClaimClients(workerPrivate->GetPrincipal(),
                                    mScope, mServiceWorkerID);

    nsRefPtr<ResolveClaimRunnable> r =
      new ResolveClaimRunnable(workerPrivate, mPromiseProxy, rv);

    AutoJSAPI jsapi;
    jsapi.Init();
    JSContext* cx = jsapi.cx();
    if (r->Dispatch(cx)) {
      return NS_OK;
    }

    // Dispatch to worker thread failed because the worker is shutting down.
    // Use a control runnable to release the runnable on the worker thread.
    nsRefPtr<PromiseWorkerProxyControlRunnable> releaseRunnable =
      new PromiseWorkerProxyControlRunnable(workerPrivate, mPromiseProxy);

    if (!releaseRunnable->Dispatch(cx)) {
      NS_RUNTIMEABORT("Failed to dispatch Claim control runnable.");
    }

    return NS_OK;
  }
};

} // namespace

already_AddRefed<Promise>
ServiceWorkerClients::MatchAll(const ClientQueryOptions& aOptions,
                               ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  nsString scope;
  mWorkerScope->GetScope(scope);

  if (aOptions.mIncludeUncontrolled || aOptions.mType != ClientType::Window) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(mWorkerScope, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsRefPtr<PromiseWorkerProxy> promiseProxy =
    PromiseWorkerProxy::Create(workerPrivate, promise);
  if (!promiseProxy->GetWorkerPromise()) {
    // Don't dispatch if adding the worker feature failed.
    return promise.forget();
  }

  nsRefPtr<MatchAllRunnable> r =
    new MatchAllRunnable(workerPrivate,
                         promiseProxy,
                         NS_ConvertUTF16toUTF8(scope));
  nsresult rv = NS_DispatchToMainThread(r);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
  }

  return promise.forget();
}

already_AddRefed<Promise>
ServiceWorkerClients::OpenWindow(const nsAString& aUrl)
{
  ErrorResult result;
  nsRefPtr<Promise> promise = Promise::Create(mWorkerScope, result);
  if (NS_WARN_IF(result.Failed())) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
  return promise.forget();
}

already_AddRefed<Promise>
ServiceWorkerClients::Claim(ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<Promise> promise = Promise::Create(mWorkerScope, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsRefPtr<PromiseWorkerProxy> promiseProxy =
    PromiseWorkerProxy::Create(workerPrivate, promise);
  if (!promiseProxy->GetWorkerPromise()) {
    // Don't dispatch if adding the worker feature failed.
    return promise.forget();
  }

  nsString scope;
  mWorkerScope->GetScope(scope);

  nsRefPtr<ClaimRunnable> runnable =
    new ClaimRunnable(promiseProxy, NS_ConvertUTF16toUTF8(scope));

  aRv = NS_DispatchToMainThread(runnable);
  if (NS_WARN_IF(aRv.Failed())) {
    promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }

  return promise.forget();
}
