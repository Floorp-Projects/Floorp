/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerPrivate.h"

#include <utility>

#include "ServiceWorkerCloneData.h"
#include "ServiceWorkerManager.h"
#include "ServiceWorkerPrivateImpl.h"
#include "ServiceWorkerUtils.h"
#include "nsContentUtils.h"
#include "nsICacheInfoChannel.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsINamed.h"
#include "nsINetworkInterceptController.h"
#include "nsIPushErrorReporter.h"
#include "nsISupportsImpl.h"
#include "nsIUploadChannel2.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsQueryObject.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "mozilla/Assertions.h"
#include "mozilla/CycleCollectedJSContext.h"  // for MicroTaskRunnable
#include "mozilla/JSObjectHolder.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Client.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/FetchUtil.h"
#include "mozilla/dom/IndexedDatabaseManager.h"
#include "mozilla/dom/InternalHeaders.h"
#include "mozilla/dom/NotificationEvent.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/PushEventBinding.h"
#include "mozilla/dom/RequestBinding.h"
#include "mozilla/dom/WorkerDebugger.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "mozilla/Services.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "mozilla/Telemetry.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/Unused.h"
#include "nsIReferrerInfo.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {
namespace dom {

using mozilla::ipc::PrincipalInfo;

NS_IMPL_CYCLE_COLLECTING_NATIVE_ADDREF(ServiceWorkerPrivate)
NS_IMPL_CYCLE_COLLECTING_NATIVE_RELEASE(ServiceWorkerPrivate)
NS_IMPL_CYCLE_COLLECTION(ServiceWorkerPrivate, mSupportsArray)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(ServiceWorkerPrivate, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(ServiceWorkerPrivate, Release)

// Tracks the "dom.serviceWorkers.disable_open_click_delay" preference. Modified
// on main thread, read on worker threads.
// It is updated every time a "notificationclick" event is dispatched. While
// this is done without synchronization, at the worst, the thread will just get
// an older value within which a popup is allowed to be displayed, which will
// still be a valid value since it was set prior to dispatching the runnable.
Atomic<uint32_t> gDOMDisableOpenClickDelay(0);

KeepAliveToken::KeepAliveToken(ServiceWorkerPrivate* aPrivate)
    : mPrivate(aPrivate) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrivate);
  mPrivate->AddToken();
}

KeepAliveToken::~KeepAliveToken() {
  MOZ_ASSERT(NS_IsMainThread());
  mPrivate->ReleaseToken();
}

NS_IMPL_ISUPPORTS0(KeepAliveToken)

ServiceWorkerPrivate::ServiceWorkerPrivate(ServiceWorkerInfo* aInfo)
    : mInfo(aInfo), mDebuggerCount(0), mTokenCount(0) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aInfo);

  mIdleWorkerTimer = NS_NewTimer();
  MOZ_ASSERT(mIdleWorkerTimer);

  RefPtr<ServiceWorkerPrivateImpl> inner = new ServiceWorkerPrivateImpl(this);

  // Assert in all debug builds as well as non-debug Nightly and Dev Edition.
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(inner->Initialize()));
#else
  MOZ_ALWAYS_SUCCEEDS(inner->Initialize());
#endif

  mInner = std::move(inner);
}

ServiceWorkerPrivate::~ServiceWorkerPrivate() {
  MOZ_ASSERT(!mWorkerPrivate);
  MOZ_ASSERT(!mTokenCount);
  MOZ_ASSERT(!mInner);
  MOZ_ASSERT(!mInfo);
  MOZ_ASSERT(mSupportsArray.IsEmpty());
  MOZ_ASSERT(mIdlePromiseHolder.IsEmpty());

  mIdleWorkerTimer->Cancel();
}

namespace {

class CheckScriptEvaluationWithCallback final : public WorkerDebuggeeRunnable {
  nsMainThreadPtrHandle<ServiceWorkerPrivate> mServiceWorkerPrivate;
  nsMainThreadPtrHandle<KeepAliveToken> mKeepAliveToken;

  // The script evaluation result must be reported even if the runnable
  // is cancelled.
  RefPtr<LifeCycleEventCallback> mScriptEvaluationCallback;

#ifdef DEBUG
  bool mDone;
#endif

 public:
  CheckScriptEvaluationWithCallback(
      WorkerPrivate* aWorkerPrivate,
      ServiceWorkerPrivate* aServiceWorkerPrivate,
      KeepAliveToken* aKeepAliveToken,
      LifeCycleEventCallback* aScriptEvaluationCallback)
      : WorkerDebuggeeRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount),
        mServiceWorkerPrivate(new nsMainThreadPtrHolder<ServiceWorkerPrivate>(
            "CheckScriptEvaluationWithCallback::mServiceWorkerPrivate",
            aServiceWorkerPrivate)),
        mKeepAliveToken(new nsMainThreadPtrHolder<KeepAliveToken>(
            "CheckScriptEvaluationWithCallback::mKeepAliveToken",
            aKeepAliveToken)),
        mScriptEvaluationCallback(aScriptEvaluationCallback)
#ifdef DEBUG
        ,
        mDone(false)
#endif
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  ~CheckScriptEvaluationWithCallback() { MOZ_ASSERT(mDone); }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->AssertIsOnWorkerThread();

    bool fetchHandlerWasAdded = aWorkerPrivate->FetchHandlerWasAdded();
    nsCOMPtr<nsIRunnable> runnable = NewRunnableMethod<bool>(
        "dom::CheckScriptEvaluationWithCallback::ReportFetchFlag", this,
        &CheckScriptEvaluationWithCallback::ReportFetchFlag,
        fetchHandlerWasAdded);
    aWorkerPrivate->DispatchToMainThread(runnable.forget());

    ReportScriptEvaluationResult(
        aWorkerPrivate->WorkerScriptExecutedSuccessfully());

    return true;
  }

  void ReportFetchFlag(bool aFetchHandlerWasAdded) {
    MOZ_ASSERT(NS_IsMainThread());
    mServiceWorkerPrivate->SetHandlesFetch(aFetchHandlerWasAdded);
  }

  nsresult Cancel() override {
    ReportScriptEvaluationResult(false);
    return WorkerRunnable::Cancel();
  }

 private:
  void ReportScriptEvaluationResult(bool aScriptEvaluationResult) {
#ifdef DEBUG
    mDone = true;
#endif
    mScriptEvaluationCallback->SetResult(aScriptEvaluationResult);
    MOZ_ALWAYS_SUCCEEDS(
        mWorkerPrivate->DispatchToMainThread(mScriptEvaluationCallback));
  }
};

}  // anonymous namespace

