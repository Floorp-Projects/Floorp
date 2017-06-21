/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Fetch.h"
#include "FetchConsumer.h"

#include "nsIDocument.h"
#include "nsIGlobalObject.h"
#include "nsIStreamLoader.h"
#include "nsIThreadRetargetableRequest.h"

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
#include "mozilla/dom/BodyUtil.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/FetchDriver.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FormData.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/MutableBlobStreamListener.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/URLSearchParams.h"
#include "mozilla/dom/workers/ServiceWorkerManager.h"
#include "mozilla/Telemetry.h"

#include "BodyExtractor.h"
#include "FetchObserver.h"
#include "InternalRequest.h"
#include "InternalResponse.h"

#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"
#include "Workers.h"

namespace mozilla {
namespace dom {

using namespace workers;

// This class helps the proxying of FetchSignal changes cross threads.
class FetchSignalProxy final : public FetchSignal::Follower
{
  // This is created and released on the main-thread.
  RefPtr<FetchSignal> mSignalMainThread;

  // The main-thread event target for runnable dispatching.
  nsCOMPtr<nsIEventTarget> mMainThreadEventTarget;

  // This value is used only for the creation of FetchSignal on the
  // main-thread. They are not updated.
  const bool mAborted;

  // This runnable propagates changes from the FetchSignal on workers to the
  // FetchSignal on main-thread.
  class FetchSignalProxyRunnable final : public Runnable
  {
    RefPtr<FetchSignalProxy> mProxy;

  public:
    explicit FetchSignalProxyRunnable(FetchSignalProxy* aProxy)
      : mProxy(aProxy)
    {}

    NS_IMETHOD
    Run() override
    {
      MOZ_ASSERT(NS_IsMainThread());
      FetchSignal* signal = mProxy->GetOrCreateSignalForMainThread();
      signal->Abort();
      return NS_OK;
    }
  };

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FetchSignalProxy)

  FetchSignalProxy(FetchSignal* aSignal, nsIEventTarget* aMainThreadEventTarget)
    : mMainThreadEventTarget(aMainThreadEventTarget)
    , mAborted(aSignal->Aborted())
  {
    MOZ_ASSERT(mMainThreadEventTarget);
    Follow(aSignal);
  }

  void
  Aborted() override
  {
    RefPtr<FetchSignalProxyRunnable> runnable =
      new FetchSignalProxyRunnable(this);
    mMainThreadEventTarget->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
  }

  FetchSignal*
  GetOrCreateSignalForMainThread()
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (!mSignalMainThread) {
      mSignalMainThread = new FetchSignal(mAborted);
    }
    return mSignalMainThread;
  }

  void
  Shutdown()
  {
    Unfollow();
  }

private:
  ~FetchSignalProxy()
  {
    NS_ProxyRelease(
      "FetchSignalProxy::mSignalMainThread",
      mMainThreadEventTarget, mSignalMainThread.forget());
  }
};

class WorkerFetchResolver final : public FetchDriverObserver
{
  friend class MainThreadFetchRunnable;
  friend class WorkerDataAvailableRunnable;
  friend class WorkerFetchResponseEndBase;
  friend class WorkerFetchResponseEndRunnable;
  friend class WorkerFetchResponseRunnable;

  RefPtr<PromiseWorkerProxy> mPromiseProxy;
  RefPtr<FetchSignalProxy> mSignalProxy;
  RefPtr<FetchObserver> mFetchObserver;

public:
  // Returns null if worker is shutting down.
  static already_AddRefed<WorkerFetchResolver>
  Create(workers::WorkerPrivate* aWorkerPrivate, Promise* aPromise,
         FetchSignal* aSignal, FetchObserver* aObserver)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    RefPtr<PromiseWorkerProxy> proxy =
      PromiseWorkerProxy::Create(aWorkerPrivate, aPromise);
    if (!proxy) {
      return nullptr;
    }

    RefPtr<FetchSignalProxy> signalProxy;
    if (aSignal) {
      signalProxy =
        new FetchSignalProxy(aSignal, aWorkerPrivate->MainThreadEventTarget());
    }

    RefPtr<WorkerFetchResolver> r =
      new WorkerFetchResolver(proxy, signalProxy, aObserver);
    return r.forget();
  }

  FetchSignal*
  GetFetchSignal()
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mSignalProxy) {
      return nullptr;
    }

    return mSignalProxy->GetOrCreateSignalForMainThread();
  }

  void
  OnResponseAvailableInternal(InternalResponse* aResponse) override;

  void
  OnResponseEnd(FetchDriverObserver::EndReason eReason) override;

  void
  OnDataAvailable() override;

