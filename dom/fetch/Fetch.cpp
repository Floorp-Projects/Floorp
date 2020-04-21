/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Fetch.h"

#include "mozilla/dom/Document.h"
#include "nsIGlobalObject.h"

#include "nsCharSeparatedTokenizer.h"
#include "nsDOMString.h"
#include "nsJSUtils.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsProxyRelease.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/BodyConsumer.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/FetchDriver.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FormData.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/RemoteWorkerChild.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/URLSearchParams.h"
#include "mozilla/net/CookieJarSettings.h"

#include "BodyExtractor.h"
#include "EmptyBody.h"
#include "FetchObserver.h"
#include "InternalRequest.h"
#include "InternalResponse.h"

#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerScope.h"

namespace mozilla {
namespace dom {

namespace {

void AbortStream(JSContext* aCx, JS::Handle<JSObject*> aStream,
                 ErrorResult& aRv) {
  aRv.MightThrowJSException();

  bool isReadable;
  if (!JS::ReadableStreamIsReadable(aCx, aStream, &isReadable)) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }
  if (!isReadable) {
    return;
  }

  RefPtr<DOMException> e = DOMException::Create(NS_ERROR_DOM_ABORT_ERR);

  JS::Rooted<JS::Value> value(aCx);
  if (!GetOrCreateDOMReflector(aCx, e, &value)) {
    return;
  }

  if (!JS::ReadableStreamError(aCx, aStream, value)) {
    aRv.StealExceptionFromJSContext(aCx);
  }
}

}  // namespace

class AbortSignalMainThread final : public AbortSignalImpl {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(AbortSignalMainThread)

  explicit AbortSignalMainThread(bool aAborted) : AbortSignalImpl(aAborted) {}

 private:
  ~AbortSignalMainThread() = default;
};

NS_IMPL_CYCLE_COLLECTION_CLASS(AbortSignalMainThread)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AbortSignalMainThread)
  tmp->Unfollow();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AbortSignalMainThread)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFollowingSignal)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AbortSignalMainThread)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(AbortSignalMainThread)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AbortSignalMainThread)

// This class helps the proxying of AbortSignalImpl changes cross threads.
class AbortSignalProxy final : public AbortFollower {
  // This is created and released on the main-thread.
  RefPtr<AbortSignalImpl> mSignalImplMainThread;

  // The main-thread event target for runnable dispatching.
  nsCOMPtr<nsIEventTarget> mMainThreadEventTarget;

  // This value is used only for the creation of AbortSignalImpl on the
  // main-thread. They are not updated.
  const bool mAborted;

  // This runnable propagates changes from the AbortSignalImpl on workers to the
  // AbortSignalImpl on main-thread.
  class AbortSignalProxyRunnable final : public Runnable {
    RefPtr<AbortSignalProxy> mProxy;

   public:
    explicit AbortSignalProxyRunnable(AbortSignalProxy* aProxy)
        : Runnable("dom::AbortSignalProxy::AbortSignalProxyRunnable"),
          mProxy(aProxy) {}

    NS_IMETHOD
    Run() override {
      MOZ_ASSERT(NS_IsMainThread());
      AbortSignalImpl* signalImpl =
          mProxy->GetOrCreateSignalImplForMainThread();
      signalImpl->Abort();
      return NS_OK;
    }
  };

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AbortSignalProxy)

  AbortSignalProxy(AbortSignalImpl* aSignalImpl,
                   nsIEventTarget* aMainThreadEventTarget)
      : mMainThreadEventTarget(aMainThreadEventTarget),
        mAborted(aSignalImpl->Aborted()) {
    MOZ_ASSERT(mMainThreadEventTarget);
    Follow(aSignalImpl);
  }

  void Abort() override {
    RefPtr<AbortSignalProxyRunnable> runnable =
        new AbortSignalProxyRunnable(this);
    mMainThreadEventTarget->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
  }

  AbortSignalImpl* GetOrCreateSignalImplForMainThread() {
    MOZ_ASSERT(NS_IsMainThread());
    if (!mSignalImplMainThread) {
      mSignalImplMainThread = new AbortSignalMainThread(mAborted);
    }
    return mSignalImplMainThread;
  }

  AbortSignalImpl* GetSignalImplForTargetThread() { return mFollowingSignal; }

  void Shutdown() { Unfollow(); }

 private:
  ~AbortSignalProxy() {
    NS_ProxyRelease("AbortSignalProxy::mSignalImplMainThread",
                    mMainThreadEventTarget, mSignalImplMainThread.forget());
  }
};

class WorkerFetchResolver final : public FetchDriverObserver {
  // Thread-safe:
  RefPtr<PromiseWorkerProxy> mPromiseProxy;
  RefPtr<AbortSignalProxy> mSignalProxy;

  // Touched only on the worker thread.
  RefPtr<FetchObserver> mFetchObserver;
  RefPtr<WeakWorkerRef> mWorkerRef;
  bool mIsShutdown;

 public:
  // Returns null if worker is shutting down.
  static already_AddRefed<WorkerFetchResolver> Create(
      WorkerPrivate* aWorkerPrivate, Promise* aPromise,
      AbortSignalImpl* aSignalImpl, FetchObserver* aObserver) {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    RefPtr<PromiseWorkerProxy> proxy =
        PromiseWorkerProxy::Create(aWorkerPrivate, aPromise);
    if (!proxy) {
      return nullptr;
    }

    RefPtr<AbortSignalProxy> signalProxy;
    if (aSignalImpl) {
      signalProxy = new AbortSignalProxy(
          aSignalImpl, aWorkerPrivate->MainThreadEventTarget());
    }

    RefPtr<WorkerFetchResolver> r =
        new WorkerFetchResolver(proxy, signalProxy, aObserver);

    RefPtr<WeakWorkerRef> workerRef = WeakWorkerRef::Create(
        aWorkerPrivate, [r]() { r->Shutdown(r->mWorkerRef->GetPrivate()); });
    if (NS_WARN_IF(!workerRef)) {
      return nullptr;
    }

    r->mWorkerRef = std::move(workerRef);

    return r.forget();
  }

