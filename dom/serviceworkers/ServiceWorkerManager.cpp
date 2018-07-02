/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerManager.h"

#include "nsAutoPtr.h"
#include "nsIConsoleService.h"
#include "nsIDocument.h"
#include "nsIEffectiveTLDService.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamLoader.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsINamed.h"
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
#include "mozilla/SystemGroup.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ClientHandle.h"
#include "mozilla/dom/ClientManager.h"
#include "mozilla/dom/ClientSource.h"
#include "mozilla/dom/ConsoleUtils.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/DOMPrefs.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/InternalHeaders.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/NotificationEvent.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/PromiseWindowProxy.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/SharedWorker.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/Unused.h"
#include "mozilla/EnumSet.h"

#include "nsContentUtils.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsQueryObject.h"
#include "nsTArray.h"

#include "ServiceWorker.h"
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
#include "ServiceWorkerUtils.h"

#ifdef PostMessage
#undef PostMessage
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace mozilla {
namespace dom {

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

static_assert(static_cast<uint16_t>(ServiceWorkerUpdateViaCache::Imports) ==
              nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS,
              "nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_*"
              " should match ServiceWorkerUpdateViaCache enumeration.");
static_assert(static_cast<uint16_t>(ServiceWorkerUpdateViaCache::All) ==
              nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_ALL,
              "nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_*"
              " should match ServiceWorkerUpdateViaCache enumeration.");
static_assert(static_cast<uint16_t>(ServiceWorkerUpdateViaCache::None) ==
              nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_NONE,
              "nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_*"
              " should match ServiceWorkerUpdateViaCache enumeration.");

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

  aData.scope() = aRegistration->Scope();

  // TODO: When bug 1426401 is implemented we will need to handle more
  //       than just the active worker here.
  RefPtr<ServiceWorkerInfo> active = aRegistration->GetActive();
  MOZ_ASSERT(active);
  if (NS_WARN_IF(!active)) {
    return NS_ERROR_FAILURE;
  }

  aData.currentWorkerURL() = active->ScriptSpec();
  aData.cacheName() = active->CacheName();
  aData.currentWorkerHandlesFetch() = active->HandlesFetch();

  aData.currentWorkerInstalledTime() = active->GetInstalledTime();
  aData.currentWorkerActivatedTime() = active->GetActivatedTime();

  aData.updateViaCache() =
    static_cast<uint32_t>(aRegistration->GetUpdateViaCache());

  aData.lastUpdateTime() = aRegistration->GetLastUpdateTime();

  MOZ_ASSERT(ServiceWorkerRegistrationDataIsValid(aData));

  return NS_OK;
}

class TeardownRunnable final : public Runnable
{
public:
  explicit TeardownRunnable(ServiceWorkerManagerChild* aActor)
    : Runnable("dom::ServiceWorkerManager::TeardownRunnable")
    , mActor(aActor)
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
      rv = obs->AddObserver(this, CLEAR_ORIGIN_DATA, false /* ownsWeak */);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  PBackgroundChild* actorChild = BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actorChild)) {
    MaybeStartShutdown();
    return;
  }

  PServiceWorkerManagerChild* actor =
    actorChild->SendPServiceWorkerManagerConstructor();
  if (!actor) {
    MaybeStartShutdown();
    return;
  }

  mActor = static_cast<ServiceWorkerManagerChild*>(actor);
}

RefPtr<GenericPromise>
ServiceWorkerManager::StartControllingClient(const ClientInfo& aClientInfo,
                                             ServiceWorkerRegistrationInfo* aRegistrationInfo,
                                             bool aControlClientHandle)
{
  MOZ_DIAGNOSTIC_ASSERT(aRegistrationInfo->GetActive());

  RefPtr<GenericPromise> ref;
  RefPtr<ServiceWorkerManager> self(this);

  const ServiceWorkerDescriptor& active =
    aRegistrationInfo->GetActive()->Descriptor();

  auto entry = mControlledClients.LookupForAdd(aClientInfo.Id());
  if (entry) {
    RefPtr<ServiceWorkerRegistrationInfo> old =
      entry.Data()->mRegistrationInfo.forget();

    if (aControlClientHandle) {
      ref = entry.Data()->mClientHandle->Control(active);
    } else {
      ref = GenericPromise::CreateAndResolve(false, __func__);
    }

    entry.Data()->mRegistrationInfo = aRegistrationInfo;

    if (old != aRegistrationInfo) {
      StopControllingRegistration(old);
      aRegistrationInfo->StartControllingClient();
    }

    Telemetry::Accumulate(Telemetry::SERVICE_WORKER_CONTROLLED_DOCUMENTS, 1);

    // Always check to see if we failed to actually control the client.  In
    // that case removed the client from our list of controlled clients.
    ref->Then(
      SystemGroup::EventTargetFor(TaskCategory::Other), __func__,
      [] (bool) {
        // do nothing on success
      }, [self, aClientInfo] (nsresult aRv) {
        // failed to control, forget about this client
        self->StopControllingClient(aClientInfo);
      });

    return ref;
  }

  RefPtr<ClientHandle> clientHandle =
    ClientManager::CreateHandle(aClientInfo,
                                SystemGroup::EventTargetFor(TaskCategory::Other));

  if (aControlClientHandle) {
    ref = clientHandle->Control(active);
  } else {
    ref = GenericPromise::CreateAndResolve(false, __func__);
  }

  aRegistrationInfo->StartControllingClient();

  entry.OrInsert([&] {
    return new ControlledClientData(clientHandle, aRegistrationInfo);
  });

  clientHandle->OnDetach()->Then(
    SystemGroup::EventTargetFor(TaskCategory::Other), __func__,
    [self, aClientInfo] {
      self->StopControllingClient(aClientInfo);
    });

  Telemetry::Accumulate(Telemetry::SERVICE_WORKER_CONTROLLED_DOCUMENTS, 1);

  // Always check to see if we failed to actually control the client.  In
  // that case removed the client from our list of controlled clients.
  ref->Then(
    SystemGroup::EventTargetFor(TaskCategory::Other), __func__,
    [] (bool) {
      // do nothing on success
    }, [self, aClientInfo] (nsresult aRv) {
      // failed to control, forget about this client
      self->StopControllingClient(aClientInfo);
    });

  return ref;
}

