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

  StopListeningForEvents();

  // Since the registration is effectively dead in the SWM we can break
  // the ref-cycle and let the binding object clean up.
  mOuter = nullptr;
}

void
ServiceWorkerRegistrationMainThread::UpdateFound()
{
  mOuter->DispatchTrustedEvent(NS_LITERAL_STRING("updatefound"));
}

void
ServiceWorkerRegistrationMainThread::UpdateState(const ServiceWorkerRegistrationDescriptor& aDescriptor)
{
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
  PromiseWindowProxy mPromise;

  ~MainThreadUpdateCallback()
  { }

public:
  explicit MainThreadUpdateCallback(nsPIDOMWindowInner* aWindow,
                                    Promise* aPromise)
    : mPromise(aWindow, aPromise)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  void
  UpdateSucceeded(ServiceWorkerRegistrationInfo* aRegistration) override
  {
    RefPtr<Promise> promise = mPromise.Get();
    nsCOMPtr<nsPIDOMWindowInner> win = mPromise.GetWindow();
    if (!promise || !win) {
      return;
    }

    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "MainThreadUpdateCallback::UpdateSucceeded",
      [promise = Move(promise)] () {
        promise->MaybeResolveWithUndefined();
      });
    MOZ_ALWAYS_SUCCEEDS(
      win->EventTargetFor(TaskCategory::Other)->Dispatch(r.forget()));
  }

  void
  UpdateFailed(ErrorResult& aStatus) override
  {
    if (RefPtr<Promise> promise = mPromise.Get()) {
      promise->MaybeReject(aStatus);
    }
  }
};

class UpdateResultRunnable final : public WorkerRunnable
{
  RefPtr<PromiseWorkerProxy> mPromiseProxy;
  IPC::Message mSerializedErrorResult;

  ~UpdateResultRunnable()
  {}

public:
  UpdateResultRunnable(PromiseWorkerProxy* aPromiseProxy, ErrorResult& aStatus)
    : WorkerRunnable(aPromiseProxy->GetWorkerPrivate())
    , mPromiseProxy(aPromiseProxy)
  {
    // ErrorResult is not thread safe.  Serialize it for transfer across
    // threads.
    IPC::WriteParam(&mSerializedErrorResult, aStatus);
    aStatus.SuppressException();
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    // Deserialize the ErrorResult now that we are back in the worker
    // thread.
    ErrorResult status;
    PickleIterator iter = PickleIterator(mSerializedErrorResult);
    Unused << IPC::ReadParam(&mSerializedErrorResult, &iter, &status);

    Promise* promise = mPromiseProxy->WorkerPromise();
    if (status.Failed()) {
      promise->MaybeReject(status);
    } else {
      promise->MaybeResolveWithUndefined();
    }
    status.SuppressException();
    mPromiseProxy->CleanUp();
    return true;
  }
};

class WorkerThreadUpdateCallback final : public ServiceWorkerUpdateFinishCallback
{
  RefPtr<PromiseWorkerProxy> mPromiseProxy;

  ~WorkerThreadUpdateCallback()
  {
  }

public:
  explicit WorkerThreadUpdateCallback(PromiseWorkerProxy* aPromiseProxy)
    : mPromiseProxy(aPromiseProxy)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  void
  UpdateSucceeded(ServiceWorkerRegistrationInfo* aRegistration) override
  {
    ErrorResult rv(NS_OK);
    Finish(rv);
  }

  void
  UpdateFailed(ErrorResult& aStatus) override
  {
    Finish(aStatus);
  }

