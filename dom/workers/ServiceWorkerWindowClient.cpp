/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ServiceWorkerWindowClient.h"

#include "js/Value.h"
#include "mozilla/Mutex.h"
#include "mozilla/dom/ClientBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/UniquePtr.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIDocument.h"
#include "nsIGlobalObject.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIWebNavigation.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsString.h"
#include "nsWeakReference.h"
#include "ServiceWorker.h"
#include "ServiceWorkerInfo.h"
#include "ServiceWorkerManager.h"
#include "WorkerPrivate.h"
#include "WorkerScope.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::workers;

using mozilla::UniquePtr;

JSObject*
ServiceWorkerWindowClient::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return WindowClientBinding::Wrap(aCx, this, aGivenProto);
}

namespace {

class ResolveOrRejectPromiseRunnable final : public WorkerRunnable
{
  RefPtr<PromiseWorkerProxy> mPromiseProxy;
  UniquePtr<ServiceWorkerClientInfo> mClientInfo;
  nsresult mRv;

public:
  // Passing a null clientInfo will resolve the promise with a null value.
  ResolveOrRejectPromiseRunnable(
    WorkerPrivate* aWorkerPrivate, PromiseWorkerProxy* aPromiseProxy,
    UniquePtr<ServiceWorkerClientInfo>&& aClientInfo)
    : WorkerRunnable(aWorkerPrivate)
    , mPromiseProxy(aPromiseProxy)
    , mClientInfo(Move(aClientInfo))
    , mRv(NS_OK)
  {
    AssertIsOnMainThread();
  }

  // Reject the promise with passed nsresult.
  ResolveOrRejectPromiseRunnable(WorkerPrivate* aWorkerPrivate,
                                 PromiseWorkerProxy* aPromiseProxy,
                                 nsresult aRv)
    : WorkerRunnable(aWorkerPrivate)
    , mPromiseProxy(aPromiseProxy)
    , mClientInfo(nullptr)
    , mRv(aRv)
  {
    MOZ_ASSERT(NS_FAILED(aRv));
    AssertIsOnMainThread();
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    RefPtr<Promise> promise = mPromiseProxy->WorkerPromise();
    MOZ_ASSERT(promise);

    if (NS_WARN_IF(NS_FAILED(mRv))) {
      promise->MaybeReject(mRv);
    } else if (mClientInfo) {
      RefPtr<ServiceWorkerWindowClient> client =
        new ServiceWorkerWindowClient(promise->GetParentObject(), *mClientInfo);
      promise->MaybeResolve(client);
    } else {
      promise->MaybeResolve(JS::NullHandleValue);
    }

    // Release the reference on the worker thread.
    mPromiseProxy->CleanUp();

    return true;
  }
};

class ClientFocusRunnable final : public Runnable
{
  uint64_t mWindowId;
  RefPtr<PromiseWorkerProxy> mPromiseProxy;

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
        nsContentUtils::DispatchFocusChromeEvent(window->GetOuterWindow());
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

    RefPtr<ResolveOrRejectPromiseRunnable> resolveRunnable;
    if (aClientInfo) {
      resolveRunnable = new ResolveOrRejectPromiseRunnable(
        mPromiseProxy->GetWorkerPrivate(), mPromiseProxy, Move(aClientInfo));
    } else {
      resolveRunnable = new ResolveOrRejectPromiseRunnable(
        mPromiseProxy->GetWorkerPrivate(), mPromiseProxy,
        NS_ERROR_DOM_INVALID_ACCESS_ERR);
    }

    resolveRunnable->Dispatch();
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

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (workerPrivate->GlobalScope()->WindowInteractionAllowed()) {
    RefPtr<PromiseWorkerProxy> promiseProxy =
      PromiseWorkerProxy::Create(workerPrivate, promise);
    if (promiseProxy) {
      RefPtr<ClientFocusRunnable> r = new ClientFocusRunnable(mWindowId,
                                                              promiseProxy);
      MOZ_ALWAYS_SUCCEEDS(workerPrivate->DispatchToMainThread(r.forget()));
    } else {
      promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    }

  } else {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
  }

  return promise.forget();
}

class WebProgressListener final : public nsIWebProgressListener,
                                  public nsSupportsWeakReference
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(WebProgressListener,
                                           nsIWebProgressListener)

  WebProgressListener(PromiseWorkerProxy* aPromiseProxy,
                      nsPIDOMWindowOuter* aWindow, nsIURI* aBaseURI)
    : mPromiseProxy(aPromiseProxy)
    , mWindow(aWindow)
    , mBaseURI(aBaseURI)
  {
    MOZ_ASSERT(aPromiseProxy);
    MOZ_ASSERT(aWindow);
    MOZ_ASSERT(aWindow->IsOuterWindow());
    MOZ_ASSERT(aBaseURI);
    AssertIsOnMainThread();

    mPromiseProxy->StoreISupports(static_cast<nsIWebProgressListener*>(this));
  }

