/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerUpdateJob.h"

#include "ServiceWorkerScriptCache.h"
#include "Workers.h"

namespace mozilla {
namespace dom {
namespace workers {

class ServiceWorkerUpdateJob2::CompareCallback final : public serviceWorkerScriptCache::CompareCallback
{
  RefPtr<ServiceWorkerUpdateJob2> mJob;

  ~CompareCallback()
  {
  }

public:
  explicit CompareCallback(ServiceWorkerUpdateJob2* aJob)
    : mJob(aJob)
  {
    MOZ_ASSERT(mJob);
  }

  virtual void
  ComparisonResult(nsresult aStatus,
                   bool aInCacheAndEqual,
                   const nsAString& aNewCacheName,
                   const nsACString& aMaxScope) override
  {
    mJob->ComparisonResult(aStatus, aInCacheAndEqual, aNewCacheName, aMaxScope);
  }

  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerUpdateJob2::CompareCallback, override)
};

class ServiceWorkerUpdateJob2::ContinueUpdateRunnable final : public LifeCycleEventCallback
{
  nsMainThreadPtrHandle<ServiceWorkerUpdateJob2> mJob;
  bool mSuccess;

public:
  explicit ContinueUpdateRunnable(const nsMainThreadPtrHandle<ServiceWorkerUpdateJob2>& aJob)
    : mJob(aJob)
    , mSuccess(false)
  {
    AssertIsOnMainThread();
  }

  void
  SetResult(bool aResult) override
  {
    mSuccess = aResult;
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();
    mJob->ContinueUpdateAfterScriptEval(mSuccess);
    mJob = nullptr;
    return NS_OK;
  }
};

class ServiceWorkerUpdateJob2::ContinueInstallRunnable final : public LifeCycleEventCallback
{
  nsMainThreadPtrHandle<ServiceWorkerUpdateJob2> mJob;
  bool mSuccess;

public:
  explicit ContinueInstallRunnable(const nsMainThreadPtrHandle<ServiceWorkerUpdateJob2>& aJob)
    : mJob(aJob)
    , mSuccess(false)
  {
    AssertIsOnMainThread();
  }

  void
  SetResult(bool aResult) override
  {
    mSuccess = aResult;
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();
    mJob->ContinueAfterInstallEvent(mSuccess);
    mJob = nullptr;
    return NS_OK;
  }
};

ServiceWorkerUpdateJob2::ServiceWorkerUpdateJob2(nsIPrincipal* aPrincipal,
                        const nsACString& aScope,
                        const nsACString& aScriptSpec,
                        nsILoadGroup* aLoadGroup)
  : ServiceWorkerJob2(Type::Update, aPrincipal, aScope, aScriptSpec)
  , mLoadGroup(aLoadGroup)
{
}

already_AddRefed<ServiceWorkerRegistrationInfo>
ServiceWorkerUpdateJob2::GetRegistration() const
{
  AssertIsOnMainThread();
  RefPtr<ServiceWorkerRegistrationInfo> ref = mRegistration;
  return ref.forget();
}

ServiceWorkerUpdateJob2::ServiceWorkerUpdateJob2(Type aType,
                                                 nsIPrincipal* aPrincipal,
                                                 const nsACString& aScope,
                                                 const nsACString& aScriptSpec,
                                                 nsILoadGroup* aLoadGroup)
  : ServiceWorkerJob2(aType, aPrincipal, aScope, aScriptSpec)
  , mLoadGroup(aLoadGroup)
{
}

ServiceWorkerUpdateJob2::~ServiceWorkerUpdateJob2()
{
}

void
ServiceWorkerUpdateJob2::FailUpdateJob(ErrorResult& aRv)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aRv.Failed());

  if (mRegistration) {
    if (mServiceWorker) {
      mServiceWorker->UpdateState(ServiceWorkerState::Redundant);
      serviceWorkerScriptCache::PurgeCache(mRegistration->mPrincipal,
                                           mServiceWorker->CacheName());
    }

    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();

    if (mRegistration->mInstallingWorker) {
      mRegistration->mInstallingWorker->UpdateState(ServiceWorkerState::Redundant);
      mRegistration->mInstallingWorker = nullptr;
      if (swm) {
        swm->InvalidateServiceWorkerRegistrationWorker(mRegistration,
                                                       WhichServiceWorker::INSTALLING_WORKER);
      }
      serviceWorkerScriptCache::PurgeCache(mRegistration->mPrincipal,
                                           mRegistration->mInstallingWorker->CacheName());
    }

    if (swm) {
      swm->MaybeRemoveRegistration(mRegistration);
    }
  }

  mServiceWorker = nullptr;
  mRegistration = nullptr;

  Finish(aRv);
}

void
ServiceWorkerUpdateJob2::FailUpdateJob(nsresult aRv)
{
  ErrorResult rv(aRv);
  FailUpdateJob(rv);
}