void
ServiceWorkerManager::StopControllingClient(const ClientInfo& aClientInfo)
{
  auto entry = mControlledClients.Lookup(aClientInfo.Id());
  if (!entry) {
    return;
  }

  RefPtr<ServiceWorkerRegistrationInfo> reg =
    entry.Data()->mRegistrationInfo.forget();

  entry.Remove();

  StopControllingRegistration(reg);
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
      obs->RemoveObserver(this, CLEAR_ORIGIN_DATA);
    }
  }

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
  RefPtr<ServiceWorkerRegistrationPromise::Private> mPromise;

  ~ServiceWorkerResolveWindowPromiseOnRegisterCallback()
  {}

  virtual void
  JobFinished(ServiceWorkerJob* aJob, ErrorResult& aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aJob);

    if (aStatus.Failed()) {
      mPromise->Reject(std::move(aStatus), __func__);
      return;
    }

    MOZ_ASSERT(aJob->GetType() == ServiceWorkerJob::Type::Register);
    RefPtr<ServiceWorkerRegisterJob> registerJob =
      static_cast<ServiceWorkerRegisterJob*>(aJob);
    RefPtr<ServiceWorkerRegistrationInfo> reg = registerJob->GetRegistration();

    mPromise->Resolve(reg->Descriptor(), __func__);
  }

public:
  ServiceWorkerResolveWindowPromiseOnRegisterCallback()
    : mPromise(new ServiceWorkerRegistrationPromise::Private(__func__))
  {}

  RefPtr<ServiceWorkerRegistrationPromise>
  Promise() const
  {
    return mPromise;
  }

  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerResolveWindowPromiseOnRegisterCallback, override)
};

namespace {

class PropagateSoftUpdateRunnable final : public Runnable
{
public:
  PropagateSoftUpdateRunnable(const OriginAttributes& aOriginAttributes,
                              const nsAString& aScope)
    : Runnable("dom::ServiceWorkerManager::PropagateSoftUpdateRunnable")
    , mOriginAttributes(aOriginAttributes)
    , mScope(aScope)
  {}

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

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
                     const nsACString& aScope,
                     bool aInternalMethod,
                     GenericPromise::Private* aPromise)
    : CancelableRunnable("dom::ServiceWorkerManager::SoftUpdateRunnable")
    , mAttrs(aOriginAttributes)
    , mScope(aScope)
    , mInternalMethod(aInternalMethod)
    , mPromise(aPromise)
  {}

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

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
    : CancelableRunnable("dom::ServiceWorkerManager::UpdateRunnable")
    , mPrincipal(aPrincipal)
    , mScope(aScope)
    , mCallback(aCallback)
    , mType(aType)
    , mPromise(aPromise)
  {}

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

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
    : CancelableRunnable("dom::ServiceWorkerManager::ResolvePromiseRunnable")
    , mPromise(aPromise)
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

RefPtr<ServiceWorkerRegistrationPromise>
ServiceWorkerManager::Register(const ClientInfo& aClientInfo,
                               const nsACString& aScopeURL,
                               const nsACString& aScriptURL,
                               ServiceWorkerUpdateViaCache aUpdateViaCache)
{
  nsCOMPtr<nsIURI> scopeURI;
  nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), aScopeURL, nullptr, nullptr);
  if (NS_FAILED(rv)) {
    return ServiceWorkerRegistrationPromise::CreateAndReject(rv, __func__);
  }

  nsCOMPtr<nsIURI> scriptURI;
  rv = NS_NewURI(getter_AddRefs(scriptURI), aScriptURL, nullptr, nullptr);
  if (NS_FAILED(rv)) {
    return ServiceWorkerRegistrationPromise::CreateAndReject(rv, __func__);
  }

  rv = ServiceWorkerScopeAndScriptAreValid(aClientInfo, scopeURI, scriptURI);
  if (NS_FAILED(rv)) {
    return ServiceWorkerRegistrationPromise::CreateAndReject(rv, __func__);
  }

  // If the previous validation step passed then we must have a principal.
  nsCOMPtr<nsIPrincipal> principal = aClientInfo.GetPrincipal();

  nsAutoCString scopeKey;
  rv = PrincipalToScopeKey(principal, scopeKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return ServiceWorkerRegistrationPromise::CreateAndReject(rv, __func__);
  }

  RefPtr<ServiceWorkerJobQueue> queue = GetOrCreateJobQueue(scopeKey,
                                                            aScopeURL);

  RefPtr<ServiceWorkerResolveWindowPromiseOnRegisterCallback> cb =
    new ServiceWorkerResolveWindowPromiseOnRegisterCallback();

  nsCOMPtr<nsILoadGroup> loadGroup = do_CreateInstance(NS_LOADGROUP_CONTRACTID);
  RefPtr<ServiceWorkerRegisterJob> job = new ServiceWorkerRegisterJob(
    principal, aScopeURL, aScriptURL, loadGroup,
    static_cast<ServiceWorkerUpdateViaCache>(aUpdateViaCache)
  );

  job->AppendResultCallback(cb);
  queue->ScheduleJob(job);

  MOZ_ASSERT(NS_IsMainThread());
  Telemetry::Accumulate(Telemetry::SERVICE_WORKER_REGISTRATIONS, 1);

  return cb->Promise();
}

/*
 * Implements the async aspects of the getRegistrations algorithm.
 */
class GetRegistrationsRunnable final : public Runnable
{
  const ClientInfo mClientInfo;
  RefPtr<ServiceWorkerRegistrationListPromise::Private> mPromise;
public:
  explicit GetRegistrationsRunnable(const ClientInfo& aClientInfo)
    : Runnable("dom::ServiceWorkerManager::GetRegistrationsRunnable")
    , mClientInfo(aClientInfo)
    , mPromise(new ServiceWorkerRegistrationListPromise::Private(__func__))
  {}

  RefPtr<ServiceWorkerRegistrationListPromise>
  Promise() const
  {
    return mPromise;
  }

  NS_IMETHOD
  Run() override
  {
    auto scopeExit = MakeScopeExit([&] {
      mPromise->Reject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
    });

    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (!swm) {
      return NS_OK;
    }

    nsCOMPtr<nsIPrincipal> principal = mClientInfo.GetPrincipal();
    if (!principal) {
      return NS_OK;
    }

    nsTArray<ServiceWorkerRegistrationDescriptor> array;

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
      scopeExit.release();
      mPromise->Resolve(array, __func__);
      return NS_OK;
    }

    for (uint32_t i = 0; i < data->mOrderedScopes.Length(); ++i) {
      RefPtr<ServiceWorkerRegistrationInfo> info =
        data->mInfos.GetWeak(data->mOrderedScopes[i]);
      if (info->IsPendingUninstall()) {
        continue;
      }

      NS_ConvertUTF8toUTF16 scope(data->mOrderedScopes[i]);

      nsCOMPtr<nsIURI> scopeURI;
      nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), scope, nullptr, nullptr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        break;
      }

      rv = principal->CheckMayLoad(scopeURI, true /* report */,
                                   false /* allowIfInheritsPrincipal */);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }

      array.AppendElement(info->Descriptor());
    }

    scopeExit.release();
    mPromise->Resolve(array, __func__);

    return NS_OK;
  }
};

