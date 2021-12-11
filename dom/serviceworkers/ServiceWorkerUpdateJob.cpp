/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerUpdateJob.h"

#include "mozilla/Telemetry.h"
#include "nsIScriptError.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "ServiceWorkerManager.h"
#include "ServiceWorkerPrivate.h"
#include "ServiceWorkerRegistrationInfo.h"
#include "ServiceWorkerScriptCache.h"
#include "mozilla/dom/WorkerCommon.h"

namespace mozilla {
namespace dom {

using serviceWorkerScriptCache::OnFailure;

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
enum ScopeStringPrefixMode { eUseDirectory, eUsePath };

nsresult GetRequiredScopeStringPrefix(nsIURI* aScriptURI, nsACString& aPrefix,
                                      ScopeStringPrefixMode aPrefixMode) {
  nsresult rv;
  if (aPrefixMode == eUseDirectory) {
    nsCOMPtr<nsIURL> scriptURL(do_QueryInterface(aScriptURI));
    if (NS_WARN_IF(!scriptURL)) {
      return NS_ERROR_FAILURE;
    }

    rv = scriptURL->GetDirectory(aPrefix);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else if (aPrefixMode == eUsePath) {
    rv = aScriptURI->GetPathQueryRef(aPrefix);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    MOZ_ASSERT_UNREACHABLE("Invalid value for aPrefixMode");
  }
  return NS_OK;
}

}  // anonymous namespace

class ServiceWorkerUpdateJob::CompareCallback final
    : public serviceWorkerScriptCache::CompareCallback {
  RefPtr<ServiceWorkerUpdateJob> mJob;

  ~CompareCallback() = default;

 public:
  explicit CompareCallback(ServiceWorkerUpdateJob* aJob) : mJob(aJob) {
    MOZ_ASSERT(mJob);
  }

  virtual void ComparisonResult(nsresult aStatus, bool aInCacheAndEqual,
                                OnFailure aOnFailure,
                                const nsAString& aNewCacheName,
                                const nsACString& aMaxScope,
                                nsLoadFlags aLoadFlags) override {
    mJob->ComparisonResult(aStatus, aInCacheAndEqual, aOnFailure, aNewCacheName,
                           aMaxScope, aLoadFlags);
  }

  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerUpdateJob::CompareCallback, override)
};

class ServiceWorkerUpdateJob::ContinueUpdateRunnable final
    : public LifeCycleEventCallback {
  nsMainThreadPtrHandle<ServiceWorkerUpdateJob> mJob;
  bool mSuccess;

 public:
  explicit ContinueUpdateRunnable(
      const nsMainThreadPtrHandle<ServiceWorkerUpdateJob>& aJob)
      : mJob(aJob), mSuccess(false) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  void SetResult(bool aResult) override { mSuccess = aResult; }

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    mJob->ContinueUpdateAfterScriptEval(mSuccess);
    mJob = nullptr;
    return NS_OK;
  }
};

class ServiceWorkerUpdateJob::ContinueInstallRunnable final
    : public LifeCycleEventCallback {
  nsMainThreadPtrHandle<ServiceWorkerUpdateJob> mJob;
  bool mSuccess;

 public:
  explicit ContinueInstallRunnable(
      const nsMainThreadPtrHandle<ServiceWorkerUpdateJob>& aJob)
      : mJob(aJob), mSuccess(false) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  void SetResult(bool aResult) override { mSuccess = aResult; }

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    mJob->ContinueAfterInstallEvent(mSuccess);
    mJob = nullptr;
    return NS_OK;
  }
};

ServiceWorkerUpdateJob::ServiceWorkerUpdateJob(
    nsIPrincipal* aPrincipal, const nsACString& aScope, nsCString aScriptSpec,
    ServiceWorkerUpdateViaCache aUpdateViaCache)
    : ServiceWorkerUpdateJob(Type::Update, aPrincipal, aScope,
                             std::move(aScriptSpec), aUpdateViaCache) {}

already_AddRefed<ServiceWorkerRegistrationInfo>
ServiceWorkerUpdateJob::GetRegistration() const {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<ServiceWorkerRegistrationInfo> ref = mRegistration;
  return ref.forget();
}

ServiceWorkerUpdateJob::ServiceWorkerUpdateJob(
    Type aType, nsIPrincipal* aPrincipal, const nsACString& aScope,
    nsCString aScriptSpec, ServiceWorkerUpdateViaCache aUpdateViaCache)
    : ServiceWorkerJob(aType, aPrincipal, aScope, std::move(aScriptSpec)),
      mUpdateViaCache(aUpdateViaCache),
      mOnFailure(serviceWorkerScriptCache::OnFailure::DoNothing) {}

ServiceWorkerUpdateJob::~ServiceWorkerUpdateJob() = default;