nsresult ServiceWorkerPrivate::CheckScriptEvaluation(
    LifeCycleEventCallback* aScriptEvaluationCallback) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mInner) {
    return mInner->CheckScriptEvaluation(aScriptEvaluationCallback);
  }

  nsresult rv = SpawnWorkerIfNeeded(LifeCycleEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<KeepAliveToken> token = CreateEventKeepAliveToken();
  RefPtr<WorkerRunnable> r = new CheckScriptEvaluationWithCallback(
      mWorkerPrivate, this, token, aScriptEvaluationCallback);
  if (NS_WARN_IF(!r->Dispatch())) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

namespace {

class KeepAliveHandler final : public ExtendableEvent::ExtensionsHandler,
                               public PromiseNativeHandler {
  // This class manages lifetime extensions added by calling WaitUntil()
  // or RespondWith(). We allow new extensions as long as we still hold
  // |mKeepAliveToken|. Once the last promise was settled, we queue a microtask
  // which releases the token and prevents further extensions. By doing this,
  // we give other pending microtasks a chance to continue adding extensions.

  RefPtr<StrongWorkerRef> mWorkerRef;
  nsMainThreadPtrHandle<KeepAliveToken> mKeepAliveToken;

  // We start holding a self reference when the first extension promise is
  // added. As far as I can tell, the only case where this is useful is when
  // we're waiting indefinitely on a promise that's no longer reachable
  // and will never be settled.
  // The cycle is broken when the last promise was settled or when the
  // worker is shutting down.
  RefPtr<KeepAliveHandler> mSelfRef;

  // Called when the last promise was settled.
  RefPtr<ExtendableEventCallback> mCallback;

  uint32_t mPendingPromisesCount;

  // We don't actually care what values the promises resolve to, only whether
  // any of them were rejected.
  bool mRejected;

 public:
  NS_DECL_ISUPPORTS

  explicit KeepAliveHandler(
      const nsMainThreadPtrHandle<KeepAliveToken>& aKeepAliveToken,
      ExtendableEventCallback* aCallback)
      : mKeepAliveToken(aKeepAliveToken),
        mCallback(aCallback),
        mPendingPromisesCount(0),
        mRejected(false) {
    MOZ_ASSERT(mKeepAliveToken);
  }

  bool Init() {
    MOZ_ASSERT(IsCurrentThreadRunningWorker());

    RefPtr<KeepAliveHandler> self = this;
    mWorkerRef = StrongWorkerRef::Create(GetCurrentThreadWorkerPrivate(),
                                         "KeepAliveHandler",
                                         [self]() { self->MaybeCleanup(); });

    if (NS_WARN_IF(!mWorkerRef)) {
      return false;
    }

    return true;
  }

  bool WaitOnPromise(Promise& aPromise) override {
    if (!mKeepAliveToken) {
      MOZ_ASSERT(!GetDispatchFlag());
      MOZ_ASSERT(!mSelfRef, "We shouldn't be holding a self reference!");
      return false;
    }
    if (!mSelfRef) {
      MOZ_ASSERT(!mPendingPromisesCount);
      mSelfRef = this;
    }

    ++mPendingPromisesCount;
    aPromise.AppendNativeHandler(this);

    return true;
  }

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    RemovePromise(Resolved);
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    RemovePromise(Rejected);
  }

  void MaybeDone() {
    MOZ_ASSERT(IsCurrentThreadRunningWorker());
    MOZ_ASSERT(!GetDispatchFlag());

    if (mPendingPromisesCount || !mKeepAliveToken) {
      return;
    }
    if (mCallback) {
      mCallback->FinishedWithResult(mRejected ? Rejected : Resolved);
    }

    MaybeCleanup();
  }

 private:
  ~KeepAliveHandler() { MaybeCleanup(); }

  void MaybeCleanup() {
    MOZ_ASSERT(IsCurrentThreadRunningWorker());

    if (!mKeepAliveToken) {
      return;
    }

    mWorkerRef = nullptr;
    mKeepAliveToken = nullptr;
    mSelfRef = nullptr;
  }

  class MaybeDoneRunner : public MicroTaskRunnable {
   public:
    explicit MaybeDoneRunner(KeepAliveHandler* aHandler) : mHandler(aHandler) {}
    virtual void Run(AutoSlowOperation& aAso) override {
      mHandler->MaybeDone();
    }

    RefPtr<KeepAliveHandler> mHandler;
  };

  void RemovePromise(ExtendableEventResult aResult) {
    MOZ_ASSERT(IsCurrentThreadRunningWorker());
    MOZ_DIAGNOSTIC_ASSERT(mPendingPromisesCount > 0);

    // Note: mSelfRef and mKeepAliveToken can be nullptr here
    //       if MaybeCleanup() was called just before a promise
    //       settled.  This can happen, for example, if the
    //       worker thread is being terminated for running too
    //       long, browser shutdown, etc.

    mRejected |= (aResult == Rejected);

    --mPendingPromisesCount;
    if (mPendingPromisesCount || GetDispatchFlag()) {
      return;
    }

    CycleCollectedJSContext* cx = CycleCollectedJSContext::Get();
    MOZ_ASSERT(cx);

    RefPtr<MaybeDoneRunner> r = new MaybeDoneRunner(this);
    cx->DispatchToMicroTask(r.forget());
  }
};

NS_IMPL_ISUPPORTS0(KeepAliveHandler)

class RegistrationUpdateRunnable : public Runnable {
  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> mRegistration;
  const bool mNeedTimeCheck;

 public:
  RegistrationUpdateRunnable(
      nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration,
      bool aNeedTimeCheck)
      : Runnable("dom::RegistrationUpdateRunnable"),
        mRegistration(aRegistration),
        mNeedTimeCheck(aNeedTimeCheck) {
    MOZ_DIAGNOSTIC_ASSERT(mRegistration);
  }

  NS_IMETHOD
  Run() override {
    if (mNeedTimeCheck) {
      mRegistration->MaybeScheduleTimeCheckAndUpdate();
    } else {
      mRegistration->MaybeScheduleUpdate();
    }
    return NS_OK;
  }
};

class ExtendableEventWorkerRunnable : public WorkerRunnable {
 protected:
  nsMainThreadPtrHandle<KeepAliveToken> mKeepAliveToken;

 public:
  ExtendableEventWorkerRunnable(WorkerPrivate* aWorkerPrivate,
                                KeepAliveToken* aKeepAliveToken)
      : WorkerRunnable(aWorkerPrivate) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aKeepAliveToken);

    mKeepAliveToken = new nsMainThreadPtrHolder<KeepAliveToken>(
        "ExtendableEventWorkerRunnable::mKeepAliveToken", aKeepAliveToken);
  }

  nsresult DispatchExtendableEventOnWorkerScope(
      JSContext* aCx, WorkerGlobalScope* aWorkerScope, ExtendableEvent* aEvent,
      ExtendableEventCallback* aCallback) {
    MOZ_ASSERT(aWorkerScope);
    MOZ_ASSERT(aEvent);
    nsCOMPtr<nsIGlobalObject> sgo = aWorkerScope;
    WidgetEvent* internalEvent = aEvent->WidgetEventPtr();

    RefPtr<KeepAliveHandler> keepAliveHandler =
        new KeepAliveHandler(mKeepAliveToken, aCallback);
    if (NS_WARN_IF(!keepAliveHandler->Init())) {
      return NS_ERROR_FAILURE;
    }

    // This must always be set *before* dispatching the event, otherwise
    // waitUntil calls will fail.
    aEvent->SetKeepAliveHandler(keepAliveHandler);

    ErrorResult result;
    aWorkerScope->DispatchEvent(*aEvent, result);
    if (NS_WARN_IF(result.Failed())) {
      result.SuppressException();
      return NS_ERROR_FAILURE;
    }

    // [[ If e’s extend lifetime promises is empty, unset e’s extensions allowed
    //    flag and abort these steps. ]]
    keepAliveHandler->MaybeDone();

    // We don't block the event when getting an exception but still report the
    // error message.
    // Report exception message. Note: This will not stop the event.
    if (internalEvent->mFlags.mExceptionWasRaised) {
      result.SuppressException();
      return NS_ERROR_XPC_JS_THREW_EXCEPTION;
    }

    return NS_OK;
  }
};

class SendMessageEventRunnable final : public ExtendableEventWorkerRunnable {
  const ClientInfoAndState mClientInfoAndState;
  RefPtr<ServiceWorkerCloneData> mData;

 public:
  SendMessageEventRunnable(WorkerPrivate* aWorkerPrivate,
                           KeepAliveToken* aKeepAliveToken,
                           const ClientInfoAndState& aClientInfoAndState,
                           RefPtr<ServiceWorkerCloneData>&& aData)
      : ExtendableEventWorkerRunnable(aWorkerPrivate, aKeepAliveToken),
        mClientInfoAndState(aClientInfoAndState),
        mData(std::move(aData)) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_DIAGNOSTIC_ASSERT(mData);
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    JS::Rooted<JS::Value> messageData(aCx);
    nsCOMPtr<nsIGlobalObject> sgo = aWorkerPrivate->GlobalScope();
    ErrorResult rv;
    mData->Read(aCx, &messageData, rv);

    // If deserialization fails, we will fire a messageerror event
    bool deserializationFailed = rv.Failed();

    if (!deserializationFailed && NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      return true;
    }

    Sequence<OwningNonNull<MessagePort>> ports;
    if (!mData->TakeTransferredPortsAsSequence(ports)) {
      return true;
    }

    RootedDictionary<ExtendableMessageEventInit> init(aCx);

    init.mBubbles = false;
    init.mCancelable = false;

    // On a messageerror event, we disregard ports:
    // https://w3c.github.io/ServiceWorker/#service-worker-postmessage
    if (!deserializationFailed) {
      init.mData = messageData;
      init.mPorts = std::move(ports);
    }

    init.mSource.SetValue().SetAsClient() =
        new Client(sgo, mClientInfoAndState);

    rv.SuppressException();
    RefPtr<EventTarget> target = aWorkerPrivate->GlobalScope();
    RefPtr<ExtendableMessageEvent> extendableEvent =
        ExtendableMessageEvent::Constructor(
            target, deserializationFailed ? u"messageerror"_ns : u"message"_ns,
            init);

    extendableEvent->SetTrusted(true);

    return NS_SUCCEEDED(DispatchExtendableEventOnWorkerScope(
        aCx, aWorkerPrivate->GlobalScope(), extendableEvent, nullptr));
  }
};

}  // anonymous namespace