RefPtr<ServiceWorkerRegistrationListPromise>
ServiceWorkerManager::GetRegistrations(const ClientInfo& aClientInfo) const
{
  RefPtr<GetRegistrationsRunnable> runnable =
    new GetRegistrationsRunnable(aClientInfo);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(runnable));
  return runnable->Promise();;
}

/*
 * Implements the async aspects of the getRegistration algorithm.
 */
class GetRegistrationRunnable final : public Runnable
{
  const ClientInfo mClientInfo;
  RefPtr<ServiceWorkerRegistrationPromise::Private> mPromise;
  nsCString mURL;

public:
  GetRegistrationRunnable(const ClientInfo& aClientInfo,
                          const nsACString& aURL)
    : Runnable("dom::ServiceWorkerManager::GetRegistrationRunnable")
    , mClientInfo(aClientInfo)
    , mPromise(new ServiceWorkerRegistrationPromise::Private(__func__))
    , mURL(aURL)
  {}

  RefPtr<ServiceWorkerRegistrationPromise>
  Promise() const
  {
    return mPromise;
  }

  NS_IMETHOD
  Run() override
  {
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (!swm) {
      mPromise->Reject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
      return NS_OK;
    }

    nsCOMPtr<nsIPrincipal> principal = mClientInfo.GetPrincipal();
    if (!principal) {
      mPromise->Reject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
      return NS_OK;
    }

    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), mURL, nullptr, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPromise->Reject(rv, __func__);
      return NS_OK;
    }

    rv = principal->CheckMayLoad(uri, true /* report */,
                                 false /* allowIfInheritsPrinciple */);
    if (NS_FAILED(rv)) {
      mPromise->Reject(NS_ERROR_DOM_SECURITY_ERR, __func__);
      return NS_OK;
    }

    RefPtr<ServiceWorkerRegistrationInfo> registration =
      swm->GetServiceWorkerRegistrationInfo(principal, uri);

    if (!registration) {
      // Reject with NS_OK means "not found".
      mPromise->Reject(NS_OK, __func__);
      return NS_OK;
    }

    mPromise->Resolve(registration->Descriptor(), __func__);

    return NS_OK;
  }
};

RefPtr<ServiceWorkerRegistrationPromise>
ServiceWorkerManager::GetRegistration(const ClientInfo& aClientInfo,
                                      const nsACString& aURL) const
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<GetRegistrationRunnable> runnable =
    new GetRegistrationRunnable(aClientInfo, aURL);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(runnable));

  return runnable->Promise();
}

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

RefPtr<ServiceWorkerRegistrationPromise>
ServiceWorkerManager::WhenReady(const ClientInfo& aClientInfo)
{
  AssertIsOnMainThread();

  for (auto &prd : mPendingReadyList) {
    if (prd->mClientHandle->Info().Id() == aClientInfo.Id() &&
        prd->mClientHandle->Info().PrincipalInfo() == aClientInfo.PrincipalInfo()) {
      return prd->mPromise;
    }
  }

  RefPtr<ServiceWorkerRegistrationInfo> reg =
    GetServiceWorkerRegistrationInfo(aClientInfo);
  if (reg && reg->GetActive()) {
    return ServiceWorkerRegistrationPromise::CreateAndResolve(reg->Descriptor(),
                                                              __func__);
  }

  nsCOMPtr<nsISerialEventTarget> target =
    SystemGroup::EventTargetFor(TaskCategory::Other);

  RefPtr<ClientHandle> handle = ClientManager::CreateHandle(aClientInfo, target);
  mPendingReadyList.AppendElement(MakeUnique<PendingReadyData>(handle));

  RefPtr<ServiceWorkerManager> self(this);
  handle->OnDetach()->Then( target, __func__,
    [self = std::move(self), aClientInfo] {
      self->RemovePendingReadyPromise(aClientInfo);
    });

  return mPendingReadyList.LastElement()->mPromise;
}

void
ServiceWorkerManager::CheckPendingReadyPromises()
{
  nsTArray<UniquePtr<PendingReadyData>> pendingReadyList;
  mPendingReadyList.SwapElements(pendingReadyList);
  for (uint32_t i = 0; i < pendingReadyList.Length(); ++i) {
    UniquePtr<PendingReadyData> prd(std::move(pendingReadyList[i]));

    RefPtr<ServiceWorkerRegistrationInfo> reg =
      GetServiceWorkerRegistrationInfo(prd->mClientHandle->Info());

    if (reg && reg->GetActive()) {
      prd->mPromise->Resolve(reg->Descriptor(), __func__);
    } else {
      mPendingReadyList.AppendElement(std::move(prd));
    }
  }
}

void
ServiceWorkerManager::RemovePendingReadyPromise(const ClientInfo& aClientInfo)
{
  nsTArray<UniquePtr<PendingReadyData>> pendingReadyList;
  mPendingReadyList.SwapElements(pendingReadyList);
  for (uint32_t i = 0; i < pendingReadyList.Length(); ++i) {
    UniquePtr<PendingReadyData> prd(std::move(pendingReadyList[i]));

    if (prd->mClientHandle->Info().Id() == aClientInfo.Id() &&
        prd->mClientHandle->Info().PrincipalInfo() == aClientInfo.PrincipalInfo()) {
      prd->mPromise->Reject(NS_ERROR_DOM_ABORT_ERR, __func__);
    } else {
      mPendingReadyList.AppendElement(std::move(prd));
    }
  }
}

void
ServiceWorkerManager::NoteInheritedController(const ClientInfo& aClientInfo,
                                              const ServiceWorkerDescriptor& aController)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIPrincipal> principal =
    PrincipalInfoToPrincipal(aController.PrincipalInfo());
  NS_ENSURE_TRUE_VOID(principal);

  nsCOMPtr<nsIURI> scope;
  nsresult rv =
    NS_NewURI(getter_AddRefs(scope), aController.Scope(), nullptr, nullptr);
  NS_ENSURE_SUCCESS_VOID(rv);

  RefPtr<ServiceWorkerRegistrationInfo> registration =
    GetServiceWorkerRegistrationInfo(principal, scope);
  NS_ENSURE_TRUE_VOID(registration);
  NS_ENSURE_TRUE_VOID(registration->GetActive());

  StartControllingClient(aClientInfo, registration, false /* aControlClientHandle */);
}