private:
   WorkerFetchResolver(PromiseWorkerProxy* aProxy,
                       FetchSignalProxy* aSignalProxy,
                       FetchObserver* aObserver)
    : mPromiseProxy(aProxy)
    , mSignalProxy(aSignalProxy)
    , mFetchObserver(aObserver)
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mPromiseProxy);
  }

  ~WorkerFetchResolver()
  {}

  virtual void
  FlushConsoleReport() override;
};

class MainThreadFetchResolver final : public FetchDriverObserver
{
  RefPtr<Promise> mPromise;
  RefPtr<Response> mResponse;
  RefPtr<FetchObserver> mFetchObserver;

  nsCOMPtr<nsILoadGroup> mLoadGroup;

  NS_DECL_OWNINGTHREAD
public:
  MainThreadFetchResolver(Promise* aPromise, FetchObserver* aObserver)
    : mPromise(aPromise)
    , mFetchObserver(aObserver)
  {}

  void
  OnResponseAvailableInternal(InternalResponse* aResponse) override;

  void SetLoadGroup(nsILoadGroup* aLoadGroup)
  {
    mLoadGroup = aLoadGroup;
  }

  void
  OnResponseEnd(FetchDriverObserver::EndReason aReason) override
  {
    if (aReason == eAborted) {
      mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    }

    mFetchObserver = nullptr;

    FlushConsoleReport();
  }

  void
  OnDataAvailable() override;

private:
  ~MainThreadFetchResolver();

  void FlushConsoleReport() override
  {
    mReporter->FlushConsoleReports(mLoadGroup);
  }
};

class MainThreadFetchRunnable : public Runnable
{
  RefPtr<WorkerFetchResolver> mResolver;
  RefPtr<InternalRequest> mRequest;

public:
  MainThreadFetchRunnable(WorkerFetchResolver* aResolver,
                          InternalRequest* aRequest)
    : mResolver(aResolver)
    , mRequest(aRequest)
  {
    MOZ_ASSERT(mResolver);
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();
    RefPtr<FetchDriver> fetch;
    RefPtr<PromiseWorkerProxy> proxy = mResolver->mPromiseProxy;

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
                              workerPrivate->MainThreadEventTarget(), false);
      nsAutoCString spec;
      if (proxy->GetWorkerPrivate()->GetBaseURI()) {
        proxy->GetWorkerPrivate()->GetBaseURI()->GetAsciiSpec(spec);
      }
      fetch->SetWorkerScript(spec);
    }

    RefPtr<FetchSignal> signal = mResolver->GetFetchSignal();

    // ...but release it before calling Fetch, because mResolver's callback can
    // be called synchronously and they want the mutex, too.
    return fetch->Fetch(signal, mResolver);
  }
};

already_AddRefed<Promise>
FetchRequest(nsIGlobalObject* aGlobal, const RequestOrUSVString& aInput,
             const RequestInit& aInit, CallerType aCallerType, ErrorResult& aRv)
{
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
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<InternalRequest> r = request->GetInternalRequest();

  RefPtr<FetchSignal> signal;
  if (aInit.mSignal.WasPassed()) {
    signal = &aInit.mSignal.Value();
    // Let's FetchDriver to deal with an already aborted signal.
  }

  RefPtr<FetchObserver> observer;
  if (aInit.mObserve.WasPassed()) {
    observer = new FetchObserver(aGlobal, signal);
    aInit.mObserve.Value().HandleEvent(*observer);
  }

  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal);
    nsCOMPtr<nsIDocument> doc;
    nsCOMPtr<nsILoadGroup> loadGroup;
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

      nsAutoCString fileNameString;
      if (nsJSUtils::GetCallingLocation(cx, fileNameString)) {
        isTrackingFetch = doc->IsScriptTracking(fileNameString);
      }
    } else {
      principal = aGlobal->PrincipalOrNull();
      if (NS_WARN_IF(!principal)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return nullptr;
      }
      nsresult rv = NS_NewLoadGroup(getter_AddRefs(loadGroup), principal);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        aRv.Throw(rv);
        return nullptr;
      }
    }

    Telemetry::Accumulate(Telemetry::FETCH_IS_MAINTHREAD, 1);

    RefPtr<MainThreadFetchResolver> resolver =
      new MainThreadFetchResolver(p, observer);
    RefPtr<FetchDriver> fetch =
      new FetchDriver(r, principal, loadGroup,
                      aGlobal->EventTargetFor(TaskCategory::Other), isTrackingFetch);
    fetch->SetDocument(doc);
    resolver->SetLoadGroup(loadGroup);
    aRv = fetch->Fetch(signal, resolver);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  } else {
    WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);

    Telemetry::Accumulate(Telemetry::FETCH_IS_MAINTHREAD, 0);

    if (worker->IsServiceWorker()) {
      r->SetSkipServiceWorker();
    }

    RefPtr<WorkerFetchResolver> resolver =
      WorkerFetchResolver::Create(worker, p, signal, observer);
    if (!resolver) {
      NS_WARNING("Could not add WorkerFetchResolver workerHolder to worker");
      aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
      return nullptr;
    }

    RefPtr<MainThreadFetchRunnable> run =
      new MainThreadFetchRunnable(resolver, r);
    worker->DispatchToMainThread(run.forget());
  }

  return p.forget();
}