nsresult ServiceWorkerPrivate::SendMessageEvent(
    RefPtr<ServiceWorkerCloneData>&& aData,
    const ClientInfoAndState& aClientInfoAndState) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mInner) {
    return mInner->SendMessageEvent(std::move(aData), aClientInfoAndState);
  }

  nsresult rv = SpawnWorkerIfNeeded(MessageEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<KeepAliveToken> token = CreateEventKeepAliveToken();
  RefPtr<SendMessageEventRunnable> runnable = new SendMessageEventRunnable(
      mWorkerPrivate, token, aClientInfoAndState, std::move(aData));

  if (!runnable->Dispatch()) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

namespace {

// Handle functional event
// 9.9.7 If the time difference in seconds calculated by the current time minus
// registration's last update check time is greater than 86400, invoke Soft
// Update algorithm.
class ExtendableFunctionalEventWorkerRunnable
    : public ExtendableEventWorkerRunnable {
 protected:
  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> mRegistration;

 public:
  ExtendableFunctionalEventWorkerRunnable(
      WorkerPrivate* aWorkerPrivate, KeepAliveToken* aKeepAliveToken,
      nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration)
      : ExtendableEventWorkerRunnable(aWorkerPrivate, aKeepAliveToken),
        mRegistration(aRegistration) {
    MOZ_DIAGNOSTIC_ASSERT(aRegistration);
  }

  void PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aRunResult) override {
    // Sub-class PreRun() or WorkerRun() methods could clear our mRegistration.
    if (mRegistration) {
      nsCOMPtr<nsIRunnable> runnable =
          new RegistrationUpdateRunnable(mRegistration, true /* time check */);
      aWorkerPrivate->DispatchToMainThread(runnable.forget());
    }

    ExtendableEventWorkerRunnable::PostRun(aCx, aWorkerPrivate, aRunResult);
  }
};

/*
 * Fires 'install' event on the ServiceWorkerGlobalScope. Modifies busy count
 * since it fires the event. This is ok since there can't be nested
 * ServiceWorkers, so the parent thread -> worker thread requirement for
 * runnables is satisfied.
 */
class LifecycleEventWorkerRunnable : public ExtendableEventWorkerRunnable {
  nsString mEventName;
  RefPtr<LifeCycleEventCallback> mCallback;

 public:
  LifecycleEventWorkerRunnable(WorkerPrivate* aWorkerPrivate,
                               KeepAliveToken* aToken,
                               const nsAString& aEventName,
                               LifeCycleEventCallback* aCallback)
      : ExtendableEventWorkerRunnable(aWorkerPrivate, aToken),
        mEventName(aEventName),
        mCallback(aCallback) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    MOZ_ASSERT(aWorkerPrivate);
    return DispatchLifecycleEvent(aCx, aWorkerPrivate);
  }

  nsresult Cancel() override {
    mCallback->SetResult(false);
    MOZ_ALWAYS_SUCCEEDS(mWorkerPrivate->DispatchToMainThread(mCallback));

    return WorkerRunnable::Cancel();
  }

 private:
  bool DispatchLifecycleEvent(JSContext* aCx, WorkerPrivate* aWorkerPrivate);
};

/*
 * Used to handle ExtendableEvent::waitUntil() and catch abnormal worker
 * termination during the execution of life cycle events. It is responsible
 * with advancing the job queue for install/activate tasks.
 */
class LifeCycleEventWatcher final : public ExtendableEventCallback {
  RefPtr<StrongWorkerRef> mWorkerRef;
  RefPtr<LifeCycleEventCallback> mCallback;

  ~LifeCycleEventWatcher() {
    // XXXcatalinb: If all the promises passed to waitUntil go out of scope,
    // the resulting Promise.all will be cycle collected and it will drop its
    // native handlers (including this object). Instead of waiting for a timeout
    // we report the failure now.
    ReportResult(false);
  }

 public:
  NS_INLINE_DECL_REFCOUNTING(LifeCycleEventWatcher, override)

  explicit LifeCycleEventWatcher(LifeCycleEventCallback* aCallback)
      : mCallback(aCallback) {
    MOZ_ASSERT(IsCurrentThreadRunningWorker());
  }

  bool Init() {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    // We need to listen for worker termination in case the event handler
    // never completes or never resolves the waitUntil promise. There are
    // two possible scenarios:
    // 1. The keepAlive token expires and the worker is terminated, in which
    //    case the registration/update promise will be rejected
    // 2. A new service worker is registered which will terminate the current
    //    installing worker.
    RefPtr<LifeCycleEventWatcher> self = this;
    mWorkerRef =
        StrongWorkerRef::Create(workerPrivate, "LifeCycleEventWatcher",
                                [self]() { self->ReportResult(false); });
    if (NS_WARN_IF(!mWorkerRef)) {
      mCallback->SetResult(false);
      // Using DispatchToMainThreadForMessaging so that state update on
      // the main thread doesn't happen too soon.
      nsresult rv = workerPrivate->DispatchToMainThreadForMessaging(mCallback);
      Unused << NS_WARN_IF(NS_FAILED(rv));
      return false;
    }

    return true;
  }

  void ReportResult(bool aResult) {
    MOZ_ASSERT(IsCurrentThreadRunningWorker());

    if (!mWorkerRef) {
      return;
    }

    mCallback->SetResult(aResult);
    // Using DispatchToMainThreadForMessaging so that state update on
    // the main thread doesn't happen too soon.
    nsresult rv =
        mWorkerRef->Private()->DispatchToMainThreadForMessaging(mCallback);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      MOZ_CRASH("Failed to dispatch life cycle event handler.");
    }

    mWorkerRef = nullptr;
  }

  void FinishedWithResult(ExtendableEventResult aResult) override {
    MOZ_ASSERT(IsCurrentThreadRunningWorker());
    ReportResult(aResult == Resolved);

    // Note, all WaitUntil() rejections are reported to client consoles
    // by the WaitUntilHandler in ServiceWorkerEvents.  This ensures that
    // errors in non-lifecycle events like FetchEvent and PushEvent are
    // reported properly.
  }
};

bool LifecycleEventWorkerRunnable::DispatchLifecycleEvent(
    JSContext* aCx, WorkerPrivate* aWorkerPrivate) {
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

  // It is important to initialize the watcher before actually dispatching
  // the event in order to catch worker termination while the event handler
  // is still executing. This can happen with infinite loops, for example.
  RefPtr<LifeCycleEventWatcher> watcher = new LifeCycleEventWatcher(mCallback);

  if (!watcher->Init()) {
    return true;
  }

  nsresult rv = DispatchExtendableEventOnWorkerScope(
      aCx, aWorkerPrivate->GlobalScope(), event, watcher);
  // Do not fail event processing when an exception is thrown.
  if (NS_FAILED(rv) && rv != NS_ERROR_XPC_JS_THREW_EXCEPTION) {
    watcher->ReportResult(false);
  }

  return true;
}

}  // anonymous namespace

nsresult ServiceWorkerPrivate::SendLifeCycleEvent(
    const nsAString& aEventType, LifeCycleEventCallback* aCallback) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mInner) {
    return mInner->SendLifeCycleEvent(aEventType, aCallback);
  }

  nsresult rv = SpawnWorkerIfNeeded(LifeCycleEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<KeepAliveToken> token = CreateEventKeepAliveToken();
  RefPtr<WorkerRunnable> r = new LifecycleEventWorkerRunnable(
      mWorkerPrivate, token, aEventType, aCallback);
  if (NS_WARN_IF(!r->Dispatch())) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

namespace {

class PushErrorReporter final : public ExtendableEventCallback {
  WorkerPrivate* mWorkerPrivate;
  nsString mMessageId;

  ~PushErrorReporter() = default;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PushErrorReporter, override)

  PushErrorReporter(WorkerPrivate* aWorkerPrivate, const nsAString& aMessageId)
      : mWorkerPrivate(aWorkerPrivate), mMessageId(aMessageId) {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  void FinishedWithResult(ExtendableEventResult aResult) override {
    if (aResult == Rejected) {
      Report(nsIPushErrorReporter::DELIVERY_UNHANDLED_REJECTION);
    }
  }

  void Report(
      uint16_t aReason = nsIPushErrorReporter::DELIVERY_INTERNAL_ERROR) {
    WorkerPrivate* workerPrivate = mWorkerPrivate;
    mWorkerPrivate->AssertIsOnWorkerThread();

    if (NS_WARN_IF(aReason > nsIPushErrorReporter::DELIVERY_INTERNAL_ERROR) ||
        mMessageId.IsEmpty()) {
      return;
    }
    nsCOMPtr<nsIRunnable> runnable = NewRunnableMethod<uint16_t>(
        "dom::PushErrorReporter::ReportOnMainThread", this,
        &PushErrorReporter::ReportOnMainThread, aReason);
    MOZ_ALWAYS_TRUE(
        NS_SUCCEEDED(workerPrivate->DispatchToMainThread(runnable.forget())));
  }

  void ReportOnMainThread(uint16_t aReason) {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIPushErrorReporter> reporter =
        do_GetService("@mozilla.org/push/Service;1");
    if (reporter) {
      nsresult rv = reporter->ReportDeliveryError(mMessageId, aReason);
      Unused << NS_WARN_IF(NS_FAILED(rv));
    }
  }
};

class SendPushEventRunnable final
    : public ExtendableFunctionalEventWorkerRunnable {
  nsString mMessageId;
  Maybe<nsTArray<uint8_t>> mData;

 public:
  SendPushEventRunnable(
      WorkerPrivate* aWorkerPrivate, KeepAliveToken* aKeepAliveToken,
      const nsAString& aMessageId, const Maybe<nsTArray<uint8_t>>& aData,
      nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> aRegistration)
      : ExtendableFunctionalEventWorkerRunnable(aWorkerPrivate, aKeepAliveToken,
                                                aRegistration),
        mMessageId(aMessageId),
        mData(aData ? Some(aData->Clone()) : Nothing()) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    MOZ_ASSERT(aWorkerPrivate);
    GlobalObject globalObj(aCx, aWorkerPrivate->GlobalScope()->GetWrapper());

    RefPtr<PushErrorReporter> errorReporter =
        new PushErrorReporter(aWorkerPrivate, mMessageId);

    RootedDictionary<PushEventInit> pei(aCx);
    if (mData) {
      const nsTArray<uint8_t>& bytes = mData.ref();
      JSObject* data =
          Uint8Array::Create(aCx, bytes.Length(), bytes.Elements());
      if (!data) {
        errorReporter->Report();
        return false;
      }
      pei.mData.Construct().SetAsArrayBufferView().Init(data);
    }
    pei.mBubbles = false;
    pei.mCancelable = false;

    ErrorResult result;
    RefPtr<PushEvent> event =
        PushEvent::Constructor(globalObj, u"push"_ns, pei, result);
    if (NS_WARN_IF(result.Failed())) {
      result.SuppressException();
      errorReporter->Report();
      return false;
    }
    event->SetTrusted(true);

    nsresult rv = DispatchExtendableEventOnWorkerScope(
        aCx, aWorkerPrivate->GlobalScope(), event, errorReporter);
    if (NS_FAILED(rv)) {
      // We don't cancel WorkerPrivate when catching an excetpion.
      errorReporter->Report(nsIPushErrorReporter::DELIVERY_UNCAUGHT_EXCEPTION);
    }

    return true;
  }
};

class SendPushSubscriptionChangeEventRunnable final
    : public ExtendableEventWorkerRunnable {
 public:
  explicit SendPushSubscriptionChangeEventRunnable(
      WorkerPrivate* aWorkerPrivate, KeepAliveToken* aKeepAliveToken)
      : ExtendableEventWorkerRunnable(aWorkerPrivate, aKeepAliveToken) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    MOZ_ASSERT(aWorkerPrivate);

    RefPtr<EventTarget> target = aWorkerPrivate->GlobalScope();

    ExtendableEventInit init;
    init.mBubbles = false;
    init.mCancelable = false;

    RefPtr<ExtendableEvent> event = ExtendableEvent::Constructor(
        target, u"pushsubscriptionchange"_ns, init);

    event->SetTrusted(true);

    DispatchExtendableEventOnWorkerScope(aCx, aWorkerPrivate->GlobalScope(),
                                         event, nullptr);

    return true;
  }
};

}  // anonymous namespace

