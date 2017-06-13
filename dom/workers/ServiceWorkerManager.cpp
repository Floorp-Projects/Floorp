/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerManager.h"

#include "nsAutoPtr.h"
#include "nsIConsoleService.h"
#include "nsIDOMEventTarget.h"
#include "nsIDocument.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamLoader.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsINetworkInterceptController.h"
#include "nsIMutableArray.h"
#include "nsIScriptError.h"
#include "nsISimpleEnumerator.h"
#include "nsITimer.h"
#include "nsIUploadChannel2.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsDebug.h"
#include "nsISupportsPrimitives.h"
#include "nsIPermissionManager.h"

#include "jsapi.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/LoadContext.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/InternalHeaders.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/NotificationEvent.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/Unused.h"
#include "mozilla/EnumSet.h"

#include "nsContentPolicyUtils.h"
#include "nsContentSecurityManager.h"
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
#include "ServiceWorkerInfo.h"
#include "ServiceWorkerJobQueue.h"
#include "ServiceWorkerManagerChild.h"
#include "ServiceWorkerPrivate.h"
#include "ServiceWorkerRegisterJob.h"
#include "ServiceWorkerRegistrar.h"
#include "ServiceWorkerRegistration.h"
#include "ServiceWorkerScriptCache.h"
#include "ServiceWorkerEvents.h"
#include "ServiceWorkerUnregisterJob.h"
#include "ServiceWorkerUpdateJob.h"
#include "ServiceWorkerUpdaterChild.h"
#include "SharedWorker.h"
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

#define PURGE_DOMAIN_DATA "browser:purge-domain-data"
#define PURGE_SESSION_HISTORY "browser:purge-session-history"
#define CLEAR_ORIGIN_DATA "clear-origin-attributes-data"

static_assert(nsIHttpChannelInternal::CORS_MODE_SAME_ORIGIN == static_cast<uint32_t>(RequestMode::Same_origin),
              "RequestMode enumeration value should match Necko CORS mode value.");
static_assert(nsIHttpChannelInternal::CORS_MODE_NO_CORS == static_cast<uint32_t>(RequestMode::No_cors),
              "RequestMode enumeration value should match Necko CORS mode value.");
static_assert(nsIHttpChannelInternal::CORS_MODE_CORS == static_cast<uint32_t>(RequestMode::Cors),
              "RequestMode enumeration value should match Necko CORS mode value.");
static_assert(nsIHttpChannelInternal::CORS_MODE_NAVIGATE == static_cast<uint32_t>(RequestMode::Navigate),
              "RequestMode enumeration value should match Necko CORS mode value.");

static_assert(nsIHttpChannelInternal::REDIRECT_MODE_FOLLOW == static_cast<uint32_t>(RequestRedirect::Follow),
              "RequestRedirect enumeration value should make Necko Redirect mode value.");
static_assert(nsIHttpChannelInternal::REDIRECT_MODE_ERROR == static_cast<uint32_t>(RequestRedirect::Error),
              "RequestRedirect enumeration value should make Necko Redirect mode value.");
static_assert(nsIHttpChannelInternal::REDIRECT_MODE_MANUAL == static_cast<uint32_t>(RequestRedirect::Manual),
              "RequestRedirect enumeration value should make Necko Redirect mode value.");
static_assert(3 == static_cast<uint32_t>(RequestRedirect::EndGuard_),
              "RequestRedirect enumeration value should make Necko Redirect mode value.");

static_assert(nsIHttpChannelInternal::FETCH_CACHE_MODE_DEFAULT == static_cast<uint32_t>(RequestCache::Default),
             "RequestCache enumeration value should match Necko Cache mode value.");
static_assert(nsIHttpChannelInternal::FETCH_CACHE_MODE_NO_STORE == static_cast<uint32_t>(RequestCache::No_store),
             "RequestCache enumeration value should match Necko Cache mode value.");
static_assert(nsIHttpChannelInternal::FETCH_CACHE_MODE_RELOAD == static_cast<uint32_t>(RequestCache::Reload),
             "RequestCache enumeration value should match Necko Cache mode value.");
static_assert(nsIHttpChannelInternal::FETCH_CACHE_MODE_NO_CACHE == static_cast<uint32_t>(RequestCache::No_cache),
             "RequestCache enumeration value should match Necko Cache mode value.");
static_assert(nsIHttpChannelInternal::FETCH_CACHE_MODE_FORCE_CACHE == static_cast<uint32_t>(RequestCache::Force_cache),
             "RequestCache enumeration value should match Necko Cache mode value.");
static_assert(nsIHttpChannelInternal::FETCH_CACHE_MODE_ONLY_IF_CACHED == static_cast<uint32_t>(RequestCache::Only_if_cached),
             "RequestCache enumeration value should match Necko Cache mode value.");
static_assert(6 == static_cast<uint32_t>(RequestCache::EndGuard_),
             "RequestCache enumeration value should match Necko Cache mode value.");

static StaticRefPtr<ServiceWorkerManager> gInstance;

struct ServiceWorkerManager::RegistrationDataPerPrincipal final
{
  // Ordered list of scopes for glob matching.
  // Each entry is an absolute URL representing the scope.
  // Each value of the hash table is an array of an absolute URLs representing
  // the scopes.
  //
  // An array is used for now since the number of controlled scopes per
  // domain is expected to be relatively low. If that assumption was proved
  // wrong this should be replaced with a better structure to avoid the
  // memmoves associated with inserting stuff in the middle of the array.
  nsTArray<nsCString> mOrderedScopes;

  // Scope to registration.
  // The scope should be a fully qualified valid URL.
  nsRefPtrHashtable<nsCStringHashKey, ServiceWorkerRegistrationInfo> mInfos;

  // Maps scopes to job queues.
  nsRefPtrHashtable<nsCStringHashKey, ServiceWorkerJobQueue> mJobQueues;

  // Map scopes to scheduled update timers.
  nsInterfaceHashtable<nsCStringHashKey, nsITimer> mUpdateTimers;
};

namespace {

nsresult
PopulateRegistrationData(nsIPrincipal* aPrincipal,
                         const ServiceWorkerRegistrationInfo* aRegistration,
                         ServiceWorkerRegistrationData& aData)
{
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aRegistration);

  if (NS_WARN_IF(!BasePrincipal::Cast(aPrincipal)->IsCodebasePrincipal())) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = PrincipalToPrincipalInfo(aPrincipal, &aData.principal());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aData.scope() = aRegistration->mScope;

  RefPtr<ServiceWorkerInfo> newest = aRegistration->Newest();
  if (NS_WARN_IF(!newest)) {
    return NS_ERROR_FAILURE;
  }

  if (aRegistration->GetActive()) {
    aData.currentWorkerURL() = aRegistration->GetActive()->ScriptSpec();
    aData.cacheName() = aRegistration->GetActive()->CacheName();
    aData.currentWorkerHandlesFetch() = aRegistration->GetActive()->HandlesFetch();

    aData.currentWorkerInstalledTime() =
      aRegistration->GetActive()->GetInstalledTime();
    aData.currentWorkerActivatedTime() =
      aRegistration->GetActive()->GetActivatedTime();
  }

  aData.loadFlags() = aRegistration->GetLoadFlags();

  aData.lastUpdateTime() = aRegistration->GetLastUpdateTime();

  return NS_OK;
}

class TeardownRunnable final : public Runnable
{
public:
  explicit TeardownRunnable(ServiceWorkerManagerChild* aActor)
    : mActor(aActor)
  {
    MOZ_ASSERT(mActor);
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(mActor);
    mActor->SendShutdown();
    return NS_OK;
  }

private:
  ~TeardownRunnable() {}

  RefPtr<ServiceWorkerManagerChild> mActor;
};

} // namespace

//////////////////////////
// ServiceWorkerManager //
//////////////////////////

NS_IMPL_ADDREF(ServiceWorkerManager)
NS_IMPL_RELEASE(ServiceWorkerManager)

NS_INTERFACE_MAP_BEGIN(ServiceWorkerManager)
  NS_INTERFACE_MAP_ENTRY(nsIServiceWorkerManager)
  NS_INTERFACE_MAP_ENTRY(nsIIPCBackgroundChildCreateCallback)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIServiceWorkerManager)
NS_INTERFACE_MAP_END

ServiceWorkerManager::ServiceWorkerManager()
  : mActor(nullptr)
  , mShuttingDown(false)
{
}

ServiceWorkerManager::~ServiceWorkerManager()
{
  // The map will assert if it is not empty when destroyed.
  mRegistrationInfos.Clear();
  MOZ_ASSERT(!mActor);
}

void
ServiceWorkerManager::Init(ServiceWorkerRegistrar* aRegistrar)
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    DebugOnly<nsresult> rv;
    rv = obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false /* ownsWeak */);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  if (XRE_IsParentProcess()) {
    MOZ_DIAGNOSTIC_ASSERT(aRegistrar);

    nsTArray<ServiceWorkerRegistrationData> data;
    aRegistrar->GetRegistrations(data);
    LoadRegistrations(data);

    if (obs) {
      DebugOnly<nsresult> rv;
      rv = obs->AddObserver(this, PURGE_SESSION_HISTORY, false /* ownsWeak */);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      rv = obs->AddObserver(this, PURGE_DOMAIN_DATA, false /* ownsWeak */);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      rv = obs->AddObserver(this, CLEAR_ORIGIN_DATA, false /* ownsWeak */);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  if (!BackgroundChild::GetOrCreateForCurrentThread(this)) {
    // Make sure to do this last as our failure cleanup expects Init() to have
    // executed.
    ActorFailed();
  }
}

void
ServiceWorkerManager::MaybeStartShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mShuttingDown) {
    return;
  }

  mShuttingDown = true;

  for (auto it1 = mRegistrationInfos.Iter(); !it1.Done(); it1.Next()) {
    for (auto it2 = it1.UserData()->mUpdateTimers.Iter(); !it2.Done(); it2.Next()) {
      nsCOMPtr<nsITimer> timer = it2.UserData();
      timer->Cancel();
    }
    it1.UserData()->mUpdateTimers.Clear();

    for (auto it2 = it1.UserData()->mJobQueues.Iter(); !it2.Done(); it2.Next()) {
      RefPtr<ServiceWorkerJobQueue> queue = it2.UserData();
      queue->CancelAll();
    }
    it1.UserData()->mJobQueues.Clear();
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);

    if (XRE_IsParentProcess()) {
      obs->RemoveObserver(this, PURGE_SESSION_HISTORY);
      obs->RemoveObserver(this, PURGE_DOMAIN_DATA);
      obs->RemoveObserver(this, CLEAR_ORIGIN_DATA);
    }
  }

  mPendingOperations.Clear();

  if (!mActor) {
    return;
  }

  mActor->ManagerShuttingDown();

  RefPtr<TeardownRunnable> runnable = new TeardownRunnable(mActor);
  nsresult rv = NS_DispatchToMainThread(runnable);
  Unused << NS_WARN_IF(NS_FAILED(rv));
  mActor = nullptr;
}

class ServiceWorkerResolveWindowPromiseOnRegisterCallback final : public ServiceWorkerJob::Callback
{
  RefPtr<nsPIDOMWindowInner> mWindow;
  // The promise "returned" by the call to Update up to
  // navigator.serviceWorker.register().
  RefPtr<Promise> mPromise;

  ~ServiceWorkerResolveWindowPromiseOnRegisterCallback()
  {}

  virtual void
  JobFinished(ServiceWorkerJob* aJob, ErrorResult& aStatus) override
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aJob);

    if (aStatus.Failed()) {
      mPromise->MaybeReject(aStatus);
      return;
    }

    MOZ_ASSERT(aJob->GetType() == ServiceWorkerJob::Type::Register);
    RefPtr<ServiceWorkerRegisterJob> registerJob =
      static_cast<ServiceWorkerRegisterJob*>(aJob);
    RefPtr<ServiceWorkerRegistrationInfo> reg = registerJob->GetRegistration();

    RefPtr<ServiceWorkerRegistration> swr =
      mWindow->GetServiceWorkerRegistration(NS_ConvertUTF8toUTF16(reg->mScope));
    mPromise->MaybeResolve(swr);
  }

public:
  ServiceWorkerResolveWindowPromiseOnRegisterCallback(nsPIDOMWindowInner* aWindow,
                                                      Promise* aPromise)
    : mWindow(aWindow)
    , mPromise(aPromise)
  {}

  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerResolveWindowPromiseOnRegisterCallback, override)
};

namespace {

class PropagateSoftUpdateRunnable final : public Runnable
{
public:
  PropagateSoftUpdateRunnable(const OriginAttributes& aOriginAttributes,
                              const nsAString& aScope)
    : mOriginAttributes(aOriginAttributes)
    , mScope(aScope)
  {}

  NS_IMETHOD Run() override
  {
    AssertIsOnMainThread();

    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (swm) {
      swm->PropagateSoftUpdate(mOriginAttributes, mScope);
    }

    return NS_OK;
  }

private:
  ~PropagateSoftUpdateRunnable()
  {}

  const OriginAttributes mOriginAttributes;
  const nsString mScope;
};

class PropagateUnregisterRunnable final : public Runnable
{
public:
  PropagateUnregisterRunnable(nsIPrincipal* aPrincipal,
                              nsIServiceWorkerUnregisterCallback* aCallback,
                              const nsAString& aScope)
    : mPrincipal(aPrincipal)
    , mCallback(aCallback)
    , mScope(aScope)
  {
    MOZ_ASSERT(aPrincipal);
  }

  NS_IMETHOD Run() override
  {
    AssertIsOnMainThread();

    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (swm) {
      swm->PropagateUnregister(mPrincipal, mCallback, mScope);
    }

    return NS_OK;
  }

private:
  ~PropagateUnregisterRunnable()
  {}

  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIServiceWorkerUnregisterCallback> mCallback;
  const nsString mScope;
};

class RemoveRunnable final : public Runnable
{
public:
  explicit RemoveRunnable(const nsACString& aHost)
  {}

  NS_IMETHOD Run() override
  {
    AssertIsOnMainThread();

    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (swm) {
      swm->Remove(mHost);
    }

    return NS_OK;
  }

private:
  ~RemoveRunnable()
  {}

  const nsCString mHost;
};

class PropagateRemoveRunnable final : public Runnable
{
public:
  explicit PropagateRemoveRunnable(const nsACString& aHost)
  {}

  NS_IMETHOD Run() override
  {
    AssertIsOnMainThread();

    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (swm) {
      swm->PropagateRemove(mHost);
    }

    return NS_OK;
  }

private:
  ~PropagateRemoveRunnable()
  {}

  const nsCString mHost;
};

class PropagateRemoveAllRunnable final : public Runnable
{
public:
  PropagateRemoveAllRunnable()
  {}

  NS_IMETHOD Run() override
  {
    AssertIsOnMainThread();

    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (swm) {
      swm->PropagateRemoveAll();
    }

    return NS_OK;
  }

private:
  ~PropagateRemoveAllRunnable()
  {}
};

class PromiseResolverCallback final : public ServiceWorkerUpdateFinishCallback
{
public:
  PromiseResolverCallback(ServiceWorkerUpdateFinishCallback* aCallback,
                          GenericPromise::Private* aPromise)
    : mCallback(aCallback)
    , mPromise(aPromise)
  {
    MOZ_DIAGNOSTIC_ASSERT(mPromise);
  }

