/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaManagerService.h"

#include "ActorsChild.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Hal.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsIIdleService.h"
#include "nsIIPCBackgroundChildCreateCallback.h"
#include "nsIObserverService.h"
#include "nsIScriptSecurityManager.h"
#include "nsXULAppAPI.h"
#include "QuotaManager.h"
#include "QuotaRequests.h"

#define PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID "profile-before-change-qm"

namespace mozilla {
namespace dom {
namespace quota {

using namespace mozilla::ipc;

namespace {

// Preference that is used to enable testing features.
const char kTestingPref[] = "dom.quotaManager.testing";

const char kIdleServiceContractId[] = "@mozilla.org/widget/idleservice;1";

// The number of seconds we will wait after receiving the idle-daily
// notification before beginning maintenance.
const uint32_t kIdleObserverTimeSec = 1;

mozilla::StaticRefPtr<QuotaManagerService> gQuotaManagerService;

mozilla::Atomic<bool> gInitialized(false);
mozilla::Atomic<bool> gClosed(false);
mozilla::Atomic<bool> gTestingMode(false);

void
TestingPrefChangedCallback(const char* aPrefName,
                           void* aClosure)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!strcmp(aPrefName, kTestingPref));
  MOZ_ASSERT(!aClosure);

  gTestingMode = Preferences::GetBool(aPrefName);
}

nsresult
CheckedPrincipalToPrincipalInfo(nsIPrincipal* aPrincipal,
                                PrincipalInfo& aPrincipalInfo)
{
  MOZ_ASSERT(aPrincipal);

  nsresult rv = PrincipalToPrincipalInfo(aPrincipal, &aPrincipalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aPrincipalInfo.type() != PrincipalInfo::TContentPrincipalInfo &&
      aPrincipalInfo.type() != PrincipalInfo::TSystemPrincipalInfo) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

class AbortOperationsRunnable final
  : public Runnable
{
  ContentParentId mContentParentId;

public:
  explicit AbortOperationsRunnable(ContentParentId aContentParentId)
    : mContentParentId(aContentParentId)
  { }

private:
  NS_DECL_NSIRUNNABLE
};

} // namespace

class QuotaManagerService::BackgroundCreateCallback final
  : public nsIIPCBackgroundChildCreateCallback
{
  RefPtr<QuotaManagerService> mService;

public:
  explicit
  BackgroundCreateCallback(QuotaManagerService* aService)
    : mService(aService)
  {
    MOZ_ASSERT(aService);
  }

  NS_DECL_ISUPPORTS

private:
  ~BackgroundCreateCallback()
  { }

  NS_DECL_NSIIPCBACKGROUNDCHILDCREATECALLBACK
};

class QuotaManagerService::PendingRequestInfo
{
protected:
  RefPtr<RequestBase> mRequest;

public:
  explicit PendingRequestInfo(RequestBase* aRequest)
    : mRequest(aRequest)
  { }

  virtual ~PendingRequestInfo()
  { }

  RequestBase*
  GetRequest() const
  {
    return mRequest;
  }

  virtual nsresult
  InitiateRequest(QuotaChild* aActor) = 0;
};

class QuotaManagerService::UsageRequestInfo
  : public PendingRequestInfo
{
  UsageRequestParams mParams;

public:
  UsageRequestInfo(UsageRequest* aRequest,
                   const UsageRequestParams& aParams)
    : PendingRequestInfo(aRequest)
    , mParams(aParams)
  {
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(aParams.type() != UsageRequestParams::T__None);
  }

  virtual nsresult
  InitiateRequest(QuotaChild* aActor) override;
};

class QuotaManagerService::RequestInfo
  : public PendingRequestInfo
{
  RequestParams mParams;

public:
  RequestInfo(Request* aRequest,
              const RequestParams& aParams)
    : PendingRequestInfo(aRequest)
    , mParams(aParams)
  {
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(aParams.type() != RequestParams::T__None);
  }

  virtual nsresult
  InitiateRequest(QuotaChild* aActor) override;
};

class QuotaManagerService::IdleMaintenanceInfo
  : public PendingRequestInfo
{
  const bool mStart;

public:
  explicit IdleMaintenanceInfo(bool aStart)
    : PendingRequestInfo(nullptr)
    , mStart(aStart)
  { }

  virtual nsresult
  InitiateRequest(QuotaChild* aActor) override;
};

QuotaManagerService::QuotaManagerService()
  : mBackgroundActor(nullptr)
  , mBackgroundActorFailed(false)
  , mIdleObserverRegistered(false)
{
  MOZ_ASSERT(NS_IsMainThread());
}

QuotaManagerService::~QuotaManagerService()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mIdleObserverRegistered);
}

