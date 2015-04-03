/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerManager.h"

#include "nsIAppsService.h"
#include "nsIDOMEventTarget.h"
#include "nsIDocument.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamLoader.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsINetworkInterceptController.h"
#include "nsIMutableArray.h"
#include "nsPIDOMWindow.h"
#include "nsScriptLoader.h"
#include "nsDebug.h"

#include "jsapi.h"

#include "mozilla/LoadContext.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/InternalHeaders.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/unused.h"

#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsQueryObject.h"
#include "nsTArray.h"

#include "RuntimeService.h"
#include "ServiceWorker.h"
#include "ServiceWorkerClient.h"
#include "ServiceWorkerContainer.h"
#include "ServiceWorkerRegistration.h"
#include "ServiceWorkerScriptCache.h"
#include "ServiceWorkerEvents.h"
#include "WorkerInlines.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"

#ifdef PostMessage
#undef PostMessage
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

BEGIN_WORKERS_NAMESPACE

static_assert(nsIHttpChannelInternal::CORS_MODE_SAME_ORIGIN == static_cast<uint32_t>(RequestMode::Same_origin),
              "RequestMode enumeration value should match Necko CORS mode value.");
static_assert(nsIHttpChannelInternal::CORS_MODE_NO_CORS == static_cast<uint32_t>(RequestMode::No_cors),
              "RequestMode enumeration value should match Necko CORS mode value.");
static_assert(nsIHttpChannelInternal::CORS_MODE_CORS == static_cast<uint32_t>(RequestMode::Cors),
              "RequestMode enumeration value should match Necko CORS mode value.");
static_assert(nsIHttpChannelInternal::CORS_MODE_CORS_WITH_FORCED_PREFLIGHT == static_cast<uint32_t>(RequestMode::Cors_with_forced_preflight),
              "RequestMode enumeration value should match Necko CORS mode value.");

struct ServiceWorkerManager::PendingOperation
{
  nsCOMPtr<nsIRunnable> mRunnable;

  ServiceWorkerJobQueue* mQueue;
  nsRefPtr<ServiceWorkerJob> mJob;

  ServiceWorkerRegistrationData mRegistration;
};

namespace {

nsresult
PopulateRegistrationData(nsIPrincipal* aPrincipal,
                         const ServiceWorkerRegistrationInfo* aRegistration,
                         ServiceWorkerRegistrationData& aData)
{
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aRegistration);

  bool isNullPrincipal = true;
  nsresult rv = aPrincipal->GetIsNullPrincipal(&isNullPrincipal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // No null principals.
  if (NS_WARN_IF(isNullPrincipal)) {
    return NS_ERROR_FAILURE;
  }

  rv = PrincipalToPrincipalInfo(aPrincipal, &aData.principal());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aData.scope() = aRegistration->mScope;
  aData.scriptSpec() = aRegistration->mScriptSpec;

  if (aRegistration->mActiveWorker) {
    aData.currentWorkerURL() = aRegistration->mActiveWorker->ScriptSpec();
    aData.activeCacheName() = aRegistration->mActiveWorker->CacheName();
  }

  if (aRegistration->mWaitingWorker) {
    aData.waitingCacheName() = aRegistration->mWaitingWorker->CacheName();
  }

  return NS_OK;
}

} // Anonymous namespace

NS_IMPL_ISUPPORTS0(ServiceWorkerJob)
NS_IMPL_ISUPPORTS0(ServiceWorkerRegistrationInfo)

void
ServiceWorkerJob::Done(nsresult aStatus)
{
  if (NS_WARN_IF(NS_FAILED(aStatus))) {
    // Windows builds complain if the return value of NS_WARN_IF isn't used.
  }

  if (mQueue) {
    mQueue->Done(this);
  }
}

void
ServiceWorkerRegistrationInfo::Clear()
{
  if (mInstallingWorker) {
    // FIXME(nsm): Terminate installing worker.
    mInstallingWorker->UpdateState(ServiceWorkerState::Redundant);
    mInstallingWorker = nullptr;
    // FIXME(nsm): Abort any inflight requests from installing worker.
  }

  if (mWaitingWorker) {
    mWaitingWorker->UpdateState(ServiceWorkerState::Redundant);

    nsresult rv = serviceWorkerScriptCache::PurgeCache(mPrincipal,
                                                       mWaitingWorker->CacheName());
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to purge the waiting cache.");
    }

    mWaitingWorker = nullptr;
  }

  if (mActiveWorker) {
    mActiveWorker->UpdateState(ServiceWorkerState::Redundant);

    nsresult rv = serviceWorkerScriptCache::PurgeCache(mPrincipal,
                                                       mActiveWorker->CacheName());
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to purge the active cache.");
    }

    mActiveWorker = nullptr;
  }

  nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  MOZ_ASSERT(swm);
  swm->InvalidateServiceWorkerRegistrationWorker(this,
                                                 WhichServiceWorker::INSTALLING_WORKER |
                                                 WhichServiceWorker::WAITING_WORKER |
                                                 WhichServiceWorker::ACTIVE_WORKER);
}

ServiceWorkerRegistrationInfo::ServiceWorkerRegistrationInfo(const nsACString& aScope,
                                                             nsIPrincipal* aPrincipal)
  : mControlledDocumentsCounter(0)
  , mScope(aScope)
  , mPrincipal(aPrincipal)
  , mPendingUninstall(false)
{ }

ServiceWorkerRegistrationInfo::~ServiceWorkerRegistrationInfo()
{
  if (IsControllingDocuments()) {
    NS_WARNING("ServiceWorkerRegistrationInfo is still controlling documents. This can be a bug or a leak in ServiceWorker API or in any other API that takes the document alive.");
  }
}

//////////////////////////
// ServiceWorkerManager //
//////////////////////////

NS_IMPL_ADDREF(ServiceWorkerManager)
NS_IMPL_RELEASE(ServiceWorkerManager)

NS_INTERFACE_MAP_BEGIN(ServiceWorkerManager)
  NS_INTERFACE_MAP_ENTRY(nsIServiceWorkerManager)
  NS_INTERFACE_MAP_ENTRY(nsIIPCBackgroundChildCreateCallback)
  if (aIID.Equals(NS_GET_IID(ServiceWorkerManager)))
    foundInterface = static_cast<nsIServiceWorkerManager*>(this);
  else
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIServiceWorkerManager)
NS_INTERFACE_MAP_END

ServiceWorkerManager::ServiceWorkerManager()
  : mActor(nullptr)
{
  // Register this component to PBackground.
  MOZ_ALWAYS_TRUE(BackgroundChild::GetOrCreateForCurrentThread(this));

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    nsRefPtr<ServiceWorkerRegistrar> swr = ServiceWorkerRegistrar::Get();
    MOZ_ASSERT(swr);

    nsTArray<ServiceWorkerRegistrationData> data;
    swr->GetRegistrations(data);
    LoadRegistrations(data);
  }
}

ServiceWorkerManager::~ServiceWorkerManager()
{
  // The map will assert if it is not empty when destroyed.
  mServiceWorkerRegistrationInfos.Clear();
}

class ContinueLifecycleTask : public nsISupports
{
  NS_DECL_ISUPPORTS

protected:
  virtual ~ContinueLifecycleTask()
  { }

public:
  virtual void ContinueAfterWorkerEvent(bool aSuccess,
                                        bool aActivateImmediately) = 0;
};

NS_IMPL_ISUPPORTS0(ContinueLifecycleTask);

class ServiceWorkerRegisterJob;

class ContinueInstallTask final : public ContinueLifecycleTask
{
  nsRefPtr<ServiceWorkerRegisterJob> mJob;

public:
  explicit ContinueInstallTask(ServiceWorkerRegisterJob* aJob)
    : mJob(aJob)
  { }

  void ContinueAfterWorkerEvent(bool aSuccess, bool aActivateImmediately) override;
};

class ContinueActivateTask final : public ContinueLifecycleTask
{
  nsRefPtr<ServiceWorkerRegistrationInfo> mRegistration;

public:
  explicit ContinueActivateTask(ServiceWorkerRegistrationInfo* aReg)
    : mRegistration(aReg)
  { }

  void
  ContinueAfterWorkerEvent(bool aSuccess, bool aActivateImmediately /* unused */) override;
};

class ContinueLifecycleRunnable final : public nsRunnable
{
  nsMainThreadPtrHandle<ContinueLifecycleTask> mTask;
  bool mSuccess;
  bool mActivateImmediately;

public:
  ContinueLifecycleRunnable(const nsMainThreadPtrHandle<ContinueLifecycleTask>& aTask,
                            bool aSuccess,
                            bool aActivateImmediately)
    : mTask(aTask)
    , mSuccess(aSuccess)
    , mActivateImmediately(aActivateImmediately)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();
    mTask->ContinueAfterWorkerEvent(mSuccess, mActivateImmediately);
    return NS_OK;
  }
};

/*
 * Fires 'install' event on the ServiceWorkerGlobalScope. Modifies busy count
 * since it fires the event. This is ok since there can't be nested
 * ServiceWorkers, so the parent thread -> worker thread requirement for
 * runnables is satisfied.
 */
class LifecycleEventWorkerRunnable final : public WorkerRunnable
{
  nsString mEventName;
  const nsMainThreadPtrHandle<ServiceWorker> mServiceWorker;
  const nsMainThreadPtrHandle<ContinueLifecycleTask> mTask;

public:
  LifecycleEventWorkerRunnable(nsMainThreadPtrHandle<ServiceWorker>& aServiceWorker,
                               const nsString& aEventName,
                               const nsMainThreadPtrHandle<ContinueLifecycleTask>& aTask)
      : WorkerRunnable(aServiceWorker->GetWorkerPrivate(), WorkerThreadModifyBusyCount)
      , mEventName(aEventName)
      , mServiceWorker(aServiceWorker)
      , mTask(aTask)
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

class ServiceWorkerUpdateFinishCallback
{
protected:
  virtual ~ServiceWorkerUpdateFinishCallback()
  { }

public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerUpdateFinishCallback)

  virtual
  void UpdateSucceeded(ServiceWorkerRegistrationInfo* aInfo)
  { }

  virtual
  void UpdateFailed(nsresult aStatus)
  { }

  virtual
  void UpdateFailed(const ErrorEventInit& aDesc)
  { }
};

class ServiceWorkerResolveWindowPromiseOnUpdateCallback final : public ServiceWorkerUpdateFinishCallback
{
  nsRefPtr<nsPIDOMWindow> mWindow;
  // The promise "returned" by the call to Update up to
  // navigator.serviceWorker.register().
  nsRefPtr<Promise> mPromise;

  ~ServiceWorkerResolveWindowPromiseOnUpdateCallback()
  { }

public:
  ServiceWorkerResolveWindowPromiseOnUpdateCallback(nsPIDOMWindow* aWindow, Promise* aPromise)
    : mWindow(aWindow)
    , mPromise(aPromise)
  {
  }

  void
  UpdateSucceeded(ServiceWorkerRegistrationInfo* aInfo) override
  {
    nsRefPtr<ServiceWorkerRegistrationMainThread> swr =
      new ServiceWorkerRegistrationMainThread(mWindow,
                                              NS_ConvertUTF8toUTF16(aInfo->mScope));
    mPromise->MaybeResolve(swr);
  }

  void
  UpdateFailed(nsresult aStatus) override
  {
    mPromise->MaybeReject(aStatus);
  }

  void
  UpdateFailed(const ErrorEventInit& aErrorDesc) override
  {
    AutoJSAPI jsapi;
    jsapi.Init(mWindow);

    JSContext* cx = jsapi.cx();

    JS::Rooted<JS::Value> fnval(cx);
    if (!ToJSValue(cx, aErrorDesc.mFilename, &fnval)) {
      JS_ClearPendingException(cx);
      mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
      return;
    }
    JS::Rooted<JSString*> fn(cx, fnval.toString());

    JS::Rooted<JS::Value> msgval(cx);
    if (!ToJSValue(cx, aErrorDesc.mMessage, &msgval)) {
      JS_ClearPendingException(cx);
      mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
      return;
    }
    JS::Rooted<JSString*> msg(cx, msgval.toString());

    JS::Rooted<JS::Value> error(cx);
    if (!JS::CreateError(cx, JSEXN_ERR, JS::NullPtr(), fn, aErrorDesc.mLineno,
                         aErrorDesc.mColno, nullptr, msg, &error)) {
      JS_ClearPendingException(cx);
      mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
      return;
    }

    mPromise->MaybeReject(cx, error);
  }
};

