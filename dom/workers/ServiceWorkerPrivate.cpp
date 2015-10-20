/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerPrivate.h"

#include "ServiceWorkerManager.h"

using namespace mozilla;
using namespace mozilla::dom;

BEGIN_WORKERS_NAMESPACE

NS_IMPL_ISUPPORTS0(ServiceWorkerPrivate)

// Tracks the "dom.disable_open_click_delay" preference.  Modified on main
// thread, read on worker threads.
// It is updated every time a "notificationclick" event is dispatched. While
// this is done without synchronization, at the worst, the thread will just get
// an older value within which a popup is allowed to be displayed, which will
// still be a valid value since it was set prior to dispatching the runnable.
Atomic<uint32_t> gDOMDisableOpenClickDelay(0);

// Used to keep track of pending waitUntil as well as in-flight extendable events.
// When the last token is released, we attempt to terminate the worker.
class KeepAliveToken final : public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  explicit KeepAliveToken(ServiceWorkerPrivate* aPrivate)
    : mPrivate(aPrivate)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aPrivate);
    mPrivate->AddToken();
  }

private:
  ~KeepAliveToken()
  {
    AssertIsOnMainThread();
    mPrivate->ReleaseToken();
  }

  RefPtr<ServiceWorkerPrivate> mPrivate;
};

NS_IMPL_ISUPPORTS0(KeepAliveToken)

ServiceWorkerPrivate::ServiceWorkerPrivate(ServiceWorkerInfo* aInfo)
  : mInfo(aInfo)
  , mIsPushWorker(false)
  , mTokenCount(0)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aInfo);

  mIdleWorkerTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  MOZ_ASSERT(mIdleWorkerTimer);
}

ServiceWorkerPrivate::~ServiceWorkerPrivate()
{
  MOZ_ASSERT(!mWorkerPrivate);
  MOZ_ASSERT(!mTokenCount);
  MOZ_ASSERT(!mInfo);

  mIdleWorkerTimer->Cancel();
}

nsresult
ServiceWorkerPrivate::SendMessageEvent(JSContext* aCx,
                                       JS::Handle<JS::Value> aMessage,
                                       const Optional<Sequence<JS::Value>>& aTransferable,
                                       UniquePtr<ServiceWorkerClientInfo>&& aClientInfo)
{
  ErrorResult rv(SpawnWorkerIfNeeded(MessageEvent, nullptr));
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  // FIXME(catalinb): Bug 1143717
  // Keep the worker alive when dispatching a message event.
  mWorkerPrivate->PostMessageToServiceWorker(aCx, aMessage, aTransferable,
                                             Move(aClientInfo), rv);
  return rv.StealNSResult();
}

namespace {

class CheckScriptEvaluationWithCallback final : public WorkerRunnable
{
  nsMainThreadPtrHandle<KeepAliveToken> mKeepAliveToken;
  RefPtr<nsRunnable> mCallback;

public:
  CheckScriptEvaluationWithCallback(WorkerPrivate* aWorkerPrivate,
                                    KeepAliveToken* aKeepAliveToken,
                                    nsRunnable* aCallback)
    : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount)
    , mKeepAliveToken(new nsMainThreadPtrHolder<KeepAliveToken>(aKeepAliveToken))
    , mCallback(aCallback)
  {
    AssertIsOnMainThread();
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
    if (aWorkerPrivate->WorkerScriptExecutedSuccessfully()) {
      nsresult rv = NS_DispatchToMainThread(mCallback);
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to dispatch CheckScriptEvaluation callback.");
      }
    }

    return true;
  }
};

} // anonymous namespace