  void UpdateSucceeded(ServiceWorkerRegistrationInfo* aInfo) override
  {
    MOZ_DIAGNOSTIC_ASSERT(mPromise);

    if (mCallback) {
      mCallback->UpdateSucceeded(aInfo);
    }

    MaybeResolve();
  }

  void UpdateFailed(ErrorResult& aStatus) override
  {
    MOZ_DIAGNOSTIC_ASSERT(mPromise);

    if (mCallback) {
      mCallback->UpdateFailed(aStatus);
    }

    MaybeResolve();
  }

private:
  ~PromiseResolverCallback()
  {
    MaybeResolve();
  }

  void
  MaybeResolve()
  {
    if (mPromise) {
      mPromise->Resolve(true, __func__);
      mPromise = nullptr;
    }
  }

  RefPtr<ServiceWorkerUpdateFinishCallback> mCallback;
  RefPtr<GenericPromise::Private> mPromise;
};

// This runnable is used for 2 different tasks:
// - to postpone the SoftUpdate() until the IPC SWM actor is created
//   (aInternalMethod == false)
// - to call the 'real' SoftUpdate when the ServiceWorkerUpdaterChild is
//   notified by the parent (aInternalMethod == true)
class SoftUpdateRunnable final : public CancelableRunnable
{
public:
  SoftUpdateRunnable(const OriginAttributes& aOriginAttributes,
                     const nsACString& aScope, bool aInternalMethod,
                     GenericPromise::Private* aPromise)
    : mAttrs(aOriginAttributes)
    , mScope(aScope)
    , mInternalMethod(aInternalMethod)
    , mPromise(aPromise)
  {}

  NS_IMETHOD Run() override
  {
    AssertIsOnMainThread();

    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (!swm) {
      return NS_ERROR_FAILURE;
    }

    if (mInternalMethod) {
      RefPtr<PromiseResolverCallback> callback =
        new PromiseResolverCallback(nullptr, mPromise);
      mPromise = nullptr;

      swm->SoftUpdateInternal(mAttrs, mScope, callback);
    } else {
      swm->SoftUpdate(mAttrs, mScope);
    }

    return NS_OK;
  }

  nsresult
  Cancel() override
  {
    mPromise = nullptr;
    return NS_OK;
  }

private:
  ~SoftUpdateRunnable()
  {
    if (mPromise) {
      mPromise->Resolve(true, __func__);
    }
  }

  const OriginAttributes mAttrs;
  const nsCString mScope;
  bool mInternalMethod;

  RefPtr<GenericPromise::Private> mPromise;
};

// This runnable is used for 3 different tasks:
// - to postpone the Update() until the IPC SWM actor is created
//   (aType == ePostpone)
// - to call the 'real' Update when the ServiceWorkerUpdaterChild is
//   notified by the parent (aType == eSuccess)
// - an error must be propagated (aType == eFailure)
class UpdateRunnable final : public CancelableRunnable
{
public:
  enum Type {
    ePostpone,
    eSuccess,
    eFailure,
  };

  UpdateRunnable(nsIPrincipal* aPrincipal,
                 const nsACString& aScope,
                 ServiceWorkerUpdateFinishCallback* aCallback,
                 Type aType,
                 GenericPromise::Private* aPromise)
    : mPrincipal(aPrincipal)
    , mScope(aScope)
    , mCallback(aCallback)
    , mType(aType)
    , mPromise(aPromise)
  {}

  NS_IMETHOD Run() override
  {
    AssertIsOnMainThread();

    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (!swm) {
      return NS_ERROR_FAILURE;
    }

    if (mType == ePostpone) {
      swm->Update(mPrincipal, mScope, mCallback);
      return NS_OK;
    }

    MOZ_ASSERT(mPromise);

    RefPtr<PromiseResolverCallback> callback =
      new PromiseResolverCallback(mCallback, mPromise);
    mPromise = nullptr;

    if (mType == eSuccess) {
      swm->UpdateInternal(mPrincipal, mScope, callback);
      return NS_OK;
    }

    ErrorResult error(NS_ERROR_DOM_ABORT_ERR);
    callback->UpdateFailed(error);
    return NS_OK;
  }

  nsresult
  Cancel() override
  {
    mPromise = nullptr;
    return NS_OK;
  }

private:
  ~UpdateRunnable()
  {
    if (mPromise) {
      mPromise->Resolve(true, __func__);
    }
  }

  nsCOMPtr<nsIPrincipal> mPrincipal;
  const nsCString mScope;
  RefPtr<ServiceWorkerUpdateFinishCallback> mCallback;
  Type mType;

  RefPtr<GenericPromise::Private> mPromise;
};

class ResolvePromiseRunnable final : public CancelableRunnable
{
public:
  explicit ResolvePromiseRunnable(GenericPromise::Private* aPromise)
    : mPromise(aPromise)
  {}

  NS_IMETHOD
  Run() override
  {
    MaybeResolve();
    return NS_OK;
  }

  nsresult
  Cancel() override
  {
    mPromise = nullptr;
    return NS_OK;
  }

private:
  ~ResolvePromiseRunnable()
  {
    MaybeResolve();
  }

  void
  MaybeResolve()
  {
    if (mPromise) {
      mPromise->Resolve(true, __func__);
      mPromise = nullptr;
    }
  }

  RefPtr<GenericPromise::Private> mPromise;
};

} // namespace

// This function implements parts of the step 3 of the following algorithm:
// https://w3c.github.io/webappsec/specs/powerfulfeatures/#settings-secure
static bool
IsFromAuthenticatedOrigin(nsIDocument* aDoc)
{
  MOZ_ASSERT(aDoc);
  nsCOMPtr<nsIDocument> doc(aDoc);
  nsCOMPtr<nsIContentSecurityManager> csm = do_GetService(NS_CONTENTSECURITYMANAGER_CONTRACTID);
  if (NS_WARN_IF(!csm)) {
    return false;
  }

  while (doc && !nsContentUtils::IsChromeDoc(doc)) {
    bool trustworthyOrigin = false;

    // The origin of the document may be different from the document URI
    // itself.  Check the principal, not the document URI itself.
    nsCOMPtr<nsIPrincipal> documentPrincipal = doc->NodePrincipal();

    // The check for IsChromeDoc() above should mean we never see a system
    // principal inside the loop.
    MOZ_ASSERT(!nsContentUtils::IsSystemPrincipal(documentPrincipal));

    csm->IsOriginPotentiallyTrustworthy(documentPrincipal, &trustworthyOrigin);
    if (!trustworthyOrigin) {
      return false;
    }

    doc = doc->GetParentDocument();
  }
  return true;
}

