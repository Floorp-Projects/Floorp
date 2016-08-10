/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerRegistration.h"

#include "ipc/ErrorIPCUtils.h"
#include "mozilla/dom/Notification.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/ServiceWorkerRegistrationBinding.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "nsCycleCollectionParticipant.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "ServiceWorker.h"
#include "ServiceWorkerManager.h"

#include "nsIDocument.h"
#include "nsIServiceWorkerManager.h"
#include "nsISupportsPrimitives.h"
#include "nsPIDOMWindow.h"
#include "nsContentUtils.h"

#include "WorkerPrivate.h"
#include "Workers.h"
#include "WorkerScope.h"

#ifndef MOZ_SIMPLEPUSH
#include "mozilla/dom/PushManagerBinding.h"
#include "mozilla/dom/PushManager.h"
#endif

using namespace mozilla::dom::workers;

namespace mozilla {
namespace dom {

/* static */ bool
ServiceWorkerRegistration::Visible(JSContext* aCx, JSObject* aObj)
{
  if (NS_IsMainThread()) {
    return Preferences::GetBool("dom.serviceWorkers.enabled", false);
  }

  // Otherwise check the pref via the work private helper
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  if (!workerPrivate) {
    return false;
  }

  return workerPrivate->ServiceWorkersEnabled();
}

/* static */ bool
ServiceWorkerRegistration::NotificationAPIVisible(JSContext* aCx, JSObject* aObj)
{
  if (NS_IsMainThread()) {
    return Preferences::GetBool("dom.webnotifications.serviceworker.enabled", false);
  }

  // Otherwise check the pref via the work private helper
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  if (!workerPrivate) {
    return false;
  }

  return workerPrivate->DOMServiceWorkerNotificationEnabled();
}

////////////////////////////////////////////////////
// Main Thread implementation

class ServiceWorkerRegistrationMainThread final : public ServiceWorkerRegistration,
                                                  public ServiceWorkerRegistrationListener
{
  friend nsPIDOMWindowInner;
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorkerRegistrationMainThread,
                                           ServiceWorkerRegistration)

  ServiceWorkerRegistrationMainThread(nsPIDOMWindowInner* aWindow,
                                      const nsAString& aScope);

  already_AddRefed<Promise>
  Update(ErrorResult& aRv) override;

  already_AddRefed<Promise>
  Unregister(ErrorResult& aRv) override;

  // Partial interface from Notification API.
  already_AddRefed<Promise>
  ShowNotification(JSContext* aCx,
                   const nsAString& aTitle,
                   const NotificationOptions& aOptions,
                   ErrorResult& aRv) override;

  already_AddRefed<Promise>
  GetNotifications(const GetNotificationOptions& aOptions,
                   ErrorResult& aRv) override;

  already_AddRefed<ServiceWorker>
  GetInstalling() override;

  already_AddRefed<ServiceWorker>
  GetWaiting() override;

  already_AddRefed<ServiceWorker>
  GetActive() override;

  already_AddRefed<PushManager>
  GetPushManager(JSContext* aCx, ErrorResult& aRv) override;

  // DOMEventTargethelper
  void DisconnectFromOwner() override
  {
    StopListeningForEvents();
    ServiceWorkerRegistration::DisconnectFromOwner();
  }

  // ServiceWorkerRegistrationListener
  void
  UpdateFound() override;

  void
  InvalidateWorkers(WhichServiceWorker aWhichOnes) override;

  void
  RegistrationRemoved() override;

  void
  GetScope(nsAString& aScope) const override
  {
    aScope = mScope;
  }

private:
  ~ServiceWorkerRegistrationMainThread();

  already_AddRefed<ServiceWorker>
  GetWorkerReference(WhichServiceWorker aWhichOne);

  void
  StartListeningForEvents();

  void
  StopListeningForEvents();

  bool mListeningForEvents;