nsresult
ServiceWorkerPrivate::ContinueOnSuccessfulScriptEvaluation(nsRunnable* aCallback)
{
  nsresult rv = SpawnWorkerIfNeeded(LifeCycleEvent, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(mKeepAliveToken);
  RefPtr<WorkerRunnable> r = new CheckScriptEvaluationWithCallback(mWorkerPrivate,
                                                                     mKeepAliveToken,
                                                                     aCallback);
  AutoJSAPI jsapi;
  jsapi.Init();
  if (NS_WARN_IF(!r->Dispatch(jsapi.cx()))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

namespace {

// Holds the worker alive until the waitUntil promise is resolved or
// rejected.
class KeepAliveHandler final : public PromiseNativeHandler
{
  nsMainThreadPtrHandle<KeepAliveToken> mKeepAliveToken;

  virtual ~KeepAliveHandler()
  {}

public:
  NS_DECL_ISUPPORTS

  explicit KeepAliveHandler(const nsMainThreadPtrHandle<KeepAliveToken>& aKeepAliveToken)
    : mKeepAliveToken(aKeepAliveToken)
  { }

  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
#ifdef DEBUG
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    workerPrivate->AssertIsOnWorkerThread();
#endif
  }

  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
#ifdef DEBUG
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    workerPrivate->AssertIsOnWorkerThread();
#endif
  }
};

NS_IMPL_ISUPPORTS0(KeepAliveHandler)

class ExtendableEventWorkerRunnable : public WorkerRunnable
{
protected:
  nsMainThreadPtrHandle<KeepAliveToken> mKeepAliveToken;

public:
  ExtendableEventWorkerRunnable(WorkerPrivate* aWorkerPrivate,
                                KeepAliveToken* aKeepAliveToken)
    : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aKeepAliveToken);

    mKeepAliveToken =
      new nsMainThreadPtrHolder<KeepAliveToken>(aKeepAliveToken);
  }

  void
  DispatchExtendableEventOnWorkerScope(JSContext* aCx,
                                       WorkerGlobalScope* aWorkerScope,
                                       ExtendableEvent* aEvent,
                                       Promise** aWaitUntilPromise)
  {
    MOZ_ASSERT(aWorkerScope);
    MOZ_ASSERT(aEvent);
    nsCOMPtr<nsIGlobalObject> sgo = aWorkerScope;
    WidgetEvent* internalEvent = aEvent->GetInternalNSEvent();

    ErrorResult result;
    result = aWorkerScope->DispatchDOMEvent(nullptr, aEvent, nullptr, nullptr);
    if (NS_WARN_IF(result.Failed()) || internalEvent->mFlags.mExceptionHasBeenRisen) {
      result.SuppressException();
      return;
    }

    RefPtr<Promise> waitUntilPromise = aEvent->GetPromise();
    if (!waitUntilPromise) {
      waitUntilPromise =
        Promise::Resolve(sgo, aCx, JS::UndefinedHandleValue, result);
      if (NS_WARN_IF(result.Failed())) {
        result.SuppressException();
        return;
      }
    }

    MOZ_ASSERT(waitUntilPromise);
    RefPtr<KeepAliveHandler> keepAliveHandler =
      new KeepAliveHandler(mKeepAliveToken);
    waitUntilPromise->AppendNativeHandler(keepAliveHandler);

    if (aWaitUntilPromise) {
      waitUntilPromise.forget(aWaitUntilPromise);
    }
  }
};

/*
 * Fires 'install' event on the ServiceWorkerGlobalScope. Modifies busy count
 * since it fires the event. This is ok since there can't be nested
 * ServiceWorkers, so the parent thread -> worker thread requirement for
 * runnables is satisfied.
 */
class LifecycleEventWorkerRunnable : public ExtendableEventWorkerRunnable
{
  nsString mEventName;
  RefPtr<LifeCycleEventCallback> mCallback;

public:
  LifecycleEventWorkerRunnable(WorkerPrivate* aWorkerPrivate,
                               KeepAliveToken* aToken,
                               const nsAString& aEventName,
                               LifeCycleEventCallback* aCallback)
      : ExtendableEventWorkerRunnable(aWorkerPrivate, aToken)
      , mEventName(aEventName)
      , mCallback(aCallback)
  {
    AssertIsOnMainThread();
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    return DispatchLifecycleEvent(aCx, aWorkerPrivate);
  }

private:
  bool
  DispatchLifecycleEvent(JSContext* aCx, WorkerPrivate* aWorkerPrivate);

};

/*
 * Used to handle ExtendableEvent::waitUntil() and proceed with
 * installation/activation.
 */
class LifecycleEventPromiseHandler final : public PromiseNativeHandler
{
  RefPtr<LifeCycleEventCallback> mCallback;

  virtual
  ~LifecycleEventPromiseHandler()
  { }

public:
  NS_DECL_ISUPPORTS

  explicit LifecycleEventPromiseHandler(LifeCycleEventCallback* aCallback)
    : mCallback(aCallback)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    workerPrivate->AssertIsOnWorkerThread();

    mCallback->SetResult(true);
    nsresult rv = NS_DispatchToMainThread(mCallback);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      NS_RUNTIMEABORT("Failed to dispatch life cycle event handler.");
    }
  }

  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    workerPrivate->AssertIsOnWorkerThread();

    mCallback->SetResult(false);
    nsresult rv = NS_DispatchToMainThread(mCallback);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      NS_RUNTIMEABORT("Failed to dispatch life cycle event handler.");
    }

    JS::Rooted<JSObject*> obj(aCx, workerPrivate->GlobalScope()->GetWrapper());
    JS::ExposeValueToActiveJS(aValue);

    js::ErrorReport report(aCx);
    if (NS_WARN_IF(!report.init(aCx, aValue))) {
      JS_ClearPendingException(aCx);
      return;
    }

    RefPtr<xpc::ErrorReport> xpcReport = new xpc::ErrorReport();
    xpcReport->Init(report.report(), report.message(),
                    /* aIsChrome = */ false, /* aWindowID = */ 0);

    RefPtr<AsyncErrorReporter> aer = new AsyncErrorReporter(xpcReport);
    NS_DispatchToMainThread(aer);
  }
};

NS_IMPL_ISUPPORTS0(LifecycleEventPromiseHandler)

