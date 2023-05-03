/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBFactory.h"

#include "BackgroundChildImpl.h"
#include "IDBRequest.h"
#include "IndexedDatabaseManager.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/IDBFactoryBinding.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackground.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StorageAccess.h"
#include "mozilla/Telemetry.h"
#include "nsAboutProtocolUtils.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsIAboutModule.h"
#include "nsILoadContext.h"
#include "nsIURI.h"
#include "nsIUUIDGenerator.h"
#include "nsIWebNavigation.h"
#include "nsNetUtil.h"
#include "nsSandboxFlags.h"
#include "nsServiceManagerUtils.h"
#include "ProfilerHelpers.h"
#include "ReportInternalError.h"
#include "ThreadLocal.h"

// Include this last to avoid path problems on Windows.
#include "ActorsChild.h"

namespace mozilla::dom {

using namespace mozilla::dom::indexedDB;
using namespace mozilla::dom::quota;
using namespace mozilla::ipc;

namespace {

Telemetry::LABELS_IDB_CUSTOM_OPEN_WITH_OPTIONS_COUNT IdentifyPrincipalType(
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo) {
  switch (aPrincipalInfo.type()) {
    case PrincipalInfo::TSystemPrincipalInfo:
      return Telemetry::LABELS_IDB_CUSTOM_OPEN_WITH_OPTIONS_COUNT::system;
    case PrincipalInfo::TContentPrincipalInfo: {
      const ContentPrincipalInfo& info =
          aPrincipalInfo.get_ContentPrincipalInfo();

      nsCOMPtr<nsIURI> uri;

      if (NS_WARN_IF(NS_FAILED(NS_NewURI(getter_AddRefs(uri), info.spec())))) {
        // This could be discriminated as an extra error value, but this is
        // extremely unlikely to fail, so we just misuse ContentOther
        return Telemetry::LABELS_IDB_CUSTOM_OPEN_WITH_OPTIONS_COUNT::
            content_other;
      }

      // TODO Are there constants defined for the schemes somewhere?
      if (uri->SchemeIs("file")) {
        return Telemetry::LABELS_IDB_CUSTOM_OPEN_WITH_OPTIONS_COUNT::
            content_file;
      }
      if (uri->SchemeIs("http") || uri->SchemeIs("https")) {
        return Telemetry::LABELS_IDB_CUSTOM_OPEN_WITH_OPTIONS_COUNT::
            content_http_https;
      }
      if (uri->SchemeIs("moz-extension")) {
        return Telemetry::LABELS_IDB_CUSTOM_OPEN_WITH_OPTIONS_COUNT::
            content_moz_ext;
      }
      if (uri->SchemeIs("about")) {
        return Telemetry::LABELS_IDB_CUSTOM_OPEN_WITH_OPTIONS_COUNT::
            content_about;
      }
      return Telemetry::LABELS_IDB_CUSTOM_OPEN_WITH_OPTIONS_COUNT::
          content_other;
    }
    case PrincipalInfo::TExpandedPrincipalInfo:
      return Telemetry::LABELS_IDB_CUSTOM_OPEN_WITH_OPTIONS_COUNT::expanded;
    default:
      return Telemetry::LABELS_IDB_CUSTOM_OPEN_WITH_OPTIONS_COUNT::other;
  }
}

}  // namespace

struct IDBFactory::PendingRequestInfo {
  RefPtr<IDBOpenDBRequest> mRequest;
  FactoryRequestParams mParams;

  PendingRequestInfo(IDBOpenDBRequest* aRequest,
                     const FactoryRequestParams& aParams)
      : mRequest(aRequest), mParams(aParams) {
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(aParams.type() != FactoryRequestParams::T__None);
  }
};

IDBFactory::IDBFactory(const IDBFactoryGuard&)
    : mBackgroundActor(nullptr),
      mInnerWindowID(0),
      mActiveTransactionCount(0),
      mActiveDatabaseCount(0),
      mBackgroundActorFailed(false),
      mPrivateBrowsingMode(false) {
  AssertIsOnOwningThread();
}

IDBFactory::~IDBFactory() {
  MOZ_ASSERT_IF(mBackgroundActorFailed, !mBackgroundActor);

  if (mBackgroundActor) {
    mBackgroundActor->SendDeleteMeInternal();
    MOZ_ASSERT(!mBackgroundActor, "SendDeleteMeInternal should have cleared!");
  }
}

// static
Result<RefPtr<IDBFactory>, nsresult> IDBFactory::CreateForWindow(
    nsPIDOMWindowInner* aWindow) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = AllowedForWindowInternal(aWindow, &principal);