  NS_IMETHOD
  OnStateChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                uint32_t aStateFlags, nsresult aStatus) override
  {
    if (!(aStateFlags & STATE_IS_DOCUMENT) ||
        !(aStateFlags & (STATE_STOP | STATE_TRANSFERRING))) {
      return NS_OK;
    }

    aWebProgress->RemoveProgressListener(this);

    WorkerPrivate* workerPrivate;

    {
      MutexAutoLock lock(mPromiseProxy->Lock());
      if (mPromiseProxy->CleanedUp()) {
        return NS_OK;
      }

      workerPrivate = mPromiseProxy->GetWorkerPrivate();
    }

    nsCOMPtr<nsIDocument> doc = mWindow->GetExtantDoc();

    RefPtr<ResolveOrRejectPromiseRunnable> resolveRunnable;
    UniquePtr<ServiceWorkerClientInfo> clientInfo;
    if (!doc) {
      resolveRunnable = new ResolveOrRejectPromiseRunnable(
        workerPrivate, mPromiseProxy, NS_ERROR_TYPE_ERR);
      resolveRunnable->Dispatch();

      return NS_OK;
    }

    // Check same origin.
    nsCOMPtr<nsIScriptSecurityManager> securityManager =
      nsContentUtils::GetSecurityManager();
    nsresult rv = securityManager->CheckSameOriginURI(doc->GetOriginalURI(),
                                                      mBaseURI, false);

    if (NS_SUCCEEDED(rv)) {
      nsContentUtils::DispatchFocusChromeEvent(mWindow->GetOuterWindow());
      clientInfo.reset(new ServiceWorkerClientInfo(doc));
    }

    resolveRunnable = new ResolveOrRejectPromiseRunnable(
      workerPrivate, mPromiseProxy, Move(clientInfo));
    resolveRunnable->Dispatch();

    return NS_OK;
  }

  NS_IMETHOD
  OnProgressChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                   int32_t aCurSelfProgress, int32_t aMaxSelfProgress,
                   int32_t aCurTotalProgress,
                   int32_t aMaxTotalProgress) override
  {
    MOZ_CRASH("Unexpected notification.");
    return NS_OK;
  }

  NS_IMETHOD
  OnLocationChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                   nsIURI* aLocation, uint32_t aFlags) override
  {
    MOZ_CRASH("Unexpected notification.");
    return NS_OK;
  }

  NS_IMETHOD
  OnStatusChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                 nsresult aStatus, const char16_t* aMessage) override
  {
    MOZ_CRASH("Unexpected notification.");
    return NS_OK;
  }

  NS_IMETHOD
  OnSecurityChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                   uint32_t aState) override
  {
    MOZ_CRASH("Unexpected notification.");
    return NS_OK;
  }

private:
  ~WebProgressListener() {}

  RefPtr<PromiseWorkerProxy> mPromiseProxy;
  nsCOMPtr<nsPIDOMWindowOuter> mWindow;
  nsCOMPtr<nsIURI> mBaseURI;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebProgressListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebProgressListener)
NS_IMPL_CYCLE_COLLECTION(WebProgressListener, mPromiseProxy,
                         mWindow)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebProgressListener)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

class ClientNavigateRunnable final : public Runnable
{
  uint64_t mWindowId;
  nsString mUrl;
  nsCString mBaseUrl;
  RefPtr<PromiseWorkerProxy> mPromiseProxy;
  WorkerPrivate* mWorkerPrivate;

public:
  ClientNavigateRunnable(uint64_t aWindowId, const nsAString& aUrl,
                         PromiseWorkerProxy* aPromiseProxy)
    : mWindowId(aWindowId)
    , mUrl(aUrl)
    , mPromiseProxy(aPromiseProxy)
  {
    MOZ_ASSERT(aPromiseProxy);
    MOZ_ASSERT(aPromiseProxy->GetWorkerPrivate());
    aPromiseProxy->GetWorkerPrivate()->AssertIsOnWorkerThread();
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();

    nsCOMPtr<nsIPrincipal> principal;

    {
      MutexAutoLock lock(mPromiseProxy->Lock());
      if (mPromiseProxy->CleanedUp()) {
        return NS_OK;
      }

      mWorkerPrivate = mPromiseProxy->GetWorkerPrivate();
      WorkerPrivate::LocationInfo& info = mWorkerPrivate->GetLocationInfo();
      mBaseUrl = info.mHref;
      principal = mWorkerPrivate->GetPrincipal();
    }

    nsCOMPtr<nsIURI> baseUrl;
    nsCOMPtr<nsIURI> url;
    nsresult rv = ParseUrl(getter_AddRefs(baseUrl), getter_AddRefs(url));

    if (NS_WARN_IF(NS_FAILED(rv))) {
      return RejectPromise(NS_ERROR_TYPE_ERR);
    }

    rv = principal->CheckMayLoad(url, true, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return RejectPromise(rv);
    }

    nsGlobalWindow* window;
    rv = Navigate(url, principal, &window);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return RejectPromise(rv);
    }