bool
LifecycleEventWorkerRunnable::DispatchLifecycleEvent(JSContext* aCx,
                                                     WorkerPrivate* aWorkerPrivate)
{
  aWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());

  RefPtr<ExtendableEvent> event;
  RefPtr<EventTarget> target = aWorkerPrivate->GlobalScope();

  if (mEventName.EqualsASCII("install") || mEventName.EqualsASCII("activate")) {
    ExtendableEventInit init;
    init.mBubbles = false;
    init.mCancelable = false;
    event = ExtendableEvent::Constructor(target, mEventName, init);
  } else {
    MOZ_CRASH("Unexpected lifecycle event");
  }

  event->SetTrusted(true);

  RefPtr<Promise> waitUntil;
  DispatchExtendableEventOnWorkerScope(aCx, aWorkerPrivate->GlobalScope(),
                                       event, getter_AddRefs(waitUntil));
  if (waitUntil) {
    RefPtr<LifecycleEventPromiseHandler> handler =
      new LifecycleEventPromiseHandler(mCallback);
    waitUntil->AppendNativeHandler(handler);
  } else {
    mCallback->SetResult(false);
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(mCallback)));
  }

  return true;
}

} // anonymous namespace

nsresult
ServiceWorkerPrivate::SendLifeCycleEvent(const nsAString& aEventType,
                                         LifeCycleEventCallback* aCallback,
                                         nsIRunnable* aLoadFailure)
{
  nsresult rv = SpawnWorkerIfNeeded(LifeCycleEvent, aLoadFailure);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(mKeepAliveToken);
  RefPtr<WorkerRunnable> r = new LifecycleEventWorkerRunnable(mWorkerPrivate,
                                                                mKeepAliveToken,
                                                                aEventType,
                                                                aCallback);
  AutoJSAPI jsapi;
  jsapi.Init();
  if (NS_WARN_IF(!r->Dispatch(jsapi.cx()))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

#ifndef MOZ_SIMPLEPUSH
namespace {

class SendPushEventRunnable final : public ExtendableEventWorkerRunnable
{
  Maybe<nsTArray<uint8_t>> mData;

public:
  SendPushEventRunnable(WorkerPrivate* aWorkerPrivate,
                        KeepAliveToken* aKeepAliveToken,
                        const Maybe<nsTArray<uint8_t>>& aData)
      : ExtendableEventWorkerRunnable(aWorkerPrivate, aKeepAliveToken)
      , mData(aData)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    GlobalObject globalObj(aCx, aWorkerPrivate->GlobalScope()->GetWrapper());

    PushEventInit pei;
    if (mData) {
      const nsTArray<uint8_t>& bytes = mData.ref();
      JSObject* data = Uint8Array::Create(aCx, bytes.Length(), bytes.Elements());
      if (!data) {
        return false;
      }
      pei.mData.Construct().SetAsArrayBufferView().Init(data);
    }
    pei.mBubbles = false;
    pei.mCancelable = false;

    ErrorResult result;
    RefPtr<PushEvent> event =
      PushEvent::Constructor(globalObj, NS_LITERAL_STRING("push"), pei, result);
    if (NS_WARN_IF(result.Failed())) {
      result.SuppressException();
      return false;
    }
    event->SetTrusted(true);

    DispatchExtendableEventOnWorkerScope(aCx, aWorkerPrivate->GlobalScope(),
                                         event, nullptr);

    return true;
  }
};

class SendPushSubscriptionChangeEventRunnable final : public ExtendableEventWorkerRunnable
{

public:
  explicit SendPushSubscriptionChangeEventRunnable(
    WorkerPrivate* aWorkerPrivate, KeepAliveToken* aKeepAliveToken)
      : ExtendableEventWorkerRunnable(aWorkerPrivate, aKeepAliveToken)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);

    WorkerGlobalScope* globalScope = aWorkerPrivate->GlobalScope();

    RefPtr<Event> event = NS_NewDOMEvent(globalScope, nullptr, nullptr);

    nsresult rv = event->InitEvent(NS_LITERAL_STRING("pushsubscriptionchange"),
                                   false, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    event->SetTrusted(true);

    globalScope->DispatchDOMEvent(nullptr, event, nullptr, nullptr);
    return true;
  }
};

} // anonymous namespace
#endif // !MOZ_SIMPLEPUSH

