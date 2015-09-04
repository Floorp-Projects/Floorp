/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ServiceWorkerWindowClient.h"

#include "mozilla/Mutex.h"
#include "mozilla/dom/ClientBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/UniquePtr.h"
#include "nsGlobalWindow.h"
#include "WorkerPrivate.h"

using namespace mozilla::dom;
using namespace mozilla::dom::workers;

using mozilla::UniquePtr;

JSObject*
ServiceWorkerWindowClient::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return WindowClientBinding::Wrap(aCx, this, aGivenProto);
}

namespace {

// Passing a null clientInfo will reject the promise with InvalidAccessError.
class ResolveOrRejectPromiseRunnable final : public WorkerRunnable
{
  nsRefPtr<PromiseWorkerProxy> mPromiseProxy;
  UniquePtr<ServiceWorkerClientInfo> mClientInfo;

public:
  ResolveOrRejectPromiseRunnable(WorkerPrivate* aWorkerPrivate,
                                 PromiseWorkerProxy* aPromiseProxy,
                                 UniquePtr<ServiceWorkerClientInfo>&& aClientInfo)
    : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount)
    , mPromiseProxy(aPromiseProxy)
    , mClientInfo(Move(aClientInfo))
  {
    AssertIsOnMainThread();
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    nsRefPtr<Promise> promise = mPromiseProxy->WorkerPromise();
    MOZ_ASSERT(promise);

    if (mClientInfo) {
      nsRefPtr<ServiceWorkerWindowClient> client =
        new ServiceWorkerWindowClient(promise->GetParentObject(), *mClientInfo);
      promise->MaybeResolve(client);
    } else {
      promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    }

    // Release the reference on the worker thread.
    mPromiseProxy->CleanUp(aCx);

    return true;
  }
};

class ClientFocusRunnable final : public nsRunnable
{
  uint64_t mWindowId;
  nsRefPtr<PromiseWorkerProxy> mPromiseProxy;

public:
  ClientFocusRunnable(uint64_t aWindowId, PromiseWorkerProxy* aPromiseProxy)
    : mWindowId(aWindowId)
    , mPromiseProxy(aPromiseProxy)
  {
    MOZ_ASSERT(mPromiseProxy);
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();
    nsGlobalWindow* window = nsGlobalWindow::GetInnerWindowWithId(mWindowId);
    UniquePtr<ServiceWorkerClientInfo> clientInfo;

    if (window) {
      nsCOMPtr<nsIDocument> doc = window->GetDocument();
      if (doc) {
        nsContentUtils::DispatchChromeEvent(doc,
                                            window->GetOuterWindow(),
                                            NS_LITERAL_STRING("DOMServiceWorkerFocusClient"),
                                            true, true);
        clientInfo.reset(new ServiceWorkerClientInfo(doc));
      }
    }

    DispatchResult(Move(clientInfo));
    return NS_OK;
  }

private:
  void
  DispatchResult(UniquePtr<ServiceWorkerClientInfo>&& aClientInfo)
  {
    AssertIsOnMainThread();
    MutexAutoLock lock(mPromiseProxy->Lock());
    if (mPromiseProxy->CleanedUp()) {
      return;
    }

    nsRefPtr<ResolveOrRejectPromiseRunnable> resolveRunnable =
      new ResolveOrRejectPromiseRunnable(mPromiseProxy->GetWorkerPrivate(),
                                         mPromiseProxy, Move(aClientInfo));

    AutoJSAPI jsapi;
    jsapi.Init();
    resolveRunnable->Dispatch(jsapi.cx());
  }
};

} // namespace

already_AddRefed<Promise>
ServiceWorkerWindowClient::Focus(ErrorResult& aRv) const
{
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  MOZ_ASSERT(global);

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (workerPrivate->GlobalScope()->WindowInteractionAllowed()) {
    nsRefPtr<PromiseWorkerProxy> promiseProxy =
      PromiseWorkerProxy::Create(workerPrivate, promise);
    if (promiseProxy) {
      nsRefPtr<ClientFocusRunnable> r = new ClientFocusRunnable(mWindowId,
                                                                promiseProxy);
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(r)));
    } else {
      promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    }

  } else {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
  }

  return promise.forget();
}