  // The following properties are cached here to ensure JS equality is satisfied
  // instead of acquiring a new worker instance from the ServiceWorkerManager
  // for every access. A null value is considered a cache miss.
  // These three may change to a new worker at any time.
  RefPtr<ServiceWorker> mInstallingWorker;
  RefPtr<ServiceWorker> mWaitingWorker;
  RefPtr<ServiceWorker> mActiveWorker;

#ifndef MOZ_SIMPLEPUSH
  RefPtr<PushManager> mPushManager;
#endif
};

NS_IMPL_ADDREF_INHERITED(ServiceWorkerRegistrationMainThread, ServiceWorkerRegistration)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerRegistrationMainThread, ServiceWorkerRegistration)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ServiceWorkerRegistrationMainThread)
NS_INTERFACE_MAP_END_INHERITING(ServiceWorkerRegistration)

#ifndef MOZ_SIMPLEPUSH
NS_IMPL_CYCLE_COLLECTION_INHERITED(ServiceWorkerRegistrationMainThread,
                                   ServiceWorkerRegistration,
                                   mPushManager,
                                   mInstallingWorker, mWaitingWorker, mActiveWorker);
#else
NS_IMPL_CYCLE_COLLECTION_INHERITED(ServiceWorkerRegistrationMainThread,
                                   ServiceWorkerRegistration,
                                   mInstallingWorker, mWaitingWorker, mActiveWorker);
#endif

ServiceWorkerRegistrationMainThread::ServiceWorkerRegistrationMainThread(nsPIDOMWindowInner* aWindow,
                                                                         const nsAString& aScope)
  : ServiceWorkerRegistration(aWindow, aScope)
  , mListeningForEvents(false)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsInnerWindow());
  StartListeningForEvents();
}

ServiceWorkerRegistrationMainThread::~ServiceWorkerRegistrationMainThread()
{
  StopListeningForEvents();
  MOZ_ASSERT(!mListeningForEvents);
}


already_AddRefed<ServiceWorker>
ServiceWorkerRegistrationMainThread::GetWorkerReference(WhichServiceWorker aWhichOne)
{
  nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
  if (!window) {
    return nullptr;
  }

  nsresult rv;
  nsCOMPtr<nsIServiceWorkerManager> swm =
    do_GetService(SERVICEWORKERMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  nsCOMPtr<nsISupports> serviceWorker;
  switch(aWhichOne) {
    case WhichServiceWorker::INSTALLING_WORKER:
      rv = swm->GetInstalling(window, mScope, getter_AddRefs(serviceWorker));
      break;
    case WhichServiceWorker::WAITING_WORKER:
      rv = swm->GetWaiting(window, mScope, getter_AddRefs(serviceWorker));
      break;
    case WhichServiceWorker::ACTIVE_WORKER:
      rv = swm->GetActive(window, mScope, getter_AddRefs(serviceWorker));
      break;
    default:
      MOZ_CRASH("Invalid enum value");
  }

  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv) || rv == NS_ERROR_DOM_NOT_FOUND_ERR,
                   "Unexpected error getting service worker instance from ServiceWorkerManager");
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  RefPtr<ServiceWorker> ref =
    static_cast<ServiceWorker*>(serviceWorker.get());
  return ref.forget();
}