void
MainThreadFetchResolver::OnResponseAvailableInternal(InternalResponse* aResponse)
{
  NS_ASSERT_OWNINGTHREAD(MainThreadFetchResolver);
  AssertIsOnMainThread();

  if (aResponse->Type() != ResponseType::Error) {
    if (mFetchObserver) {
      mFetchObserver->SetState(FetchState::Complete);
    }

    nsCOMPtr<nsIGlobalObject> go = mPromise->GetParentObject();
    mResponse = new Response(go, aResponse);
    mPromise->MaybeResolve(mResponse);
  } else {
    if (mFetchObserver) {
      mFetchObserver->SetState(FetchState::Errored);
    }

    ErrorResult result;
    result.ThrowTypeError<MSG_FETCH_FAILED>();
    mPromise->MaybeReject(result);
  }
}

void
MainThreadFetchResolver::OnDataAvailable()
{
  NS_ASSERT_OWNINGTHREAD(MainThreadFetchResolver);
  AssertIsOnMainThread();

  if (!mFetchObserver) {
    return;
  }

  if (mFetchObserver->State() == FetchState::Requesting) {
    mFetchObserver->SetState(FetchState::Responding);
  }
}

MainThreadFetchResolver::~MainThreadFetchResolver()
{
  NS_ASSERT_OWNINGTHREAD(MainThreadFetchResolver);
}

class WorkerFetchResponseRunnable final : public MainThreadWorkerRunnable
{
  RefPtr<WorkerFetchResolver> mResolver;
  // Passed from main thread to worker thread after being initialized.
  RefPtr<InternalResponse> mInternalResponse;
public:
  WorkerFetchResponseRunnable(WorkerPrivate* aWorkerPrivate,
                              WorkerFetchResolver* aResolver,
                              InternalResponse* aResponse)
    : MainThreadWorkerRunnable(aWorkerPrivate)
    , mResolver(aResolver)
    , mInternalResponse(aResponse)
  {
     MOZ_ASSERT(mResolver);
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    RefPtr<Promise> promise = mResolver->mPromiseProxy->WorkerPromise();

    if (mInternalResponse->Type() != ResponseType::Error) {
      if (mResolver->mFetchObserver) {
        mResolver->mFetchObserver->SetState(FetchState::Complete);
      }

      RefPtr<nsIGlobalObject> global = aWorkerPrivate->GlobalScope();
      RefPtr<Response> response = new Response(global, mInternalResponse);
      promise->MaybeResolve(response);
    } else {
      if (mResolver->mFetchObserver) {
        mResolver->mFetchObserver->SetState(FetchState::Errored);
      }

      ErrorResult result;
      result.ThrowTypeError<MSG_FETCH_FAILED>();
      promise->MaybeReject(result);
    }
    return true;
  }
};

class WorkerDataAvailableRunnable final : public MainThreadWorkerRunnable
{
  RefPtr<WorkerFetchResolver> mResolver;
public:
  WorkerDataAvailableRunnable(WorkerPrivate* aWorkerPrivate,
                              WorkerFetchResolver* aResolver)
    : MainThreadWorkerRunnable(aWorkerPrivate)
    , mResolver(aResolver)
  {
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    if (mResolver->mFetchObserver &&
        mResolver->mFetchObserver->State() == FetchState::Requesting) {
      mResolver->mFetchObserver->SetState(FetchState::Responding);
    }

    return true;
  }
};

