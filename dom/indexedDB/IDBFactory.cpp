/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBFactory.h"

#include "BackgroundChildImpl.h"
#include "IDBRequest.h"
#include "IndexedDatabaseManager.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Preferences.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/IDBFactoryBinding.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackground.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/Telemetry.h"
#include "mozIThirdPartyUtil.h"
#include "nsAboutProtocolUtils.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsIAboutModule.h"
#include "nsILoadContext.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsIUUIDGenerator.h"
#include "nsIWebNavigation.h"
#include "nsSandboxFlags.h"
#include "nsServiceManagerUtils.h"
#include "ProfilerHelpers.h"
#include "ReportInternalError.h"

// Include this last to avoid path problems on Windows.
#include "ActorsChild.h"

#ifdef DEBUG
#include "nsContentUtils.h" // For assertions.
#endif

namespace mozilla {
namespace dom {

using namespace mozilla::dom::indexedDB;
using namespace mozilla::dom::quota;
using namespace mozilla::ipc;

namespace {

const char kPrefIndexedDBEnabled[] = "dom.indexedDB.enabled";

} // namespace

struct IDBFactory::PendingRequestInfo
{
  RefPtr<IDBOpenDBRequest> mRequest;
  FactoryRequestParams mParams;

  PendingRequestInfo(IDBOpenDBRequest* aRequest,
                     const FactoryRequestParams& aParams)
  : mRequest(aRequest), mParams(aParams)
  {
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(aParams.type() != FactoryRequestParams::T__None);
  }
};

IDBFactory::IDBFactory()
  : mOwningObject(nullptr)
  , mBackgroundActor(nullptr)
  , mInnerWindowID(0)
  , mActiveTransactionCount(0)
  , mActiveDatabaseCount(0)
  , mBackgroundActorFailed(false)
  , mPrivateBrowsingMode(false)
{
  AssertIsOnOwningThread();
}

IDBFactory::~IDBFactory()
{
  MOZ_ASSERT_IF(mBackgroundActorFailed, !mBackgroundActor);

  mOwningObject = nullptr;
  mozilla::DropJSObjects(this);

  if (mBackgroundActor) {
    mBackgroundActor->SendDeleteMeInternal();
    MOZ_ASSERT(!mBackgroundActor, "SendDeleteMeInternal should have cleared!");
  }
}

// static
nsresult
IDBFactory::CreateForWindow(nsPIDOMWindowInner* aWindow,
                            IDBFactory** aFactory)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aFactory);

  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = AllowedForWindowInternal(aWindow, getter_AddRefs(principal));

  if (!(NS_SUCCEEDED(rv) && nsContentUtils::IsSystemPrincipal(principal)) &&
      NS_WARN_IF(!Preferences::GetBool(kPrefIndexedDBEnabled, false))) {
    *aFactory = nullptr;
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  if (rv == NS_ERROR_DOM_NOT_SUPPORTED_ERR) {
    NS_WARNING("IndexedDB is not permitted in a third-party window.");
    *aFactory = nullptr;
    return NS_OK;
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (rv == NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR) {
      IDB_REPORT_INTERNAL_ERR();
    }
    return rv;
  }

  MOZ_ASSERT(principal);

  nsAutoPtr<PrincipalInfo> principalInfo(new PrincipalInfo());
  rv = PrincipalToPrincipalInfo(principal, principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  MOZ_ASSERT(principalInfo->type() == PrincipalInfo::TContentPrincipalInfo ||
             principalInfo->type() == PrincipalInfo::TSystemPrincipalInfo);

  nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(aWindow);
  nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(webNav);

  RefPtr<IDBFactory> factory = new IDBFactory();
  factory->mPrincipalInfo = std::move(principalInfo);
  factory->mWindow = aWindow;
  factory->mTabChild = TabChild::GetFrom(aWindow);
  factory->mEventTarget =
    nsGlobalWindowInner::Cast(aWindow)->EventTargetFor(TaskCategory::Other);
  factory->mInnerWindowID = aWindow->WindowID();
  factory->mPrivateBrowsingMode =
    loadContext && loadContext->UsePrivateBrowsing();

  factory.forget(aFactory);
  return NS_OK;
}

// static
nsresult
IDBFactory::CreateForMainThreadJS(JSContext* aCx,
                                  JS::Handle<JSObject*> aOwningObject,
                                  IDBFactory** aFactory)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<PrincipalInfo> principalInfo(new PrincipalInfo());
  nsIPrincipal* principal = nsContentUtils::ObjectPrincipal(aOwningObject);
  MOZ_ASSERT(principal);
  bool isSystem;
  if (!AllowedForPrincipal(principal, &isSystem)) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsresult rv = PrincipalToPrincipalInfo(principal, principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = CreateForMainThreadJSInternal(aCx, aOwningObject, principalInfo, aFactory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(!principalInfo);

  return NS_OK;
}

// static
nsresult
IDBFactory::CreateForWorker(JSContext* aCx,
                            JS::Handle<JSObject*> aOwningObject,
                            const PrincipalInfo& aPrincipalInfo,
                            uint64_t aInnerWindowID,
                            IDBFactory** aFactory)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aPrincipalInfo.type() != PrincipalInfo::T__None);

  nsAutoPtr<PrincipalInfo> principalInfo(new PrincipalInfo(aPrincipalInfo));

  nsresult rv =
    CreateForJSInternal(aCx,
                        aOwningObject,
                        principalInfo,
                        aInnerWindowID,
                        aFactory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(!principalInfo);

  return NS_OK;
}

// static
nsresult
IDBFactory::CreateForMainThreadJSInternal(
                                       JSContext* aCx,
                                       JS::Handle<JSObject*> aOwningObject,
                                       nsAutoPtr<PrincipalInfo>& aPrincipalInfo,
                                       IDBFactory** aFactory)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipalInfo);

