/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerRegistrationImpl.h"

#include "ipc/ErrorIPCUtils.h"
#include "mozilla/dom/DOMPrefs.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/PushManagerBinding.h"
#include "mozilla/dom/PushManager.h"
#include "mozilla/dom/ServiceWorkerRegistrationBinding.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "nsCycleCollectionParticipant.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "ServiceWorker.h"
#include "ServiceWorkerManager.h"
#include "ServiceWorkerPrivate.h"
#include "ServiceWorkerRegistration.h"

#include "nsIDocument.h"
#include "nsIServiceWorkerManager.h"
#include "nsISupportsPrimitives.h"
#include "nsPIDOMWindow.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

////////////////////////////////////////////////////
// Main Thread implementation

ServiceWorkerRegistrationMainThread::ServiceWorkerRegistrationMainThread(const ServiceWorkerRegistrationDescriptor& aDescriptor)
  : mOuter(nullptr)
  , mDescriptor(aDescriptor)
  , mScope(NS_ConvertUTF8toUTF16(aDescriptor.Scope()))
  , mListeningForEvents(false)
{
  MOZ_ASSERT(NS_IsMainThread());
}

ServiceWorkerRegistrationMainThread::~ServiceWorkerRegistrationMainThread()
{
  MOZ_DIAGNOSTIC_ASSERT(!mListeningForEvents);
  MOZ_DIAGNOSTIC_ASSERT(!mOuter);
}

// XXXnsm, maybe this can be optimized to only add when a event handler is
// registered.
void
ServiceWorkerRegistrationMainThread::StartListeningForEvents()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mListeningForEvents);
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (swm) {
    swm->AddRegistrationEventListener(mScope, this);
    mListeningForEvents = true;
  }
}

void
ServiceWorkerRegistrationMainThread::StopListeningForEvents()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mListeningForEvents) {
    return;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (swm) {
    swm->RemoveRegistrationEventListener(mScope, this);
  }
  mListeningForEvents = false;
}

void
ServiceWorkerRegistrationMainThread::RegistrationRemovedInternal()
{
  MOZ_ASSERT(NS_IsMainThread());
  // Its possible for the binding object to be collected while we the
  // runnable to call this method is in the event queue.  Double check
  // whether there is still anything to do here.
  if (mOuter) {
    mOuter->RegistrationRemoved();
  }
  StopListeningForEvents();
}

void
ServiceWorkerRegistrationMainThread::UpdateFound()
{
  mOuter->DispatchTrustedEvent(NS_LITERAL_STRING("updatefound"));
}

void
ServiceWorkerRegistrationMainThread::UpdateState(const ServiceWorkerRegistrationDescriptor& aDescriptor)
{
  mDescriptor = aDescriptor;
  mOuter->UpdateState(aDescriptor);
}

void
ServiceWorkerRegistrationMainThread::RegistrationRemoved()
{
  // Queue a runnable to clean up the registration.  This is necessary
  // because there may be runnables in the event queue already to
  // update the registration state.  We want to let those run
  // if possible before clearing our mOuter reference.
  nsCOMPtr<nsIRunnable> r = NewRunnableMethod(
    "ServiceWorkerRegistrationMainThread::RegistrationRemoved",
    this,
    &ServiceWorkerRegistrationMainThread::RegistrationRemovedInternal);
  MOZ_ALWAYS_SUCCEEDS(SystemGroup::Dispatch(TaskCategory::Other, r.forget()));
}

bool
ServiceWorkerRegistrationMainThread::MatchesDescriptor(const ServiceWorkerRegistrationDescriptor& aDescriptor)
{
  return mOuter->MatchesDescriptor(aDescriptor);
}

void
ServiceWorkerRegistrationMainThread::SetServiceWorkerRegistration(ServiceWorkerRegistration* aReg)
{
  MOZ_DIAGNOSTIC_ASSERT(aReg);
  MOZ_DIAGNOSTIC_ASSERT(!mOuter);
  mOuter = aReg;
  StartListeningForEvents();
}