// XXXnsm, maybe this can be optimized to only add when a event handler is
// registered.
void
ServiceWorkerRegistrationMainThread::StartListeningForEvents()
{
  AssertIsOnMainThread();
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
  AssertIsOnMainThread();
  if (!mListeningForEvents) {
    return;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (swm) {
    swm->RemoveRegistrationEventListener(mScope, this);
  }
  mListeningForEvents = false;
}

already_AddRefed<ServiceWorker>
ServiceWorkerRegistrationMainThread::GetInstalling()
{
  AssertIsOnMainThread();
  if (!mInstallingWorker) {
    mInstallingWorker = GetWorkerReference(WhichServiceWorker::INSTALLING_WORKER);
  }

  RefPtr<ServiceWorker> ret = mInstallingWorker;
  return ret.forget();
}

already_AddRefed<ServiceWorker>
ServiceWorkerRegistrationMainThread::GetWaiting()
{
  AssertIsOnMainThread();
  if (!mWaitingWorker) {
    mWaitingWorker = GetWorkerReference(WhichServiceWorker::WAITING_WORKER);
  }

  RefPtr<ServiceWorker> ret = mWaitingWorker;
  return ret.forget();
}

already_AddRefed<ServiceWorker>
ServiceWorkerRegistrationMainThread::GetActive()
{
  AssertIsOnMainThread();
  if (!mActiveWorker) {
    mActiveWorker = GetWorkerReference(WhichServiceWorker::ACTIVE_WORKER);
  }

  RefPtr<ServiceWorker> ret = mActiveWorker;
  return ret.forget();
}

void
ServiceWorkerRegistrationMainThread::UpdateFound()
{
  DispatchTrustedEvent(NS_LITERAL_STRING("updatefound"));
}

void
ServiceWorkerRegistrationMainThread::InvalidateWorkers(WhichServiceWorker aWhichOnes)
{
  AssertIsOnMainThread();
  if (aWhichOnes & WhichServiceWorker::INSTALLING_WORKER) {
    mInstallingWorker = nullptr;
  }

  if (aWhichOnes & WhichServiceWorker::WAITING_WORKER) {
    mWaitingWorker = nullptr;
  }

  if (aWhichOnes & WhichServiceWorker::ACTIVE_WORKER) {
    mActiveWorker = nullptr;
  }

}

void
ServiceWorkerRegistrationMainThread::RegistrationRemoved()
{
  // If the registration is being removed completely, remove it from the
  // window registration hash table so that a new registration would get a new
  // wrapper JS object.
  if (nsCOMPtr<nsPIDOMWindowInner> window = GetOwner()) {
    window->InvalidateServiceWorkerRegistration(mScope);
  }
}

namespace {

void
UpdateInternal(nsIPrincipal* aPrincipal,
               const nsAString& aScope,
               ServiceWorkerUpdateFinishCallback* aCallback)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aCallback);

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  MOZ_ASSERT(swm);

  swm->Update(aPrincipal, NS_ConvertUTF16toUTF8(aScope), aCallback);
}

class MainThreadUpdateCallback final : public ServiceWorkerUpdateFinishCallback
{
  RefPtr<Promise> mPromise;

  ~MainThreadUpdateCallback()
  { }

public:
  explicit MainThreadUpdateCallback(Promise* aPromise)
    : mPromise(aPromise)
  {
    AssertIsOnMainThread();
  }

  void
  UpdateSucceeded(ServiceWorkerRegistrationInfo* aRegistration) override
  {
    mPromise->MaybeResolveWithUndefined();
  }

  void
  UpdateFailed(ErrorResult& aStatus) override
  {
    mPromise->MaybeReject(aStatus);
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
    AssertIsOnMainThread();
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

class UpdateRunnable final : public Runnable
{
public:
  UpdateRunnable(PromiseWorkerProxy* aPromiseProxy,
                 const nsAString& aScope)
    : mPromiseProxy(aPromiseProxy)
    , mScope(aScope)
  {}

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();
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

    RefPtr<WorkerThreadUpdateCallback> cb =
      new WorkerThreadUpdateCallback(mPromiseProxy);
    UpdateInternal(principal, mScope, cb);
    return NS_OK;
  }

private:
  ~UpdateRunnable()
  {}

  RefPtr<PromiseWorkerProxy> mPromiseProxy;
  const nsString mScope;
};

class UnregisterCallback final : public nsIServiceWorkerUnregisterCallback
{
  RefPtr<Promise> mPromise;

public:
  NS_DECL_ISUPPORTS

  explicit UnregisterCallback(Promise* aPromise)
    : mPromise(aPromise)
  {
    MOZ_ASSERT(mPromise);
  }