nsresult ServiceWorkerPrivate::SendPushEvent(
    const nsAString& aMessageId, const Maybe<nsTArray<uint8_t>>& aData,
    ServiceWorkerRegistrationInfo* aRegistration) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mInner) {
    return mInner->SendPushEvent(aRegistration, aMessageId, aData);
  }

  nsresult rv = SpawnWorkerIfNeeded(PushEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<KeepAliveToken> token = CreateEventKeepAliveToken();

  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> regInfo(
      new nsMainThreadPtrHolder<ServiceWorkerRegistrationInfo>(
          "ServiceWorkerRegistrationInfoProxy", aRegistration, false));

  RefPtr<WorkerRunnable> r = new SendPushEventRunnable(
      mWorkerPrivate, token, aMessageId, aData, regInfo);

  if (mInfo->State() == ServiceWorkerState::Activating) {
    mPendingFunctionalEvents.AppendElement(r.forget());
    return NS_OK;
  }

  MOZ_ASSERT(mInfo->State() == ServiceWorkerState::Activated);

  if (NS_WARN_IF(!r->Dispatch())) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult ServiceWorkerPrivate::SendPushSubscriptionChangeEvent() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mInner) {
    return mInner->SendPushSubscriptionChangeEvent();
  }

  nsresult rv = SpawnWorkerIfNeeded(PushSubscriptionChangeEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<KeepAliveToken> token = CreateEventKeepAliveToken();
  RefPtr<WorkerRunnable> r =
      new SendPushSubscriptionChangeEventRunnable(mWorkerPrivate, token);
  if (NS_WARN_IF(!r->Dispatch())) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

namespace {

class AllowWindowInteractionHandler final : public ExtendableEventCallback,
                                            public nsITimerCallback,
                                            public nsINamed {
  nsCOMPtr<nsITimer> mTimer;
  RefPtr<StrongWorkerRef> mWorkerRef;

  ~AllowWindowInteractionHandler() {
    // We must either fail to initialize or call ClearWindowAllowed.
    MOZ_DIAGNOSTIC_ASSERT(!mTimer);
    MOZ_DIAGNOSTIC_ASSERT(!mWorkerRef);
  }

  void ClearWindowAllowed(WorkerPrivate* aWorkerPrivate) {
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

    mWorkerRef = nullptr;
  }

  void StartClearWindowTimer(WorkerPrivate* aWorkerPrivate) {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(!mTimer);

    nsresult rv;
    nsCOMPtr<nsITimer> timer =
        NS_NewTimer(aWorkerPrivate->ControlEventTarget());
    if (NS_WARN_IF(!timer)) {
      return;
    }

    MOZ_ASSERT(!mWorkerRef);
    RefPtr<AllowWindowInteractionHandler> self = this;
    mWorkerRef = StrongWorkerRef::Create(
        aWorkerPrivate, "AllowWindowInteractionHandler", [self]() {
          // We could try to hold the worker alive until the timer fires, but
          // other APIs are not likely to work in this partially shutdown state.
          // We might as well let the worker thread exit.
          self->ClearWindowAllowed(self->mWorkerRef->Private());
        });

    if (!mWorkerRef) {
      return;
    }

    aWorkerPrivate->GlobalScope()->AllowWindowInteraction();
    timer.swap(mTimer);

    // We swap first and then initialize the timer so that even if initializing
    // fails, we still clean the busy count and interaction count correctly.
    // The timer can't be initialized before modifying the busy count since the
    // timer thread could run and call the timeout but the worker may
    // already be terminating and modifying the busy count could fail.
    rv = mTimer->InitWithCallback(this, gDOMDisableOpenClickDelay,
                                  nsITimer::TYPE_ONE_SHOT);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      ClearWindowAllowed(aWorkerPrivate);
      return;
    }
  }

  // nsITimerCallback virtual methods
  NS_IMETHOD
  Notify(nsITimer* aTimer) override {
    MOZ_DIAGNOSTIC_ASSERT(mTimer == aTimer);
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    ClearWindowAllowed(workerPrivate);
    return NS_OK;
  }

  // nsINamed virtual methods
  NS_IMETHOD
  GetName(nsACString& aName) override {
    aName.AssignLiteral("AllowWindowInteractionHandler");
    return NS_OK;
  }

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit AllowWindowInteractionHandler(WorkerPrivate* aWorkerPrivate) {
    StartClearWindowTimer(aWorkerPrivate);
  }

  void FinishedWithResult(ExtendableEventResult /* aResult */) override {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    ClearWindowAllowed(workerPrivate);
  }
};

NS_IMPL_ISUPPORTS(AllowWindowInteractionHandler, nsITimerCallback, nsINamed)

class SendNotificationEventRunnable final
    : public ExtendableEventWorkerRunnable {
  const nsString mEventName;
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
  SendNotificationEventRunnable(WorkerPrivate* aWorkerPrivate,
                                KeepAliveToken* aKeepAliveToken,
                                const nsAString& aEventName,
                                const nsAString& aID, const nsAString& aTitle,
                                const nsAString& aDir, const nsAString& aLang,
                                const nsAString& aBody, const nsAString& aTag,
                                const nsAString& aIcon, const nsAString& aData,
                                const nsAString& aBehavior,
                                const nsAString& aScope)
      : ExtendableEventWorkerRunnable(aWorkerPrivate, aKeepAliveToken),
        mEventName(aEventName),
        mID(aID),
        mTitle(aTitle),
        mDir(aDir),
        mLang(aLang),
        mBody(aBody),
        mTag(aTag),
        mIcon(aIcon),
        mData(aData),
        mBehavior(aBehavior),
        mScope(aScope) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    MOZ_ASSERT(aWorkerPrivate);

    RefPtr<EventTarget> target = do_QueryObject(aWorkerPrivate->GlobalScope());

    ErrorResult result;
    RefPtr<Notification> notification = Notification::ConstructFromFields(
        aWorkerPrivate->GlobalScope(), mID, mTitle, mDir, mLang, mBody, mTag,
        mIcon, mData, mScope, result);
    if (NS_WARN_IF(result.Failed())) {
      return false;
    }

    NotificationEventInit nei;
    nei.mNotification = notification;
    nei.mBubbles = false;
    nei.mCancelable = false;

    RefPtr<NotificationEvent> event =
        NotificationEvent::Constructor(target, mEventName, nei);

    event->SetTrusted(true);

    RefPtr<AllowWindowInteractionHandler> allowWindowInteraction;
    if (mEventName.EqualsLiteral(NOTIFICATION_CLICK_EVENT_NAME)) {
      allowWindowInteraction =
          new AllowWindowInteractionHandler(aWorkerPrivate);
    }

    nsresult rv = DispatchExtendableEventOnWorkerScope(
        aCx, aWorkerPrivate->GlobalScope(), event, allowWindowInteraction);
    // Don't reject when catching an exception
    if (NS_FAILED(rv) && rv != NS_ERROR_XPC_JS_THREW_EXCEPTION &&
        allowWindowInteraction) {
      allowWindowInteraction->FinishedWithResult(Rejected);
    }

    return true;
  }
};

}  // namespace