void ServiceWorkerUpdateJob::FailUpdateJob(ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRv.Failed());

  // Cleanup after a failed installation.  This essentially implements
  // step 13 of the Install algorithm.
  //
  //  https://w3c.github.io/ServiceWorker/#installation-algorithm
  //
  // The spec currently only runs this after an install event fails,
  // but we must handle many more internal errors.  So we check for
  // cleanup on every non-successful exit.
  if (mRegistration) {
    // Some kinds of failures indicate there is something broken in the
    // currently installed registration.  In these cases we want to fully
    // unregister.
    if (mOnFailure == OnFailure::Uninstall) {
      mRegistration->ClearAsCorrupt();
    }

    // Otherwise just clear the workers we may have created as part of the
    // update process.
    else {
      mRegistration->ClearEvaluating();
      mRegistration->ClearInstalling();
    }

    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (swm) {
      swm->MaybeRemoveRegistration(mRegistration);

      // Also clear the registration on disk if we are forcing uninstall
      // due to a particularly bad failure.
      if (mOnFailure == OnFailure::Uninstall) {
        swm->MaybeSendUnregister(mRegistration->Principal(),
                                 mRegistration->Scope());
      }
    }
  }

  mRegistration = nullptr;

  Finish(aRv);
}

void ServiceWorkerUpdateJob::FailUpdateJob(nsresult aRv) {
  ErrorResult rv(aRv);
  FailUpdateJob(rv);
}

void ServiceWorkerUpdateJob::AsyncExecute() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(GetType() == Type::Update);

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (Canceled() || !swm) {
    FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  // Invoke Update algorithm:
  // https://w3c.github.io/ServiceWorker/#update-algorithm
  //
  // "Let registration be the result of running the Get Registration algorithm
  // passing job’s scope url as the argument."
  RefPtr<ServiceWorkerRegistrationInfo> registration =
      swm->GetRegistration(mPrincipal, mScope);

  if (!registration) {
    ErrorResult rv;
    rv.ThrowTypeError<MSG_SW_UPDATE_BAD_REGISTRATION>(mScope, "uninstalled");
    FailUpdateJob(rv);
    return;
  }

  // "Let newestWorker be the result of running Get Newest Worker algorithm
  // passing registration as the argument."
  RefPtr<ServiceWorkerInfo> newest = registration->Newest();

  // "If job’s job type is update, and newestWorker is not null and its script
  // url does not equal job’s script url, then:
  //   1. Invoke Reject Job Promise with job and TypeError.
  //   2. Invoke Finish Job with job and abort these steps."
  if (newest && !newest->ScriptSpec().Equals(mScriptSpec)) {
    ErrorResult rv;
    rv.ThrowTypeError<MSG_SW_UPDATE_BAD_REGISTRATION>(mScope, "changed");
    FailUpdateJob(rv);
    return;
  }

  SetRegistration(registration);
  Update();
}

void ServiceWorkerUpdateJob::SetRegistration(
    ServiceWorkerRegistrationInfo* aRegistration) {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(!mRegistration);
  MOZ_ASSERT(aRegistration);
  mRegistration = aRegistration;
}

void ServiceWorkerUpdateJob::Update() {
  MOZ_ASSERT(NS_IsMainThread());
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

  nsresult rv = serviceWorkerScriptCache::Compare(
      mRegistration, mPrincipal, cacheName, NS_ConvertUTF8toUTF16(mScriptSpec),
      callback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailUpdateJob(rv);
    return;
  }
}

ServiceWorkerUpdateViaCache ServiceWorkerUpdateJob::GetUpdateViaCache() const {
  return mUpdateViaCache;
}