nsresult
ServiceWorkerPrivate::SendPushEvent(const Maybe<nsTArray<uint8_t>>& aData)
{
#ifdef MOZ_SIMPLEPUSH
  return NS_ERROR_NOT_AVAILABLE;
#else
  nsresult rv = SpawnWorkerIfNeeded(PushEvent, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(mKeepAliveToken);
  RefPtr<WorkerRunnable> r = new SendPushEventRunnable(mWorkerPrivate,
                                                         mKeepAliveToken,
                                                         aData);
  AutoJSAPI jsapi;
  jsapi.Init();
  if (NS_WARN_IF(!r->Dispatch(jsapi.cx()))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
#endif // MOZ_SIMPLEPUSH
}

nsresult
ServiceWorkerPrivate::SendPushSubscriptionChangeEvent()
{
#ifdef MOZ_SIMPLEPUSH
  return NS_ERROR_NOT_AVAILABLE;
#else
  nsresult rv = SpawnWorkerIfNeeded(PushSubscriptionChangeEvent, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(mKeepAliveToken);
  RefPtr<WorkerRunnable> r =
    new SendPushSubscriptionChangeEventRunnable(mWorkerPrivate, mKeepAliveToken);
  AutoJSAPI jsapi;
  jsapi.Init();
  if (NS_WARN_IF(!r->Dispatch(jsapi.cx()))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
#endif // MOZ_SIMPLEPUSH
}

namespace {

static void
DummyNotificationTimerCallback(nsITimer* aTimer, void* aClosure)
{
  // Nothing.
}

class AllowWindowInteractionHandler;

class ClearWindowAllowedRunnable final : public WorkerRunnable
{
public:
  ClearWindowAllowedRunnable(WorkerPrivate* aWorkerPrivate,
                             AllowWindowInteractionHandler* aHandler)
  : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  , mHandler(aHandler)
  { }

private:
  bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    // WorkerRunnable asserts that the dispatch is from parent thread if
    // the busy count modification is WorkerThreadUnchangedBusyCount.
    // Since this runnable will be dispatched from the timer thread, we override
    // PreDispatch and PostDispatch to skip the check.
    return true;
  }

  void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) override
  {
    // Silence bad assertions.
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override;

  RefPtr<AllowWindowInteractionHandler> mHandler;
};

class AllowWindowInteractionHandler final : public PromiseNativeHandler
{
  friend class ClearWindowAllowedRunnable;
  nsCOMPtr<nsITimer> mTimer;

  ~AllowWindowInteractionHandler()
  {
  }

  void
  ClearWindowAllowed(WorkerPrivate* aWorkerPrivate)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    if (!mTimer) {
      return;
    }

    // XXXcatalinb: This *might* be executed after the global was unrooted, in
    // which case GlobalScope() will return null. Making the check here just
    // to be safe.
    WorkerGlobalScope* globalScope = aWorkerPrivate->GlobalScope();
    if (!globalScope) {
      return;
    }

    globalScope->ConsumeWindowInteraction();
    mTimer->Cancel();
    mTimer = nullptr;
    MOZ_ALWAYS_TRUE(aWorkerPrivate->ModifyBusyCountFromWorker(aWorkerPrivate->GetJSContext(),
                                                              false));
  }

  void
  StartClearWindowTimer(WorkerPrivate* aWorkerPrivate)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(!mTimer);

    nsresult rv;
    nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    RefPtr<ClearWindowAllowedRunnable> r =
      new ClearWindowAllowedRunnable(aWorkerPrivate, this);

    RefPtr<TimerThreadEventTarget> target =
      new TimerThreadEventTarget(aWorkerPrivate, r);

    rv = timer->SetTarget(target);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    // The important stuff that *has* to be reversed.
    if (NS_WARN_IF(!aWorkerPrivate->ModifyBusyCountFromWorker(aWorkerPrivate->GetJSContext(), true))) {
      return;
    }
    aWorkerPrivate->GlobalScope()->AllowWindowInteraction();
    timer.swap(mTimer);

    // We swap first and then initialize the timer so that even if initializing
    // fails, we still clean the busy count and interaction count correctly.
    // The timer can't be initialized before modifying the busy count since the
    // timer thread could run and call the timeout but the worker may
    // already be terminating and modifying the busy count could fail.
    rv = mTimer->InitWithFuncCallback(DummyNotificationTimerCallback, nullptr,
                                      gDOMDisableOpenClickDelay,
                                      nsITimer::TYPE_ONE_SHOT);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      ClearWindowAllowed(aWorkerPrivate);
      return;
    }
  }

public:
  NS_DECL_ISUPPORTS

  explicit AllowWindowInteractionHandler(WorkerPrivate* aWorkerPrivate)
  {
    StartClearWindowTimer(aWorkerPrivate);
  }

  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
    ClearWindowAllowed(workerPrivate);
  }

  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
    ClearWindowAllowed(workerPrivate);
  }
};

NS_IMPL_ISUPPORTS0(AllowWindowInteractionHandler)

bool
ClearWindowAllowedRunnable::WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
{
  mHandler->ClearWindowAllowed(aWorkerPrivate);
  return true;
}

class SendNotificationClickEventRunnable final : public ExtendableEventWorkerRunnable
{
  const nsString mID;
  const nsString mTitle;
  const nsString mDir;
  const nsString mLang;
  const nsString mBody;
  const nsString mTag;
  const nsString mIcon;
  const nsString mData;
  const nsString mBehavior;
  const nsString mScope;

public:
  SendNotificationClickEventRunnable(WorkerPrivate* aWorkerPrivate,
                                     KeepAliveToken* aKeepAliveToken,
                                     const nsAString& aID,
                                     const nsAString& aTitle,
                                     const nsAString& aDir,
                                     const nsAString& aLang,
                                     const nsAString& aBody,
                                     const nsAString& aTag,
                                     const nsAString& aIcon,
                                     const nsAString& aData,
                                     const nsAString& aBehavior,
                                     const nsAString& aScope)
      : ExtendableEventWorkerRunnable(aWorkerPrivate, aKeepAliveToken)
      , mID(aID)
      , mTitle(aTitle)
      , mDir(aDir)
      , mLang(aLang)
      , mBody(aBody)
      , mTag(aTag)
      , mIcon(aIcon)
      , mData(aData)
      , mBehavior(aBehavior)
      , mScope(aScope)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);

    RefPtr<EventTarget> target = do_QueryObject(aWorkerPrivate->GlobalScope());

    ErrorResult result;
    RefPtr<Notification> notification =
      Notification::ConstructFromFields(aWorkerPrivate->GlobalScope(), mID,
                                        mTitle, mDir, mLang, mBody, mTag, mIcon,
                                        mData, mScope, result);
    if (NS_WARN_IF(result.Failed())) {
      return false;
    }

    NotificationEventInit nei;
    nei.mNotification = notification;
    nei.mBubbles = false;
    nei.mCancelable = false;

    RefPtr<NotificationEvent> event =
      NotificationEvent::Constructor(target,
                                     NS_LITERAL_STRING("notificationclick"),
                                     nei, result);
    if (NS_WARN_IF(result.Failed())) {
      return false;
    }

    event->SetTrusted(true);
    RefPtr<Promise> waitUntil;
    aWorkerPrivate->GlobalScope()->AllowWindowInteraction();
    DispatchExtendableEventOnWorkerScope(aCx, aWorkerPrivate->GlobalScope(),
                                         event, getter_AddRefs(waitUntil));
      aWorkerPrivate->GlobalScope()->ConsumeWindowInteraction();
    if (waitUntil) {
      RefPtr<AllowWindowInteractionHandler> allowWindowInteraction =
        new AllowWindowInteractionHandler(aWorkerPrivate);
      waitUntil->AppendNativeHandler(allowWindowInteraction);
    }

    return true;
  }
};

} // namespace anonymous