nsresult ServiceWorkerPrivate::SendNotificationEvent(
    const nsAString& aEventName, const nsAString& aID, const nsAString& aTitle,
    const nsAString& aDir, const nsAString& aLang, const nsAString& aBody,
    const nsAString& aTag, const nsAString& aIcon, const nsAString& aData,
    const nsAString& aBehavior, const nsAString& aScope) {
  MOZ_ASSERT(NS_IsMainThread());

  WakeUpReason why;
  if (aEventName.EqualsLiteral(NOTIFICATION_CLICK_EVENT_NAME)) {
    why = NotificationClickEvent;
    gDOMDisableOpenClickDelay =
        Preferences::GetInt("dom.serviceWorkers.disable_open_click_delay");
  } else if (aEventName.EqualsLiteral(NOTIFICATION_CLOSE_EVENT_NAME)) {
    why = NotificationCloseEvent;
  } else {
    MOZ_ASSERT_UNREACHABLE("Invalid notification event name");
    return NS_ERROR_FAILURE;
  }

  if (mInner) {
    return mInner->SendNotificationEvent(aEventName, aID, aTitle, aDir, aLang,
                                         aBody, aTag, aIcon, aData, aBehavior,
                                         aScope, gDOMDisableOpenClickDelay);
  }

  nsresult rv = SpawnWorkerIfNeeded(why);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<KeepAliveToken> token = CreateEventKeepAliveToken();

  RefPtr<WorkerRunnable> r = new SendNotificationEventRunnable(
      mWorkerPrivate, token, aEventName, aID, aTitle, aDir, aLang, aBody, aTag,
      aIcon, aData, aBehavior, aScope);
  if (NS_WARN_IF(!r->Dispatch())) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

namespace {

// Inheriting ExtendableEventWorkerRunnable so that the worker is not terminated
// while handling the fetch event, though that's very unlikely.
class FetchEventRunnable : public ExtendableFunctionalEventWorkerRunnable,
                           public nsIHttpHeaderVisitor {
  nsMainThreadPtrHandle<nsIInterceptedChannel> mInterceptedChannel;
  const nsCString mScriptSpec;
  nsTArray<nsCString> mHeaderNames;
  nsTArray<nsCString> mHeaderValues;
  nsCString mSpec;
  nsCString mFragment;
  nsCString mMethod;
  nsString mClientId;
  nsString mResultingClientId;
  RequestCache mCacheMode;
  RequestMode mRequestMode;
  RequestRedirect mRequestRedirect;
  RequestCredentials mRequestCredentials;
  nsContentPolicyType mContentPolicyType;
  nsCOMPtr<nsIInputStream> mUploadStream;
  int64_t mUploadStreamContentLength;
  nsString mReferrer;
  ReferrerPolicy mReferrerPolicy;
  nsString mIntegrity;
  const bool mIsNonSubresourceRequest;

 public:
  FetchEventRunnable(
      WorkerPrivate* aWorkerPrivate, KeepAliveToken* aKeepAliveToken,
      nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
      // CSP checks might require the worker script spec
      // later on.
      const nsACString& aScriptSpec,
      nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration,
      const nsAString& aClientId, const nsAString& aResultingClientId,
      bool aMarkLaunchServiceWorkerEnd, bool aIsNonSubresourceRequest)
      : ExtendableFunctionalEventWorkerRunnable(aWorkerPrivate, aKeepAliveToken,
                                                aRegistration),
        mInterceptedChannel(aChannel),
        mScriptSpec(aScriptSpec),
        mClientId(aClientId),
        mResultingClientId(aResultingClientId),
        mCacheMode(RequestCache::Default),
        mRequestMode(RequestMode::No_cors),
        mRequestRedirect(RequestRedirect::Follow)
        // By default we set it to same-origin since normal HTTP fetches always
        // send credentials to same-origin websites unless explicitly forbidden.
        ,
        mRequestCredentials(RequestCredentials::Same_origin),
        mContentPolicyType(nsIContentPolicy::TYPE_INVALID),
        mUploadStreamContentLength(-1),
        mReferrer(NS_LITERAL_STRING_FROM_CSTRING(kFETCH_CLIENT_REFERRER_STR)),
        mReferrerPolicy(ReferrerPolicy::_empty),
        mIsNonSubresourceRequest(aIsNonSubresourceRequest) {
    MOZ_ASSERT(aWorkerPrivate);
  }

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD
  VisitHeader(const nsACString& aHeader, const nsACString& aValue) override {
    mHeaderNames.AppendElement(aHeader);
    mHeaderValues.AppendElement(aValue);
    return NS_OK;
  }

  nsresult Init() {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIChannel> channel;
    nsresult rv = mInterceptedChannel->GetChannel(getter_AddRefs(channel));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> uri;
    rv = mInterceptedChannel->GetSecureUpgradedChannelURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    // Normally we rely on the Request constructor to strip the fragment, but
    // when creating the FetchEvent we bypass the constructor.  So strip the
    // fragment manually here instead.  We can't do it later when we create
    // the Request because that code executes off the main thread.
    nsCOMPtr<nsIURI> uriNoFragment;
    rv = NS_GetURIWithoutRef(uri, getter_AddRefs(uriNoFragment));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = uriNoFragment->GetSpec(mSpec);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = uri->GetRef(mFragment);
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t loadFlags;
    rv = channel->GetLoadFlags(&loadFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
    mContentPolicyType = loadInfo->InternalContentPolicyType();

    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);
    MOZ_ASSERT(httpChannel, "How come we don't have an HTTP channel?");

    mReferrerPolicy = ReferrerPolicy::_empty;
    mReferrer.Truncate();
    nsCOMPtr<nsIReferrerInfo> referrerInfo = httpChannel->GetReferrerInfo();
    if (referrerInfo) {
      mReferrerPolicy = referrerInfo->ReferrerPolicy();
      Unused << referrerInfo->GetComputedReferrerSpec(mReferrer);
    }

    rv = httpChannel->GetRequestMethod(mMethod);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIHttpChannelInternal> internalChannel =
        do_QueryInterface(httpChannel);
    NS_ENSURE_TRUE(internalChannel, NS_ERROR_NOT_AVAILABLE);

    mRequestMode = InternalRequest::MapChannelToRequestMode(channel);

    // This is safe due to static_asserts in ServiceWorkerManager.cpp.
    uint32_t redirectMode;
    rv = internalChannel->GetRedirectMode(&redirectMode);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    mRequestRedirect = static_cast<RequestRedirect>(redirectMode);

    // This is safe due to static_asserts in ServiceWorkerManager.cpp.
    uint32_t cacheMode;
    rv = internalChannel->GetFetchCacheMode(&cacheMode);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    mCacheMode = static_cast<RequestCache>(cacheMode);

    rv = internalChannel->GetIntegrityMetadata(mIntegrity);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    mRequestCredentials =
        InternalRequest::MapChannelToRequestCredentials(channel);

    rv = httpChannel->VisitNonDefaultRequestHeaders(this);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIUploadChannel2> uploadChannel = do_QueryInterface(httpChannel);
    if (uploadChannel) {
      MOZ_ASSERT(!mUploadStream);
      nsCOMPtr<nsIInputStream> uploadStream;
      rv = uploadChannel->CloneUploadStream(&mUploadStreamContentLength,
                                            getter_AddRefs(uploadStream));
      NS_ENSURE_SUCCESS(rv, rv);
      mUploadStream = uploadStream;
    }

    return NS_OK;
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    MOZ_ASSERT(aWorkerPrivate);

    return DispatchFetchEvent(aCx, aWorkerPrivate);
  }

  nsresult Cancel() override {
    nsCOMPtr<nsIRunnable> runnable = new ResumeRequest(mInterceptedChannel);
    if (NS_FAILED(mWorkerPrivate->DispatchToMainThread(runnable))) {
      NS_WARNING("Failed to resume channel on FetchEventRunnable::Cancel()!\n");
    }
    WorkerRunnable::Cancel();
    return NS_OK;
  }

 private:
  ~FetchEventRunnable() = default;

  class ResumeRequest final : public Runnable {
    nsMainThreadPtrHandle<nsIInterceptedChannel> mChannel;

   public:
    explicit ResumeRequest(
        nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel)
        : Runnable("dom::FetchEventRunnable::ResumeRequest"),
          mChannel(aChannel) {}

    NS_IMETHOD Run() override {
      MOZ_ASSERT(NS_IsMainThread());

      nsresult rv = mChannel->ResetInterception(false);
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to resume intercepted network request");
        mChannel->CancelInterception(rv);
      }
      return rv;
    }
  };

  bool DispatchFetchEvent(JSContext* aCx, WorkerPrivate* aWorkerPrivate) {
    MOZ_ASSERT(aCx);
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
    GlobalObject globalObj(aCx, aWorkerPrivate->GlobalScope()->GetWrapper());

    RefPtr<InternalHeaders> internalHeaders =
        new InternalHeaders(HeadersGuardEnum::Request);
    MOZ_ASSERT(mHeaderNames.Length() == mHeaderValues.Length());
    for (uint32_t i = 0; i < mHeaderNames.Length(); i++) {
      ErrorResult result;
      internalHeaders->Set(mHeaderNames[i], mHeaderValues[i], result);
      if (NS_WARN_IF(result.Failed())) {
        result.SuppressException();
        return false;
      }
    }

    ErrorResult result;
    internalHeaders->SetGuard(HeadersGuardEnum::Immutable, result);
    if (NS_WARN_IF(result.Failed())) {
      result.SuppressException();
      return false;
    }
    auto internalReq = MakeSafeRefPtr<InternalRequest>(
        mSpec, mFragment, mMethod, internalHeaders.forget(), mCacheMode,
        mRequestMode, mRequestRedirect, mRequestCredentials, mReferrer,
        mReferrerPolicy, mContentPolicyType, mIntegrity);
    internalReq->SetBody(mUploadStream, mUploadStreamContentLength);

    nsCOMPtr<nsIChannel> channel;
    nsresult rv = mInterceptedChannel->GetChannel(getter_AddRefs(channel));
    NS_ENSURE_SUCCESS(rv, false);

    nsCOMPtr<nsICacheInfoChannel> cic = do_QueryInterface(channel);
    if (cic && !cic->PreferredAlternativeDataTypes().IsEmpty()) {
      // TODO: the internal request probably needs all the preferred types.
      nsAutoCString alternativeDataType;
      alternativeDataType.Assign(
          cic->PreferredAlternativeDataTypes()[0].type());
      internalReq->SetPreferredAlternativeDataType(alternativeDataType);
    }

    nsCOMPtr<nsIGlobalObject> global =
        do_QueryInterface(globalObj.GetAsSupports());
    if (NS_WARN_IF(!global)) {
      return false;
    }

    // TODO This request object should be created with a AbortSignal object
    // which should be aborted if the loading is aborted. See bug 1394102.
    RefPtr<Request> request =
        new Request(global, internalReq.clonePtr(), nullptr);

    MOZ_ASSERT_IF(internalReq->IsNavigationRequest(),
                  request->Redirect() == RequestRedirect::Manual);

    RootedDictionary<FetchEventInit> init(aCx);
    init.mRequest = request;
    init.mBubbles = false;
    init.mCancelable = true;
    // Only expose the FetchEvent.clientId on subresource requests for now.
    // Once we implement .targetClientId we can then start exposing .clientId
    // on non-subresource requests as well.  See bug 1487534.
    if (!mClientId.IsEmpty() && !internalReq->IsNavigationRequest()) {
      init.mClientId = mClientId;
    }

    /*
     * https://w3c.github.io/ServiceWorker/#on-fetch-request-algorithm
     *
     * "If request is a non-subresource request and request’s
     * destination is not "report", initialize e’s resultingClientId attribute
     * to reservedClient’s [resultingClient's] id, and to the empty string
     * otherwise." (Step 18.8)
     */
    if (!mResultingClientId.IsEmpty() && mIsNonSubresourceRequest &&
        internalReq->Destination() != RequestDestination::Report) {
      init.mResultingClientId = mResultingClientId;
    }

    RefPtr<FetchEvent> event =
        FetchEvent::Constructor(globalObj, u"fetch"_ns, init);

    event->PostInit(mInterceptedChannel, mRegistration, mScriptSpec);
    event->SetTrusted(true);

    nsresult rv2 = DispatchExtendableEventOnWorkerScope(
        aCx, aWorkerPrivate->GlobalScope(), event, nullptr);
    if ((NS_WARN_IF(NS_FAILED(rv2)) &&
         rv2 != NS_ERROR_XPC_JS_THREW_EXCEPTION) ||
        !event->WaitToRespond()) {
      nsCOMPtr<nsIRunnable> runnable;
      MOZ_ASSERT(!aWorkerPrivate->UsesSystemPrincipal(),
                 "We don't support system-principal serviceworkers");
      if (event->DefaultPrevented(CallerType::NonSystem)) {
        runnable = new CancelChannelRunnable(mInterceptedChannel, mRegistration,
                                             NS_ERROR_INTERCEPTION_FAILED);
      } else {
        runnable = new ResumeRequest(mInterceptedChannel);
      }

      MOZ_ALWAYS_SUCCEEDS(
          mWorkerPrivate->DispatchToMainThread(runnable.forget()));
    }

    return true;
  }
};

NS_IMPL_ISUPPORTS_INHERITED(FetchEventRunnable, WorkerRunnable,
                            nsIHttpHeaderVisitor)

}  // anonymous namespace