void
ServiceWorkerUpdateJob2::AsyncExecute()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(GetType() == Type::Update);

  if (Canceled()) {
    FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  RefPtr<ServiceWorkerRegistrationInfo> registration =
    swm->GetRegistration(mPrincipal, mScope);

  if (!registration || registration->mPendingUninstall) {
    ErrorResult rv;
    rv.ThrowTypeError<MSG_SW_UPDATE_BAD_REGISTRATION>(NS_ConvertUTF8toUTF16(mScope),
                                                      NS_LITERAL_STRING("uninstalled"));
    FailUpdateJob(rv);
    return;
  }

  // If a different script has been registered between when this update
  // was scheduled and it running now, then simply abort.
  RefPtr<ServiceWorkerInfo> newest = registration->Newest();
  if (newest && !mScriptSpec.Equals(newest->ScriptSpec())) {
    ErrorResult rv;
    rv.ThrowTypeError<MSG_SW_UPDATE_BAD_REGISTRATION>(NS_ConvertUTF8toUTF16(mScope),
                                                      NS_LITERAL_STRING("changed"));
    FailUpdateJob(rv);
    return;
  }

  SetRegistration(registration);
  Update();
}

void
ServiceWorkerUpdateJob2::SetRegistration(ServiceWorkerRegistrationInfo* aRegistration)
{
  AssertIsOnMainThread();

  MOZ_ASSERT(!mRegistration);
  MOZ_ASSERT(aRegistration);
  mRegistration = aRegistration;
}

void
ServiceWorkerUpdateJob2::Update()
{
  AssertIsOnMainThread();

  // SetRegistration() must be called before Update().
  MOZ_ASSERT(mRegistration);

  if (Canceled()) {
    FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  MOZ_ASSERT(!mRegistration->mInstallingWorker);

  RefPtr<ServiceWorkerInfo> workerInfo = mRegistration->Newest();
  nsAutoString cacheName;

  // If the script has not changed, we need to perform a byte-for-byte
  // comparison.
  if (workerInfo && workerInfo->ScriptSpec().Equals(mScriptSpec)) {
    cacheName = workerInfo->CacheName();
  }

  RefPtr<CompareCallback> callback = new CompareCallback(this);

  nsresult rv =
    serviceWorkerScriptCache::Compare(mRegistration, mPrincipal, cacheName,
                                      NS_ConvertUTF8toUTF16(mScriptSpec),
                                      callback, mLoadGroup);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailUpdateJob(rv);
    return;
  }
}

