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
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/IDBFactoryBinding.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackground.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozIThirdPartyUtil.h"
#include "nsAboutProtocolUtils.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsIAboutModule.h"
#include "nsIIPCBackgroundChildCreateCallback.h"
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
namespace indexedDB {

using namespace mozilla::dom::quota;
using namespace mozilla::ipc;

namespace {

const char kPrefIndexedDBEnabled[] = "dom.indexedDB.enabled";

} // namespace

class IDBFactory::BackgroundCreateCallback final
  : public nsIIPCBackgroundChildCreateCallback
{
  nsRefPtr<IDBFactory> mFactory;
  LoggingInfo mLoggingInfo;

public:
  explicit
  BackgroundCreateCallback(IDBFactory* aFactory,
                           const LoggingInfo& aLoggingInfo)
    : mFactory(aFactory)
    , mLoggingInfo(aLoggingInfo)
  {
    MOZ_ASSERT(aFactory);
  }

  NS_DECL_ISUPPORTS

private:
  ~BackgroundCreateCallback()
  { }

  NS_DECL_NSIIPCBACKGROUNDCHILDCREATECALLBACK
};

struct IDBFactory::PendingRequestInfo
{
  nsRefPtr<IDBOpenDBRequest> mRequest;
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
  , mBackgroundActorFailed(false)
  , mPrivateBrowsingMode(false)
{
#ifdef DEBUG
  mOwningThread = PR_GetCurrentThread();
#endif
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
IDBFactory::CreateForWindow(nsPIDOMWindow* aWindow,
                            IDBFactory** aFactory)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsInnerWindow());
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

  nsRefPtr<IDBFactory> factory = new IDBFactory();
  factory->mPrincipalInfo = Move(principalInfo);
  factory->mWindow = aWindow;
  factory->mTabChild = TabChild::GetFrom(aWindow);
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
IDBFactory::CreateForDatastore(JSContext* aCx,
                               JS::Handle<JSObject*> aOwningObject,
                               IDBFactory** aFactory)
{
  MOZ_ASSERT(NS_IsMainThread());

  // There should be a null principal pushed here, but it's still chrome...
  MOZ_ASSERT(!nsContentUtils::IsCallerChrome());

  nsAutoPtr<PrincipalInfo> principalInfo(
    new PrincipalInfo(SystemPrincipalInfo()));

  nsresult rv =
    CreateForMainThreadJSInternal(aCx, aOwningObject, principalInfo, aFactory);
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
  MOZ_ASSERT(JS_GetGlobalForObject(aCx, aOwningObject) == aOwningObject,
             "Not a global object!");

  if (aPrincipalInfo->type() != PrincipalInfo::TContentPrincipalInfo &&
      aPrincipalInfo->type() != PrincipalInfo::TSystemPrincipalInfo) {
    NS_WARNING("IndexedDB not allowed for this principal!");
    aPrincipalInfo = nullptr;
    *aFactory = nullptr;
    return NS_OK;
  }

  nsRefPtr<IDBFactory> factory = new IDBFactory();
  factory->mPrincipalInfo = aPrincipalInfo.forget();
  factory->mOwningObject = aOwningObject;
  mozilla::HoldJSObjects(factory.get());
  factory->mInnerWindowID = aInnerWindowID;

  factory.forget(aFactory);
  return NS_OK;
}

// static
bool
IDBFactory::AllowedForWindow(nsPIDOMWindow* aWindow)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsInnerWindow());

  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = AllowedForWindowInternal(aWindow, getter_AddRefs(principal));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return true;
}

// static
nsresult
IDBFactory::AllowedForWindowInternal(nsPIDOMWindow* aWindow,
                                     nsIPrincipal** aPrincipal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsInnerWindow());

  if (NS_WARN_IF(!IndexedDatabaseManager::GetOrCreate())) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsIDocument* document = aWindow->GetExtantDoc();
  if (document->GetSandboxFlags() & SANDBOXED_ORIGIN) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
  MOZ_ASSERT(sop);

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  if (NS_WARN_IF(!principal)) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  bool isSystemPrincipal;
  if (!AllowedForPrincipal(principal, &isSystemPrincipal)) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  if (isSystemPrincipal) {
    principal.forget(aPrincipal);
    return NS_OK;
  }

  // Whitelist about:home, since it doesn't have a base domain it would not
  // pass the ThirdPartyUtil check, though it should be able to use indexedDB.
  bool skipThirdPartyCheck = false;

  nsCOMPtr<nsIURI> uri;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(principal->GetURI(getter_AddRefs(uri))));

  bool isAbout;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(uri->SchemeIs("about", &isAbout)));

  if (isAbout) {
    nsCOMPtr<nsIAboutModule> module;
    if (NS_SUCCEEDED(NS_GetAboutModule(uri, getter_AddRefs(module)))) {
      uint32_t flags;
      if (NS_SUCCEEDED(module->GetURIFlags(uri, &flags))) {
        skipThirdPartyCheck = flags & nsIAboutModule::ENABLE_INDEXED_DB;
      } else {
        NS_WARNING("GetURIFlags failed!");
      }
    } else {
      NS_WARNING("NS_GetAboutModule failed!");
    }
  }

  if (!skipThirdPartyCheck) {
    nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
      do_GetService(THIRDPARTYUTIL_CONTRACTID);
    MOZ_ASSERT(thirdPartyUtil);

    bool isThirdParty;
    if (NS_WARN_IF(NS_FAILED(
          thirdPartyUtil->IsThirdPartyWindow(aWindow,
                                             nullptr,
                                             &isThirdParty)))) {
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    if (isThirdParty) {
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

  bool isNullPrincipal;
  if (NS_WARN_IF(NS_FAILED(aPrincipal->GetIsNullPrincipal(&isNullPrincipal))) ||
      isNullPrincipal) {
    return false;
  }

  return true;
}

#ifdef DEBUG

void
IDBFactory::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(mOwningThread);
  MOZ_ASSERT(PR_GetCurrentThread() == mOwningThread);
}

#endif // DEBUG

bool
IDBFactory::IsChrome() const
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mPrincipalInfo);

  return mPrincipalInfo->type() == PrincipalInfo::TSystemPrincipalInfo;
}