  if (rv == NS_ERROR_DOM_NOT_SUPPORTED_ERR) {
    NS_WARNING("IndexedDB is not permitted in a third-party window.");
    return RefPtr<IDBFactory>{};
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (rv == NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR) {
      IDB_REPORT_INTERNAL_ERR();
    }
    return Err(rv);
  }

  MOZ_ASSERT(principal);

  auto principalInfo = MakeUnique<PrincipalInfo>();
  rv = PrincipalToPrincipalInfo(principal, principalInfo.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    IDB_REPORT_INTERNAL_ERR();
    return Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  MOZ_ASSERT(principalInfo->type() == PrincipalInfo::TContentPrincipalInfo ||
             principalInfo->type() == PrincipalInfo::TSystemPrincipalInfo);

  if (NS_WARN_IF(!QuotaManager::IsPrincipalInfoValid(*principalInfo))) {
    IDB_REPORT_INTERNAL_ERR();
    return Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(aWindow);
  nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(webNav);

  auto factory = MakeRefPtr<IDBFactory>(IDBFactoryGuard{});
  factory->mPrincipalInfo = std::move(principalInfo);

  factory->mGlobal = do_QueryInterface(aWindow);
  MOZ_ASSERT(factory->mGlobal);

  factory->mBrowserChild = BrowserChild::GetFrom(aWindow);
  factory->mEventTarget =
      nsGlobalWindowInner::Cast(aWindow)->EventTargetFor(TaskCategory::Other);
  factory->mInnerWindowID = aWindow->WindowID();
  factory->mPrivateBrowsingMode =
      loadContext && loadContext->UsePrivateBrowsing();

  return factory;
}

// static
Result<RefPtr<IDBFactory>, nsresult> IDBFactory::CreateForMainThreadJS(
    nsIGlobalObject* aGlobal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aGlobal);

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aGlobal);
  if (NS_WARN_IF(!sop)) {
    return Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  auto principalInfo = MakeUnique<PrincipalInfo>();
  nsIPrincipal* principal = sop->GetEffectiveStoragePrincipal();
  MOZ_ASSERT(principal);
  bool isSystem;
  if (!AllowedForPrincipal(principal, &isSystem)) {
    return Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  nsresult rv = PrincipalToPrincipalInfo(principal, principalInfo.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return Err(rv);
  }

  if (NS_WARN_IF(!QuotaManager::IsPrincipalInfoValid(*principalInfo))) {
    return Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  return CreateForMainThreadJSInternal(aGlobal, std::move(principalInfo));
}

// static
Result<RefPtr<IDBFactory>, nsresult> IDBFactory::CreateForWorker(
    nsIGlobalObject* aGlobal, const PrincipalInfo& aPrincipalInfo,
    uint64_t aInnerWindowID) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(aPrincipalInfo.type() != PrincipalInfo::T__None);

  return CreateInternal(aGlobal, MakeUnique<PrincipalInfo>(aPrincipalInfo),
                        aInnerWindowID);
}

// static
Result<RefPtr<IDBFactory>, nsresult> IDBFactory::CreateForMainThreadJSInternal(
    nsIGlobalObject* aGlobal, UniquePtr<PrincipalInfo> aPrincipalInfo) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(aPrincipalInfo);

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::GetOrCreate();
  if (NS_WARN_IF(!mgr)) {
    IDB_REPORT_INTERNAL_ERR();
    return Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  return CreateInternal(aGlobal, std::move(aPrincipalInfo),
                        /* aInnerWindowID */ 0);
}

// static
Result<RefPtr<IDBFactory>, nsresult> IDBFactory::CreateInternal(
    nsIGlobalObject* aGlobal, UniquePtr<PrincipalInfo> aPrincipalInfo,
    uint64_t aInnerWindowID) {
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(aPrincipalInfo);
  MOZ_ASSERT(aPrincipalInfo->type() != PrincipalInfo::T__None);

  if (aPrincipalInfo->type() != PrincipalInfo::TContentPrincipalInfo &&
      aPrincipalInfo->type() != PrincipalInfo::TSystemPrincipalInfo) {
    NS_WARNING("IndexedDB not allowed for this principal!");
    return RefPtr<IDBFactory>{};
  }

  auto factory = MakeRefPtr<IDBFactory>(IDBFactoryGuard{});
  factory->mPrincipalInfo = std::move(aPrincipalInfo);
  factory->mGlobal = aGlobal;
  factory->mEventTarget = GetCurrentSerialEventTarget();
  factory->mInnerWindowID = aInnerWindowID;

  return factory;
}

// static
bool IDBFactory::AllowedForWindow(nsPIDOMWindowInner* aWindow) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  return !NS_WARN_IF(NS_FAILED(AllowedForWindowInternal(aWindow, nullptr)));
}

// static
nsresult IDBFactory::AllowedForWindowInternal(
    nsPIDOMWindowInner* aWindow, nsCOMPtr<nsIPrincipal>* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  if (NS_WARN_IF(!IndexedDatabaseManager::GetOrCreate())) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  StorageAccess access = StorageAllowedForWindow(aWindow);

  // the factory callsite records whether the browser is in private browsing.
  // and thus we don't have to respect that setting here. IndexedDB has no
  // concept of session-local storage, and thus ignores it.
  if (access == StorageAccess::eDeny) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (ShouldPartitionStorage(access) &&
      !StoragePartitioningEnabled(
          access, aWindow->GetExtantDoc()->CookieJarSettings())) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
  MOZ_ASSERT(sop);

  nsCOMPtr<nsIPrincipal> principal = sop->GetEffectiveStoragePrincipal();
  if (NS_WARN_IF(!principal)) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  if (principal->IsSystemPrincipal()) {
    *aPrincipal = std::move(principal);
    return NS_OK;
  }

  // About URIs shouldn't be able to access IndexedDB unless they have the
  // nsIAboutModule::ENABLE_INDEXED_DB flag set on them.

  if (principal->SchemeIs("about")) {
    uint32_t flags;
    if (NS_SUCCEEDED(principal->GetAboutModuleFlags(&flags))) {
      if (!(flags & nsIAboutModule::ENABLE_INDEXED_DB)) {
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      }
    } else {
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }
  }

  if (aPrincipal) {
    *aPrincipal = std::move(principal);
  }
  return NS_OK;
}

// static
bool IDBFactory::AllowedForPrincipal(nsIPrincipal* aPrincipal,
                                     bool* aIsSystemPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  if (NS_WARN_IF(!IndexedDatabaseManager::GetOrCreate())) {
    return false;
  }

  if (aPrincipal->IsSystemPrincipal()) {
    if (aIsSystemPrincipal) {
      *aIsSystemPrincipal = true;
    }
    return true;
  }

  if (aIsSystemPrincipal) {
    *aIsSystemPrincipal = false;
  }

  return !aPrincipal->GetIsNullPrincipal();
}

void IDBFactory::UpdateActiveTransactionCount(int32_t aDelta) {
  AssertIsOnOwningThread();
  MOZ_DIAGNOSTIC_ASSERT(aDelta > 0 || (mActiveTransactionCount + aDelta) <
                                          mActiveTransactionCount);
  mActiveTransactionCount += aDelta;
}

void IDBFactory::UpdateActiveDatabaseCount(int32_t aDelta) {
  AssertIsOnOwningThread();
  MOZ_DIAGNOSTIC_ASSERT(aDelta > 0 ||
                        (mActiveDatabaseCount + aDelta) < mActiveDatabaseCount);
  mActiveDatabaseCount += aDelta;

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mGlobal);
  if (window) {
    window->UpdateActiveIndexedDBDatabaseCount(aDelta);
  }
}