  NS_IMETHODIMP
  UnregisterSucceeded(bool aState) override
  {
    AssertIsOnMainThread();
    mPromise->MaybeResolve(aState);
    return NS_OK;
  }

  NS_IMETHODIMP
  UnregisterFailed() override
  {
    AssertIsOnMainThread();

    mPromise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
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
                                   Maybe<bool> aState)
    : WorkerRunnable(aProxy->GetWorkerPrivate())
    , mPromiseWorkerProxy(aProxy)
    , mState(aState)
  {
    AssertIsOnMainThread();
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

  NS_IMETHODIMP
  UnregisterSucceeded(bool aState) override
  {
    AssertIsOnMainThread();
    Finish(Some(aState));
    return NS_OK;
  }

  NS_IMETHODIMP
  UnregisterFailed() override
  {
    AssertIsOnMainThread();
    Finish(Nothing());
    return NS_OK;
  }

private:
  ~WorkerUnregisterCallback()
  {}

  void
  Finish(Maybe<bool> aState)
  {
    AssertIsOnMainThread();
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
  StartUnregisterRunnable(PromiseWorkerProxy* aProxy,
                          const nsAString& aScope)
    : mPromiseWorkerProxy(aProxy)
    , mScope(aScope)
  {
    MOZ_ASSERT(aProxy);
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();

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
  AssertIsOnMainThread();
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(GetOwner());
  if (!go) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(go, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsCOMPtr<nsIDocument> doc = GetOwner()->GetExtantDoc();
  MOZ_ASSERT(doc);

  RefPtr<MainThreadUpdateCallback> cb =
    new MainThreadUpdateCallback(promise);
  UpdateInternal(doc->NodePrincipal(), mScope, cb);

  return promise.forget();
}

already_AddRefed<Promise>
ServiceWorkerRegistrationMainThread::Unregister(ErrorResult& aRv)
{
  AssertIsOnMainThread();
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(GetOwner());
  if (!go) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // Although the spec says that the same-origin checks should also be done
  // asynchronously, we do them in sync because the Promise created by the
  // WebIDL infrastructure due to a returned error will be resolved
  // asynchronously. We aren't making any internal state changes in these
  // checks, so ordering of multiple calls is not affected.
  nsCOMPtr<nsIDocument> document = GetOwner()->GetExtantDoc();
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

  RefPtr<UnregisterCallback> cb = new UnregisterCallback(promise);

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
  AssertIsOnMainThread();
  nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
  if (NS_WARN_IF(!window)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<ServiceWorker> worker = GetActive();
  if (!worker) {
    aRv.ThrowTypeError<MSG_NO_ACTIVE_WORKER>(mScope);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(window);
  RefPtr<Promise> p =
    Notification::ShowPersistentNotification(aCx, global, mScope, aTitle,
                                             aOptions, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return p.forget();
}

already_AddRefed<Promise>
ServiceWorkerRegistrationMainThread::GetNotifications(const GetNotificationOptions& aOptions, ErrorResult& aRv)
{
  AssertIsOnMainThread();
  nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
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
  AssertIsOnMainThread();

#ifdef MOZ_SIMPLEPUSH
  return nullptr;
#else

  if (!mPushManager) {
    nsCOMPtr<nsIGlobalObject> globalObject = do_QueryInterface(GetOwner());

    if (!globalObject) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    GlobalObject global(aCx, globalObject->GetGlobalJSObject());
    mPushManager = PushManager::Constructor(global, mScope, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  RefPtr<PushManager> ret = mPushManager;
  return ret.forget();

#endif /* ! MOZ_SIMPLEPUSH */
}

////////////////////////////////////////////////////
// Worker Thread implementation

class ServiceWorkerRegistrationWorkerThread final : public ServiceWorkerRegistration
                                                  , public WorkerHolder
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorkerRegistrationWorkerThread,
                                           ServiceWorkerRegistration)

  ServiceWorkerRegistrationWorkerThread(WorkerPrivate* aWorkerPrivate,
                                        const nsAString& aScope);

  already_AddRefed<Promise>
  Update(ErrorResult& aRv) override;

  already_AddRefed<Promise>
  Unregister(ErrorResult& aRv) override;

  // Partial interface from Notification API.
  already_AddRefed<Promise>
  ShowNotification(JSContext* aCx,
                   const nsAString& aTitle,
                   const NotificationOptions& aOptions,
                   ErrorResult& aRv) override;

  already_AddRefed<Promise>
  GetNotifications(const GetNotificationOptions& aOptions,
                   ErrorResult& aRv) override;

  already_AddRefed<ServiceWorker>
  GetInstalling() override;

  already_AddRefed<ServiceWorker>
  GetWaiting() override;

  already_AddRefed<ServiceWorker>
  GetActive() override;

  void
  GetScope(nsAString& aScope) const override
  {
    aScope = mScope;
  }

  bool
  Notify(Status aStatus) override;

  already_AddRefed<PushManager>
  GetPushManager(JSContext* aCx, ErrorResult& aRv) override;

private:
  enum Reason
  {
    RegistrationIsGoingAway = 0,
    WorkerIsGoingAway,
  };

  ~ServiceWorkerRegistrationWorkerThread();

  void
  InitListener();

  void
  ReleaseListener(Reason aReason);

  WorkerPrivate* mWorkerPrivate;
  RefPtr<WorkerListener> mListener;

#ifndef MOZ_SIMPLEPUSH
  RefPtr<PushManager> mPushManager;
#endif
};

class WorkerListener final : public ServiceWorkerRegistrationListener
{
  // Accessed on the main thread.
  WorkerPrivate* mWorkerPrivate;
  nsString mScope;
  bool mListeningForEvents;

  // Accessed on the worker thread.
  ServiceWorkerRegistrationWorkerThread* mRegistration;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WorkerListener, override)

  WorkerListener(WorkerPrivate* aWorkerPrivate,
                 ServiceWorkerRegistrationWorkerThread* aReg)
    : mWorkerPrivate(aWorkerPrivate)
    , mListeningForEvents(false)
    , mRegistration(aReg)
  {
    MOZ_ASSERT(mWorkerPrivate);
    mWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(mRegistration);
    // Copy scope so we can return it on the main thread.
    mRegistration->GetScope(mScope);
  }

  void
  StartListeningForEvents()
  {
    AssertIsOnMainThread();
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
    AssertIsOnMainThread();

    MOZ_ASSERT(mListeningForEvents);

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
  InvalidateWorkers(WhichServiceWorker aWhichOnes) override
  {
    AssertIsOnMainThread();
    // FIXME(nsm);
  }

  void
  RegistrationRemoved() override
  {
    AssertIsOnMainThread();
  }

  void
  GetScope(nsAString& aScope) const override
  {
    aScope = mScope;
  }

  ServiceWorkerRegistrationWorkerThread*
  GetRegistration() const
  {
    if (mWorkerPrivate) {
      mWorkerPrivate->AssertIsOnWorkerThread();
    }
    return mRegistration;
  }

  void
  ClearRegistration()
  {
    if (mWorkerPrivate) {
      mWorkerPrivate->AssertIsOnWorkerThread();
    }
    mRegistration = nullptr;
  }

private:
  ~WorkerListener()
  {
    MOZ_ASSERT(!mListeningForEvents);
  }
};

NS_IMPL_ADDREF_INHERITED(ServiceWorkerRegistrationWorkerThread, ServiceWorkerRegistration)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerRegistrationWorkerThread, ServiceWorkerRegistration)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ServiceWorkerRegistrationWorkerThread)
NS_INTERFACE_MAP_END_INHERITING(ServiceWorkerRegistration)

// Expanded macros since we need special behaviour to release the proxy.
NS_IMPL_CYCLE_COLLECTION_CLASS(ServiceWorkerRegistrationWorkerThread)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ServiceWorkerRegistrationWorkerThread,
                                                  ServiceWorkerRegistration)
#ifndef MOZ_SIMPLEPUSH
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPushManager)
#endif
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ServiceWorkerRegistrationWorkerThread,
                                                ServiceWorkerRegistration)
#ifndef MOZ_SIMPLEPUSH
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPushManager)
#endif
  tmp->ReleaseListener(RegistrationIsGoingAway);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

ServiceWorkerRegistrationWorkerThread::ServiceWorkerRegistrationWorkerThread(WorkerPrivate* aWorkerPrivate,
                                                                             const nsAString& aScope)
  : ServiceWorkerRegistration(nullptr, aScope)
  , mWorkerPrivate(aWorkerPrivate)
{
  InitListener();
}

ServiceWorkerRegistrationWorkerThread::~ServiceWorkerRegistrationWorkerThread()
{
  ReleaseListener(RegistrationIsGoingAway);
  MOZ_ASSERT(!mListener);
}

already_AddRefed<workers::ServiceWorker>
ServiceWorkerRegistrationWorkerThread::GetInstalling()
{
  // FIXME(nsm): Will be implemented after Bug 1113522.
  return nullptr;
}

already_AddRefed<ServiceWorker>
ServiceWorkerRegistrationWorkerThread::GetWaiting()
{
  // FIXME(nsm): Will be implemented after Bug 1113522.
  return nullptr;
}

already_AddRefed<ServiceWorker>
ServiceWorkerRegistrationWorkerThread::GetActive()
{
  // FIXME(nsm): Will be implemented after Bug 1113522.
  return nullptr;
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

  RefPtr<UpdateRunnable> r = new UpdateRunnable(proxy, mScope);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(r));

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
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(r));