  AbortSignalImpl* GetAbortSignalForMainThread() {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mSignalProxy) {
      return nullptr;
    }

    return mSignalProxy->GetOrCreateSignalImplForMainThread();
  }

  AbortSignalImpl* GetAbortSignalForTargetThread() {
    mPromiseProxy->GetWorkerPrivate()->AssertIsOnWorkerThread();

    if (!mSignalProxy) {
      return nullptr;
    }

    return mSignalProxy->GetSignalImplForTargetThread();
  }

  PromiseWorkerProxy* PromiseProxy() const {
    MOZ_ASSERT(NS_IsMainThread());
    return mPromiseProxy;
  }

  Promise* WorkerPromise(WorkerPrivate* aWorkerPrivate) const {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(!mIsShutdown);

    return mPromiseProxy->WorkerPromise();
  }

  FetchObserver* GetFetchObserver(WorkerPrivate* aWorkerPrivate) const {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    return mFetchObserver;
  }

  void OnResponseAvailableInternal(InternalResponse* aResponse) override;

  void OnResponseEnd(FetchDriverObserver::EndReason eReason) override;

  bool NeedOnDataAvailable() override;

  void OnDataAvailable() override;

  void Shutdown(WorkerPrivate* aWorkerPrivate) {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    mIsShutdown = true;
    mPromiseProxy->CleanUp();

    mFetchObserver = nullptr;

    if (mSignalProxy) {
      mSignalProxy->Shutdown();
    }

    mWorkerRef = nullptr;
  }

  bool IsShutdown(WorkerPrivate* aWorkerPrivate) const {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    return mIsShutdown;
  }

 private:
  WorkerFetchResolver(PromiseWorkerProxy* aProxy,
                      AbortSignalProxy* aSignalProxy, FetchObserver* aObserver)
      : mPromiseProxy(aProxy),
        mSignalProxy(aSignalProxy),
        mFetchObserver(aObserver),
        mIsShutdown(false) {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mPromiseProxy);
  }

  ~WorkerFetchResolver() = default;

  virtual void FlushConsoleReport() override;
};

class MainThreadFetchResolver final : public FetchDriverObserver {
  RefPtr<Promise> mPromise;
  RefPtr<Response> mResponse;
  RefPtr<FetchObserver> mFetchObserver;
  RefPtr<AbortSignalImpl> mSignalImpl;
  const bool mMozErrors;

  nsCOMPtr<nsILoadGroup> mLoadGroup;

  NS_DECL_OWNINGTHREAD
 public:
  MainThreadFetchResolver(Promise* aPromise, FetchObserver* aObserver,
                          AbortSignalImpl* aSignalImpl, bool aMozErrors)
      : mPromise(aPromise),
        mFetchObserver(aObserver),
        mSignalImpl(aSignalImpl),
        mMozErrors(aMozErrors) {}

  void OnResponseAvailableInternal(InternalResponse* aResponse) override;

  void SetLoadGroup(nsILoadGroup* aLoadGroup) { mLoadGroup = aLoadGroup; }

  void OnResponseEnd(FetchDriverObserver::EndReason aReason) override {
    if (aReason == eAborted) {
      mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    }

    mFetchObserver = nullptr;

    FlushConsoleReport();
  }

  bool NeedOnDataAvailable() override;

  void OnDataAvailable() override;

 private:
  ~MainThreadFetchResolver();

  void FlushConsoleReport() override {
    mReporter->FlushConsoleReports(mLoadGroup);
  }
};

class MainThreadFetchRunnable : public Runnable {
  RefPtr<WorkerFetchResolver> mResolver;
  const ClientInfo mClientInfo;
  const Maybe<ServiceWorkerDescriptor> mController;
  nsCOMPtr<nsICSPEventListener> mCSPEventListener;
  RefPtr<InternalRequest> mRequest;
  UniquePtr<SerializedStackHolder> mOriginStack;

 public:
  MainThreadFetchRunnable(WorkerFetchResolver* aResolver,
                          const ClientInfo& aClientInfo,
                          const Maybe<ServiceWorkerDescriptor>& aController,
                          nsICSPEventListener* aCSPEventListener,
                          InternalRequest* aRequest,
                          UniquePtr<SerializedStackHolder>&& aOriginStack)
      : Runnable("dom::MainThreadFetchRunnable"),
        mResolver(aResolver),
        mClientInfo(aClientInfo),
        mController(aController),
        mCSPEventListener(aCSPEventListener),
        mRequest(aRequest),
        mOriginStack(std::move(aOriginStack)) {
    MOZ_ASSERT(mResolver);
  }