bool IDBFactory::IsChrome() const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mPrincipalInfo);

  return mPrincipalInfo->type() == PrincipalInfo::TSystemPrincipalInfo;
}

RefPtr<IDBOpenDBRequest> IDBFactory::Open(JSContext* aCx,
                                          const nsAString& aName,
                                          uint64_t aVersion,
                                          CallerType aCallerType,
                                          ErrorResult& aRv) {
  return OpenInternal(aCx,
                      /* aPrincipal */ nullptr, aName,
                      Optional<uint64_t>(aVersion),
                      /* aDeleting */ false, aCallerType, aRv);
}

RefPtr<IDBOpenDBRequest> IDBFactory::Open(JSContext* aCx,
                                          const nsAString& aName,
                                          const IDBOpenDBOptions& aOptions,
                                          CallerType aCallerType,
                                          ErrorResult& aRv) {
  // This overload is nonstandard, see bug 1275496.
  // Ignore calls with empty options for telemetry of usage count.
  // Unfortunately, we cannot distinguish between the use of the method with
  // only a single argument (which actually is a standard overload we don't want
  // to count) an empty dictionary passed explicitly (which is the custom
  // overload we would like to count). However, we assume that the latter is so
  // rare that it can be neglected.
  if (aOptions.IsAnyMemberPresent()) {
    Telemetry::AccumulateCategorical(IdentifyPrincipalType(*mPrincipalInfo));
  }

  return OpenInternal(aCx,
                      /* aPrincipal */ nullptr, aName, aOptions.mVersion,
                      /* aDeleting */ false, aCallerType, aRv);
}