void
ServiceWorkerRegistrationMainThread::ClearServiceWorkerRegistration(ServiceWorkerRegistration* aReg)
{
  MOZ_ASSERT_IF(mOuter, mOuter == aReg);
  StopListeningForEvents();
  mOuter = nullptr;
}

namespace {

void
UpdateInternal(nsIPrincipal* aPrincipal,
               const nsACString& aScope,
               ServiceWorkerUpdateFinishCallback* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aCallback);

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    // browser shutdown
    return;
  }

  swm->Update(aPrincipal, aScope, aCallback);
}

class MainThreadUpdateCallback final : public ServiceWorkerUpdateFinishCallback
{
  RefPtr<ServiceWorkerRegistrationPromise::Private> mPromise;

  ~MainThreadUpdateCallback()
  {
    mPromise->Reject(NS_ERROR_DOM_ABORT_ERR, __func__);
  }

public:
  MainThreadUpdateCallback()
    : mPromise(new ServiceWorkerRegistrationPromise::Private(__func__))
  {
  }

  void
  UpdateSucceeded(ServiceWorkerRegistrationInfo* aRegistration) override
  {
    mPromise->Resolve(aRegistration->Descriptor(), __func__);
  }

  void
  UpdateFailed(ErrorResult& aStatus) override
  {
    mPromise->Reject(Move(aStatus), __func__);
  }

  RefPtr<ServiceWorkerRegistrationPromise>
  Promise() const
  {
    return mPromise;
  }
};

class WorkerThreadUpdateCallback final : public ServiceWorkerUpdateFinishCallback
{
  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
  RefPtr<ServiceWorkerRegistrationPromise::Private> mPromise;

  ~WorkerThreadUpdateCallback() = default;

public:
  WorkerThreadUpdateCallback(RefPtr<ThreadSafeWorkerRef>&& aWorkerRef,
                             ServiceWorkerRegistrationPromise::Private* aPromise)
    : mWorkerRef(Move(aWorkerRef))
    , mPromise(aPromise)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  void
  UpdateSucceeded(ServiceWorkerRegistrationInfo* aRegistration) override
  {
    mPromise->Resolve(aRegistration->Descriptor(), __func__);
    mWorkerRef = nullptr;
  }

  void
  UpdateFailed(ErrorResult& aStatus) override
  {
    mPromise->Reject(Move(aStatus), __func__);
    mWorkerRef = nullptr;
  }
};

class SWRUpdateRunnable final : public Runnable
{
  class TimerCallback final : public nsITimerCallback
  {
    RefPtr<ServiceWorkerPrivate> mPrivate;
    RefPtr<Runnable> mRunnable;

  public:
    TimerCallback(ServiceWorkerPrivate* aPrivate,
                  Runnable* aRunnable)
      : mPrivate(aPrivate)
      , mRunnable(aRunnable)
    {
      MOZ_ASSERT(mPrivate);
      MOZ_ASSERT(aRunnable);
    }

    NS_IMETHOD
    Notify(nsITimer *aTimer) override
    {
      mRunnable->Run();
      mPrivate->RemoveISupports(aTimer);

      return NS_OK;
    }

    NS_DECL_THREADSAFE_ISUPPORTS