  return promise.forget();
}

class StartListeningRunnable final : public Runnable
{
  RefPtr<WorkerListener> mListener;
public:
  explicit StartListeningRunnable(WorkerListener* aListener)
    : mListener(aListener)
  {}

  NS_IMETHOD
  Run() override
  {
    mListener->StartListeningForEvents();
    return NS_OK;
  }
};

void
ServiceWorkerRegistrationWorkerThread::InitListener()
{
  MOZ_ASSERT(!mListener);
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  mListener = new WorkerListener(worker, this);
  if (!HoldWorker(worker)) {
    mListener = nullptr;
    NS_WARNING("Could not add feature");
    return;
  }

  RefPtr<StartListeningRunnable> r =
    new StartListeningRunnable(mListener);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(r));
}

class AsyncStopListeningRunnable final : public Runnable
{
  RefPtr<WorkerListener> mListener;
public:
  explicit AsyncStopListeningRunnable(WorkerListener* aListener)
    : mListener(aListener)
  {}

  NS_IMETHOD
  Run() override
  {
    mListener->StopListeningForEvents();
    return NS_OK;
  }
};

class SyncStopListeningRunnable final : public WorkerMainThreadRunnable
{
  RefPtr<WorkerListener> mListener;
public:
  SyncStopListeningRunnable(WorkerPrivate* aWorkerPrivate,
                            WorkerListener* aListener)
    : WorkerMainThreadRunnable(aWorkerPrivate,
                               NS_LITERAL_CSTRING("ServiceWorkerRegistration :: StopListening"))
    , mListener(aListener)
  {}