  if (aPrincipalInfo->type() != PrincipalInfo::TSystemPrincipalInfo &&
      NS_WARN_IF(!Preferences::GetBool(kPrefIndexedDBEnabled, false))) {
    *aFactory = nullptr;
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::GetOrCreate();
  if (NS_WARN_IF(!mgr)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsresult rv =
    CreateForJSInternal(aCx,
                        aOwningObject,
                        aPrincipalInfo,
                        /* aInnerWindowID */ 0,
                        aFactory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

// static
nsresult
IDBFactory::CreateForJSInternal(JSContext* aCx,
                                JS::Handle<JSObject*> aOwningObject,
                                nsAutoPtr<PrincipalInfo>& aPrincipalInfo,
                                uint64_t aInnerWindowID,
                                IDBFactory** aFactory)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aOwningObject);
  MOZ_ASSERT(aPrincipalInfo);
  MOZ_ASSERT(aPrincipalInfo->type() != PrincipalInfo::T__None);
  MOZ_ASSERT(aFactory);
  MOZ_ASSERT(JS_IsGlobalObject(aOwningObject));

  if (aPrincipalInfo->type() != PrincipalInfo::TContentPrincipalInfo &&
      aPrincipalInfo->type() != PrincipalInfo::TSystemPrincipalInfo) {
    NS_WARNING("IndexedDB not allowed for this principal!");
    aPrincipalInfo = nullptr;
    *aFactory = nullptr;
    return NS_OK;
  }

  RefPtr<IDBFactory> factory = new IDBFactory();
  factory->mPrincipalInfo = aPrincipalInfo.forget();
  factory->mOwningObject = aOwningObject;
  mozilla::HoldJSObjects(factory.get());
  factory->mEventTarget = GetCurrentThreadEventTarget();
  factory->mInnerWindowID = aInnerWindowID;

  factory.forget(aFactory);
  return NS_OK;
}

// static
bool
IDBFactory::AllowedForWindow(nsPIDOMWindowInner* aWindow)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = AllowedForWindowInternal(aWindow, getter_AddRefs(principal));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return true;
}

// static
nsresult
IDBFactory::AllowedForWindowInternal(nsPIDOMWindowInner* aWindow,
                                     nsIPrincipal** aPrincipal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  if (NS_WARN_IF(!IndexedDatabaseManager::GetOrCreate())) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsContentUtils::StorageAccess access =
    nsContentUtils::StorageAllowedForWindow(aWindow);

  // the factory callsite records whether the browser is in private browsing.
  // and thus we don't have to respect that setting here. IndexedDB has no
  // concept of session-local storage, and thus ignores it.
  if (access == nsContentUtils::StorageAccess::eDeny) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
  MOZ_ASSERT(sop);

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  if (NS_WARN_IF(!principal)) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;

  }

  if (nsContentUtils::IsSystemPrincipal(principal)) {
    principal.forget(aPrincipal);
    return NS_OK;
  }