  private:
    ~TimerCallback()
    { }
  };

public:
  SWRUpdateRunnable(StrongWorkerRef* aWorkerRef,
                    ServiceWorkerRegistrationPromise::Private* aPromise,
                    const ServiceWorkerDescriptor& aDescriptor)
    : Runnable("dom::SWRUpdateRunnable")
    , mMutex("SWRUpdateRunnable")
    , mWorkerRef(new ThreadSafeWorkerRef(aWorkerRef))
    , mPromise(aPromise)
    , mDescriptor(aDescriptor)
    , mDelayed(false)
  {
    MOZ_DIAGNOSTIC_ASSERT(mWorkerRef);
    MOZ_DIAGNOSTIC_ASSERT(mPromise);
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    ErrorResult result;

    nsCOMPtr<nsIPrincipal> principal = mDescriptor.GetPrincipal();
    if (NS_WARN_IF(!principal)) {
      mPromise->Reject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
      return NS_OK;
    }

    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (NS_WARN_IF(!swm)) {
      mPromise->Reject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
      return NS_OK;
    }

    // This will delay update jobs originating from a service worker thread.
    // We don't currently handle ServiceWorkerRegistration.update() from other
    // worker types. Also, we assume this registration matches self.registration
    // on the service worker global. This is ok for now because service worker globals
    // are the only worker contexts where we expose ServiceWorkerRegistration.
    RefPtr<ServiceWorkerRegistrationInfo> registration =
      swm->GetRegistration(principal, mDescriptor.Scope());
    if (NS_WARN_IF(!registration)) {
      return NS_OK;
    }

    RefPtr<ServiceWorkerInfo> worker = registration->GetByDescriptor(mDescriptor);
    uint32_t delay = registration->GetUpdateDelay();

    // if we have a timer object, it means we've already been delayed once.
    if (delay && !mDelayed) {
      nsCOMPtr<nsITimerCallback> cb = new TimerCallback(worker->WorkerPrivate(), this);
      Result<nsCOMPtr<nsITimer>, nsresult> result =
        NS_NewTimerWithCallback(cb, delay, nsITimer::TYPE_ONE_SHOT,
                                SystemGroup::EventTargetFor(TaskCategory::Other));

      nsCOMPtr<nsITimer> timer = result.unwrapOr(nullptr);
      if (NS_WARN_IF(!timer)) {
        return NS_OK;
      }

      mDelayed = true;

      // We're storing the timer object on the calling service worker's private.
      // ServiceWorkerPrivate will drop the reference if the worker terminates,
      // which will cancel the timer.
      if (!worker->WorkerPrivate()->MaybeStoreISupports(timer)) {
        // The worker thread is already shutting down.  Just cancel the timer
        // and let the update runnable be destroyed.
        timer->Cancel();
        return NS_OK;
      }

      return NS_OK;
    }

    RefPtr<ServiceWorkerRegistrationPromise::Private> promise;
    {
      MutexAutoLock lock(mMutex);
      promise.swap(mPromise);
    }

    RefPtr<WorkerThreadUpdateCallback> cb =
      new WorkerThreadUpdateCallback(Move(mWorkerRef), promise);
    UpdateInternal(principal, mDescriptor.Scope(), cb);

    return NS_OK;
  }

private:
  ~SWRUpdateRunnable()
  {
    MutexAutoLock lock(mMutex);
    if (mPromise) {
      mPromise->Reject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
    }
  }

  // Protects promise access across threads
  Mutex mMutex;

  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
  RefPtr<ServiceWorkerRegistrationPromise::Private> mPromise;
  const ServiceWorkerDescriptor mDescriptor;
  bool mDelayed;
};

NS_IMPL_ISUPPORTS(SWRUpdateRunnable::TimerCallback, nsITimerCallback)

class UnregisterCallback final : public nsIServiceWorkerUnregisterCallback
{
  RefPtr<GenericPromise::Private> mPromise;

public:
  NS_DECL_ISUPPORTS

  UnregisterCallback()
    : mPromise(new GenericPromise::Private(__func__))
  {
  }

  NS_IMETHOD
  UnregisterSucceeded(bool aState) override
  {
    mPromise->Resolve(aState, __func__);
    return NS_OK;
  }

  NS_IMETHOD
  UnregisterFailed() override
  {
    mPromise->Reject(NS_ERROR_DOM_SECURITY_ERR, __func__);
    return NS_OK;
  }

  RefPtr<GenericPromise>
  Promise() const
  {
    return mPromise;
  }

private:
  ~UnregisterCallback() = default;
};

NS_IMPL_ISUPPORTS(UnregisterCallback, nsIServiceWorkerUnregisterCallback)

class WorkerUnregisterCallback final : public nsIServiceWorkerUnregisterCallback
{
  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
  RefPtr<GenericPromise::Private> mPromise;
public:
  NS_DECL_ISUPPORTS