void
IDBFactory::SetBackgroundActor(BackgroundFactoryChild* aBackgroundActor)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aBackgroundActor);
  MOZ_ASSERT(!mBackgroundActor);

  mBackgroundActor = aBackgroundActor;
}

void
IDBFactory::IncrementParentLoggingRequestSerialNumber()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mBackgroundActor);

  mBackgroundActor->SendIncrementLoggingRequestSerialNumber();
}

already_AddRefed<IDBOpenDBRequest>
IDBFactory::Open(const nsAString& aName,
                 uint64_t aVersion,
                 ErrorResult& aRv)
{
  return OpenInternal(/* aPrincipal */ nullptr,
                      aName,
                      Optional<uint64_t>(aVersion),
                      Optional<StorageType>(),
                      /* aDeleting */ false,
                      aRv);
}

already_AddRefed<IDBOpenDBRequest>
IDBFactory::Open(const nsAString& aName,
                 const IDBOpenDBOptions& aOptions,
                 ErrorResult& aRv)
{
  return OpenInternal(/* aPrincipal */ nullptr,
                      aName,
                      aOptions.mVersion,
                      aOptions.mStorage,
                      /* aDeleting */ false,
                      aRv);
}

already_AddRefed<IDBOpenDBRequest>
IDBFactory::DeleteDatabase(const nsAString& aName,
                           const IDBOpenDBOptions& aOptions,
                           ErrorResult& aRv)
{
  return OpenInternal(/* aPrincipal */ nullptr,
                      aName,
                      Optional<uint64_t>(),
                      aOptions.mStorage,
                      /* aDeleting */ true,
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
IDBFactory::OpenForPrincipal(nsIPrincipal* aPrincipal,
                             const nsAString& aName,
                             uint64_t aVersion,
                             ErrorResult& aRv)
{
  MOZ_ASSERT(aPrincipal);
  if (!NS_IsMainThread()) {
    MOZ_CRASH("Figure out security checks for workers!");
  }
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  return OpenInternal(aPrincipal,
                      aName,
                      Optional<uint64_t>(aVersion),
                      Optional<StorageType>(),
                      /* aDeleting */ false,
                      aRv);
}

already_AddRefed<IDBOpenDBRequest>
IDBFactory::OpenForPrincipal(nsIPrincipal* aPrincipal,
                             const nsAString& aName,
                             const IDBOpenDBOptions& aOptions,
                             ErrorResult& aRv)
{
  MOZ_ASSERT(aPrincipal);
  if (!NS_IsMainThread()) {
    MOZ_CRASH("Figure out security checks for workers!");
  }
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  return OpenInternal(aPrincipal,
                      aName,
                      aOptions.mVersion,
                      aOptions.mStorage,
                      /* aDeleting */ false,
                      aRv);
}

already_AddRefed<IDBOpenDBRequest>
IDBFactory::DeleteForPrincipal(nsIPrincipal* aPrincipal,
                               const nsAString& aName,
                               const IDBOpenDBOptions& aOptions,
                               ErrorResult& aRv)
{
  MOZ_ASSERT(aPrincipal);
  if (!NS_IsMainThread()) {
    MOZ_CRASH("Figure out security checks for workers!");
  }
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  return OpenInternal(aPrincipal,
                      aName,
                      Optional<uint64_t>(),
                      aOptions.mStorage,
                      /* aDeleting */ true,
                      aRv);
}

already_AddRefed<IDBOpenDBRequest>
IDBFactory::OpenInternal(nsIPrincipal* aPrincipal,
                         const nsAString& aName,
                         const Optional<uint64_t>& aVersion,
                         const Optional<StorageType>& aStorageType,
                         bool aDeleting,
                         ErrorResult& aRv)
{
  MOZ_ASSERT(mWindow || mOwningObject);
  MOZ_ASSERT_IF(!mWindow, !mPrivateBrowsingMode);

  CommonFactoryRequestParams commonParams;
  commonParams.privateBrowsingMode() = mPrivateBrowsingMode;

  PrincipalInfo& principalInfo = commonParams.principalInfo();

  if (aPrincipal) {
    if (!NS_IsMainThread()) {
      MOZ_CRASH("Figure out security checks for workers!");
    }
    MOZ_ASSERT(nsContentUtils::IsCallerChrome());

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
      aRv.ThrowTypeError(MSG_INVALID_VERSION);
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

  if (principalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    // Chrome privilege always gets persistent storage.
    persistenceType = PERSISTENCE_TYPE_PERSISTENT;
  } else {
    persistenceType = PersistenceTypeFromStorage(aStorageType);
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

  if (!mBackgroundActor && mPendingRequests.IsEmpty()) {
    // We need to start the sequence to create a background actor for this
    // thread.
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
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(uuidGen->GenerateUUIDInPlace(&id)));

      newIDBThreadLocal = idbThreadLocal = new ThreadLocal(id);
    }

    nsRefPtr<BackgroundCreateCallback> cb =
      new BackgroundCreateCallback(this, idbThreadLocal->GetLoggingInfo());
    if (NS_WARN_IF(!BackgroundChild::GetOrCreateForCurrentThread(cb))) {
      IDB_REPORT_INTERNAL_ERR();
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      return nullptr;
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

  AutoJSAPI autoJS;
  nsRefPtr<IDBOpenDBRequest> request;

  if (mWindow) {
    AutoJSContext cx;
    if (NS_WARN_IF(!autoJS.Init(mWindow, cx))) {
      IDB_REPORT_INTERNAL_ERR();
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      return nullptr;
    }

    JS::Rooted<JSObject*> scriptOwner(cx,
      static_cast<nsGlobalWindow*>(mWindow.get())->FastGetGlobalJSObject());
    MOZ_ASSERT(scriptOwner);

    request = IDBOpenDBRequest::CreateForWindow(this, mWindow, scriptOwner);
  } else {
    autoJS.Init(mOwningObject.get());
    JS::Rooted<JSObject*> scriptOwner(autoJS.cx(), mOwningObject);

    request = IDBOpenDBRequest::CreateForJS(this, scriptOwner);
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

  // If we already have a background actor then we can start this request now.
  if (mBackgroundActor) {
    nsresult rv = InitiateRequest(request, params);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      IDB_REPORT_INTERNAL_ERR();
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      return nullptr;
    }
  } else {
    mPendingRequests.AppendElement(new PendingRequestInfo(request, params));
  }

  return request.forget();
}

nsresult
IDBFactory::BackgroundActorCreated(PBackgroundChild* aBackgroundActor,
                                   const LoggingInfo& aLoggingInfo)
{
  MOZ_ASSERT(aBackgroundActor);
  MOZ_ASSERT(!mBackgroundActor);
  MOZ_ASSERT(!mBackgroundActorFailed);

  {
    BackgroundFactoryChild* actor = new BackgroundFactoryChild(this);

    mBackgroundActor =
      static_cast<BackgroundFactoryChild*>(
        aBackgroundActor->SendPBackgroundIDBFactoryConstructor(actor,
                                                               aLoggingInfo));
  }

  if (NS_WARN_IF(!mBackgroundActor)) {
    BackgroundActorFailed();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsresult rv = NS_OK;

  for (uint32_t index = 0, count = mPendingRequests.Length();
       index < count;
       index++) {
    nsAutoPtr<PendingRequestInfo> info(mPendingRequests[index].forget());

    nsresult rv2 = InitiateRequest(info->mRequest, info->mParams);

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
IDBFactory::BackgroundActorFailed()
{
  MOZ_ASSERT(!mPendingRequests.IsEmpty());
  MOZ_ASSERT(!mBackgroundActor);
  MOZ_ASSERT(!mBackgroundActorFailed);

  mBackgroundActorFailed = true;

  for (uint32_t index = 0, count = mPendingRequests.Length();
       index < count;
       index++) {
    nsAutoPtr<PendingRequestInfo> info(mPendingRequests[index].forget());
    info->mRequest->
      DispatchNonTransactionError(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  mPendingRequests.Clear();
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

  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(IDBFactory)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IDBFactory)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBFactory)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBFactory)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IDBFactory)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
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
  return IDBFactoryBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_ISUPPORTS(IDBFactory::BackgroundCreateCallback,
                  nsIIPCBackgroundChildCreateCallback)

void
IDBFactory::BackgroundCreateCallback::ActorCreated(PBackgroundChild* aActor)
{
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(mFactory);

  nsRefPtr<IDBFactory> factory;
  mFactory.swap(factory);

  factory->BackgroundActorCreated(aActor, mLoggingInfo);
}

void
IDBFactory::BackgroundCreateCallback::ActorFailed()
{
  MOZ_ASSERT(mFactory);

  nsRefPtr<IDBFactory> factory;
  mFactory.swap(factory);

  factory->BackgroundActorFailed();
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