// If we return an error code here, the ServiceWorkerContainer will
// automatically reject the Promise.
NS_IMETHODIMP
ServiceWorkerManager::Register(mozIDOMWindow* aWindow,
                               nsIURI* aScopeURI,
                               nsIURI* aScriptURI,
                               nsLoadFlags aLoadFlags,
                               nsISupports** aPromise)
{
  AssertIsOnMainThread();

  if (NS_WARN_IF(!aWindow)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  auto* window = nsPIDOMWindowInner::From(aWindow);
  nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
  if (!doc) {
    return NS_ERROR_FAILURE;
  }

  // Don't allow service workers to register when the *document* is chrome.
  if (NS_WARN_IF(nsContentUtils::IsSystemPrincipal(doc->NodePrincipal()))) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsPIDOMWindowOuter> outerWindow = window->GetOuterWindow();
  bool serviceWorkersTestingEnabled =
    outerWindow->GetServiceWorkersTestingEnabled();

  bool authenticatedOrigin;
  if (Preferences::GetBool("dom.serviceWorkers.testing.enabled") ||
      serviceWorkersTestingEnabled) {
    authenticatedOrigin = true;
  } else {
    authenticatedOrigin = IsFromAuthenticatedOrigin(doc);
  }

  if (!authenticatedOrigin) {
    NS_WARNING("ServiceWorker registration from insecure websites is not allowed.");
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // Data URLs are not allowed.
  nsCOMPtr<nsIPrincipal> documentPrincipal = doc->NodePrincipal();

  nsresult rv = documentPrincipal->CheckMayLoad(aScriptURI, true /* report */,
                                                false /* allowIfInheritsPrincipal */);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // Check content policy.
  int16_t decision = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER,
                                 aScriptURI,
                                 documentPrincipal,
                                 doc,
                                 EmptyCString(),
                                 nullptr,
                                 &decision);
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_WARN_IF(decision != nsIContentPolicy::ACCEPT)) {
    return NS_ERROR_CONTENT_BLOCKED;
  }


  rv = documentPrincipal->CheckMayLoad(aScopeURI, true /* report */,
                                       false /* allowIfInheritsPrinciple */);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // The IsOriginPotentiallyTrustworthy() check allows file:// and possibly other
  // URI schemes.  We need to explicitly only allows http and https schemes.
  // Note, we just use the aScriptURI here for the check since its already
  // been verified as same origin with the document principal.  This also
  // is a good block against accidentally allowing blob: script URIs which
  // might inherit the origin.
  bool isHttp = false;
  bool isHttps = false;
  aScriptURI->SchemeIs("http", &isHttp);
  aScriptURI->SchemeIs("https", &isHttps);
  if (NS_WARN_IF(!isHttp && !isHttps)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCString cleanedScope;
  rv = aScopeURI->GetSpecIgnoringRef(cleanedScope);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString spec;
  rv = aScriptURI->GetSpecIgnoringRef(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIGlobalObject> sgo = do_QueryInterface(window);
  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(sgo, result);
  if (result.Failed()) {
    return result.StealNSResult();
  }

  nsAutoCString scopeKey;
  rv = PrincipalToScopeKey(documentPrincipal, scopeKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AddRegisteringDocument(cleanedScope, doc);

  RefPtr<ServiceWorkerJobQueue> queue = GetOrCreateJobQueue(scopeKey,
                                                            cleanedScope);

  RefPtr<ServiceWorkerResolveWindowPromiseOnRegisterCallback> cb =
    new ServiceWorkerResolveWindowPromiseOnRegisterCallback(window, promise);

  nsCOMPtr<nsILoadGroup> docLoadGroup = doc->GetDocumentLoadGroup();
  RefPtr<WorkerLoadInfo::InterfaceRequestor> ir =
    new WorkerLoadInfo::InterfaceRequestor(documentPrincipal, docLoadGroup);
  ir->MaybeAddTabChild(docLoadGroup);

  // Create a load group that is separate from, yet related to, the document's load group.
  // This allows checks for interfaces like nsILoadContext to yield the values used by the
  // the document, yet will not cancel the update job if the document's load group is cancelled.
  nsCOMPtr<nsILoadGroup> loadGroup = do_CreateInstance(NS_LOADGROUP_CONTRACTID);
  MOZ_ALWAYS_SUCCEEDS(loadGroup->SetNotificationCallbacks(ir));

  RefPtr<ServiceWorkerRegisterJob> job =
    new ServiceWorkerRegisterJob(documentPrincipal, cleanedScope, spec,
                                 loadGroup, aLoadFlags);
  job->AppendResultCallback(cb);
  queue->ScheduleJob(job);

  AssertIsOnMainThread();
  Telemetry::Accumulate(Telemetry::SERVICE_WORKER_REGISTRATIONS, 1);

  ContentChild* contentChild = ContentChild::GetSingleton();
  if (contentChild &&
      contentChild->GetRemoteType().EqualsLiteral(FILE_REMOTE_TYPE)) {
    nsString message(NS_LITERAL_STRING("ServiceWorker registered by document "
                                       "embedded in a file:/// URI.  This may "
                                       "result in unexpected behavior."));
    ReportToAllClients(cleanedScope, message, EmptyString(),
                       EmptyString(), 0, 0, nsIScriptError::warningFlag);
    Telemetry::Accumulate(Telemetry::FILE_EMBEDDED_SERVICEWORKERS, 1);
  }

  promise.forget(aPromise);
  return NS_OK;
}

void
ServiceWorkerManager::AppendPendingOperation(nsIRunnable* aRunnable)
{
  MOZ_ASSERT(!mActor);
  MOZ_ASSERT(aRunnable);

  if (!mShuttingDown) {
    mPendingOperations.AppendElement(aRunnable);
  }
}

/*
 * Implements the async aspects of the getRegistrations algorithm.
 */
class GetRegistrationsRunnable final : public Runnable
{
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<Promise> mPromise;
public:
  GetRegistrationsRunnable(nsPIDOMWindowInner* aWindow, Promise* aPromise)
    : mWindow(aWindow), mPromise(aPromise)
  {}

  NS_IMETHOD
  Run() override
  {
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (!swm) {
      mPromise->MaybeReject(NS_ERROR_UNEXPECTED);
      return NS_OK;
    }

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

    nsTArray<RefPtr<ServiceWorkerRegistration>> array;

    if (NS_WARN_IF(!BasePrincipal::Cast(principal)->IsCodebasePrincipal())) {
      return NS_OK;
    }

    nsAutoCString scopeKey;
    nsresult rv = swm->PrincipalToScopeKey(principal, scopeKey);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    ServiceWorkerManager::RegistrationDataPerPrincipal* data;
    if (!swm->mRegistrationInfos.Get(scopeKey, &data)) {
      mPromise->MaybeResolve(array);
      return NS_OK;
    }

    for (uint32_t i = 0; i < data->mOrderedScopes.Length(); ++i) {
      RefPtr<ServiceWorkerRegistrationInfo> info =
        data->mInfos.GetWeak(data->mOrderedScopes[i]);
      if (info->mPendingUninstall) {
        continue;
      }

      NS_ConvertUTF8toUTF16 scope(data->mOrderedScopes[i]);

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

      RefPtr<ServiceWorkerRegistration> swr =
        mWindow->GetServiceWorkerRegistration(scope);

      array.AppendElement(swr);
    }

    mPromise->MaybeResolve(array);
    return NS_OK;
  }
};

// If we return an error code here, the ServiceWorkerContainer will
// automatically reject the Promise.
NS_IMETHODIMP
ServiceWorkerManager::GetRegistrations(mozIDOMWindow* aWindow,
                                       nsISupports** aPromise)
{
  AssertIsOnMainThread();

  if (NS_WARN_IF(!aWindow)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  auto* window = nsPIDOMWindowInner::From(aWindow);
  nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  // Don't allow service workers to register when the *document* is chrome for
  // now.
  MOZ_ASSERT(!nsContentUtils::IsSystemPrincipal(doc->NodePrincipal()));

  nsCOMPtr<nsIGlobalObject> sgo = do_QueryInterface(window);
  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(sgo, result);
  if (result.Failed()) {
    return result.StealNSResult();
  }

  nsCOMPtr<nsIRunnable> runnable =
    new GetRegistrationsRunnable(window, promise);
  promise.forget(aPromise);
  return NS_DispatchToCurrentThread(runnable);
}

/*
 * Implements the async aspects of the getRegistration algorithm.
 */
class GetRegistrationRunnable final : public Runnable
{
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<Promise> mPromise;
  nsString mDocumentURL;

public:
  GetRegistrationRunnable(nsPIDOMWindowInner* aWindow, Promise* aPromise,
                          const nsAString& aDocumentURL)
    : mWindow(aWindow), mPromise(aPromise), mDocumentURL(aDocumentURL)
  {}

  NS_IMETHOD
  Run() override
  {
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (!swm) {
      mPromise->MaybeReject(NS_ERROR_UNEXPECTED);
      return NS_OK;
    }

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

    RefPtr<ServiceWorkerRegistrationInfo> registration =
      swm->GetServiceWorkerRegistrationInfo(principal, uri);

    if (!registration) {
      mPromise->MaybeResolveWithUndefined();
      return NS_OK;
    }

    NS_ConvertUTF8toUTF16 scope(registration->mScope);
    RefPtr<ServiceWorkerRegistration> swr =
      mWindow->GetServiceWorkerRegistration(scope);
    mPromise->MaybeResolve(swr);

    return NS_OK;
  }
};

// If we return an error code here, the ServiceWorkerContainer will
// automatically reject the Promise.
NS_IMETHODIMP
ServiceWorkerManager::GetRegistration(mozIDOMWindow* aWindow,
                                      const nsAString& aDocumentURL,
                                      nsISupports** aPromise)
{
  AssertIsOnMainThread();

  if (NS_WARN_IF(!aWindow)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  auto* window = nsPIDOMWindowInner::From(aWindow);
  nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  // Don't allow service workers to register when the *document* is chrome for
  // now.
  MOZ_ASSERT(!nsContentUtils::IsSystemPrincipal(doc->NodePrincipal()));

  nsCOMPtr<nsIGlobalObject> sgo = do_QueryInterface(window);
  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(sgo, result);
  if (result.Failed()) {
    return result.StealNSResult();
  }

  nsCOMPtr<nsIRunnable> runnable =
    new GetRegistrationRunnable(window, promise, aDocumentURL);
  promise.forget(aPromise);
  return NS_DispatchToCurrentThread(runnable);
}

class GetReadyPromiseRunnable final : public Runnable
{
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<Promise> mPromise;

public:
  GetReadyPromiseRunnable(nsPIDOMWindowInner* aWindow, Promise* aPromise)
    : mWindow(aWindow), mPromise(aPromise)
  {}

  NS_IMETHOD
  Run() override
  {
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (!swm) {
      mPromise->MaybeReject(NS_ERROR_UNEXPECTED);
      return NS_OK;
    }

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

NS_IMETHODIMP
ServiceWorkerManager::SendPushEvent(const nsACString& aOriginAttributes,
                                    const nsACString& aScope,
                                    uint32_t aDataLength,
                                    uint8_t* aDataBytes,
                                    uint8_t optional_argc)
{
  if (optional_argc == 2) {
    nsTArray<uint8_t> data;
    if (!data.InsertElementsAt(0, aDataBytes, aDataLength, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    return SendPushEvent(aOriginAttributes, aScope, EmptyString(), Some(data));
  }
  MOZ_ASSERT(optional_argc == 0);
  return SendPushEvent(aOriginAttributes, aScope, EmptyString(), Nothing());
}

nsresult
ServiceWorkerManager::SendPushEvent(const nsACString& aOriginAttributes,
                                    const nsACString& aScope,
                                    const nsAString& aMessageId,
                                    const Maybe<nsTArray<uint8_t>>& aData)
{
  OriginAttributes attrs;
  if (!attrs.PopulateFromSuffix(aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  ServiceWorkerInfo* serviceWorker = GetActiveWorkerInfoForScope(attrs, aScope);
  if (NS_WARN_IF(!serviceWorker)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<ServiceWorkerRegistrationInfo> registration =
    GetRegistration(serviceWorker->Principal(), aScope);
  MOZ_DIAGNOSTIC_ASSERT(registration);

  return serviceWorker->WorkerPrivate()->SendPushEvent(aMessageId, aData,
                                                       registration);
}

NS_IMETHODIMP
ServiceWorkerManager::SendPushSubscriptionChangeEvent(const nsACString& aOriginAttributes,
                                                      const nsACString& aScope)
{
  OriginAttributes attrs;
  if (!attrs.PopulateFromSuffix(aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  ServiceWorkerInfo* info = GetActiveWorkerInfoForScope(attrs, aScope);
  if (!info) {
    return NS_ERROR_FAILURE;
  }
  return info->WorkerPrivate()->SendPushSubscriptionChangeEvent();
}

nsresult
ServiceWorkerManager::SendNotificationEvent(const nsAString& aEventName,
                                            const nsACString& aOriginSuffix,
                                            const nsACString& aScope,
                                            const nsAString& aID,
                                            const nsAString& aTitle,
                                            const nsAString& aDir,
                                            const nsAString& aLang,
                                            const nsAString& aBody,
                                            const nsAString& aTag,
                                            const nsAString& aIcon,
                                            const nsAString& aData,
                                            const nsAString& aBehavior)
{
  OriginAttributes attrs;
  if (!attrs.PopulateFromSuffix(aOriginSuffix)) {
    return NS_ERROR_INVALID_ARG;
  }

  ServiceWorkerInfo* info = GetActiveWorkerInfoForScope(attrs, aScope);
  if (!info) {
    return NS_ERROR_FAILURE;
  }

  ServiceWorkerPrivate* workerPrivate = info->WorkerPrivate();
  return workerPrivate->SendNotificationEvent(aEventName, aID, aTitle, aDir,
                                              aLang, aBody, aTag,
                                              aIcon, aData, aBehavior,
                                              NS_ConvertUTF8toUTF16(aScope));
}

NS_IMETHODIMP
ServiceWorkerManager::SendNotificationClickEvent(const nsACString& aOriginSuffix,
                                                 const nsACString& aScope,
                                                 const nsAString& aID,
                                                 const nsAString& aTitle,
                                                 const nsAString& aDir,
                                                 const nsAString& aLang,
                                                 const nsAString& aBody,
                                                 const nsAString& aTag,
                                                 const nsAString& aIcon,
                                                 const nsAString& aData,
                                                 const nsAString& aBehavior)
{
  return SendNotificationEvent(NS_LITERAL_STRING(NOTIFICATION_CLICK_EVENT_NAME),
                               aOriginSuffix, aScope, aID, aTitle, aDir, aLang,
                               aBody, aTag, aIcon, aData, aBehavior);
}

NS_IMETHODIMP
ServiceWorkerManager::SendNotificationCloseEvent(const nsACString& aOriginSuffix,
                                                 const nsACString& aScope,
                                                 const nsAString& aID,
                                                 const nsAString& aTitle,
                                                 const nsAString& aDir,
                                                 const nsAString& aLang,
                                                 const nsAString& aBody,
                                                 const nsAString& aTag,
                                                 const nsAString& aIcon,
                                                 const nsAString& aData,
                                                 const nsAString& aBehavior)
{
  return SendNotificationEvent(NS_LITERAL_STRING(NOTIFICATION_CLOSE_EVENT_NAME),
                               aOriginSuffix, aScope, aID, aTitle, aDir, aLang,
                               aBody, aTag, aIcon, aData, aBehavior);
}

NS_IMETHODIMP
ServiceWorkerManager::GetReadyPromise(mozIDOMWindow* aWindow,
                                      nsISupports** aPromise)
{
  AssertIsOnMainThread();

  if (NS_WARN_IF(!aWindow)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  auto* window = nsPIDOMWindowInner::From(aWindow);

  nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return NS_ERROR_FAILURE;
  }

  // Don't allow service workers to register when the *document* is chrome for
  // now.
  MOZ_ASSERT(!nsContentUtils::IsSystemPrincipal(doc->NodePrincipal()));

  MOZ_ASSERT(!mPendingReadyPromises.Contains(window));

  nsCOMPtr<nsIGlobalObject> sgo = do_QueryInterface(window);
  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(sgo, result);
  if (result.Failed()) {
    return result.StealNSResult();
  }

  nsCOMPtr<nsIRunnable> runnable =
    new GetReadyPromiseRunnable(window, promise);
  promise.forget(aPromise);
  return NS_DispatchToCurrentThread(runnable);
}

NS_IMETHODIMP
ServiceWorkerManager::RemoveReadyPromise(mozIDOMWindow* aWindow)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWindow);

  if (!aWindow) {
    return NS_ERROR_FAILURE;
  }

  mPendingReadyPromises.Remove(aWindow);
  return NS_OK;
}

void
ServiceWorkerManager::StorePendingReadyPromise(nsPIDOMWindowInner* aWindow,
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
  for (auto iter = mPendingReadyPromises.Iter(); !iter.Done(); iter.Next()) {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(iter.Key());
    MOZ_ASSERT(window);

    nsAutoPtr<PendingReadyPromise>& pendingReadyPromise = iter.Data();
    if (CheckReadyPromise(window, pendingReadyPromise->mURI,
                          pendingReadyPromise->mPromise)) {
      iter.Remove();
    }
  }
}

bool
ServiceWorkerManager::CheckReadyPromise(nsPIDOMWindowInner* aWindow,
                                        nsIURI* aURI, Promise* aPromise)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aURI);

  nsCOMPtr<nsIDocument> doc = aWindow->GetExtantDoc();
  MOZ_ASSERT(doc);

  nsCOMPtr<nsIPrincipal> principal = doc->NodePrincipal();
  MOZ_ASSERT(principal);

  RefPtr<ServiceWorkerRegistrationInfo> registration =
    GetServiceWorkerRegistrationInfo(principal, aURI);

  if (registration && registration->GetActive()) {
    NS_ConvertUTF8toUTF16 scope(registration->mScope);
    RefPtr<ServiceWorkerRegistration> swr =
      aWindow->GetServiceWorkerRegistration(scope);
    aPromise->MaybeResolve(swr);
    return true;
  }

  return false;
}

ServiceWorkerInfo*
ServiceWorkerManager::GetActiveWorkerInfoForScope(const OriginAttributes& aOriginAttributes,
                                                  const nsACString& aScope)
{
  AssertIsOnMainThread();

  nsCOMPtr<nsIURI> scopeURI;
  nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), aScope, nullptr, nullptr);
  if (NS_FAILED(rv)) {
    return nullptr;
  }
  nsCOMPtr<nsIPrincipal> principal =
    BasePrincipal::CreateCodebasePrincipal(scopeURI, aOriginAttributes);
  RefPtr<ServiceWorkerRegistrationInfo> registration =
    GetServiceWorkerRegistrationInfo(principal, scopeURI);
  if (!registration) {
    return nullptr;
  }

  return registration->GetActive();
}

ServiceWorkerInfo*
ServiceWorkerManager::GetActiveWorkerInfoForDocument(nsIDocument* aDocument)
{
  AssertIsOnMainThread();

  RefPtr<ServiceWorkerRegistrationInfo> registration;
  GetDocumentRegistration(aDocument, getter_AddRefs(registration));

  if (!registration) {
    return nullptr;
  }

  return registration->GetActive();
}

namespace {

class UnregisterJobCallback final : public ServiceWorkerJob::Callback
{
  nsCOMPtr<nsIServiceWorkerUnregisterCallback> mCallback;

  ~UnregisterJobCallback()
  {
  }

public:
  explicit UnregisterJobCallback(nsIServiceWorkerUnregisterCallback* aCallback)
    : mCallback(aCallback)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mCallback);
  }

  void
  JobFinished(ServiceWorkerJob* aJob, ErrorResult& aStatus)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aJob);

    if (aStatus.Failed()) {
      mCallback->UnregisterFailed();
      return;
    }

    MOZ_ASSERT(aJob->GetType() == ServiceWorkerJob::Type::Unregister);
    RefPtr<ServiceWorkerUnregisterJob> unregisterJob =
      static_cast<ServiceWorkerUnregisterJob*>(aJob);
    mCallback->UnregisterSucceeded(unregisterJob->GetResult());
  }

  NS_INLINE_DECL_REFCOUNTING(UnregisterJobCallback)
};

} // anonymous namespace

NS_IMETHODIMP
ServiceWorkerManager::Unregister(nsIPrincipal* aPrincipal,
                                 nsIServiceWorkerUnregisterCallback* aCallback,
                                 const nsAString& aScope)
{
  AssertIsOnMainThread();

  if (!aPrincipal) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;

// This is not accessible by content, and callers should always ensure scope is
// a correct URI, so this is wrapped in DEBUG
#ifdef DEBUG
  nsCOMPtr<nsIURI> scopeURI;
  rv = NS_NewURI(getter_AddRefs(scopeURI), aScope, nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }
#endif

  nsAutoCString scopeKey;
  rv = PrincipalToScopeKey(aPrincipal, scopeKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  NS_ConvertUTF16toUTF8 scope(aScope);
  RefPtr<ServiceWorkerJobQueue> queue = GetOrCreateJobQueue(scopeKey, scope);

  RefPtr<ServiceWorkerUnregisterJob> job =
    new ServiceWorkerUnregisterJob(aPrincipal, scope, true /* send to parent */);

  if (aCallback) {
    RefPtr<UnregisterJobCallback> cb = new UnregisterJobCallback(aCallback);
    job->AppendResultCallback(cb);
  }

  queue->ScheduleJob(job);
  return NS_OK;
}

nsresult
ServiceWorkerManager::NotifyUnregister(nsIPrincipal* aPrincipal,
                                       const nsAString& aScope)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);

  nsresult rv;

// This is not accessible by content, and callers should always ensure scope is
// a correct URI, so this is wrapped in DEBUG
#ifdef DEBUG
  nsCOMPtr<nsIURI> scopeURI;
  rv = NS_NewURI(getter_AddRefs(scopeURI), aScope, nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
#endif

  nsAutoCString scopeKey;
  rv = PrincipalToScopeKey(aPrincipal, scopeKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  NS_ConvertUTF16toUTF8 scope(aScope);
  RefPtr<ServiceWorkerJobQueue> queue = GetOrCreateJobQueue(scopeKey, scope);

  RefPtr<ServiceWorkerUnregisterJob> job =
    new ServiceWorkerUnregisterJob(aPrincipal, scope,
                                    false /* send to parent */);

  queue->ScheduleJob(job);
  return NS_OK;
}

void
ServiceWorkerManager::WorkerIsIdle(ServiceWorkerInfo* aWorker)
{
  AssertIsOnMainThread();
  MOZ_DIAGNOSTIC_ASSERT(aWorker);

  RefPtr<ServiceWorkerRegistrationInfo> reg =
    GetRegistration(aWorker->Principal(), aWorker->Scope());
  if (!reg) {
    return;
  }

  if (reg->GetActive() != aWorker) {
    return;
  }

  if (!reg->IsControllingDocuments() && reg->mPendingUninstall) {
    RemoveRegistration(reg);
    return;
  }

  reg->TryToActivateAsync();
}

already_AddRefed<ServiceWorkerJobQueue>
ServiceWorkerManager::GetOrCreateJobQueue(const nsACString& aKey,
                                          const nsACString& aScope)
{
  MOZ_ASSERT(!aKey.IsEmpty());
  ServiceWorkerManager::RegistrationDataPerPrincipal* data;
  // XXX we could use LookupForAdd here to avoid a hashtable lookup, except that
  // leads to a false positive assertion, see bug 1370674 comment 7.
  if (!mRegistrationInfos.Get(aKey, &data)) {
    data = new RegistrationDataPerPrincipal();
    mRegistrationInfos.Put(aKey, data);
  }

  RefPtr<ServiceWorkerJobQueue> queue =
    data->mJobQueues.LookupForAdd(aScope).OrInsert(
      []() { return new ServiceWorkerJobQueue(); });

  return queue.forget();
}

/* static */
already_AddRefed<ServiceWorkerManager>
ServiceWorkerManager::GetInstance()
{
  // Note: We don't simply check gInstance for null-ness here, since otherwise
  // this can resurrect the ServiceWorkerManager pretty late during shutdown.
  static bool firstTime = true;
  if (firstTime) {
    RefPtr<ServiceWorkerRegistrar> swr;

    // Don't create the ServiceWorkerManager until the ServiceWorkerRegistrar is
    // initialized.
    if (XRE_IsParentProcess()) {
      swr = ServiceWorkerRegistrar::Get();
      if (!swr) {
        return nullptr;
      }
    }

    firstTime = false;

    AssertIsOnMainThread();

    gInstance = new ServiceWorkerManager();
    gInstance->Init(swr);
    ClearOnShutdown(&gInstance);
  }
  RefPtr<ServiceWorkerManager> copy = gInstance.get();
  return copy.forget();
}

void
ServiceWorkerManager::FinishFetch(ServiceWorkerRegistrationInfo* aRegistration)
{
}

void
ServiceWorkerManager::ReportToAllClients(const nsCString& aScope,
                                         const nsString& aMessage,
                                         const nsString& aFilename,
                                         const nsString& aLine,
                                         uint32_t aLineNumber,
                                         uint32_t aColumnNumber,
                                         uint32_t aFlags)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv;

  if (!aFilename.IsEmpty()) {
    rv = NS_NewURI(getter_AddRefs(uri), aFilename);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

  AutoTArray<uint64_t, 16> windows;

  // Report errors to every controlled document.
  for (auto iter = mControlledDocuments.Iter(); !iter.Done(); iter.Next()) {
    ServiceWorkerRegistrationInfo* reg = iter.UserData();
    MOZ_ASSERT(reg);
    if (!reg->mScope.Equals(aScope)) {
      continue;
    }

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(iter.Key());
    if (!doc || !doc->IsCurrentActiveDocument() || !doc->GetWindow()) {
      continue;
    }

    windows.AppendElement(doc->InnerWindowID());

    nsContentUtils::ReportToConsoleNonLocalized(aMessage,
                                                aFlags,
                                                NS_LITERAL_CSTRING("Service Workers"),
                                                doc,
                                                uri,
                                                aLine,
                                                aLineNumber,
                                                aColumnNumber,
                                                nsContentUtils::eOMIT_LOCATION);
  }

  // Report to any documents that have called .register() for this scope.  They
  // may not be controlled, but will still want to see error reports.
  WeakDocumentList* regList = mRegisteringDocuments.Get(aScope);
  if (regList) {
    for (int32_t i = regList->Length() - 1; i >= 0; --i) {
      nsCOMPtr<nsIDocument> doc = do_QueryReferent(regList->ElementAt(i));
      if (!doc) {
        regList->RemoveElementAt(i);
        continue;
      }

      if (!doc->IsCurrentActiveDocument()) {
        continue;
      }

      uint64_t innerWindowId = doc->InnerWindowID();
      if (windows.Contains(innerWindowId)) {
        continue;
      }

      windows.AppendElement(innerWindowId);

      nsContentUtils::ReportToConsoleNonLocalized(aMessage,
                                                  aFlags,
                                                  NS_LITERAL_CSTRING("Service Workers"),
                                                  doc,
                                                  uri,
                                                  aLine,
                                                  aLineNumber,
                                                  aColumnNumber,
                                                  nsContentUtils::eOMIT_LOCATION);
    }

    if (regList->IsEmpty()) {
      regList = nullptr;
      nsAutoPtr<WeakDocumentList> doomed;
      mRegisteringDocuments.RemoveAndForget(aScope, doomed);
    }
  }

  InterceptionList* intList = mNavigationInterceptions.Get(aScope);
  if (intList) {
    nsIConsoleService* consoleService = nullptr;
    for (uint32_t i = 0; i < intList->Length(); ++i) {
      nsCOMPtr<nsIInterceptedChannel> channel = intList->ElementAt(i);

      nsCOMPtr<nsIChannel> inner;
      rv = channel->GetChannel(getter_AddRefs(inner));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }

      uint64_t innerWindowId = nsContentUtils::GetInnerWindowID(inner);
      if (innerWindowId == 0 || windows.Contains(innerWindowId)) {
        continue;
      }

      windows.AppendElement(innerWindowId);

      // Unfortunately the nsContentUtils helpers don't provide a convenient
      // way to log to a window ID without a document.  Use console service
      // directly.
      nsCOMPtr<nsIScriptError> errorObject =
        do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return;
      }

      rv = errorObject->InitWithWindowID(aMessage,
                                         aFilename,
                                         aLine,
                                         aLineNumber,
                                         aColumnNumber,
                                         aFlags,
                                         NS_LITERAL_CSTRING("Service Workers"),
                                         innerWindowId);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return;
      }

      if (!consoleService) {
        rv = CallGetService(NS_CONSOLESERVICE_CONTRACTID, &consoleService);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return;
        }
      }

      consoleService->LogMessage(errorObject);
    }
  }

  // If there are no documents to report to, at least report something to the
  // browser console.
  if (windows.IsEmpty()) {
    nsContentUtils::ReportToConsoleNonLocalized(aMessage,
                                                aFlags,
                                                NS_LITERAL_CSTRING("Service Workers"),
                                                nullptr,  // document
                                                uri,
                                                aLine,
                                                aLineNumber,
                                                aColumnNumber,
                                                nsContentUtils::eOMIT_LOCATION);
    return;
  }
}

/* static */
void
ServiceWorkerManager::LocalizeAndReportToAllClients(
                                          const nsCString& aScope,
                                          const char* aStringKey,
                                          const nsTArray<nsString>& aParamArray,
                                          uint32_t aFlags,
                                          const nsString& aFilename,
                                          const nsString& aLine,
                                          uint32_t aLineNumber,
                                          uint32_t aColumnNumber)
{
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    return;
  }

  nsresult rv;
  nsXPIDLString message;
  rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                             aStringKey, aParamArray, message);
  if (NS_SUCCEEDED(rv)) {
    swm->ReportToAllClients(aScope, message,
                            aFilename, aLine, aLineNumber, aColumnNumber,
                            aFlags);
  } else {
    NS_WARNING("Failed to format and therefore report localized error.");
  }
}