class ContinueUpdateRunnable final : public nsRunnable
{
  nsMainThreadPtrHandle<nsISupports> mJob;
public:
  explicit ContinueUpdateRunnable(const nsMainThreadPtrHandle<nsISupports> aJob)
    : mJob(aJob)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD Run();
};

class CheckWorkerEvaluationAndContinueUpdateWorkerRunnable final : public WorkerRunnable
{
  const nsMainThreadPtrHandle<ServiceWorker> mServiceWorker;
  const nsMainThreadPtrHandle<nsISupports> mJob;
public:
  CheckWorkerEvaluationAndContinueUpdateWorkerRunnable(nsMainThreadPtrHandle<ServiceWorker>& aServiceWorker,
                                                       const nsMainThreadPtrHandle<nsISupports> aJob)
    : WorkerRunnable(aServiceWorker->GetWorkerPrivate(), WorkerThreadUnchangedBusyCount)
    , mServiceWorker(aServiceWorker)
    , mJob(aJob)
  {
    AssertIsOnMainThread();
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
    if (aWorkerPrivate->WorkerScriptExecutedSuccessfully()) {
      nsRefPtr<ContinueUpdateRunnable> r = new ContinueUpdateRunnable(mJob);
      nsresult rv = NS_DispatchToMainThread(r);
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to dispatch ContinueUpdateRunnable to main thread.");
      }
    }

    return true;
  }
};

namespace {
nsresult
GetRequiredScopeStringPrefix(const nsACString& aScriptSpec, nsACString& aPrefix)
{
  nsCOMPtr<nsIURI> scriptURI;
  nsresult rv = NS_NewURI(getter_AddRefs(scriptURI), aScriptSpec,
                 nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = scriptURI->GetPrePath(aPrefix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIURL> scriptURL(do_QueryInterface(scriptURI));
  if (NS_WARN_IF(!scriptURL)) {
    return rv;
  }

  nsAutoCString dir;
  rv = scriptURL->GetDirectory(dir);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aPrefix.Append(dir);
  return NS_OK;
}
} // anonymous namespace

class ServiceWorkerRegisterJob final : public ServiceWorkerJob,
                                       public serviceWorkerScriptCache::CompareCallback
{
  friend class ContinueInstallTask;

  nsCString mScope;
  nsCString mScriptSpec;
  nsRefPtr<ServiceWorkerRegistrationInfo> mRegistration;
  nsRefPtr<ServiceWorkerUpdateFinishCallback> mCallback;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsRefPtr<ServiceWorkerInfo> mUpdateAndInstallInfo;

  ~ServiceWorkerRegisterJob()
  { }

  enum
  {
    REGISTER_JOB = 0,
    UPDATE_JOB = 1,
  } mJobType;

public:
  NS_DECL_ISUPPORTS_INHERITED

  // [[Register]]
  ServiceWorkerRegisterJob(ServiceWorkerJobQueue* aQueue,
                           const nsCString& aScope,
                           const nsCString& aScriptSpec,
                           ServiceWorkerUpdateFinishCallback* aCallback,
                           nsIPrincipal* aPrincipal)
    : ServiceWorkerJob(aQueue)
    , mScope(aScope)
    , mScriptSpec(aScriptSpec)
    , mCallback(aCallback)
    , mPrincipal(aPrincipal)
    , mJobType(REGISTER_JOB)
  { }

  // [[Update]]
  ServiceWorkerRegisterJob(ServiceWorkerJobQueue* aQueue,
                           ServiceWorkerRegistrationInfo* aRegistration,
                           ServiceWorkerUpdateFinishCallback* aCallback)
    : ServiceWorkerJob(aQueue)
    , mRegistration(aRegistration)
    , mCallback(aCallback)
    , mJobType(UPDATE_JOB)
  { }

  void
  Start() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (!swm->HasBackgroundActor()) {
      nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableMethod(this, &ServiceWorkerRegisterJob::Start);
      swm->AppendPendingOperation(runnable);
      return;
    }

    if (mJobType == REGISTER_JOB) {
      mRegistration = swm->GetRegistration(mScope);

      if (mRegistration) {
        nsRefPtr<ServiceWorkerInfo> newest = mRegistration->Newest();
        if (newest && mScriptSpec.Equals(newest->ScriptSpec()) &&
            mScriptSpec.Equals(mRegistration->mScriptSpec)) {
          mRegistration->mPendingUninstall = false;
          Succeed();
          Done(NS_OK);
          return;
        }
      } else {
        mRegistration = swm->CreateNewRegistration(mScope, mPrincipal);
      }

      mRegistration->mScriptSpec = mScriptSpec;
      swm->StoreRegistration(mPrincipal, mRegistration);
    } else {
      MOZ_ASSERT(mJobType == UPDATE_JOB);
    }

    Update();
  }

  void
  ComparisonResult(nsresult aStatus, bool aInCacheAndEqual) override
  {
    if (NS_WARN_IF(NS_FAILED(aStatus))) {
      Fail(NS_ERROR_DOM_TYPE_ERR);
      return;
    }

    if (aInCacheAndEqual) {
      Succeed();
      Done(NS_OK);
      return;
    }

    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();

    // FIXME: Bug 1130101 - Read max scope from Service-Worker-Allowed header.
    nsAutoCString allowedPrefix;
    nsresult rv = GetRequiredScopeStringPrefix(mRegistration->mScriptSpec, allowedPrefix);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      Fail(NS_ERROR_DOM_SECURITY_ERR);
      return;
    }

    if (!StringBeginsWith(mRegistration->mScope, allowedPrefix)) {
      NS_WARNING("By default a service worker's scope is restricted to at or below it's script's location.");
      Fail(NS_ERROR_DOM_SECURITY_ERR);
      return;
    }

    nsAutoString cacheName;
    rv = serviceWorkerScriptCache::GenerateCacheName(cacheName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      Fail(NS_ERROR_DOM_TYPE_ERR);
      return;
    }

    // We have to create a ServiceWorker here simply to ensure there are no
    // errors. Ideally we should just pass this worker on to ContinueInstall.
    MOZ_ASSERT(!swm->mSetOfScopesBeingUpdated.Contains(mRegistration->mScope));
    swm->mSetOfScopesBeingUpdated.Put(mRegistration->mScope, true);

    MOZ_ASSERT(!mUpdateAndInstallInfo);
    mUpdateAndInstallInfo =
      new ServiceWorkerInfo(mRegistration, mRegistration->mScriptSpec,
                            cacheName);
    nsRefPtr<ServiceWorker> serviceWorker;
    rv = swm->CreateServiceWorker(mRegistration->mPrincipal,
                                  mUpdateAndInstallInfo,
                                  getter_AddRefs(serviceWorker));

    if (NS_WARN_IF(NS_FAILED(rv))) {
      swm->mSetOfScopesBeingUpdated.Remove(mRegistration->mScope);
      Fail(NS_ERROR_DOM_ABORT_ERR);
      return;
    }

    nsRefPtr<ServiceWorkerJob> upcasted = this;
    nsMainThreadPtrHandle<nsISupports> handle(
        new nsMainThreadPtrHolder<nsISupports>(upcasted));

    nsMainThreadPtrHandle<ServiceWorker> serviceWorkerHandle(
      new nsMainThreadPtrHolder<ServiceWorker>(serviceWorker));
    nsRefPtr<CheckWorkerEvaluationAndContinueUpdateWorkerRunnable> r =
      new CheckWorkerEvaluationAndContinueUpdateWorkerRunnable(serviceWorkerHandle, handle);
    AutoJSAPI jsapi;
    jsapi.Init();
    bool ok = r->Dispatch(jsapi.cx());
    if (NS_WARN_IF(!ok)) {
      swm->mSetOfScopesBeingUpdated.Remove(mRegistration->mScope);
      Fail(NS_ERROR_DOM_ABORT_ERR);
      return;
    }
  }

  // Public so our error handling code can use it.
  void
  Fail(const ErrorEventInit& aError)
  {
    MOZ_ASSERT(mCallback);
    mCallback->UpdateFailed(aError);
    FailCommon(NS_ERROR_DOM_JS_EXCEPTION);
  }

  // Public so our error handling code can continue with a successful worker.
  void
  ContinueInstall()
  {
    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    MOZ_ASSERT(swm->mSetOfScopesBeingUpdated.Contains(mRegistration->mScope));
    swm->mSetOfScopesBeingUpdated.Remove(mRegistration->mScope);
    // This is effectively the end of Step 4.3 of the [[Update]] algorithm.
    // The invocation of [[Install]] is not part of the atomic block.

    // Begin [[Install]] atomic step 4.
    if (mRegistration->mInstallingWorker) {
      // FIXME(nsm): Terminate and stuff
      mRegistration->mInstallingWorker->UpdateState(ServiceWorkerState::Redundant);
    }

    swm->InvalidateServiceWorkerRegistrationWorker(mRegistration,
                                                   WhichServiceWorker::INSTALLING_WORKER);

    mRegistration->mInstallingWorker = mUpdateAndInstallInfo.forget();
    mRegistration->mInstallingWorker->UpdateState(ServiceWorkerState::Installing);

    Succeed();

    // Step 4.6 "Queue a task..." for updatefound.
    nsCOMPtr<nsIRunnable> upr =
      NS_NewRunnableMethodWithArg<ServiceWorkerRegistrationInfo*>(swm,
                                                                  &ServiceWorkerManager::FireUpdateFound,
                                                                  mRegistration);
    NS_DispatchToMainThread(upr);

    nsRefPtr<ServiceWorker> serviceWorker;
    nsresult rv = swm->CreateServiceWorker(mRegistration->mPrincipal,
                                           mRegistration->mInstallingWorker,
                                           getter_AddRefs(serviceWorker));

    if (NS_WARN_IF(NS_FAILED(rv))) {
      ContinueAfterInstallEvent(false /* aSuccess */, false /* aActivateImmediately */);
      return;
    }

    nsMainThreadPtrHandle<ContinueLifecycleTask> handle(
        new nsMainThreadPtrHolder<ContinueLifecycleTask>(new ContinueInstallTask(this)));

    nsMainThreadPtrHandle<ServiceWorker> serviceWorkerHandle(
      new nsMainThreadPtrHolder<ServiceWorker>(serviceWorker));
    nsRefPtr<LifecycleEventWorkerRunnable> r =
      new LifecycleEventWorkerRunnable(serviceWorkerHandle, NS_LITERAL_STRING("install"), handle);

    AutoJSAPI jsapi;
    jsapi.Init();

    // This triggers Step 4.7 "Queue a task to run the following substeps..."
    // which sends the install event to the worker.
    r->Dispatch(jsapi.cx());
  }

private:
  void
  Update()
  {
    MOZ_ASSERT(mRegistration);
    nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableMethod(this, &ServiceWorkerRegisterJob::ContinueUpdate);
    NS_DispatchToMainThread(r);
  }