nsresult ServiceWorkerPrivate::SendFetchEvent(
    nsIInterceptedChannel* aChannel, nsILoadGroup* aLoadGroup,
    const nsAString& aClientId, const nsAString& aResultingClientId) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (NS_WARN_IF(!mInfo || !swm)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIChannel> channel;
  nsresult rv = aChannel->GetChannel(getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, rv);
  bool isNonSubresourceRequest =
      nsContentUtils::IsNonSubresourceRequest(channel);

  RefPtr<ServiceWorkerRegistrationInfo> registration;
  if (isNonSubresourceRequest) {
    registration = swm->GetRegistration(mInfo->Principal(), mInfo->Scope());
  } else {
    nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();

    // We'll check for a null registration below rather than an error code here.
    Unused << swm->GetClientRegistration(loadInfo->GetClientInfo().ref(),
                                         getter_AddRefs(registration));
  }

  // Its possible the registration is removed between starting the interception
  // and actually dispatching the fetch event.  In these cases we simply
  // want to restart the original network request.  Since this is a normal
  // condition we handle the reset here instead of returning an error which
  // would in turn trigger a console report.
  if (!registration) {
    nsresult rv = aChannel->ResetInterception(false);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to resume intercepted network request");
      aChannel->CancelInterception(rv);
    }
    return NS_OK;
  }

  // Handle Fetch algorithm - step 16. If the service worker didn't register
  // any fetch event handlers, then abort the interception and maybe trigger
  // the soft update algorithm.
  if (!mInfo->HandlesFetch()) {
    nsresult rv = aChannel->ResetInterception(false);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to resume intercepted network request");
      aChannel->CancelInterception(rv);
    }

    // Trigger soft updates if necessary.
    registration->MaybeScheduleTimeCheckAndUpdate();

    return NS_OK;
  }

  if (mInner) {
    return mInner->SendFetchEvent(std::move(registration), aChannel, aClientId,
                                  aResultingClientId);
  }

  bool newWorkerCreated = false;
  rv = SpawnWorkerIfNeeded(FetchEvent, &newWorkerCreated, aLoadGroup);
  NS_ENSURE_SUCCESS(rv, rv);

  nsMainThreadPtrHandle<nsIInterceptedChannel> handle(
      new nsMainThreadPtrHolder<nsIInterceptedChannel>("nsIInterceptedChannel",
                                                       aChannel, false));

  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> regInfo(
      new nsMainThreadPtrHolder<ServiceWorkerRegistrationInfo>(
          "ServiceWorkerRegistrationInfoProxy", registration, false));

  RefPtr<KeepAliveToken> token = CreateEventKeepAliveToken();

  RefPtr<FetchEventRunnable> r = new FetchEventRunnable(
      mWorkerPrivate, token, handle, mInfo->ScriptSpec(), regInfo, aClientId,
      aResultingClientId, newWorkerCreated, isNonSubresourceRequest);
  rv = r->Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mInfo->State() == ServiceWorkerState::Activating) {
    mPendingFunctionalEvents.AppendElement(r.forget());
    return NS_OK;
  }

  MOZ_ASSERT(mInfo->State() == ServiceWorkerState::Activated);

  if (NS_WARN_IF(!r->Dispatch())) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult ServiceWorkerPrivate::SpawnWorkerIfNeeded(WakeUpReason aWhy,
                                                   bool* aNewWorkerCreated,
                                                   nsILoadGroup* aLoadGroup) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mInner);

  // Defaults to no new worker created, but if there is one, we'll set the value
  // to true at the end of this function.
  if (aNewWorkerCreated) {
    *aNewWorkerCreated = false;
  }

  // If the worker started shutting down on itself we may have a stale
  // reference here.  Invoke our termination code to clean it out.
  if (mWorkerPrivate && mWorkerPrivate->ParentStatusProtected() > Running) {
    TerminateWorker();
    MOZ_DIAGNOSTIC_ASSERT(!mWorkerPrivate);
  }

  if (mWorkerPrivate) {
    // If we have a load group here then use it to update the service worker
    // load group.  This was added when we needed the load group's tab child
    // to pass some security checks.  Those security checks are gone, though,
    // and we could possibly remove this now.  For now we just do it
    // opportunistically.  When the service worker is running in a separate
    // process from the client that initiated the intercepted channel, then
    // the load group will be nullptr.  UpdateOverrideLoadGroup ignores nullptr
    // load groups.
    mWorkerPrivate->UpdateOverridenLoadGroup(aLoadGroup);
    RenewKeepAliveToken(aWhy);

    return NS_OK;
  }

  // Sanity check: mSupportsArray should be empty if we're about to
  // spin up a new worker.
  MOZ_ASSERT(mSupportsArray.IsEmpty());

  if (NS_WARN_IF(!mInfo)) {
    NS_WARNING("Trying to wake up a dead service worker.");
    return NS_ERROR_FAILURE;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  NS_ENSURE_TRUE(swm, NS_ERROR_FAILURE);

  RefPtr<ServiceWorkerRegistrationInfo> reg =
      swm->GetRegistration(mInfo->Principal(), mInfo->Scope());
  NS_ENSURE_TRUE(reg, NS_ERROR_FAILURE);

  // TODO(catalinb): Bug 1192138 - Add telemetry for service worker wake-ups.

  // Ensure that the IndexedDatabaseManager is initialized
  Unused << NS_WARN_IF(!IndexedDatabaseManager::GetOrCreate());

  WorkerLoadInfo info;
  nsresult rv = NS_NewURI(getter_AddRefs(info.mBaseURI), mInfo->ScriptSpec());

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  info.mResolvedScriptURI = info.mBaseURI;
  MOZ_ASSERT(!mInfo->CacheName().IsEmpty());
  info.mServiceWorkerCacheName = mInfo->CacheName();

  info.mServiceWorkerDescriptor.emplace(mInfo->Descriptor());
  info.mServiceWorkerRegistrationDescriptor.emplace(reg->Descriptor());

  info.mLoadGroup = aLoadGroup;

  // If we are loading a script for a ServiceWorker then we must not
  // try to intercept it.  If the interception matches the current
  // ServiceWorker's scope then we could deadlock the load.
  info.mLoadFlags =
      mInfo->GetImportsLoadFlags() | nsIChannel::LOAD_BYPASS_SERVICE_WORKER;

  rv = info.mBaseURI->GetHost(info.mDomain);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  info.mPrincipal = mInfo->Principal();
  info.mLoadingPrincipal = info.mPrincipal;

  info.mCookieJarSettings =
      mozilla::net::CookieJarSettings::Create(info.mPrincipal);

  MOZ_ASSERT(info.mCookieJarSettings);

  // We can get the partitionKey from the principal of the ServiceWorker because
  // it's a foreign partitioned principal. In other words, the principal will
  // have the partitionKey if it's in a third-party context. Otherwise, we can
  // set the partitionKey via the script URI because it's in the first-party
  // context.
  if (!info.mPrincipal->OriginAttributesRef().mPartitionKey.IsEmpty()) {
    net::CookieJarSettings::Cast(info.mCookieJarSettings)
        ->SetPartitionKey(info.mPrincipal->OriginAttributesRef().mPartitionKey);
  } else {
    net::CookieJarSettings::Cast(info.mCookieJarSettings)
        ->SetPartitionKey(info.mResolvedScriptURI);
  }

  if (StaticPrefs::privacy_partition_serviceWorkers()) {
    nsCOMPtr<nsIPrincipal> partitionedPrincipal;
    StoragePrincipalHelper::CreatePartitionedPrincipalForServiceWorker(
        info.mPrincipal, info.mCookieJarSettings,
        getter_AddRefs(partitionedPrincipal));

    info.mPartitionedPrincipal = partitionedPrincipal;
  } else {
    // The partitioned principal will be the same as the mPrincipal if
    // partitioned service worker is disabled.
    info.mPartitionedPrincipal = info.mPrincipal;
  }

  info.mStorageAccess =
      StorageAllowedForServiceWorker(info.mPrincipal, info.mCookieJarSettings);

  info.mOriginAttributes = mInfo->GetOriginAttributes();

  // We don't have a good way to know if a service worker is regsitered under a
  // third-party context. The only information we can rely on is if the service
  // worker is running under a partitioned principal.
  info.mIsThirdPartyContextToTopWindow =
      !mInfo->Principal()->OriginAttributesRef().mPartitionKey.IsEmpty();

  // Verify that we don't have any CSP on pristine client.
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  if (info.mChannel) {
    nsCOMPtr<nsILoadInfo> loadinfo = info.mChannel->LoadInfo();
    csp = loadinfo->GetCsp();
  }
  MOZ_DIAGNOSTIC_ASSERT(!csp);
#endif

  // Default CSP permissions for now.  These will be overrided if necessary
  // based on the script CSP headers during load in ScriptLoader.
  info.mEvalAllowed = true;
  info.mReportCSPViolations = false;

  WorkerPrivate::OverrideLoadInfoLoadGroup(info, info.mPrincipal);

  rv = info.SetPrincipalsAndCSPOnMainThread(
      info.mPrincipal, info.mPartitionedPrincipal, info.mLoadGroup, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  info.mAgentClusterId = reg->AgentClusterId();

  AutoJSAPI jsapi;
  jsapi.Init();
  ErrorResult error;
  NS_ConvertUTF8toUTF16 scriptSpec(mInfo->ScriptSpec());

  mWorkerPrivate = WorkerPrivate::Constructor(jsapi.cx(), scriptSpec, false,
                                              WorkerKindService, VoidString(),
                                              ""_ns, &info, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  RenewKeepAliveToken(aWhy);

  if (aNewWorkerCreated) {
    *aNewWorkerCreated = true;
  }

  return NS_OK;
}

bool ServiceWorkerPrivate::MaybeStoreISupports(nsISupports* aSupports) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mWorkerPrivate) {
    MOZ_DIAGNOSTIC_ASSERT(mSupportsArray.IsEmpty());
    return false;
  }

  MOZ_ASSERT(!mSupportsArray.Contains(aSupports));
  mSupportsArray.AppendElement(aSupports);
  return true;
}