  // About URIs shouldn't be able to access IndexedDB unless they have the
  // nsIAboutModule::ENABLE_INDEXED_DB flag set on them.
  nsCOMPtr<nsIURI> uri;
  MOZ_ALWAYS_SUCCEEDS(principal->GetURI(getter_AddRefs(uri)));
  MOZ_ASSERT(uri);

  bool isAbout = false;
  MOZ_ALWAYS_SUCCEEDS(uri->SchemeIs("about", &isAbout));

  if (isAbout) {
    nsCOMPtr<nsIAboutModule> module;
    if (NS_SUCCEEDED(NS_GetAboutModule(uri, getter_AddRefs(module)))) {
      uint32_t flags;
      if (NS_SUCCEEDED(module->GetURIFlags(uri, &flags))) {
        if (!(flags & nsIAboutModule::ENABLE_INDEXED_DB)) {
          return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
        }
      } else {
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      }
    } else {
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }
  }

  principal.forget(aPrincipal);
  return NS_OK;
}

// static
bool
IDBFactory::AllowedForPrincipal(nsIPrincipal* aPrincipal,
                                bool* aIsSystemPrincipal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  if (NS_WARN_IF(!IndexedDatabaseManager::GetOrCreate())) {
    return false;
  }

  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    if (aIsSystemPrincipal) {
      *aIsSystemPrincipal = true;
    }
    return true;
  } else if (aIsSystemPrincipal) {
    *aIsSystemPrincipal = false;
  }

  if (aPrincipal->GetIsNullPrincipal()) {
    return false;
  }

  return true;
}

void
IDBFactory::UpdateActiveTransactionCount(int32_t aDelta)
{
  AssertIsOnOwningThread();
  MOZ_DIAGNOSTIC_ASSERT(aDelta > 0 ||
                        (mActiveTransactionCount + aDelta) < mActiveTransactionCount);
  mActiveTransactionCount += aDelta;
  if (mWindow) {
    mWindow->UpdateActiveIndexedDBTransactionCount(aDelta);
  }
}

void
IDBFactory::UpdateActiveDatabaseCount(int32_t aDelta)
{
  AssertIsOnOwningThread();
  MOZ_DIAGNOSTIC_ASSERT(aDelta > 0 ||
                        (mActiveDatabaseCount + aDelta) < mActiveDatabaseCount);
  mActiveDatabaseCount += aDelta;
  if (mWindow) {
    mWindow->UpdateActiveIndexedDBDatabaseCount(aDelta);
  }
}

bool
IDBFactory::IsChrome() const
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mPrincipalInfo);

  return mPrincipalInfo->type() == PrincipalInfo::TSystemPrincipalInfo;
}

void
IDBFactory::IncrementParentLoggingRequestSerialNumber()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mBackgroundActor);

  mBackgroundActor->SendIncrementLoggingRequestSerialNumber();
}

already_AddRefed<IDBOpenDBRequest>
IDBFactory::Open(JSContext* aCx,
                 const nsAString& aName,
                 uint64_t aVersion,
                 CallerType aCallerType,
                 ErrorResult& aRv)
{
  return OpenInternal(aCx,
                      /* aPrincipal */ nullptr,
                      aName,
                      Optional<uint64_t>(aVersion),
                      Optional<StorageType>(),
                      /* aDeleting */ false,
                      aCallerType,
                      aRv);
}