ServiceWorkerInfo*
ServiceWorkerManager::GetActiveWorkerInfoForScope(const OriginAttributes& aOriginAttributes,
                                                  const nsACString& aScope)
{
  MOZ_ASSERT(NS_IsMainThread());

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
  MOZ_ASSERT(NS_IsMainThread());

  Maybe<ClientInfo> clientInfo(aDocument->GetClientInfo());
  if (clientInfo.isNothing()) {
    return nullptr;
  }

  RefPtr<ServiceWorkerRegistrationInfo> registration;
  GetClientRegistration(clientInfo.ref(), getter_AddRefs(registration));

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
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mCallback);
  }

  void
  JobFinished(ServiceWorkerJob* aJob, ErrorResult& aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());
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

  NS_INLINE_DECL_REFCOUNTING(UnregisterJobCallback, override)
};

} // anonymous namespace

NS_IMETHODIMP
ServiceWorkerManager::Unregister(nsIPrincipal* aPrincipal,
                                 nsIServiceWorkerUnregisterCallback* aCallback,
                                 const nsAString& aScope)
{
  MOZ_ASSERT(NS_IsMainThread());

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
  MOZ_ASSERT(NS_IsMainThread());
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
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aWorker);

  RefPtr<ServiceWorkerRegistrationInfo> reg =
    GetRegistration(aWorker->Principal(), aWorker->Scope());
  if (!reg) {
    return;
  }

  if (reg->GetActive() != aWorker) {
    return;
  }

  if (!reg->IsControllingClients() && reg->IsPendingUninstall()) {
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

    MOZ_ASSERT(NS_IsMainThread());

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
  ConsoleUtils::ReportForServiceWorkerScope(NS_ConvertUTF8toUTF16(aScope),
                                            aMessage,
                                            aFilename,
                                            aLineNumber,
                                            aColumnNumber,
                                            ConsoleUtils::eError);
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
  nsAutoString message;
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
  MOZ_ASSERT(NS_IsMainThread());
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
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIPrincipal> principal =
    PrincipalInfoToPrincipal(aRegistration.principal());
  if (!principal) {
    return;
  }

  RefPtr<ServiceWorkerRegistrationInfo> registration =
    GetRegistration(principal, aRegistration.scope());
  if (!registration) {
    registration =
      CreateNewRegistration(
        aRegistration.scope(),
        principal,
        static_cast<ServiceWorkerUpdateViaCache>(aRegistration.updateViaCache())
      );
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

  nsLoadFlags importsLoadFlags = nsIChannel::LOAD_BYPASS_SERVICE_WORKER;
  importsLoadFlags |=
    aRegistration.updateViaCache() == static_cast<uint16_t>(ServiceWorkerUpdateViaCache::None)
      ? nsIRequest::LOAD_NORMAL
      : nsIRequest::VALIDATE_ALWAYS;

  const nsCString& currentWorkerURL = aRegistration.currentWorkerURL();
  if (!currentWorkerURL.IsEmpty()) {
    registration->SetActive(
      new ServiceWorkerInfo(registration->Principal(),
                            registration->Scope(),
                            registration->Id(),
                            registration->Version(),
                            currentWorkerURL,
                            aRegistration.cacheName(),
                            importsLoadFlags));
    registration->GetActive()->SetHandlesFetch(aRegistration.currentWorkerHandlesFetch());
    registration->GetActive()->SetInstalledTime(aRegistration.currentWorkerInstalledTime());
    registration->GetActive()->SetActivatedTime(aRegistration.currentWorkerActivatedTime());
  }
}

void
ServiceWorkerManager::LoadRegistrations(
                  const nsTArray<ServiceWorkerRegistrationData>& aRegistrations)
{
  MOZ_ASSERT(NS_IsMainThread());

  for (uint32_t i = 0, len = aRegistrations.Length(); i < len; ++i) {
    LoadRegistration(aRegistrations[i]);
  }
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
ServiceWorkerManager::GetServiceWorkerRegistrationInfo(nsPIDOMWindowInner* aWindow) const
{
  MOZ_ASSERT(aWindow);
  nsCOMPtr<nsIDocument> document = aWindow->GetExtantDoc();
  return GetServiceWorkerRegistrationInfo(document);
}

already_AddRefed<ServiceWorkerRegistrationInfo>
ServiceWorkerManager::GetServiceWorkerRegistrationInfo(nsIDocument* aDoc) const
{
  MOZ_ASSERT(aDoc);
  nsCOMPtr<nsIURI> documentURI = aDoc->GetDocumentURI();
  nsCOMPtr<nsIPrincipal> principal = aDoc->NodePrincipal();
  RefPtr<ServiceWorkerRegistrationInfo> reg =
    GetServiceWorkerRegistrationInfo(principal, documentURI);
  if (reg) {
    auto storageAllowed = nsContentUtils::StorageAllowedForDocument(aDoc);
    if (storageAllowed != nsContentUtils::StorageAccess::eAllow) {
      reg = nullptr;
    }
  }
  return reg.forget();
}

already_AddRefed<ServiceWorkerRegistrationInfo>
ServiceWorkerManager::GetServiceWorkerRegistrationInfo(const ClientInfo& aClientInfo) const
{
  nsCOMPtr<nsIPrincipal> principal = aClientInfo.GetPrincipal();
  NS_ENSURE_TRUE(principal, nullptr);

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aClientInfo.URL(),
                          nullptr, nullptr);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return GetServiceWorkerRegistrationInfo(principal, uri);
}

already_AddRefed<ServiceWorkerRegistrationInfo>
ServiceWorkerManager::GetServiceWorkerRegistrationInfo(nsIPrincipal* aPrincipal,
                                                       nsIURI* aURI) const
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
                                                       nsIURI* aURI) const
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
  rv = registration->Principal()->GetOrigin(origin);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  MOZ_ASSERT(origin.Equals(aScopeKey));