  NS_IMETHOD
  Run() override {
    AssertIsOnMainThread();
    RefPtr<FetchDriver> fetch;
    RefPtr<PromiseWorkerProxy> proxy = mResolver->PromiseProxy();

    {
      // Acquire the proxy mutex while getting data from the WorkerPrivate...
      MutexAutoLock lock(proxy->Lock());
      if (proxy->CleanedUp()) {
        NS_WARNING("Aborting Fetch because worker already shut down");
        return NS_OK;
      }

      WorkerPrivate* workerPrivate = proxy->GetWorkerPrivate();
      MOZ_ASSERT(workerPrivate);
      nsCOMPtr<nsIPrincipal> principal = workerPrivate->GetPrincipal();
      MOZ_ASSERT(principal);
      nsCOMPtr<nsILoadGroup> loadGroup = workerPrivate->GetLoadGroup();
      MOZ_ASSERT(loadGroup);
      // We don't track if a worker is spawned from a tracking script for now,
      // so pass false as the last argument to FetchDriver().
      fetch = new FetchDriver(mRequest, principal, loadGroup,
                              workerPrivate->MainThreadEventTarget(),
                              workerPrivate->CookieJarSettings(),
                              workerPrivate->GetPerformanceStorage(), false);
      nsAutoCString spec;
      if (proxy->GetWorkerPrivate()->GetBaseURI()) {
        proxy->GetWorkerPrivate()->GetBaseURI()->GetAsciiSpec(spec);
      }
      fetch->SetWorkerScript(spec);

      fetch->SetClientInfo(mClientInfo);
      fetch->SetController(mController);
      fetch->SetCSPEventListener(mCSPEventListener);
    }

    fetch->SetOriginStack(std::move(mOriginStack));

    RefPtr<AbortSignalImpl> signalImpl =
        mResolver->GetAbortSignalForMainThread();

    // ...but release it before calling Fetch, because mResolver's callback can
    // be called synchronously and they want the mutex, too.
    return fetch->Fetch(signalImpl, mResolver);
  }
};

already_AddRefed<Promise> FetchRequest(nsIGlobalObject* aGlobal,
                                       const RequestOrUSVString& aInput,
                                       const RequestInit& aInit,
                                       CallerType aCallerType,
                                       ErrorResult& aRv) {
  RefPtr<Promise> p = Promise::Create(aGlobal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  MOZ_ASSERT(aGlobal);

  // Double check that we have chrome privileges if the Request's content
  // policy type has been overridden.
  MOZ_ASSERT_IF(aInput.IsRequest() &&
                    aInput.GetAsRequest().IsContentPolicyTypeOverridden(),
                aCallerType == CallerType::System);

  AutoJSAPI jsapi;
  if (!jsapi.Init(aGlobal)) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> jsGlobal(cx, aGlobal->GetGlobalJSObject());
  GlobalObject global(cx, jsGlobal);

  RefPtr<Request> request = Request::Constructor(global, aInput, aInit, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<InternalRequest> r = request->GetInternalRequest();
  RefPtr<AbortSignalImpl> signalImpl = request->GetSignalImpl();

  if (signalImpl && signalImpl->Aborted()) {
    // Already aborted signal rejects immediately.
    aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
    return nullptr;
  }

  RefPtr<FetchObserver> observer;
  if (aInit.mObserve.WasPassed()) {
    observer = new FetchObserver(aGlobal, signalImpl);
    aInit.mObserve.Value().HandleEvent(*observer);
  }

  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal);
    nsCOMPtr<Document> doc;
    nsCOMPtr<nsILoadGroup> loadGroup;
    nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
    nsIPrincipal* principal;
    bool isTrackingFetch = false;
    if (window) {
      doc = window->GetExtantDoc();
      if (!doc) {
        aRv.Throw(NS_ERROR_FAILURE);
        return nullptr;
      }
      principal = doc->NodePrincipal();
      loadGroup = doc->GetDocumentLoadGroup();
      cookieJarSettings = doc->CookieJarSettings();

      isTrackingFetch = doc->IsScriptTracking(cx);
    } else {
      principal = aGlobal->PrincipalOrNull();
      if (NS_WARN_IF(!principal)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return nullptr;
      }

      cookieJarSettings = mozilla::net::CookieJarSettings::Create();
    }

    if (!loadGroup) {
      nsresult rv = NS_NewLoadGroup(getter_AddRefs(loadGroup), principal);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        aRv.Throw(rv);
        return nullptr;
      }
    }

    RefPtr<MainThreadFetchResolver> resolver = new MainThreadFetchResolver(
        p, observer, signalImpl, request->MozErrors());
    RefPtr<FetchDriver> fetch = new FetchDriver(
        r, principal, loadGroup, aGlobal->EventTargetFor(TaskCategory::Other),
        cookieJarSettings, nullptr,  // PerformanceStorage
        isTrackingFetch);
    fetch->SetDocument(doc);
    resolver->SetLoadGroup(loadGroup);
    aRv = fetch->Fetch(signalImpl, resolver);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  } else {
    WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);

    if (worker->IsServiceWorker()) {
      r->SetSkipServiceWorker();
    }

    RefPtr<WorkerFetchResolver> resolver =
        WorkerFetchResolver::Create(worker, p, signalImpl, observer);
    if (!resolver) {
      NS_WARNING("Could not keep the worker alive.");
      aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
      return nullptr;
    }

    Maybe<ClientInfo> clientInfo(worker->GlobalScope()->GetClientInfo());
    if (clientInfo.isNothing()) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return nullptr;
    }

    UniquePtr<SerializedStackHolder> stack;
    if (worker->IsWatchedByDevtools()) {
      stack = GetCurrentStackForNetMonitor(cx);
    }

    RefPtr<MainThreadFetchRunnable> run = new MainThreadFetchRunnable(
        resolver, clientInfo.ref(), worker->GlobalScope()->GetController(),
        worker->CSPEventListener(), r, std::move(stack));
    worker->DispatchToMainThread(run.forget());
  }

  return p.forget();
}

class ResolveFetchPromise : public Runnable {
 public:
  ResolveFetchPromise(Promise* aPromise, Response* aResponse)
      : Runnable("ResolveFetchPromise"),
        mPromise(aPromise),
        mResponse(aResponse) {}

  NS_IMETHOD Run() {
    mPromise->MaybeResolve(mResponse);
    return NS_OK;
  }
  RefPtr<Promise> mPromise;
  RefPtr<Response> mResponse;
};