void
ServiceWorkerManager::FlushReportsToAllClients(const nsACString& aScope,
                                               nsIConsoleReportCollector* aReporter)
{
  AutoTArray<uint64_t, 16> windows;

  // Report errors to every controlled document.
  for (auto iter = mControlledDocuments.Iter(); !iter.Done(); iter.Next()) {
    ServiceWorkerRegistrationInfo* reg = iter.UserData();
    MOZ_ASSERT(reg);
    if (!reg->mScope.Equals(aScope)) {
      continue;
    }

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(iter.Key());
    if (!doc || !doc->IsCurrentActiveDocument() || !doc->GetWindow()) {
      continue;
    }

    uint64_t innerWindowId = doc->InnerWindowID();
    windows.AppendElement(innerWindowId);

    aReporter->FlushReportsToConsole(
      innerWindowId, nsIConsoleReportCollector::ReportAction::Save);
  }

  // Report to any documents that have called .register() for this scope.  They
  // may not be controlled, but will still want to see error reports.
  WeakDocumentList* regList = mRegisteringDocuments.Get(aScope);
  if (regList) {
    for (int32_t i = regList->Length() - 1; i >= 0; --i) {
      nsCOMPtr<nsIDocument> doc = do_QueryReferent(regList->ElementAt(i));
      if (!doc) {
        regList->RemoveElementAt(i);
        continue;
      }

      if (!doc->IsCurrentActiveDocument()) {
        continue;
      }

      uint64_t innerWindowId = doc->InnerWindowID();
      if (windows.Contains(innerWindowId)) {
        continue;
      }

      windows.AppendElement(innerWindowId);

      aReporter->FlushReportsToConsole(
        innerWindowId, nsIConsoleReportCollector::ReportAction::Save);
    }

    if (regList->IsEmpty()) {
      regList = nullptr;
      nsAutoPtr<WeakDocumentList> doomed;
      mRegisteringDocuments.RemoveAndForget(aScope, doomed);
    }
  }

  nsresult rv;
  InterceptionList* intList = mNavigationInterceptions.Get(aScope);
  if (intList) {
    for (uint32_t i = 0; i < intList->Length(); ++i) {
      nsCOMPtr<nsIInterceptedChannel> channel = intList->ElementAt(i);

      nsCOMPtr<nsIChannel> inner;
      rv = channel->GetChannel(getter_AddRefs(inner));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }

      uint64_t innerWindowId = nsContentUtils::GetInnerWindowID(inner);
      if (innerWindowId == 0 || windows.Contains(innerWindowId)) {
        continue;
      }

      windows.AppendElement(innerWindowId);

      aReporter->FlushReportsToConsole(
        innerWindowId, nsIConsoleReportCollector::ReportAction::Save);
    }
  }

  // If there are no documents to report to, at least report something to the
  // browser console.
  if (windows.IsEmpty()) {
    aReporter->FlushReportsToConsole(0);
    return;
  }

  aReporter->ClearConsoleReports();
}

void
ServiceWorkerManager::HandleError(JSContext* aCx,
                                  nsIPrincipal* aPrincipal,
                                  const nsCString& aScope,
                                  const nsString& aWorkerURL,
                                  const nsString& aMessage,
                                  const nsString& aFilename,
                                  const nsString& aLine,
                                  uint32_t aLineNumber,
                                  uint32_t aColumnNumber,
                                  uint32_t aFlags,
                                  JSExnType aExnType)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);

  nsAutoCString scopeKey;
  nsresult rv = PrincipalToScopeKey(aPrincipal, scopeKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  ServiceWorkerManager::RegistrationDataPerPrincipal* data;
  if (NS_WARN_IF(!mRegistrationInfos.Get(scopeKey, &data))) {
    return;
  }

  // Always report any uncaught exceptions or errors to the console of
  // each client.
  ReportToAllClients(aScope, aMessage, aFilename, aLine, aLineNumber,
                     aColumnNumber, aFlags);
}

void
ServiceWorkerManager::LoadRegistration(
                             const ServiceWorkerRegistrationData& aRegistration)
{
  AssertIsOnMainThread();

  nsCOMPtr<nsIPrincipal> principal =
    PrincipalInfoToPrincipal(aRegistration.principal());
  if (!principal) {
    return;
  }

  RefPtr<ServiceWorkerRegistrationInfo> registration =
    GetRegistration(principal, aRegistration.scope());
  if (!registration) {
    registration = CreateNewRegistration(aRegistration.scope(), principal,
                                         aRegistration.loadFlags());
  } else {
    // If active worker script matches our expectations for a "current worker",
    // then we are done. Since scripts with the same URL might have different
    // contents such as updated scripts or scripts with different LoadFlags, we
    // use the CacheName to judje whether the two scripts are identical, where
    // the CacheName is an UUID generated when a new script is found.
    if (registration->GetActive() &&
        registration->GetActive()->CacheName() == aRegistration.cacheName()) {
      // No needs for updates.
      return;
    }
  }

  registration->SetLastUpdateTime(aRegistration.lastUpdateTime());

  const nsCString& currentWorkerURL = aRegistration.currentWorkerURL();
  if (!currentWorkerURL.IsEmpty()) {
    registration->SetActive(
      new ServiceWorkerInfo(registration->mPrincipal,
                            registration->mScope,
                            currentWorkerURL,
                            aRegistration.cacheName(),
                            registration->GetLoadFlags()));
    registration->GetActive()->SetHandlesFetch(aRegistration.currentWorkerHandlesFetch());
    registration->GetActive()->SetInstalledTime(aRegistration.currentWorkerInstalledTime());
    registration->GetActive()->SetActivatedTime(aRegistration.currentWorkerActivatedTime());
  }
}

void
ServiceWorkerManager::LoadRegistrations(
                  const nsTArray<ServiceWorkerRegistrationData>& aRegistrations)
{
  AssertIsOnMainThread();

  for (uint32_t i = 0, len = aRegistrations.Length(); i < len; ++i) {
    LoadRegistration(aRegistrations[i]);
  }
}

void
ServiceWorkerManager::ActorFailed()
{
  MOZ_DIAGNOSTIC_ASSERT(!mActor);
  MaybeStartShutdown();
}

void
ServiceWorkerManager::ActorCreated(mozilla::ipc::PBackgroundChild* aActor)
{
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!mActor);

  if (mShuttingDown) {
    MOZ_DIAGNOSTIC_ASSERT(mPendingOperations.IsEmpty());
    return;
  }

  PServiceWorkerManagerChild* actor =
    aActor->SendPServiceWorkerManagerConstructor();
  if (!actor) {
    ActorFailed();
    return;
  }

  mActor = static_cast<ServiceWorkerManagerChild*>(actor);

  // Flush the pending requests.
  for (uint32_t i = 0, len = mPendingOperations.Length(); i < len; ++i) {
    MOZ_ASSERT(mPendingOperations[i]);
    nsresult rv = NS_DispatchToCurrentThread(mPendingOperations[i].forget());
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to dispatch a runnable.");
    }
  }

  mPendingOperations.Clear();
}

void
ServiceWorkerManager::StoreRegistration(
                                   nsIPrincipal* aPrincipal,
                                   ServiceWorkerRegistrationInfo* aRegistration)
{
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aRegistration);

  if (mShuttingDown) {
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(mActor);
  if (!mActor) {
    return;
  }

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

  mActor->SendRegister(data);
}

already_AddRefed<ServiceWorkerRegistrationInfo>
ServiceWorkerManager::GetServiceWorkerRegistrationInfo(nsPIDOMWindowInner* aWindow)
{
  MOZ_ASSERT(aWindow);
  nsCOMPtr<nsIDocument> document = aWindow->GetExtantDoc();
  return GetServiceWorkerRegistrationInfo(document);
}

already_AddRefed<ServiceWorkerRegistrationInfo>
ServiceWorkerManager::GetServiceWorkerRegistrationInfo(nsIDocument* aDoc)
{
  MOZ_ASSERT(aDoc);
  nsCOMPtr<nsIURI> documentURI = aDoc->GetDocumentURI();
  nsCOMPtr<nsIPrincipal> principal = aDoc->NodePrincipal();
  return GetServiceWorkerRegistrationInfo(principal, documentURI);
}

already_AddRefed<ServiceWorkerRegistrationInfo>
ServiceWorkerManager::GetServiceWorkerRegistrationInfo(nsIPrincipal* aPrincipal,
                                                       nsIURI* aURI)
{
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aURI);

  //XXXnsm Temporary fix until Bug 1171432 is fixed.
  if (NS_WARN_IF(BasePrincipal::Cast(aPrincipal)->AppId() == nsIScriptSecurityManager::UNKNOWN_APP_ID)) {
    return nullptr;
  }

  nsAutoCString scopeKey;
  nsresult rv = PrincipalToScopeKey(aPrincipal, scopeKey);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return GetServiceWorkerRegistrationInfo(scopeKey, aURI);
}

already_AddRefed<ServiceWorkerRegistrationInfo>
ServiceWorkerManager::GetServiceWorkerRegistrationInfo(const nsACString& aScopeKey,
                                                       nsIURI* aURI)
{
  MOZ_ASSERT(aURI);

  nsAutoCString spec;
  nsresult rv = aURI->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  nsAutoCString scope;
  RegistrationDataPerPrincipal* data;
  if (!FindScopeForPath(aScopeKey, spec, &data, scope)) {
    return nullptr;
  }

  MOZ_ASSERT(data);

  RefPtr<ServiceWorkerRegistrationInfo> registration;
  data->mInfos.Get(scope, getter_AddRefs(registration));
  // ordered scopes and registrations better be in sync.
  MOZ_ASSERT(registration);

#ifdef DEBUG
  nsAutoCString origin;
  rv = registration->mPrincipal->GetOrigin(origin);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  MOZ_ASSERT(origin.Equals(aScopeKey));
#endif

  if (registration->mPendingUninstall) {
    return nullptr;
  }
  return registration.forget();
}