#endif

  if (registration->IsPendingUninstall()) {
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

/* static */ nsresult
ServiceWorkerManager::PrincipalInfoToScopeKey(const PrincipalInfo& aPrincipalInfo,
                                              nsACString& aKey)
{
  if (aPrincipalInfo.type() != PrincipalInfo::TContentPrincipalInfo) {
    return NS_ERROR_FAILURE;
  }

  auto content = aPrincipalInfo.get_ContentPrincipalInfo();

  nsAutoCString suffix;
  content.attrs().CreateSuffix(suffix);

  aKey = content.originNoSuffix();
  aKey.Append(suffix);

  return NS_OK;
}

/* static */ void
ServiceWorkerManager::AddScopeAndRegistration(const nsACString& aScope,
                                              ServiceWorkerRegistrationInfo* aInfo)
{
  MOZ_ASSERT(aInfo);
  MOZ_ASSERT(aInfo->Principal());

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    // browser shutdown
    return;
  }

  nsAutoCString scopeKey;
  nsresult rv = swm->PrincipalToScopeKey(aInfo->Principal(), scopeKey);
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
  nsresult rv = swm->PrincipalToScopeKey(aRegistration->Principal(), scopeKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  RegistrationDataPerPrincipal* data;
  if (!swm->mRegistrationInfos.Get(scopeKey, &data)) {
    return;
  }

  if (auto entry = data->mUpdateTimers.Lookup(aRegistration->Scope())) {
    entry.Data()->Cancel();
    entry.Remove();
  }

  // Verify there are no controlled clients for the purged registration.
  for (auto iter = swm->mControlledClients.Iter(); !iter.Done(); iter.Next()) {
    auto& reg = iter.UserData()->mRegistrationInfo;
    if (reg->Scope().Equals(aRegistration->Scope()) &&
        reg->Principal()->Equals(aRegistration->Principal())) {
      MOZ_DIAGNOSTIC_ASSERT(aRegistration->IsCorrupt(),
                            "controlled client when removing non-corrupt registration");
      iter.Remove();
      break;
    }
  }

  RefPtr<ServiceWorkerRegistrationInfo> info;
  data->mInfos.Remove(aRegistration->Scope(), getter_AddRefs(info));
  data->mOrderedScopes.RemoveElement(aRegistration->Scope());
  swm->NotifyListenersOnUnregister(info);

  swm->MaybeRemoveRegistrationInfo(scopeKey);
  aRegistration->NotifyRemoved();
}

void
ServiceWorkerManager::MaybeRemoveRegistrationInfo(const nsACString& aScopeKey)
{
  if (auto entry = mRegistrationInfos.Lookup(aScopeKey)) {
    if (entry.Data()->mOrderedScopes.IsEmpty() &&
        entry.Data()->mJobQueues.Count() == 0) {
      entry.Remove();
    }
  }
}

bool
ServiceWorkerManager::StartControlling(const ClientInfo& aClientInfo,
                                       const ServiceWorkerDescriptor& aServiceWorker)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIPrincipal> principal =
    PrincipalInfoToPrincipal(aServiceWorker.PrincipalInfo());
  NS_ENSURE_TRUE(principal, false);

  nsCOMPtr<nsIURI> scope;
  nsresult rv =
    NS_NewURI(getter_AddRefs(scope), aServiceWorker.Scope(), nullptr, nullptr);
  NS_ENSURE_SUCCESS(rv, false);

  RefPtr<ServiceWorkerRegistrationInfo> registration =
    GetServiceWorkerRegistrationInfo(principal, scope);
  NS_ENSURE_TRUE(registration, false);
  NS_ENSURE_TRUE(registration->GetActive(), false);

  StartControllingClient(aClientInfo, registration);

  return true;
}

void
ServiceWorkerManager::MaybeCheckNavigationUpdate(const ClientInfo& aClientInfo)
{
  MOZ_ASSERT(NS_IsMainThread());
  // We perform these success path navigation update steps when the
  // document tells us its more or less done loading.  This avoids
  // slowing down page load and also lets pages consistently get
  // updatefound events when they fire.
  //
  // 9.8.20 If respondWithEntered is false, then:
  // 9.8.22 Else: (respondWith was entered and succeeded)
  //    If request is a non-subresource request, then: Invoke Soft Update
  //    algorithm.
  ControlledClientData* data = mControlledClients.Get(aClientInfo.Id());
  if (data && data->mRegistrationInfo) {
    data->mRegistrationInfo->MaybeScheduleUpdate();
  }
}

void
ServiceWorkerManager::StopControllingRegistration(ServiceWorkerRegistrationInfo* aRegistration)
{
  aRegistration->StopControllingClient();
  if (aRegistration->IsControllingClients() || !aRegistration->IsIdle()) {
    return;
  }

  if (aRegistration->IsPendingUninstall()) {
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

  aScope = NS_ConvertUTF8toUTF16(r->Scope());
  return NS_OK;
}

namespace {

class ContinueDispatchFetchEventRunnable : public Runnable
{
  RefPtr<ServiceWorkerPrivate> mServiceWorkerPrivate;
  nsCOMPtr<nsIInterceptedChannel> mChannel;
  nsCOMPtr<nsILoadGroup> mLoadGroup;
  bool mIsReload;
public:
  ContinueDispatchFetchEventRunnable(
    ServiceWorkerPrivate* aServiceWorkerPrivate,
    nsIInterceptedChannel* aChannel,
    nsILoadGroup* aLoadGroup,
    bool aIsReload)
    : Runnable("dom::ServiceWorkerManager::ContinueDispatchFetchEventRunnable")
    , mServiceWorkerPrivate(aServiceWorkerPrivate)
    , mChannel(aChannel)
    , mLoadGroup(aLoadGroup)
    , mIsReload(aIsReload)
  {
    MOZ_ASSERT(aServiceWorkerPrivate);
    MOZ_ASSERT(aChannel);
  }

  void
  HandleError()
  {
    MOZ_ASSERT(NS_IsMainThread());
    NS_WARNING("Unexpected error while dispatching fetch event!");
    nsresult rv = mChannel->ResetInterception();
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to resume intercepted network request");
      mChannel->CancelInterception(rv);
    }
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

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

    nsString clientId;
    nsCOMPtr<nsILoadInfo> loadInfo = channel->GetLoadInfo();
    if (loadInfo) {
      Maybe<ClientInfo> clientInfo = loadInfo->GetClientInfo();
      if (clientInfo.isSome()) {
        char buf[NSID_LENGTH];
        clientInfo.ref().Id().ToProvidedString(buf);
        NS_ConvertASCIItoUTF16 uuid(buf);

        // Remove {} and the null terminator
        clientId.Assign(Substring(uuid, 1, NSID_LENGTH - 3));
      }
    }

    rv = mServiceWorkerPrivate->SendFetchEvent(mChannel, mLoadGroup, clientId,
                                               mIsReload);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      HandleError();
    }

    return NS_OK;
  }
};

} // anonymous namespace

