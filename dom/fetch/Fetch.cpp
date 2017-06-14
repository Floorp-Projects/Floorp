/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Fetch.h"

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
    NS_ProxyRelease(mMainThreadEventTarget, mSignalMainThread.forget());
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

namespace {
/*
 * Called on successfully reading the complete stream.
 */
template <class Derived>
class ContinueConsumeBodyRunnable final : public MainThreadWorkerRunnable
{
  // This has been addrefed before this runnable is dispatched,
  // released in WorkerRun().
  FetchBody<Derived>* mFetchBody;
  nsresult mStatus;
  uint32_t mLength;
  uint8_t* mResult;

public:
  ContinueConsumeBodyRunnable(FetchBody<Derived>* aFetchBody, nsresult aStatus,
                              uint32_t aLength, uint8_t* aResult)
    : MainThreadWorkerRunnable(aFetchBody->mWorkerPrivate)
    , mFetchBody(aFetchBody)
    , mStatus(aStatus)
    , mLength(aLength)
    , mResult(aResult)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    mFetchBody->ContinueConsumeBody(mStatus, mLength, mResult);
    return true;
  }
};

/*
 * Called on successfully reading the complete stream for Blob.
 */
template <class Derived>
class ContinueConsumeBlobBodyRunnable final : public MainThreadWorkerRunnable
{
  // This has been addrefed before this runnable is dispatched,
  // released in WorkerRun().
  FetchBody<Derived>* mFetchBody;
  RefPtr<BlobImpl> mBlobImpl;

public:
  ContinueConsumeBlobBodyRunnable(FetchBody<Derived>* aFetchBody,
                                  BlobImpl* aBlobImpl)
    : MainThreadWorkerRunnable(aFetchBody->mWorkerPrivate)
    , mFetchBody(aFetchBody)
    , mBlobImpl(aBlobImpl)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mBlobImpl);
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    mFetchBody->ContinueConsumeBlobBody(mBlobImpl);
    return true;
  }
};

// OnStreamComplete always adopts the buffer, utility class to release it in
// a couple of places.
class MOZ_STACK_CLASS AutoFreeBuffer final {
  uint8_t* mBuffer;

public:
  explicit AutoFreeBuffer(uint8_t* aBuffer)
    : mBuffer(aBuffer)
  {}

  ~AutoFreeBuffer()
  {
    free(mBuffer);
  }

  void
  Reset()
  {
    mBuffer= nullptr;
  }
};

template <class Derived>
class FailConsumeBodyWorkerRunnable : public MainThreadWorkerControlRunnable
{
  FetchBody<Derived>* mBody;
public:
  explicit FailConsumeBodyWorkerRunnable(FetchBody<Derived>* aBody)
    : MainThreadWorkerControlRunnable(aBody->mWorkerPrivate)
    , mBody(aBody)
  {
    AssertIsOnMainThread();
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    mBody->ContinueConsumeBody(NS_ERROR_FAILURE, 0, nullptr);
    return true;
  }
};

/*
 * In case of failure to create a stream pump or dispatch stream completion to
 * worker, ensure we cleanup properly. Thread agnostic.
 */
template <class Derived>
class MOZ_STACK_CLASS AutoFailConsumeBody final
{
  FetchBody<Derived>* mBody;
public:
  explicit AutoFailConsumeBody(FetchBody<Derived>* aBody)
    : mBody(aBody)
  { }

  ~AutoFailConsumeBody()
  {
    AssertIsOnMainThread();
    if (mBody) {
      if (mBody->mWorkerPrivate) {
        RefPtr<FailConsumeBodyWorkerRunnable<Derived>> r =
          new FailConsumeBodyWorkerRunnable<Derived>(mBody);
        if (!r->Dispatch()) {
          MOZ_CRASH("We are going to leak");
        }
      } else {
        mBody->ContinueConsumeBody(NS_ERROR_FAILURE, 0, nullptr);
      }
    }
  }

  void
  DontFail()
  {
    mBody = nullptr;
  }
};