/* static */ nsresult
ServiceWorkerManager::PrincipalToScopeKey(nsIPrincipal* aPrincipal,
                                          nsACString& aKey)
{
  MOZ_ASSERT(aPrincipal);

  if (!BasePrincipal::Cast(aPrincipal)->IsCodebasePrincipal()) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = aPrincipal->GetOrigin(aKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

/* static */ void
ServiceWorkerManager::AddScopeAndRegistration(const nsACString& aScope,
                                              ServiceWorkerRegistrationInfo* aInfo)
{
  MOZ_ASSERT(aInfo);
  MOZ_ASSERT(aInfo->mPrincipal);

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    // browser shutdown
    return;
  }

  nsAutoCString scopeKey;
  nsresult rv = swm->PrincipalToScopeKey(aInfo->mPrincipal, scopeKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  MOZ_ASSERT(!scopeKey.IsEmpty());

  RegistrationDataPerPrincipal* data =
    swm->mRegistrationInfos.LookupForAdd(scopeKey).OrInsert(
      []() { return new RegistrationDataPerPrincipal(); });

  for (uint32_t i = 0; i < data->mOrderedScopes.Length(); ++i) {
    const nsCString& current = data->mOrderedScopes[i];

    // Perfect match!
    if (aScope.Equals(current)) {
      data->mInfos.Put(aScope, aInfo);
      swm->NotifyListenersOnRegister(aInfo);
      return;
    }

    // Sort by length, with longest match first.
    // /foo/bar should be before /foo/
    // Similarly /foo/b is between the two.
    if (StringBeginsWith(aScope, current)) {
      data->mOrderedScopes.InsertElementAt(i, aScope);
      data->mInfos.Put(aScope, aInfo);
      swm->NotifyListenersOnRegister(aInfo);
      return;
    }
  }

  data->mOrderedScopes.AppendElement(aScope);
  data->mInfos.Put(aScope, aInfo);
  swm->NotifyListenersOnRegister(aInfo);
}

/* static */ bool
ServiceWorkerManager::FindScopeForPath(const nsACString& aScopeKey,
                                       const nsACString& aPath,
                                       RegistrationDataPerPrincipal** aData,
                                       nsACString& aMatch)
{
  MOZ_ASSERT(aData);

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();

  if (!swm || !swm->mRegistrationInfos.Get(aScopeKey, aData)) {
    return false;
  }

  for (uint32_t i = 0; i < (*aData)->mOrderedScopes.Length(); ++i) {
    const nsCString& current = (*aData)->mOrderedScopes[i];
    if (StringBeginsWith(aPath, current)) {
      aMatch = current;
      return true;
    }
  }

  return false;
}

/* static */ bool
ServiceWorkerManager::HasScope(nsIPrincipal* aPrincipal,
                               const nsACString& aScope)
{
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    return false;
  }

  nsAutoCString scopeKey;
  nsresult rv = PrincipalToScopeKey(aPrincipal, scopeKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  RegistrationDataPerPrincipal* data;
  if (!swm->mRegistrationInfos.Get(scopeKey, &data)) {
    return false;
  }

  return data->mOrderedScopes.Contains(aScope);
}

/* static */ void
ServiceWorkerManager::RemoveScopeAndRegistration(ServiceWorkerRegistrationInfo* aRegistration)
{
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    return;
  }

  nsAutoCString scopeKey;
  nsresult rv = swm->PrincipalToScopeKey(aRegistration->mPrincipal, scopeKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  RegistrationDataPerPrincipal* data;
  if (!swm->mRegistrationInfos.Get(scopeKey, &data)) {
    return;
  }

  data->mUpdateTimers.LookupRemoveIf(aRegistration->mScope,
    [] (nsCOMPtr<nsITimer>& aTimer) {
      aTimer->Cancel();
      return true;  // remove it
    });

  // The registration should generally only be removed if there are no controlled
  // documents, but mControlledDocuments can contain references to potentially
  // controlled docs.  This happens when the service worker is not active yet.
  // We must purge these references since we are evicting the registration.
  for (auto iter = swm->mControlledDocuments.Iter(); !iter.Done(); iter.Next()) {
    ServiceWorkerRegistrationInfo* reg = iter.UserData();
    MOZ_ASSERT(reg);
    if (reg->mScope.Equals(aRegistration->mScope)) {
      iter.Remove();
    }
  }

  RefPtr<ServiceWorkerRegistrationInfo> info;
  data->mInfos.Remove(aRegistration->mScope, getter_AddRefs(info));
  data->mOrderedScopes.RemoveElement(aRegistration->mScope);
  swm->NotifyListenersOnUnregister(info);

  swm->MaybeRemoveRegistrationInfo(scopeKey);
  swm->NotifyServiceWorkerRegistrationRemoved(aRegistration);
}

void
ServiceWorkerManager::MaybeRemoveRegistrationInfo(const nsACString& aScopeKey)
{
  mRegistrationInfos.LookupRemoveIf(aScopeKey,
    [] (RegistrationDataPerPrincipal* aData) {
      bool remove = aData->mOrderedScopes.IsEmpty() &&
                    aData->mJobQueues.Count() == 0;
      return remove;
    });
}

void
ServiceWorkerManager::MaybeStartControlling(nsIDocument* aDoc,
                                            const nsAString& aDocumentId)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aDoc);
  RefPtr<ServiceWorkerRegistrationInfo> registration =
    GetServiceWorkerRegistrationInfo(aDoc);
  if (registration) {
    MOZ_ASSERT(!mControlledDocuments.Contains(aDoc));
    StartControllingADocument(registration, aDoc, aDocumentId);
  }
}

void
ServiceWorkerManager::MaybeStopControlling(nsIDocument* aDoc)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aDoc);
  RefPtr<ServiceWorkerRegistrationInfo> registration;
  mControlledDocuments.Remove(aDoc, getter_AddRefs(registration));
  // A document which was uncontrolled does not maintain that state itself, so
  // it will always call MaybeStopControlling() even if there isn't an
  // associated registration. So this check is required.
  if (registration) {
    StopControllingADocument(registration);
  }
}

void
ServiceWorkerManager::MaybeCheckNavigationUpdate(nsIDocument* aDoc)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aDoc);
  // We perform these success path navigation update steps when the
  // document tells us its more or less done loading.  This avoids
  // slowing down page load and also lets pages consistently get
  // updatefound events when they fire.
  //
  // 9.8.20 If respondWithEntered is false, then:
  // 9.8.22 Else: (respondWith was entered and succeeded)
  //    If request is a non-subresource request, then: Invoke Soft Update
  //    algorithm.
  RefPtr<ServiceWorkerRegistrationInfo> registration;
  mControlledDocuments.Get(aDoc, getter_AddRefs(registration));
  if (registration) {
    registration->MaybeScheduleUpdate();
  }
}

void
ServiceWorkerManager::StartControllingADocument(ServiceWorkerRegistrationInfo* aRegistration,
                                                nsIDocument* aDoc,
                                                const nsAString& aDocumentId)
{
  MOZ_ASSERT(aRegistration);
  MOZ_ASSERT(aDoc);

  aRegistration->StartControllingADocument();
  mControlledDocuments.Put(aDoc, aRegistration);
  if (!aDocumentId.IsEmpty()) {
    aDoc->SetId(aDocumentId);
  }
  Telemetry::Accumulate(Telemetry::SERVICE_WORKER_CONTROLLED_DOCUMENTS, 1);
}

void
ServiceWorkerManager::StopControllingADocument(ServiceWorkerRegistrationInfo* aRegistration)
{
  aRegistration->StopControllingADocument();
  if (aRegistration->IsControllingDocuments() || !aRegistration->IsIdle()) {
    return;
  }

  if (aRegistration->mPendingUninstall) {
    RemoveRegistration(aRegistration);
    return;
  }

  // We use to aggressively terminate the worker at this point, but it
  // caused problems.  There are more uses for a service worker than actively
  // controlled documents.  We need to let the worker naturally terminate
  // in case its handling push events, message events, etc.
  aRegistration->TryToActivateAsync();
}

NS_IMETHODIMP
ServiceWorkerManager::GetScopeForUrl(nsIPrincipal* aPrincipal,
                                     const nsAString& aUrl, nsAString& aScope)
{
  MOZ_ASSERT(aPrincipal);

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aUrl, nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<ServiceWorkerRegistrationInfo> r =
    GetServiceWorkerRegistrationInfo(aPrincipal, uri);
  if (!r) {
      return NS_ERROR_FAILURE;
  }

  aScope = NS_ConvertUTF8toUTF16(r->mScope);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerManager::AddRegistrationEventListener(const nsAString& aScope,
                                                   ServiceWorkerRegistrationListener* aListener)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aListener);
#ifdef DEBUG
  // Ensure a registration is only listening for it's own scope.
  nsAutoString regScope;
  aListener->GetScope(regScope);
  MOZ_ASSERT(!regScope.IsEmpty());
  MOZ_ASSERT(aScope.Equals(regScope));
#endif

  MOZ_ASSERT(!mServiceWorkerRegistrationListeners.Contains(aListener));
  mServiceWorkerRegistrationListeners.AppendElement(aListener);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerManager::RemoveRegistrationEventListener(const nsAString& aScope,
                                                      ServiceWorkerRegistrationListener* aListener)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aListener);
#ifdef DEBUG
  // Ensure a registration is unregistering for it's own scope.
  nsAutoString regScope;
  aListener->GetScope(regScope);
  MOZ_ASSERT(!regScope.IsEmpty());
  MOZ_ASSERT(aScope.Equals(regScope));
#endif

  MOZ_ASSERT(mServiceWorkerRegistrationListeners.Contains(aListener));
  mServiceWorkerRegistrationListeners.RemoveElement(aListener);
  return NS_OK;
}

void
ServiceWorkerManager::FireUpdateFoundOnServiceWorkerRegistrations(
  ServiceWorkerRegistrationInfo* aRegistration)
{
  AssertIsOnMainThread();

  nsTObserverArray<ServiceWorkerRegistrationListener*>::ForwardIterator it(mServiceWorkerRegistrationListeners);
  while (it.HasMore()) {
    RefPtr<ServiceWorkerRegistrationListener> target = it.GetNext();
    nsAutoString regScope;
    target->GetScope(regScope);
    MOZ_ASSERT(!regScope.IsEmpty());

    NS_ConvertUTF16toUTF8 utf8Scope(regScope);
    if (utf8Scope.Equals(aRegistration->mScope)) {
      target->UpdateFound();
    }
  }
}

/*
 * This is used for installing, waiting and active.
 */
