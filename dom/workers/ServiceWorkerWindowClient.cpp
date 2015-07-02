/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ServiceWorkerWindowClient.h"

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

    Promise* promise = mPromiseProxy->GetWorkerPromise();
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
    MOZ_ASSERT(mPromiseProxy->GetWorkerPromise());
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();
    nsGlobalWindow* window = nsGlobalWindow::GetInnerWindowWithId(mWindowId);
    UniquePtr<ServiceWorkerClientInfo> clientInfo;

    if (window) {
      mozilla::ErrorResult result;
      //FIXME(catalinb): Bug 1144660 - check if we are allowed to focus here.
      window->Focus(result);
      clientInfo.reset(new ServiceWorkerClientInfo(window->GetDocument(),
                                                   window->GetOuterWindow()));
    }

    DispatchResult(Move(clientInfo));
    return NS_OK;
  }

private:
  void
  DispatchResult(UniquePtr<ServiceWorkerClientInfo>&& aClientInfo)
  {
    WorkerPrivate* workerPrivate = mPromiseProxy->GetWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    nsRefPtr<ResolveOrRejectPromiseRunnable> resolveRunnable =
      new ResolveOrRejectPromiseRunnable(workerPrivate, mPromiseProxy,
                                         Move(aClientInfo));

    AutoJSAPI jsapi;
    jsapi.Init();
    JSContext* cx = jsapi.cx();
    if (!resolveRunnable->Dispatch(cx)) {
      nsRefPtr<PromiseWorkerProxyControlRunnable> controlRunnable =
        new PromiseWorkerProxyControlRunnable(workerPrivate, mPromiseProxy);
      if (!controlRunnable->Dispatch(cx)) {
        NS_RUNTIMEABORT("Failed to dispatch Focus promise control runnable.");
      }
    }
  }
};

} // anonymous namespace

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

  nsRefPtr<PromiseWorkerProxy> promiseProxy =
    PromiseWorkerProxy::Create(workerPrivate, promise);
  if (!promiseProxy->GetWorkerPromise()) {
    // Don't dispatch if adding the worker feature failed.
    return promise.forget();
  }

  nsRefPtr<ClientFocusRunnable> r = new ClientFocusRunnable(mWindowId,
                                                            promiseProxy);
  aRv = NS_DispatchToMainThread(r);
  if (NS_WARN_IF(aRv.Failed())) {
    promise->MaybeReject(aRv);
  }

  return promise.forget();
}
