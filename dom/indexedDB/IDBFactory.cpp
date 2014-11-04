/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBFactory.h"

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
#include "nsGlobalWindow.h"
#include "nsIIPCBackgroundChildCreateCallback.h"
#include "nsILoadContext.h"
#include "nsIPrincipal.h"
#include "nsIWebNavigation.h"
#include "ProfilerHelpers.h"
#include "ReportInternalError.h"

// Include this last to avoid path problems on Windows.
#include "ActorsChild.h"

#ifdef DEBUG
#include "nsContentUtils.h" // For IsCallerChrome assertions.
#endif

namespace mozilla {
namespace dom {
namespace indexedDB {

using namespace mozilla::dom::quota;
using namespace mozilla::ipc;

namespace {

const char kPrefIndexedDBEnabled[] = "dom.indexedDB.enabled";

nsresult
GetPrincipalInfoFromPrincipal(nsIPrincipal* aPrincipal,
                              PrincipalInfo* aPrincipalInfo)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aPrincipalInfo);

  bool isNullPrincipal;
  nsresult rv = aPrincipal->GetIsNullPrincipal(&isNullPrincipal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (isNullPrincipal) {
    NS_WARNING("IndexedDB not supported from this principal!");
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  rv = PrincipalToPrincipalInfo(aPrincipal, aPrincipalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

} // anonymous namespace

class IDBFactory::BackgroundCreateCallback MOZ_FINAL
  : public nsIIPCBackgroundChildCreateCallback
{
  nsRefPtr<IDBFactory> mFactory;

public:
  explicit BackgroundCreateCallback(IDBFactory* aFactory)
  : mFactory(aFactory)
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
  , mRootedOwningObject(false)
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

  if (mRootedOwningObject) {
    mOwningObject = nullptr;
    mozilla::DropJSObjects(this);
  }

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

  if (NS_WARN_IF(!Preferences::GetBool(kPrefIndexedDBEnabled, false))) {
    *aFactory = nullptr;
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
  if (NS_WARN_IF(!sop)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  if (NS_WARN_IF(!principal)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsAutoPtr<PrincipalInfo> principalInfo(new PrincipalInfo());

  if (NS_WARN_IF(NS_FAILED(GetPrincipalInfoFromPrincipal(principal,
                                                         principalInfo)))) {
    // Not allowed.
    *aFactory = nullptr;
    return NS_OK;
  }

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::GetOrCreate();
  if (NS_WARN_IF(!mgr)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(aWindow);
  nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(webNav);

  bool privateBrowsingMode = loadContext && loadContext->UsePrivateBrowsing();

  nsRefPtr<IDBFactory> factory = new IDBFactory();
  factory->mPrincipalInfo = Move(principalInfo);
  factory->mWindow = aWindow;
  factory->mTabChild = TabChild::GetFrom(aWindow);
  factory->mPrivateBrowsingMode = privateBrowsingMode;

  factory.forget(aFactory);
  return NS_OK;
}

// static
nsresult
IDBFactory::CreateForChromeJS(JSContext* aCx,
                              JS::Handle<JSObject*> aOwningObject,
                              IDBFactory** aFactory)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  nsAutoPtr<PrincipalInfo> principalInfo(
    new PrincipalInfo(SystemPrincipalInfo()));

  nsresult rv =
    CreateForJSInternal(aCx, aOwningObject, principalInfo, aFactory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(!principalInfo);

  return NS_OK;
}

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
    CreateForJSInternal(aCx, aOwningObject, principalInfo, aFactory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(!principalInfo);

  return NS_OK;
}

// static
nsresult
IDBFactory::CreateForJSInternal(JSContext* aCx,
                                JS::Handle<JSObject*> aOwningObject,
                                nsAutoPtr<PrincipalInfo>& aPrincipalInfo,
                                IDBFactory** aFactory)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aOwningObject);
  MOZ_ASSERT(aPrincipalInfo);
  MOZ_ASSERT(aFactory);
  MOZ_ASSERT(JS_GetGlobalForObject(aCx, aOwningObject) == aOwningObject,
             "Not a global object!");

  if (!NS_IsMainThread()) {
    MOZ_CRASH("Not yet supported off the main thread!");
  }

  if (NS_WARN_IF(!Preferences::GetBool(kPrefIndexedDBEnabled, false))) {
    *aFactory = nullptr;
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::GetOrCreate();
  if (NS_WARN_IF(!mgr)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsRefPtr<IDBFactory> factory = new IDBFactory();
  factory->mPrincipalInfo = aPrincipalInfo.forget();
  factory->mOwningObject = aOwningObject;

  mozilla::HoldJSObjects(factory.get());
  factory->mRootedOwningObject = true;

  factory.forget(aFactory);
  return NS_OK;
}

#ifdef DEBUG

void
IDBFactory::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(mOwningThread);
  MOZ_ASSERT(PR_GetCurrentThread() == mOwningThread);
}

#endif // DEBUG

void
IDBFactory::SetBackgroundActor(BackgroundFactoryChild* aBackgroundActor)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aBackgroundActor);
  MOZ_ASSERT(!mBackgroundActor);

  mBackgroundActor = aBackgroundActor;
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

    if (NS_WARN_IF(NS_FAILED(GetPrincipalInfoFromPrincipal(aPrincipal,
                                                           &principalInfo)))) {
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

  // XXX We need a bug to switch to temporary storage by default.

  PersistenceType persistenceType;
  bool persistenceTypeIsExplicit;

  if (principalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    // Chrome privilege always gets persistent storage.
    persistenceType = PERSISTENCE_TYPE_PERSISTENT;
    persistenceTypeIsExplicit = false;
  } else {
    persistenceType = PersistenceTypeFromStorage(aStorageType);
    persistenceTypeIsExplicit = aStorageType.WasPassed();
  }

  DatabaseMetadata& metadata = commonParams.metadata();
  metadata.name() = aName;
  metadata.persistenceType() = persistenceType;
  metadata.persistenceTypeIsExplicit() = persistenceTypeIsExplicit;

  FactoryRequestParams params;
  if (aDeleting) {
    metadata.version() = 0;
    params = DeleteDatabaseRequestParams(commonParams);
  } else {
    metadata.version() = version;
    params = OpenDatabaseRequestParams(commonParams);
  }

  if (!mBackgroundActor) {
    // If another consumer has already created a background actor for this
    // thread then we can start this request immediately.
    if (PBackgroundChild* bgActor = BackgroundChild::GetForCurrentThread()) {
      nsresult rv = BackgroundActorCreated(bgActor);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        IDB_REPORT_INTERNAL_ERR();
        aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
        return nullptr;
      }
      MOZ_ASSERT(mBackgroundActor);
    } else if (mPendingRequests.IsEmpty()) {
      // We need to start the sequence to create a background actor for this
      // thread.
      nsRefPtr<BackgroundCreateCallback> cb =
        new BackgroundCreateCallback(this);
      if (NS_WARN_IF(!BackgroundChild::GetOrCreateForCurrentThread(cb))) {
        IDB_REPORT_INTERNAL_ERR();
        aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
        return nullptr;
      }
    }
  }

  AutoJSAPI autoJS;
  nsRefPtr<IDBOpenDBRequest> request;

  if (mWindow) {
    if (NS_WARN_IF(!autoJS.Init(mWindow))) {
      IDB_REPORT_INTERNAL_ERR();
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      return nullptr;
    }

    JS::Rooted<JSObject*> scriptOwner(autoJS.cx(),
      static_cast<nsGlobalWindow*>(mWindow.get())->FastGetGlobalJSObject());
    MOZ_ASSERT(scriptOwner);

    request = IDBOpenDBRequest::CreateForWindow(this, mWindow, scriptOwner);
  } else {
    autoJS.Init();
    JS::Rooted<JSObject*> scriptOwner(autoJS.cx(), mOwningObject);

    request = IDBOpenDBRequest::CreateForJS(this, scriptOwner);
  }

  MOZ_ASSERT(request);

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

#ifdef IDB_PROFILER_USE_MARKS
  {
    NS_ConvertUTF16toUTF8 profilerName(aName);
    if (aDeleting) {
      IDB_PROFILER_MARK("IndexedDB Request %llu: deleteDatabase(\"%s\")",
                        "MT IDBFactory.deleteDatabase()",
                        request->GetSerialNumber(), profilerName.get());
    } else {
      IDB_PROFILER_MARK("IndexedDB Request %llu: open(\"%s\", %lld)",
                        "MT IDBFactory.open()",
                        request->GetSerialNumber(), profilerName.get(),
                        aVersion);
    }
  }
#endif

  return request.forget();
}

nsresult
IDBFactory::BackgroundActorCreated(PBackgroundChild* aBackgroundActor)
{
  MOZ_ASSERT(aBackgroundActor);
  MOZ_ASSERT(!mBackgroundActor);
  MOZ_ASSERT(!mBackgroundActorFailed);

  {
    BackgroundFactoryChild* actor = new BackgroundFactoryChild(this);

    MOZ_ASSERT(NS_IsMainThread(), "Fix this windowId stuff for workers!");

    OptionalWindowId windowId;
    if (mWindow && IndexedDatabaseManager::IsMainProcess()) {
      MOZ_ASSERT(mWindow->IsInnerWindow());
      windowId = mWindow->WindowID();
    } else {
      windowId = void_t();
    }

    mBackgroundActor =
      static_cast<BackgroundFactoryChild*>(
        aBackgroundActor->SendPBackgroundIDBFactoryConstructor(actor,
                                                               windowId));
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
  if (tmp->mOwningObject) {
    tmp->mOwningObject = nullptr;
  }
  if (tmp->mRootedOwningObject) {
    mozilla::DropJSObjects(tmp);
    tmp->mRootedOwningObject = false;
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IDBFactory)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mOwningObject)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

JSObject*
IDBFactory::WrapObject(JSContext* aCx)
{
  return IDBFactoryBinding::Wrap(aCx, this);
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

  factory->BackgroundActorCreated(aActor);
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