void ServiceWorkerPrivate::RemoveISupports(nsISupports* aSupports) {
  MOZ_ASSERT(NS_IsMainThread());
  mSupportsArray.RemoveElement(aSupports);
}

void ServiceWorkerPrivate::TerminateWorker() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mInner) {
    return mInner->TerminateWorker();
  }

  mIdleWorkerTimer->Cancel();
  mIdleKeepAliveToken = nullptr;
  if (mWorkerPrivate) {
    if (StaticPrefs::dom_serviceWorkers_testing_enabled()) {
      nsCOMPtr<nsIObserverService> os = services::GetObserverService();
      if (os) {
        os->NotifyObservers(nullptr, "service-worker-shutdown", nullptr);
      }
    }

    Unused << NS_WARN_IF(!mWorkerPrivate->Cancel());
    RefPtr<WorkerPrivate> workerPrivate = std::move(mWorkerPrivate);
    mozilla::Unused << workerPrivate;
    mSupportsArray.Clear();

    // Any pending events are never going to fire on this worker.  Cancel
    // them so that intercepted channels can be reset and other resources
    // cleaned up.
    nsTArray<RefPtr<WorkerRunnable>> pendingEvents =
        std::move(mPendingFunctionalEvents);
    for (uint32_t i = 0; i < pendingEvents.Length(); ++i) {
      pendingEvents[i]->Cancel();
    }
  }
}

void ServiceWorkerPrivate::NoteDeadServiceWorkerInfo() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mInner) {
    mInner->NoteDeadOuter();
    mInner = nullptr;
  } else {
    TerminateWorker();
  }

  mInfo = nullptr;
}

namespace {

class UpdateStateControlRunnable final
    : public MainThreadWorkerControlRunnable {
  const ServiceWorkerState mState;

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate);
    aWorkerPrivate->UpdateServiceWorkerState(mState);
    return true;
  }

 public:
  UpdateStateControlRunnable(WorkerPrivate* aWorkerPrivate,
                             ServiceWorkerState aState)
      : MainThreadWorkerControlRunnable(aWorkerPrivate), mState(aState) {}
};

}  // anonymous namespace

void ServiceWorkerPrivate::UpdateState(ServiceWorkerState aState) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mInner) {
    return mInner->UpdateState(aState);
  }

  if (!mWorkerPrivate) {
    MOZ_DIAGNOSTIC_ASSERT(mPendingFunctionalEvents.IsEmpty());
    return;
  }

  RefPtr<WorkerRunnable> r =
      new UpdateStateControlRunnable(mWorkerPrivate, aState);
  Unused << r->Dispatch();

  if (aState != ServiceWorkerState::Activated) {
    return;
  }

  nsTArray<RefPtr<WorkerRunnable>> pendingEvents =
      std::move(mPendingFunctionalEvents);

  for (uint32_t i = 0; i < pendingEvents.Length(); ++i) {
    RefPtr<WorkerRunnable> r = std::move(pendingEvents[i]);
    if (NS_WARN_IF(!r->Dispatch())) {
      NS_WARNING("Failed to dispatch pending functional event!");
    }
  }
}