  WorkerUnregisterCallback(RefPtr<ThreadSafeWorkerRef>&& aWorkerRef,
                           RefPtr<GenericPromise::Private>&& aPromise)
    : mWorkerRef(Move(aWorkerRef))
    , mPromise(Move(aPromise))
  {
    MOZ_DIAGNOSTIC_ASSERT(mWorkerRef);
    MOZ_DIAGNOSTIC_ASSERT(mPromise);
  }

  NS_IMETHOD
  UnregisterSucceeded(bool aState) override
  {
    mPromise->Resolve(aState, __func__);
    mWorkerRef = nullptr;
    return NS_OK;
  }

  NS_IMETHOD
  UnregisterFailed() override
  {
    mPromise->Reject(NS_ERROR_DOM_SECURITY_ERR, __func__);
    mWorkerRef = nullptr;
    return NS_OK;
  }

private:
  ~WorkerUnregisterCallback() = default;
};

NS_IMPL_ISUPPORTS(WorkerUnregisterCallback, nsIServiceWorkerUnregisterCallback);

/*
 * If the worker goes away, we still continue to unregister, but we don't try to
 * resolve the worker Promise (which doesn't exist by that point).
 */
class StartUnregisterRunnable final : public Runnable
{
  // The promise is protected by the mutex.
  Mutex mMutex;

  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
  RefPtr<GenericPromise::Private> mPromise;
  const ServiceWorkerRegistrationDescriptor mDescriptor;

  ~StartUnregisterRunnable()
  {
    MutexAutoLock lock(mMutex);
    if (mPromise) {
      mPromise->Reject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
    }
  }

public:
  StartUnregisterRunnable(StrongWorkerRef* aWorkerRef,
                          GenericPromise::Private* aPromise,
                          const ServiceWorkerRegistrationDescriptor& aDescriptor)
    : Runnable("dom::StartUnregisterRunnable")
    , mMutex("StartUnregisterRunnable")
    , mWorkerRef(new ThreadSafeWorkerRef(aWorkerRef))
    , mPromise(aPromise)
    , mDescriptor(aDescriptor)
  {
    MOZ_DIAGNOSTIC_ASSERT(mWorkerRef);
    MOZ_DIAGNOSTIC_ASSERT(mPromise);
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsIPrincipal> principal = mDescriptor.GetPrincipal();
    if (!principal) {
      mPromise->Reject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
      return NS_OK;
    }

    nsCOMPtr<nsIServiceWorkerManager> swm =
      mozilla::services::GetServiceWorkerManager();
    if (!swm) {
      mPromise->Reject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
      return NS_OK;
    }

    RefPtr<GenericPromise::Private> promise;
    {
      MutexAutoLock lock(mMutex);
      promise = mPromise.forget();
    }

    RefPtr<WorkerUnregisterCallback> cb =
      new WorkerUnregisterCallback(Move(mWorkerRef), Move(promise));

    nsresult rv = swm->Unregister(principal,
                                  cb,
                                  NS_ConvertUTF8toUTF16(mDescriptor.Scope()));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPromise->Reject(rv, __func__);
      return NS_OK;
    }

    return NS_OK;
  }
};

} // namespace

RefPtr<ServiceWorkerRegistrationPromise>
ServiceWorkerRegistrationMainThread::Update()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(mOuter);

  nsCOMPtr<nsIPrincipal> principal = mDescriptor.GetPrincipal();
  if (!principal) {
    return ServiceWorkerRegistrationPromise::CreateAndReject(
      NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
  }

  RefPtr<MainThreadUpdateCallback> cb = new MainThreadUpdateCallback();
  UpdateInternal(principal, NS_ConvertUTF16toUTF8(mScope), cb);

  return cb->Promise();
}