void
ServiceWorkerUpdateJob2::ComparisonResult(nsresult aStatus,
                                          bool aInCacheAndEqual,
                                          const nsAString& aNewCacheName,
                                          const nsACString& aMaxScope)
{
  AssertIsOnMainThread();

  if (NS_WARN_IF(Canceled())) {
    FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  if (NS_WARN_IF(NS_FAILED(aStatus))) {
    FailUpdateJob(aStatus);
    return;
  }

  // The spec validates the response before performing the byte-for-byte check.
  // Here we perform the comparison in another module and then validate the
  // script URL and scope.  Make sure to do this validation before accepting
  // an byte-for-byte match since the service-worker-allowed header might have
  // changed since the last time it was installed.

  nsCOMPtr<nsIURI> scriptURI;
  nsresult rv = NS_NewURI(getter_AddRefs(scriptURI), mScriptSpec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailUpdateJob(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsCOMPtr<nsIURI> maxScopeURI;
  if (!aMaxScope.IsEmpty()) {
    rv = NS_NewURI(getter_AddRefs(maxScopeURI), aMaxScope,
                   nullptr, scriptURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      FailUpdateJob(NS_ERROR_DOM_SECURITY_ERR);
      return;
    }
  }

  nsAutoCString defaultAllowedPrefix;
  rv = GetRequiredScopeStringPrefix(scriptURI, defaultAllowedPrefix,
                                    eUseDirectory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailUpdateJob(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsAutoCString maxPrefix(defaultAllowedPrefix);
  if (maxScopeURI) {
    rv = GetRequiredScopeStringPrefix(maxScopeURI, maxPrefix, eUsePath);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      FailUpdateJob(NS_ERROR_DOM_SECURITY_ERR);
      return;
    }
  }

  if (!StringBeginsWith(mRegistration->mScope, maxPrefix)) {
    nsXPIDLString message;
    NS_ConvertUTF8toUTF16 reportScope(mRegistration->mScope);
    NS_ConvertUTF8toUTF16 reportMaxPrefix(maxPrefix);
    const char16_t* params[] = { reportScope.get(), reportMaxPrefix.get() };

    rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                               "ServiceWorkerScopePathMismatch",
                                               params, message);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to format localized string");
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    swm->ReportToAllClients(mScope,
                            message,
                            EmptyString(),
                            EmptyString(), 0, 0,
                            nsIScriptError::errorFlag);
    FailUpdateJob(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  // The response has been validated, so now we can consider if its a
  // byte-for-byte match.
  if (aInCacheAndEqual) {
    Finish(NS_OK);
    return;
  }

  Telemetry::Accumulate(Telemetry::SERVICE_WORKER_UPDATED, 1);

  MOZ_ASSERT(!mServiceWorker);
  mServiceWorker = new ServiceWorkerInfo(mRegistration->mPrincipal,
                                         mRegistration->mScope,
                                         mScriptSpec, aNewCacheName);

  nsMainThreadPtrHandle<ServiceWorkerUpdateJob2> handle(
      new nsMainThreadPtrHolder<ServiceWorkerUpdateJob2>(this));
  RefPtr<LifeCycleEventCallback> callback = new ContinueUpdateRunnable(handle);

  ServiceWorkerPrivate* workerPrivate = mServiceWorker->WorkerPrivate();
  MOZ_ASSERT(workerPrivate);
  rv = workerPrivate->CheckScriptEvaluation(callback);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
    return;
  }
}

void
ServiceWorkerUpdateJob2::ContinueUpdateAfterScriptEval(bool aScriptEvaluationResult)
{
  AssertIsOnMainThread();

  if (Canceled()) {
    FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  if (NS_WARN_IF(!aScriptEvaluationResult)) {
    ErrorResult error;

    NS_ConvertUTF8toUTF16 scriptSpec(mScriptSpec);
    NS_ConvertUTF8toUTF16 scope(mRegistration->mScope);
    error.ThrowTypeError<MSG_SW_SCRIPT_THREW>(scriptSpec, scope);
    FailUpdateJob(error);
    return;
  }

  Install();
}

void
ServiceWorkerUpdateJob2::Install()
{
  AssertIsOnMainThread();

  if (Canceled()) {
    return FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
  }

  MOZ_ASSERT(!mRegistration->mInstallingWorker);

  MOZ_ASSERT(mServiceWorker);
  mRegistration->mInstallingWorker = mServiceWorker.forget();
  mRegistration->mInstallingWorker->UpdateState(ServiceWorkerState::Installing);
  mRegistration->NotifyListenersOnChange();

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  swm->InvalidateServiceWorkerRegistrationWorker(mRegistration,
                                                 WhichServiceWorker::INSTALLING_WORKER);

  InvokeResultCallbacks(NS_OK);

  // The job should NOT fail from this point on.

  // fire the updatefound event
  nsCOMPtr<nsIRunnable> upr =
    NS_NewRunnableMethodWithArg<RefPtr<ServiceWorkerRegistrationInfo>>(
      swm,
      &ServiceWorkerManager::FireUpdateFoundOnServiceWorkerRegistrations,
      mRegistration);
  NS_DispatchToMainThread(upr);

  // Call ContinueAfterInstallEvent(false) on main thread if the SW
  // script fails to load.
  nsCOMPtr<nsIRunnable> failRunnable = NS_NewRunnableMethodWithArgs<bool>
    (this, &ServiceWorkerUpdateJob2::ContinueAfterInstallEvent, false);

  nsMainThreadPtrHandle<ServiceWorkerUpdateJob2> handle(
    new nsMainThreadPtrHolder<ServiceWorkerUpdateJob2>(this));
  RefPtr<LifeCycleEventCallback> callback = new ContinueInstallRunnable(handle);

  // Send the install event to the worker thread
  ServiceWorkerPrivate* workerPrivate =
    mRegistration->mInstallingWorker->WorkerPrivate();
  nsresult rv = workerPrivate->SendLifeCycleEvent(NS_LITERAL_STRING("install"),
                                                  callback, failRunnable);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ContinueAfterInstallEvent(false /* aSuccess */);
  }
}

void
ServiceWorkerUpdateJob2::ContinueAfterInstallEvent(bool aInstallEventSuccess)
{
  if (Canceled()) {
    return FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
  }

  MOZ_ASSERT(mRegistration->mInstallingWorker);

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();

  // "If installFailed is true"
  if (NS_WARN_IF(!aInstallEventSuccess)) {
    mRegistration->mInstallingWorker->UpdateState(ServiceWorkerState::Redundant);
    mRegistration->mInstallingWorker = nullptr;
    swm->InvalidateServiceWorkerRegistrationWorker(mRegistration,
                                                   WhichServiceWorker::INSTALLING_WORKER);
    swm->MaybeRemoveRegistration(mRegistration);
    FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  // "If registration's waiting worker is not null"
  if (mRegistration->mWaitingWorker) {
    mRegistration->mWaitingWorker->WorkerPrivate()->TerminateWorker();
    mRegistration->mWaitingWorker->UpdateState(ServiceWorkerState::Redundant);

    nsresult rv =
      serviceWorkerScriptCache::PurgeCache(mRegistration->mPrincipal,
                                           mRegistration->mWaitingWorker->CacheName());
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to purge the old waiting cache.");
    }
  }

  mRegistration->mWaitingWorker = mRegistration->mInstallingWorker.forget();
  mRegistration->mWaitingWorker->UpdateState(ServiceWorkerState::Installed);
  mRegistration->NotifyListenersOnChange();
  swm->StoreRegistration(mPrincipal, mRegistration);
  swm->InvalidateServiceWorkerRegistrationWorker(mRegistration,
                                                 WhichServiceWorker::INSTALLING_WORKER |
                                                 WhichServiceWorker::WAITING_WORKER);

  Finish(NS_OK);

  // Activate() is invoked out of band of atomic.
  mRegistration->TryToActivateAsync();
}

} // namespace workers
} // namespace dom
} // namespace mozilla