// static
QuotaManagerService*
QuotaManagerService::GetOrCreate()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gClosed) {
    MOZ_ASSERT(false, "Calling GetOrCreate() after shutdown!");
    return nullptr;
  }

  if (!gQuotaManagerService) {
    RefPtr<QuotaManagerService> instance(new QuotaManagerService());

    nsresult rv = instance->Init();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    if (gInitialized.exchange(true)) {
      MOZ_ASSERT(false, "Initialized more than once?!");
    }

    gQuotaManagerService = instance;

    ClearOnShutdown(&gQuotaManagerService);
  }

  return gQuotaManagerService;
}

// static
QuotaManagerService*
QuotaManagerService::Get()
{
  // Does not return an owning reference.
  return gQuotaManagerService;
}

// static
QuotaManagerService*
QuotaManagerService::FactoryCreate()
{
  // Returns a raw pointer that carries an owning reference! Lame, but the
  // singleton factory macros force this.
  QuotaManagerService* quotaManagerService = GetOrCreate();
  NS_IF_ADDREF(quotaManagerService);
  return quotaManagerService;
}

void
QuotaManagerService::ClearBackgroundActor()
{
  MOZ_ASSERT(NS_IsMainThread());

  mBackgroundActor = nullptr;
}

void
QuotaManagerService::NoteLiveManager(QuotaManager* aManager)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aManager);

  mBackgroundThread = aManager->OwningThread();
}

void
QuotaManagerService::NoteShuttingDownManager()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  mBackgroundThread = nullptr;
}

void
QuotaManagerService::AbortOperationsForProcess(ContentParentId aContentParentId)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!mBackgroundThread) {
    return;
  }

  RefPtr<AbortOperationsRunnable> runnable =
    new AbortOperationsRunnable(aContentParentId);

  MOZ_ALWAYS_SUCCEEDS(
    mBackgroundThread->Dispatch(runnable, NS_DISPATCH_NORMAL));
}

nsresult
QuotaManagerService::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (NS_WARN_IF(!observerService)) {
      return NS_ERROR_FAILURE;
    }

    nsresult rv =
      observerService->AddObserver(this,
                                   PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID,
                                   false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  Preferences::RegisterCallbackAndCall(TestingPrefChangedCallback,
                                       kTestingPref);

  return NS_OK;
}

void
QuotaManagerService::Destroy()
{
  // Setting the closed flag prevents the service from being recreated.
  // Don't set it though if there's no real instance created.
  if (gInitialized && gClosed.exchange(true)) {
    MOZ_ASSERT(false, "Shutdown more than once?!");
  }

  Preferences::UnregisterCallback(TestingPrefChangedCallback, kTestingPref);

  delete this;
}