  bool
  MainThreadRun() override
  {
    mListener->StopListeningForEvents();
    return true;
  }
};

void
ServiceWorkerRegistrationWorkerThread::ReleaseListener(Reason aReason)
{
  if (!mListener) {
    return;
  }

  // We can assert worker here, because:
  // 1) We always HoldWorker, so if the worker has shutdown already, we'll
  //    have received Notify and removed it. If HoldWorker had failed,
  //    mListener will be null and we won't reach here.
  // 2) Otherwise, worker is still around even if we are going away.
  mWorkerPrivate->AssertIsOnWorkerThread();
  ReleaseWorker();

  mListener->ClearRegistration();

  if (aReason == RegistrationIsGoingAway) {
    RefPtr<AsyncStopListeningRunnable> r =
      new AsyncStopListeningRunnable(mListener);
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(r));
  } else if (aReason == WorkerIsGoingAway) {
    RefPtr<SyncStopListeningRunnable> r =
      new SyncStopListeningRunnable(mWorkerPrivate, mListener);
    ErrorResult rv;
    r->Dispatch(rv);
    if (rv.Failed()) {
      NS_ERROR("Failed to dispatch stop listening runnable!");
      // And now what?
      rv.SuppressException();
    }
  } else {
    MOZ_CRASH("Bad reason");
  }
  mListener = nullptr;
  mWorkerPrivate = nullptr;
}