  // Aspects of (actually the whole algorithm) of [[Update]] after
  // "Run the following steps in parallel."
  void
  ContinueUpdate()
  {
    AssertIsOnMainThread();
    if (mRegistration->mInstallingWorker) {
      // FIXME(nsm): "Terminate installing worker".
      mRegistration->mInstallingWorker->UpdateState(ServiceWorkerState::Redundant);
      mRegistration->mInstallingWorker = nullptr;
    }

    nsRefPtr<ServiceWorkerInfo> workerInfo = mRegistration->Newest();
    nsAutoString cacheName;

    // 9.2.20 If newestWorker is not null, and newestWorker's script url is
    // equal to registration's registering script url and response is a
    // byte-for-byte match with the script resource of newestWorker...
    if (workerInfo && workerInfo->ScriptSpec().Equals(mRegistration->mScriptSpec)) {
      cacheName = workerInfo->CacheName();
    }

    nsresult rv =
      serviceWorkerScriptCache::Compare(mRegistration->mPrincipal, cacheName,
                                        NS_ConvertUTF8toUTF16(mRegistration->mScriptSpec),
                                        this);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return Fail(rv);
    }
  }

  void
  Succeed()
  {
    MOZ_ASSERT(mCallback);
    mCallback->UpdateSucceeded(mRegistration);
    mCallback = nullptr;
  }

  void
  FailCommon(nsresult aRv)
  {
    mUpdateAndInstallInfo = nullptr;
    if (mRegistration->mInstallingWorker) {
      nsresult rv = serviceWorkerScriptCache::PurgeCache(mRegistration->mPrincipal,
                                                         mRegistration->mInstallingWorker->CacheName());
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to purge the installing worker cache.");
      }
    }

    mCallback = nullptr;
    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    swm->MaybeRemoveRegistration(mRegistration);
    // Ensures that the job can't do anything useful from this point on.
    mRegistration = nullptr;
    Done(aRv);
  }

  // This MUST only be called when the job is still performing actions related
  // to registration or update. After the spec resolves the update promise, use
  // Done() with the failure code instead.
  void
  Fail(nsresult aRv)
  {
    MOZ_ASSERT(mCallback);
    mCallback->UpdateFailed(aRv);
    FailCommon(aRv);
  }

  void
  ContinueAfterInstallEvent(bool aInstallEventSuccess, bool aActivateImmediately)
  {
    if (!mRegistration->mInstallingWorker) {
      NS_WARNING("mInstallingWorker was null.");
      return Done(NS_ERROR_DOM_ABORT_ERR);
    }

    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();

    // "If installFailed is true"
    if (!aInstallEventSuccess) {
      mRegistration->mInstallingWorker->UpdateState(ServiceWorkerState::Redundant);
      mRegistration->mInstallingWorker = nullptr;
      swm->InvalidateServiceWorkerRegistrationWorker(mRegistration,
                                                     WhichServiceWorker::INSTALLING_WORKER);
      swm->MaybeRemoveRegistration(mRegistration);
      return Done(NS_ERROR_DOM_ABORT_ERR);
    }

    // "If registration's waiting worker is not null"
    if (mRegistration->mWaitingWorker) {
      // FIXME(nsm): Terminate
      mRegistration->mWaitingWorker->UpdateState(ServiceWorkerState::Redundant);

      nsresult rv = serviceWorkerScriptCache::PurgeCache(mRegistration->mPrincipal,
                                                         mRegistration->mWaitingWorker->CacheName());
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to purge the old waiting cache.");
      }
    }

    mRegistration->mWaitingWorker = mRegistration->mInstallingWorker.forget();
    mRegistration->mWaitingWorker->UpdateState(ServiceWorkerState::Installed);
    swm->InvalidateServiceWorkerRegistrationWorker(mRegistration,
                                                   WhichServiceWorker::INSTALLING_WORKER | WhichServiceWorker::WAITING_WORKER);

    // FIXME(nsm): Bug 982711 Deal with activateImmediately.
    NS_WARN_IF_FALSE(!aActivateImmediately, "Immediate activation using replace() is not supported yet");
    Done(NS_OK);
    // Activate() is invoked out of band of atomic.
    mRegistration->TryToActivate();
  }
};

NS_IMPL_ISUPPORTS_INHERITED0(ServiceWorkerRegisterJob, ServiceWorkerJob);

NS_IMETHODIMP
ContinueUpdateRunnable::Run()
{
  AssertIsOnMainThread();
  nsRefPtr<ServiceWorkerJob> job = static_cast<ServiceWorkerJob*>(mJob.get());
  nsRefPtr<ServiceWorkerRegisterJob> upjob = static_cast<ServiceWorkerRegisterJob*>(job.get());
  upjob->ContinueInstall();
  return NS_OK;
}

void
ContinueInstallTask::ContinueAfterWorkerEvent(bool aSuccess, bool aActivateImmediately)
{
  // This does not start the job immediately if there are other jobs in the
  // queue, which captures the "atomic" behaviour we want.
  mJob->ContinueAfterInstallEvent(aSuccess, aActivateImmediately);
}

// If we return an error code here, the ServiceWorkerContainer will
// automatically reject the Promise.
NS_IMETHODIMP
ServiceWorkerManager::Register(nsIDOMWindow* aWindow,
                               nsIURI* aScopeURI,
                               nsIURI* aScriptURI,
                               nsISupports** aPromise)
{
  AssertIsOnMainThread();

  // XXXnsm Don't allow chrome callers for now, we don't support chrome
  // ServiceWorkers.
  MOZ_ASSERT(!nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);

  nsCOMPtr<nsPIDOMWindow> outerWindow = window->GetOuterWindow();
  bool serviceWorkersTestingEnabled =
    outerWindow->GetServiceWorkersTestingEnabled();

  nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
  if (!doc) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> documentURI = doc->GetBaseURI();

  bool authenticatedOrigin = false;
  if (Preferences::GetBool("dom.serviceWorkers.testing.enabled") ||
      serviceWorkersTestingEnabled) {
    authenticatedOrigin = true;
  }

  nsresult rv;
  if (!authenticatedOrigin) {
    nsAutoCString scheme;
    rv = documentURI->GetScheme(scheme);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (scheme.EqualsLiteral("https") ||
        scheme.EqualsLiteral("file") ||
        scheme.EqualsLiteral("app")) {
      authenticatedOrigin = true;
    }
  }

  if (!authenticatedOrigin) {
    nsAutoCString host;
    rv = documentURI->GetHost(host);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (host.Equals("127.0.0.1") ||
        host.Equals("localhost") ||
        host.Equals("::1")) {
      authenticatedOrigin = true;
    }
  }

  if (!authenticatedOrigin) {
    bool isFile;
    rv = documentURI->SchemeIs("file", &isFile);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!isFile) {
      bool isHttps;
      rv = documentURI->SchemeIs("https", &isHttps);
      if (NS_WARN_IF(NS_FAILED(rv)) || !isHttps) {
        NS_WARNING("ServiceWorker registration from insecure websites is not allowed.");
        return NS_ERROR_DOM_SECURITY_ERR;
      }
    }
  }

  // Data URLs are not allowed.
  nsCOMPtr<nsIPrincipal> documentPrincipal = doc->NodePrincipal();

  rv = documentPrincipal->CheckMayLoad(aScriptURI, true /* report */,
                                       false /* allowIfInheritsPrincipal */);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  rv = documentPrincipal->CheckMayLoad(aScopeURI, true /* report */,
                                       false /* allowIfInheritsPrinciple */);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCString cleanedScope;
  rv = aScopeURI->GetSpecIgnoringRef(cleanedScope);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString spec;
  rv = aScriptURI->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIGlobalObject> sgo = do_QueryInterface(window);
  ErrorResult result;
  nsRefPtr<Promise> promise = Promise::Create(sgo, result);
  if (result.Failed()) {
    return result.ErrorCode();
  }

  ServiceWorkerJobQueue* queue = GetOrCreateJobQueue(cleanedScope);
  MOZ_ASSERT(queue);

  nsRefPtr<ServiceWorkerResolveWindowPromiseOnUpdateCallback> cb =
    new ServiceWorkerResolveWindowPromiseOnUpdateCallback(window, promise);

  nsRefPtr<ServiceWorkerRegisterJob> job =
    new ServiceWorkerRegisterJob(queue, cleanedScope, spec, cb, documentPrincipal);
  queue->Append(job);

  promise.forget(aPromise);
  return NS_OK;
}

void
ServiceWorkerManager::AppendPendingOperation(ServiceWorkerJobQueue* aQueue,
                                             ServiceWorkerJob* aJob)
{
  MOZ_ASSERT(!mActor);
  MOZ_ASSERT(aQueue);
  MOZ_ASSERT(aJob);

  PendingOperation* opt = mPendingOperations.AppendElement();
  opt->mQueue = aQueue;
  opt->mJob = aJob;
}

void
ServiceWorkerManager::AppendPendingOperation(nsIRunnable* aRunnable)
{
  MOZ_ASSERT(!mActor);
  MOZ_ASSERT(aRunnable);

  PendingOperation* opt = mPendingOperations.AppendElement();
  opt->mRunnable = aRunnable;
}

/*
 * Used to handle ExtendableEvent::waitUntil() and proceed with
 * installation/activation.
 */
class LifecycleEventPromiseHandler final : public PromiseNativeHandler
{
  nsMainThreadPtrHandle<ContinueLifecycleTask> mTask;
  bool mActivateImmediately;

  virtual
  ~LifecycleEventPromiseHandler()
  { }

public:
  LifecycleEventPromiseHandler(const nsMainThreadPtrHandle<ContinueLifecycleTask>& aTask,
                               bool aActivateImmediately)
    : mTask(aTask)
    , mActivateImmediately(aActivateImmediately)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    workerPrivate->AssertIsOnWorkerThread();

    nsRefPtr<ContinueLifecycleRunnable> r =
      new ContinueLifecycleRunnable(mTask, true /* success */, mActivateImmediately);
    NS_DispatchToMainThread(r);
  }

  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    workerPrivate->AssertIsOnWorkerThread();

    nsRefPtr<ContinueLifecycleRunnable> r =
      new ContinueLifecycleRunnable(mTask, false /* success */, mActivateImmediately);
    NS_DispatchToMainThread(r);
  }
};

bool
LifecycleEventWorkerRunnable::DispatchLifecycleEvent(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
{
  aWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());

  nsRefPtr<ExtendableEvent> event;
  nsRefPtr<EventTarget> target = aWorkerPrivate->GlobalScope();

  if (mEventName.EqualsASCII("install") || mEventName.EqualsASCII("activate")) {
    // FIXME(nsm): Bug 982787 pass previous active worker.
    ExtendableEventInit init;
    init.mBubbles = false;
    init.mCancelable = true;
    event = ExtendableEvent::Constructor(target, mEventName, init);
  } else {
    MOZ_CRASH("Unexpected lifecycle event");
  }

  event->SetTrusted(true);

  nsRefPtr<Promise> waitUntilPromise;

  ErrorResult result;
  result = target->DispatchDOMEvent(nullptr, event, nullptr, nullptr);

  nsCOMPtr<nsIGlobalObject> sgo = aWorkerPrivate->GlobalScope();
  WidgetEvent* internalEvent = event->GetInternalNSEvent();
  if (!result.Failed() && !internalEvent->mFlags.mExceptionHasBeenRisen) {
    waitUntilPromise = event->GetPromise();
    if (!waitUntilPromise) {
      ErrorResult result;
      waitUntilPromise =
        Promise::Resolve(sgo, aCx, JS::UndefinedHandleValue, result);
      if (NS_WARN_IF(result.Failed())) {
        return true;
      }
    }
  } else {
    // Continue with a canceled install.
    // Although the spec has different routines to deal with popping stuff
    // off it's internal queues, we can reuse the ContinueAfterInstallEvent()
    // logic.
    waitUntilPromise = Promise::Reject(sgo, aCx,
                                       JS::UndefinedHandleValue, result);
  }

  if (result.Failed()) {
    return false;
  }

  nsRefPtr<LifecycleEventPromiseHandler> handler =
    new LifecycleEventPromiseHandler(mTask, false /* activateImmediately */);
  waitUntilPromise->AppendNativeHandler(handler);
  return true;
}

void
ServiceWorkerRegistrationInfo::TryToActivate()
{
  if (!IsControllingDocuments()) {
    Activate();
  }
}

void
ContinueActivateTask::ContinueAfterWorkerEvent(bool aSuccess, bool aActivateImmediately /* unused */)
{
  mRegistration->FinishActivate(aSuccess);
}