template <class Derived>
class ConsumeBodyDoneObserver : public nsIStreamLoaderObserver
                              , public MutableBlobStorageCallback
{
  FetchBody<Derived>* mFetchBody;

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit ConsumeBodyDoneObserver(FetchBody<Derived>* aFetchBody)
    : mFetchBody(aFetchBody)
  { }

  NS_IMETHOD
  OnStreamComplete(nsIStreamLoader* aLoader,
                   nsISupports* aCtxt,
                   nsresult aStatus,
                   uint32_t aResultLength,
                   const uint8_t* aResult) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    // If the binding requested cancel, we don't need to call
    // ContinueConsumeBody, since that is the originator.
    if (aStatus == NS_BINDING_ABORTED) {
      return NS_OK;
    }

    uint8_t* nonconstResult = const_cast<uint8_t*>(aResult);
    if (mFetchBody->mWorkerPrivate) {
      RefPtr<ContinueConsumeBodyRunnable<Derived>> r =
        new ContinueConsumeBodyRunnable<Derived>(mFetchBody,
                                        aStatus,
                                        aResultLength,
                                        nonconstResult);
      if (!r->Dispatch()) {
        // XXXcatalinb: The worker is shutting down, the pump will be canceled
        // by FetchBodyWorkerHolder::Notify.
        NS_WARNING("Could not dispatch ConsumeBodyRunnable");
        // Return failure so that aResult is freed.
        return NS_ERROR_FAILURE;
      }
    } else {
      mFetchBody->ContinueConsumeBody(aStatus, aResultLength, nonconstResult);
    }

    // FetchBody is responsible for data.
    return NS_SUCCESS_ADOPTED_DATA;
  }

  virtual void BlobStoreCompleted(MutableBlobStorage* aBlobStorage,
                                  Blob* aBlob,
                                  nsresult aRv) override
  {
    // On error.
    if (NS_FAILED(aRv)) {
      OnStreamComplete(nullptr, nullptr, aRv, 0, nullptr);
      return;
    }

    MOZ_ASSERT(aBlob);

    if (mFetchBody->mWorkerPrivate) {
      RefPtr<ContinueConsumeBlobBodyRunnable<Derived>> r =
        new ContinueConsumeBlobBodyRunnable<Derived>(mFetchBody, aBlob->Impl());

      if (!r->Dispatch()) {
        NS_WARNING("Could not dispatch ConsumeBlobBodyRunnable");
        return;
      }
    } else {
      mFetchBody->ContinueConsumeBlobBody(aBlob->Impl());
    }
  }

private:
  virtual ~ConsumeBodyDoneObserver()
  { }
};

template <class Derived>
NS_IMPL_ADDREF(ConsumeBodyDoneObserver<Derived>)
template <class Derived>
NS_IMPL_RELEASE(ConsumeBodyDoneObserver<Derived>)
template <class Derived>
NS_INTERFACE_MAP_BEGIN(ConsumeBodyDoneObserver<Derived>)
  NS_INTERFACE_MAP_ENTRY(nsIStreamLoaderObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamLoaderObserver)
NS_INTERFACE_MAP_END

template <class Derived>
class BeginConsumeBodyRunnable final : public Runnable
{
  FetchBody<Derived>* mFetchBody;
public:
  explicit BeginConsumeBodyRunnable(FetchBody<Derived>* aBody)
    : mFetchBody(aBody)
  { }

  NS_IMETHOD
  Run() override
  {
    mFetchBody->BeginConsumeBodyMainThread();
    return NS_OK;
  }
};

template <class Derived>
class CancelPumpRunnable final : public WorkerMainThreadRunnable
{
  FetchBody<Derived>* mBody;
public:
  explicit CancelPumpRunnable(FetchBody<Derived>* aBody)
    : WorkerMainThreadRunnable(aBody->mWorkerPrivate,
                               NS_LITERAL_CSTRING("Fetch :: Cancel Pump"))
    , mBody(aBody)
  { }

  bool
  MainThreadRun() override
  {
    mBody->CancelPump();
    return true;
  }
};
} // namespace

template <class Derived>
class FetchBodyWorkerHolder final : public workers::WorkerHolder
{
  // This is addrefed before the workerHolder is created, and is released in
  // ContinueConsumeBody() so we can hold a rawptr.
  FetchBody<Derived>* mBody;
  bool mWasNotified;

public:
  explicit FetchBodyWorkerHolder(FetchBody<Derived>* aBody)
    : mBody(aBody)
    , mWasNotified(false)
  { }

  ~FetchBodyWorkerHolder()
  { }