already_AddRefed<IDBOpenDBRequest>
IDBFactory::Open(JSContext* aCx,
                 const nsAString& aName,
                 const IDBOpenDBOptions& aOptions,
                 CallerType aCallerType,
                 ErrorResult& aRv)
{
  if (!IsChrome() &&
      aOptions.mStorage.WasPassed()) {

    if (mWindow && mWindow->GetExtantDoc()) {
      mWindow->GetExtantDoc()->WarnOnceAbout(nsIDocument::eIDBOpenDBOptions_StorageType);
    } else if (!NS_IsMainThread()) {
      // The method below reports on the main thread too, so we need to make sure we're on a worker.
      // Workers don't have a WarnOnceAbout mechanism, so this will be reported every time.
      WorkerPrivate::ReportErrorToConsole("IDBOpenDBOptions_StorageType");
    }

    bool ignore = false;
    // Ignore internal usage on about: pages.
    if (NS_IsMainThread()) {
      nsCOMPtr<nsIPrincipal> principal = PrincipalInfoToPrincipal(*mPrincipalInfo);
      if (principal) {
        nsCOMPtr<nsIURI> uri;
        nsresult rv = principal->GetURI(getter_AddRefs(uri));
        if (NS_SUCCEEDED(rv) && uri) {
          bool isAbout;
          rv = uri->SchemeIs("about", &isAbout);
          if (NS_SUCCEEDED(rv) && isAbout) {
            ignore = true;
          }
        }
      }
    }

    if (!ignore) {
      switch (aOptions.mStorage.Value()) {
        case StorageType::Persistent: {
          Telemetry::ScalarAdd(Telemetry::ScalarID::IDB_TYPE_PERSISTENT_COUNT, 1);
          break;
        }

        case StorageType::Temporary: {
          Telemetry::ScalarAdd(Telemetry::ScalarID::IDB_TYPE_TEMPORARY_COUNT, 1);
          break;
        }

        case StorageType::Default:
        case StorageType::EndGuard_:
          break;

        default:
          MOZ_CRASH("Invalid storage type!");
      }
    }
  }

  return OpenInternal(aCx,
                      /* aPrincipal */ nullptr,
                      aName,
                      aOptions.mVersion,
                      aOptions.mStorage,
                      /* aDeleting */ false,
                      aCallerType,
                      aRv);
}

already_AddRefed<IDBOpenDBRequest>
IDBFactory::DeleteDatabase(JSContext* aCx,
                           const nsAString& aName,
                           const IDBOpenDBOptions& aOptions,
                           CallerType aCallerType,
                           ErrorResult& aRv)
{
  return OpenInternal(aCx,
                      /* aPrincipal */ nullptr,
                      aName,
                      Optional<uint64_t>(),
                      aOptions.mStorage,
                      /* aDeleting */ true,
                      aCallerType,
                      aRv);
}

int16_t
IDBFactory::Cmp(JSContext* aCx, JS::Handle<JS::Value> aFirst,
                JS::Handle<JS::Value> aSecond, ErrorResult& aRv)
{
  Key first, second;
  nsresult rv = first.SetFromJSVal(aCx, aFirst);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return 0;
  }

  rv = second.SetFromJSVal(aCx, aSecond);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return 0;
  }

  if (first.IsUnset() || second.IsUnset()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
    return 0;
  }

  return Key::CompareKeys(first, second);
}

already_AddRefed<IDBOpenDBRequest>
IDBFactory::OpenForPrincipal(JSContext* aCx,
                             nsIPrincipal* aPrincipal,
                             const nsAString& aName,
                             uint64_t aVersion,
                             SystemCallerGuarantee aGuarantee,
                             ErrorResult& aRv)
{
  MOZ_ASSERT(aPrincipal);
  if (!NS_IsMainThread()) {
    MOZ_CRASH("Figure out security checks for workers!  What's this aPrincipal "
              "we have on a worker thread?");
  }

  return OpenInternal(aCx,
                      aPrincipal,
                      aName,
                      Optional<uint64_t>(aVersion),
                      Optional<StorageType>(),
                      /* aDeleting */ false,
                      aGuarantee,
                      aRv);
}

already_AddRefed<IDBOpenDBRequest>
IDBFactory::OpenForPrincipal(JSContext* aCx,
                             nsIPrincipal* aPrincipal,
                             const nsAString& aName,
                             const IDBOpenDBOptions& aOptions,
                             SystemCallerGuarantee aGuarantee,
                             ErrorResult& aRv)
{
  MOZ_ASSERT(aPrincipal);
  if (!NS_IsMainThread()) {
    MOZ_CRASH("Figure out security checks for workers!  What's this aPrincipal "
              "we have on a worker thread?");
  }

  return OpenInternal(aCx,
                      aPrincipal,
                      aName,
                      aOptions.mVersion,
                      aOptions.mStorage,
                      /* aDeleting */ false,
                      aGuarantee,
                      aRv);
}