void MainThreadFetchResolver::OnResponseAvailableInternal(
    InternalResponse* aResponse) {
  NS_ASSERT_OWNINGTHREAD(MainThreadFetchResolver);
  AssertIsOnMainThread();

  if (aResponse->Type() != ResponseType::Error) {
    if (mFetchObserver) {
      mFetchObserver->SetState(FetchState::Complete);
    }

    nsCOMPtr<nsIGlobalObject> go = mPromise->GetParentObject();
    mResponse = new Response(go, aResponse, mSignalImpl);
    nsCOMPtr<nsPIDOMWindowInner> inner = do_QueryInterface(go);
    BrowsingContext* bc = inner ? inner->GetBrowsingContext() : nullptr;
    bc = bc ? bc->Top() : nullptr;
    if (bc && bc->IsLoading()) {
      bc->AddDeprioritizedLoadRunner(
          new ResolveFetchPromise(mPromise, mResponse));
    } else {
      mPromise->MaybeResolve(mResponse);
    }
  } else {
    if (mFetchObserver) {
      mFetchObserver->SetState(FetchState::Errored);
    }

    if (mMozErrors) {
      mPromise->MaybeReject(aResponse->GetErrorCode());
      return;
    }

    mPromise->MaybeRejectWithTypeError<MSG_FETCH_FAILED>();
  }
}

bool MainThreadFetchResolver::NeedOnDataAvailable() {
  NS_ASSERT_OWNINGTHREAD(MainThreadFetchResolver);
  return !!mFetchObserver;
}

void MainThreadFetchResolver::OnDataAvailable() {
  NS_ASSERT_OWNINGTHREAD(MainThreadFetchResolver);
  AssertIsOnMainThread();

  if (!mFetchObserver) {
    return;
  }

  if (mFetchObserver->State() == FetchState::Requesting) {
    mFetchObserver->SetState(FetchState::Responding);
  }
}

MainThreadFetchResolver::~MainThreadFetchResolver() {
  NS_ASSERT_OWNINGTHREAD(MainThreadFetchResolver);
}

class WorkerFetchResponseRunnable final : public MainThreadWorkerRunnable {
  RefPtr<WorkerFetchResolver> mResolver;
  // Passed from main thread to worker thread after being initialized.
  RefPtr<InternalResponse> mInternalResponse;

 public:
  WorkerFetchResponseRunnable(WorkerPrivate* aWorkerPrivate,
                              WorkerFetchResolver* aResolver,
                              InternalResponse* aResponse)
      : MainThreadWorkerRunnable(aWorkerPrivate),
        mResolver(aResolver),
        mInternalResponse(aResponse) {
    MOZ_ASSERT(mResolver);
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    RefPtr<Promise> promise = mResolver->WorkerPromise(aWorkerPrivate);
    RefPtr<FetchObserver> fetchObserver =
        mResolver->GetFetchObserver(aWorkerPrivate);

    if (mInternalResponse->Type() != ResponseType::Error) {
      if (fetchObserver) {
        fetchObserver->SetState(FetchState::Complete);
      }

      RefPtr<nsIGlobalObject> global = aWorkerPrivate->GlobalScope();
      RefPtr<Response> response =
          new Response(global, mInternalResponse,
                       mResolver->GetAbortSignalForTargetThread());
      promise->MaybeResolve(response);
    } else {
      if (fetchObserver) {
        fetchObserver->SetState(FetchState::Errored);
      }

      promise->MaybeRejectWithTypeError<MSG_FETCH_FAILED>();
    }
    return true;
  }
};

class WorkerDataAvailableRunnable final : public MainThreadWorkerRunnable {
  RefPtr<WorkerFetchResolver> mResolver;

 public:
  WorkerDataAvailableRunnable(WorkerPrivate* aWorkerPrivate,
                              WorkerFetchResolver* aResolver)
      : MainThreadWorkerRunnable(aWorkerPrivate), mResolver(aResolver) {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    RefPtr<FetchObserver> fetchObserver =
        mResolver->GetFetchObserver(aWorkerPrivate);

    if (fetchObserver && fetchObserver->State() == FetchState::Requesting) {
      fetchObserver->SetState(FetchState::Responding);
    }

    return true;
  }
};

class WorkerFetchResponseEndBase {
 protected:
  RefPtr<WorkerFetchResolver> mResolver;

 public:
  explicit WorkerFetchResponseEndBase(WorkerFetchResolver* aResolver)
      : mResolver(aResolver) {
    MOZ_ASSERT(aResolver);
  }

  void WorkerRunInternal(WorkerPrivate* aWorkerPrivate) {
    mResolver->Shutdown(aWorkerPrivate);
  }
};

class WorkerFetchResponseEndRunnable final : public MainThreadWorkerRunnable,
                                             public WorkerFetchResponseEndBase {
  FetchDriverObserver::EndReason mReason;

 public:
  WorkerFetchResponseEndRunnable(WorkerPrivate* aWorkerPrivate,
                                 WorkerFetchResolver* aResolver,
                                 FetchDriverObserver::EndReason aReason)
      : MainThreadWorkerRunnable(aWorkerPrivate),
        WorkerFetchResponseEndBase(aResolver),
        mReason(aReason) {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    if (mResolver->IsShutdown(aWorkerPrivate)) {
      return true;
    }

    if (mReason == FetchDriverObserver::eAborted) {
      mResolver->WorkerPromise(aWorkerPrivate)
          ->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    }

    WorkerRunInternal(aWorkerPrivate);
    return true;
  }

  nsresult Cancel() override {
    // Execute Run anyway to make sure we cleanup our promise proxy to avoid
    // leaking the worker thread
    Run();
    return WorkerRunnable::Cancel();
  }
};