nsresult
ServiceWorkerManager::GetServiceWorkerForScope(nsPIDOMWindowInner* aWindow,
                                               const nsAString& aScope,
                                               WhichServiceWorker aWhichWorker,
                                               nsISupports** aServiceWorker)
{
  AssertIsOnMainThread();

  if (NS_WARN_IF(!aWindow)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsCOMPtr<nsIDocument> doc = aWindow->GetExtantDoc();
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

  RefPtr<ServiceWorkerRegistrationInfo> registration =
    GetRegistration(documentPrincipal, scope);
  if (NS_WARN_IF(!registration)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<ServiceWorkerInfo> info;
  if (aWhichWorker == WhichServiceWorker::INSTALLING_WORKER) {
    info = registration->GetInstalling();
  } else if (aWhichWorker == WhichServiceWorker::WAITING_WORKER) {
    info = registration->GetWaiting();
  } else if (aWhichWorker == WhichServiceWorker::ACTIVE_WORKER) {
    info = registration->GetActive();
  } else {
    MOZ_CRASH("Invalid worker type");
  }

  if (NS_WARN_IF(!info)) {
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  RefPtr<ServiceWorker> serviceWorker = info->GetOrCreateInstance(aWindow);

  serviceWorker->SetState(info->State());
  serviceWorker.forget(aServiceWorker);
  return NS_OK;
}

namespace {

class ContinueDispatchFetchEventRunnable : public Runnable
{
  RefPtr<ServiceWorkerPrivate> mServiceWorkerPrivate;
  nsCOMPtr<nsIInterceptedChannel> mChannel;
  nsCOMPtr<nsILoadGroup> mLoadGroup;
  nsString mDocumentId;
  bool mIsReload;
public:
  ContinueDispatchFetchEventRunnable(ServiceWorkerPrivate* aServiceWorkerPrivate,
                                     nsIInterceptedChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     const nsAString& aDocumentId,
                                     bool aIsReload)
    : mServiceWorkerPrivate(aServiceWorkerPrivate)
    , mChannel(aChannel)
    , mLoadGroup(aLoadGroup)
    , mDocumentId(aDocumentId)
    , mIsReload(aIsReload)
  {
    MOZ_ASSERT(aServiceWorkerPrivate);
    MOZ_ASSERT(aChannel);
  }

  void
  HandleError()
  {
    AssertIsOnMainThread();
    NS_WARNING("Unexpected error while dispatching fetch event!");
    DebugOnly<nsresult> rv = mChannel->ResetInterception();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Failed to resume intercepted network request");
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();

    nsCOMPtr<nsIChannel> channel;
    nsresult rv = mChannel->GetChannel(getter_AddRefs(channel));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      HandleError();
      return NS_OK;
    }

    // The channel might have encountered an unexpected error while ensuring
    // the upload stream is cloneable.  Check here and reset the interception
    // if that happens.
    nsresult status;
    rv = channel->GetStatus(&status);
    if (NS_WARN_IF(NS_FAILED(rv) || NS_FAILED(status))) {
      HandleError();
      return NS_OK;
    }

    rv = mServiceWorkerPrivate->SendFetchEvent(mChannel, mLoadGroup,
                                               mDocumentId, mIsReload);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      HandleError();
    }

    return NS_OK;
  }
};

} // anonymous namespace

void
ServiceWorkerManager::DispatchFetchEvent(const OriginAttributes& aOriginAttributes,
                                         nsIDocument* aDoc,
                                         const nsAString& aDocumentIdForTopLevelNavigation,
                                         nsIInterceptedChannel* aChannel,
                                         bool aIsReload,
                                         bool aIsSubresourceLoad,
                                         ErrorResult& aRv)
{
  MOZ_ASSERT(aChannel);
  AssertIsOnMainThread();

  RefPtr<ServiceWorkerInfo> serviceWorker;
  nsCOMPtr<nsILoadGroup> loadGroup;
  nsAutoString documentId;

  if (aIsSubresourceLoad) {
    MOZ_ASSERT(aDoc);

    serviceWorker = GetActiveWorkerInfoForDocument(aDoc);
    if (!serviceWorker) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    loadGroup = aDoc->GetDocumentLoadGroup();
    nsresult rv = aDoc->GetOrCreateId(documentId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  } else {
    nsCOMPtr<nsIChannel> internalChannel;
    aRv = aChannel->GetChannel(getter_AddRefs(internalChannel));
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    internalChannel->GetLoadGroup(getter_AddRefs(loadGroup));

    // TODO: Use aDocumentIdForTopLevelNavigation for potentialClientId, pending
    // the spec change.

    nsCOMPtr<nsIURI> uri;
    aRv = aChannel->GetSecureUpgradedChannelURI(getter_AddRefs(uri));
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    // non-subresource request means the URI contains the principal
    nsCOMPtr<nsIPrincipal> principal =
      BasePrincipal::CreateCodebasePrincipal(uri, aOriginAttributes);

    RefPtr<ServiceWorkerRegistrationInfo> registration =
      GetServiceWorkerRegistrationInfo(principal, uri);
    if (!registration) {
      NS_WARNING("No registration found when dispatching the fetch event");
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    // While we only enter this method if IsAvailable() previously saw
    // an active worker, it is possible for that worker to be removed
    // before we get to this point.  Therefore we must handle a nullptr
    // active worker here.
    serviceWorker = registration->GetActive();
    if (!serviceWorker) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    AddNavigationInterception(serviceWorker->Scope(), aChannel);
  }

  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(serviceWorker);

  nsCOMPtr<nsIRunnable> continueRunnable =
    new ContinueDispatchFetchEventRunnable(serviceWorker->WorkerPrivate(),
                                           aChannel, loadGroup,
                                           documentId, aIsReload);

  // When this service worker was registered, we also sent down the permissions
  // for the runnable. They should have arrived by now, but we still need to
  // wait for them if they have not.
  nsCOMPtr<nsIRunnable> permissionsRunnable = NS_NewRunnableFunction([=] () {
      nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();
      MOZ_ALWAYS_SUCCEEDS(permMgr->WhenPermissionsAvailable(serviceWorker->Principal(),
                                                            continueRunnable));
    });

  nsCOMPtr<nsIChannel> innerChannel;
  aRv = aChannel->GetChannel(getter_AddRefs(innerChannel));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsCOMPtr<nsIUploadChannel2> uploadChannel = do_QueryInterface(innerChannel);

  // If there is no upload stream, then continue immediately
  if (!uploadChannel) {
    MOZ_ALWAYS_SUCCEEDS(permissionsRunnable->Run());
    return;
  }
  // Otherwise, ensure the upload stream can be cloned directly.  This may
  // require some async copying, so provide a callback.
  aRv = uploadChannel->EnsureUploadStreamIsCloneable(permissionsRunnable);
}

bool
ServiceWorkerManager::IsAvailable(nsIPrincipal* aPrincipal,
                                  nsIURI* aURI)
{
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aURI);

  RefPtr<ServiceWorkerRegistrationInfo> registration =
    GetServiceWorkerRegistrationInfo(aPrincipal, aURI);
  return registration && registration->GetActive();
}

bool
ServiceWorkerManager::IsControlled(nsIDocument* aDoc, ErrorResult& aRv)
{
  MOZ_ASSERT(aDoc);

  if (nsContentUtils::IsInPrivateBrowsing(aDoc)) {
    // Handle the case where a service worker was previously registered in
    // a non-private window (bug 1255621).
    return false;
  }

  RefPtr<ServiceWorkerRegistrationInfo> registration;
  nsresult rv = GetDocumentRegistration(aDoc, getter_AddRefs(registration));
  if (NS_WARN_IF(NS_FAILED(rv) && rv != NS_ERROR_NOT_AVAILABLE)) {
    // It's OK to ignore the case where we don't have a registration.
    aRv.Throw(rv);
    return false;
  }

  return !!registration;
}

nsresult
ServiceWorkerManager::GetDocumentRegistration(nsIDocument* aDoc,
                                              ServiceWorkerRegistrationInfo** aRegistrationInfo)
{
  RefPtr<ServiceWorkerRegistrationInfo> registration;
  if (!mControlledDocuments.Get(aDoc, getter_AddRefs(registration))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // If the document is controlled, the current worker MUST be non-null.
  if (!registration->GetActive()) {
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
ServiceWorkerManager::GetDocumentController(nsPIDOMWindowInner* aWindow,
                                            nsISupports** aServiceWorker)
{
  if (NS_WARN_IF(!aWindow)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsCOMPtr<nsIDocument> doc = aWindow->GetExtantDoc();
  if (!doc) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  RefPtr<ServiceWorkerRegistrationInfo> registration;
  nsresult rv = GetDocumentRegistration(doc, getter_AddRefs(registration));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(registration->GetActive());
  RefPtr<ServiceWorker> serviceWorker =
    registration->GetActive()->GetOrCreateInstance(aWindow);

  serviceWorker.forget(aServiceWorker);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerManager::GetInstalling(nsPIDOMWindowInner* aWindow,
                                    const nsAString& aScope,
                                    nsISupports** aServiceWorker)
{
  return GetServiceWorkerForScope(aWindow, aScope,
                                  WhichServiceWorker::INSTALLING_WORKER,
                                  aServiceWorker);
}

NS_IMETHODIMP
ServiceWorkerManager::GetWaiting(nsPIDOMWindowInner* aWindow,
                                 const nsAString& aScope,
                                 nsISupports** aServiceWorker)
{
  return GetServiceWorkerForScope(aWindow, aScope,
                                  WhichServiceWorker::WAITING_WORKER,
                                  aServiceWorker);
}

NS_IMETHODIMP
ServiceWorkerManager::GetActive(nsPIDOMWindowInner* aWindow,
                                const nsAString& aScope,
                                nsISupports** aServiceWorker)
{
  return GetServiceWorkerForScope(aWindow, aScope,
                                  WhichServiceWorker::ACTIVE_WORKER,
                                  aServiceWorker);
}

void
ServiceWorkerManager::TransitionServiceWorkerRegistrationWorker(ServiceWorkerRegistrationInfo* aRegistration,
                                                                WhichServiceWorker aWhichOne)
{
  AssertIsOnMainThread();
  nsTObserverArray<ServiceWorkerRegistrationListener*>::ForwardIterator it(mServiceWorkerRegistrationListeners);
  while (it.HasMore()) {
    RefPtr<ServiceWorkerRegistrationListener> target = it.GetNext();
    nsAutoString regScope;
    target->GetScope(regScope);
    MOZ_ASSERT(!regScope.IsEmpty());

    NS_ConvertUTF16toUTF8 utf8Scope(regScope);

    if (utf8Scope.Equals(aRegistration->mScope)) {
      target->TransitionWorker(aWhichOne);
    }
  }
}

void
ServiceWorkerManager::InvalidateServiceWorkerRegistrationWorker(ServiceWorkerRegistrationInfo* aRegistration,
                                                                WhichServiceWorker aWhichOnes)
{
  AssertIsOnMainThread();
  nsTObserverArray<ServiceWorkerRegistrationListener*>::ForwardIterator it(mServiceWorkerRegistrationListeners);
  while (it.HasMore()) {
    RefPtr<ServiceWorkerRegistrationListener> target = it.GetNext();
    nsAutoString regScope;
    target->GetScope(regScope);
    MOZ_ASSERT(!regScope.IsEmpty());

    NS_ConvertUTF16toUTF8 utf8Scope(regScope);

    if (utf8Scope.Equals(aRegistration->mScope)) {
      target->InvalidateWorkers(aWhichOnes);
    }
  }
}

void
ServiceWorkerManager::NotifyServiceWorkerRegistrationRemoved(ServiceWorkerRegistrationInfo* aRegistration)
{
  AssertIsOnMainThread();
  nsTObserverArray<ServiceWorkerRegistrationListener*>::ForwardIterator it(mServiceWorkerRegistrationListeners);
  while (it.HasMore()) {
    RefPtr<ServiceWorkerRegistrationListener> target = it.GetNext();
    nsAutoString regScope;
    target->GetScope(regScope);
    MOZ_ASSERT(!regScope.IsEmpty());

    NS_ConvertUTF16toUTF8 utf8Scope(regScope);

    if (utf8Scope.Equals(aRegistration->mScope)) {
      target->RegistrationRemoved();
    }
  }
}

void
ServiceWorkerManager::SoftUpdate(const OriginAttributes& aOriginAttributes,
                                 const nsACString& aScope)
{
  AssertIsOnMainThread();

  if (mShuttingDown) {
    return;
  }

  if (!mActor) {
    RefPtr<Runnable> runnable =
      new SoftUpdateRunnable(aOriginAttributes, aScope, false, nullptr);
    AppendPendingOperation(runnable);
    return;
  }

  RefPtr<GenericPromise::Private> promise =
    new GenericPromise::Private(__func__);

  RefPtr<CancelableRunnable> successRunnable =
    new SoftUpdateRunnable(aOriginAttributes, aScope, true, promise);

  RefPtr<CancelableRunnable> failureRunnable =
    new ResolvePromiseRunnable(promise);

  ServiceWorkerUpdaterChild* actor =
    new ServiceWorkerUpdaterChild(promise, successRunnable, failureRunnable);

  mActor->SendPServiceWorkerUpdaterConstructor(actor, aOriginAttributes,
                                               nsCString(aScope));
}

namespace {

class UpdateJobCallback final : public ServiceWorkerJob::Callback
{
  RefPtr<ServiceWorkerUpdateFinishCallback> mCallback;

  ~UpdateJobCallback() = default;

public:
  explicit UpdateJobCallback(ServiceWorkerUpdateFinishCallback* aCallback)
    : mCallback(aCallback)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mCallback);
  }

  void
  JobFinished(ServiceWorkerJob* aJob, ErrorResult& aStatus)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aJob);

    if (aStatus.Failed()) {
      mCallback->UpdateFailed(aStatus);
      return;
    }

    MOZ_DIAGNOSTIC_ASSERT(aJob->GetType() == ServiceWorkerJob::Type::Update);
    RefPtr<ServiceWorkerUpdateJob> updateJob =
      static_cast<ServiceWorkerUpdateJob*>(aJob);
    RefPtr<ServiceWorkerRegistrationInfo> reg = updateJob->GetRegistration();
    mCallback->UpdateSucceeded(reg);
  }

  NS_INLINE_DECL_REFCOUNTING(UpdateJobCallback)
};

} // anonymous namespace

void
ServiceWorkerManager::SoftUpdateInternal(const OriginAttributes& aOriginAttributes,
                                         const nsACString& aScope,
                                         ServiceWorkerUpdateFinishCallback* aCallback)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aCallback);

  if (mShuttingDown) {
    return;
  }

  nsCOMPtr<nsIURI> scopeURI;
  nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), aScope);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsIPrincipal> principal =
    BasePrincipal::CreateCodebasePrincipal(scopeURI, aOriginAttributes);
  if (NS_WARN_IF(!principal)) {
    return;
  }

  nsAutoCString scopeKey;
  rv = PrincipalToScopeKey(principal, scopeKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  RefPtr<ServiceWorkerRegistrationInfo> registration =
    GetRegistration(scopeKey, aScope);
  if (NS_WARN_IF(!registration)) {
    return;
  }

  // "If registration's uninstalling flag is set, abort these steps."
  if (registration->mPendingUninstall) {
    return;
  }

  // "If registration's installing worker is not null, abort these steps."
  if (registration->GetInstalling()) {
    return;
  }

  // "Let newestWorker be the result of running Get Newest Worker algorithm
  // passing registration as its argument.
  // If newestWorker is null, abort these steps."
  RefPtr<ServiceWorkerInfo> newest = registration->Newest();
  if (!newest) {
    return;
  }

  // "If the registration queue for registration is empty, invoke Update algorithm,
  // or its equivalent, with client, registration as its argument."
  // TODO(catalinb): We don't implement the force bypass cache flag.
  // See: https://github.com/slightlyoff/ServiceWorker/issues/759
  RefPtr<ServiceWorkerJobQueue> queue = GetOrCreateJobQueue(scopeKey,
                                                            aScope);

  RefPtr<ServiceWorkerUpdateJob> job =
    new ServiceWorkerUpdateJob(principal, registration->mScope,
                               newest->ScriptSpec(), nullptr,
                               registration->GetLoadFlags());

  RefPtr<UpdateJobCallback> cb = new UpdateJobCallback(aCallback);
  job->AppendResultCallback(cb);

  queue->ScheduleJob(job);
}

void
ServiceWorkerManager::Update(nsIPrincipal* aPrincipal,
                             const nsACString& aScope,
                             ServiceWorkerUpdateFinishCallback* aCallback)
{
  AssertIsOnMainThread();

  if (!mActor) {
    RefPtr<Runnable> runnable =
      new UpdateRunnable(aPrincipal, aScope, aCallback,
                         UpdateRunnable::ePostpone, nullptr);
    AppendPendingOperation(runnable);
    return;
  }

  RefPtr<GenericPromise::Private> promise =
    new GenericPromise::Private(__func__);

  RefPtr<CancelableRunnable> successRunnable =
    new UpdateRunnable(aPrincipal, aScope, aCallback,
                       UpdateRunnable::eSuccess, promise);

  RefPtr<CancelableRunnable> failureRunnable =
    new UpdateRunnable(aPrincipal, aScope, aCallback,
                       UpdateRunnable::eFailure, promise);

  ServiceWorkerUpdaterChild* actor =
    new ServiceWorkerUpdaterChild(promise, successRunnable, failureRunnable);

  mActor->SendPServiceWorkerUpdaterConstructor(actor,
                                               aPrincipal->OriginAttributesRef(),
                                               nsCString(aScope));
}

void
ServiceWorkerManager::UpdateInternal(nsIPrincipal* aPrincipal,
                                     const nsACString& aScope,
                                     ServiceWorkerUpdateFinishCallback* aCallback)
{
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aCallback);

  nsAutoCString scopeKey;
  nsresult rv = PrincipalToScopeKey(aPrincipal, scopeKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  RefPtr<ServiceWorkerRegistrationInfo> registration =
    GetRegistration(scopeKey, aScope);
  if (NS_WARN_IF(!registration)) {
    return;
  }

  // "Let newestWorker be the result of running Get Newest Worker algorithm
  // passing registration as its argument.
  // If newestWorker is null, return a promise rejected with "InvalidStateError"
  RefPtr<ServiceWorkerInfo> newest = registration->Newest();
  if (!newest) {
    ErrorResult error(NS_ERROR_DOM_INVALID_STATE_ERR);
    aCallback->UpdateFailed(error);

    // In case the callback does not consume the exception
    error.SuppressException();

    return;
  }

  RefPtr<ServiceWorkerJobQueue> queue = GetOrCreateJobQueue(scopeKey, aScope);

  // "Invoke Update algorithm, or its equivalent, with client, registration as
  // its argument."
  RefPtr<ServiceWorkerUpdateJob> job =
    new ServiceWorkerUpdateJob(aPrincipal, registration->mScope,
                               newest->ScriptSpec(), nullptr,
                               registration->GetLoadFlags());

  RefPtr<UpdateJobCallback> cb = new UpdateJobCallback(aCallback);
  job->AppendResultCallback(cb);

  queue->ScheduleJob(job);
}

namespace {

static void
FireControllerChangeOnDocument(nsIDocument* aDocument)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aDocument);

  nsCOMPtr<nsPIDOMWindowInner> w = aDocument->GetInnerWindow();
  if (!w) {
    NS_WARNING("Failed to dispatch controllerchange event");
    return;
  }

  auto* window = nsGlobalWindow::Cast(w.get());
  dom::Navigator* navigator = window->Navigator();
  if (!navigator) {
    return;
  }

  RefPtr<ServiceWorkerContainer> container = navigator->ServiceWorker();
  ErrorResult result;
  container->ControllerChanged(result);
  if (result.Failed()) {
    NS_WARNING("Failed to dispatch controllerchange event");
  }
}

} // anonymous namespace