already_AddRefed<IDBOpenDBRequest>
IDBFactory::DeleteForPrincipal(JSContext* aCx,
                               nsIPrincipal* aPrincipal,
                               const nsAString& aName,
                               const IDBOpenDBOptions& aOptions,
                               SystemCallerGuarantee aGuarantee,
                               ErrorResult& aRv)
{
  MOZ_ASSERT(aPrincipal);
  if (!NS_IsMainThread()) {
    MOZ_CRASH("Figure out security checks for workers!  What's this aPrincipal "
              "we have on a worker thread?");
  }

  return OpenInternal(aCx,
                      aPrincipal,
                      aName,
                      Optional<uint64_t>(),
                      aOptions.mStorage,
                      /* aDeleting */ true,
                      aGuarantee,
                      aRv);
}

already_AddRefed<IDBOpenDBRequest>
IDBFactory::OpenInternal(JSContext* aCx,
                         nsIPrincipal* aPrincipal,
                         const nsAString& aName,
                         const Optional<uint64_t>& aVersion,
                         const Optional<StorageType>& aStorageType,
                         bool aDeleting,
                         CallerType aCallerType,
                         ErrorResult& aRv)
{
  MOZ_ASSERT(mWindow || mOwningObject);
  MOZ_ASSERT_IF(!mWindow, !mPrivateBrowsingMode);

  CommonFactoryRequestParams commonParams;

  PrincipalInfo& principalInfo = commonParams.principalInfo();

  if (aPrincipal) {
    if (!NS_IsMainThread()) {
      MOZ_CRASH("Figure out security checks for workers!  What's this "
                "aPrincipal we have on a worker thread?");
    }
    MOZ_ASSERT(aCallerType == CallerType::System);
    MOZ_DIAGNOSTIC_ASSERT(mPrivateBrowsingMode == (aPrincipal->GetPrivateBrowsingId() > 0));

    if (NS_WARN_IF(NS_FAILED(PrincipalToPrincipalInfo(aPrincipal,
                                                      &principalInfo)))) {
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
  } else {
    principalInfo = *mPrincipalInfo;
  }

  uint64_t version = 0;
  if (!aDeleting && aVersion.WasPassed()) {
    if (aVersion.Value() < 1) {
      aRv.ThrowTypeError<MSG_INVALID_VERSION>();
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
  if (!isInternal && principalInfo.type() == PrincipalInfo::TContentPrincipalInfo) {
    nsCString origin = principalInfo.get_ContentPrincipalInfo().originNoSuffix();
    isInternal = QuotaManager::IsOriginInternal(origin);
  }

  // Allow storage attributes for add-ons independent of the pref.
  // This works in the main thread only, workers don't have the principal.
  bool isAddon = false;
  if (NS_IsMainThread()) {
    // aPrincipal is passed inconsistently, so even when we are already on
    // the main thread, we may have been passed a null aPrincipal.
    nsCOMPtr<nsIPrincipal> principal = PrincipalInfoToPrincipal(principalInfo);
    if (principal) {
      nsAutoString addonId;
      Unused << NS_WARN_IF(NS_FAILED(principal->GetAddonId(addonId)));
      isAddon = !addonId.IsEmpty();
    }
  }

  if (isInternal) {
    // Chrome privilege and internal origins always get persistent storage.
    persistenceType = PERSISTENCE_TYPE_PERSISTENT;
  } else if (isAddon || DOMPrefs::IndexedDBStorageOptionsEnabled()) {
    persistenceType = PersistenceTypeFromStorage(aStorageType);
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

    nsAutoPtr<ThreadLocal> newIDBThreadLocal;
    ThreadLocal* idbThreadLocal;

    if (threadLocal && threadLocal->mIndexedDBThreadLocal) {
      idbThreadLocal = threadLocal->mIndexedDBThreadLocal;
    } else {
      nsCOMPtr<nsIUUIDGenerator> uuidGen =
        do_GetService("@mozilla.org/uuid-generator;1");
      MOZ_ASSERT(uuidGen);

      nsID id;
      MOZ_ALWAYS_SUCCEEDS(uuidGen->GenerateUUIDInPlace(&id));

      newIDBThreadLocal = idbThreadLocal = new ThreadLocal(id);
    }

    PBackgroundChild* backgroundActor =
      BackgroundChild::GetOrCreateForCurrentThread();
    if (NS_WARN_IF(!backgroundActor)) {
      IDB_REPORT_INTERNAL_ERR();
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      return nullptr;
    }

    {
      BackgroundFactoryChild* actor = new BackgroundFactoryChild(this);

      // Set EventTarget for the top-level actor.
      // All child actors created later inherit the same event target.
      backgroundActor->SetEventTargetForActor(actor, EventTarget());
      MOZ_ASSERT(actor->GetActorEventTarget());
      mBackgroundActor =
        static_cast<BackgroundFactoryChild*>(
          backgroundActor->SendPBackgroundIDBFactoryConstructor(actor,
                                                                idbThreadLocal->GetLoggingInfo()));

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

      threadLocal->mIndexedDBThreadLocal = newIDBThreadLocal.forget();
    }
  }

  RefPtr<IDBOpenDBRequest> request;

  if (mWindow) {
    JS::Rooted<JSObject*> scriptOwner(aCx,
                                      nsGlobalWindowInner::Cast(mWindow.get())->FastGetGlobalJSObject());
    MOZ_ASSERT(scriptOwner);

    request = IDBOpenDBRequest::CreateForWindow(aCx, this, mWindow, scriptOwner);
  } else {
    JS::Rooted<JSObject*> scriptOwner(aCx, mOwningObject);

    request = IDBOpenDBRequest::CreateForJS(aCx, this, scriptOwner);
    if (!request) {
      MOZ_ASSERT(!NS_IsMainThread());
      aRv.ThrowUncatchableException();
      return nullptr;
    }
  }

  MOZ_ASSERT(request);

  if (aDeleting) {
    IDB_LOG_MARK("IndexedDB %s: Child  Request[%llu]: "
                   "indexedDB.deleteDatabase(\"%s\")",
                 "IndexedDB %s: C R[%llu]: IDBFactory.deleteDatabase()",
                 IDB_LOG_ID_STRING(),
                 request->LoggingSerialNumber(),
                 NS_ConvertUTF16toUTF8(aName).get());
  } else {
    IDB_LOG_MARK("IndexedDB %s: Child  Request[%llu]: "
                   "indexedDB.open(\"%s\", %s)",
                 "IndexedDB %s: C R[%llu]: IDBFactory.open()",
                 IDB_LOG_ID_STRING(),
                 request->LoggingSerialNumber(),
                 NS_ConvertUTF16toUTF8(aName).get(),
                 IDB_LOG_STRINGIFY(aVersion));
  }

  nsresult rv = InitiateRequest(request, params);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    IDB_REPORT_INTERNAL_ERR();
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  return request.forget();
}

nsresult
IDBFactory::InitiateRequest(IDBOpenDBRequest* aRequest,
                            const FactoryRequestParams& aParams)
{
  MOZ_ASSERT(aRequest);
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

  auto actor =
    new BackgroundFactoryRequestChild(this,
                                      aRequest,
                                      deleting,
                                      requestedVersion);

  if (!mBackgroundActor->SendPBackgroundIDBFactoryRequestConstructor(actor,
                                                                     aParams)) {
    aRequest->DispatchNonTransactionError(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  MOZ_ASSERT(actor->GetActorEventTarget(),
    "The event target shall be inherited from its manager actor.");

  return NS_OK;
}

void
IDBFactory::RebindToNewWindow(nsPIDOMWindowInner* aNewWindow)
{
  MOZ_DIAGNOSTIC_ASSERT(aNewWindow);
  MOZ_DIAGNOSTIC_ASSERT(mWindow);
  MOZ_DIAGNOSTIC_ASSERT(aNewWindow != mWindow);

  mWindow->UpdateActiveIndexedDBTransactionCount(-1 * mActiveTransactionCount);
  mWindow->UpdateActiveIndexedDBDatabaseCount(-1 * mActiveDatabaseCount);

  mWindow = aNewWindow;

  mInnerWindowID = aNewWindow->WindowID();
  mWindow->UpdateActiveIndexedDBTransactionCount(mActiveTransactionCount);
  mWindow->UpdateActiveIndexedDBDatabaseCount(mActiveDatabaseCount);
}

void
IDBFactory::DisconnectFromWindow(nsPIDOMWindowInner* aOldWindow)
{
  MOZ_DIAGNOSTIC_ASSERT(aOldWindow);
  // If CC unlinks us first, then mWindow might be nullptr
  MOZ_DIAGNOSTIC_ASSERT(!mWindow || mWindow == aOldWindow);

  mWindow = nullptr;
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(IDBFactory)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IDBFactory)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBFactory)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBFactory)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IDBFactory)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IDBFactory)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mOwningObject = nullptr;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IDBFactory)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mOwningObject)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

JSObject*
IDBFactory::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return IDBFactory_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