  bool Notify(workers::Status aStatus) override
  {
    MOZ_ASSERT(aStatus > workers::Running);
    if (!mWasNotified) {
      mWasNotified = true;
      mBody->ContinueConsumeBody(NS_BINDING_ABORTED, 0, nullptr);
    }
    return true;
  }
};

template <class Derived>
FetchBody<Derived>::FetchBody(nsIGlobalObject* aOwner)
  : mWorkerHolder(nullptr)
  , mOwner(aOwner)
  , mBodyUsed(false)
#ifdef DEBUG
  , mReadDone(false)
#endif
{
  MOZ_ASSERT(aOwner);

  if (!NS_IsMainThread()) {
    mWorkerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(mWorkerPrivate);
    mMainThreadEventTarget = mWorkerPrivate->MainThreadEventTarget();
  } else {
    mWorkerPrivate = nullptr;
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

// Returns true if addref succeeded.
// Always succeeds on main thread.
// May fail on worker if RegisterWorkerHolder() fails. In that case, it will
// release the object before returning false.
template <class Derived>
bool
FetchBody<Derived>::AddRefObject()
{
  AssertIsOnTargetThread();
  DerivedClass()->AddRef();

  if (mWorkerPrivate && !mWorkerHolder) {
    if (!RegisterWorkerHolder()) {
      ReleaseObject();
      return false;
    }
  }
  return true;
}

template <class Derived>
void
FetchBody<Derived>::ReleaseObject()
{
  AssertIsOnTargetThread();

  if (mWorkerPrivate && mWorkerHolder) {
    UnregisterWorkerHolder();
  }

  DerivedClass()->Release();
}

template <class Derived>
bool
FetchBody<Derived>::RegisterWorkerHolder()
{
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(!mWorkerHolder);
  mWorkerHolder = new FetchBodyWorkerHolder<Derived>(this);

  if (!mWorkerHolder->HoldWorker(mWorkerPrivate, Closing)) {
    NS_WARNING("Failed to add workerHolder");
    mWorkerHolder = nullptr;
    return false;
  }

  return true;
}

template <class Derived>
void
FetchBody<Derived>::UnregisterWorkerHolder()
{
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerHolder);

  mWorkerHolder->ReleaseWorker();
  mWorkerHolder = nullptr;
}

template <class Derived>
void
FetchBody<Derived>::CancelPump()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mConsumeBodyPump);
  mConsumeBodyPump->Cancel(NS_BINDING_ABORTED);
}

// Return value is used by ConsumeBody to bubble the error code up to WebIDL so
// mConsumePromise doesn't have to be rejected on early exit.
template <class Derived>
nsresult
FetchBody<Derived>::BeginConsumeBody()
{
  AssertIsOnTargetThread();
  MOZ_ASSERT(!mWorkerHolder);
  MOZ_ASSERT(mConsumePromise);

  // The FetchBody is not thread-safe refcounted. We addref it here and release
  // it once the stream read is finished.
  if (!AddRefObject()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIRunnable> r = new BeginConsumeBodyRunnable<Derived>(this);
  nsresult rv = NS_OK;
  mMainThreadEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ReleaseObject();
    return rv;
  }
  return NS_OK;
}

/*
 * BeginConsumeBodyMainThread() will automatically reject the consume promise
 * and clean up on any failures, so there is no need for callers to do so,
 * reflected in a lack of error return code.
 */