class WorkerFetchResponseEndControlRunnable final
    : public MainThreadWorkerControlRunnable,
      public WorkerFetchResponseEndBase {
 public:
  WorkerFetchResponseEndControlRunnable(WorkerPrivate* aWorkerPrivate,
                                        WorkerFetchResolver* aResolver)
      : MainThreadWorkerControlRunnable(aWorkerPrivate),
        WorkerFetchResponseEndBase(aResolver) {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    WorkerRunInternal(aWorkerPrivate);
    return true;
  }

  // Control runnable cancel already calls Run().
};

void WorkerFetchResolver::OnResponseAvailableInternal(
    InternalResponse* aResponse) {
  AssertIsOnMainThread();

  MutexAutoLock lock(mPromiseProxy->Lock());
  if (mPromiseProxy->CleanedUp()) {
    return;
  }

  RefPtr<WorkerFetchResponseRunnable> r = new WorkerFetchResponseRunnable(
      mPromiseProxy->GetWorkerPrivate(), this, aResponse);

  if (!r->Dispatch()) {
    NS_WARNING("Could not dispatch fetch response");
  }
}

bool WorkerFetchResolver::NeedOnDataAvailable() {
  AssertIsOnMainThread();
  MutexAutoLock lock(mPromiseProxy->Lock());
  return !!mFetchObserver;
}

void WorkerFetchResolver::OnDataAvailable() {
  AssertIsOnMainThread();

  MutexAutoLock lock(mPromiseProxy->Lock());
  if (mPromiseProxy->CleanedUp()) {
    return;
  }

  RefPtr<WorkerDataAvailableRunnable> r =
      new WorkerDataAvailableRunnable(mPromiseProxy->GetWorkerPrivate(), this);
  Unused << r->Dispatch();
}

void WorkerFetchResolver::OnResponseEnd(
    FetchDriverObserver::EndReason aReason) {
  AssertIsOnMainThread();
  MutexAutoLock lock(mPromiseProxy->Lock());
  if (mPromiseProxy->CleanedUp()) {
    return;
  }

  FlushConsoleReport();

  RefPtr<WorkerFetchResponseEndRunnable> r = new WorkerFetchResponseEndRunnable(
      mPromiseProxy->GetWorkerPrivate(), this, aReason);

  if (!r->Dispatch()) {
    RefPtr<WorkerFetchResponseEndControlRunnable> cr =
        new WorkerFetchResponseEndControlRunnable(
            mPromiseProxy->GetWorkerPrivate(), this);
    // This can fail if the worker thread is canceled or killed causing
    // the PromiseWorkerProxy to give up its WorkerRef immediately,
    // allowing the worker thread to become Dead.
    if (!cr->Dispatch()) {
      NS_WARNING("Failed to dispatch WorkerFetchResponseEndControlRunnable");
    }
  }
}

void WorkerFetchResolver::FlushConsoleReport() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mPromiseProxy);

  if (!mReporter) {
    return;
  }

  WorkerPrivate* worker = mPromiseProxy->GetWorkerPrivate();
  if (!worker) {
    mReporter->FlushReportsToConsole(0);
    return;
  }

  if (worker->IsServiceWorker()) {
    // Flush to service worker
    mReporter->FlushReportsToConsoleForServiceWorkerScope(
        worker->ServiceWorkerScope());
    return;
  }

  if (worker->IsSharedWorker()) {
    // Flush to shared worker
    worker->GetRemoteWorkerController()->FlushReportsOnMainThread(mReporter);
    return;
  }

  // Flush to dedicated worker
  mReporter->FlushConsoleReports(worker->GetLoadGroup());
}