void
ServiceWorkerRegistrationInfo::Activate()
{
  nsRefPtr<ServiceWorkerInfo> activatingWorker = mWaitingWorker;
  nsRefPtr<ServiceWorkerInfo> exitingWorker = mActiveWorker;

  nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  swm->InvalidateServiceWorkerRegistrationWorker(this, WhichServiceWorker::WAITING_WORKER | WhichServiceWorker::ACTIVE_WORKER);
  if (!activatingWorker) {
    return;
  }

  if (exitingWorker) {
    // FIXME(nsm): Wait for worker.
    // Terminate worker
    exitingWorker->UpdateState(ServiceWorkerState::Redundant);
    nsresult rv = serviceWorkerScriptCache::PurgeCache(mPrincipal,
                                                       exitingWorker->CacheName());
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to purge the activating cache.");
    }
  }

  mActiveWorker = activatingWorker.forget();
  mWaitingWorker = nullptr;
  mActiveWorker->UpdateState(ServiceWorkerState::Activating);

  // FIXME(nsm): Unlink appcache if there is one.

  swm->CheckPendingReadyPromises();

  // "Queue a task to fire a simple event named controllerchange..."
  nsCOMPtr<nsIRunnable> controllerChangeRunnable =
    NS_NewRunnableMethodWithArg<ServiceWorkerRegistrationInfo*>(swm,
                                                                &ServiceWorkerManager::FireControllerChange,
                                                                this);
  NS_DispatchToMainThread(controllerChangeRunnable);

  MOZ_ASSERT(mActiveWorker);
  nsRefPtr<ServiceWorker> serviceWorker;
  nsresult rv =
    swm->CreateServiceWorker(mPrincipal,
                             mActiveWorker,
                             getter_AddRefs(serviceWorker));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableMethodWithArg<bool>(this,
                                        &ServiceWorkerRegistrationInfo::FinishActivate,
                                        false /* success */);
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(r)));
    return;
  }

  nsMainThreadPtrHandle<ContinueLifecycleTask> handle(
    new nsMainThreadPtrHolder<ContinueLifecycleTask>(new ContinueActivateTask(this)));

  nsMainThreadPtrHandle<ServiceWorker> serviceWorkerHandle(
    new nsMainThreadPtrHolder<ServiceWorker>(serviceWorker));
  nsRefPtr<LifecycleEventWorkerRunnable> r =
    new LifecycleEventWorkerRunnable(serviceWorkerHandle, NS_LITERAL_STRING("activate"), handle);

  AutoJSAPI jsapi;
  jsapi.Init();
  r->Dispatch(jsapi.cx());
}

/*
 * Implements the async aspects of the getRegistrations algorithm.
 */
class GetRegistrationsRunnable : public nsRunnable
{
  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsRefPtr<Promise> mPromise;
public:
  GetRegistrationsRunnable(nsPIDOMWindow* aWindow, Promise* aPromise)
    : mWindow(aWindow), mPromise(aPromise)
  { }

  NS_IMETHODIMP
  Run()
  {
    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();

    nsIDocument* doc = mWindow->GetExtantDoc();
    if (!doc) {
      mPromise->MaybeReject(NS_ERROR_UNEXPECTED);
      return NS_OK;
    }

    nsCOMPtr<nsIURI> docURI = doc->GetDocumentURI();
    if (!docURI) {
      mPromise->MaybeReject(NS_ERROR_UNEXPECTED);
      return NS_OK;
    }

    nsCOMPtr<nsIPrincipal> principal = doc->NodePrincipal();
    if (!principal) {
      mPromise->MaybeReject(NS_ERROR_UNEXPECTED);
      return NS_OK;
    }

    nsTArray<nsRefPtr<ServiceWorkerRegistrationMainThread>> array;

    bool isNullPrincipal = true;
    nsresult rv = principal->GetIsNullPrincipal(&isNullPrincipal);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (nsContentUtils::IsSystemPrincipal(principal) || isNullPrincipal) {
      mPromise->MaybeResolve(array);
      return NS_OK;
    }

    for (uint32_t i = 0; i < swm->mOrderedScopes.Length(); ++i) {
      NS_ConvertUTF8toUTF16 scope(swm->mOrderedScopes[i]);

      nsCOMPtr<nsIURI> scopeURI;
      nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), scope, nullptr, nullptr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        mPromise->MaybeReject(rv);
        break;
      }

      rv = principal->CheckMayLoad(scopeURI, true /* report */,
                                   false /* allowIfInheritsPrincipal */);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }

      nsRefPtr<ServiceWorkerRegistrationMainThread> swr =
        new ServiceWorkerRegistrationMainThread(mWindow, scope);

      array.AppendElement(swr);
    }

    mPromise->MaybeResolve(array);
    return NS_OK;
  }
};

// If we return an error code here, the ServiceWorkerContainer will
// automatically reject the Promise.
NS_IMETHODIMP
ServiceWorkerManager::GetRegistrations(nsIDOMWindow* aWindow,
                                       nsISupports** aPromise)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWindow);

  // XXXnsm Don't allow chrome callers for now, we don't support chrome
  // ServiceWorkers.
  MOZ_ASSERT(!nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);
  if (!window) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIGlobalObject> sgo = do_QueryInterface(window);
  ErrorResult result;
  nsRefPtr<Promise> promise = Promise::Create(sgo, result);
  if (result.Failed()) {
    return result.ErrorCode();
  }

  nsCOMPtr<nsIRunnable> runnable =
    new GetRegistrationsRunnable(window, promise);
  promise.forget(aPromise);
  return NS_DispatchToCurrentThread(runnable);
}

/*
 * Implements the async aspects of the getRegistration algorithm.
 */
class GetRegistrationRunnable : public nsRunnable
{
  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsRefPtr<Promise> mPromise;
  nsString mDocumentURL;

public:
  GetRegistrationRunnable(nsPIDOMWindow* aWindow, Promise* aPromise,
                          const nsAString& aDocumentURL)
    : mWindow(aWindow), mPromise(aPromise), mDocumentURL(aDocumentURL)
  { }

  NS_IMETHODIMP
  Run()
  {
    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();

    nsIDocument* doc = mWindow->GetExtantDoc();
    if (!doc) {
      mPromise->MaybeReject(NS_ERROR_UNEXPECTED);
      return NS_OK;
    }

    nsCOMPtr<nsIURI> docURI = doc->GetDocumentURI();
    if (!docURI) {
      mPromise->MaybeReject(NS_ERROR_UNEXPECTED);
      return NS_OK;
    }

    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), mDocumentURL, nullptr, docURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPromise->MaybeReject(rv);
      return NS_OK;
    }

    nsCOMPtr<nsIPrincipal> principal = doc->NodePrincipal();
    if (!principal) {
      mPromise->MaybeReject(NS_ERROR_UNEXPECTED);
      return NS_OK;
    }

    rv = principal->CheckMayLoad(uri, true /* report */,
                                 false /* allowIfInheritsPrinciple */);
    if (NS_FAILED(rv)) {
      mPromise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
      return NS_OK;
    }

    nsRefPtr<ServiceWorkerRegistrationInfo> registration =
      swm->GetServiceWorkerRegistrationInfo(uri);

    if (!registration) {
      mPromise->MaybeResolve(JS::UndefinedHandleValue);
      return NS_OK;
    }

    NS_ConvertUTF8toUTF16 scope(registration->mScope);
    nsRefPtr<ServiceWorkerRegistrationMainThread> swr =
      new ServiceWorkerRegistrationMainThread(mWindow, scope);
    mPromise->MaybeResolve(swr);

    return NS_OK;
  }
};

// If we return an error code here, the ServiceWorkerContainer will
// automatically reject the Promise.
NS_IMETHODIMP
ServiceWorkerManager::GetRegistration(nsIDOMWindow* aWindow,
                                      const nsAString& aDocumentURL,
                                      nsISupports** aPromise)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWindow);

  // XXXnsm Don't allow chrome callers for now, we don't support chrome
  // ServiceWorkers.
  MOZ_ASSERT(!nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);
  if (!window) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIGlobalObject> sgo = do_QueryInterface(window);
  ErrorResult result;
  nsRefPtr<Promise> promise = Promise::Create(sgo, result);
  if (result.Failed()) {
    return result.ErrorCode();
  }

  nsCOMPtr<nsIRunnable> runnable =
    new GetRegistrationRunnable(window, promise, aDocumentURL);
  promise.forget(aPromise);
  return NS_DispatchToCurrentThread(runnable);
}

class GetReadyPromiseRunnable : public nsRunnable
{
  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsRefPtr<Promise> mPromise;

public:
  GetReadyPromiseRunnable(nsPIDOMWindow* aWindow, Promise* aPromise)
    : mWindow(aWindow), mPromise(aPromise)
  { }

  NS_IMETHODIMP
  Run()
  {
    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();

    nsIDocument* doc = mWindow->GetExtantDoc();
    if (!doc) {
      mPromise->MaybeReject(NS_ERROR_UNEXPECTED);
      return NS_OK;
    }

    nsCOMPtr<nsIURI> docURI = doc->GetDocumentURI();
    if (!docURI) {
      mPromise->MaybeReject(NS_ERROR_UNEXPECTED);
      return NS_OK;
    }

    if (!swm->CheckReadyPromise(mWindow, docURI, mPromise)) {
      swm->StorePendingReadyPromise(mWindow, docURI, mPromise);
    }

    return NS_OK;
  }
};

#ifndef MOZ_SIMPLEPUSH

class SendPushEventRunnable final : public WorkerRunnable
{
  nsString mData;
  nsMainThreadPtrHandle<ServiceWorker> mServiceWorker;

public:
  SendPushEventRunnable(
    WorkerPrivate* aWorkerPrivate,
    const nsAString& aData,
    nsMainThreadPtrHandle<ServiceWorker>& aServiceWorker)
      : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount)
      , mData(aData)
      , mServiceWorker(aServiceWorker)
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
    pei.mData.Construct(mData);
    pei.mBubbles = false;
    pei.mCancelable = true;

    ErrorResult result;
    nsRefPtr<PushEvent> event =
      PushEvent::Constructor(globalObj, NS_LITERAL_STRING("push"), pei, result);
    if (NS_WARN_IF(result.Failed())) {
      return false;
    }

    event->SetTrusted(true);
    event->PostInit(mServiceWorker);

    nsRefPtr<EventTarget> target = do_QueryObject(aWorkerPrivate->GlobalScope());
    target->DispatchDOMEvent(nullptr, event, nullptr, nullptr);
    return true;
  }
};

class SendPushSubscriptionChangeEventRunnable final : public WorkerRunnable
{
  nsMainThreadPtrHandle<ServiceWorker> mServiceWorker;

public:
  SendPushSubscriptionChangeEventRunnable(
    WorkerPrivate* aWorkerPrivate,
    nsMainThreadPtrHandle<ServiceWorker>& aServiceWorker)
      : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount)
      , mServiceWorker(aServiceWorker)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);

    nsRefPtr<EventTarget> target = do_QueryObject(aWorkerPrivate->GlobalScope());

    nsContentUtils::DispatchTrustedEvent(nullptr, target,
                                         NS_LITERAL_STRING("pushsubscriptionchange"),
                                         true, true);
    return true;
  }
};

#endif /* ! MOZ_SIMPLEPUSH */

NS_IMETHODIMP
ServiceWorkerManager::SendPushEvent(const nsACString& aScope, const nsAString& aData)
{
#ifdef MOZ_SIMPLEPUSH
  return NS_ERROR_NOT_AVAILABLE;
#else
  nsRefPtr<ServiceWorker> serviceWorker = CreateServiceWorkerForScope(aScope);
  if (!serviceWorker) {
    return NS_ERROR_FAILURE;
  }

  nsMainThreadPtrHandle<ServiceWorker> serviceWorkerHandle(
    new nsMainThreadPtrHolder<ServiceWorker>(serviceWorker));

  nsRefPtr<SendPushEventRunnable> r =
    new SendPushEventRunnable(serviceWorker->GetWorkerPrivate(), aData, serviceWorkerHandle);

  AutoJSAPI jsapi;
  jsapi.Init();
  if (NS_WARN_IF(!r->Dispatch(jsapi.cx()))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
#endif
}

NS_IMETHODIMP
ServiceWorkerManager::SendPushSubscriptionChangedEvent(const nsACString& aScope)
{
#ifdef MOZ_SIMPLEPUSH
  return NS_ERROR_NOT_AVAILABLE;
#else
  nsRefPtr<ServiceWorker> serviceWorker = CreateServiceWorkerForScope(aScope);
  if (!serviceWorker) {
    return NS_ERROR_FAILURE;
  }
  nsMainThreadPtrHandle<ServiceWorker> serviceWorkerHandle(
    new nsMainThreadPtrHolder<ServiceWorker>(serviceWorker));

  nsRefPtr<SendPushSubscriptionChangeEventRunnable> r =
    new SendPushSubscriptionChangeEventRunnable(serviceWorker->GetWorkerPrivate(), serviceWorkerHandle);

  AutoJSAPI jsapi;
  jsapi.Init();
  if (NS_WARN_IF(!r->Dispatch(jsapi.cx()))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
#endif
}

NS_IMETHODIMP
ServiceWorkerManager::GetReadyPromise(nsIDOMWindow* aWindow,
                                      nsISupports** aPromise)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWindow);

  // XXXnsm Don't allow chrome callers for now, we don't support chrome
  // ServiceWorkers.
  MOZ_ASSERT(!nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);
  if (!window) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(!mPendingReadyPromises.Contains(window));

  nsCOMPtr<nsIGlobalObject> sgo = do_QueryInterface(window);
  ErrorResult result;
  nsRefPtr<Promise> promise = Promise::Create(sgo, result);
  if (result.Failed()) {
    return result.ErrorCode();
  }

  nsCOMPtr<nsIRunnable> runnable =
    new GetReadyPromiseRunnable(window, promise);
  promise.forget(aPromise);
  return NS_DispatchToCurrentThread(runnable);
}