RefPtr<IDBOpenDBRequest> IDBFactory::DeleteDatabase(
    JSContext* aCx, const nsAString& aName, const IDBOpenDBOptions& aOptions,
    CallerType aCallerType, ErrorResult& aRv) {
  return OpenInternal(aCx,
                      /* aPrincipal */ nullptr, aName, Optional<uint64_t>(),
                      /* aDeleting */ true, aCallerType, aRv);
}

int16_t IDBFactory::Cmp(JSContext* aCx, JS::Handle<JS::Value> aFirst,
                        JS::Handle<JS::Value> aSecond, ErrorResult& aRv) {
  Key first, second;
  auto result = first.SetFromJSVal(aCx, aFirst);
  if (result.isErr()) {
    aRv = result.unwrapErr().ExtractErrorResult(
        InvalidMapsTo<NS_ERROR_DOM_INDEXEDDB_DATA_ERR>);
    return 0;
  }

  result = second.SetFromJSVal(aCx, aSecond);
  if (result.isErr()) {
    aRv = result.unwrapErr().ExtractErrorResult(
        InvalidMapsTo<NS_ERROR_DOM_INDEXEDDB_DATA_ERR>);
    return 0;
  }

  if (first.IsUnset() || second.IsUnset()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
    return 0;
  }

  return Key::CompareKeys(first, second);
}

RefPtr<IDBOpenDBRequest> IDBFactory::OpenForPrincipal(
    JSContext* aCx, nsIPrincipal* aPrincipal, const nsAString& aName,
    uint64_t aVersion, SystemCallerGuarantee aGuarantee, ErrorResult& aRv) {
  MOZ_ASSERT(aPrincipal);
  if (!NS_IsMainThread()) {
    MOZ_CRASH(
        "Figure out security checks for workers!  What's this aPrincipal "
        "we have on a worker thread?");
  }

  return OpenInternal(aCx, aPrincipal, aName, Optional<uint64_t>(aVersion),
                      /* aDeleting */ false, aGuarantee, aRv);
}

RefPtr<IDBOpenDBRequest> IDBFactory::OpenForPrincipal(
    JSContext* aCx, nsIPrincipal* aPrincipal, const nsAString& aName,
    const IDBOpenDBOptions& aOptions, SystemCallerGuarantee aGuarantee,
    ErrorResult& aRv) {
  MOZ_ASSERT(aPrincipal);
  if (!NS_IsMainThread()) {
    MOZ_CRASH(
        "Figure out security checks for workers!  What's this aPrincipal "
        "we have on a worker thread?");
  }

  return OpenInternal(aCx, aPrincipal, aName, aOptions.mVersion,
                      /* aDeleting */ false, aGuarantee, aRv);
}

RefPtr<IDBOpenDBRequest> IDBFactory::DeleteForPrincipal(
    JSContext* aCx, nsIPrincipal* aPrincipal, const nsAString& aName,
    const IDBOpenDBOptions& aOptions, SystemCallerGuarantee aGuarantee,
    ErrorResult& aRv) {
  MOZ_ASSERT(aPrincipal);
  if (!NS_IsMainThread()) {
    MOZ_CRASH(
        "Figure out security checks for workers!  What's this aPrincipal "
        "we have on a worker thread?");
  }

  return OpenInternal(aCx, aPrincipal, aName, Optional<uint64_t>(),
                      /* aDeleting */ true, aGuarantee, aRv);
}