void
ServiceWorkerManager::DispatchFetchEvent(nsIInterceptedChannel* aChannel,
                                         ErrorResult& aRv)
{
  MOZ_ASSERT(aChannel);
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIChannel> internalChannel;
  aRv = aChannel->GetChannel(getter_AddRefs(internalChannel));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsCOMPtr<nsILoadGroup> loadGroup;
  aRv = internalChannel->GetLoadGroup(getter_AddRefs(loadGroup));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = internalChannel->GetLoadInfo();
  if (NS_WARN_IF(!loadInfo)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  RefPtr<ServiceWorkerInfo> serviceWorker;

  if (!nsContentUtils::IsNonSubresourceRequest(internalChannel)) {
    const Maybe<ServiceWorkerDescriptor>& controller = loadInfo->GetController();
    if (NS_WARN_IF(controller.isNothing())) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    RefPtr<ServiceWorkerRegistrationInfo> registration =
      GetRegistration(controller.ref().PrincipalInfo(), controller.ref().Scope());
    if (NS_WARN_IF(!registration)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    serviceWorker = registration->GetActive();
    if (NS_WARN_IF(!serviceWorker) ||
        NS_WARN_IF(serviceWorker->Descriptor().Id() != controller.ref().Id())) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
  } else {
    nsCOMPtr<nsIURI> uri;
    aRv = aChannel->GetSecureUpgradedChannelURI(getter_AddRefs(uri));
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    // non-subresource request means the URI contains the principal
    nsCOMPtr<nsIPrincipal> principal =
      BasePrincipal::CreateCodebasePrincipal(uri,
                                             loadInfo->GetOriginAttributes());

    RefPtr<ServiceWorkerRegistrationInfo> registration =
      GetServiceWorkerRegistrationInfo(principal, uri);
    if (NS_WARN_IF(!registration)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    // While we only enter this method if IsAvailable() previously saw
    // an active worker, it is possible for that worker to be removed
    // before we get to this point.  Therefore we must handle a nullptr
    // active worker here.
    serviceWorker = registration->GetActive();
    if (NS_WARN_IF(!serviceWorker)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    // If there is a reserved client it should be marked as controlled before
    // the FetchEvent is dispatched.
    Maybe<ClientInfo> clientInfo = loadInfo->GetReservedClientInfo();

    // Also override the initial about:blank controller since the real
    // network load may be intercepted by a different service worker.  If
    // the intial about:blank has a controller here its simply been
    // inherited from its parent.
    if (clientInfo.isNothing()) {
      clientInfo = loadInfo->GetInitialClientInfo();

      // TODO: We need to handle the case where the initial about:blank is
      //       controlled, but the final document load is not.  Right now
      //       the spec does not really say what to do.  There currently
      //       is no way for the controller to be cleared from a client in
      //       the spec or our implementation.  We may want to force a
      //       new inner window to be created instead of reusing the
      //       initial about:blank global.  See bug 1419620 and the spec
      //       issue here: https://github.com/w3c/ServiceWorker/issues/1232
    }

    if (clientInfo.isSome()) {
      // ClientChannelHelper is not called for STS upgrades that get
      // intercepted by a service worker when interception occurs in
      // the content process.  Therefore the reserved client is not
      // properly cleared in that case leading to a situation where
      // a ClientSource with an http:// principal is controlled by
      // a ServiceWorker with an https:// principal.
      //
      // This does not occur when interception is handled by the
      // simpler InterceptedHttpChannel approach in the parent.
      //
      // As a temporary work around check for this principal mismatch
      // here and perform the ClientChannelHelper's replacement of
      // reserved client automatically.
      if (!XRE_IsParentProcess()) {
        nsCOMPtr<nsIPrincipal> clientPrincipal = clientInfo.ref().GetPrincipal();
        if (!clientPrincipal || !clientPrincipal->Equals(principal)) {
          UniquePtr<ClientSource> reservedClient =
            loadInfo->TakeReservedClientSource();

          nsCOMPtr<nsISerialEventTarget> target =
            reservedClient ? reservedClient->EventTarget()
                           : SystemGroup::EventTargetFor(TaskCategory::Other);

          reservedClient.reset();
          reservedClient = ClientManager::CreateSource(ClientType::Window,
                                                       target,
                                                       principal);

          loadInfo->GiveReservedClientSource(std::move(reservedClient));

          clientInfo = loadInfo->GetReservedClientInfo();
        }
      }

      // First, attempt to mark the reserved client controlled directly.  This
      // will update the controlled status in the ClientManagerService in the
      // parent.  It will also eventually propagate back to the ClientSource.
      StartControllingClient(clientInfo.ref(), registration);
    }

    uint32_t redirectMode = nsIHttpChannelInternal::REDIRECT_MODE_MANUAL;
    nsCOMPtr<nsIHttpChannelInternal> http = do_QueryInterface(internalChannel);
    MOZ_ALWAYS_SUCCEEDS(http->GetRedirectMode(&redirectMode));

    // Synthetic redirects for non-subresource requests with a "follow"
    // redirect mode may switch controllers.  This is basically worker
    // scripts right now.  In this case we need to explicitly clear the
    // controller to avoid assertions on the SetController() below.
    if (redirectMode == nsIHttpChannelInternal::REDIRECT_MODE_FOLLOW) {
      loadInfo->ClearController();
    }

    // But we also note the reserved state on the LoadInfo.  This allows the
    // ClientSource to be updated immediately after the nsIChannel starts.
    // This is necessary to have the correct controller in place for immediate
    // follow-on requests.
    loadInfo->SetController(serviceWorker->Descriptor());
  }

  MOZ_DIAGNOSTIC_ASSERT(serviceWorker);

  nsCOMPtr<nsIRunnable> continueRunnable =
    new ContinueDispatchFetchEventRunnable(serviceWorker->WorkerPrivate(),
                                           aChannel, loadGroup,
                                           loadInfo->GetIsDocshellReload());

  // When this service worker was registered, we also sent down the permissions
  // for the runnable. They should have arrived by now, but we still need to
  // wait for them if they have not.
  nsCOMPtr<nsIRunnable> permissionsRunnable = NS_NewRunnableFunction(
    "dom::ServiceWorkerManager::DispatchFetchEvent", [=]() {
      nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();
      MOZ_ALWAYS_SUCCEEDS(permMgr->WhenPermissionsAvailable(serviceWorker->Principal(),
                                                            continueRunnable));
    });

  nsCOMPtr<nsIUploadChannel2> uploadChannel = do_QueryInterface(internalChannel);

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

nsresult
ServiceWorkerManager::GetClientRegistration(const ClientInfo& aClientInfo,
                                            ServiceWorkerRegistrationInfo** aRegistrationInfo)
{
  ControlledClientData* data = mControlledClients.Get(aClientInfo.Id());
  if (!data || !data->mRegistrationInfo) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // If the document is controlled, the current worker MUST be non-null.
  if (!data->mRegistrationInfo->GetActive()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<ServiceWorkerRegistrationInfo> ref = data->mRegistrationInfo;
  ref.forget(aRegistrationInfo);
  return NS_OK;
}

void
ServiceWorkerManager::SoftUpdate(const OriginAttributes& aOriginAttributes,
                                 const nsACString& aScope)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mShuttingDown) {
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
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mCallback);
  }

  void
  JobFinished(ServiceWorkerJob* aJob, ErrorResult& aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());
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

  NS_INLINE_DECL_REFCOUNTING(UpdateJobCallback, override)
};

} // anonymous namespace

void
ServiceWorkerManager::SoftUpdateInternal(const OriginAttributes& aOriginAttributes,
                                         const nsACString& aScope,
                                         ServiceWorkerUpdateFinishCallback* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());
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
  if (registration->IsPendingUninstall()) {
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
    new ServiceWorkerUpdateJob(principal, registration->Scope(),
                               newest->ScriptSpec(), nullptr,
                               registration->GetUpdateViaCache());

  RefPtr<UpdateJobCallback> cb = new UpdateJobCallback(aCallback);
  job->AppendResultCallback(cb);

  queue->ScheduleJob(job);
}

void
ServiceWorkerManager::Update(nsIPrincipal* aPrincipal,
                             const nsACString& aScope,
                             ServiceWorkerUpdateFinishCallback* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());

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
    new ServiceWorkerUpdateJob(aPrincipal, registration->Scope(),
                               newest->ScriptSpec(), nullptr,
                               registration->GetUpdateViaCache());

  RefPtr<UpdateJobCallback> cb = new UpdateJobCallback(aCallback);
  job->AppendResultCallback(cb);

  queue->ScheduleJob(job);
}

already_AddRefed<GenericPromise>
ServiceWorkerManager::MaybeClaimClient(const ClientInfo& aClientInfo,
                                       ServiceWorkerRegistrationInfo* aWorkerRegistration)
{
  MOZ_DIAGNOSTIC_ASSERT(aWorkerRegistration);

  RefPtr<GenericPromise> ref;

  if (!aWorkerRegistration->GetActive()) {
    ref = GenericPromise::CreateAndReject(NS_ERROR_DOM_INVALID_STATE_ERR,
                                          __func__);
    return ref.forget();
  }

  // Same origin check
  nsCOMPtr<nsIPrincipal> principal(aClientInfo.GetPrincipal());
  if (!aWorkerRegistration->Principal()->Equals(principal)) {
    ref = GenericPromise::CreateAndReject(NS_ERROR_DOM_SECURITY_ERR, __func__);
    return ref.forget();
  }

  // The registration that should be controlling the client
  RefPtr<ServiceWorkerRegistrationInfo> matchingRegistration =
    GetServiceWorkerRegistrationInfo(aClientInfo);

  // The registration currently controlling the client
  RefPtr<ServiceWorkerRegistrationInfo> controllingRegistration;
  GetClientRegistration(aClientInfo, getter_AddRefs(controllingRegistration));

  if (aWorkerRegistration != matchingRegistration ||
      aWorkerRegistration == controllingRegistration) {
    ref = GenericPromise::CreateAndResolve(true, __func__);
    return ref.forget();
  }

  ref = StartControllingClient(aClientInfo, aWorkerRegistration);
  return ref.forget();
}

already_AddRefed<GenericPromise>
ServiceWorkerManager::MaybeClaimClient(const ClientInfo& aClientInfo,
                                       const ServiceWorkerDescriptor& aServiceWorker)
{
  RefPtr<GenericPromise> ref;

  nsCOMPtr<nsIPrincipal> principal = aServiceWorker.GetPrincipal();
  if (!principal) {
    ref = GenericPromise::CreateAndResolve(false, __func__);
    return ref.forget();
  }

  RefPtr<ServiceWorkerRegistrationInfo> registration =
    GetRegistration(principal, aServiceWorker.Scope());

  // While ServiceWorkerManager is distributed across child processes its
  // possible for us to sometimes get a claim for a new worker that has
  // not propagated to this process yet.  For now, simply note that we
  // are done.  The fix for this is to move the SWM to the parent process
  // so there are no consistency errors.
  if (NS_WARN_IF(!registration) || NS_WARN_IF(!registration->GetActive())) {
    ref = GenericPromise::CreateAndResolve(false, __func__);
    return ref.forget();
  }

  ref = MaybeClaimClient(aClientInfo, registration);
  return ref.forget();
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
ServiceWorkerManager::UpdateClientControllers(ServiceWorkerRegistrationInfo* aRegistration)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<ServiceWorkerInfo> activeWorker = aRegistration->GetActive();
  MOZ_DIAGNOSTIC_ASSERT(activeWorker);

  AutoTArray<RefPtr<ClientHandle>, 16> handleList;
  for (auto iter = mControlledClients.Iter(); !iter.Done(); iter.Next()) {
    if (iter.UserData()->mRegistrationInfo != aRegistration) {
      continue;
    }

    handleList.AppendElement(iter.UserData()->mClientHandle);
  }

  // Fire event after iterating mControlledClients is done to prevent
  // modification by reentering from the event handlers during iteration.
  for (auto& handle : handleList) {
    RefPtr<GenericPromise> p = handle->Control(activeWorker->Descriptor());

    RefPtr<ServiceWorkerManager> self = this;

    // If we fail to control the client, then automatically remove it
    // from our list of controlled clients.
    p->Then(
      SystemGroup::EventTargetFor(TaskCategory::Other), __func__,
      [] (bool) {
        // do nothing on success
      }, [self, clientInfo = handle->Info()] (nsresult aRv) {
        // failed to control, forget about this client
        self->StopControllingClient(clientInfo);
      });
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

already_AddRefed<ServiceWorkerRegistrationInfo>
ServiceWorkerManager::GetRegistration(const PrincipalInfo& aPrincipalInfo,
                                      const nsACString& aScope) const
{
  nsAutoCString scopeKey;
  nsresult rv = PrincipalInfoToScopeKey(aPrincipalInfo, scopeKey);
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
ServiceWorkerManager::CreateNewRegistration(
    const nsCString& aScope,
    nsIPrincipal* aPrincipal,
    ServiceWorkerUpdateViaCache aUpdateViaCache)
{
#ifdef DEBUG
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIURI> scopeURI;
  nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), aScope, nullptr, nullptr);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  RefPtr<ServiceWorkerRegistrationInfo> tmp =
    GetRegistration(aPrincipal, aScope);
  MOZ_ASSERT(!tmp);
#endif

  RefPtr<ServiceWorkerRegistrationInfo> registration =
    new ServiceWorkerRegistrationInfo(aScope, aPrincipal, aUpdateViaCache);

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
  if (!newest && HasScope(aRegistration->Principal(), aRegistration->Scope())) {
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
  MOZ_ASSERT(aRegistration->IsPendingUninstall() || !newest);
#endif

  MOZ_ASSERT(HasScope(aRegistration->Principal(), aRegistration->Scope()));

  // When a registration is removed, we must clear its contents since the DOM
  // object may be held by content script.
  aRegistration->Clear();

  RemoveScopeAndRegistration(aRegistration);
}

NS_IMETHODIMP
ServiceWorkerManager::GetAllRegistrations(nsIArray** aResult)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIMutableArray> array(do_CreateInstance(NS_ARRAY_CONTRACTID));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (auto it1 = mRegistrationInfos.Iter(); !it1.Done(); it1.Next()) {
    for (auto it2 = it1.UserData()->mInfos.Iter(); !it2.Done(); it2.Next()) {
      ServiceWorkerRegistrationInfo* reg = it2.UserData();
      MOZ_ASSERT(reg);

      if (reg->IsPendingUninstall()) {
        continue;
      }

      array->AppendElement(reg);
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
  aRegistrationData->mJobQueues.Get(aRegistration->Scope(), getter_AddRefs(queue));
  if (queue) {
    queue->CancelAll();
  }

  if (auto entry = aRegistrationData->mUpdateTimers.Lookup(aRegistration->Scope())) {
    entry.Data()->Cancel();
    entry.Remove();
  }

  // Since Unregister is async, it is ok to call it in an enumeration.
  Unregister(aRegistration->Principal(), nullptr, NS_ConvertUTF8toUTF16(aRegistration->Scope()));
}

void
ServiceWorkerManager::Remove(const nsACString& aHost)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIEffectiveTLDService> tldService =
    do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  if (NS_WARN_IF(!tldService)) {
    return;
  }

  for (auto it1 = mRegistrationInfos.Iter(); !it1.Done(); it1.Next()) {
    ServiceWorkerManager::RegistrationDataPerPrincipal* data = it1.UserData();
    for (auto it2 = data->mInfos.Iter(); !it2.Done(); it2.Next()) {
      ServiceWorkerRegistrationInfo* reg = it2.UserData();
      nsCOMPtr<nsIURI> scopeURI;
      nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), it2.Key(),
                              nullptr, nullptr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }

      nsAutoCString host;
      rv = scopeURI->GetHost(host);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }

      // This way subdomains are also cleared.
      bool hasRootDomain = false;
      rv = tldService->HasRootDomain(host, aHost, &hasRootDomain);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }

      if (hasRootDomain) {
        ForceUnregister(data, reg);
      }
    }
  }
}