nsresult
QuotaManagerService::InitiateRequest(nsAutoPtr<PendingRequestInfo>& aInfo)
{
  // Nothing can be done here if we have previously failed to create a
  // background actor.
  if (mBackgroundActorFailed) {
    return NS_ERROR_FAILURE;
  }

  if (!mBackgroundActor && mPendingRequests.IsEmpty()) {
    if (PBackgroundChild* actor = BackgroundChild::GetForCurrentThread()) {
      BackgroundActorCreated(actor);
    } else {
      // We need to start the sequence to create a background actor for this
      // thread.
      RefPtr<BackgroundCreateCallback> cb = new BackgroundCreateCallback(this);
      if (NS_WARN_IF(!BackgroundChild::GetOrCreateForCurrentThread(cb))) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  // If we already have a background actor then we can start this request now.
  if (mBackgroundActor) {
    nsresult rv = aInfo->InitiateRequest(mBackgroundActor);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    mPendingRequests.AppendElement(aInfo.forget());
  }

  return NS_OK;
}

nsresult
QuotaManagerService::BackgroundActorCreated(PBackgroundChild* aBackgroundActor)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBackgroundActor);
  MOZ_ASSERT(!mBackgroundActor);
  MOZ_ASSERT(!mBackgroundActorFailed);

  {
    QuotaChild* actor = new QuotaChild(this);

    mBackgroundActor =
      static_cast<QuotaChild*>(aBackgroundActor->SendPQuotaConstructor(actor));
  }

  if (NS_WARN_IF(!mBackgroundActor)) {
    BackgroundActorFailed();
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_OK;

  for (uint32_t index = 0, count = mPendingRequests.Length();
       index < count;
       index++) {
    nsAutoPtr<PendingRequestInfo> info(mPendingRequests[index].forget());

    nsresult rv2 = info->InitiateRequest(mBackgroundActor);

    // Warn for every failure, but just return the first failure if there are
    // multiple failures.
    if (NS_WARN_IF(NS_FAILED(rv2)) && NS_SUCCEEDED(rv)) {
      rv = rv2;
    }
  }

  mPendingRequests.Clear();

  return rv;
}

void
QuotaManagerService::BackgroundActorFailed()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mPendingRequests.IsEmpty());
  MOZ_ASSERT(!mBackgroundActor);
  MOZ_ASSERT(!mBackgroundActorFailed);

  mBackgroundActorFailed = true;

  for (uint32_t index = 0, count = mPendingRequests.Length();
       index < count;
       index++) {
    nsAutoPtr<PendingRequestInfo> info(mPendingRequests[index].forget());

    RequestBase* request = info->GetRequest();
    if (request) {
      request->SetError(NS_ERROR_FAILURE);
    }
  }

  mPendingRequests.Clear();
}

void
QuotaManagerService::PerformIdleMaintenance()
{
  using namespace mozilla::hal;

  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  // If we're running on battery power then skip all idle maintenance since we
  // would otherwise be doing lots of disk I/O.
  BatteryInformation batteryInfo;

#ifdef MOZ_WIDGET_ANDROID
  // Android XPCShell doesn't load the AndroidBridge that is needed to make
  // GetCurrentBatteryInformation work...
  if (!QuotaManager::IsRunningXPCShellTests())
#endif
  {
    // In order to give the correct battery level, hal must have registered
    // battery observers.
    RegisterBatteryObserver(this);
    GetCurrentBatteryInformation(&batteryInfo);
    UnregisterBatteryObserver(this);
  }

  // If we're running XPCShell because we always want to be able to test this
  // code so pretend that we're always charging.
  if (QuotaManager::IsRunningXPCShellTests()) {
    batteryInfo.level() = 100;
    batteryInfo.charging() = true;
  }

  if (NS_WARN_IF(!batteryInfo.charging())) {
    return;
  }

  if (QuotaManager::IsRunningXPCShellTests()) {
    // We don't want user activity to impact this code if we're running tests.
    Unused << Observe(nullptr, OBSERVER_TOPIC_IDLE, nullptr);
  } else if (!mIdleObserverRegistered) {
    nsCOMPtr<nsIIdleService> idleService =
      do_GetService(kIdleServiceContractId);
    MOZ_ASSERT(idleService);

    MOZ_ALWAYS_SUCCEEDS(
      idleService->AddIdleObserver(this, kIdleObserverTimeSec));

    mIdleObserverRegistered = true;
  }
}

void
QuotaManagerService::RemoveIdleObserver()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (mIdleObserverRegistered) {
    nsCOMPtr<nsIIdleService> idleService =
      do_GetService(kIdleServiceContractId);
    MOZ_ASSERT(idleService);

    MOZ_ALWAYS_SUCCEEDS(
      idleService->RemoveIdleObserver(this, kIdleObserverTimeSec));

    mIdleObserverRegistered = false;
  }
}

NS_IMPL_ADDREF(QuotaManagerService)
NS_IMPL_RELEASE_WITH_DESTROY(QuotaManagerService, Destroy())
NS_IMPL_QUERY_INTERFACE(QuotaManagerService,
                        nsIQuotaManagerService,
                        nsIObserver)