RefPtr<GenericPromise>
ServiceWorkerRegistrationMainThread::Unregister()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(mOuter);

  nsCOMPtr<nsIServiceWorkerManager> swm =
    mozilla::services::GetServiceWorkerManager();
  if (!swm) {
    return GenericPromise::CreateAndReject(NS_ERROR_DOM_INVALID_STATE_ERR,
                                           __func__);
  }

  nsCOMPtr<nsIPrincipal> principal = mDescriptor.GetPrincipal();
  if (!principal) {
    return GenericPromise::CreateAndReject(NS_ERROR_DOM_INVALID_STATE_ERR,
                                           __func__);
  }

  RefPtr<UnregisterCallback> cb = new UnregisterCallback();

  nsresult rv = swm->Unregister(principal, cb,
                                NS_ConvertUTF8toUTF16(mDescriptor.Scope()));
  if (NS_FAILED(rv)) {
    return GenericPromise::CreateAndReject(rv, __func__);
  }

  return cb->Promise();
}

////////////////////////////////////////////////////
// Worker Thread implementation

class WorkerListener final : public ServiceWorkerRegistrationListener
{
  const nsString mScope;
  bool mListeningForEvents;

  // Set and unset on worker thread, used on main-thread and protected by mutex.
  ServiceWorkerRegistrationWorkerThread* mRegistration;

  Mutex mMutex;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WorkerListener, override)

  WorkerListener(ServiceWorkerRegistrationWorkerThread* aReg,
                 const nsAString& aScope)
    : mScope(aScope)
    , mListeningForEvents(false)
    , mRegistration(aReg)
    , mMutex("WorkerListener::mMutex")
  {
    MOZ_ASSERT(IsCurrentThreadRunningWorker());
    MOZ_ASSERT(mRegistration);
  }

  void
  StartListeningForEvents()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!mListeningForEvents);
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (swm) {
      // FIXME(nsm): Maybe the function shouldn't take an explicit scope.
      swm->AddRegistrationEventListener(mScope, this);
      mListeningForEvents = true;
    }
  }

  void
  StopListeningForEvents()
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mListeningForEvents) {
      return;
    }

    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();

    if (swm) {
      // FIXME(nsm): Maybe the function shouldn't take an explicit scope.
      swm->RemoveRegistrationEventListener(mScope, this);
      mListeningForEvents = false;
    }
  }

  // ServiceWorkerRegistrationListener
  void
  UpdateFound() override;

  void
  UpdateState(const ServiceWorkerRegistrationDescriptor& aDescriptor) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    // TODO: Not implemented
  }

  void
  RegistrationRemoved() override;

  void
  GetScope(nsAString& aScope) const override
  {
    aScope = mScope;
  }

  bool
  MatchesDescriptor(const ServiceWorkerRegistrationDescriptor& aDescriptor) override
  {
    // TODO: Not implemented
    return false;
  }

  void
  ClearRegistration()
  {
    MOZ_ASSERT(IsCurrentThreadRunningWorker());
    MutexAutoLock lock(mMutex);
    mRegistration = nullptr;
  }

private:
  ~WorkerListener()
  {
    MOZ_ASSERT(!mListeningForEvents);
  }
};

ServiceWorkerRegistrationWorkerThread::ServiceWorkerRegistrationWorkerThread(const ServiceWorkerRegistrationDescriptor& aDescriptor)
  : mOuter(nullptr)
  , mDescriptor(aDescriptor)
  , mScope(NS_ConvertUTF8toUTF16(aDescriptor.Scope()))
{
}

ServiceWorkerRegistrationWorkerThread::~ServiceWorkerRegistrationWorkerThread()
{
  MOZ_DIAGNOSTIC_ASSERT(!mListener);
  MOZ_DIAGNOSTIC_ASSERT(!mOuter);
}

void
ServiceWorkerRegistrationWorkerThread::RegistrationRemoved()
{
  // The SWM notifying us that the registration was removed on the MT may
  // race with ClearServiceWorkerRegistration() on the worker thread.  So
  // double-check that mOuter is still valid.
  if (mOuter) {
    mOuter->RegistrationRemoved();
  }
}

