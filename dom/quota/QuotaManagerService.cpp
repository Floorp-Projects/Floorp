/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaManagerService.h"

#include "ActorsChild.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Hal.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Unused.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsIIdleService.h"
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

const char kIdleServiceContractId[] = "@mozilla.org/widget/idleservice;1";

// The number of seconds we will wait after receiving the idle-daily
// notification before beginning maintenance.
const uint32_t kIdleObserverTimeSec = 1;

mozilla::StaticRefPtr<QuotaManagerService> gQuotaManagerService;

mozilla::Atomic<bool> gInitialized(false);
mozilla::Atomic<bool> gClosed(false);

nsresult CheckedPrincipalToPrincipalInfo(nsIPrincipal* aPrincipal,
                                         PrincipalInfo& aPrincipalInfo) {
  MOZ_ASSERT(aPrincipal);

  nsresult rv = PrincipalToPrincipalInfo(aPrincipal, &aPrincipalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!QuotaManager::IsPrincipalInfoValid(aPrincipalInfo))) {
    return NS_ERROR_FAILURE;
  }

  if (aPrincipalInfo.type() != PrincipalInfo::TContentPrincipalInfo &&
      aPrincipalInfo.type() != PrincipalInfo::TSystemPrincipalInfo) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

nsresult GetClearResetOriginParams(nsIPrincipal* aPrincipal,
                                   const nsACString& aPersistenceType,
                                   const nsAString& aClientType, bool aMatchAll,
                                   ClearResetOriginParams& aParams) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  nsresult rv =
      CheckedPrincipalToPrincipalInfo(aPrincipal, aParams.principalInfo());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  Nullable<PersistenceType> persistenceType;
  rv = NullablePersistenceTypeFromText(aPersistenceType, &persistenceType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_INVALID_ARG;
  }

  if (persistenceType.IsNull()) {
    aParams.persistenceTypeIsExplicit() = false;
  } else {
    aParams.persistenceType() = persistenceType.Value();
    aParams.persistenceTypeIsExplicit() = true;
  }

  Nullable<Client::Type> clientType;
  rv = Client::NullableTypeFromText(aClientType, &clientType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_INVALID_ARG;
  }

  if (clientType.IsNull()) {
    aParams.clientTypeIsExplicit() = false;
  } else {
    aParams.clientType() = clientType.Value();
    aParams.clientTypeIsExplicit() = true;
  }

  aParams.matchAll() = aMatchAll;

  return NS_OK;
}

}  // namespace

class QuotaManagerService::PendingRequestInfo {
 protected:
  RefPtr<RequestBase> mRequest;

 public:
  explicit PendingRequestInfo(RequestBase* aRequest) : mRequest(aRequest) {}

  virtual ~PendingRequestInfo() {}

  RequestBase* GetRequest() const { return mRequest; }

  virtual nsresult InitiateRequest(QuotaChild* aActor) = 0;
};

class QuotaManagerService::UsageRequestInfo : public PendingRequestInfo {
  UsageRequestParams mParams;

 public:
  UsageRequestInfo(UsageRequest* aRequest, const UsageRequestParams& aParams)
      : PendingRequestInfo(aRequest), mParams(aParams) {
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(aParams.type() != UsageRequestParams::T__None);
  }

  virtual nsresult InitiateRequest(QuotaChild* aActor) override;
};

class QuotaManagerService::RequestInfo : public PendingRequestInfo {
  RequestParams mParams;

 public:
  RequestInfo(Request* aRequest, const RequestParams& aParams)
      : PendingRequestInfo(aRequest), mParams(aParams) {
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(aParams.type() != RequestParams::T__None);
  }

  virtual nsresult InitiateRequest(QuotaChild* aActor) override;
};

class QuotaManagerService::IdleMaintenanceInfo : public PendingRequestInfo {
  const bool mStart;

 public:
  explicit IdleMaintenanceInfo(bool aStart)
      : PendingRequestInfo(nullptr), mStart(aStart) {}

  virtual nsresult InitiateRequest(QuotaChild* aActor) override;
};

QuotaManagerService::QuotaManagerService()
    : mBackgroundActor(nullptr),
      mBackgroundActorFailed(false),
      mIdleObserverRegistered(false) {
  MOZ_ASSERT(NS_IsMainThread());
}

QuotaManagerService::~QuotaManagerService() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mIdleObserverRegistered);
}

