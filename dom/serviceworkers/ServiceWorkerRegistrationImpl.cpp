/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerRegistrationImpl.h"

#include "ipc/ErrorIPCUtils.h"
#include "mozilla/dom/DOMPrefs.h"
#include "mozilla/dom/Notification.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWindowProxy.h"
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
  PromiseWindowProxy mPromise;

public:
  NS_DECL_ISUPPORTS

  explicit UnregisterCallback(nsPIDOMWindowInner* aWindow,
                              Promise* aPromise)
    : mPromise(aWindow, aPromise)
  {
    MOZ_ASSERT(aPromise);
  }

  NS_IMETHOD
  UnregisterSucceeded(bool aState) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    RefPtr<Promise> promise = mPromise.Get();
    nsCOMPtr<nsPIDOMWindowInner> win = mPromise.GetWindow();
    if (!promise || !win) {
      return NS_OK;
    }

    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "UnregisterCallback::UnregisterSucceeded",
      [promise = Move(promise), aState] () {
        promise->MaybeResolve(aState);
      });
    MOZ_ALWAYS_SUCCEEDS(
      win->EventTargetFor(TaskCategory::Other)->Dispatch(r.forget()));
    return NS_OK;
  }

  NS_IMETHOD
  UnregisterFailed() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (RefPtr<Promise> promise = mPromise.Get()) {
      promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    }
    return NS_OK;
  }

private:
  ~UnregisterCallback()
  { }
};

NS_IMPL_ISUPPORTS(UnregisterCallback, nsIServiceWorkerUnregisterCallback)

class FulfillUnregisterPromiseRunnable final : public WorkerRunnable
{
  RefPtr<PromiseWorkerProxy> mPromiseWorkerProxy;
  Maybe<bool> mState;
public:
  FulfillUnregisterPromiseRunnable(PromiseWorkerProxy* aProxy,
                                   const Maybe<bool>& aState)
    : WorkerRunnable(aProxy->GetWorkerPrivate())
    , mPromiseWorkerProxy(aProxy)
    , mState(aState)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mPromiseWorkerProxy);
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    RefPtr<Promise> promise = mPromiseWorkerProxy->WorkerPromise();
    if (mState.isSome()) {
      promise->MaybeResolve(mState.value());
    } else {
      promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    }

    mPromiseWorkerProxy->CleanUp();
    return true;
  }
};

class WorkerUnregisterCallback final : public nsIServiceWorkerUnregisterCallback
{
  RefPtr<PromiseWorkerProxy> mPromiseWorkerProxy;
public:
  NS_DECL_ISUPPORTS

  explicit WorkerUnregisterCallback(PromiseWorkerProxy* aProxy)
    : mPromiseWorkerProxy(aProxy)
  {
    MOZ_ASSERT(aProxy);
  }

  NS_IMETHOD
  UnregisterSucceeded(bool aState) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    Finish(Some(aState));
    return NS_OK;
  }

  NS_IMETHOD
  UnregisterFailed() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    Finish(Nothing());
    return NS_OK;
  }

private:
  ~WorkerUnregisterCallback()
  {}

  void
  Finish(const Maybe<bool>& aState)
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (!mPromiseWorkerProxy) {
      return;
    }

    RefPtr<PromiseWorkerProxy> proxy = mPromiseWorkerProxy.forget();
    MutexAutoLock lock(proxy->Lock());
    if (proxy->CleanedUp()) {
      return;
    }

    RefPtr<WorkerRunnable> r =
      new FulfillUnregisterPromiseRunnable(proxy, aState);

    r->Dispatch();
  }
};

NS_IMPL_ISUPPORTS(WorkerUnregisterCallback, nsIServiceWorkerUnregisterCallback);

/*
 * If the worker goes away, we still continue to unregister, but we don't try to
 * resolve the worker Promise (which doesn't exist by that point).
 */
class StartUnregisterRunnable final : public Runnable
{
  RefPtr<PromiseWorkerProxy> mPromiseWorkerProxy;
  const nsString mScope;

public:
  StartUnregisterRunnable(PromiseWorkerProxy* aProxy, const nsAString& aScope)
    : Runnable("dom::StartUnregisterRunnable")
    , mPromiseWorkerProxy(aProxy)
    , mScope(aScope)
  {
    MOZ_ASSERT(aProxy);
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    // XXXnsm: There is a rare chance of this failing if the worker gets
    // destroyed. In that case, unregister() called from a SW is no longer
    // guaranteed to run. We should fix this by having a main thread proxy
    // maintain a strongref to ServiceWorkerRegistrationInfo and use its
    // principal. Can that be trusted?
    nsCOMPtr<nsIPrincipal> principal;
    {
      MutexAutoLock lock(mPromiseWorkerProxy->Lock());
      if (mPromiseWorkerProxy->CleanedUp()) {
        return NS_OK;
      }

      WorkerPrivate* worker = mPromiseWorkerProxy->GetWorkerPrivate();
      MOZ_ASSERT(worker);
      principal = worker->GetPrincipal();
    }
    MOZ_ASSERT(principal);

    RefPtr<WorkerUnregisterCallback> cb =
      new WorkerUnregisterCallback(mPromiseWorkerProxy);
    nsCOMPtr<nsIServiceWorkerManager> swm =
      mozilla::services::GetServiceWorkerManager();
    nsresult rv = swm->Unregister(principal, cb, mScope);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      cb->UnregisterFailed();
    }

    return NS_OK;
  }
};
} // namespace

RefPtr<ServiceWorkerRegistrationPromise>
ServiceWorkerRegistrationMainThread::Update(ErrorResult& aRv)
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

