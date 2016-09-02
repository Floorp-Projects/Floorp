/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerUpdateJob.h"

#include "nsIScriptError.h"
#include "nsIURL.h"
#include "ServiceWorkerScriptCache.h"
#include "Workers.h"

namespace mozilla {
namespace dom {
namespace workers {

namespace {

/**
 * The spec mandates slightly different behaviors for computing the scope
 * prefix string in case a Service-Worker-Allowed header is specified versus
 * when it's not available.
 *
 * With the header:
 *   "Set maxScopeString to "/" concatenated with the strings in maxScope's
 *    path (including empty strings), separated from each other by "/"."
 * Without the header:
 *   "Set maxScopeString to "/" concatenated with the strings, except the last
 *    string that denotes the script's file name, in registration's registering
 *    script url's path (including empty strings), separated from each other by
 *    "/"."
 *
 * In simpler terms, if the header is not present, we should only use the
 * "directory" part of the pathname, and otherwise the entire pathname should be
 * used.  ScopeStringPrefixMode allows the caller to specify the desired
 * behavior.
 */
enum ScopeStringPrefixMode {
  eUseDirectory,
  eUsePath
};

nsresult
GetRequiredScopeStringPrefix(nsIURI* aScriptURI, nsACString& aPrefix,
                             ScopeStringPrefixMode aPrefixMode)
{
  nsresult rv = aScriptURI->GetPrePath(aPrefix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aPrefixMode == eUseDirectory) {
    nsCOMPtr<nsIURL> scriptURL(do_QueryInterface(aScriptURI));
    if (NS_WARN_IF(!scriptURL)) {
      return NS_ERROR_FAILURE;
    }

    nsAutoCString dir;
    rv = scriptURL->GetDirectory(dir);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    aPrefix.Append(dir);
  } else if (aPrefixMode == eUsePath) {
    nsAutoCString path;
    rv = aScriptURI->GetPath(path);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    aPrefix.Append(path);
  } else {
    MOZ_ASSERT_UNREACHABLE("Invalid value for aPrefixMode");
  }
  return NS_OK;
}

} // anonymous namespace

class ServiceWorkerUpdateJob::CompareCallback final : public serviceWorkerScriptCache::CompareCallback
{
  RefPtr<ServiceWorkerUpdateJob> mJob;

  ~CompareCallback()
  {
  }

public:
  explicit CompareCallback(ServiceWorkerUpdateJob* aJob)
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

  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerUpdateJob::CompareCallback, override)
};

class ServiceWorkerUpdateJob::ContinueUpdateRunnable final : public LifeCycleEventCallback
{
  nsMainThreadPtrHandle<ServiceWorkerUpdateJob> mJob;
  bool mSuccess;

public:
  explicit ContinueUpdateRunnable(const nsMainThreadPtrHandle<ServiceWorkerUpdateJob>& aJob)
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

class ServiceWorkerUpdateJob::ContinueInstallRunnable final : public LifeCycleEventCallback
{
  nsMainThreadPtrHandle<ServiceWorkerUpdateJob> mJob;
  bool mSuccess;

public:
  explicit ContinueInstallRunnable(const nsMainThreadPtrHandle<ServiceWorkerUpdateJob>& aJob)
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

ServiceWorkerUpdateJob::ServiceWorkerUpdateJob(nsIPrincipal* aPrincipal,
                                               const nsACString& aScope,
                                               const nsACString& aScriptSpec,
                                               nsILoadGroup* aLoadGroup)
  : ServiceWorkerJob(Type::Update, aPrincipal, aScope, aScriptSpec)
  , mLoadGroup(aLoadGroup)
{
}

already_AddRefed<ServiceWorkerRegistrationInfo>
ServiceWorkerUpdateJob::GetRegistration() const
{
  AssertIsOnMainThread();
  RefPtr<ServiceWorkerRegistrationInfo> ref = mRegistration;
  return ref.forget();
}

ServiceWorkerUpdateJob::ServiceWorkerUpdateJob(Type aType,
                                               nsIPrincipal* aPrincipal,
                                               const nsACString& aScope,
                                               const nsACString& aScriptSpec,
                                               nsILoadGroup* aLoadGroup)
  : ServiceWorkerJob(aType, aPrincipal, aScope, aScriptSpec)
  , mLoadGroup(aLoadGroup)
{
}

ServiceWorkerUpdateJob::~ServiceWorkerUpdateJob()
{
}

void
ServiceWorkerUpdateJob::FailUpdateJob(ErrorResult& aRv)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aRv.Failed());

  // Cleanup after a failed installation.  This essentially implements
  // step 12 of the Install algorithm.
  //
  //  https://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html#installation-algorithm
  //
  // The spec currently only runs this after an install event fails,
  // but we must handle many more internal errors.  So we check for
  // cleanup on every non-successful exit.
  if (mRegistration) {
    mRegistration->ClearEvaluating();
    mRegistration->ClearInstalling();

    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (swm) {
      swm->MaybeRemoveRegistration(mRegistration);
    }
  }

  mRegistration = nullptr;

  Finish(aRv);
}

void
ServiceWorkerUpdateJob::FailUpdateJob(nsresult aRv)
{
  ErrorResult rv(aRv);
  FailUpdateJob(rv);
}

void
ServiceWorkerUpdateJob::AsyncExecute()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(GetType() == Type::Update);

  if (Canceled()) {
    FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  // Begin step 1 of the Update algorithm.
  //
  //  https://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html#update-algorithm

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

  // If a Register job with a new script executed ahead of us in the job queue,
  // then our update for the old script no longer makes sense.  Simply abort
  // in this case.
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
ServiceWorkerUpdateJob::SetRegistration(ServiceWorkerRegistrationInfo* aRegistration)
{
  AssertIsOnMainThread();

  MOZ_ASSERT(!mRegistration);
  MOZ_ASSERT(aRegistration);
  mRegistration = aRegistration;
}

void
ServiceWorkerUpdateJob::Update()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!Canceled());