void ServiceWorkerUpdateJob::ComparisonResult(nsresult aStatus,
                                              bool aInCacheAndEqual,
                                              OnFailure aOnFailure,
                                              const nsAString& aNewCacheName,
                                              const nsACString& aMaxScope,
                                              nsLoadFlags aLoadFlags) {
  MOZ_ASSERT(NS_IsMainThread());

  mOnFailure = aOnFailure;

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (NS_WARN_IF(Canceled() || !swm)) {
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
    rv = NS_NewURI(getter_AddRefs(maxScopeURI), aMaxScope, nullptr, scriptURI);
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

  nsCOMPtr<nsIURI> scopeURI;
  rv = NS_NewURI(getter_AddRefs(scopeURI), mRegistration->Scope(), nullptr,
                 scriptURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailUpdateJob(NS_ERROR_FAILURE);
    return;
  }

  nsAutoCString scopeString;
  rv = scopeURI->GetPathQueryRef(scopeString);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailUpdateJob(NS_ERROR_FAILURE);
    return;
  }

  if (!StringBeginsWith(scopeString, maxPrefix)) {
    nsAutoString message;
    NS_ConvertUTF8toUTF16 reportScope(mRegistration->Scope());
    NS_ConvertUTF8toUTF16 reportMaxPrefix(maxPrefix);

    rv = nsContentUtils::FormatLocalizedString(
        message, nsContentUtils::eDOM_PROPERTIES,
        "ServiceWorkerScopePathMismatch", reportScope, reportMaxPrefix);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to format localized string");
    swm->ReportToAllClients(mScope, message, u""_ns, u""_ns, 0, 0,
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
  nsLoadFlags flags = aLoadFlags;
  if (GetUpdateViaCache() == ServiceWorkerUpdateViaCache::None) {
    flags |= nsIRequest::VALIDATE_ALWAYS;
  }

  RefPtr<ServiceWorkerInfo> sw = new ServiceWorkerInfo(
      mRegistration->Principal(), mRegistration->Scope(), mRegistration->Id(),
      mRegistration->Version(), mScriptSpec, aNewCacheName, flags);

  // If the registration is corrupt enough to force an uninstall if the
  // upgrade fails, then we want to make sure the upgrade takes effect
  // if it succeeds.  Therefore force the skip-waiting flag on to replace
  // the broken worker after install.
  if (aOnFailure == OnFailure::Uninstall) {
    sw->SetSkipWaitingFlag();
  }

  mRegistration->SetEvaluating(sw);

  nsMainThreadPtrHandle<ServiceWorkerUpdateJob> handle(
      new nsMainThreadPtrHolder<ServiceWorkerUpdateJob>(
          "ServiceWorkerUpdateJob", this));
  RefPtr<LifeCycleEventCallback> callback = new ContinueUpdateRunnable(handle);

  ServiceWorkerPrivate* workerPrivate = sw->WorkerPrivate();
  MOZ_ASSERT(workerPrivate);
  rv = workerPrivate->CheckScriptEvaluation(callback);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
    return;
  }
}

void ServiceWorkerUpdateJob::ContinueUpdateAfterScriptEval(
    bool aScriptEvaluationResult) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (Canceled() || !swm) {
    FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  // Step 7.5 of the Update algorithm verifying that the script evaluated
  // successfully.

  if (NS_WARN_IF(!aScriptEvaluationResult)) {
    ErrorResult error;
    error.ThrowTypeError<MSG_SW_SCRIPT_THREW>(mScriptSpec,
                                              mRegistration->Scope());
    FailUpdateJob(error);
    return;
  }

  Install();
}

void ServiceWorkerUpdateJob::Install() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!Canceled());

  MOZ_ASSERT(!mRegistration->GetInstalling());

  // Begin step 2 of the Install algorithm.
  //
  //  https://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html#installation-algorithm

  mRegistration->TransitionEvaluatingToInstalling();

  // Step 6 of the Install algorithm resolving the job promise.
  InvokeResultCallbacks(NS_OK);

  // Queue a task to fire an event named updatefound at all the
  // ServiceWorkerRegistration.
  mRegistration->FireUpdateFound();

  nsMainThreadPtrHandle<ServiceWorkerUpdateJob> handle(
      new nsMainThreadPtrHolder<ServiceWorkerUpdateJob>(
          "ServiceWorkerUpdateJob", this));
  RefPtr<LifeCycleEventCallback> callback = new ContinueInstallRunnable(handle);

  // Send the install event to the worker thread
  ServiceWorkerPrivate* workerPrivate =
      mRegistration->GetInstalling()->WorkerPrivate();
  nsresult rv = workerPrivate->SendLifeCycleEvent(u"install"_ns, callback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ContinueAfterInstallEvent(false /* aSuccess */);
  }
}

void ServiceWorkerUpdateJob::ContinueAfterInstallEvent(
    bool aInstallEventSuccess) {
  if (Canceled()) {
    return FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
  }

  // If we haven't been canceled we should have a registration.  There appears
  // to be a path where it gets cleared before we call into here.  Assert
  // to try to catch this condition, but don't crash in release.
  MOZ_DIAGNOSTIC_ASSERT(mRegistration);
  if (!mRegistration) {
    return FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
  }

  // Continue executing the Install algorithm at step 12.

  // "If installFailed is true"
  if (NS_WARN_IF(!aInstallEventSuccess)) {
    // The installing worker is cleaned up by FailUpdateJob().
    FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  // Abort the update Job if the installWorker is null (e.g. when an extension
  // is shutting down and all its workers have been terminated).
  if (!mRegistration->GetInstalling()) {
    return FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
  }

  mRegistration->TransitionInstallingToWaiting();

  Finish(NS_OK);

  // Step 20 calls for explicitly waiting for queued event tasks to fire.
  // Instead, we simply queue a runnable to execute Activate.  This ensures the
  // events are flushed from the queue before proceeding.

  // Step 22 of the Install algorithm.  Activate is executed after the
  // completion of this job.  The controlling client and skipWaiting checks are
  // performed in TryToActivate().
  mRegistration->TryToActivateAsync();
}

}  // namespace dom
}  // namespace mozilla