    nsCOMPtr<nsIDocShell> docShell = window->GetDocShell();
    nsCOMPtr<nsIWebProgress> webProgress = do_GetInterface(docShell);
    if (NS_WARN_IF(!webProgress)) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIWebProgressListener> listener =
      new WebProgressListener(mPromiseProxy, window->GetOuterWindow(), baseUrl);

    rv = webProgress->AddProgressListener(
      listener, nsIWebProgress::NOTIFY_STATE_DOCUMENT);

    if (NS_WARN_IF(NS_FAILED(rv))) {
      return RejectPromise(rv);
    }

    return NS_OK;
  }

private:
  nsresult
  RejectPromise(nsresult aRv)
  {
    MOZ_ASSERT(mWorkerPrivate);
    RefPtr<ResolveOrRejectPromiseRunnable> resolveRunnable =
      new ResolveOrRejectPromiseRunnable(mWorkerPrivate, mPromiseProxy, aRv);

    resolveRunnable->Dispatch();
    return NS_OK;
  }

  nsresult
  ResolvePromise(UniquePtr<ServiceWorkerClientInfo>&& aClientInfo)
  {
    MOZ_ASSERT(mWorkerPrivate);
    RefPtr<ResolveOrRejectPromiseRunnable> resolveRunnable =
      new ResolveOrRejectPromiseRunnable(mWorkerPrivate, mPromiseProxy,
                                         Move(aClientInfo));

    resolveRunnable->Dispatch();
    return NS_OK;
  }

  nsresult
  ParseUrl(nsIURI** aBaseUrl, nsIURI** aUrl)
  {
    MOZ_ASSERT(aBaseUrl);
    MOZ_ASSERT(aUrl);
    AssertIsOnMainThread();

    nsCOMPtr<nsIURI> baseUrl;
    nsresult rv = NS_NewURI(getter_AddRefs(baseUrl), mBaseUrl);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> url;
    rv = NS_NewURI(getter_AddRefs(url), mUrl, nullptr, baseUrl);
    NS_ENSURE_SUCCESS(rv, rv);

    baseUrl.forget(aBaseUrl);
    url.forget(aUrl);

    return NS_OK;
  }

  nsresult
  Navigate(nsIURI* aUrl, nsIPrincipal* aPrincipal, nsGlobalWindow** aWindow)
  {
    MOZ_ASSERT(aWindow);

    nsGlobalWindow* window = nsGlobalWindow::GetInnerWindowWithId(mWindowId);
    if (NS_WARN_IF(!window)) {
      return NS_ERROR_TYPE_ERR;
    }

    nsCOMPtr<nsIDocument> doc = window->GetDocument();
    if (NS_WARN_IF(!doc)) {
      return NS_ERROR_TYPE_ERR;
    }

    if (NS_WARN_IF(!doc->IsActive())) {
      return NS_ERROR_TYPE_ERR;
    }

    nsCOMPtr<nsIDocShell> docShell = window->GetDocShell();
    if (NS_WARN_IF(!docShell)) {
      return NS_ERROR_TYPE_ERR;
    }

    nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
    nsresult rv = docShell->CreateLoadInfo(getter_AddRefs(loadInfo));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    loadInfo->SetTriggeringPrincipal(aPrincipal);
    loadInfo->SetReferrer(doc->GetOriginalURI());
    loadInfo->SetReferrerPolicy(doc->GetReferrerPolicy());
    loadInfo->SetLoadType(nsIDocShellLoadInfo::loadStopContentAndReplace);
    loadInfo->SetSourceDocShell(docShell);
    rv =
      docShell->LoadURI(aUrl, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE, true);

    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    *aWindow = window;
    return NS_OK;
  }
};

already_AddRefed<Promise>
ServiceWorkerWindowClient::Navigate(const nsAString& aUrl, ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  MOZ_ASSERT(global);

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (aUrl.EqualsLiteral("about:blank")) {
    promise->MaybeReject(NS_ERROR_TYPE_ERR);
    return promise.forget();
  }

  RefPtr<PromiseWorkerProxy> promiseProxy =
    PromiseWorkerProxy::Create(workerPrivate, promise);
  if (promiseProxy) {
    RefPtr<ClientNavigateRunnable> r =
      new ClientNavigateRunnable(mWindowId, aUrl, promiseProxy);
    MOZ_ALWAYS_SUCCEEDS(workerPrivate->DispatchToMainThread(r.forget()));
  } else {
    promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }

  return promise.forget();
}