NS_IMETHODIMP
ServiceWorkerManager::RemoveReadyPromise(nsIDOMWindow* aWindow)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);
  if (!window) {
    return NS_ERROR_FAILURE;
  }

  mPendingReadyPromises.Remove(aWindow);
  return NS_OK;
}

void
ServiceWorkerManager::StorePendingReadyPromise(nsPIDOMWindow* aWindow,
                                               nsIURI* aURI,
                                               Promise* aPromise)
{
  PendingReadyPromise* data;

  // We should not have 2 pending promises for the same window.
  MOZ_ASSERT(!mPendingReadyPromises.Get(aWindow, &data));

  data = new PendingReadyPromise(aURI, aPromise);
  mPendingReadyPromises.Put(aWindow, data);
}

void
ServiceWorkerManager::CheckPendingReadyPromises()
{
  mPendingReadyPromises.Enumerate(CheckPendingReadyPromisesEnumerator, this);
}

PLDHashOperator
ServiceWorkerManager::CheckPendingReadyPromisesEnumerator(
                                          nsISupports* aSupports,
                                          nsAutoPtr<PendingReadyPromise>& aData,
                                          void* aPtr)
{
  ServiceWorkerManager* aSwm = static_cast<ServiceWorkerManager*>(aPtr);

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aSupports);

  if (aSwm->CheckReadyPromise(window, aData->mURI, aData->mPromise)) {
    return PL_DHASH_REMOVE;
  }

  return PL_DHASH_NEXT;
}

bool
ServiceWorkerManager::CheckReadyPromise(nsPIDOMWindow* aWindow,
                                        nsIURI* aURI, Promise* aPromise)
{
  nsRefPtr<ServiceWorkerRegistrationInfo> registration =
    GetServiceWorkerRegistrationInfo(aURI);

  if (registration && registration->mActiveWorker) {
    NS_ConvertUTF8toUTF16 scope(registration->mScope);
    nsRefPtr<ServiceWorkerRegistrationMainThread> swr =
      new ServiceWorkerRegistrationMainThread(aWindow, scope);
    aPromise->MaybeResolve(swr);
    return true;
  }

  return false;
}

already_AddRefed<ServiceWorker>
ServiceWorkerManager::CreateServiceWorkerForScope(const nsACString& aScope)
{
  AssertIsOnMainThread();

  nsCOMPtr<nsIURI> scopeURI;
  nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), aScope, nullptr, nullptr);
  if (NS_FAILED(rv)) {
    return nullptr;
  }
  nsRefPtr<ServiceWorkerRegistrationInfo> registration = GetServiceWorkerRegistrationInfo(scopeURI);
  if (!registration) {
    return nullptr;
  }

  if (!registration->mActiveWorker) {
    return nullptr;
  }

  nsRefPtr<ServiceWorker> sw;
  rv = CreateServiceWorker(registration->mPrincipal,
                           registration->mActiveWorker,
                           getter_AddRefs(sw));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return sw.forget();
}

class ServiceWorkerUnregisterJob final : public ServiceWorkerJob
{
  nsRefPtr<ServiceWorkerRegistrationInfo> mRegistration;
  const nsCString mScope;
  nsCOMPtr<nsIServiceWorkerUnregisterCallback> mCallback;
  PrincipalInfo mPrincipalInfo;

  ~ServiceWorkerUnregisterJob()
  { }

public:
  ServiceWorkerUnregisterJob(ServiceWorkerJobQueue* aQueue,
                             const nsACString& aScope,
                             nsIServiceWorkerUnregisterCallback* aCallback,
                             PrincipalInfo& aPrincipalInfo)
    : ServiceWorkerJob(aQueue)
    , mScope(aScope)
    , mCallback(aCallback)
    , mPrincipalInfo(aPrincipalInfo)
  {
    AssertIsOnMainThread();
  }

  void
  Start() override
  {
    AssertIsOnMainThread();
    nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableMethod(this, &ServiceWorkerUnregisterJob::UnregisterAndDone);
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(r)));
  }

private:
  // You probably want UnregisterAndDone().
  nsresult
  Unregister()
  {
    AssertIsOnMainThread();

    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();

    // "Let registration be the result of running [[Get Registration]]
    // algorithm passing scope as the argument."
    nsRefPtr<ServiceWorkerRegistrationInfo> registration;
    if (!swm->mServiceWorkerRegistrationInfos.Get(mScope, getter_AddRefs(registration))) {
      // "If registration is null, then, resolve promise with false."
      return mCallback ? mCallback->UnregisterSucceeded(false) : NS_OK;
    }

    MOZ_ASSERT(registration);

    // "Set registration's uninstalling flag."
    registration->mPendingUninstall = true;
    // "Resolve promise with true"
    nsresult rv = mCallback ? mCallback->UnregisterSucceeded(true) : NS_OK;
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // "If no service worker client is using registration..."
    if (!registration->IsControllingDocuments()) {
      // "If registration's uninstalling flag is set.."
      if (!registration->mPendingUninstall) {
        return NS_OK;
      }

      // "Invoke [[Clear Registration]]..."
      registration->Clear();
      swm->RemoveRegistration(registration);
    }

    MOZ_ASSERT(swm->mActor);
    swm->mActor->SendUnregisterServiceWorker(mPrincipalInfo,
                                             NS_ConvertUTF8toUTF16(mScope));

    return NS_OK;
  }

  // The unregister job is done irrespective of success or failure of any sort.
  void
  UnregisterAndDone()
  {
    Done(Unregister());
  }
};

NS_IMETHODIMP
ServiceWorkerManager::Unregister(nsIPrincipal* aPrincipal,
                                 nsIServiceWorkerUnregisterCallback* aCallback,
                                 const nsAString& aScope)
{
  AssertIsOnMainThread();

  if (!aPrincipal) {
    return NS_ERROR_FAILURE;
  }

// This is not accessible by content, and callers should always ensure scope is
// a correct URI, so this is wrapped in DEBUG
#ifdef DEBUG
  nsCOMPtr<nsIURI> scopeURI;
  nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), aScope, nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }
#endif

  NS_ConvertUTF16toUTF8 scope(aScope);
  ServiceWorkerJobQueue* queue = GetOrCreateJobQueue(scope);
  MOZ_ASSERT(queue);

  PrincipalInfo principalInfo;
  if (NS_WARN_IF(NS_FAILED(PrincipalToPrincipalInfo(aPrincipal,
                                                    &principalInfo)))) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsRefPtr<ServiceWorkerUnregisterJob> job =
    new ServiceWorkerUnregisterJob(queue, scope, aCallback, principalInfo);

  if (mActor) {
    queue->Append(job);
    return NS_OK;
  }

  AppendPendingOperation(queue, job);
  return NS_OK;
}

/* static */
already_AddRefed<ServiceWorkerManager>
ServiceWorkerManager::GetInstance()
{
  nsCOMPtr<nsIServiceWorkerManager> swm = mozilla::services::GetServiceWorkerManager();
  nsRefPtr<ServiceWorkerManager> concrete = do_QueryObject(swm);
  return concrete.forget();
}

void
ServiceWorkerManager::FinishFetch(ServiceWorkerRegistrationInfo* aRegistration)
{
}

bool
ServiceWorkerManager::HandleError(JSContext* aCx,
                                  const nsCString& aScope,
                                  const nsString& aWorkerURL,
                                  nsString aMessage,
                                  nsString aFilename,
                                  nsString aLine,
                                  uint32_t aLineNumber,
                                  uint32_t aColumnNumber,
                                  uint32_t aFlags)
{
  AssertIsOnMainThread();

  if (!mSetOfScopesBeingUpdated.Contains(aScope)) {
    return false;
  }

  mSetOfScopesBeingUpdated.Remove(aScope);

  ServiceWorkerJobQueue* queue = mJobQueues.Get(aScope);
  MOZ_ASSERT(queue);
  ServiceWorkerJob* job = queue->Peek();
  ServiceWorkerRegisterJob* regJob = static_cast<ServiceWorkerRegisterJob*>(job);
  MOZ_ASSERT(regJob);

  RootedDictionary<ErrorEventInit> init(aCx);
  init.mMessage = aMessage;
  init.mFilename = aFilename;
  init.mLineno = aLineNumber;
  init.mColno = aColumnNumber;

  regJob->Fail(init);
  return true;
}

void
ServiceWorkerRegistrationInfo::FinishActivate(bool aSuccess)
{
  if (mPendingUninstall || !mActiveWorker) {
    return;
  }

  if (aSuccess) {
    mActiveWorker->UpdateState(ServiceWorkerState::Activated);
    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    swm->StoreRegistration(mPrincipal, this);
  } else {
    mActiveWorker->UpdateState(ServiceWorkerState::Redundant);
    mActiveWorker = nullptr;
  }
}

NS_IMETHODIMP
ServiceWorkerManager::CreateServiceWorkerForWindow(nsPIDOMWindow* aWindow,
                                                   ServiceWorkerInfo* aInfo,
                                                   ServiceWorker** aServiceWorker)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aInfo);

  AutoJSAPI jsapi;
  jsapi.Init(aWindow);
  JSContext* cx = jsapi.cx();

  WorkerLoadInfo loadInfo;
  nsresult rv = WorkerPrivate::GetLoadInfo(cx, aWindow, nullptr,
                                           NS_ConvertUTF8toUTF16(aInfo->ScriptSpec()),
                                           false,
                                           WorkerPrivate::OverrideLoadGroup,
                                           &loadInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(!aInfo->CacheName().IsEmpty());
  loadInfo.mServiceWorkerCacheName = aInfo->CacheName();

  RuntimeService* rs = RuntimeService::GetOrCreateService();
  if (!rs) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<SharedWorker> sharedWorker;
  rv = rs->CreateSharedWorkerForServiceWorkerFromLoadInfo(cx, &loadInfo,
                                                          NS_ConvertUTF8toUTF16(aInfo->ScriptSpec()),
                                                          aInfo->Scope(),
                                                          getter_AddRefs(sharedWorker));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsRefPtr<ServiceWorker> serviceWorker =
    new ServiceWorker(aWindow, aInfo, sharedWorker);

  serviceWorker.forget(aServiceWorker);
  return rv;
}

void
ServiceWorkerManager::LoadRegistrations(
                  const nsTArray<ServiceWorkerRegistrationData>& aRegistrations)
{
  AssertIsOnMainThread();

  for (uint32_t i = 0, len = aRegistrations.Length(); i < len; ++i) {
    nsCOMPtr<nsIPrincipal> principal =
      PrincipalInfoToPrincipal(aRegistrations[i].principal());
    if (!principal) {
      continue;
    }

    ServiceWorkerRegistrationInfo* registration =
      CreateNewRegistration(aRegistrations[i].scope(), principal);

    registration->mScriptSpec = aRegistrations[i].scriptSpec();

    const nsCString& currentWorkerURL = aRegistrations[i].currentWorkerURL();
    if (!currentWorkerURL.IsEmpty()) {
      registration->mActiveWorker =
        new ServiceWorkerInfo(registration, currentWorkerURL, aRegistrations[i].activeCacheName());
      registration->mActiveWorker->SetActivateStateUncheckedWithoutEvent(ServiceWorkerState::Activated);
    }
  }
}

void
ServiceWorkerManager::ActorFailed()
{
  MOZ_CRASH("Failed to create a PBackgroundChild actor!");
}

void
ServiceWorkerManager::ActorCreated(mozilla::ipc::PBackgroundChild* aActor)
{
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!mActor);
  mActor = aActor;

  // Flush the pending requests.
  for (uint32_t i = 0, len = mPendingOperations.Length(); i < len; ++i) {
    MOZ_ASSERT(mPendingOperations[i].mRunnable ||
               (mPendingOperations[i].mJob && mPendingOperations[i].mQueue));

    if (mPendingOperations[i].mRunnable) {
      nsresult rv = NS_DispatchToCurrentThread(mPendingOperations[i].mRunnable);
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to dispatch a runnable.");
        return;
      }
    } else {
      mPendingOperations[i].mQueue->Append(mPendingOperations[i].mJob);
    }
  }

  mPendingOperations.Clear();
}