RefPtr<IDBOpenDBRequest> IDBFactory::OpenInternal(
    JSContext* aCx, nsIPrincipal* aPrincipal, const nsAString& aName,
    const Optional<uint64_t>& aVersion, bool aDeleting, CallerType aCallerType,
    ErrorResult& aRv) {
  if (NS_WARN_IF(!mGlobal)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  CommonFactoryRequestParams commonParams;

  PrincipalInfo& principalInfo = commonParams.principalInfo();

  if (aPrincipal) {
    if (!NS_IsMainThread()) {
      MOZ_CRASH(
          "Figure out security checks for workers!  What's this "
          "aPrincipal we have on a worker thread?");
    }
    MOZ_ASSERT(aCallerType == CallerType::System);
    MOZ_DIAGNOSTIC_ASSERT(mPrivateBrowsingMode ==
                          (aPrincipal->GetPrivateBrowsingId() > 0));

    if (NS_WARN_IF(
            NS_FAILED(PrincipalToPrincipalInfo(aPrincipal, &principalInfo)))) {
      IDB_REPORT_INTERNAL_ERR();
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      return nullptr;
    }

    if (principalInfo.type() != PrincipalInfo::TContentPrincipalInfo &&
        principalInfo.type() != PrincipalInfo::TSystemPrincipalInfo) {
      IDB_REPORT_INTERNAL_ERR();
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      return nullptr;
    }

    if (NS_WARN_IF(!QuotaManager::IsPrincipalInfoValid(principalInfo))) {
      IDB_REPORT_INTERNAL_ERR();
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      return nullptr;
    }
  } else {
    if (mGlobal->GetStorageAccess() == StorageAccess::ePrivateBrowsing) {
      if (NS_IsMainThread()) {
        SetUseCounter(
            mGlobal->GetGlobalJSObject(),
            aDeleting
                ? eUseCounter_custom_PrivateBrowsingIDBFactoryOpen
                : eUseCounter_custom_PrivateBrowsingIDBFactoryDeleteDatabase);
      } else {
        SetUseCounter(
            aDeleting ? UseCounterWorker::Custom_PrivateBrowsingIDBFactoryOpen
                      : UseCounterWorker::
                            Custom_PrivateBrowsingIDBFactoryDeleteDatabase);
      }
    }
    principalInfo = *mPrincipalInfo;
  }

  uint64_t version = 0;
  if (!aDeleting && aVersion.WasPassed()) {
    if (aVersion.Value() < 1) {
      aRv.ThrowTypeError("0 (Zero) is not a valid database version.");
      return nullptr;
    }
    version = aVersion.Value();
  }

  // Nothing can be done here if we have previously failed to create a
  // background actor.
  if (mBackgroundActorFailed) {
    IDB_REPORT_INTERNAL_ERR();
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  PersistenceType persistenceType;

  bool isInternal = principalInfo.type() == PrincipalInfo::TSystemPrincipalInfo;
  if (!isInternal &&
      principalInfo.type() == PrincipalInfo::TContentPrincipalInfo) {
    nsCString origin =
        principalInfo.get_ContentPrincipalInfo().originNoSuffix();
    isInternal = QuotaManager::IsOriginInternal(origin);
  }

  if (isInternal) {
    // Chrome privilege and internal origins always get persistent storage.
    persistenceType = PERSISTENCE_TYPE_PERSISTENT;
  } else {
    persistenceType = PERSISTENCE_TYPE_DEFAULT;
  }

  DatabaseMetadata& metadata = commonParams.metadata();
  metadata.name() = aName;
  metadata.persistenceType() = persistenceType;

  FactoryRequestParams params;
  if (aDeleting) {
    metadata.version() = 0;
    params = DeleteDatabaseRequestParams(commonParams);
  } else {
    metadata.version() = version;
    params = OpenDatabaseRequestParams(commonParams);
  }

  if (!mBackgroundActor) {
    BackgroundChildImpl::ThreadLocal* threadLocal =
        BackgroundChildImpl::GetThreadLocalForCurrentThread();

    UniquePtr<ThreadLocal> newIDBThreadLocal;
    ThreadLocal* idbThreadLocal;

    if (threadLocal && threadLocal->mIndexedDBThreadLocal) {
      idbThreadLocal = threadLocal->mIndexedDBThreadLocal.get();
    } else {
      nsCOMPtr<nsIUUIDGenerator> uuidGen =
          do_GetService("@mozilla.org/uuid-generator;1");
      MOZ_ASSERT(uuidGen);

      nsID id;
      MOZ_ALWAYS_SUCCEEDS(uuidGen->GenerateUUIDInPlace(&id));

      newIDBThreadLocal = WrapUnique(new ThreadLocal(id));
      idbThreadLocal = newIDBThreadLocal.get();
    }

    PBackgroundChild* backgroundActor =
        BackgroundChild::GetOrCreateForCurrentThread();
    if (NS_WARN_IF(!backgroundActor)) {
      IDB_REPORT_INTERNAL_ERR();
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      return nullptr;
    }

    {
      BackgroundFactoryChild* actor = new BackgroundFactoryChild(*this);

      mBackgroundActor = static_cast<BackgroundFactoryChild*>(
          backgroundActor->SendPBackgroundIDBFactoryConstructor(
              actor, idbThreadLocal->GetLoggingInfo()));

      if (NS_WARN_IF(!mBackgroundActor)) {
        mBackgroundActorFailed = true;
        IDB_REPORT_INTERNAL_ERR();
        aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
        return nullptr;
      }
    }

    if (newIDBThreadLocal) {
      if (!threadLocal) {
        threadLocal = BackgroundChildImpl::GetThreadLocalForCurrentThread();
      }
      MOZ_ASSERT(threadLocal);
      MOZ_ASSERT(!threadLocal->mIndexedDBThreadLocal);

      threadLocal->mIndexedDBThreadLocal = std::move(newIDBThreadLocal);
    }
  }

  RefPtr<IDBOpenDBRequest> request = IDBOpenDBRequest::Create(
      aCx, SafeRefPtr{this, AcquireStrongRefFromRawPtr{}}, mGlobal);
  if (!request) {
    MOZ_ASSERT(!NS_IsMainThread());
    aRv.ThrowUncatchableException();
    return nullptr;
  }

  MOZ_ASSERT(request);

  if (aDeleting) {
    IDB_LOG_MARK_CHILD_REQUEST(
        "indexedDB.deleteDatabase(\"%s\")", "IDBFactory.deleteDatabase(%.0s)",
        request->LoggingSerialNumber(), NS_ConvertUTF16toUTF8(aName).get());
  } else {
    IDB_LOG_MARK_CHILD_REQUEST(
        "indexedDB.open(\"%s\", %s)", "IDBFactory.open(%.0s%.0s)",
        request->LoggingSerialNumber(), NS_ConvertUTF16toUTF8(aName).get(),
        IDB_LOG_STRINGIFY(aVersion));
  }

  nsresult rv = InitiateRequest(WrapNotNull(request), params);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    IDB_REPORT_INTERNAL_ERR();
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  return request;
}

nsresult IDBFactory::InitiateRequest(
    const NotNull<RefPtr<IDBOpenDBRequest>>& aRequest,
    const FactoryRequestParams& aParams) {
  MOZ_ASSERT(mBackgroundActor);
  MOZ_ASSERT(!mBackgroundActorFailed);

  bool deleting;
  uint64_t requestedVersion;

  switch (aParams.type()) {
    case FactoryRequestParams::TDeleteDatabaseRequestParams: {
      const DatabaseMetadata& metadata =
          aParams.get_DeleteDatabaseRequestParams().commonParams().metadata();
      deleting = true;
      requestedVersion = metadata.version();
      break;
    }

    case FactoryRequestParams::TOpenDatabaseRequestParams: {
      const DatabaseMetadata& metadata =
          aParams.get_OpenDatabaseRequestParams().commonParams().metadata();
      deleting = false;
      requestedVersion = metadata.version();
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  auto actor = new BackgroundFactoryRequestChild(
      SafeRefPtr{this, AcquireStrongRefFromRawPtr{}}, aRequest, deleting,
      requestedVersion);

  if (!mBackgroundActor->SendPBackgroundIDBFactoryRequestConstructor(actor,
                                                                     aParams)) {
    aRequest->DispatchNonTransactionError(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

void IDBFactory::DisconnectFromGlobal(nsIGlobalObject* aOldGlobal) {
  MOZ_DIAGNOSTIC_ASSERT(aOldGlobal);
  // If CC unlinks us first, then mGlobal might be nullptr
  MOZ_DIAGNOSTIC_ASSERT(!mGlobal || mGlobal == aOldGlobal);

  mGlobal = nullptr;
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(IDBFactory)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IDBFactory)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBFactory)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBFactory)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IDBFactory)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowserChild)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEventTarget)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IDBFactory)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBrowserChild)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEventTarget)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IDBFactory)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

JSObject* IDBFactory::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return IDBFactory_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