UniquePtr<ServiceWorkerClientInfo>
ServiceWorkerManager::GetClient(nsIPrincipal* aPrincipal,
                                const nsAString& aClientId,
                                ErrorResult& aRv)
{
  UniquePtr<ServiceWorkerClientInfo> clientInfo;
  nsCOMPtr<nsISupportsInterfacePointer> ifptr =
    do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID);
  if (NS_WARN_IF(!ifptr)) {
    return clientInfo;
  }
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return clientInfo;
  }

  nsresult rv = obs->NotifyObservers(ifptr, "service-worker-get-client",
                                     PromiseFlatString(aClientId).get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return clientInfo;
  }

  nsCOMPtr<nsISupports> ptr;
  ifptr->GetData(getter_AddRefs(ptr));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(ptr);
  if (NS_WARN_IF(!doc)) {
    return clientInfo;
  }

  bool equals = false;
  aPrincipal->Equals(doc->NodePrincipal(), &equals);
  if (!equals) {
    return clientInfo;
  }

  if (!IsFromAuthenticatedOrigin(doc)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return clientInfo;
  }

  clientInfo.reset(new ServiceWorkerClientInfo(doc));
  return clientInfo;
}

void
ServiceWorkerManager::GetAllClients(nsIPrincipal* aPrincipal,
                                    const nsCString& aScope,
                                    uint64_t aServiceWorkerID,
                                    bool aIncludeUncontrolled,
                                    nsTArray<ServiceWorkerClientInfo>& aDocuments)
{
  MOZ_ASSERT(aPrincipal);

  RefPtr<ServiceWorkerRegistrationInfo> registration =
    GetRegistration(aPrincipal, aScope);

  if (!registration) {
    // The registration was removed, leave the array empty.
    return;
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return;
  }

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  nsresult rv = obs->EnumerateObservers("service-worker-get-client",
                                        getter_AddRefs(enumerator));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Get a list of Client documents out of the observer service
  AutoTArray<nsCOMPtr<nsIDocument>, 32> docList;
  bool loop = true;
  while (NS_SUCCEEDED(enumerator->HasMoreElements(&loop)) && loop) {
    nsCOMPtr<nsISupports> ptr;
    rv = enumerator->GetNext(getter_AddRefs(ptr));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(ptr);
    if (!doc || !doc->GetWindow()) {
      continue;
    }

    bool equals = false;
    Unused << aPrincipal->Equals(doc->NodePrincipal(), &equals);
    if (!equals) {
      continue;
    }

    // Treat http windows with devtools opened as secure if the correct devtools
    // setting is enabled.
    if (!doc->GetWindow()->GetServiceWorkersTestingEnabled() &&
        !Preferences::GetBool("dom.serviceWorkers.testing.enabled") &&
        !IsFromAuthenticatedOrigin(doc)) {
      continue;
    }

    // If we are only returning controlled Clients then skip any documents
    // that are for different registrations.  We also skip service workers
    // that don't match the ID of our calling service worker.  We should
    // only return Clients controlled by that precise service worker.
    if (!aIncludeUncontrolled) {
      ServiceWorkerRegistrationInfo* reg = mControlledDocuments.GetWeak(doc);
      if (!reg || reg->mScope != aScope || !reg->GetActive() ||
          reg->GetActive()->ID() != aServiceWorkerID) {
        continue;
      }
    }

    if (!aIncludeUncontrolled && !mControlledDocuments.Contains(doc)) {
      continue;
    }

    docList.AppendElement(doc.forget());
  }

  // The observer service gives us the list in reverse creation order.
  // We need to maintain creation order, so reverse the list before
  // processing.
  docList.Reverse();

  // Finally convert to the list of ServiceWorkerClientInfo objects.
  uint32_t ordinal = 0;
  for (uint32_t i = 0; i < docList.Length(); ++i) {
    aDocuments.AppendElement(ServiceWorkerClientInfo(docList[i], ordinal));
    ordinal += 1;
  }

  aDocuments.Sort();
}

void
ServiceWorkerManager::MaybeClaimClient(nsIDocument* aDocument,
                                       ServiceWorkerRegistrationInfo* aWorkerRegistration)
{
  MOZ_ASSERT(aWorkerRegistration);
  MOZ_ASSERT(aWorkerRegistration->GetActive());

  // Same origin check
  if (!aWorkerRegistration->mPrincipal->Equals(aDocument->NodePrincipal())) {
    return;
  }

  // The registration that should be controlling the client
  RefPtr<ServiceWorkerRegistrationInfo> matchingRegistration =
    GetServiceWorkerRegistrationInfo(aDocument);

  // The registration currently controlling the client
  RefPtr<ServiceWorkerRegistrationInfo> controllingRegistration;
  GetDocumentRegistration(aDocument, getter_AddRefs(controllingRegistration));

  if (aWorkerRegistration != matchingRegistration ||
        aWorkerRegistration == controllingRegistration) {
    return;
  }

  if (controllingRegistration) {
    StopControllingADocument(controllingRegistration);
  }

  StartControllingADocument(aWorkerRegistration, aDocument, NS_LITERAL_STRING(""));
  FireControllerChangeOnDocument(aDocument);
}

nsresult
ServiceWorkerManager::ClaimClients(nsIPrincipal* aPrincipal,
                                   const nsCString& aScope, uint64_t aId)
{
  RefPtr<ServiceWorkerRegistrationInfo> registration =
    GetRegistration(aPrincipal, aScope);

  if (!registration || !registration->GetActive() ||
      !(registration->GetActive()->ID() == aId)) {
    // The worker is not active.
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  nsresult rv = obs->EnumerateObservers("service-worker-get-client",
                                        getter_AddRefs(enumerator));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool loop = true;
  while (NS_SUCCEEDED(enumerator->HasMoreElements(&loop)) && loop) {
    nsCOMPtr<nsISupports> ptr;
    rv = enumerator->GetNext(getter_AddRefs(ptr));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(ptr);
    MaybeClaimClient(doc, registration);
  }

  return NS_OK;
}

void
ServiceWorkerManager::SetSkipWaitingFlag(nsIPrincipal* aPrincipal,
                                         const nsCString& aScope,
                                         uint64_t aServiceWorkerID)
{
  RefPtr<ServiceWorkerRegistrationInfo> registration =
    GetRegistration(aPrincipal, aScope);
  if (NS_WARN_IF(!registration)) {
    return;
  }

  RefPtr<ServiceWorkerInfo> worker =
    registration->GetServiceWorkerInfoById(aServiceWorkerID);

  if (NS_WARN_IF(!worker)) {
    return;
  }

  worker->SetSkipWaitingFlag();

  if (worker->State() == ServiceWorkerState::Installed) {
    registration->TryToActivateAsync();
  }
}

void
ServiceWorkerManager::FireControllerChange(ServiceWorkerRegistrationInfo* aRegistration)
{
  AssertIsOnMainThread();
  for (auto iter = mControlledDocuments.Iter(); !iter.Done(); iter.Next()) {
    if (iter.UserData() != aRegistration) {
      continue;
    }

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(iter.Key());
    if (NS_WARN_IF(!doc)) {
      continue;
    }

    FireControllerChangeOnDocument(doc);
  }
}

already_AddRefed<ServiceWorkerRegistrationInfo>
ServiceWorkerManager::GetRegistration(nsIPrincipal* aPrincipal,
                                      const nsACString& aScope) const
{
  MOZ_ASSERT(aPrincipal);

  nsAutoCString scopeKey;
  nsresult rv = PrincipalToScopeKey(aPrincipal, scopeKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return GetRegistration(scopeKey, aScope);
}

NS_IMETHODIMP
ServiceWorkerManager::GetRegistrationByPrincipal(nsIPrincipal* aPrincipal,
                                                 const nsAString& aScope,
                                                 nsIServiceWorkerRegistrationInfo** aInfo)
{
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aInfo);

  nsCOMPtr<nsIURI> scopeURI;
  nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), aScope, nullptr, nullptr);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<ServiceWorkerRegistrationInfo> info =
    GetServiceWorkerRegistrationInfo(aPrincipal, scopeURI);
  if (!info) {
    return NS_ERROR_FAILURE;
  }
  info.forget(aInfo);

  return NS_OK;
}

already_AddRefed<ServiceWorkerRegistrationInfo>
ServiceWorkerManager::GetRegistration(const nsACString& aScopeKey,
                                      const nsACString& aScope) const
{
  RefPtr<ServiceWorkerRegistrationInfo> reg;

  RegistrationDataPerPrincipal* data;
  if (!mRegistrationInfos.Get(aScopeKey, &data)) {
    return reg.forget();
  }

  data->mInfos.Get(aScope, getter_AddRefs(reg));
  return reg.forget();
}

already_AddRefed<ServiceWorkerRegistrationInfo>
ServiceWorkerManager::CreateNewRegistration(const nsCString& aScope,
                                            nsIPrincipal* aPrincipal,
                                            nsLoadFlags aLoadFlags)
{
#ifdef DEBUG
  AssertIsOnMainThread();
  nsCOMPtr<nsIURI> scopeURI;
  nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), aScope, nullptr, nullptr);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  RefPtr<ServiceWorkerRegistrationInfo> tmp =
    GetRegistration(aPrincipal, aScope);
  MOZ_ASSERT(!tmp);
#endif

  RefPtr<ServiceWorkerRegistrationInfo> registration =
    new ServiceWorkerRegistrationInfo(aScope, aPrincipal, aLoadFlags);
  // From now on ownership of registration is with
  // mServiceWorkerRegistrationInfos.
  AddScopeAndRegistration(aScope, registration);
  return registration.forget();
}

void
ServiceWorkerManager::MaybeRemoveRegistration(ServiceWorkerRegistrationInfo* aRegistration)
{
  MOZ_ASSERT(aRegistration);
  RefPtr<ServiceWorkerInfo> newest = aRegistration->Newest();
  if (!newest && HasScope(aRegistration->mPrincipal, aRegistration->mScope)) {
    RemoveRegistration(aRegistration);
  }
}

void
ServiceWorkerManager::RemoveRegistration(ServiceWorkerRegistrationInfo* aRegistration)
{
  // Note, we do not need to call mActor->SendUnregister() here.  There are a few
  // ways we can get here:
  // 1) Through a normal unregister which calls SendUnregister() in the unregister
  //    job Start() method.
  // 2) Through origin storage being purged.  These result in ForceUnregister()
  //    starting unregister jobs which in turn call SendUnregister().
  // 3) Through the failure to install a new service worker.  Since we don't store
  //    the registration until install succeeds, we do not need to call
  //    SendUnregister here.
  // Assert these conditions by testing for pending uninstall (cases 1 and 2) or
  // null workers (case 3).
#ifdef DEBUG
  RefPtr<ServiceWorkerInfo> newest = aRegistration->Newest();
  MOZ_ASSERT(aRegistration->mPendingUninstall || !newest);
#endif

  MOZ_ASSERT(HasScope(aRegistration->mPrincipal, aRegistration->mScope));

  // When a registration is removed, we must clear its contents since the DOM
  // object may be held by content script.
  aRegistration->Clear();

  RemoveScopeAndRegistration(aRegistration);
}

namespace {
/**
 * See toolkit/modules/sessionstore/Utils.jsm function hasRootDomain().
 *
 * Returns true if the |url| passed in is part of the given root |domain|.
 * For example, if |url| is "www.mozilla.org", and we pass in |domain| as
 * "mozilla.org", this will return true. It would return false the other way
 * around.
 */
bool
HasRootDomain(nsIURI* aURI, const nsACString& aDomain)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aURI);

  nsAutoCString host;
  nsresult rv = aURI->GetHost(host);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  nsACString::const_iterator start, end;
  host.BeginReading(start);
  host.EndReading(end);
  if (!FindInReadable(aDomain, start, end)) {
    return false;
  }

  if (host.Equals(aDomain)) {
    return true;
  }

  // Beginning of the string matches, can't look at the previous char.
  if (start.get() == host.BeginReading()) {
    // Equals failed so this is fine.
    return false;
  }

  char prevChar = *(--start);
  return prevChar == '.';
}

} // namespace

NS_IMETHODIMP
ServiceWorkerManager::GetAllRegistrations(nsIArray** aResult)
{
  AssertIsOnMainThread();

  nsCOMPtr<nsIMutableArray> array(do_CreateInstance(NS_ARRAY_CONTRACTID));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (auto it1 = mRegistrationInfos.Iter(); !it1.Done(); it1.Next()) {
    for (auto it2 = it1.UserData()->mInfos.Iter(); !it2.Done(); it2.Next()) {
      ServiceWorkerRegistrationInfo* reg = it2.UserData();
      MOZ_ASSERT(reg);

      if (reg->mPendingUninstall) {
        continue;
      }

      array->AppendElement(reg, false);
    }
  }

  array.forget(aResult);
  return NS_OK;
}

// MUST ONLY BE CALLED FROM Remove(), RemoveAll() and RemoveAllRegistrations()!
void
ServiceWorkerManager::ForceUnregister(RegistrationDataPerPrincipal* aRegistrationData,
                                      ServiceWorkerRegistrationInfo* aRegistration)
{
  MOZ_ASSERT(aRegistrationData);
  MOZ_ASSERT(aRegistration);

  RefPtr<ServiceWorkerJobQueue> queue;
  aRegistrationData->mJobQueues.Get(aRegistration->mScope, getter_AddRefs(queue));
  if (queue) {
    queue->CancelAll();
  }

  aRegistrationData->mUpdateTimers.LookupRemoveIf(aRegistration->mScope,
    [] (nsCOMPtr<nsITimer>& aTimer) {
      aTimer->Cancel();
      return true;  // remove it
    });

  // Since Unregister is async, it is ok to call it in an enumeration.
  Unregister(aRegistration->mPrincipal, nullptr, NS_ConvertUTF8toUTF16(aRegistration->mScope));
}

NS_IMETHODIMP
ServiceWorkerManager::RemoveAndPropagate(const nsACString& aHost)
{
  Remove(aHost);
  PropagateRemove(aHost);
  return NS_OK;
}

void
ServiceWorkerManager::Remove(const nsACString& aHost)
{
  AssertIsOnMainThread();

  // We need to postpone this operation in case we don't have an actor because
  // this is needed by the ForceUnregister.
  if (!mActor) {
    RefPtr<nsIRunnable> runnable = new RemoveRunnable(aHost);
    AppendPendingOperation(runnable);
    return;
  }

  for (auto it1 = mRegistrationInfos.Iter(); !it1.Done(); it1.Next()) {
    ServiceWorkerManager::RegistrationDataPerPrincipal* data = it1.UserData();
    for (auto it2 = data->mInfos.Iter(); !it2.Done(); it2.Next()) {
      ServiceWorkerRegistrationInfo* reg = it2.UserData();
      nsCOMPtr<nsIURI> scopeURI;
      nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), it2.Key(),
                              nullptr, nullptr);
      // This way subdomains are also cleared.
      if (NS_SUCCEEDED(rv) && HasRootDomain(scopeURI, aHost)) {
        ForceUnregister(data, reg);
      }
    }
  }
}

void
ServiceWorkerManager::PropagateRemove(const nsACString& aHost)
{
  AssertIsOnMainThread();

  if (!mActor) {
    RefPtr<nsIRunnable> runnable = new PropagateRemoveRunnable(aHost);
    AppendPendingOperation(runnable);
    return;
  }

  mActor->SendPropagateRemove(nsCString(aHost));
}

void
ServiceWorkerManager::RemoveAll()
{
  AssertIsOnMainThread();

  for (auto it1 = mRegistrationInfos.Iter(); !it1.Done(); it1.Next()) {
    ServiceWorkerManager::RegistrationDataPerPrincipal* data = it1.UserData();
    for (auto it2 = data->mInfos.Iter(); !it2.Done(); it2.Next()) {
      ServiceWorkerRegistrationInfo* reg = it2.UserData();
      ForceUnregister(data, reg);
    }
  }
}