template <class Derived>
void
FetchBody<Derived>::BeginConsumeBodyMainThread()
{
  AssertIsOnMainThread();
  AutoFailConsumeBody<Derived> autoReject(DerivedClass());
  nsresult rv;
  nsCOMPtr<nsIInputStream> stream;
  DerivedClass()->GetBody(getter_AddRefs(stream));
  if (!stream) {
    rv = NS_NewCStringInputStream(getter_AddRefs(stream), EmptyCString());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

  nsCOMPtr<nsIInputStreamPump> pump;
  rv = NS_NewInputStreamPump(getter_AddRefs(pump),
                             stream, -1, -1, 0, 0, false,
                             mMainThreadEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  RefPtr<ConsumeBodyDoneObserver<Derived>> p = new ConsumeBodyDoneObserver<Derived>(this);

  nsCOMPtr<nsIStreamListener> listener;
  if (mConsumeType == CONSUME_BLOB) {
    MutableBlobStorage::MutableBlobStorageType type =
      MutableBlobStorage::eOnlyInMemory;

    const mozilla::UniquePtr<mozilla::ipc::PrincipalInfo>& principalInfo =
      DerivedClass()->GetPrincipalInfo();
    // We support temporary file for blobs only if the principal is known and
    // it's system or content not in private Browsing.
    if (principalInfo &&
        (principalInfo->type() == mozilla::ipc::PrincipalInfo::TSystemPrincipalInfo ||
         (principalInfo->type() == mozilla::ipc::PrincipalInfo::TContentPrincipalInfo &&
          principalInfo->get_ContentPrincipalInfo().attrs().mPrivateBrowsingId == 0))) {
      type = MutableBlobStorage::eCouldBeInTemporaryFile;
    }

    listener = new MutableBlobStreamListener(type, nullptr, mMimeType, p,
                                             mMainThreadEventTarget);
  } else {
    nsCOMPtr<nsIStreamLoader> loader;
    rv = NS_NewStreamLoader(getter_AddRefs(loader), p);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    listener = loader;
  }

  rv = pump->AsyncRead(listener, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Now that everything succeeded, we can assign the pump to a pointer that
  // stays alive for the lifetime of the FetchBody.
  mConsumeBodyPump =
    new nsMainThreadPtrHolder<nsIInputStreamPump>(pump, mMainThreadEventTarget);
  // It is ok for retargeting to fail and reads to happen on the main thread.
  autoReject.DontFail();

  // Try to retarget, otherwise fall back to main thread.
  nsCOMPtr<nsIThreadRetargetableRequest> rr = do_QueryInterface(pump);
  if (rr) {
    nsCOMPtr<nsIEventTarget> sts = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    rv = rr->RetargetDeliveryTo(sts);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      NS_WARNING("Retargeting failed");
    }
  }
}

template <class Derived>
void
FetchBody<Derived>::ContinueConsumeBody(nsresult aStatus, uint32_t aResultLength, uint8_t* aResult)
{
  AssertIsOnTargetThread();
  // Just a precaution to ensure ContinueConsumeBody is not called out of
  // sync with a body read.
  MOZ_ASSERT(mBodyUsed);
  MOZ_ASSERT(!mReadDone);
  MOZ_ASSERT_IF(mWorkerPrivate, mWorkerHolder);
#ifdef DEBUG
  mReadDone = true;
#endif

  AutoFreeBuffer autoFree(aResult);

  MOZ_ASSERT(mConsumePromise);
  RefPtr<Promise> localPromise = mConsumePromise.forget();

  RefPtr<Derived> derivedClass = DerivedClass();
  ReleaseObject();

  if (NS_WARN_IF(NS_FAILED(aStatus))) {
    localPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);

    // If binding aborted, cancel the pump. We can't assert mConsumeBodyPump.
    // In the (admittedly rare) situation that BeginConsumeBodyMainThread()
    // context switches out, and the worker thread gets canceled before the
    // pump is setup, mConsumeBodyPump will be null.
    // We've to use the !! form since non-main thread pointer access on
    // a nsMainThreadPtrHandle is not permitted.
    if (aStatus == NS_BINDING_ABORTED && !!mConsumeBodyPump) {
      if (NS_IsMainThread()) {
        CancelPump();
      } else {
        MOZ_ASSERT(mWorkerPrivate);
        // In case of worker thread, we block the worker while the request is
        // canceled on the main thread. This ensures that OnStreamComplete has
        // a valid FetchBody around to call CancelPump and we don't release the
        // FetchBody on the main thread.
        RefPtr<CancelPumpRunnable<Derived>> r =
          new CancelPumpRunnable<Derived>(this);
        ErrorResult rv;
        r->Dispatch(Terminating, rv);
        if (rv.Failed()) {
          NS_WARNING("Could not dispatch CancelPumpRunnable. Nothing we can do here");
          // None of our callers are callled directly from JS, so there is no
          // point in trying to propagate this failure out of here.  And
          // localPromise is already rejected.  Just suppress the failure.
          rv.SuppressException();
        }
      }
    }
  }

  // Release the pump and then early exit if there was an error.
  // Uses NS_ProxyRelease internally, so this is safe.
  mConsumeBodyPump = nullptr;

  // Don't warn here since we warned above.
  if (NS_FAILED(aStatus)) {
    return;
  }

  // Finish successfully consuming body according to type.
  MOZ_ASSERT(aResult);

  AutoJSAPI jsapi;
  if (!jsapi.Init(derivedClass->GetParentObject())) {
    localPromise->MaybeReject(NS_ERROR_UNEXPECTED);
    return;
  }

  JSContext* cx = jsapi.cx();
  ErrorResult error;

  switch (mConsumeType) {
    case CONSUME_ARRAYBUFFER: {
      JS::Rooted<JSObject*> arrayBuffer(cx);
      BodyUtil::ConsumeArrayBuffer(cx, &arrayBuffer, aResultLength, aResult,
                                   error);

      if (!error.Failed()) {
        JS::Rooted<JS::Value> val(cx);
        val.setObjectOrNull(arrayBuffer);

        localPromise->MaybeResolve(cx, val);
        // ArrayBuffer takes over ownership.
        autoFree.Reset();
      }
      break;
    }
    case CONSUME_BLOB: {
      MOZ_CRASH("This should not happen.");
      break;
    }
    case CONSUME_FORMDATA: {
      nsCString data;
      data.Adopt(reinterpret_cast<char*>(aResult), aResultLength);
      autoFree.Reset();

      RefPtr<dom::FormData> fd = BodyUtil::ConsumeFormData(
        derivedClass->GetParentObject(),
        mMimeType, data, error);
      if (!error.Failed()) {
        localPromise->MaybeResolve(fd);
      }
      break;
    }
    case CONSUME_TEXT:
      // fall through handles early exit.
    case CONSUME_JSON: {
      nsString decoded;
      if (NS_SUCCEEDED(BodyUtil::ConsumeText(aResultLength, aResult, decoded))) {
        if (mConsumeType == CONSUME_TEXT) {
          localPromise->MaybeResolve(decoded);
        } else {
          JS::Rooted<JS::Value> json(cx);
          BodyUtil::ConsumeJson(cx, &json, decoded, error);
          if (!error.Failed()) {
            localPromise->MaybeResolve(cx, json);
          }
        }
      };
      break;
    }
    default:
      NS_NOTREACHED("Unexpected consume body type");
  }

  error.WouldReportJSException();
  if (error.Failed()) {
    localPromise->MaybeReject(error);
  }
}

template <class Derived>
void
FetchBody<Derived>::ContinueConsumeBlobBody(BlobImpl* aBlobImpl)
{
  AssertIsOnTargetThread();
  // Just a precaution to ensure ContinueConsumeBody is not called out of
  // sync with a body read.
  MOZ_ASSERT(mBodyUsed);
  MOZ_ASSERT(!mReadDone);
  MOZ_ASSERT(mConsumeType == CONSUME_BLOB);
  MOZ_ASSERT_IF(mWorkerPrivate, mWorkerHolder);
#ifdef DEBUG
  mReadDone = true;
#endif

  MOZ_ASSERT(mConsumePromise);
  RefPtr<Promise> localPromise = mConsumePromise.forget();

  RefPtr<Derived> derivedClass = DerivedClass();
  ReleaseObject();

  // Release the pump and then early exit if there was an error.
  // Uses NS_ProxyRelease internally, so this is safe.
  mConsumeBodyPump = nullptr;

  RefPtr<dom::Blob> blob =
    dom::Blob::Create(derivedClass->GetParentObject(), aBlobImpl);
  MOZ_ASSERT(blob);

  localPromise->MaybeResolve(blob);
}

template <class Derived>
already_AddRefed<Promise>
FetchBody<Derived>::ConsumeBody(ConsumeType aType, ErrorResult& aRv)
{
  if (BodyUsed()) {
    aRv.ThrowTypeError<MSG_FETCH_BODY_CONSUMED_ERROR>();
    return nullptr;
  }

  mConsumeType = aType;
  SetBodyUsed();

  mConsumePromise = Promise::Create(DerivedClass()->GetParentObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  aRv = BeginConsumeBody();
  if (NS_WARN_IF(aRv.Failed())) {
    mConsumePromise = nullptr;
    return nullptr;
  }

  RefPtr<Promise> promise = mConsumePromise;
  return promise.forget();
}

template
already_AddRefed<Promise>
FetchBody<Request>::ConsumeBody(ConsumeType aType, ErrorResult& aRv);

template
already_AddRefed<Promise>
FetchBody<Response>::ConsumeBody(ConsumeType aType, ErrorResult& aRv);

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