void
ServiceWorkerManager::StoreRegistration(
                                   nsIPrincipal* aPrincipal,
                                   ServiceWorkerRegistrationInfo* aRegistration)
{
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aRegistration);

  ServiceWorkerRegistrationData data;
  nsresult rv = PopulateRegistrationData(aPrincipal, aRegistration, data);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  PrincipalInfo principalInfo;
  if (NS_WARN_IF(NS_FAILED(PrincipalToPrincipalInfo(aPrincipal,
                                                    &principalInfo)))) {
    return;
  }

  mActor->SendRegisterServiceWorker(data);
}

already_AddRefed<ServiceWorkerRegistrationInfo>
ServiceWorkerManager::GetServiceWorkerRegistrationInfo(nsPIDOMWindow* aWindow)
{
  nsCOMPtr<nsIDocument> document = aWindow->GetExtantDoc();
  return GetServiceWorkerRegistrationInfo(document);
}

already_AddRefed<ServiceWorkerRegistrationInfo>
ServiceWorkerManager::GetServiceWorkerRegistrationInfo(nsIDocument* aDoc)
{
  nsCOMPtr<nsIURI> documentURI = aDoc->GetDocumentURI();
  return GetServiceWorkerRegistrationInfo(documentURI);
}

already_AddRefed<ServiceWorkerRegistrationInfo>
ServiceWorkerManager::GetServiceWorkerRegistrationInfo(nsIURI* aURI)
{
  nsCString spec;
  nsresult rv = aURI->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  nsCString scope = FindScopeForPath(mOrderedScopes, spec);
  if (scope.IsEmpty()) {
    return nullptr;
  }

  nsRefPtr<ServiceWorkerRegistrationInfo> registration;
  mServiceWorkerRegistrationInfos.Get(scope, getter_AddRefs(registration));
  // ordered scopes and registrations better be in sync.
  MOZ_ASSERT(registration);

  if (registration->mPendingUninstall) {
    return nullptr;
  }
  return registration.forget();
}

/* static */ void
ServiceWorkerManager::AddScope(nsTArray<nsCString>& aList, const nsACString& aScope)
{
  for (uint32_t i = 0; i < aList.Length(); ++i) {
    const nsCString& current = aList[i];

    // Perfect match!
    if (aScope.Equals(current)) {
      return;
    }

    // Sort by length, with longest match first.
    // /foo/bar should be before /foo/
    // Similarly /foo/b is between the two.
    if (StringBeginsWith(aScope, current)) {
      aList.InsertElementAt(i, aScope);
      return;
    }
  }

  aList.AppendElement(aScope);
}

/* static */ nsCString
ServiceWorkerManager::FindScopeForPath(nsTArray<nsCString>& aList, const nsACString& aPath)
{
  nsCString match;

  for (uint32_t i = 0; i < aList.Length(); ++i) {
    const nsCString& current = aList[i];
    if (StringBeginsWith(aPath, current)) {
      match = current;
      break;
    }
  }

  return match;
}

/* static */ void
ServiceWorkerManager::RemoveScope(nsTArray<nsCString>& aList, const nsACString& aScope)
{
  aList.RemoveElement(aScope);
}

void
ServiceWorkerManager::MaybeStartControlling(nsIDocument* aDoc)
{
  AssertIsOnMainThread();
  nsRefPtr<ServiceWorkerRegistrationInfo> registration =
    GetServiceWorkerRegistrationInfo(aDoc);
  if (registration) {
    MOZ_ASSERT(!mControlledDocuments.Contains(aDoc));
    registration->StartControllingADocument();
    // Use the already_AddRefed<> form of Put to avoid the addref-deref since
    // we don't need the registration pointer in this function anymore.
    mControlledDocuments.Put(aDoc, registration.forget());
  }
}

void
ServiceWorkerManager::MaybeStopControlling(nsIDocument* aDoc)
{
  MOZ_ASSERT(aDoc);
  nsRefPtr<ServiceWorkerRegistrationInfo> registration;
  mControlledDocuments.Remove(aDoc, getter_AddRefs(registration));
  // A document which was uncontrolled does not maintain that state itself, so
  // it will always call MaybeStopControlling() even if there isn't an
  // associated registration. So this check is required.
  if (registration) {
    registration->StopControllingADocument();
    if (!registration->IsControllingDocuments()) {
      if (registration->mPendingUninstall) {
        registration->Clear();
        RemoveRegistration(registration);
      } else {
        registration->TryToActivate();
      }
    }
  }
}

NS_IMETHODIMP
ServiceWorkerManager::GetScopeForUrl(const nsAString& aUrl, nsAString& aScope)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aUrl, nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<ServiceWorkerRegistrationInfo> r = GetServiceWorkerRegistrationInfo(uri);
  if (!r) {
      return NS_ERROR_FAILURE;
  }

  aScope = NS_ConvertUTF8toUTF16(r->mScope);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerManager::AddRegistrationEventListener(const nsAString& aScope, nsIDOMEventTarget* aListener)
{
  AssertIsOnMainThread();
  nsAutoCString scope = NS_ConvertUTF16toUTF8(aScope);

  // TODO: this is very very bad:
  ServiceWorkerRegistrationBase* registration = static_cast<ServiceWorkerRegistrationBase*>(aListener);
  MOZ_ASSERT(!mServiceWorkerRegistrations.Contains(registration));
#ifdef DEBUG
  // Ensure a registration is only listening for it's own scope.
  nsAutoString regScope;
  registration->GetScope(regScope);
  MOZ_ASSERT(!regScope.IsEmpty());
  MOZ_ASSERT(scope.Equals(NS_ConvertUTF16toUTF8(regScope)));
#endif
  mServiceWorkerRegistrations.AppendElement(registration);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerManager::RemoveRegistrationEventListener(const nsAString& aScope, nsIDOMEventTarget* aListener)
{
  AssertIsOnMainThread();
  nsCString scope = NS_ConvertUTF16toUTF8(aScope);
  ServiceWorkerRegistrationBase* registration = static_cast<ServiceWorkerRegistrationBase*>(aListener);
  MOZ_ASSERT(mServiceWorkerRegistrations.Contains(registration));
#ifdef DEBUG
  // Ensure a registration is unregistering for it's own scope.
  nsAutoString regScope;
  registration->GetScope(regScope);
  MOZ_ASSERT(!regScope.IsEmpty());
  MOZ_ASSERT(scope.Equals(NS_ConvertUTF16toUTF8(regScope)));
#endif
  mServiceWorkerRegistrations.RemoveElement(registration);
  return NS_OK;
}

void
ServiceWorkerManager::FireEventOnServiceWorkerRegistrations(
  ServiceWorkerRegistrationInfo* aRegistration,
  const nsAString& aName)
{
  AssertIsOnMainThread();

  nsTObserverArray<ServiceWorkerRegistrationBase*>::ForwardIterator it(mServiceWorkerRegistrations);
  while (it.HasMore()) {
    nsRefPtr<ServiceWorkerRegistrationBase> target = it.GetNext();
    nsAutoString regScope;
    target->GetScope(regScope);
    MOZ_ASSERT(!regScope.IsEmpty());

    NS_ConvertUTF16toUTF8 utf8Scope(regScope);
    if (utf8Scope.Equals(aRegistration->mScope)) {
      nsresult rv = target->DispatchTrustedEvent(aName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        // Warn only.
      }
    }
  }
}

/*
 * This is used for installing, waiting and active.
 */
NS_IMETHODIMP
ServiceWorkerManager::GetServiceWorkerForScope(nsIDOMWindow* aWindow,
                                               const nsAString& aScope,
                                               WhichServiceWorker aWhichWorker,
                                               nsISupports** aServiceWorker)
{
  AssertIsOnMainThread();

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);
  if (NS_WARN_IF(!window)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
  MOZ_ASSERT(doc);

  ///////////////////////////////////////////
  // Security check
  nsAutoCString scope = NS_ConvertUTF16toUTF8(aScope);
  nsCOMPtr<nsIURI> scopeURI;
  // We pass nullptr as the base URI since scopes obtained from
  // ServiceWorkerRegistrations MUST be fully qualified URIs.
  nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), scope, nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsIPrincipal> documentPrincipal = doc->NodePrincipal();
  rv = documentPrincipal->CheckMayLoad(scopeURI, true /* report */,
                                       false /* allowIfInheritsPrinciple */);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }
  ////////////////////////////////////////////

  nsRefPtr<ServiceWorkerRegistrationInfo> registration = GetRegistration(scope);
  if (NS_WARN_IF(!registration)) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<ServiceWorkerInfo> info;
  if (aWhichWorker == WhichServiceWorker::INSTALLING_WORKER) {
    info = registration->mInstallingWorker;
  } else if (aWhichWorker == WhichServiceWorker::WAITING_WORKER) {
    info = registration->mWaitingWorker;
  } else if (aWhichWorker == WhichServiceWorker::ACTIVE_WORKER) {
    info = registration->mActiveWorker;
  } else {
    MOZ_CRASH("Invalid worker type");
  }

  if (NS_WARN_IF(!info)) {
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  nsRefPtr<ServiceWorker> serviceWorker;
  rv = CreateServiceWorkerForWindow(window,
                                    info,
                                    getter_AddRefs(serviceWorker));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  serviceWorker->SetState(info->State());
  serviceWorker.forget(aServiceWorker);
  return NS_OK;
}

class FetchEventRunnable : public WorkerRunnable
                         , public nsIHttpHeaderVisitor {
  nsMainThreadPtrHandle<nsIInterceptedChannel> mInterceptedChannel;
  nsMainThreadPtrHandle<ServiceWorker> mServiceWorker;
  nsTArray<nsCString> mHeaderNames;
  nsTArray<nsCString> mHeaderValues;
  nsAutoPtr<ServiceWorkerClientInfo> mClientInfo;
  nsCString mSpec;
  nsCString mMethod;
  bool mIsReload;
  RequestMode mRequestMode;
  RequestCredentials mRequestCredentials;
  nsContentPolicyType mContentPolicyType;
public:
  FetchEventRunnable(WorkerPrivate* aWorkerPrivate,
                     nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                     nsMainThreadPtrHandle<ServiceWorker>& aServiceWorker,
                     nsAutoPtr<ServiceWorkerClientInfo>& aClientInfo,
                     bool aIsReload)
    : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount)
    , mInterceptedChannel(aChannel)
    , mServiceWorker(aServiceWorker)
    , mClientInfo(aClientInfo)
    , mIsReload(aIsReload)
    , mRequestMode(RequestMode::No_cors)
    // By default we set it to same-origin since normal HTTP fetches always
    // send credentials to same-origin websites unless explicitly forbidden.
    , mRequestCredentials(RequestCredentials::Same_origin)
    , mContentPolicyType(nsIContentPolicy::TYPE_INVALID)
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

    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);
    NS_ENSURE_TRUE(httpChannel, NS_ERROR_NOT_AVAILABLE);

    rv = httpChannel->GetRequestMethod(mMethod);
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t loadFlags;
    rv = channel->GetLoadFlags(&loadFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIHttpChannelInternal> internalChannel = do_QueryInterface(httpChannel);
    NS_ENSURE_TRUE(internalChannel, NS_ERROR_NOT_AVAILABLE);

    uint32_t mode;
    internalChannel->GetCorsMode(&mode);
    switch (mode) {
      case nsIHttpChannelInternal::CORS_MODE_SAME_ORIGIN:
        mRequestMode = RequestMode::Same_origin;
        break;
      case nsIHttpChannelInternal::CORS_MODE_NO_CORS:
        mRequestMode = RequestMode::No_cors;
        break;
      case nsIHttpChannelInternal::CORS_MODE_CORS:
      case nsIHttpChannelInternal::CORS_MODE_CORS_WITH_FORCED_PREFLIGHT:
        mRequestMode = RequestMode::Cors;
        break;
      default:
        MOZ_CRASH("Unexpected CORS mode");
    }

    if (loadFlags & nsIRequest::LOAD_ANONYMOUS) {
      mRequestCredentials = RequestCredentials::Omit;
    } else {
      bool includeCrossOrigin;
      internalChannel->GetCorsIncludeCredentials(&includeCrossOrigin);
      if (includeCrossOrigin) {
        mRequestCredentials = RequestCredentials::Include;
      }
    }

    rv = httpChannel->VisitRequestHeaders(this);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILoadInfo> loadInfo;
    rv = channel->GetLoadInfo(getter_AddRefs(loadInfo));
    NS_ENSURE_SUCCESS(rv, rv);

    mContentPolicyType = loadInfo->GetContentPolicyType();

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

    nsRefPtr<InternalHeaders> internalHeaders = new InternalHeaders(HeadersGuardEnum::Request);
    MOZ_ASSERT(mHeaderNames.Length() == mHeaderValues.Length());
    for (uint32_t i = 0; i < mHeaderNames.Length(); i++) {
      ErrorResult result;
      internalHeaders->Set(mHeaderNames[i], mHeaderValues[i], result);
      if (NS_WARN_IF(result.Failed())) {
        return false;
      }
    }

    nsRefPtr<Headers> headers = new Headers(globalObj.GetAsSupports(), internalHeaders);
    reqInit.mHeaders.Construct();
    reqInit.mHeaders.Value().SetAsHeaders() = headers;

    //TODO(jdm): set request body

    reqInit.mMode.Construct(mRequestMode);
    reqInit.mCredentials.Construct(mRequestCredentials);

    ErrorResult result;
    nsRefPtr<Request> request = Request::Constructor(globalObj, requestInfo, reqInit, result);
    if (NS_WARN_IF(result.Failed())) {
      return false;
    }
    // For Telemetry, note that this Request object was created by a Fetch event.
    nsRefPtr<InternalRequest> internalReq = request->GetInternalRequest();
    MOZ_ASSERT(internalReq);
    internalReq->SetCreatedByFetchEvent();

    request->SetContentPolicyType(mContentPolicyType);

    RootedDictionary<FetchEventInit> init(aCx);
    init.mRequest.Construct();
    init.mRequest.Value() = request;
    init.mBubbles = false;
    init.mCancelable = true;
    init.mIsReload.Construct(mIsReload);
    nsRefPtr<FetchEvent> event =
      FetchEvent::Constructor(globalObj, NS_LITERAL_STRING("fetch"), init, result);
    if (NS_WARN_IF(result.Failed())) {
      return false;
    }

    event->PostInit(mInterceptedChannel, mServiceWorker, mClientInfo);
    event->SetTrusted(true);

    nsRefPtr<EventTarget> target = do_QueryObject(aWorkerPrivate->GlobalScope());
    nsresult rv2 = target->DispatchDOMEvent(nullptr, event, nullptr, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv2)) || !event->WaitToRespond()) {
      nsCOMPtr<nsIRunnable> runnable = new ResumeRequest(mInterceptedChannel);
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(runnable)));
    }
    return true;
  }
};