NS_IMETHODIMP
QuotaManagerService::Init(nsIQuotaRequest** _retval)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  if (NS_WARN_IF(!gTestingMode)) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<Request> request = new Request();

  InitParams params;

  nsAutoPtr<PendingRequestInfo> info(new RequestInfo(request, params));

  nsresult rv = InitiateRequest(info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::InitStoragesForPrincipal(
                                             nsIPrincipal* aPrincipal,
                                             const nsACString& aPersistenceType,
                                             nsIQuotaRequest** _retval)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  if (NS_WARN_IF(!gTestingMode)) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<Request> request = new Request();

  InitOriginParams params;

  nsresult rv = CheckedPrincipalToPrincipalInfo(aPrincipal,
                                                params.principalInfo());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  Nullable<PersistenceType> persistenceType;
  rv = NullablePersistenceTypeFromText(aPersistenceType, &persistenceType);
  if (NS_WARN_IF(NS_FAILED(rv)) || persistenceType.IsNull()) {
    return NS_ERROR_INVALID_ARG;
  }

  params.persistenceType() = persistenceType.Value();

  nsAutoPtr<PendingRequestInfo> info(new RequestInfo(request, params));

  rv = InitiateRequest(info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::GetUsage(nsIQuotaUsageCallback* aCallback,
                              bool aGetAll,
                              nsIQuotaUsageRequest** _retval)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCallback);

  RefPtr<UsageRequest> request = new UsageRequest(aCallback);

  AllUsageParams params;

  params.getAll() = aGetAll;

  nsAutoPtr<PendingRequestInfo> info(new UsageRequestInfo(request, params));

  nsresult rv = InitiateRequest(info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::GetUsageForPrincipal(nsIPrincipal* aPrincipal,
                                          nsIQuotaUsageCallback* aCallback,
                                          bool aGetGroupUsage,
                                          nsIQuotaUsageRequest** _retval)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aCallback);

  RefPtr<UsageRequest> request = new UsageRequest(aPrincipal, aCallback);

  OriginUsageParams params;

  nsresult rv = CheckedPrincipalToPrincipalInfo(aPrincipal,
                                                params.principalInfo());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  params.getGroupUsage() = aGetGroupUsage;

  nsAutoPtr<PendingRequestInfo> info(new UsageRequestInfo(request, params));

  rv = InitiateRequest(info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::Clear(nsIQuotaRequest** _retval)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!gTestingMode)) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<Request> request = new Request();

  ClearAllParams params;

  nsAutoPtr<PendingRequestInfo> info(new RequestInfo(request, params));

  nsresult rv = InitiateRequest(info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::ClearStoragesForPrincipal(nsIPrincipal* aPrincipal,
                                               const nsACString& aPersistenceType,
                                               bool aClearAll,
                                               nsIQuotaRequest** _retval)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  nsCString suffix;
  aPrincipal->OriginAttributesRef().CreateSuffix(suffix);

  if (NS_WARN_IF(aClearAll && !suffix.IsEmpty())) {
    // The originAttributes should be default originAttributes when the
    // aClearAll flag is set.
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<Request> request = new Request(aPrincipal);

  ClearOriginParams params;

  nsresult rv = CheckedPrincipalToPrincipalInfo(aPrincipal,
                                                params.principalInfo());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  Nullable<PersistenceType> persistenceType;
  rv = NullablePersistenceTypeFromText(aPersistenceType, &persistenceType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_INVALID_ARG;
  }

  if (persistenceType.IsNull()) {
    params.persistenceTypeIsExplicit() = false;
  } else {
    params.persistenceType() = persistenceType.Value();
    params.persistenceTypeIsExplicit() = true;
  }

  params.clearAll() = aClearAll;

  nsAutoPtr<PendingRequestInfo> info(new RequestInfo(request, params));

  rv = InitiateRequest(info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::Reset(nsIQuotaRequest** _retval)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!gTestingMode)) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<Request> request = new Request();

  ResetAllParams params;

  nsAutoPtr<PendingRequestInfo> info(new RequestInfo(request, params));

  nsresult rv = InitiateRequest(info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::Persisted(nsIPrincipal* aPrincipal,
                               nsIQuotaRequest** _retval)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(_retval);

  RefPtr<Request> request = new Request(aPrincipal);

  PersistedParams params;

  nsresult rv = CheckedPrincipalToPrincipalInfo(aPrincipal,
                                                params.principalInfo());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoPtr<PendingRequestInfo> info(new RequestInfo(request, params));

  rv = InitiateRequest(info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::Persist(nsIPrincipal* aPrincipal,
                             nsIQuotaRequest** _retval)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(_retval);

  RefPtr<Request> request = new Request(aPrincipal);

  PersistParams params;

  nsresult rv = CheckedPrincipalToPrincipalInfo(aPrincipal,
                                                params.principalInfo());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoPtr<PendingRequestInfo> info(new RequestInfo(request, params));

  rv = InitiateRequest(info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const char16_t* aData)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp(aTopic, PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID)) {
    RemoveIdleObserver();
    return NS_OK;
  }

  if (!strcmp(aTopic, "clear-origin-attributes-data")) {
    RefPtr<Request> request = new Request();

    ClearDataParams params;
    params.pattern() = nsDependentString(aData);

    nsAutoPtr<PendingRequestInfo> info(new RequestInfo(request, params));

    nsresult rv = InitiateRequest(info);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, OBSERVER_TOPIC_IDLE_DAILY)) {
    PerformIdleMaintenance();
    return NS_OK;
  }

  if (!strcmp(aTopic, OBSERVER_TOPIC_IDLE)) {
    nsAutoPtr<PendingRequestInfo> info(
      new IdleMaintenanceInfo(/* aStart */ true));

    nsresult rv = InitiateRequest(info);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, OBSERVER_TOPIC_ACTIVE)) {
    RemoveIdleObserver();

    nsAutoPtr<PendingRequestInfo> info(
      new IdleMaintenanceInfo(/* aStart */ false));

    nsresult rv = InitiateRequest(info);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  MOZ_ASSERT_UNREACHABLE("Should never get here!");
  return NS_OK;
}