// static
QuotaManagerService* QuotaManagerService::GetOrCreate() {
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
QuotaManagerService* QuotaManagerService::Get() {
  // Does not return an owning reference.
  return gQuotaManagerService;
}

// static
already_AddRefed<QuotaManagerService> QuotaManagerService::FactoryCreate() {
  RefPtr<QuotaManagerService> quotaManagerService = GetOrCreate();
  return quotaManagerService.forget();
}

void QuotaManagerService::ClearBackgroundActor() {
  MOZ_ASSERT(NS_IsMainThread());

  mBackgroundActor = nullptr;
}

void QuotaManagerService::AbortOperationsForProcess(
    ContentParentId aContentParentId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = EnsureBackgroundActor();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (NS_WARN_IF(
          !mBackgroundActor->SendAbortOperationsForProcess(aContentParentId))) {
    return;
  }
}

nsresult QuotaManagerService::Init() {
  MOZ_ASSERT(NS_IsMainThread());

  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (NS_WARN_IF(!observerService)) {
      return NS_ERROR_FAILURE;
    }

    nsresult rv = observerService->AddObserver(
        this, PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

void QuotaManagerService::Destroy() {
  // Setting the closed flag prevents the service from being recreated.
  // Don't set it though if there's no real instance created.
  if (gInitialized && gClosed.exchange(true)) {
    MOZ_ASSERT(false, "Shutdown more than once?!");
  }

  delete this;
}

nsresult QuotaManagerService::EnsureBackgroundActor() {
  MOZ_ASSERT(NS_IsMainThread());

  // Nothing can be done here if we have previously failed to create a
  // background actor.
  if (mBackgroundActorFailed) {
    return NS_ERROR_FAILURE;
  }

  if (!mBackgroundActor) {
    PBackgroundChild* backgroundActor =
        BackgroundChild::GetOrCreateForCurrentThread();
    if (NS_WARN_IF(!backgroundActor)) {
      mBackgroundActorFailed = true;
      return NS_ERROR_FAILURE;
    }

    {
      QuotaChild* actor = new QuotaChild(this);

      mBackgroundActor = static_cast<QuotaChild*>(
          backgroundActor->SendPQuotaConstructor(actor));
    }
  }

  if (!mBackgroundActor) {
    mBackgroundActorFailed = true;
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult QuotaManagerService::InitiateRequest(
    nsAutoPtr<PendingRequestInfo>& aInfo) {
  nsresult rv = EnsureBackgroundActor();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aInfo->InitiateRequest(mBackgroundActor);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void QuotaManagerService::PerformIdleMaintenance() {
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

void QuotaManagerService::RemoveIdleObserver() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (mIdleObserverRegistered) {
    nsCOMPtr<nsIIdleService> idleService =
        do_GetService(kIdleServiceContractId);
    MOZ_ASSERT(idleService);

    // Ignore the return value of RemoveIdleObserver, it may fail if the
    // observer has already been unregistered during shutdown.
    Unused << idleService->RemoveIdleObserver(this, kIdleObserverTimeSec);

    mIdleObserverRegistered = false;
  }
}

NS_IMPL_ADDREF(QuotaManagerService)
NS_IMPL_RELEASE_WITH_DESTROY(QuotaManagerService, Destroy())
NS_IMPL_QUERY_INTERFACE(QuotaManagerService, nsIQuotaManagerService,
                        nsIObserver)

NS_IMETHODIMP
QuotaManagerService::Init(nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  if (NS_WARN_IF(!StaticPrefs::dom_quotaManager_testing())) {
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
QuotaManagerService::InitTemporaryStorage(nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  if (NS_WARN_IF(!StaticPrefs::dom_quotaManager_testing())) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<Request> request = new Request();

  InitTemporaryStorageParams params;

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
    nsIPrincipal* aPrincipal, const nsACString& aPersistenceType,
    nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  if (NS_WARN_IF(!StaticPrefs::dom_quotaManager_testing())) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<Request> request = new Request();

  InitOriginParams params;

  nsresult rv =
      CheckedPrincipalToPrincipalInfo(aPrincipal, params.principalInfo());
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
QuotaManagerService::GetUsage(nsIQuotaUsageCallback* aCallback, bool aGetAll,
                              nsIQuotaUsageRequest** _retval) {
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
                                          bool aFromMemory,
                                          nsIQuotaUsageRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aCallback);

  RefPtr<UsageRequest> request = new UsageRequest(aPrincipal, aCallback);

  OriginUsageParams params;

  nsresult rv =
      CheckedPrincipalToPrincipalInfo(aPrincipal, params.principalInfo());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  params.fromMemory() = aFromMemory;

  nsAutoPtr<PendingRequestInfo> info(new UsageRequestInfo(request, params));

  rv = InitiateRequest(info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::Clear(nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!StaticPrefs::dom_quotaManager_testing())) {
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
QuotaManagerService::ClearStoragesForOriginAttributesPattern(
    const nsAString& aPattern, nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());

  OriginAttributesPattern pattern;
  MOZ_ALWAYS_TRUE(pattern.Init(aPattern));

  RefPtr<Request> request = new Request();

  ClearDataParams params;

  params.pattern() = pattern;

  nsAutoPtr<PendingRequestInfo> info(new RequestInfo(request, params));

  nsresult rv = InitiateRequest(info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::ClearStoragesForPrincipal(
    nsIPrincipal* aPrincipal, const nsACString& aPersistenceType,
    const nsAString& aClientType, bool aClearAll, nsIQuotaRequest** _retval) {
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

  ClearResetOriginParams commonParams;

  nsresult rv = GetClearResetOriginParams(aPrincipal, aPersistenceType,
                                          aClientType, aClearAll, commonParams);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RequestParams params;
  params = ClearOriginParams(commonParams);

  nsAutoPtr<PendingRequestInfo> info(new RequestInfo(request, params));

  rv = InitiateRequest(info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::Reset(nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!StaticPrefs::dom_quotaManager_testing())) {
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
QuotaManagerService::ResetStoragesForPrincipal(
    nsIPrincipal* aPrincipal, const nsACString& aPersistenceType,
    const nsAString& aClientType, bool aResetAll, nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  if (NS_WARN_IF(!StaticPrefs::dom_quotaManager_testing())) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCString suffix;
  aPrincipal->OriginAttributesRef().CreateSuffix(suffix);

  if (NS_WARN_IF(aResetAll && !suffix.IsEmpty())) {
    // The originAttributes should be default originAttributes when the
    // aClearAll flag is set.
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<Request> request = new Request(aPrincipal);

  ClearResetOriginParams commonParams;

  nsresult rv = GetClearResetOriginParams(aPrincipal, aPersistenceType,
                                          aClientType, aResetAll, commonParams);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RequestParams params;
  params = ResetOriginParams(commonParams);

  nsAutoPtr<PendingRequestInfo> info(new RequestInfo(request, params));

  rv = InitiateRequest(info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::Persisted(nsIPrincipal* aPrincipal,
                               nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(_retval);

  RefPtr<Request> request = new Request(aPrincipal);

  PersistedParams params;

  nsresult rv =
      CheckedPrincipalToPrincipalInfo(aPrincipal, params.principalInfo());
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
                             nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(_retval);

  RefPtr<Request> request = new Request(aPrincipal);

  PersistParams params;

  nsresult rv =
      CheckedPrincipalToPrincipalInfo(aPrincipal, params.principalInfo());
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
QuotaManagerService::Estimate(nsIPrincipal* aPrincipal,
                              nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  RefPtr<Request> request = new Request(aPrincipal);

  EstimateParams params;

  nsresult rv =
      CheckedPrincipalToPrincipalInfo(aPrincipal, params.principalInfo());
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
QuotaManagerService::ListInitializedOrigins(nsIQuotaCallback* aCallback,
                                            nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCallback);

  RefPtr<Request> request = new Request(aCallback);

  ListInitializedOriginsParams params;

  nsAutoPtr<PendingRequestInfo> info(new RequestInfo(request, params));

  nsresult rv = InitiateRequest(info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::Observe(nsISupports* aSubject, const char* aTopic,
                             const char16_t* aData) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp(aTopic, PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID)) {
    RemoveIdleObserver();
    return NS_OK;
  }

  if (!strcmp(aTopic, "clear-origin-attributes-data")) {
    nsCOMPtr<nsIQuotaRequest> request;
    nsresult rv = ClearStoragesForOriginAttributesPattern(
        nsDependentString(aData), getter_AddRefs(request));
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

void QuotaManagerService::Notify(const hal::BatteryInformation& aBatteryInfo) {
  // This notification is received when battery data changes. We don't need to
  // deal with this notification.
}

nsresult QuotaManagerService::UsageRequestInfo::InitiateRequest(
    QuotaChild* aActor) {
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

nsresult QuotaManagerService::RequestInfo::InitiateRequest(QuotaChild* aActor) {
  MOZ_ASSERT(aActor);

  auto request = static_cast<Request*>(mRequest.get());

  auto actor = new QuotaRequestChild(request);

  if (!aActor->SendPQuotaRequestConstructor(actor, mParams)) {
    request->SetError(NS_ERROR_FAILURE);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult QuotaManagerService::IdleMaintenanceInfo::InitiateRequest(
    QuotaChild* aActor) {
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

}  // namespace quota
}  // namespace dom
}  // namespace mozilla