NS_IMPL_ISUPPORTS_INHERITED(FetchEventRunnable, WorkerRunnable, nsIHttpHeaderVisitor)

NS_IMETHODIMP
ServiceWorkerManager::DispatchFetchEvent(nsIDocument* aDoc, nsIInterceptedChannel* aChannel,
                                         bool aIsReload)
{
  MOZ_ASSERT(aChannel);
  nsCOMPtr<nsISupports> serviceWorker;

  bool isNavigation = false;
  nsresult rv = aChannel->GetIsNavigation(&isNavigation);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoPtr<ServiceWorkerClientInfo> clientInfo;

  if (!isNavigation) {
    MOZ_ASSERT(aDoc);
    rv = GetDocumentController(aDoc->GetInnerWindow(), getter_AddRefs(serviceWorker));
    clientInfo = new ServiceWorkerClientInfo(aDoc);
  } else {
    nsCOMPtr<nsIChannel> internalChannel;
    rv = aChannel->GetChannel(getter_AddRefs(internalChannel));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> uri;
    rv = internalChannel->GetURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    nsRefPtr<ServiceWorkerRegistrationInfo> registration =
      GetServiceWorkerRegistrationInfo(uri);
    if (!registration) {
      NS_WARNING("No registration found when dispatching the fetch event");
      return NS_ERROR_FAILURE;
    }
    // This should only happen if IsAvailableForURI() returned true.
    MOZ_ASSERT(registration->mActiveWorker);

    nsRefPtr<ServiceWorker> sw;
    rv = CreateServiceWorker(registration->mPrincipal,
                             registration->mActiveWorker,
                             getter_AddRefs(sw));
    serviceWorker = sw.forget();
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsMainThreadPtrHandle<nsIInterceptedChannel> handle(
    new nsMainThreadPtrHolder<nsIInterceptedChannel>(aChannel, false));

  nsRefPtr<ServiceWorker> sw = static_cast<ServiceWorker*>(serviceWorker.get());
  nsMainThreadPtrHandle<ServiceWorker> serviceWorkerHandle(
    new nsMainThreadPtrHolder<ServiceWorker>(sw));

  // clientInfo is null if we don't have a controlled document
  nsRefPtr<FetchEventRunnable> event =
    new FetchEventRunnable(sw->GetWorkerPrivate(), handle, serviceWorkerHandle, clientInfo, aIsReload);
  rv = event->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  AutoJSAPI api;
  api.Init();
  if (NS_WARN_IF(!event->Dispatch(api.cx()))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerManager::IsAvailableForURI(nsIURI* aURI, bool* aIsAvailable)
{
  MOZ_ASSERT(aURI);
  MOZ_ASSERT(aIsAvailable);
  nsRefPtr<ServiceWorkerRegistrationInfo> registration =
    GetServiceWorkerRegistrationInfo(aURI);
  *aIsAvailable = registration && registration->mActiveWorker;
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerManager::IsControlled(nsIDocument* aDoc, bool* aIsControlled)
{
  MOZ_ASSERT(aDoc);
  MOZ_ASSERT(aIsControlled);
  nsRefPtr<ServiceWorkerRegistrationInfo> registration;
  nsresult rv = GetDocumentRegistration(aDoc, getter_AddRefs(registration));
  if (NS_WARN_IF(NS_FAILED(rv) && rv != NS_ERROR_NOT_AVAILABLE)) {
    // It's OK to ignore the case where we don't have a registration.
    return rv;
  }
  *aIsControlled = !!registration;
  return NS_OK;
}

nsresult
ServiceWorkerManager::GetDocumentRegistration(nsIDocument* aDoc,
                                              ServiceWorkerRegistrationInfo** aRegistrationInfo)
{
  nsRefPtr<ServiceWorkerRegistrationInfo> registration;
  if (!mControlledDocuments.Get(aDoc, getter_AddRefs(registration))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // If the document is controlled, the current worker MUST be non-null.
  if (!registration->mActiveWorker) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  registration.forget(aRegistrationInfo);
  return NS_OK;
}

/*
 * The .controller is for the registration associated with the document when
 * the document was loaded.
 */
NS_IMETHODIMP
ServiceWorkerManager::GetDocumentController(nsIDOMWindow* aWindow, nsISupports** aServiceWorker)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);
  MOZ_ASSERT(window);
  if (!window || !window->GetExtantDoc()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();

  nsRefPtr<ServiceWorkerRegistrationInfo> registration;
  nsresult rv = GetDocumentRegistration(doc, getter_AddRefs(registration));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsRefPtr<ServiceWorker> serviceWorker;
  rv = CreateServiceWorkerForWindow(window,
                                    registration->mActiveWorker,
                                    getter_AddRefs(serviceWorker));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  serviceWorker.forget(aServiceWorker);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerManager::GetInstalling(nsIDOMWindow* aWindow,
                                    const nsAString& aScope,
                                    nsISupports** aServiceWorker)
{
  return GetServiceWorkerForScope(aWindow, aScope,
                                  WhichServiceWorker::INSTALLING_WORKER,
                                  aServiceWorker);
}

NS_IMETHODIMP
ServiceWorkerManager::GetWaiting(nsIDOMWindow* aWindow,
                                 const nsAString& aScope,
                                 nsISupports** aServiceWorker)
{
  return GetServiceWorkerForScope(aWindow, aScope,
                                  WhichServiceWorker::WAITING_WORKER,
                                  aServiceWorker);
}

NS_IMETHODIMP
ServiceWorkerManager::GetActive(nsIDOMWindow* aWindow,
                                const nsAString& aScope,
                                nsISupports** aServiceWorker)
{
  return GetServiceWorkerForScope(aWindow, aScope,
                                  WhichServiceWorker::ACTIVE_WORKER,
                                  aServiceWorker);
}

NS_IMETHODIMP
ServiceWorkerManager::CreateServiceWorker(nsIPrincipal* aPrincipal,
                                          ServiceWorkerInfo* aInfo,
                                          ServiceWorker** aServiceWorker)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);

  WorkerLoadInfo info;
  nsresult rv = NS_NewURI(getter_AddRefs(info.mBaseURI), aInfo->ScriptSpec(),
                          nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  info.mResolvedScriptURI = info.mBaseURI;
  MOZ_ASSERT(!aInfo->CacheName().IsEmpty());
  info.mServiceWorkerCacheName = aInfo->CacheName();

  rv = info.mBaseURI->GetHost(info.mDomain);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  info.mPrincipal = aPrincipal;

  // NOTE: this defaults the SW load context to:
  //  - private browsing = false
  //  - content = true
  //  - use remote tabs = false
  // Alternatively we could persist the original load group values and use
  // them here.
  WorkerPrivate::OverrideLoadInfoLoadGroup(info);

  RuntimeService* rs = RuntimeService::GetOrCreateService();
  if (!rs) {
    return NS_ERROR_FAILURE;
  }

  AutoJSAPI jsapi;
  jsapi.Init();
  nsRefPtr<SharedWorker> sharedWorker;
  rv = rs->CreateSharedWorkerForServiceWorkerFromLoadInfo(jsapi.cx(), &info,
                                                          NS_ConvertUTF8toUTF16(aInfo->ScriptSpec()),
                                                          aInfo->Scope(),
                                                          getter_AddRefs(sharedWorker));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsRefPtr<ServiceWorker> serviceWorker =
    new ServiceWorker(nullptr, aInfo, sharedWorker);

  serviceWorker.forget(aServiceWorker);
  return NS_OK;
}

void
ServiceWorkerManager::InvalidateServiceWorkerRegistrationWorker(ServiceWorkerRegistrationInfo* aRegistration,
                                                                WhichServiceWorker aWhichOnes)
{
  AssertIsOnMainThread();
  nsTObserverArray<ServiceWorkerRegistrationBase*>::ForwardIterator it(mServiceWorkerRegistrations);
  while (it.HasMore()) {
    nsRefPtr<ServiceWorkerRegistrationBase> target = it.GetNext();
    nsAutoString regScope;
    target->GetScope(regScope);
    MOZ_ASSERT(!regScope.IsEmpty());

    NS_ConvertUTF16toUTF8 utf8Scope(regScope);

    if (utf8Scope.Equals(aRegistration->mScope)) {
      target->InvalidateWorkerReference(aWhichOnes);
    }
  }
}

NS_IMETHODIMP
ServiceWorkerManager::SoftUpdate(const nsAString& aScope)
{
  AssertIsOnMainThread();
  NS_ConvertUTF16toUTF8 scope(aScope);

  nsRefPtr<ServiceWorkerRegistrationInfo> registration;
  mServiceWorkerRegistrationInfos.Get(scope, getter_AddRefs(registration));
  if (NS_WARN_IF(!registration)) {
    return NS_OK;
  }

  // "If registration's uninstalling flag is set, abort these steps."
  if (registration->mPendingUninstall) {
    return NS_OK;
  }

  // "If registration's installing worker is not null, abort these steps."
  if (registration->mInstallingWorker) {
    return NS_OK;
  }

  // "Let newestWorker be the result of running Get Newest Worker algorithm
  // passing registration as its argument.
  // If newestWorker is null, abort these steps."
  nsRefPtr<ServiceWorkerInfo> newest = registration->Newest();
  if (!newest) {
    return NS_OK;
  }

  // "Set registration's registering script url to newestWorker's script url."
  registration->mScriptSpec = newest->ScriptSpec();

  ServiceWorkerJobQueue* queue = GetOrCreateJobQueue(scope);
  MOZ_ASSERT(queue);

  nsRefPtr<ServiceWorkerUpdateFinishCallback> cb =
    new ServiceWorkerUpdateFinishCallback();

  // "Invoke Update algorithm, or its equivalent, with client, registration as
  // its argument."
  nsRefPtr<ServiceWorkerRegisterJob> job =
    new ServiceWorkerRegisterJob(queue, registration, cb);
  queue->Append(job);
  return NS_OK;
}

namespace {

class MOZ_STACK_CLASS FilterRegistrationData
{
public:
  FilterRegistrationData(nsTArray<ServiceWorkerClientInfo>& aDocuments,
                         ServiceWorkerRegistrationInfo* aRegistration)
  : mDocuments(aDocuments),
    mRegistration(aRegistration)
  {
  }

  nsTArray<ServiceWorkerClientInfo>& mDocuments;
  nsRefPtr<ServiceWorkerRegistrationInfo> mRegistration;
};

static PLDHashOperator
EnumControlledDocuments(nsISupports* aKey,
                        ServiceWorkerRegistrationInfo* aRegistration,
                        void* aData)
{
  FilterRegistrationData* data = static_cast<FilterRegistrationData*>(aData);
  if (data->mRegistration != aRegistration) {
    return PL_DHASH_NEXT;
  }

  nsCOMPtr<nsIDocument> document = do_QueryInterface(aKey);

  if (!document || !document->GetWindow()) {
      return PL_DHASH_NEXT;
  }

  ServiceWorkerClientInfo clientInfo(document);
  data->mDocuments.AppendElement(clientInfo);

  return PL_DHASH_NEXT;
}

static PLDHashOperator
FireControllerChangeOnMatchingDocument(nsISupports* aKey,
                                       ServiceWorkerRegistrationInfo* aValue,
                                       void* aData)
{
  AssertIsOnMainThread();

  ServiceWorkerRegistrationInfo* contextReg = static_cast<ServiceWorkerRegistrationInfo*>(aData);
  if (aValue != contextReg) {
    return PL_DHASH_NEXT;
  }

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aKey);
  if (NS_WARN_IF(!doc)) {
    return PL_DHASH_NEXT;
  }

  nsCOMPtr<nsPIDOMWindow> w = doc->GetWindow();
  MOZ_ASSERT(w);
  auto* window = static_cast<nsGlobalWindow*>(w.get());
  if (NS_WARN_IF(!window)) {
    NS_WARNING("No valid nsGlobalWindow");
    return PL_DHASH_NEXT;
  }

  ErrorResult result;
  dom::Navigator* navigator = window->GetNavigator(result);
  if (NS_WARN_IF(result.Failed())) {
    return PL_DHASH_NEXT;
  }

  nsRefPtr<ServiceWorkerContainer> container = navigator->ServiceWorker();
  result = container->DispatchTrustedEvent(NS_LITERAL_STRING("controllerchange"));
  if (result.Failed()) {
    NS_WARNING("Failed to dispatch controllerchange event");
  }

  return PL_DHASH_NEXT;
}
} // anonymous namespace

void
ServiceWorkerManager::GetAllClients(const nsCString& aScope,
                                    nsTArray<ServiceWorkerClientInfo>& aControlledDocuments)
{
  nsRefPtr<ServiceWorkerRegistrationInfo> registration = GetRegistration(aScope);

  if (!registration) {
    // The registration was removed, leave the array empty.
    return;
  }

  FilterRegistrationData data(aControlledDocuments, registration);

  mControlledDocuments.EnumerateRead(EnumControlledDocuments, &data);
}

void
ServiceWorkerManager::FireControllerChange(ServiceWorkerRegistrationInfo* aRegistration)
{
  mControlledDocuments.EnumerateRead(FireControllerChangeOnMatchingDocument, aRegistration);
}

ServiceWorkerRegistrationInfo*
ServiceWorkerManager::CreateNewRegistration(const nsCString& aScope,
                                            nsIPrincipal* aPrincipal)
{
#ifdef DEBUG
  AssertIsOnMainThread();
  nsCOMPtr<nsIURI> scopeURI;
  nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), aScope, nullptr, nullptr);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
#endif
  ServiceWorkerRegistrationInfo* registration = new ServiceWorkerRegistrationInfo(aScope, aPrincipal);
  // From now on ownership of registration is with
  // mServiceWorkerRegistrationInfos.
  mServiceWorkerRegistrationInfos.Put(aScope, registration);
  AddScope(mOrderedScopes, aScope);
  return registration;
}

void
ServiceWorkerManager::MaybeRemoveRegistration(ServiceWorkerRegistrationInfo* aRegistration)
{
  MOZ_ASSERT(aRegistration);
  nsRefPtr<ServiceWorkerInfo> newest = aRegistration->Newest();
  if (!newest) {
    RemoveRegistration(aRegistration);
  }
}

void
ServiceWorkerManager::RemoveRegistration(ServiceWorkerRegistrationInfo* aRegistration)
{
  MOZ_ASSERT(aRegistration);
  MOZ_ASSERT(!aRegistration->IsControllingDocuments());
  MOZ_ASSERT(mServiceWorkerRegistrationInfos.Contains(aRegistration->mScope));
  ServiceWorkerManager::RemoveScope(mOrderedScopes, aRegistration->mScope);

  // Hold a ref since the hashtable may be the last ref.
  nsRefPtr<ServiceWorkerRegistrationInfo> reg;
  mServiceWorkerRegistrationInfos.Remove(aRegistration->mScope,
                                         getter_AddRefs(reg));
  MOZ_ASSERT(reg);

  // All callers should be either from a job in which case the actor is
  // available, or from MaybeStopControlling(), in which case, this will only be
  // called if a valid registration is found. If a valid registration exists,
  // it means the actor is available since the original map of registrations is
  // populated by it, and any new registrations wait until the actor is
  // available before proceeding (See ServiceWorkerRegisterJob::Start).
  MOZ_ASSERT(mActor);

  PrincipalInfo principalInfo;
  if (NS_WARN_IF(NS_FAILED(PrincipalToPrincipalInfo(reg->mPrincipal,
                                                    &principalInfo)))) {
    //XXXnsm I can't think of any other reason a stored principal would fail to
    //convert.
    NS_WARNING("Unable to unregister serviceworker due to possible OOM");
    return;
  }
  mActor->SendUnregisterServiceWorker(principalInfo, NS_ConvertUTF8toUTF16(reg->mScope));
}

class ServiceWorkerDataInfo final : public nsIServiceWorkerInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVICEWORKERINFO

  static already_AddRefed<ServiceWorkerDataInfo>
  Create(const ServiceWorkerRegistrationData& aData);

private:
  ServiceWorkerDataInfo()
  {}

  ~ServiceWorkerDataInfo()
  {}

  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsString mScope;
  nsString mScriptSpec;
  nsString mCurrentWorkerURL;
  nsString mActiveCacheName;
  nsString mWaitingCacheName;
};