void
QuotaManagerService::Notify(const hal::BatteryInformation& aBatteryInfo)
{
  // This notification is received when battery data changes. We don't need to
  // deal with this notification.
}

NS_IMETHODIMP
AbortOperationsRunnable::Run()
{
  AssertIsOnBackgroundThread();

  if (QuotaManager::IsShuttingDown()) {
    return NS_OK;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    return NS_OK;
  }

  quotaManager->AbortOperationsForProcess(mContentParentId);

  return NS_OK;
}

NS_IMPL_ISUPPORTS(QuotaManagerService::BackgroundCreateCallback,
                  nsIIPCBackgroundChildCreateCallback)

void
QuotaManagerService::
BackgroundCreateCallback::ActorCreated(PBackgroundChild* aActor)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(mService);

  RefPtr<QuotaManagerService> service;
  mService.swap(service);

  service->BackgroundActorCreated(aActor);
}

void
QuotaManagerService::
BackgroundCreateCallback::ActorFailed()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mService);

  RefPtr<QuotaManagerService> service;
  mService.swap(service);

  service->BackgroundActorFailed();
}

nsresult
QuotaManagerService::
UsageRequestInfo::InitiateRequest(QuotaChild* aActor)
{
  MOZ_ASSERT(aActor);

  auto request = static_cast<UsageRequest*>(mRequest.get());

  auto actor = new QuotaUsageRequestChild(request);

  if (!aActor->SendPQuotaUsageRequestConstructor(actor, mParams)) {
    request->SetError(NS_ERROR_FAILURE);
    return NS_ERROR_FAILURE;
  }

  request->SetBackgroundActor(actor);

  return NS_OK;
}

nsresult
QuotaManagerService::
RequestInfo::InitiateRequest(QuotaChild* aActor)
{
  MOZ_ASSERT(aActor);

  auto request = static_cast<Request*>(mRequest.get());

  auto actor = new QuotaRequestChild(request);

  if (!aActor->SendPQuotaRequestConstructor(actor, mParams)) {
    request->SetError(NS_ERROR_FAILURE);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
QuotaManagerService::
IdleMaintenanceInfo::InitiateRequest(QuotaChild* aActor)
{
  MOZ_ASSERT(aActor);

  bool result;

  if (mStart) {
    result = aActor->SendStartIdleMaintenance();
  } else {
    result = aActor->SendStopIdleMaintenance();
  }

  if (!result) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

} // namespace quota
} // namespace dom
} // namespace mozilla