nsresult ExtractByteStreamFromBody(const fetch::OwningBodyInit& aBodyInit,
                                   nsIInputStream** aStream,
                                   nsCString& aContentTypeWithCharset,
                                   uint64_t& aContentLength) {
  MOZ_ASSERT(aStream);
  nsAutoCString charset;
  aContentTypeWithCharset.SetIsVoid(true);

  if (aBodyInit.IsArrayBuffer()) {
    BodyExtractor<const ArrayBuffer> body(&aBodyInit.GetAsArrayBuffer());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsArrayBufferView()) {
    BodyExtractor<const ArrayBufferView> body(
        &aBodyInit.GetAsArrayBufferView());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsBlob()) {
    Blob& blob = aBodyInit.GetAsBlob();
    BodyExtractor<const Blob> body(&blob);
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsFormData()) {
    FormData& formData = aBodyInit.GetAsFormData();
    BodyExtractor<const FormData> body(&formData);
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsUSVString()) {
    BodyExtractor<const nsAString> body(&aBodyInit.GetAsUSVString());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsURLSearchParams()) {
    URLSearchParams& usp = aBodyInit.GetAsURLSearchParams();
    BodyExtractor<const URLSearchParams> body(&usp);
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  MOZ_ASSERT_UNREACHABLE("Should never reach here");
  return NS_ERROR_FAILURE;
}

nsresult ExtractByteStreamFromBody(const fetch::BodyInit& aBodyInit,
                                   nsIInputStream** aStream,
                                   nsCString& aContentTypeWithCharset,
                                   uint64_t& aContentLength) {
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(!*aStream);

  nsAutoCString charset;
  aContentTypeWithCharset.SetIsVoid(true);

  if (aBodyInit.IsArrayBuffer()) {
    BodyExtractor<const ArrayBuffer> body(&aBodyInit.GetAsArrayBuffer());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsArrayBufferView()) {
    BodyExtractor<const ArrayBufferView> body(
        &aBodyInit.GetAsArrayBufferView());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsBlob()) {
    BodyExtractor<const Blob> body(&aBodyInit.GetAsBlob());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsFormData()) {
    BodyExtractor<const FormData> body(&aBodyInit.GetAsFormData());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsUSVString()) {
    BodyExtractor<const nsAString> body(&aBodyInit.GetAsUSVString());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsURLSearchParams()) {
    BodyExtractor<const URLSearchParams> body(
        &aBodyInit.GetAsURLSearchParams());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  MOZ_ASSERT_UNREACHABLE("Should never reach here");
  return NS_ERROR_FAILURE;
}

nsresult ExtractByteStreamFromBody(const fetch::ResponseBodyInit& aBodyInit,
                                   nsIInputStream** aStream,
                                   nsCString& aContentTypeWithCharset,
                                   uint64_t& aContentLength) {
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(!*aStream);

  // ReadableStreams should be handled by
  // BodyExtractorReadableStream::GetAsStream.
  MOZ_ASSERT(!aBodyInit.IsReadableStream());

  nsAutoCString charset;
  aContentTypeWithCharset.SetIsVoid(true);

  if (aBodyInit.IsArrayBuffer()) {
    BodyExtractor<const ArrayBuffer> body(&aBodyInit.GetAsArrayBuffer());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsArrayBufferView()) {
    BodyExtractor<const ArrayBufferView> body(
        &aBodyInit.GetAsArrayBufferView());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsBlob()) {
    BodyExtractor<const Blob> body(&aBodyInit.GetAsBlob());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsFormData()) {
    BodyExtractor<const FormData> body(&aBodyInit.GetAsFormData());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsUSVString()) {
    BodyExtractor<const nsAString> body(&aBodyInit.GetAsUSVString());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsURLSearchParams()) {
    BodyExtractor<const URLSearchParams> body(
        &aBodyInit.GetAsURLSearchParams());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  MOZ_ASSERT_UNREACHABLE("Should never reach here");
  return NS_ERROR_FAILURE;
}

template <class Derived>
FetchBody<Derived>::FetchBody(nsIGlobalObject* aOwner)
    : mOwner(aOwner),
      mWorkerPrivate(nullptr),
      mReadableStreamBody(nullptr),
      mReadableStreamReader(nullptr),
      mBodyUsed(false) {
  MOZ_ASSERT(aOwner);

  if (!NS_IsMainThread()) {
    mWorkerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(mWorkerPrivate);
    mMainThreadEventTarget = mWorkerPrivate->MainThreadEventTarget();
  } else {
    mMainThreadEventTarget = aOwner->EventTargetFor(TaskCategory::Other);
  }

  MOZ_ASSERT(mMainThreadEventTarget);
}

template FetchBody<Request>::FetchBody(nsIGlobalObject* aOwner);

template FetchBody<Response>::FetchBody(nsIGlobalObject* aOwner);

template <class Derived>
FetchBody<Derived>::~FetchBody() {
  Unfollow();
}

template FetchBody<Request>::~FetchBody();

template FetchBody<Response>::~FetchBody();

template <class Derived>
bool FetchBody<Derived>::GetBodyUsed(ErrorResult& aRv) const {
  if (mBodyUsed) {
    return true;
  }

  // If this stream is disturbed, return true.
  if (mReadableStreamBody) {
    aRv.MightThrowJSException();

    AutoJSAPI jsapi;
    if (!jsapi.Init(mOwner)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return true;
    }

    JSContext* cx = jsapi.cx();
    JS::Rooted<JSObject*> body(cx, mReadableStreamBody);
    bool disturbed;
    if (!JS::ReadableStreamIsDisturbed(cx, body, &disturbed)) {
      aRv.StealExceptionFromJSContext(cx);
      return false;
    }

    return disturbed;
  }

  return false;
}

template bool FetchBody<Request>::GetBodyUsed(ErrorResult&) const;

template bool FetchBody<Response>::GetBodyUsed(ErrorResult&) const;

template <class Derived>
bool FetchBody<Derived>::CheckBodyUsed() const {
  IgnoredErrorResult result;
  bool bodyUsed = GetBodyUsed(result);
  if (result.Failed()) {
    // Ignore the error.
    return true;
  }
  return bodyUsed;
}

template <class Derived>
void FetchBody<Derived>::SetBodyUsed(JSContext* aCx, ErrorResult& aRv) {
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(mOwner->EventTargetFor(TaskCategory::Other)->IsOnCurrentThread());

  if (mBodyUsed) {
    return;
  }

  mBodyUsed = true;

  // If we already have a ReadableStreamBody and it has been created by DOM, we
  // have to lock it now because it can have been shared with other objects.
  if (mReadableStreamBody) {
    aRv.MightThrowJSException();

    JSAutoRealm ar(aCx, mOwner->GetGlobalJSObject());

    JS::Rooted<JSObject*> readableStreamObj(aCx, mReadableStreamBody);

    JS::ReadableStreamMode mode;
    if (!JS::ReadableStreamGetMode(aCx, readableStreamObj, &mode)) {
      aRv.StealExceptionFromJSContext(aCx);
      return;
    }

    if (mode == JS::ReadableStreamMode::ExternalSource) {
      LockStream(aCx, readableStreamObj, aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return;
      }
    } else {
      // If this is not a native ReadableStream, let's activate the
      // FetchStreamReader.
      MOZ_ASSERT(mFetchStreamReader);
      JS::Rooted<JSObject*> reader(aCx);
      mFetchStreamReader->StartConsuming(aCx, readableStreamObj, &reader, aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return;
      }

      mReadableStreamReader = reader;
    }
  }
}

template void FetchBody<Request>::SetBodyUsed(JSContext* aCx, ErrorResult& aRv);

template void FetchBody<Response>::SetBodyUsed(JSContext* aCx,
                                               ErrorResult& aRv);

template <class Derived>
already_AddRefed<Promise> FetchBody<Derived>::ConsumeBody(
    JSContext* aCx, BodyConsumer::ConsumeType aType, ErrorResult& aRv) {
  aRv.MightThrowJSException();

  RefPtr<AbortSignalImpl> signalImpl = DerivedClass()->GetSignalImpl();
  if (signalImpl && signalImpl->Aborted()) {
    aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
    return nullptr;
  }

  bool bodyUsed = GetBodyUsed(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  if (bodyUsed) {
    aRv.ThrowTypeError<MSG_FETCH_BODY_CONSUMED_ERROR>();
    return nullptr;
  }

  // Null bodies are a special-case in the fetch spec.  The Body mix-in can only
  // be "disturbed" or "locked" if its associated "body" is non-null.
  // Additionally, the Body min-in's "consume body" algorithm explicitly creates
  // a fresh empty ReadableStream object in step 2.  This means that `bodyUsed`
  // will never return true for a null body.
  //
  // To this end, we create a fresh (empty) body every time a request is made
  // and consume its body here, without marking this FetchBody consumed via
  // SetBodyUsed.
  nsCOMPtr<nsIInputStream> bodyStream;
  DerivedClass()->GetBody(getter_AddRefs(bodyStream));
  if (!bodyStream) {
    RefPtr<EmptyBody> emptyBody = EmptyBody::Create(
        DerivedClass()->GetParentObject(),
        DerivedClass()->GetPrincipalInfo().get(), signalImpl, mMimeType, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    return emptyBody->ConsumeBody(aCx, aType, aRv);
  }

  SetBodyUsed(aCx, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = DerivedClass()->GetParentObject();

  MutableBlobStorage::MutableBlobStorageType blobStorageType =
      MutableBlobStorage::eOnlyInMemory;
  const mozilla::UniquePtr<mozilla::ipc::PrincipalInfo>& principalInfo =
      DerivedClass()->GetPrincipalInfo();
  // We support temporary file for blobs only if the principal is known and
  // it's system or content not in private Browsing.
  if (principalInfo &&
      (principalInfo->type() ==
           mozilla::ipc::PrincipalInfo::TSystemPrincipalInfo ||
       (principalInfo->type() ==
            mozilla::ipc::PrincipalInfo::TContentPrincipalInfo &&
        principalInfo->get_ContentPrincipalInfo().attrs().mPrivateBrowsingId ==
            0))) {
    blobStorageType = MutableBlobStorage::eCouldBeInTemporaryFile;
  }

  RefPtr<Promise> promise = BodyConsumer::Create(
      global, mMainThreadEventTarget, bodyStream, signalImpl, aType,
      BodyBlobURISpec(), BodyLocalPath(), MimeType(), blobStorageType, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return promise.forget();
}

template already_AddRefed<Promise> FetchBody<Request>::ConsumeBody(
    JSContext* aCx, BodyConsumer::ConsumeType aType, ErrorResult& aRv);

template already_AddRefed<Promise> FetchBody<Response>::ConsumeBody(
    JSContext* aCx, BodyConsumer::ConsumeType aType, ErrorResult& aRv);

template already_AddRefed<Promise> FetchBody<EmptyBody>::ConsumeBody(
    JSContext* aCx, BodyConsumer::ConsumeType aType, ErrorResult& aRv);

template <class Derived>
void FetchBody<Derived>::SetMimeType() {
  // Extract mime type.
  ErrorResult result;
  nsCString contentTypeValues;
  MOZ_ASSERT(DerivedClass()->GetInternalHeaders());
  DerivedClass()->GetInternalHeaders()->Get(NS_LITERAL_CSTRING("Content-Type"),
                                            contentTypeValues, result);
  MOZ_ALWAYS_TRUE(!result.Failed());

  // HTTP ABNF states Content-Type may have only one value.
  // This is from the "parse a header value" of the fetch spec.
  if (!contentTypeValues.IsVoid() && contentTypeValues.Find(",") == -1) {
    // Convert from a bytestring to a UTF8 CString.
    CopyLatin1toUTF8(contentTypeValues, mMimeType);
    ToLowerCase(mMimeType);
  }
}

template void FetchBody<Request>::SetMimeType();

template void FetchBody<Response>::SetMimeType();

template <class Derived>
void FetchBody<Derived>::OverrideMimeType(const nsACString& aMimeType) {
  mMimeType = aMimeType;
}

template <class Derived>
const nsACString& FetchBody<Derived>::BodyBlobURISpec() const {
  return DerivedClass()->BodyBlobURISpec();
}

template const nsACString& FetchBody<Request>::BodyBlobURISpec() const;

template const nsACString& FetchBody<Response>::BodyBlobURISpec() const;

template const nsACString& FetchBody<EmptyBody>::BodyBlobURISpec() const;

template <class Derived>
const nsAString& FetchBody<Derived>::BodyLocalPath() const {
  return DerivedClass()->BodyLocalPath();
}

template const nsAString& FetchBody<Request>::BodyLocalPath() const;

template const nsAString& FetchBody<Response>::BodyLocalPath() const;

template const nsAString& FetchBody<EmptyBody>::BodyLocalPath() const;

template <class Derived>
void FetchBody<Derived>::SetReadableStreamBody(JSContext* aCx,
                                               JSObject* aBody) {
  MOZ_ASSERT(!mReadableStreamBody);
  MOZ_ASSERT(aBody);
  mReadableStreamBody = aBody;

  RefPtr<AbortSignalImpl> signalImpl = DerivedClass()->GetSignalImpl();
  if (!signalImpl) {
    return;
  }

  bool aborted = signalImpl->Aborted();
  if (aborted) {
    JS::Rooted<JSObject*> body(aCx, mReadableStreamBody);
    IgnoredErrorResult result;
    AbortStream(aCx, body, result);
    if (NS_WARN_IF(result.Failed())) {
      return;
    }
  } else if (!IsFollowing()) {
    Follow(signalImpl);
  }
}

template void FetchBody<Request>::SetReadableStreamBody(JSContext* aCx,
                                                        JSObject* aBody);

template void FetchBody<Response>::SetReadableStreamBody(JSContext* aCx,
                                                         JSObject* aBody);

template <class Derived>
void FetchBody<Derived>::GetBody(JSContext* aCx,
                                 JS::MutableHandle<JSObject*> aBodyOut,
                                 ErrorResult& aRv) {
  if (mReadableStreamBody) {
    aBodyOut.set(mReadableStreamBody);
    return;
  }

  nsCOMPtr<nsIInputStream> inputStream;
  DerivedClass()->GetBody(getter_AddRefs(inputStream));

  if (!inputStream) {
    aBodyOut.set(nullptr);
    return;
  }

  BodyStream::Create(aCx, this, DerivedClass()->GetParentObject(), inputStream,
                     aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  MOZ_ASSERT(mReadableStreamBody);

  JS::Rooted<JSObject*> body(aCx, mReadableStreamBody);

  // If the body has been already consumed, we lock the stream.
  bool bodyUsed = GetBodyUsed(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  if (bodyUsed) {
    LockStream(aCx, body, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  }

  RefPtr<AbortSignalImpl> signalImpl = DerivedClass()->GetSignalImpl();
  if (signalImpl) {
    if (signalImpl->Aborted()) {
      AbortStream(aCx, body, aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return;
      }
    } else if (!IsFollowing()) {
      Follow(signalImpl);
    }
  }

  aBodyOut.set(mReadableStreamBody);
}

template void FetchBody<Request>::GetBody(JSContext* aCx,
                                          JS::MutableHandle<JSObject*> aMessage,
                                          ErrorResult& aRv);

template void FetchBody<Response>::GetBody(
    JSContext* aCx, JS::MutableHandle<JSObject*> aMessage, ErrorResult& aRv);

template <class Derived>
void FetchBody<Derived>::LockStream(JSContext* aCx, JS::HandleObject aStream,
                                    ErrorResult& aRv) {
  aRv.MightThrowJSException();

#if DEBUG
  JS::ReadableStreamMode streamMode;
  if (!JS::ReadableStreamGetMode(aCx, aStream, &streamMode)) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }
  MOZ_ASSERT(streamMode == JS::ReadableStreamMode::ExternalSource);
#endif  // DEBUG

  // This is native stream, creating a reader will not execute any JS code.
  JS::Rooted<JSObject*> reader(
      aCx, JS::ReadableStreamGetReader(aCx, aStream,
                                       JS::ReadableStreamReaderMode::Default));
  if (!reader) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }

  mReadableStreamReader = reader;
}

template void FetchBody<Request>::LockStream(JSContext* aCx,
                                             JS::HandleObject aStream,
                                             ErrorResult& aRv);

template void FetchBody<Response>::LockStream(JSContext* aCx,
                                              JS::HandleObject aStream,
                                              ErrorResult& aRv);

template <class Derived>
void FetchBody<Derived>::MaybeTeeReadableStreamBody(
    JSContext* aCx, JS::MutableHandle<JSObject*> aBodyOut,
    FetchStreamReader** aStreamReader, nsIInputStream** aInputStream,
    ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(aStreamReader);
  MOZ_DIAGNOSTIC_ASSERT(aInputStream);
  MOZ_DIAGNOSTIC_ASSERT(!CheckBodyUsed());

  aBodyOut.set(nullptr);
  *aStreamReader = nullptr;
  *aInputStream = nullptr;

  if (!mReadableStreamBody) {
    return;
  }

  aRv.MightThrowJSException();

  JSAutoRealm ar(aCx, mOwner->GetGlobalJSObject());

  JS::Rooted<JSObject*> stream(aCx, mReadableStreamBody);

  // If this is a ReadableStream with an external source, this has been
  // generated by a Fetch. In this case, Fetch will be able to recreate it
  // again when GetBody() is called.
  JS::ReadableStreamMode streamMode;
  if (!JS::ReadableStreamGetMode(aCx, stream, &streamMode)) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }
  if (streamMode == JS::ReadableStreamMode::ExternalSource) {
    aBodyOut.set(nullptr);
    return;
  }

  JS::Rooted<JSObject*> branch1(aCx);
  JS::Rooted<JSObject*> branch2(aCx);

  if (!JS::ReadableStreamTee(aCx, stream, &branch1, &branch2)) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }

  mReadableStreamBody = branch1;
  aBodyOut.set(branch2);

  aRv = FetchStreamReader::Create(aCx, mOwner, aStreamReader, aInputStream);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

template void FetchBody<Request>::MaybeTeeReadableStreamBody(
    JSContext* aCx, JS::MutableHandle<JSObject*> aMessage,
    FetchStreamReader** aStreamReader, nsIInputStream** aInputStream,
    ErrorResult& aRv);

template void FetchBody<Response>::MaybeTeeReadableStreamBody(
    JSContext* aCx, JS::MutableHandle<JSObject*> aMessage,
    FetchStreamReader** aStreamReader, nsIInputStream** aInputStream,
    ErrorResult& aRv);

template <class Derived>
void FetchBody<Derived>::Abort() {
  if (!mReadableStreamBody) {
    return;
  }

  AutoJSAPI jsapi;
  if (!jsapi.Init(mOwner)) {
    return;
  }

  JSContext* cx = jsapi.cx();

  JS::Rooted<JSObject*> body(cx, mReadableStreamBody);
  IgnoredErrorResult result;
  AbortStream(cx, body, result);
}

template void FetchBody<Request>::Abort();

template void FetchBody<Response>::Abort();

}  // namespace dom
}  // namespace mozilla