nsresult
ServiceWorkerPrivate::SendNotificationClickEvent(const nsAString& aID,
                                                 const nsAString& aTitle,
                                                 const nsAString& aDir,
                                                 const nsAString& aLang,
                                                 const nsAString& aBody,
                                                 const nsAString& aTag,
                                                 const nsAString& aIcon,
                                                 const nsAString& aData,
                                                 const nsAString& aBehavior,
                                                 const nsAString& aScope)
{
  nsresult rv = SpawnWorkerIfNeeded(NotificationClickEvent, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  gDOMDisableOpenClickDelay = Preferences::GetInt("dom.disable_open_click_delay");

  RefPtr<WorkerRunnable> r =
    new SendNotificationClickEventRunnable(mWorkerPrivate, mKeepAliveToken,
                                           aID, aTitle, aDir, aLang,
                                           aBody, aTag, aIcon, aData,
                                           aBehavior, aScope);
  AutoJSAPI jsapi;
  jsapi.Init();
  if (NS_WARN_IF(!r->Dispatch(jsapi.cx()))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

namespace {

// Inheriting ExtendableEventWorkerRunnable so that the worker is not terminated
// while handling the fetch event, though that's very unlikely.
class FetchEventRunnable : public ExtendableEventWorkerRunnable
                         , public nsIHttpHeaderVisitor {
  nsMainThreadPtrHandle<nsIInterceptedChannel> mInterceptedChannel;
  const nsCString mScriptSpec;
  nsTArray<nsCString> mHeaderNames;
  nsTArray<nsCString> mHeaderValues;
  UniquePtr<ServiceWorkerClientInfo> mClientInfo;
  nsCString mSpec;
  nsCString mMethod;
  bool mIsReload;
  DebugOnly<bool> mIsHttpChannel;
  RequestMode mRequestMode;
  RequestRedirect mRequestRedirect;
  RequestCredentials mRequestCredentials;
  nsContentPolicyType mContentPolicyType;
  nsCOMPtr<nsIInputStream> mUploadStream;
  nsCString mReferrer;
public:
  FetchEventRunnable(WorkerPrivate* aWorkerPrivate,
                     KeepAliveToken* aKeepAliveToken,
                     nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                     // CSP checks might require the worker script spec
                     // later on.
                     const nsACString& aScriptSpec,
                     UniquePtr<ServiceWorkerClientInfo>&& aClientInfo,
                     bool aIsReload)
    : ExtendableEventWorkerRunnable(aWorkerPrivate, aKeepAliveToken)
    , mInterceptedChannel(aChannel)
    , mScriptSpec(aScriptSpec)
    , mClientInfo(Move(aClientInfo))
    , mIsReload(aIsReload)
    , mIsHttpChannel(false)
    , mRequestMode(RequestMode::No_cors)
    , mRequestRedirect(RequestRedirect::Follow)
    // By default we set it to same-origin since normal HTTP fetches always
    // send credentials to same-origin websites unless explicitly forbidden.
    , mRequestCredentials(RequestCredentials::Same_origin)
    , mContentPolicyType(nsIContentPolicy::TYPE_INVALID)
    , mReferrer(kFETCH_CLIENT_REFERRER_STR)
  {
    MOZ_ASSERT(aWorkerPrivate);
  }

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD
  VisitHeader(const nsACString& aHeader, const nsACString& aValue) override
  {
    mHeaderNames.AppendElement(aHeader);
    mHeaderValues.AppendElement(aValue);
    return NS_OK;
  }

  nsresult
  Init()
  {
    AssertIsOnMainThread();
    nsCOMPtr<nsIChannel> channel;
    nsresult rv = mInterceptedChannel->GetChannel(getter_AddRefs(channel));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> uri;
    rv = channel->GetURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = uri->GetSpec(mSpec);
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t loadFlags;
    rv = channel->GetLoadFlags(&loadFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILoadInfo> loadInfo;
    rv = channel->GetLoadInfo(getter_AddRefs(loadInfo));
    NS_ENSURE_SUCCESS(rv, rv);

    mContentPolicyType = loadInfo->InternalContentPolicyType();

    nsCOMPtr<nsIURI> referrerURI;
    rv = NS_GetReferrerFromChannel(channel, getter_AddRefs(referrerURI));
    // We can't bail on failure since certain non-http channels like JAR
    // channels are intercepted but don't have referrers.
    if (NS_SUCCEEDED(rv) && referrerURI) {
      rv = referrerURI->GetSpec(mReferrer);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);
    if (httpChannel) {
      mIsHttpChannel = true;

      rv = httpChannel->GetRequestMethod(mMethod);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIHttpChannelInternal> internalChannel = do_QueryInterface(httpChannel);
      NS_ENSURE_TRUE(internalChannel, NS_ERROR_NOT_AVAILABLE);

      mRequestMode = InternalRequest::MapChannelToRequestMode(channel);

      // This is safe due to static_asserts at top of file.
      uint32_t redirectMode;
      internalChannel->GetRedirectMode(&redirectMode);
      mRequestRedirect = static_cast<RequestRedirect>(redirectMode);

      if (loadFlags & nsIRequest::LOAD_ANONYMOUS) {
        mRequestCredentials = RequestCredentials::Omit;
      } else {
        bool includeCrossOrigin;
        internalChannel->GetCorsIncludeCredentials(&includeCrossOrigin);
        if (includeCrossOrigin) {
          mRequestCredentials = RequestCredentials::Include;
        }
      }

      rv = httpChannel->VisitNonDefaultRequestHeaders(this);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIUploadChannel2> uploadChannel = do_QueryInterface(httpChannel);
      if (uploadChannel) {
        MOZ_ASSERT(!mUploadStream);
        rv = uploadChannel->CloneUploadStream(getter_AddRefs(mUploadStream));
        NS_ENSURE_SUCCESS(rv, rv);
      }
    } else {
      nsCOMPtr<nsIJARChannel> jarChannel = do_QueryInterface(channel);
      // If it is not an HTTP channel it must be a JAR one.
      NS_ENSURE_TRUE(jarChannel, NS_ERROR_NOT_AVAILABLE);

      mMethod = "GET";

      mRequestMode = InternalRequest::MapChannelToRequestMode(channel);

      if (loadFlags & nsIRequest::LOAD_ANONYMOUS) {
        mRequestCredentials = RequestCredentials::Omit;
      }
    }

    return NS_OK;
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    return DispatchFetchEvent(aCx, aWorkerPrivate);
  }

private:
  ~FetchEventRunnable() {}

  class ResumeRequest final : public nsRunnable {
    nsMainThreadPtrHandle<nsIInterceptedChannel> mChannel;
  public:
    explicit ResumeRequest(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel)
      : mChannel(aChannel)
    {
    }

    NS_IMETHOD Run()
    {
      AssertIsOnMainThread();
      nsresult rv = mChannel->ResetInterception();
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to resume intercepted network request");
      return rv;
    }
  };

  bool
  DispatchFetchEvent(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    MOZ_ASSERT(aCx);
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
    GlobalObject globalObj(aCx, aWorkerPrivate->GlobalScope()->GetWrapper());

    NS_ConvertUTF8toUTF16 local(mSpec);
    RequestOrUSVString requestInfo;
    requestInfo.SetAsUSVString().Rebind(local.Data(), local.Length());

    RootedDictionary<RequestInit> reqInit(aCx);
    reqInit.mMethod.Construct(mMethod);

    RefPtr<InternalHeaders> internalHeaders = new InternalHeaders(HeadersGuardEnum::Request);
    MOZ_ASSERT(mHeaderNames.Length() == mHeaderValues.Length());
    for (uint32_t i = 0; i < mHeaderNames.Length(); i++) {
      ErrorResult result;
      internalHeaders->Set(mHeaderNames[i], mHeaderValues[i], result);
      if (NS_WARN_IF(result.Failed())) {
        result.SuppressException();
        return false;
      }
    }

    RefPtr<Headers> headers = new Headers(globalObj.GetAsSupports(), internalHeaders);
    reqInit.mHeaders.Construct();
    reqInit.mHeaders.Value().SetAsHeaders() = headers;

    reqInit.mMode.Construct(mRequestMode);
    reqInit.mRedirect.Construct(mRequestRedirect);
    reqInit.mCredentials.Construct(mRequestCredentials);

    ErrorResult result;
    RefPtr<Request> request = Request::Constructor(globalObj, requestInfo, reqInit, result);
    if (NS_WARN_IF(result.Failed())) {
      result.SuppressException();
      return false;
    }
    // For Telemetry, note that this Request object was created by a Fetch event.
    RefPtr<InternalRequest> internalReq = request->GetInternalRequest();
    MOZ_ASSERT(internalReq);
    internalReq->SetCreatedByFetchEvent();

    internalReq->SetBody(mUploadStream);
    internalReq->SetReferrer(NS_ConvertUTF8toUTF16(mReferrer));

    request->SetContentPolicyType(mContentPolicyType);

    request->GetInternalHeaders()->SetGuard(HeadersGuardEnum::Immutable, result);
    if (NS_WARN_IF(result.Failed())) {
      result.SuppressException();
      return false;
    }

    // TODO: remove conditional on http here once app protocol support is
    //       removed from service worker interception
    MOZ_ASSERT_IF(mIsHttpChannel && internalReq->IsNavigationRequest(),
                  request->Redirect() == RequestRedirect::Manual);

    RootedDictionary<FetchEventInit> init(aCx);
    init.mRequest.Construct();
    init.mRequest.Value() = request;
    init.mBubbles = false;
    init.mCancelable = true;
    init.mIsReload.Construct(mIsReload);
    RefPtr<FetchEvent> event =
      FetchEvent::Constructor(globalObj, NS_LITERAL_STRING("fetch"), init, result);
    if (NS_WARN_IF(result.Failed())) {
      result.SuppressException();
      return false;
    }

    event->PostInit(mInterceptedChannel, mScriptSpec, Move(mClientInfo));
    event->SetTrusted(true);

    RefPtr<EventTarget> target = do_QueryObject(aWorkerPrivate->GlobalScope());
    nsresult rv2 = target->DispatchDOMEvent(nullptr, event, nullptr, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv2)) || !event->WaitToRespond()) {
      nsCOMPtr<nsIRunnable> runnable;
      if (event->DefaultPrevented(aCx)) {
        runnable = new CancelChannelRunnable(mInterceptedChannel, NS_ERROR_INTERCEPTION_CANCELED);
      } else {
        runnable = new ResumeRequest(mInterceptedChannel);
      }

      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(runnable)));
    }

    RefPtr<Promise> respondWithPromise = event->GetPromise();
    if (respondWithPromise) {
      RefPtr<KeepAliveHandler> keepAliveHandler =
        new KeepAliveHandler(mKeepAliveToken);
      respondWithPromise->AppendNativeHandler(keepAliveHandler);
    }
    return true;
  }
};

NS_IMPL_ISUPPORTS_INHERITED(FetchEventRunnable, WorkerRunnable, nsIHttpHeaderVisitor)

} // anonymous namespace