  // SetRegistration() must be called before Update().
  MOZ_ASSERT(mRegistration);
  MOZ_ASSERT(!mRegistration->GetInstalling());

  // Begin the script download and comparison steps starting at step 5
  // of the Update algorithm.

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
ServiceWorkerUpdateJob::ComparisonResult(nsresult aStatus,
                                         bool aInCacheAndEqual,
                                         const nsAString& aNewCacheName,
                                         const nsACString& aMaxScope)
{
  AssertIsOnMainThread();

  if (NS_WARN_IF(Canceled())) {
    FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  // Handle failure of the download or comparison.  This is part of Update
  // step 5 as "If the algorithm asynchronously completes with null, then:".
  if (NS_WARN_IF(NS_FAILED(aStatus))) {
    FailUpdateJob(aStatus);
    return;
  }

  // The spec validates the response before performing the byte-for-byte check.
  // Here we perform the comparison in another module and then validate the
  // script URL and scope.  Make sure to do this validation before accepting
  // an byte-for-byte match since the service-worker-allowed header might have
  // changed since the last time it was installed.

  // This is step 2 the "validate response" section of Update algorithm step 5.
  // Step 1 is performed in the serviceWorkerScriptCache code.

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
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to format localized string");
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
  // byte-for-byte match.  This is step 6 of the Update algorithm.
  if (aInCacheAndEqual) {
    Finish(NS_OK);
    return;
  }

  Telemetry::Accumulate(Telemetry::SERVICE_WORKER_UPDATED, 1);

  // Begin step 7 of the Update algorithm to evaluate the new script.

  RefPtr<ServiceWorkerInfo> sw =
    new ServiceWorkerInfo(mRegistration->mPrincipal,
                          mRegistration->mScope,
                          mScriptSpec, aNewCacheName);

  mRegistration->SetEvaluating(sw);

  nsMainThreadPtrHandle<ServiceWorkerUpdateJob> handle(
      new nsMainThreadPtrHolder<ServiceWorkerUpdateJob>(this));
  RefPtr<LifeCycleEventCallback> callback = new ContinueUpdateRunnable(handle);

  ServiceWorkerPrivate* workerPrivate = sw->WorkerPrivate();
  MOZ_ASSERT(workerPrivate);
  rv = workerPrivate->CheckScriptEvaluation(callback);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
    return;
  }
}

void
ServiceWorkerUpdateJob::ContinueUpdateAfterScriptEval(bool aScriptEvaluationResult)
{
  AssertIsOnMainThread();

  if (Canceled()) {
    FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  // Step 7.5 of the Update algorithm verifying that the script evaluated
  // successfully.

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
ServiceWorkerUpdateJob::Install()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!Canceled());

  MOZ_ASSERT(!mRegistration->GetInstalling());

  // Begin step 2 of the Install algorithm.
  //
  //  https://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html#installation-algorithm

  mRegistration->TransitionEvaluatingToInstalling();

  // Step 6 of the Install algorithm resolving the job promise.
  InvokeResultCallbacks(NS_OK);

  // The job promise cannot be rejected after this point, but the job can
  // still fail; e.g. if the install event handler throws, etc.

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();

  // fire the updatefound event
  nsCOMPtr<nsIRunnable> upr =
    NewRunnableMethod<RefPtr<ServiceWorkerRegistrationInfo>>(
      swm,
      &ServiceWorkerManager::FireUpdateFoundOnServiceWorkerRegistrations,
      mRegistration);
  NS_DispatchToMainThread(upr);

  // Call ContinueAfterInstallEvent(false) on main thread if the SW
  // script fails to load.
  nsCOMPtr<nsIRunnable> failRunnable = NewRunnableMethod<bool>
    (this, &ServiceWorkerUpdateJob::ContinueAfterInstallEvent, false);

  nsMainThreadPtrHandle<ServiceWorkerUpdateJob> handle(
    new nsMainThreadPtrHolder<ServiceWorkerUpdateJob>(this));
  RefPtr<LifeCycleEventCallback> callback = new ContinueInstallRunnable(handle);

  // Send the install event to the worker thread
  ServiceWorkerPrivate* workerPrivate =
    mRegistration->GetInstalling()->WorkerPrivate();
  nsresult rv = workerPrivate->SendLifeCycleEvent(NS_LITERAL_STRING("install"),
                                                  callback, failRunnable);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ContinueAfterInstallEvent(false /* aSuccess */);
  }
}

void
ServiceWorkerUpdateJob::ContinueAfterInstallEvent(bool aInstallEventSuccess)
{
  if (Canceled()) {
    return FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
  }

  MOZ_ASSERT(mRegistration->GetInstalling());

  // Continue executing the Install algorithm at step 12.

  // "If installFailed is true"
  if (NS_WARN_IF(!aInstallEventSuccess)) {
    // The installing worker is cleaned up by FailUpdateJob().
    FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  mRegistration->TransitionInstallingToWaiting();

  Finish(NS_OK);

  // Step 20 calls for explicitly waiting for queued event tasks to fire.  Instead,
  // we simply queue a runnable to execute Activate.  This ensures the events are
  // flushed from the queue before proceeding.

  // Step 22 of the Install algorithm.  Activate is executed after the completion
  // of this job.  The controlling client and skipWaiting checks are performed
  // in TryToActivate().
  mRegistration->TryToActivateAsync();
}

} // namespace workers
} // namespace dom
} // namespace mozilla