class WorkerFetchResponseEndBase
{
protected:
  RefPtr<WorkerFetchResolver> mResolver;

public:
  explicit WorkerFetchResponseEndBase(WorkerFetchResolver* aResolver)
    : mResolver(aResolver)
  {
    MOZ_ASSERT(aResolver);
  }

  void
  WorkerRunInternal(WorkerPrivate* aWorkerPrivate)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    mResolver->mPromiseProxy->CleanUp();

    mResolver->mFetchObserver = nullptr;

    if (mResolver->mSignalProxy) {
      mResolver->mSignalProxy->Shutdown();
      mResolver->mSignalProxy = nullptr;
    }
  }
};

class WorkerFetchResponseEndRunnable final : public MainThreadWorkerRunnable
                                           , public WorkerFetchResponseEndBase
{
  FetchDriverObserver::EndReason mReason;

public:
  WorkerFetchResponseEndRunnable(WorkerPrivate* aWorkerPrivate,
                                 WorkerFetchResolver* aResolver,
                                 FetchDriverObserver::EndReason aReason)
    : MainThreadWorkerRunnable(aWorkerPrivate)
    , WorkerFetchResponseEndBase(aResolver)
    , mReason(aReason)
  {
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    if (mReason == FetchDriverObserver::eAborted) {
      RefPtr<Promise> promise = mResolver->mPromiseProxy->WorkerPromise();
      promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    }

    WorkerRunInternal(aWorkerPrivate);
    return true;
  }

  nsresult
  Cancel() override
  {
    // Execute Run anyway to make sure we cleanup our promise proxy to avoid
    // leaking the worker thread
    Run();
    return WorkerRunnable::Cancel();
  }
};

class WorkerFetchResponseEndControlRunnable final : public MainThreadWorkerControlRunnable
                                                  , public WorkerFetchResponseEndBase
{
public:
  WorkerFetchResponseEndControlRunnable(WorkerPrivate* aWorkerPrivate,
                                        WorkerFetchResolver* aResolver)
    : MainThreadWorkerControlRunnable(aWorkerPrivate)
    , WorkerFetchResponseEndBase(aResolver)
  {
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    WorkerRunInternal(aWorkerPrivate);
    return true;
  }

  // Control runnable cancel already calls Run().
};

void
WorkerFetchResolver::OnResponseAvailableInternal(InternalResponse* aResponse)
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mPromiseProxy->Lock());
  if (mPromiseProxy->CleanedUp()) {
    return;
  }

  RefPtr<WorkerFetchResponseRunnable> r =
    new WorkerFetchResponseRunnable(mPromiseProxy->GetWorkerPrivate(), this,
                                    aResponse);

  if (!r->Dispatch()) {
    NS_WARNING("Could not dispatch fetch response");
  }
}

void
WorkerFetchResolver::OnDataAvailable()
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mPromiseProxy->Lock());
  if (mPromiseProxy->CleanedUp()) {
    return;
  }

  RefPtr<WorkerDataAvailableRunnable> r =
    new WorkerDataAvailableRunnable(mPromiseProxy->GetWorkerPrivate(), this);
  Unused << r->Dispatch();
}

void
WorkerFetchResolver::OnResponseEnd(FetchDriverObserver::EndReason aReason)
{
  AssertIsOnMainThread();
  MutexAutoLock lock(mPromiseProxy->Lock());
  if (mPromiseProxy->CleanedUp()) {
    return;
  }

  FlushConsoleReport();

  RefPtr<WorkerFetchResponseEndRunnable> r =
    new WorkerFetchResponseEndRunnable(mPromiseProxy->GetWorkerPrivate(),
                                       this, aReason);

  if (!r->Dispatch()) {
    RefPtr<WorkerFetchResponseEndControlRunnable> cr =
      new WorkerFetchResponseEndControlRunnable(mPromiseProxy->GetWorkerPrivate(),
                                                this);
    // This can fail if the worker thread is canceled or killed causing
    // the PromiseWorkerProxy to give up its WorkerHolder immediately,
    // allowing the worker thread to become Dead.
    if (!cr->Dispatch()) {
      NS_WARNING("Failed to dispatch WorkerFetchResponseEndControlRunnable");
    }
  }
}