void
ServiceWorkerManager::PropagateRemoveAll()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!mActor) {
    RefPtr<nsIRunnable> runnable = new PropagateRemoveAllRunnable();
    AppendPendingOperation(runnable);
    return;
  }

  mActor->SendPropagateRemoveAll();
}

void
ServiceWorkerManager::RemoveAllRegistrations(OriginAttributesPattern* aPattern)
{
  AssertIsOnMainThread();

  MOZ_ASSERT(aPattern);

  for (auto it1 = mRegistrationInfos.Iter(); !it1.Done(); it1.Next()) {
    ServiceWorkerManager::RegistrationDataPerPrincipal* data = it1.UserData();

    // We can use iteration because ForceUnregister (and Unregister) are
    // async. Otherwise doing some R/W operations on an hashtable during
    // iteration will crash.
    for (auto it2 = data->mInfos.Iter(); !it2.Done(); it2.Next()) {
      ServiceWorkerRegistrationInfo* reg = it2.UserData();

      MOZ_ASSERT(reg);
      MOZ_ASSERT(reg->mPrincipal);

      bool matches =
        aPattern->Matches(reg->mPrincipal->OriginAttributesRef());
      if (!matches) {
        continue;
      }

      ForceUnregister(data, reg);
    }
  }
}

NS_IMETHODIMP
ServiceWorkerManager::AddListener(nsIServiceWorkerManagerListener* aListener)
{
  AssertIsOnMainThread();

  if (!aListener || mListeners.Contains(aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  mListeners.AppendElement(aListener);

  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerManager::RemoveListener(nsIServiceWorkerManagerListener* aListener)
{
  AssertIsOnMainThread();

  if (!aListener || !mListeners.Contains(aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  mListeners.RemoveElement(aListener);

  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerManager::ShouldReportToWindow(mozIDOMWindowProxy* aWindow,
                                           const nsACString& aScope,
                                           bool* aResult)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aResult);

  *aResult = false;

  // Get the inner window ID to compare to our document windows below.
  nsCOMPtr<nsPIDOMWindowOuter> targetWin = nsPIDOMWindowOuter::From(aWindow);
  if (NS_WARN_IF(!targetWin)) {
    return NS_OK;
  }

  targetWin = targetWin->GetScriptableTop();
  uint64_t winId = targetWin->WindowID();

  // Check our weak registering document references first.  This way we clear
  // out as many dead weak references as possible when this method is called.
  WeakDocumentList* list = mRegisteringDocuments.Get(aScope);
  if (list) {
    for (int32_t i = list->Length() - 1; i >= 0; --i) {
      nsCOMPtr<nsIDocument> doc = do_QueryReferent(list->ElementAt(i));
      if (!doc) {
        list->RemoveElementAt(i);
        continue;
      }

      if (!doc->IsCurrentActiveDocument()) {
        continue;
      }

      nsCOMPtr<nsPIDOMWindowOuter> win = doc->GetWindow();
      if (!win) {
        continue;
      }

      win = win->GetScriptableTop();

      // Match.  We should report to this window.
      if (win && winId == win->WindowID()) {
        *aResult = true;
        return NS_OK;
      }
    }

    if (list->IsEmpty()) {
      list = nullptr;
      nsAutoPtr<WeakDocumentList> doomed;
      mRegisteringDocuments.RemoveAndForget(aScope, doomed);
    }
  }

  // Examine any windows performing a navigation that we are currently
  // intercepting.
  InterceptionList* intList = mNavigationInterceptions.Get(aScope);
  if (intList) {
    for (uint32_t i = 0; i < intList->Length(); ++i) {
      nsCOMPtr<nsIInterceptedChannel> channel = intList->ElementAt(i);

      nsCOMPtr<nsIChannel> inner;
      nsresult rv = channel->GetChannel(getter_AddRefs(inner));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }

      uint64_t id = nsContentUtils::GetInnerWindowID(inner);
      if (id == 0) {
        continue;
      }

      nsCOMPtr<nsPIDOMWindowInner> win = nsGlobalWindow::GetInnerWindowWithId(id)->AsInner();
      if (!win) {
        continue;
      }

      nsCOMPtr<nsPIDOMWindowOuter> outer = win->GetScriptableTop();

      // Match.  We should report to this window.
      if (outer && winId == outer->WindowID()) {
        *aResult = true;
        return NS_OK;
      }
    }
  }

  // Next examine controlled documents to see if the windows match.
  for (auto iter = mControlledDocuments.Iter(); !iter.Done(); iter.Next()) {
    ServiceWorkerRegistrationInfo* reg = iter.UserData();
    MOZ_ASSERT(reg);
    if (!reg->mScope.Equals(aScope)) {
      continue;
    }

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(iter.Key());
    if (!doc || !doc->IsCurrentActiveDocument()) {
      continue;
    }

    nsCOMPtr<nsPIDOMWindowOuter> win = doc->GetWindow();
    if (!win) {
      continue;
    }

    win = win->GetScriptableTop();

    // Match.  We should report to this window.
    if (win && winId == win->WindowID()) {
      *aResult = true;
      return NS_OK;
    }
  }

  // No match.  We should not report to this window.
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerManager::Observe(nsISupports* aSubject,
                              const char* aTopic,
                              const char16_t* aData)
{
  if (strcmp(aTopic, PURGE_SESSION_HISTORY) == 0) {
    MOZ_ASSERT(XRE_IsParentProcess());
    RemoveAll();
    PropagateRemoveAll();
    return NS_OK;
  }

  if (strcmp(aTopic, PURGE_DOMAIN_DATA) == 0) {
    MOZ_ASSERT(XRE_IsParentProcess());
    nsAutoString domain(aData);
    RemoveAndPropagate(NS_ConvertUTF16toUTF8(domain));
    return NS_OK;
  }

  if (strcmp(aTopic, CLEAR_ORIGIN_DATA) == 0) {
    MOZ_ASSERT(XRE_IsParentProcess());
    OriginAttributesPattern pattern;
    MOZ_ALWAYS_TRUE(pattern.Init(nsAutoString(aData)));

    RemoveAllRegistrations(&pattern);
    return NS_OK;
  }

  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    MaybeStartShutdown();
    return NS_OK;
  }

  MOZ_CRASH("Received message we aren't supposed to be registered for!");
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerManager::PropagateSoftUpdate(JS::Handle<JS::Value> aOriginAttributes,
                                          const nsAString& aScope,
                                          JSContext* aCx)
{
  AssertIsOnMainThread();

  OriginAttributes attrs;
  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  PropagateSoftUpdate(attrs, aScope);
  return NS_OK;
}

void
ServiceWorkerManager::PropagateSoftUpdate(const OriginAttributes& aOriginAttributes,
                                          const nsAString& aScope)
{
  AssertIsOnMainThread();

  if (!mActor) {
    RefPtr<nsIRunnable> runnable =
      new PropagateSoftUpdateRunnable(aOriginAttributes, aScope);
    AppendPendingOperation(runnable);
    return;
  }

  mActor->SendPropagateSoftUpdate(aOriginAttributes, nsString(aScope));
}

NS_IMETHODIMP
ServiceWorkerManager::PropagateUnregister(nsIPrincipal* aPrincipal,
                                          nsIServiceWorkerUnregisterCallback* aCallback,
                                          const nsAString& aScope)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);

  if (!mActor) {
    RefPtr<nsIRunnable> runnable =
      new PropagateUnregisterRunnable(aPrincipal, aCallback, aScope);
    AppendPendingOperation(runnable);
    return NS_OK;
  }

  PrincipalInfo principalInfo;
  if (NS_WARN_IF(NS_FAILED(PrincipalToPrincipalInfo(aPrincipal,
                                                    &principalInfo)))) {
    return NS_ERROR_FAILURE;
  }

  mActor->SendPropagateUnregister(principalInfo, nsString(aScope));

  nsresult rv = Unregister(aPrincipal, aCallback, aScope);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void
ServiceWorkerManager::NotifyListenersOnRegister(
                                        nsIServiceWorkerRegistrationInfo* aInfo)
{
  nsTArray<nsCOMPtr<nsIServiceWorkerManagerListener>> listeners(mListeners);
  for (size_t index = 0; index < listeners.Length(); ++index) {
    listeners[index]->OnRegister(aInfo);
  }
}

void
ServiceWorkerManager::NotifyListenersOnUnregister(
                                        nsIServiceWorkerRegistrationInfo* aInfo)
{
  nsTArray<nsCOMPtr<nsIServiceWorkerManagerListener>> listeners(mListeners);
  for (size_t index = 0; index < listeners.Length(); ++index) {
    listeners[index]->OnUnregister(aInfo);
  }
}

void
ServiceWorkerManager::AddRegisteringDocument(const nsACString& aScope,
                                             nsIDocument* aDoc)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!aScope.IsEmpty());
  MOZ_ASSERT(aDoc);

  WeakDocumentList* list = mRegisteringDocuments.LookupOrAdd(aScope);
  MOZ_ASSERT(list);

  for (int32_t i = list->Length() - 1; i >= 0; --i) {
    nsCOMPtr<nsIDocument> existing = do_QueryReferent(list->ElementAt(i));
    if (!existing) {
      list->RemoveElementAt(i);
      continue;
    }
    if (existing == aDoc) {
      return;
    }
  }

  list->AppendElement(do_GetWeakReference(aDoc));
}

class ServiceWorkerManager::InterceptionReleaseHandle final : public nsISupports
{
  const nsCString mScope;

  // Weak reference to channel is safe, because the channel holds a
  // reference to this object.  Also, the pointer is only used for
  // comparison purposes.
  nsIInterceptedChannel* mChannel;

  ~InterceptionReleaseHandle()
  {
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (swm) {
      swm->RemoveNavigationInterception(mScope, mChannel);
    }
  }

public:
  InterceptionReleaseHandle(const nsACString& aScope,
                            nsIInterceptedChannel* aChannel)
    : mScope(aScope)
    , mChannel(aChannel)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(!aScope.IsEmpty());
    MOZ_ASSERT(mChannel);
  }

  NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS0(ServiceWorkerManager::InterceptionReleaseHandle);

void
ServiceWorkerManager::AddNavigationInterception(const nsACString& aScope,
                                                nsIInterceptedChannel* aChannel)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!aScope.IsEmpty());
  MOZ_ASSERT(aChannel);

  InterceptionList* list =
    mNavigationInterceptions.LookupOrAdd(aScope);
  MOZ_ASSERT(list);
  MOZ_ASSERT(!list->Contains(aChannel));

  nsCOMPtr<nsISupports> releaseHandle =
    new InterceptionReleaseHandle(aScope, aChannel);
  aChannel->SetReleaseHandle(releaseHandle);

  list->AppendElement(aChannel);
}

void
ServiceWorkerManager::RemoveNavigationInterception(const nsACString& aScope,
                                                   nsIInterceptedChannel* aChannel)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aChannel);
  InterceptionList* list =
    mNavigationInterceptions.Get(aScope);
  if (list) {
    MOZ_ALWAYS_TRUE(list->RemoveElement(aChannel));
    MOZ_ASSERT(!list->Contains(aChannel));
    if (list->IsEmpty()) {
      list = nullptr;
      nsAutoPtr<InterceptionList> doomed;
      mNavigationInterceptions.RemoveAndForget(aScope, doomed);
    }
  }
}

class UpdateTimerCallback final : public nsITimerCallback
{
  nsCOMPtr<nsIPrincipal> mPrincipal;
  const nsCString mScope;

  ~UpdateTimerCallback()
  {
  }

public:
  UpdateTimerCallback(nsIPrincipal* aPrincipal, const nsACString& aScope)
    : mPrincipal(aPrincipal)
    , mScope(aScope)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mPrincipal);
    MOZ_ASSERT(!mScope.IsEmpty());
  }

  NS_IMETHOD
  Notify(nsITimer* aTimer) override
  {
    AssertIsOnMainThread();

    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (!swm) {
      // shutting down, do nothing
      return NS_OK;
    }

    swm->UpdateTimerFired(mPrincipal, mScope);
    return NS_OK;
  }

  NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS(UpdateTimerCallback, nsITimerCallback)

bool
ServiceWorkerManager::MayHaveActiveServiceWorkerInstance(ContentParent* aContent,
                                                         nsIPrincipal* aPrincipal)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);

  if (mShuttingDown) {
    return false;
  }

  nsAutoCString scopeKey;
  nsresult rv = PrincipalToScopeKey(aPrincipal, scopeKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  RegistrationDataPerPrincipal* data;
  if (!mRegistrationInfos.Get(scopeKey, &data)) {
    return false;
  }

  return true;
}

void
ServiceWorkerManager::ScheduleUpdateTimer(nsIPrincipal* aPrincipal,
                                          const nsACString& aScope)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(!aScope.IsEmpty());

  if (mShuttingDown) {
    return;
  }

  nsAutoCString scopeKey;
  nsresult rv = PrincipalToScopeKey(aPrincipal, scopeKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  RegistrationDataPerPrincipal* data;
  if (!mRegistrationInfos.Get(scopeKey, &data)) {
    return;
  }

  nsCOMPtr<nsITimer>& timer = data->mUpdateTimers.GetOrInsert(aScope);
  if (timer) {
    // There is already a timer scheduled.  In this case just use the original
    // schedule time.  We don't want to push it out to a later time since that
    // could allow updates to be starved forever if events are continuously
    // fired.
    return;
  }

  timer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    data->mUpdateTimers.Remove(aScope); // another lookup, but very rare
    return;
  }

  nsCOMPtr<nsITimerCallback> callback = new UpdateTimerCallback(aPrincipal,
                                                                aScope);

  const uint32_t UPDATE_DELAY_MS = 1000;

  rv = timer->InitWithCallback(callback, UPDATE_DELAY_MS,
                               nsITimer::TYPE_ONE_SHOT);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    data->mUpdateTimers.Remove(aScope); // another lookup, but very rare
    return;
  }
}

void
ServiceWorkerManager::UpdateTimerFired(nsIPrincipal* aPrincipal,
                                       const nsACString& aScope)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(!aScope.IsEmpty());

  if (mShuttingDown) {
    return;
  }

  // First cleanup the timer.
  nsAutoCString scopeKey;
  nsresult rv = PrincipalToScopeKey(aPrincipal, scopeKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  RegistrationDataPerPrincipal* data;
  if (!mRegistrationInfos.Get(scopeKey, &data)) {
    return;
  }

  data->mUpdateTimers.LookupRemoveIf(aScope,
    [] (nsCOMPtr<nsITimer>& aTimer) {
      aTimer->Cancel();
      return true;  // remove it
    });

  RefPtr<ServiceWorkerRegistrationInfo> registration;
  data->mInfos.Get(aScope, getter_AddRefs(registration));
  if (!registration) {
    return;
  }

  if (!registration->CheckAndClearIfUpdateNeeded()) {
    return;
  }

  OriginAttributes attrs = aPrincipal->OriginAttributesRef();

  SoftUpdate(attrs, aScope);
}

void
ServiceWorkerManager::MaybeSendUnregister(nsIPrincipal* aPrincipal,
                                          const nsACString& aScope)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(!aScope.IsEmpty());

  if (!mActor) {
    return;
  }

  PrincipalInfo principalInfo;
  nsresult rv = PrincipalToPrincipalInfo(aPrincipal, &principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  Unused << mActor->SendUnregister(principalInfo, NS_ConvertUTF8toUTF16(aScope));
}

END_WORKERS_NAMESPACE