nsresult ServiceWorkerPrivate::GetDebugger(nsIWorkerDebugger** aResult) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aResult);

  if (mInner) {
    *aResult = nullptr;
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (!mDebuggerCount) {
    return NS_OK;
  }

  MOZ_ASSERT(mWorkerPrivate);

  nsCOMPtr<nsIWorkerDebugger> debugger = mWorkerPrivate->Debugger();
  debugger.forget(aResult);

  return NS_OK;
}

nsresult ServiceWorkerPrivate::AttachDebugger() {
  MOZ_ASSERT(NS_IsMainThread());

  // When the first debugger attaches to a worker, we spawn a worker if needed,
  // and cancel the idle timeout. The idle timeout should not be reset until
  // the last debugger detached from the worker.
  if (!mDebuggerCount) {
    nsresult rv = mInner ? mInner->SpawnWorkerIfNeeded()
                         : SpawnWorkerIfNeeded(AttachEvent);
    NS_ENSURE_SUCCESS(rv, rv);

    /**
     * Under parent-intercept mode (i.e. non-null `mInner`), renewing the idle
     * KeepAliveToken for spawning workers happens asynchronously, rather than
     * synchronously without parent-intercept (see
     * `ServiceWorkerPrivate::SpawnWorkerIfNeeded`). The asynchronous renewal is
     * because the actual spawning of workers under parent-intercept occurs in a
     * content process, so we will only renew once notified that the worker has
     * been successfully created
     * (see `ServiceWorkerPrivateImpl::CreationSucceeded`).
     *
     * This means that the DevTools way of starting up a worker by calling
     * `AttachDebugger` immediately followed by `DetachDebugger` will spawn and
     * immediately terminate a worker (because `mTokenCount` is possibly 0
     * due to the idle KeepAliveToken being created asynchronously). So, just
     * renew the KeepAliveToken right now.
     */
    if (mInner) {
      RenewKeepAliveToken(AttachEvent);
    }

    mIdleWorkerTimer->Cancel();
  }

  ++mDebuggerCount;

  return NS_OK;
}

nsresult ServiceWorkerPrivate::DetachDebugger() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mDebuggerCount) {
    return NS_ERROR_UNEXPECTED;
  }

  --mDebuggerCount;

  // When the last debugger detaches from a worker, we either reset the idle
  // timeout, or terminate the worker if there are no more active tokens.
  if (!mDebuggerCount) {
    if (mTokenCount) {
      ResetIdleTimeout();
    } else {
      TerminateWorker();
    }
  }

  return NS_OK;
}

bool ServiceWorkerPrivate::IsIdle() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mTokenCount == 0 || (mTokenCount == 1 && mIdleKeepAliveToken);
}

RefPtr<GenericPromise> ServiceWorkerPrivate::GetIdlePromise() {
#ifdef DEBUG
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsIdle());
  MOZ_ASSERT(!mIdlePromiseObtained, "Idle promise may only be obtained once!");
  mIdlePromiseObtained = true;
#endif

  return mIdlePromiseHolder.Ensure(__func__);
}

namespace {

class ServiceWorkerPrivateTimerCallback final : public nsITimerCallback,
                                                public nsINamed {
 public:
  using Method = void (ServiceWorkerPrivate::*)(nsITimer*);

  ServiceWorkerPrivateTimerCallback(ServiceWorkerPrivate* aServiceWorkerPrivate,
                                    Method aMethod)
      : mServiceWorkerPrivate(aServiceWorkerPrivate), mMethod(aMethod) {}

  NS_IMETHOD
  Notify(nsITimer* aTimer) override {
    (mServiceWorkerPrivate->*mMethod)(aTimer);
    mServiceWorkerPrivate = nullptr;
    return NS_OK;
  }

  NS_IMETHOD
  GetName(nsACString& aName) override {
    aName.AssignLiteral("ServiceWorkerPrivateTimerCallback");
    return NS_OK;
  }

 private:
  ~ServiceWorkerPrivateTimerCallback() = default;

  RefPtr<ServiceWorkerPrivate> mServiceWorkerPrivate;
  Method mMethod;

  NS_DECL_THREADSAFE_ISUPPORTS
};

NS_IMPL_ISUPPORTS(ServiceWorkerPrivateTimerCallback, nsITimerCallback,
                  nsINamed);

}  // anonymous namespace

void ServiceWorkerPrivate::NoteIdleWorkerCallback(nsITimer* aTimer) {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(aTimer == mIdleWorkerTimer, "Invalid timer!");

  // Release ServiceWorkerPrivate's token, since the grace period has ended.
  mIdleKeepAliveToken = nullptr;

  if (mWorkerPrivate || (mInner && !mInner->WorkerIsDead())) {
    // There sould only be EITHER mWorkerPrivate or mInner (but not both).
    MOZ_ASSERT(!(mWorkerPrivate && mInner));

    // If we still have a living worker at this point it means that either there
    // are pending waitUntil promises or the worker is doing some long-running
    // computation. Wait a bit more until we forcibly terminate the worker.
    uint32_t timeout =
        Preferences::GetInt("dom.serviceWorkers.idle_extended_timeout");
    nsCOMPtr<nsITimerCallback> cb = new ServiceWorkerPrivateTimerCallback(
        this, &ServiceWorkerPrivate::TerminateWorkerCallback);
    DebugOnly<nsresult> rv = mIdleWorkerTimer->InitWithCallback(
        cb, timeout, nsITimer::TYPE_ONE_SHOT);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
}

void ServiceWorkerPrivate::TerminateWorkerCallback(nsITimer* aTimer) {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(aTimer == this->mIdleWorkerTimer, "Invalid timer!");

  // mInfo must be non-null at this point because NoteDeadServiceWorkerInfo
  // which zeroes it calls TerminateWorker which cancels our timer which will
  // ensure we don't get invoked even if the nsTimerEvent is in the event queue.
  ServiceWorkerManager::LocalizeAndReportToAllClients(
      mInfo->Scope(), "ServiceWorkerGraceTimeoutTermination",
      nsTArray<nsString>{NS_ConvertUTF8toUTF16(mInfo->Scope())});

  TerminateWorker();
}

void ServiceWorkerPrivate::RenewKeepAliveToken(WakeUpReason aWhy) {
  // We should have an active worker if we're renewing the keep alive token.
  MOZ_ASSERT(mWorkerPrivate || (mInner && !mInner->WorkerIsDead()));

  // If there is at least one debugger attached to the worker, the idle worker
  // timeout was canceled when the first debugger attached to the worker. It
  // should not be reset until the last debugger detaches from the worker.
  if (!mDebuggerCount) {
    ResetIdleTimeout();
  }

  if (!mIdleKeepAliveToken) {
    mIdleKeepAliveToken = new KeepAliveToken(this);
  }
}

void ServiceWorkerPrivate::ResetIdleTimeout() {
  uint32_t timeout = Preferences::GetInt("dom.serviceWorkers.idle_timeout");
  nsCOMPtr<nsITimerCallback> cb = new ServiceWorkerPrivateTimerCallback(
      this, &ServiceWorkerPrivate::NoteIdleWorkerCallback);
  DebugOnly<nsresult> rv =
      mIdleWorkerTimer->InitWithCallback(cb, timeout, nsITimer::TYPE_ONE_SHOT);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

void ServiceWorkerPrivate::AddToken() {
  MOZ_ASSERT(NS_IsMainThread());
  ++mTokenCount;
}

void ServiceWorkerPrivate::ReleaseToken() {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(mTokenCount > 0);
  --mTokenCount;

  if (IsIdle()) {
    mIdlePromiseHolder.ResolveIfExists(true, __func__);

    if (!mTokenCount) {
      TerminateWorker();
    }

    // mInfo can be nullptr here if NoteDeadServiceWorkerInfo() is called while
    // the KeepAliveToken is being proxy released as a runnable.
    else if (mInfo) {
      RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
      if (swm) {
        swm->WorkerIsIdle(mInfo);
      }
    }
  }
}

already_AddRefed<KeepAliveToken>
ServiceWorkerPrivate::CreateEventKeepAliveToken() {
  MOZ_ASSERT(NS_IsMainThread());

  // When the WorkerPrivate is in a separate process, we first hold a normal
  // KeepAliveToken. Then, after we're notified that the worker is alive, we
  // create the idle KeepAliveToken.
  MOZ_ASSERT(mWorkerPrivate || (mInner && !mInner->WorkerIsDead()));
  MOZ_ASSERT(mIdleKeepAliveToken || (mInner && !mInner->WorkerIsDead()));

  RefPtr<KeepAliveToken> ref = new KeepAliveToken(this);
  return ref.forget();
}

void ServiceWorkerPrivate::SetHandlesFetch(bool aValue) {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!mInfo)) {
    return;
  }

  mInfo->SetHandlesFetch(aValue);
}

}  // namespace dom
}  // namespace mozilla