already_AddRefed<Promise>
ServiceWorkerRegistrationMainThread::Unregister(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(mOuter);

  nsCOMPtr<nsIGlobalObject> go = mOuter->GetParentObject();
  if (!go) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // Although the spec says that the same-origin checks should also be done
  // asynchronously, we do them in sync because the Promise created by the
  // WebIDL infrastructure due to a returned error will be resolved
  // asynchronously. We aren't making any internal state changes in these
  // checks, so ordering of multiple calls is not affected.
  nsCOMPtr<nsIDocument> document = mOuter->GetOwner()->GetExtantDoc();
  if (!document) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIURI> scopeURI;
  nsCOMPtr<nsIURI> baseURI = document->GetBaseURI();
  // "If the origin of scope is not client's origin..."
  nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), mScope, nullptr, baseURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIPrincipal> documentPrincipal = document->NodePrincipal();
  rv = documentPrincipal->CheckMayLoad(scopeURI, true /* report */,
                                       false /* allowIfInheritsPrinciple */);
  if (NS_FAILED(rv)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  nsAutoCString uriSpec;
  aRv = scopeURI->GetSpecIgnoringRef(uriSpec);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsCOMPtr<nsIServiceWorkerManager> swm =
    mozilla::services::GetServiceWorkerManager();

  RefPtr<Promise> promise = Promise::Create(go, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<UnregisterCallback> cb = new UnregisterCallback(mOuter->GetOwner(), promise);

  NS_ConvertUTF8toUTF16 scope(uriSpec);
  aRv = swm->Unregister(documentPrincipal, cb, scope);
  if (aRv.Failed()) {
    return nullptr;
  }

  return promise.forget();
}

// Notification API extension.
already_AddRefed<Promise>
ServiceWorkerRegistrationMainThread::ShowNotification(JSContext* aCx,
                                                      const nsAString& aTitle,
                                                      const NotificationOptions& aOptions,
                                                      ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(mOuter);

  nsCOMPtr<nsPIDOMWindowInner> window = mOuter->GetOwner();
  if (NS_WARN_IF(!window)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<ServiceWorker> worker = mOuter->GetActive();
  if (!worker) {
    aRv.ThrowTypeError<MSG_NO_ACTIVE_WORKER>(mScope);
    return nullptr;
  }

  RefPtr<Promise> p =
    Notification::ShowPersistentNotification(aCx, window->AsGlobal(), mScope,
                                             aTitle, aOptions, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return p.forget();
}

already_AddRefed<Promise>
ServiceWorkerRegistrationMainThread::GetNotifications(const GetNotificationOptions& aOptions, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(mOuter);

  nsCOMPtr<nsPIDOMWindowInner> window = mOuter->GetOwner();
  if (NS_WARN_IF(!window)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  return Notification::Get(window, aOptions, mScope, aRv);
}

already_AddRefed<PushManager>
ServiceWorkerRegistrationMainThread::GetPushManager(JSContext* aCx,
                                                    ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(mOuter);

  nsCOMPtr<nsIGlobalObject> globalObject = mOuter->GetParentObject();

  if (!globalObject) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  GlobalObject global(aCx, globalObject->GetGlobalJSObject());
  RefPtr<PushManager> ret = PushManager::Constructor(global, mScope, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  return ret.forget();
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
ServiceWorkerRegistrationWorkerThread::Update(ErrorResult& aRv)
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

already_AddRefed<Promise>
ServiceWorkerRegistrationWorkerThread::Unregister(ErrorResult& aRv)
{
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  if (!worker->IsServiceWorker()) {
    // For other workers, the registration probably originated from
    // getRegistration(), so we may have to validate origin etc. Let's do this
    // this later.
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(worker->GlobalScope(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<PromiseWorkerProxy> proxy = PromiseWorkerProxy::Create(worker, promise);
  if (!proxy) {
    aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
    return nullptr;
  }

  RefPtr<StartUnregisterRunnable> r = new StartUnregisterRunnable(proxy, mScope);
  MOZ_ALWAYS_SUCCEEDS(worker->DispatchToMainThread(r.forget()));

  return promise.forget();
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
    Unused << NS_WARN_IF(!r->Dispatch());

    StopListeningForEvents();
    return;
  }

  mRegistration->RegistrationRemoved();
}

// Notification API extension.
already_AddRefed<Promise>
ServiceWorkerRegistrationWorkerThread::ShowNotification(JSContext* aCx,
                                                        const nsAString& aTitle,
                                                        const NotificationOptions& aOptions,
                                                        ErrorResult& aRv)
{
  if (!mWorkerRef || !mWorkerRef->GetPrivate()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Until Bug 1131324 exposes ServiceWorkerContainer on workers,
  // ShowPersistentNotification() checks for valid active worker while it is
  // also verifying scope so that we block the worker on the main thread only
  // once.
  RefPtr<Promise> p =
    Notification::ShowPersistentNotification(aCx,
                                             mWorkerRef->GetPrivate()->GlobalScope(),
                                             mScope, aTitle, aOptions, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return p.forget();
}

already_AddRefed<Promise>
ServiceWorkerRegistrationWorkerThread::GetNotifications(const GetNotificationOptions& aOptions,
                                                        ErrorResult& aRv)
{
  MOZ_ASSERT(mWorkerRef && mWorkerRef->GetPrivate());
  return Notification::WorkerGet(mWorkerRef->GetPrivate(), aOptions, mScope, aRv);
}

already_AddRefed<PushManager>
ServiceWorkerRegistrationWorkerThread::GetPushManager(JSContext* aCx, ErrorResult& aRv)
{
  RefPtr<PushManager> ret = new PushManager(mScope);
  return ret.forget();
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