bool
ServiceWorkerRegistrationWorkerThread::Notify(Status aStatus)
{
  ReleaseListener(WorkerIsGoingAway);
  return true;
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
      reg->DispatchTrustedEvent(NS_LITERAL_STRING("updatefound"));
    }
    return true;
  }
};

void
WorkerListener::UpdateFound()
{
  AssertIsOnMainThread();
  if (mWorkerPrivate) {
    RefPtr<FireUpdateFoundRunnable> r =
      new FireUpdateFoundRunnable(mWorkerPrivate, this);
    NS_WARN_IF(!r->Dispatch());
  }
}

// Notification API extension.
already_AddRefed<Promise>
ServiceWorkerRegistrationWorkerThread::ShowNotification(JSContext* aCx,
                                                        const nsAString& aTitle,
                                                        const NotificationOptions& aOptions,
                                                        ErrorResult& aRv)
{
  // Until Bug 1131324 exposes ServiceWorkerContainer on workers,
  // ShowPersistentNotification() checks for valid active worker while it is
  // also verifying scope so that we block the worker on the main thread only
  // once.
  RefPtr<Promise> p =
    Notification::ShowPersistentNotification(aCx, mWorkerPrivate->GlobalScope(),
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
  return Notification::WorkerGet(mWorkerPrivate, aOptions, mScope, aRv);
}

already_AddRefed<PushManager>
ServiceWorkerRegistrationWorkerThread::GetPushManager(JSContext* aCx, ErrorResult& aRv)
{
#ifdef MOZ_SIMPLEPUSH
  return nullptr;
#else

  if (!mPushManager) {
    mPushManager = new PushManager(mScope);
  }

  RefPtr<PushManager> ret = mPushManager;
  return ret.forget();

#endif /* ! MOZ_SIMPLEPUSH */
}

////////////////////////////////////////////////////
// Base class implementation

NS_IMPL_ADDREF_INHERITED(ServiceWorkerRegistration, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerRegistration, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ServiceWorkerRegistration)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

ServiceWorkerRegistration::ServiceWorkerRegistration(nsPIDOMWindowInner* aWindow,
                                                     const nsAString& aScope)
  : DOMEventTargetHelper(aWindow)
  , mScope(aScope)
{}

JSObject*
ServiceWorkerRegistration::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto)
{
  return ServiceWorkerRegistrationBinding::Wrap(aCx, this, aGivenProto);
}

/* static */ already_AddRefed<ServiceWorkerRegistration>
ServiceWorkerRegistration::CreateForMainThread(nsPIDOMWindowInner* aWindow,
                                               const nsAString& aScope)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<ServiceWorkerRegistration> registration =
    new ServiceWorkerRegistrationMainThread(aWindow, aScope);

  return registration.forget();
}

/* static */ already_AddRefed<ServiceWorkerRegistration>
ServiceWorkerRegistration::CreateForWorker(workers::WorkerPrivate* aWorkerPrivate,
                                           const nsAString& aScope)
{
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<ServiceWorkerRegistration> registration =
    new ServiceWorkerRegistrationWorkerThread(aWorkerPrivate, aScope);

  return registration.forget();
}

} // dom namespace
} // mozilla namespace