void
ServiceWorkerRegistrationWorkerThread::SetServiceWorkerRegistration(ServiceWorkerRegistration* aReg)
{
  MOZ_DIAGNOSTIC_ASSERT(aReg);
  MOZ_DIAGNOSTIC_ASSERT(!mOuter);
  mOuter = aReg;
  InitListener();
}

void
ServiceWorkerRegistrationWorkerThread::ClearServiceWorkerRegistration(ServiceWorkerRegistration* aReg)
{
  MOZ_ASSERT_IF(mOuter, mOuter == aReg);
  ReleaseListener();
  mOuter = nullptr;
}

RefPtr<ServiceWorkerRegistrationPromise>
ServiceWorkerRegistrationWorkerThread::Update()
{
  if (NS_WARN_IF(!mWorkerRef->GetPrivate())) {
    return ServiceWorkerRegistrationPromise::CreateAndReject(
      NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
  }

  RefPtr<StrongWorkerRef> workerRef =
    StrongWorkerRef::Create(mWorkerRef->GetPrivate(),
                            "ServiceWorkerRegistration::Update");
  if (NS_WARN_IF(!workerRef)) {
    return ServiceWorkerRegistrationPromise::CreateAndReject(
      NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
  }

  // Avoid infinite update loops by ignoring update() calls during top
  // level script evaluation.  See:
  // https://github.com/slightlyoff/ServiceWorker/issues/800
  if (workerRef->Private()->LoadScriptAsPartOfLoadingServiceWorkerScript()) {
    return ServiceWorkerRegistrationPromise::CreateAndResolve(mDescriptor,
                                                              __func__);
  }

  // Eventually we need to support all workers, but for right now this
  // code assumes we're on a service worker global as self.registration.
  if (NS_WARN_IF(!workerRef->Private()->IsServiceWorker())) {
    return ServiceWorkerRegistrationPromise::CreateAndReject(
      NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
  }

  RefPtr<ServiceWorkerRegistrationPromise::Private> outer =
    new ServiceWorkerRegistrationPromise::Private(__func__);

  RefPtr<SWRUpdateRunnable> r =
    new SWRUpdateRunnable(workerRef,
                          outer,
                          workerRef->Private()->GetServiceWorkerDescriptor());

  nsresult rv = workerRef->Private()->DispatchToMainThread(r.forget());
  if (NS_FAILED(rv)) {
    outer->Reject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
    return outer.forget();
  }

  return outer.forget();
}

RefPtr<GenericPromise>
ServiceWorkerRegistrationWorkerThread::Unregister()
{
  if (NS_WARN_IF(!mWorkerRef->GetPrivate())) {
    return GenericPromise::CreateAndReject(
      NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
  }

  RefPtr<StrongWorkerRef> workerRef =
    StrongWorkerRef::Create(mWorkerRef->GetPrivate(), __func__);
  if (NS_WARN_IF(!workerRef)) {
    return GenericPromise::CreateAndReject(
      NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
  }

  // Eventually we need to support all workers, but for right now this
  // code assumes we're on a service worker global as self.registration.
  if (NS_WARN_IF(!workerRef->Private()->IsServiceWorker())) {
    return GenericPromise::CreateAndReject(
      NS_ERROR_DOM_SECURITY_ERR, __func__);
  }

  RefPtr<GenericPromise::Private> outer = new GenericPromise::Private(__func__);

  RefPtr<StartUnregisterRunnable> r =
    new StartUnregisterRunnable(workerRef, outer, mDescriptor);

  nsresult rv = workerRef->Private()->DispatchToMainThread(r);
  if (NS_FAILED(rv)) {
    outer->Reject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
    return outer.forget();
  }

  return outer.forget();
}

void
ServiceWorkerRegistrationWorkerThread::InitListener()
{
  MOZ_ASSERT(!mListener);
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  RefPtr<ServiceWorkerRegistrationWorkerThread> self = this;
  mWorkerRef = WeakWorkerRef::Create(worker, [self]() {
    self->ReleaseListener();

    // Break the ref-cycle immediately when the worker thread starts to
    // teardown.  We must make sure its GC'd before the worker RuntimeService is
    // destroyed.  The WorkerListener may not be able to post a runnable
    // clearing this value after shutdown begins and thus delaying cleanup too
    // late.
    self->mOuter = nullptr;
  });

  if (NS_WARN_IF(!mWorkerRef)) {
    return;
  }

  mListener = new WorkerListener(this, mScope);

  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod("dom::WorkerListener::StartListeningForEvents",
                      mListener,
                      &WorkerListener::StartListeningForEvents);
  MOZ_ALWAYS_SUCCEEDS(worker->DispatchToMainThread(r.forget()));
}

void
ServiceWorkerRegistrationWorkerThread::ReleaseListener()
{
  if (!mListener) {
    return;
  }

  MOZ_ASSERT(IsCurrentThreadRunningWorker());

  mListener->ClearRegistration();

  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod("dom::WorkerListener::StopListeningForEvents",
                      mListener,
                      &WorkerListener::StopListeningForEvents);
  // Calling GetPrivate() is safe because this method is called when the
  // WorkerRef is notified.
  MOZ_ALWAYS_SUCCEEDS(mWorkerRef->GetPrivate()->DispatchToMainThread(r.forget()));

  mListener = nullptr;
  mWorkerRef = nullptr;
}

class FireUpdateFoundRunnable final : public WorkerRunnable
{
  RefPtr<WorkerListener> mListener;
public:
  FireUpdateFoundRunnable(WorkerPrivate* aWorkerPrivate,
                          WorkerListener* aListener)
    : WorkerRunnable(aWorkerPrivate)
    , mListener(aListener)
  {
    // Need this assertion for now since runnables which modify busy count can
    // only be dispatched from parent thread to worker thread and we don't deal
    // with nested workers. SW threads can't be nested.
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    mListener->UpdateFound();
    return true;
  }
};

void
WorkerListener::UpdateFound()
{
  MutexAutoLock lock(mMutex);
  if (!mRegistration) {
    return;
  }

  if (NS_IsMainThread()) {
    RefPtr<FireUpdateFoundRunnable> r =
      new FireUpdateFoundRunnable(mRegistration->GetWorkerPrivate(lock), this);
    Unused << NS_WARN_IF(!r->Dispatch());
    return;
  }

  mRegistration->UpdateFound();
}

class RegistrationRemovedWorkerRunnable final : public WorkerRunnable
{
  RefPtr<WorkerListener> mListener;
public:
  RegistrationRemovedWorkerRunnable(WorkerPrivate* aWorkerPrivate,
                                    WorkerListener* aListener)
    : WorkerRunnable(aWorkerPrivate)
    , mListener(aListener)
  {
    // Need this assertion for now since runnables which modify busy count can
    // only be dispatched from parent thread to worker thread and we don't deal
    // with nested workers. SW threads can't be nested.
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    mListener->RegistrationRemoved();
    return true;
  }
};

void
WorkerListener::RegistrationRemoved()
{
  MutexAutoLock lock(mMutex);
  if (!mRegistration) {
    return;
  }

  if (NS_IsMainThread()) {
    RefPtr<WorkerRunnable> r =
      new RegistrationRemovedWorkerRunnable(mRegistration->GetWorkerPrivate(lock), this);
    Unused << r->Dispatch();

    StopListeningForEvents();
    return;
  }

  mRegistration->RegistrationRemoved();
}

void
ServiceWorkerRegistrationWorkerThread::UpdateFound()
{
  mOuter->DispatchTrustedEvent(NS_LITERAL_STRING("updatefound"));
}

WorkerPrivate*
ServiceWorkerRegistrationWorkerThread::GetWorkerPrivate(const MutexAutoLock& aProofOfLock)
{
  // In this case, calling GetUnsafePrivate() is ok because we have a proof of
  // mutex lock.
  MOZ_ASSERT(mWorkerRef && mWorkerRef->GetUnsafePrivate());
  return mWorkerRef->GetUnsafePrivate();
}

} // dom namespace
} // mozilla namespace