void
ServiceWorkerManager::PropagateRemove(const nsACString& aHost)
{
  MOZ_ASSERT(NS_IsMainThread());
  mActor->SendPropagateRemove(nsCString(aHost));
}

void
ServiceWorkerManager::RemoveAll()
{
  MOZ_ASSERT(NS_IsMainThread());

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
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());
  mActor->SendPropagateRemoveAll();
}

void
ServiceWorkerManager::RemoveAllRegistrations(OriginAttributesPattern* aPattern)
{
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(aPattern);

  for (auto it1 = mRegistrationInfos.Iter(); !it1.Done(); it1.Next()) {
    ServiceWorkerManager::RegistrationDataPerPrincipal* data = it1.UserData();

    // We can use iteration because ForceUnregister (and Unregister) are
    // async. Otherwise doing some R/W operations on an hashtable during
    // iteration will crash.
    for (auto it2 = data->mInfos.Iter(); !it2.Done(); it2.Next()) {
      ServiceWorkerRegistrationInfo* reg = it2.UserData();

      MOZ_ASSERT(reg);
      MOZ_ASSERT(reg->Principal());

      bool matches =
        aPattern->Matches(reg->Principal()->OriginAttributesRef());
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
  MOZ_ASSERT(NS_IsMainThread());

  if (!aListener || mListeners.Contains(aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  mListeners.AppendElement(aListener);

  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerManager::RemoveListener(nsIServiceWorkerManagerListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aListener || !mListeners.Contains(aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  mListeners.RemoveElement(aListener);

  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerManager::Observe(nsISupports* aSubject,
                              const char* aTopic,
                              const char16_t* aData)
{
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
  MOZ_ASSERT(NS_IsMainThread());

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
  MOZ_ASSERT(NS_IsMainThread());
  mActor->SendPropagateSoftUpdate(aOriginAttributes, nsString(aScope));
}

NS_IMETHODIMP
ServiceWorkerManager::PropagateUnregister(nsIPrincipal* aPrincipal,
                                          nsIServiceWorkerUnregisterCallback* aCallback,
                                          const nsAString& aScope)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

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

class UpdateTimerCallback final : public nsITimerCallback
                                , public nsINamed
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
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mPrincipal);
    MOZ_ASSERT(!mScope.IsEmpty());
  }

  NS_IMETHOD
  Notify(nsITimer* aTimer) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (!swm) {
      // shutting down, do nothing
      return NS_OK;
    }

    swm->UpdateTimerFired(mPrincipal, mScope);
    return NS_OK;
  }

  NS_IMETHOD
  GetName(nsACString& aName) override
  {
    aName.AssignLiteral("UpdateTimerCallback");
    return NS_OK;
  }

  NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS(UpdateTimerCallback, nsITimerCallback, nsINamed)

bool
ServiceWorkerManager::MayHaveActiveServiceWorkerInstance(ContentParent* aContent,
                                                         nsIPrincipal* aPrincipal)
{
  MOZ_ASSERT(NS_IsMainThread());
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
  MOZ_ASSERT(NS_IsMainThread());
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

  nsCOMPtr<nsITimerCallback> callback = new UpdateTimerCallback(aPrincipal,
                                                                aScope);

  const uint32_t UPDATE_DELAY_MS = 1000;

  // Label with SystemGroup because UpdateTimerCallback only sends an IPC message
  // (PServiceWorkerUpdaterConstructor) without touching any web contents.
  rv = NS_NewTimerWithCallback(getter_AddRefs(timer),
                               callback, UPDATE_DELAY_MS,
                               nsITimer::TYPE_ONE_SHOT,
                               SystemGroup::EventTargetFor(TaskCategory::Other));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    data->mUpdateTimers.Remove(aScope); // another lookup, but very rare
    return;
  }
}

void
ServiceWorkerManager::UpdateTimerFired(nsIPrincipal* aPrincipal,
                                       const nsACString& aScope)
{
  MOZ_ASSERT(NS_IsMainThread());
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

  if (auto entry = data->mUpdateTimers.Lookup(aScope)) {
    entry.Data()->Cancel();
    entry.Remove();
  }

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
  MOZ_ASSERT(NS_IsMainThread());
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

} // namespace dom
} // namespace mozilla