nsresult
ServiceWorkerPrivate::SendFetchEvent(nsIInterceptedChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     UniquePtr<ServiceWorkerClientInfo>&& aClientInfo,
                                     bool aIsReload)
{
  // if the ServiceWorker script fails to load for some reason, just resume
  // the original channel.
  nsCOMPtr<nsIRunnable> failRunnable =
    NS_NewRunnableMethod(aChannel, &nsIInterceptedChannel::ResetInterception);

  nsresult rv = SpawnWorkerIfNeeded(FetchEvent, failRunnable, aLoadGroup);
  NS_ENSURE_SUCCESS(rv, rv);

  nsMainThreadPtrHandle<nsIInterceptedChannel> handle(
    new nsMainThreadPtrHolder<nsIInterceptedChannel>(aChannel, false));

  if (NS_WARN_IF(!mInfo)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<FetchEventRunnable> r =
    new FetchEventRunnable(mWorkerPrivate, mKeepAliveToken, handle,
                           mInfo->ScriptSpec(), Move(aClientInfo), aIsReload);
  rv = r->Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AutoJSAPI jsapi;
  jsapi.Init();
  if (NS_WARN_IF(!r->Dispatch(jsapi.cx()))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
ServiceWorkerPrivate::SpawnWorkerIfNeeded(WakeUpReason aWhy,
                                          nsIRunnable* aLoadFailedRunnable,
                                          nsILoadGroup* aLoadGroup)
{
  AssertIsOnMainThread();

  // XXXcatalinb: We need to have a separate load group that's linked to
  // an existing tab child to pass security checks on b2g.
  // This should be fixed in bug 1125961, but for now we enforce updating
  // the overriden load group when intercepting a fetch.
  MOZ_ASSERT_IF(aWhy == FetchEvent, aLoadGroup);

  if (mWorkerPrivate) {
    mWorkerPrivate->UpdateOverridenLoadGroup(aLoadGroup);
    ResetIdleTimeout(aWhy);

    return NS_OK;
  }

  if (NS_WARN_IF(!mInfo)) {
    NS_WARNING("Trying to wake up a dead service worker.");
    return NS_ERROR_FAILURE;
  }

  // TODO(catalinb): Bug 1192138 - Add telemetry for service worker wake-ups.

  // Ensure that the IndexedDatabaseManager is initialized
  NS_WARN_IF(!indexedDB::IndexedDatabaseManager::GetOrCreate());

  WorkerLoadInfo info;
  nsresult rv = NS_NewURI(getter_AddRefs(info.mBaseURI), mInfo->ScriptSpec(),
                          nullptr, nullptr);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  info.mResolvedScriptURI = info.mBaseURI;
  MOZ_ASSERT(!mInfo->CacheName().IsEmpty());
  info.mServiceWorkerCacheName = mInfo->CacheName();
  info.mServiceWorkerID = mInfo->ID();
  info.mLoadGroup = aLoadGroup;
  info.mLoadFailedAsyncRunnable = aLoadFailedRunnable;

  rv = info.mBaseURI->GetHost(info.mDomain);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  info.mPrincipal = mInfo->GetPrincipal();

  nsContentUtils::StorageAccess access =
    nsContentUtils::StorageAllowedForPrincipal(info.mPrincipal);
  info.mStorageAllowed = access > nsContentUtils::StorageAccess::ePrivateBrowsing;
  info.mPrivateBrowsing = false;

  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = info.mPrincipal->GetCsp(getter_AddRefs(csp));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  info.mCSP = csp;
  if (info.mCSP) {
    rv = info.mCSP->GetAllowsEval(&info.mReportCSPViolations,
                                  &info.mEvalAllowed);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    info.mEvalAllowed = true;
    info.mReportCSPViolations = false;
  }

  WorkerPrivate::OverrideLoadInfoLoadGroup(info);

  AutoJSAPI jsapi;
  jsapi.Init();
  ErrorResult error;
  NS_ConvertUTF8toUTF16 scriptSpec(mInfo->ScriptSpec());

  mWorkerPrivate = WorkerPrivate::Constructor(jsapi.cx(),
                                              scriptSpec,
                                              false, WorkerTypeService,
                                              mInfo->Scope(), &info, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  mIsPushWorker = false;
  ResetIdleTimeout(aWhy);

  return NS_OK;
}

void
ServiceWorkerPrivate::TerminateWorker()
{
  AssertIsOnMainThread();

  mIdleWorkerTimer->Cancel();
  mKeepAliveToken = nullptr;
  if (mWorkerPrivate) {
    if (Preferences::GetBool("dom.serviceWorkers.testing.enabled")) {
      nsCOMPtr<nsIObserverService> os = services::GetObserverService();
      if (os) {
        os->NotifyObservers(this, "service-worker-shutdown", nullptr);
      }
    }

    AutoJSAPI jsapi;
    jsapi.Init();
    NS_WARN_IF(!mWorkerPrivate->Terminate(jsapi.cx()));
    mWorkerPrivate = nullptr;
  }
}

void
ServiceWorkerPrivate::NoteDeadServiceWorkerInfo()
{
  AssertIsOnMainThread();
  mInfo = nullptr;
  TerminateWorker();
}

void
ServiceWorkerPrivate::NoteStoppedControllingDocuments()
{
  AssertIsOnMainThread();
  if (mIsPushWorker) {
    return;
  }

  TerminateWorker();
}

/* static */ void
ServiceWorkerPrivate::NoteIdleWorkerCallback(nsITimer* aTimer, void* aPrivate)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrivate);

  RefPtr<ServiceWorkerPrivate> swp = static_cast<ServiceWorkerPrivate*>(aPrivate);

  MOZ_ASSERT(aTimer == swp->mIdleWorkerTimer, "Invalid timer!");

  // Release ServiceWorkerPrivate's token, since the grace period has ended.
  swp->mKeepAliveToken = nullptr;

  if (swp->mWorkerPrivate) {
    // If we still have a workerPrivate at this point it means there are pending
    // waitUntil promises. Wait a bit more until we forcibly terminate the
    // worker.
    uint32_t timeout = Preferences::GetInt("dom.serviceWorkers.idle_extended_timeout");
    DebugOnly<nsresult> rv =
      swp->mIdleWorkerTimer->InitWithFuncCallback(ServiceWorkerPrivate::TerminateWorkerCallback,
                                                  aPrivate,
                                                  timeout,
                                                  nsITimer::TYPE_ONE_SHOT);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
}

/* static */ void
ServiceWorkerPrivate::TerminateWorkerCallback(nsITimer* aTimer, void *aPrivate)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrivate);

  RefPtr<ServiceWorkerPrivate> serviceWorkerPrivate =
    static_cast<ServiceWorkerPrivate*>(aPrivate);

  MOZ_ASSERT(aTimer == serviceWorkerPrivate->mIdleWorkerTimer,
      "Invalid timer!");

  serviceWorkerPrivate->TerminateWorker();
}

void
ServiceWorkerPrivate::ResetIdleTimeout(WakeUpReason aWhy)
{
  // We should have an active worker if we're reseting the idle timeout
  MOZ_ASSERT(mWorkerPrivate);

  if (aWhy == PushEvent || aWhy == PushSubscriptionChangeEvent) {
    mIsPushWorker = true;
  }

  uint32_t timeout = Preferences::GetInt("dom.serviceWorkers.idle_timeout");
  DebugOnly<nsresult> rv =
    mIdleWorkerTimer->InitWithFuncCallback(ServiceWorkerPrivate::NoteIdleWorkerCallback,
                                           this, timeout,
                                           nsITimer::TYPE_ONE_SHOT);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (!mKeepAliveToken) {
    mKeepAliveToken = new KeepAliveToken(this);
  }
}

void
ServiceWorkerPrivate::AddToken()
{
  AssertIsOnMainThread();
  ++mTokenCount;
}

void
ServiceWorkerPrivate::ReleaseToken()
{
  AssertIsOnMainThread();

  MOZ_ASSERT(mTokenCount > 0);
  --mTokenCount;
  if (!mTokenCount) {
    TerminateWorker();
  }
}

END_WORKERS_NAMESPACE