NS_IMPL_ISUPPORTS(ServiceWorkerDataInfo, nsIServiceWorkerInfo)

/* static */ already_AddRefed<ServiceWorkerDataInfo>
ServiceWorkerDataInfo::Create(const ServiceWorkerRegistrationData& aData)
{
  AssertIsOnMainThread();

  nsRefPtr<ServiceWorkerDataInfo> info = new ServiceWorkerDataInfo();

  info->mPrincipal = PrincipalInfoToPrincipal(aData.principal());
  if (!info->mPrincipal) {
    return nullptr;
  }

  CopyUTF8toUTF16(aData.scope(), info->mScope);
  CopyUTF8toUTF16(aData.scriptSpec(), info->mScriptSpec);
  CopyUTF8toUTF16(aData.currentWorkerURL(), info->mCurrentWorkerURL);
  info->mActiveCacheName = aData.activeCacheName();
  info->mWaitingCacheName = aData.waitingCacheName();

  return info.forget();
}

NS_IMETHODIMP
ServiceWorkerDataInfo::GetPrincipal(nsIPrincipal** aPrincipal)
{
  AssertIsOnMainThread();
  NS_ADDREF(*aPrincipal = mPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerDataInfo::GetScope(nsAString& aScope)
{
  AssertIsOnMainThread();
  aScope = mScope;
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerDataInfo::GetScriptSpec(nsAString& aScriptSpec)
{
  AssertIsOnMainThread();
  aScriptSpec = mScriptSpec;
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerDataInfo::GetCurrentWorkerURL(nsAString& aCurrentWorkerURL)
{
  AssertIsOnMainThread();
  aCurrentWorkerURL = mCurrentWorkerURL;
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerDataInfo::GetActiveCacheName(nsAString& aActiveCacheName)
{
  AssertIsOnMainThread();
  aActiveCacheName = mActiveCacheName;
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerDataInfo::GetWaitingCacheName(nsAString& aWaitingCacheName)
{
  AssertIsOnMainThread();
  aWaitingCacheName = mWaitingCacheName;
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerManager::GetAllRegistrations(nsIArray** aResult)
{
  AssertIsOnMainThread();

  nsRefPtr<ServiceWorkerRegistrar> swr = ServiceWorkerRegistrar::Get();
  MOZ_ASSERT(swr);

  nsTArray<ServiceWorkerRegistrationData> data;
  swr->GetRegistrations(data);

  nsCOMPtr<nsIMutableArray> array(do_CreateInstance(NS_ARRAY_CONTRACTID));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t i = 0, len = data.Length(); i < len; ++i) {
    nsCOMPtr<nsIServiceWorkerInfo> info = ServiceWorkerDataInfo::Create(data[i]);
    if (!info) {
      return NS_ERROR_FAILURE;
    }

    array->AppendElement(info, false);
  }

  array.forget(aResult);
  return NS_OK;
}

static PLDHashOperator
UpdateEachRegistration(const nsACString& aKey,
                       ServiceWorkerRegistrationInfo* aInfo,
                       void* aUserArg) {
  auto This = static_cast<ServiceWorkerManager*>(aUserArg);
  MOZ_ASSERT(!aInfo->mScope.IsEmpty());
  nsresult res = This->SoftUpdate(NS_ConvertUTF8toUTF16(aInfo->mScope));
  unused << NS_WARN_IF(NS_FAILED(res));

  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
ServiceWorkerManager::UpdateAllRegistrations()
{
  AssertIsOnMainThread();

  mServiceWorkerRegistrationInfos.EnumerateRead(UpdateEachRegistration, this);

  return NS_OK;
}

void
ServiceWorkerInfo::AppendWorker(ServiceWorker* aWorker)
{
  MOZ_ASSERT(aWorker);
#ifdef DEBUG
  nsAutoString workerURL;
  aWorker->GetScriptURL(workerURL);
  MOZ_ASSERT(workerURL.Equals(NS_ConvertUTF8toUTF16(mScriptSpec)));
#endif
  MOZ_ASSERT(!mInstances.Contains(aWorker));

  mInstances.AppendElement(aWorker);
  aWorker->SetState(State());
}

void
ServiceWorkerInfo::RemoveWorker(ServiceWorker* aWorker)
{
  MOZ_ASSERT(aWorker);
#ifdef DEBUG
  nsAutoString workerURL;
  aWorker->GetScriptURL(workerURL);
  MOZ_ASSERT(workerURL.Equals(NS_ConvertUTF8toUTF16(mScriptSpec)));
#endif
  MOZ_ASSERT(mInstances.Contains(aWorker));

  mInstances.RemoveElement(aWorker);
}

void
ServiceWorkerInfo::UpdateState(ServiceWorkerState aState)
{
#ifdef DEBUG
  // Any state can directly transition to redundant, but everything else is
  // ordered.
  if (aState != ServiceWorkerState::Redundant) {
    MOZ_ASSERT_IF(mState == ServiceWorkerState::EndGuard_, aState == ServiceWorkerState::Installing);
    MOZ_ASSERT_IF(mState == ServiceWorkerState::Installing, aState == ServiceWorkerState::Installed);
    MOZ_ASSERT_IF(mState == ServiceWorkerState::Installed, aState == ServiceWorkerState::Activating);
    MOZ_ASSERT_IF(mState == ServiceWorkerState::Activating, aState == ServiceWorkerState::Activated);
  }
  // Activated can only go to redundant.
  MOZ_ASSERT_IF(mState == ServiceWorkerState::Activated, aState == ServiceWorkerState::Redundant);
#endif
  mState = aState;
  for (uint32_t i = 0; i < mInstances.Length(); ++i) {
    mInstances[i]->QueueStateChangeEvent(mState);
  }
}
END_WORKERS_NAMESPACE