void
WorkerFetchResolver::FlushConsoleReport()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mPromiseProxy);

  if(!mReporter) {
    return;
  }

  workers::WorkerPrivate* worker = mPromiseProxy->GetWorkerPrivate();
  if (!worker) {
    mReporter->FlushReportsToConsole(0);
    return;
  }

  if (worker->IsServiceWorker()) {
    // Flush to service worker
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (!swm) {
      mReporter->FlushReportsToConsole(0);
      return;
    }

    swm->FlushReportsToAllClients(worker->ServiceWorkerScope(), mReporter);
    return;
  }

  if (worker->IsSharedWorker()) {
    // Flush to shared worker
    worker->FlushReportsToSharedWorkers(mReporter);
    return;
  }

  // Flush to dedicated worker
  mReporter->FlushConsoleReports(worker->GetLoadGroup());
}

nsresult
ExtractByteStreamFromBody(const fetch::OwningBodyInit& aBodyInit,
                          nsIInputStream** aStream,
                          nsCString& aContentTypeWithCharset,
                          uint64_t& aContentLength)
{
  MOZ_ASSERT(aStream);
  nsAutoCString charset;
  aContentTypeWithCharset.SetIsVoid(true);

  if (aBodyInit.IsArrayBuffer()) {
    BodyExtractor<const ArrayBuffer> body(&aBodyInit.GetAsArrayBuffer());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsArrayBufferView()) {
    BodyExtractor<const ArrayBufferView> body(&aBodyInit.GetAsArrayBufferView());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsBlob()) {
    Blob& blob = aBodyInit.GetAsBlob();
    BodyExtractor<nsIXHRSendable> body(&blob);
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsFormData()) {
    FormData& formData = aBodyInit.GetAsFormData();
    BodyExtractor<nsIXHRSendable> body(&formData);
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
    BodyExtractor<nsIXHRSendable> body(&usp);
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  NS_NOTREACHED("Should never reach here");
  return NS_ERROR_FAILURE;
}

nsresult
ExtractByteStreamFromBody(const fetch::BodyInit& aBodyInit,
                          nsIInputStream** aStream,
                          nsCString& aContentTypeWithCharset,
                          uint64_t& aContentLength)
{
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
    BodyExtractor<const ArrayBufferView> body(&aBodyInit.GetAsArrayBufferView());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsBlob()) {
    BodyExtractor<nsIXHRSendable> body(&aBodyInit.GetAsBlob());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsFormData()) {
    BodyExtractor<nsIXHRSendable> body(&aBodyInit.GetAsFormData());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsUSVString()) {
    BodyExtractor<const nsAString> body(&aBodyInit.GetAsUSVString());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  if (aBodyInit.IsURLSearchParams()) {
    BodyExtractor<nsIXHRSendable> body(&aBodyInit.GetAsURLSearchParams());
    return body.GetAsStream(aStream, &aContentLength, aContentTypeWithCharset,
                            charset);
  }

  NS_NOTREACHED("Should never reach here");
  return NS_ERROR_FAILURE;
}

template <class Derived>
FetchBody<Derived>::FetchBody(nsIGlobalObject* aOwner)
  : mOwner(aOwner)
  , mWorkerPrivate(nullptr)
  , mBodyUsed(false)
{
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

template
FetchBody<Request>::FetchBody(nsIGlobalObject* aOwner);

template
FetchBody<Response>::FetchBody(nsIGlobalObject* aOwner);

template <class Derived>
FetchBody<Derived>::~FetchBody()
{
}

template <class Derived>
already_AddRefed<Promise>
FetchBody<Derived>::ConsumeBody(FetchConsumeType aType, ErrorResult& aRv)
{
  if (BodyUsed()) {
    aRv.ThrowTypeError<MSG_FETCH_BODY_CONSUMED_ERROR>();
    return nullptr;
  }

  SetBodyUsed();

  RefPtr<Promise> promise =
    FetchBodyConsumer<Derived>::Create(DerivedClass()->GetParentObject(),
                                       mMainThreadEventTarget, this, aType,
                                       aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return promise.forget();
}

template
already_AddRefed<Promise>
FetchBody<Request>::ConsumeBody(FetchConsumeType aType, ErrorResult& aRv);

template
already_AddRefed<Promise>
FetchBody<Response>::ConsumeBody(FetchConsumeType aType, ErrorResult& aRv);

template <class Derived>
void
FetchBody<Derived>::SetMimeType()
{
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
    mMimeType = contentTypeValues;
    ToLowerCase(mMimeType);
  }
}

template
void
FetchBody<Request>::SetMimeType();

template
void
FetchBody<Response>::SetMimeType();

} // namespace dom
} // namespace mozilla