  void
  Finish(ErrorResult& aStatus)
  {
    if (!mPromiseProxy) {
      return;
    }

    RefPtr<PromiseWorkerProxy> proxy = mPromiseProxy.forget();

    MutexAutoLock lock(proxy->Lock());
    if (proxy->CleanedUp()) {
      return;
    }

    RefPtr<UpdateResultRunnable> r =
      new UpdateResultRunnable(proxy, aStatus);
    r->Dispatch();
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
  explicit SWRUpdateRunnable(PromiseWorkerProxy* aPromiseProxy)
    : Runnable("dom::SWRUpdateRunnable")
    , mPromiseProxy(aPromiseProxy)
    , mDescriptor(aPromiseProxy->GetWorkerPrivate()->GetServiceWorkerDescriptor())
    , mDelayed(false)
  {
    MOZ_ASSERT(mPromiseProxy);

    // This runnable is used for update calls originating from a worker thread,
    // which may be delayed in some cases.
    MOZ_ASSERT(mPromiseProxy->GetWorkerPrivate()->IsServiceWorker());
    MOZ_ASSERT(mPromiseProxy->GetWorkerPrivate());
    mPromiseProxy->GetWorkerPrivate()->AssertIsOnWorkerThread();
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    ErrorResult result;

    nsCOMPtr<nsIPrincipal> principal;
    // UpdateInternal may try to reject the promise synchronously leading
    // to a deadlock.
    {
      MutexAutoLock lock(mPromiseProxy->Lock());
      if (mPromiseProxy->CleanedUp()) {
        return NS_OK;
      }

      principal = mPromiseProxy->GetWorkerPrivate()->GetPrincipal();
    }
    MOZ_ASSERT(principal);

    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (NS_WARN_IF(!swm)) {
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
      worker->WorkerPrivate()->StoreISupports(timer);

      return NS_OK;
    }

    RefPtr<WorkerThreadUpdateCallback> cb =
      new WorkerThreadUpdateCallback(mPromiseProxy);
    UpdateInternal(principal, mDescriptor.Scope(), cb);
    return NS_OK;
  }

private:
  ~SWRUpdateRunnable()
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  RefPtr<PromiseWorkerProxy> mPromiseProxy;
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

already_AddRefed<Promise>
ServiceWorkerRegistrationMainThread::Update(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mOuter) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> go = mOuter->GetParentObject();
  if (!go) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(go, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsCOMPtr<nsIDocument> doc = mOuter->GetOwner()->GetExtantDoc();
  MOZ_ASSERT(doc);

  RefPtr<MainThreadUpdateCallback> cb =
    new MainThreadUpdateCallback(mOuter->GetOwner(), promise);
  UpdateInternal(doc->NodePrincipal(), NS_ConvertUTF16toUTF8(mScope), cb);

  return promise.forget();
}

already_AddRefed<Promise>
ServiceWorkerRegistrationMainThread::Unregister(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mOuter) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

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
  if (!mOuter) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

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
  if (!mOuter) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
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

  if (!mOuter) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

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
  // Accessed on the main thread.
  WorkerPrivate* mWorkerPrivate;
  const nsString mScope;
  bool mListeningForEvents;

  // Accessed on the worker thread.
  ServiceWorkerRegistrationWorkerThread* mRegistration;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WorkerListener, override)

  WorkerListener(WorkerPrivate* aWorkerPrivate,
                 ServiceWorkerRegistrationWorkerThread* aReg,
                 const nsAString& aScope)
    : mWorkerPrivate(aWorkerPrivate)
    , mScope(aScope)
    , mListeningForEvents(false)
    , mRegistration(aReg)
  {
    MOZ_ASSERT(mWorkerPrivate);
    mWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(mRegistration);
  }

  void
  StartListeningForEvents()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!mListeningForEvents);
    MOZ_ASSERT(mWorkerPrivate);
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

    // We aren't going to need this anymore and we shouldn't hold on since the
    // worker will go away soon.
    mWorkerPrivate = nullptr;

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

  ServiceWorkerRegistrationWorkerThread*
  GetRegistration() const
  {
    MOZ_ASSERT(IsCurrentThreadRunningWorker());
    return mRegistration;
  }

  void
  ClearRegistration()
  {
    MOZ_ASSERT(IsCurrentThreadRunningWorker());
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
  mOuter = nullptr;
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

already_AddRefed<Promise>
ServiceWorkerRegistrationWorkerThread::Update(ErrorResult& aRv)
{
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  RefPtr<Promise> promise = Promise::Create(worker->GlobalScope(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Avoid infinite update loops by ignoring update() calls during top
  // level script evaluation.  See:
  // https://github.com/slightlyoff/ServiceWorker/issues/800
  if (worker->LoadScriptAsPartOfLoadingServiceWorkerScript()) {
    promise->MaybeResolveWithUndefined();
    return promise.forget();
  }

  RefPtr<PromiseWorkerProxy> proxy = PromiseWorkerProxy::Create(worker, promise);
  if (!proxy) {
    aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
    return nullptr;
  }

  RefPtr<SWRUpdateRunnable> r = new SWRUpdateRunnable(proxy);
  MOZ_ALWAYS_SUCCEEDS(worker->DispatchToMainThread(r.forget()));

  return promise.forget();
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

  mListener = new WorkerListener(worker, this, mScope);

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

  if (!mWorkerRef) {
    mListener = nullptr;
    NS_WARNING("Could not add feature");
    return;
  }

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

    ServiceWorkerRegistrationWorkerThread* reg = mListener->GetRegistration();
    if (reg) {
      reg->UpdateFound();
    }
    return true;
  }
};

void
WorkerListener::UpdateFound()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mWorkerPrivate) {
    RefPtr<FireUpdateFoundRunnable> r =
      new FireUpdateFoundRunnable(mWorkerPrivate, this);
    Unused << NS_WARN_IF(!r->Dispatch());
  }
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

    ServiceWorkerRegistrationWorkerThread* reg = mListener->GetRegistration();
    if (reg) {
      reg->RegistrationRemoved();
    }
    return true;
  }
};

void
WorkerListener::RegistrationRemoved()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mWorkerPrivate) {
    return;
  }

  RefPtr<WorkerRunnable> r =
    new RegistrationRemovedWorkerRunnable(mWorkerPrivate, this);
  Unused << r->Dispatch();

  StopListeningForEvents();
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

} // dom namespace
} // mozilla namespace
