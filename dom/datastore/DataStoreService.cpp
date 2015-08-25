/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DataStoreService.h"

#include "DataStoreCallbacks.h"
#include "DataStoreDB.h"
#include "DataStoreRevision.h"
#include "mozilla/dom/DataStore.h"
#include "mozilla/dom/DataStoreBinding.h"
#include "mozilla/dom/DataStoreImplBinding.h"
#include "nsIDataStore.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/indexedDB/IDBCursor.h"
#include "mozilla/dom/indexedDB/IDBObjectStore.h"
#include "mozilla/dom/indexedDB/IDBRequest.h"
#include "mozilla/dom/indexedDB/IDBTransaction.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/unused.h"

#include "mozIApplication.h"
#include "mozIApplicationClearPrivateDataParams.h"
#include "nsIAppsService.h"
#include "nsIDOMEvent.h"
#include "nsIDocument.h"
#include "nsIDOMGlobalPropertyInitializer.h"
#include "nsIIOService.h"
#include "nsIMutableArray.h"
#include "nsIObserverService.h"
#include "nsIPermissionManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsISupportsPrimitives.h"
#include "nsIUUIDGenerator.h"
#include "nsPIDOMWindow.h"
#include "nsIURI.h"

#include "nsContentUtils.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#define ASSERT_PARENT_PROCESS()                                             \
  MOZ_ASSERT(XRE_IsParentProcess());                                        \
  if (NS_WARN_IF(!XRE_IsParentProcess())) {                                 \
    return NS_ERROR_FAILURE;                                                \
  }

using mozilla::BasePrincipal;
using mozilla::OriginAttributes;

namespace mozilla {
namespace dom {

using namespace indexedDB;

// This class contains all the information about a DataStore.
class DataStoreInfo
{
public:
  DataStoreInfo()
    : mReadOnly(true)
    , mEnabled(false)
  {}

  DataStoreInfo(const nsAString& aName,
                const nsAString& aOriginURL,
                const nsAString& aManifestURL,
                bool aReadOnly,
                bool aEnabled)
  {
    Init(aName, aOriginURL, aManifestURL, aReadOnly, aEnabled);
  }

  void Init(const nsAString& aName,
            const nsAString& aOriginURL,
            const nsAString& aManifestURL,
            bool aReadOnly,
            bool aEnabled)
  {
    mName = aName;
    mOriginURL = aOriginURL;
    mManifestURL = aManifestURL;
    mReadOnly = aReadOnly;
    mEnabled = aEnabled;
  }

  void Update(const nsAString& aName,
              const nsAString& aOriginURL,
              const nsAString& aManifestURL,
              bool aReadOnly)
  {
    mName = aName;
    mOriginURL = aOriginURL;
    mManifestURL = aManifestURL;
    mReadOnly = aReadOnly;
  }

  void Enable()
  {
    mEnabled = true;
  }

  nsString mName;
  nsString mOriginURL;
  nsString mManifestURL;
  bool mReadOnly;

  // A DataStore is enabled when it has its first revision.
  bool mEnabled;
};

namespace {

// Singleton for DataStoreService.
StaticRefPtr<DataStoreService> gDataStoreService;
static uint64_t gCounterID = 0;

typedef nsClassHashtable<nsUint32HashKey, DataStoreInfo> HashApp;

void
RejectPromise(nsPIDOMWindow* aWindow, Promise* aPromise, nsresult aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(NS_FAILED(aRv));

  nsRefPtr<DOMError> error;
  if (aRv == NS_ERROR_DOM_SECURITY_ERR) {
    error = new DOMError(aWindow, NS_LITERAL_STRING("SecurityError"),
                         NS_LITERAL_STRING("Access denied"));
  } else {
    error = new DOMError(aWindow, NS_LITERAL_STRING("InternalError"),
                         NS_LITERAL_STRING("An error occurred"));
  }

  aPromise->MaybeRejectBrokenly(error);
}

void
DeleteDatabase(const nsAString& aName,
               const nsAString& aManifestURL)
{
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  nsRefPtr<DataStoreDB> db = new DataStoreDB(aManifestURL, aName);
  db->Delete();
}

PLDHashOperator
DeleteDataStoresAppEnumerator(
                             const uint32_t& aAppId,
                             nsAutoPtr<DataStoreInfo>& aInfo,
                             void* aUserData)
{
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  auto* appId = static_cast<uint32_t*>(aUserData);
  if (*appId != aAppId) {
    return PL_DHASH_NEXT;
  }

  DeleteDatabase(aInfo->mName, aInfo->mManifestURL);
  return PL_DHASH_REMOVE;
}

PLDHashOperator
DeleteDataStoresEnumerator(const nsAString& aName,
                           nsAutoPtr<HashApp>& aApps,
                           void* aUserData)
{
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  aApps->Enumerate(DeleteDataStoresAppEnumerator, aUserData);
  return aApps->Count() ? PL_DHASH_NEXT : PL_DHASH_REMOVE;
}

void
GeneratePermissionName(nsAString& aPermission,
                       const nsAString& aName,
                       const nsAString& aManifestURL)
{
  aPermission.AssignLiteral("indexedDB-chrome-");
  aPermission.Append(aName);
  aPermission.Append('|');
  aPermission.Append(aManifestURL);
}

nsresult
ResetPermission(uint32_t aAppId, const nsAString& aOriginURL,
                const nsAString& aManifestURL,
                const nsAString& aPermission,
                bool aReadOnly)
{
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  nsresult rv;
  nsCOMPtr<nsIIOService> ioService(do_GetService(NS_IOSERVICE_CONTRACTID, &rv));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIURI> uri;
  rv = ioService->NewURI(NS_ConvertUTF16toUTF8(aOriginURL), nullptr, nullptr,
                         getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  OriginAttributes attrs(aAppId, false);
  nsCOMPtr<nsIPrincipal> principal =
    BasePrincipal::CreateCodebasePrincipal(uri, attrs);
  NS_ENSURE_TRUE(principal, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPermissionManager> pm =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  if (!pm) {
    return NS_ERROR_FAILURE;
  }

  nsCString basePermission;
  basePermission.Append(NS_ConvertUTF16toUTF8(aPermission));

  // Write permission
  {
    nsCString permission;
    permission.Append(basePermission);
    permission.AppendLiteral("-write");

    uint32_t perm = nsIPermissionManager::UNKNOWN_ACTION;
    rv = pm->TestExactPermissionFromPrincipal(principal, permission.get(),
                                              &perm);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (aReadOnly && perm == nsIPermissionManager::ALLOW_ACTION) {
      rv = pm->RemoveFromPrincipal(principal, permission.get());
    }
    else if (!aReadOnly && perm != nsIPermissionManager::ALLOW_ACTION) {
      rv = pm->AddFromPrincipal(principal, permission.get(),
                                nsIPermissionManager::ALLOW_ACTION,
                                nsIPermissionManager::EXPIRE_NEVER, 0);
    }

    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Read permission
  {
    nsCString permission;
    permission.Append(basePermission);
    permission.AppendLiteral("-read");

    uint32_t perm = nsIPermissionManager::UNKNOWN_ACTION;
    rv = pm->TestExactPermissionFromPrincipal(principal, permission.get(),
                                              &perm);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (perm != nsIPermissionManager::ALLOW_ACTION) {
      rv = pm->AddFromPrincipal(principal, permission.get(),
                                nsIPermissionManager::ALLOW_ACTION,
                                nsIPermissionManager::EXPIRE_NEVER, 0);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  // Generic permission
  uint32_t perm = nsIPermissionManager::UNKNOWN_ACTION;
  rv = pm->TestExactPermissionFromPrincipal(principal, basePermission.get(),
                                            &perm);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (perm != nsIPermissionManager::ALLOW_ACTION) {
    rv = pm->AddFromPrincipal(principal, basePermission.get(),
                              nsIPermissionManager::ALLOW_ACTION,
                              nsIPermissionManager::EXPIRE_NEVER, 0);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

class MOZ_STACK_CLASS GetDataStoreInfosData
{
public:
  GetDataStoreInfosData(nsClassHashtable<nsStringHashKey, HashApp>& aAccessStores,
                        const nsAString& aName, const nsAString& aManifestURL,
                        uint32_t aAppId, nsTArray<DataStoreInfo>& aStores)
    : mAccessStores(aAccessStores)
    , mName(aName)
    , mManifestURL(aManifestURL)
    , mAppId(aAppId)
    , mStores(aStores)
  {}

  nsClassHashtable<nsStringHashKey, HashApp>& mAccessStores;
  nsString mName;
  nsString mManifestURL;
  uint32_t mAppId;
  nsTArray<DataStoreInfo>& mStores;
};

PLDHashOperator
GetDataStoreInfosEnumerator(const uint32_t& aAppId,
                            DataStoreInfo* aInfo,
                            void* aUserData)
{
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  auto* data = static_cast<GetDataStoreInfosData*>(aUserData);
  if (aAppId == data->mAppId) {
    return PL_DHASH_NEXT;
  }

  HashApp* apps;
  if (!data->mAccessStores.Get(data->mName, &apps)) {
    return PL_DHASH_NEXT;
  }

  if (!data->mManifestURL.IsEmpty() &&
      !data->mManifestURL.Equals(aInfo->mManifestURL)) {
    return PL_DHASH_NEXT;
  }

  DataStoreInfo* accessInfo = nullptr;
  if (!apps->Get(data->mAppId, &accessInfo)) {
    return PL_DHASH_NEXT;
  }

  bool readOnly = aInfo->mReadOnly || accessInfo->mReadOnly;
  DataStoreInfo* accessStore = data->mStores.AppendElement();
  accessStore->Init(aInfo->mName, aInfo->mOriginURL,
                    aInfo->mManifestURL, readOnly,
                    aInfo->mEnabled);

  return PL_DHASH_NEXT;
}

PLDHashOperator
GetAppManifestURLsEnumerator(const uint32_t& aAppId,
                             DataStoreInfo* aInfo,
                             void* aUserData)
{
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  auto* manifestURLs = static_cast<nsIMutableArray*>(aUserData);
  nsCOMPtr<nsISupportsString> manifestURL(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  if (manifestURL) {
    manifestURL->SetData(aInfo->mManifestURL);
    manifestURLs->AppendElement(manifestURL, false);
  }

  return PL_DHASH_NEXT;
}

// This class is useful to enumerate the add permissions for each app.
class MOZ_STACK_CLASS AddPermissionsData
{
public:
  AddPermissionsData(const nsAString& aPermission, bool aReadOnly)
    : mPermission(aPermission)
    , mReadOnly(aReadOnly)
    , mResult(NS_OK)
  {}

  nsString mPermission;
  bool mReadOnly;
  nsresult mResult;
};

PLDHashOperator
AddPermissionsEnumerator(const uint32_t& aAppId,
                         DataStoreInfo* aInfo,
                         void* userData)
{
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  auto* data = static_cast<AddPermissionsData*>(userData);

  // ReadOnly is decided by the owner first.
  bool readOnly = data->mReadOnly || aInfo->mReadOnly;

  data->mResult = ResetPermission(aAppId, aInfo->mOriginURL,
                                  aInfo->mManifestURL,
                                  data->mPermission,
                                  readOnly);
  return NS_FAILED(data->mResult) ? PL_DHASH_STOP : PL_DHASH_NEXT;
}

// This class is useful to enumerate the add permissions for each app.
class MOZ_STACK_CLASS AddAccessPermissionsData
{
public:
  AddAccessPermissionsData(uint32_t aAppId, const nsAString& aName,
                           const nsAString& aOriginURL, bool aReadOnly)
    : mAppId(aAppId)
    , mName(aName)
    , mOriginURL(aOriginURL)
    , mReadOnly(aReadOnly)
    , mResult(NS_OK)
  {}

  uint32_t mAppId;
  nsString mName;
  nsString mOriginURL;
  bool mReadOnly;
  nsresult mResult;
};

PLDHashOperator
AddAccessPermissionsEnumerator(const uint32_t& aAppId,
                               DataStoreInfo* aInfo,
                               void* userData)
{
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  auto* data = static_cast<AddAccessPermissionsData*>(userData);

  nsString permission;
  GeneratePermissionName(permission, data->mName, aInfo->mManifestURL);

  // ReadOnly is decided by the owner first.
  bool readOnly = aInfo->mReadOnly || data->mReadOnly;

  data->mResult = ResetPermission(data->mAppId, data->mOriginURL,
                                  aInfo->mManifestURL,
                                  permission, readOnly);
  return NS_FAILED(data->mResult) ? PL_DHASH_STOP : PL_DHASH_NEXT;
}

} /* anonymous namespace */

// A PendingRequest is created when a content code wants a list of DataStores
// but some of them are not enabled yet.
class PendingRequest
{
public:
  void Init(nsPIDOMWindow* aWindow, Promise* aPromise,
            const nsTArray<DataStoreInfo>& aStores,
            const nsTArray<nsString>& aPendingDataStores)
  {
    mWindow = aWindow;
    mPromise = aPromise;
    mStores = aStores;
    mPendingDataStores = aPendingDataStores;
  }

  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsRefPtr<Promise> mPromise;
  nsTArray<DataStoreInfo> mStores;

  // This array contains the list of manifestURLs of the DataStores that are
  // not enabled yet.
  nsTArray<nsString> mPendingDataStores;
};

// This callback is used to enable a DataStore when its first revisionID is
// created.
class RevisionAddedEnableStoreCallback final :
  public DataStoreRevisionCallback
{
private:
  ~RevisionAddedEnableStoreCallback() {}
public:
  NS_INLINE_DECL_REFCOUNTING(RevisionAddedEnableStoreCallback);

  RevisionAddedEnableStoreCallback(uint32_t aAppId,
                                   const nsAString& aName,
                                   const nsAString& aManifestURL)
    : mAppId(aAppId)
    , mName(aName)
    , mManifestURL(aManifestURL)
  {
    MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());
  }

  void
  Run(const nsAString& aRevisionId)
  {
    MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

    nsRefPtr<DataStoreService> service = DataStoreService::Get();
    MOZ_ASSERT(service);

    service->EnableDataStore(mAppId, mName, mManifestURL);
  }

private:
  uint32_t mAppId;
  nsString mName;
  nsString mManifestURL;
};

// This DataStoreDBCallback is called when DataStoreDB opens the DataStore DB.
// Then the first revision will be created if it's needed.
class FirstRevisionIdCallback final : public DataStoreDBCallback
                                    , public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS

  FirstRevisionIdCallback(uint32_t aAppId, const nsAString& aName,
                          const nsAString& aManifestURL)
    : mAppId(aAppId)
    , mName(aName)
    , mManifestURL(aManifestURL)
  {
    MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());
  }

  void
  Run(DataStoreDB* aDb, RunStatus aStatus) override
  {
    MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());
    MOZ_ASSERT(aDb);

    if (aStatus == Error) {
      NS_WARNING("Failed to create the first revision.");
      return;
    }

    ErrorResult error;

    if (aStatus == Success) {
      mTxn = aDb->Transaction();

      nsRefPtr<IDBObjectStore> store =
      mTxn->ObjectStore(NS_LITERAL_STRING(DATASTOREDB_REVISION), error);
      if (NS_WARN_IF(error.Failed())) {
        return;
      }

      AutoSafeJSContext cx;
      mRequest = store->OpenCursor(cx, JS::UndefinedHandleValue,
                                   IDBCursorDirection::Prev, error);
      if (NS_WARN_IF(error.Failed())) {
        return;
      }

      nsresult rv;
      rv = mRequest->EventTarget::AddEventListener(NS_LITERAL_STRING("success"),
                                                   this, false);
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to add an EventListener.");
        return;
      }

      return;
    }

    // The DB has just been created.

    error = CreateFirstRevision(aDb->Transaction());
    if (error.Failed()) {
      NS_WARNING("Failed to add a revision to a DataStore.");
    }
  }

  nsresult
  CreateFirstRevision(IDBTransaction* aTxn)
  {
    MOZ_ASSERT(aTxn);

    ErrorResult error;
    nsRefPtr<IDBObjectStore> store =
      aTxn->ObjectStore(NS_LITERAL_STRING(DATASTOREDB_REVISION), error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    MOZ_ASSERT(store);

    nsRefPtr<RevisionAddedEnableStoreCallback> callback =
      new RevisionAddedEnableStoreCallback(mAppId, mName, mManifestURL);

    // Note: this cx is only used for rooting and AddRevision, neither of which
    // actually care which compartment we're in.
    AutoSafeJSContext cx;

    // If the revision doesn't exist, let's create it.
    nsRefPtr<DataStoreRevision> revision = new DataStoreRevision();
    nsresult rv = revision->AddRevision(cx, store, 0,
                                        DataStoreRevision::RevisionVoid,
                                        callback);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  // nsIDOMEventListener
  NS_IMETHOD
  HandleEvent(nsIDOMEvent* aEvent) override
  {
    MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

    nsRefPtr<IDBRequest> request;
    request.swap(mRequest);

    nsRefPtr<IDBTransaction> txn;
    txn.swap(mTxn);

    request->RemoveEventListener(NS_LITERAL_STRING("success"), this, false);

    nsString type;
    nsresult rv = aEvent->GetType(type);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

#ifdef DEBUG
    MOZ_ASSERT(type.EqualsASCII("success"));
#endif

    AutoSafeJSContext cx;

    ErrorResult error;
    JS::Rooted<JS::Value> result(cx);
    request->GetResult(cx, &result, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }

    // This means that the content is a IDBCursor, so the first revision already
    // exists.
    if (result.isObject()) {
#ifdef DEBUG
      IDBCursor* cursor = nullptr;
      error = UNWRAP_OBJECT(IDBCursor, &result.toObject(), cursor);
      MOZ_ASSERT(!error.Failed());
#endif

      nsRefPtr<DataStoreService> service = DataStoreService::Get();
      MOZ_ASSERT(service);

      return service->EnableDataStore(mAppId, mName, mManifestURL);
    }

    rv = CreateFirstRevision(txn);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

private:
  ~FirstRevisionIdCallback() {}

  nsRefPtr<IDBRequest> mRequest;
  nsRefPtr<IDBTransaction> mTxn;

  uint32_t mAppId;
  nsString mName;
  nsString mManifestURL;
};

NS_IMPL_ISUPPORTS(FirstRevisionIdCallback, nsIDOMEventListener)

// This class calls the 'retrieveRevisionId' method of the DataStore object for
// any DataStore in the 'mResults' array. When all of them are called, the
// promise is resolved with 'mResults'.
// The reson why this has to be done is because DataStore are object that can be
// created in any thread and in any process. The first revision has been
// created, but they don't know its value yet.
class RetrieveRevisionsCounter
{
private:
  ~RetrieveRevisionsCounter() {}
public:
  NS_INLINE_DECL_REFCOUNTING(RetrieveRevisionsCounter);

  RetrieveRevisionsCounter(uint32_t aId, Promise* aPromise, uint32_t aCount)
    : mPromise(aPromise)
    , mId(aId)
    , mCount(aCount)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  void
  AppendDataStore(JSContext* aCx, DataStore* aDataStore,
                  nsIDataStore* aDataStoreIf)
  {
    MOZ_ASSERT(NS_IsMainThread());

    mResults.AppendElement(aDataStore);

    // DataStore will run this callback when the revisionID is retrieved.
    JSFunction* func = js::NewFunctionWithReserved(aCx, JSCallback,
                                                   0 /* nargs */, 0 /* flags */,
                                                   nullptr);
    if (!func) {
      return;
    }

    JS::Rooted<JSObject*> obj(aCx, JS_GetFunctionObject(func));
    if (!obj) {
      return;
    }

    // We use the ID to know which counter is this. The service keeps all of
    // these counters alive with their own IDs in an hashtable.
    js::SetFunctionNativeReserved(obj, 0, JS::Int32Value(mId));

    JS::Rooted<JS::Value> value(aCx, JS::ObjectValue(*obj));
    nsresult rv = aDataStoreIf->RetrieveRevisionId(value);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

private:
  static bool
  JSCallback(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
  {
    MOZ_ASSERT(NS_IsMainThread());

    JS::CallArgs args = CallArgsFromVp(aArgc, aVp);

    JS::Rooted<JS::Value> value(aCx,
                                js::GetFunctionNativeReserved(&args.callee(), 0));
    uint32_t id = value.toInt32();

    nsRefPtr<DataStoreService> service = DataStoreService::Get();
    MOZ_ASSERT(service);

    nsRefPtr<RetrieveRevisionsCounter> counter = service->GetCounter(id);
    MOZ_ASSERT(counter);

    // When all the callbacks are called, we can resolve the promise and remove
    // the counter from the service.
    --counter->mCount;
    if (!counter->mCount) {
      service->RemoveCounter(id);
      counter->mPromise->MaybeResolve(counter->mResults);
    }

    return true;
  }

  nsRefPtr<Promise> mPromise;
  nsTArray<nsRefPtr<DataStore>> mResults;

  uint32_t mId;
  uint32_t mCount;
};

/* static */ already_AddRefed<DataStoreService>
DataStoreService::GetOrCreate()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gDataStoreService) {
    nsRefPtr<DataStoreService> service = new DataStoreService();
    if (NS_WARN_IF(NS_FAILED(service->Init()))) {
      return nullptr;
    }

    gDataStoreService = service;
  }

  nsRefPtr<DataStoreService> service = gDataStoreService.get();
  return service.forget();
}

/* static */ already_AddRefed<DataStoreService>
DataStoreService::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<DataStoreService> service = gDataStoreService.get();
  return service.forget();
}

/* static */ void
DataStoreService::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gDataStoreService) {
    if (XRE_IsParentProcess()) {
      nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
      if (obs) {
        obs->RemoveObserver(gDataStoreService, "webapps-clear-data");
      }
    }

    gDataStoreService = nullptr;
  }
}

NS_INTERFACE_MAP_BEGIN(DataStoreService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDataStoreService)
  NS_INTERFACE_MAP_ENTRY(nsIDataStoreService)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(DataStoreService)
NS_IMPL_RELEASE(DataStoreService)

DataStoreService::DataStoreService()
{
  MOZ_ASSERT(NS_IsMainThread());
}

DataStoreService::~DataStoreService()
{
  MOZ_ASSERT(NS_IsMainThread());
}

nsresult
DataStoreService::Init()
{
  if (!XRE_IsParentProcess()) {
    return NS_OK;
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = obs->AddObserver(this, "webapps-clear-data", false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
DataStoreService::InstallDataStore(uint32_t aAppId,
                                   const nsAString& aName,
                                   const nsAString& aOriginURL,
                                   const nsAString& aManifestURL,
                                   bool aReadOnly)
{
  ASSERT_PARENT_PROCESS()
  MOZ_ASSERT(NS_IsMainThread());

  HashApp* apps = nullptr;
  if (!mStores.Get(aName, &apps)) {
    apps = new HashApp();
    mStores.Put(aName, apps);
  }

  DataStoreInfo* info = nullptr;
  if (!apps->Get(aAppId, &info)) {
    info = new DataStoreInfo(aName, aOriginURL, aManifestURL, aReadOnly, false);
    apps->Put(aAppId, info);
  } else {
    info->Update(aName, aOriginURL, aManifestURL, aReadOnly);
  }

  nsresult rv = AddPermissions(aAppId, aName, aOriginURL, aManifestURL,
                               aReadOnly);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Immediately create the first revision.
  return CreateFirstRevisionId(aAppId, aName, aManifestURL);
}

NS_IMETHODIMP
DataStoreService::InstallAccessDataStore(uint32_t aAppId,
                                         const nsAString& aName,
                                         const nsAString& aOriginURL,
                                         const nsAString& aManifestURL,
                                         bool aReadOnly)
{
  ASSERT_PARENT_PROCESS()
  MOZ_ASSERT(NS_IsMainThread());

  HashApp* apps = nullptr;
  if (!mAccessStores.Get(aName, &apps)) {
    apps = new HashApp();
    mAccessStores.Put(aName, apps);
  }

  DataStoreInfo* info = nullptr;
  if (!apps->Get(aAppId, &info)) {
    info = new DataStoreInfo(aName, aOriginURL, aManifestURL, aReadOnly, false);
    apps->Put(aAppId, info);
  } else {
    info->Update(aName, aOriginURL, aManifestURL, aReadOnly);
  }

  return AddAccessPermissions(aAppId, aName, aOriginURL, aManifestURL,
                              aReadOnly);
}

NS_IMETHODIMP
DataStoreService::GetDataStores(nsIDOMWindow* aWindow,
                                const nsAString& aName,
                                const nsAString& aOwner,
                                nsISupports** aDataStores)
{
  // FIXME This will be a thread-safe method.
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);
  if (!window) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(window);
  ErrorResult rv;
  nsRefPtr<Promise> promise = Promise::Create(global, rv);
  if (rv.Failed()) {
    return rv.StealNSResult();
  }

  nsCOMPtr<nsIDocument> document = window->GetDoc();
  MOZ_ASSERT(document);

  nsCOMPtr<nsIPrincipal> principal = document->NodePrincipal();
  MOZ_ASSERT(principal);

  nsTArray<DataStoreInfo> stores;

  // If this request comes from the main process, we have access to the
  // window, so we can skip the ipc communication.
  if (XRE_IsParentProcess()) {
    uint32_t appId;
    nsresult rv = principal->GetAppId(&appId);
    if (NS_FAILED(rv)) {
      RejectPromise(window, promise, rv);
      promise.forget(aDataStores);
      return NS_OK;
    }

    rv = GetDataStoreInfos(aName, aOwner, appId, principal, stores);
    if (NS_FAILED(rv)) {
      RejectPromise(window, promise, rv);
      promise.forget(aDataStores);
      return NS_OK;
    }
  }

  else {
    // This method can be called in the child so we need to send a request
    // to the parent and create DataStore object here.
    ContentChild* contentChild = ContentChild::GetSingleton();

    nsTArray<DataStoreSetting> array;
    if (!contentChild->SendDataStoreGetStores(nsAutoString(aName),
                                              nsAutoString(aOwner),
                                              IPC::Principal(principal),
                                              &array)) {
      RejectPromise(window, promise, NS_ERROR_FAILURE);
      promise.forget(aDataStores);
      return NS_OK;
    }

    for (uint32_t i = 0; i < array.Length(); ++i) {
      DataStoreInfo* info = stores.AppendElement();
      info->Init(array[i].name(), array[i].originURL(),
                 array[i].manifestURL(), array[i].readOnly(),
                 array[i].enabled());
    }
  }

  GetDataStoresCreate(window, promise, stores);
  promise.forget(aDataStores);
  return NS_OK;
}

void
DataStoreService::GetDataStoresCreate(nsPIDOMWindow* aWindow, Promise* aPromise,
                                      const nsTArray<DataStoreInfo>& aStores)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aStores.Length()) {
    GetDataStoresResolve(aWindow, aPromise, aStores);
    return;
  }

  nsTArray<nsString> pendingDataStores;
  for (uint32_t i = 0; i < aStores.Length(); ++i) {
    if (!aStores[i].mEnabled) {
      pendingDataStores.AppendElement(aStores[i].mManifestURL);
    }
  }

  if (!pendingDataStores.Length()) {
    GetDataStoresResolve(aWindow, aPromise, aStores);
    return;
  }

  PendingRequests* requests;
  if (!mPendingRequests.Get(aStores[0].mName, &requests)) {
    requests = new PendingRequests();
    mPendingRequests.Put(aStores[0].mName, requests);
  }

  PendingRequest* request = requests->AppendElement();
  request->Init(aWindow, aPromise, aStores, pendingDataStores);
}

void
DataStoreService::GetDataStoresResolve(nsPIDOMWindow* aWindow,
                                       Promise* aPromise,
                                       const nsTArray<DataStoreInfo>& aStores)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aStores.Length()) {
    nsTArray<nsRefPtr<DataStore>> results;
    aPromise->MaybeResolve(results);
    return;
  }

  AutoSafeJSContext cx;

  // The counter will finish this task once all the DataStores will know their
  // first revision Ids.
  nsRefPtr<RetrieveRevisionsCounter> counter =
    new RetrieveRevisionsCounter(++gCounterID, aPromise, aStores.Length());
  mPendingCounters.Put(gCounterID, counter);

  for (uint32_t i = 0; i < aStores.Length(); ++i) {
    nsCOMPtr<nsIDataStore> dataStore =
      do_CreateInstance("@mozilla.org/dom/datastore;1");
    if (NS_WARN_IF(!dataStore)) {
      return;
    }

    nsresult rv = dataStore->Init(aWindow, aStores[i].mName,
                                  aStores[i].mManifestURL,
                                  aStores[i].mReadOnly);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    nsCOMPtr<nsIXPConnectWrappedJS> xpcwrappedjs = do_QueryInterface(dataStore);
    if (NS_WARN_IF(!xpcwrappedjs)) {
      return;
    }

    JS::Rooted<JSObject*> dataStoreJS(cx, xpcwrappedjs->GetJSObject());
    if (NS_WARN_IF(!dataStoreJS)) {
      return;
    }

    nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aWindow);
    MOZ_ASSERT(global);

    JSAutoCompartment ac(cx, dataStoreJS);
    nsRefPtr<DataStoreImpl> dataStoreObj = new DataStoreImpl(dataStoreJS,
                                                             global);

    nsRefPtr<DataStore> exposedStore = new DataStore(aWindow);

    ErrorResult error;
    exposedStore->SetDataStoreImpl(*dataStoreObj, error);
    if (error.Failed()) {
      return;
    }

    JS::Rooted<JS::Value> exposedObject(cx);
    if (!GetOrCreateDOMReflector(cx, exposedStore, &exposedObject)) {
      JS_ClearPendingException(cx);
      return;
    }

    dataStore->SetExposedObject(exposedObject);

    counter->AppendDataStore(cx, exposedStore, dataStore);
  }
}

// Thie method populates 'aStores' with the list of DataStores with 'aName' as
// name and available for this 'aAppId'.
nsresult
DataStoreService::GetDataStoreInfos(const nsAString& aName,
                                    const nsAString& aOwner,
                                    uint32_t aAppId,
                                    nsIPrincipal* aPrincipal,
                                    nsTArray<DataStoreInfo>& aStores)
{
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  nsCOMPtr<nsIAppsService> appsService =
    do_GetService("@mozilla.org/AppsService;1");
  if (NS_WARN_IF(!appsService)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<mozIApplication> app;
  nsresult rv = appsService->GetAppByLocalId(aAppId, getter_AddRefs(app));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!app) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (!DataStoreService::CheckPermission(aPrincipal)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  aStores.Clear();

  HashApp* apps = nullptr;
  if (!mStores.Get(aName, &apps)) {
    return NS_OK;
  }

  DataStoreInfo* info = nullptr;
  if (apps->Get(aAppId, &info) &&
      (aOwner.IsEmpty() || aOwner.Equals(info->mManifestURL))) {
    DataStoreInfo* owned = aStores.AppendElement();
    owned->Init(info->mName, info->mOriginURL, info->mManifestURL, false,
                info->mEnabled);
  }

  GetDataStoreInfosData data(mAccessStores, aName, aOwner, aAppId, aStores);
  apps->EnumerateRead(GetDataStoreInfosEnumerator, &data);
  return NS_OK;
}

NS_IMETHODIMP
DataStoreService::GetAppManifestURLsForDataStore(const nsAString& aName,
                                                 nsIArray** aManifestURLs)
{
  ASSERT_PARENT_PROCESS()
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIMutableArray> manifestURLs = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!manifestURLs) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  HashApp* apps = nullptr;
  if (mStores.Get(aName, &apps)) {
    apps->EnumerateRead(GetAppManifestURLsEnumerator, manifestURLs.get());
  }
  if (mAccessStores.Get(aName, &apps)) {
    apps->EnumerateRead(GetAppManifestURLsEnumerator, manifestURLs.get());
  }

  manifestURLs.forget(aManifestURLs);
  return NS_OK;
}

bool
DataStoreService::CheckPermission(nsIPrincipal* aPrincipal)
{
  // First of all, the general pref has to be turned on.
  bool enabled = false;
  Preferences::GetBool("dom.datastore.enabled", &enabled);
  if (!enabled) {
    return false;
  }

  // Just for testing, we can enable DataStore for any kind of app.
  if (Preferences::GetBool("dom.testing.datastore_enabled_for_hosted_apps", false)) {
    return true;
  }

  if (!aPrincipal) {
    return false;
  }

  uint16_t status;
  if (NS_FAILED(aPrincipal->GetAppStatus(&status))) {
    return false;
  }

  // Certified apps are always allowed.
  if (status == nsIPrincipal::APP_STATUS_CERTIFIED) {
    return true;
  }

  if (status != nsIPrincipal::APP_STATUS_PRIVILEGED) {
    return false;
  }

  // Privileged apps are allowed if they are the homescreen.
  nsAdoptingString homescreen =
    Preferences::GetString("dom.mozApps.homescreenURL");
  if (!homescreen) {
    return false;
  }

  uint32_t appId;
  nsresult rv = aPrincipal->GetAppId(&appId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  nsCOMPtr<nsIAppsService> appsService =
    do_GetService("@mozilla.org/AppsService;1");
  if (NS_WARN_IF(!appsService)) {
    return false;
  }

  nsAutoString manifestURL;
  rv = appsService->GetManifestURLByLocalId(appId, manifestURL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return manifestURL.Equals(homescreen);
}

NS_IMETHODIMP
DataStoreService::CheckPermission(nsIPrincipal* aPrincipal,
                                  bool* aResult)
{
  MOZ_ASSERT(NS_IsMainThread());

  *aResult = DataStoreService::CheckPermission(aPrincipal);

  return NS_OK;
}

// This method is called when an app with DataStores is deleted.
void
DataStoreService::DeleteDataStores(uint32_t aAppId)
{
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  mStores.Enumerate(DeleteDataStoresEnumerator, &aAppId);
  mAccessStores.Enumerate(DeleteDataStoresEnumerator, &aAppId);
}

NS_IMETHODIMP
DataStoreService::Observe(nsISupports* aSubject,
                          const char* aTopic,
                          const char16_t* aData)
{
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  if (strcmp(aTopic, "webapps-clear-data")) {
    return NS_OK;
  }

  nsCOMPtr<mozIApplicationClearPrivateDataParams> params =
    do_QueryInterface(aSubject);
  MOZ_ASSERT(params);

  // DataStore is explosed to apps, not browser content.
  bool browserOnly;
  nsresult rv = params->GetBrowserOnly(&browserOnly);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (browserOnly) {
    return NS_OK;
  }

  uint32_t appId;
  rv = params->GetAppId(&appId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  DeleteDataStores(appId);

  return NS_OK;
}

nsresult
DataStoreService::AddPermissions(uint32_t aAppId,
                                 const nsAString& aName,
                                 const nsAString& aOriginURL,
                                 const nsAString& aManifestURL,
                                 bool aReadOnly)
{
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  // This is the permission name.
  nsString permission;
  GeneratePermissionName(permission, aName, aManifestURL);

  // When a new DataStore is installed, the permissions must be set for the
  // owner app.
  nsresult rv = ResetPermission(aAppId, aOriginURL, aManifestURL, permission,
                                aReadOnly);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // For any app that wants to have access to this DataStore we add the
  // permissions.
  HashApp* apps;
  if (!mAccessStores.Get(aName, &apps)) {
    return NS_OK;
  }

  AddPermissionsData data(permission, aReadOnly);
  apps->EnumerateRead(AddPermissionsEnumerator, &data);
  return data.mResult;
}

nsresult
DataStoreService::AddAccessPermissions(uint32_t aAppId, const nsAString& aName,
                                       const nsAString& aOriginURL,
                                       const nsAString& aManifestURL,
                                       bool aReadOnly)
{
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  // When an app wants to have access to a DataStore, the permissions must be
  // set.
  HashApp* apps = nullptr;
  if (!mStores.Get(aName, &apps)) {
    return NS_OK;
  }

  AddAccessPermissionsData data(aAppId, aName, aOriginURL, aReadOnly);
  apps->EnumerateRead(AddAccessPermissionsEnumerator, &data);
  return data.mResult;
}

// This method starts the operation to create the first revision for a DataStore
// if needed.
nsresult
DataStoreService::CreateFirstRevisionId(uint32_t aAppId,
                                        const nsAString& aName,
                                        const nsAString& aManifestURL)
{
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  nsRefPtr<DataStoreDB> db = new DataStoreDB(aManifestURL, aName);

  nsRefPtr<FirstRevisionIdCallback> callback =
    new FirstRevisionIdCallback(aAppId, aName, aManifestURL);

  Sequence<nsString> dbs;
  if (!dbs.AppendElement(NS_LITERAL_STRING(DATASTOREDB_REVISION), fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return db->Open(IDBTransactionMode::Readwrite, dbs, callback);
}

nsresult
DataStoreService::EnableDataStore(uint32_t aAppId, const nsAString& aName,
                                  const nsAString& aManifestURL)
{
  MOZ_ASSERT(NS_IsMainThread());

  {
    HashApp* apps = nullptr;
    DataStoreInfo* info = nullptr;
    if (mStores.Get(aName, &apps) && apps->Get(aAppId, &info)) {
      info->Enable();
    }
  }

  // Notify the child processes.
  if (XRE_IsParentProcess()) {
    nsTArray<ContentParent*> children;
    ContentParent::GetAll(children);
    for (uint32_t i = 0; i < children.Length(); i++) {
      if (children[i]->NeedsDataStoreInfos()) {
        unused << children[i]->SendDataStoreNotify(aAppId, nsAutoString(aName),
                                                   nsAutoString(aManifestURL));
      }
    }
  }

  // Maybe we have some pending request waiting for this DataStore.
  PendingRequests* requests;
  if (!mPendingRequests.Get(aName, &requests)) {
    return NS_OK;
  }

  for (uint32_t i = 0; i < requests->Length();) {
    PendingRequest& request = requests->ElementAt(i);
    nsTArray<nsString>::index_type pos =
      request.mPendingDataStores.IndexOf(aManifestURL);
    if (pos != request.mPendingDataStores.NoIndex) {
      request.mPendingDataStores.RemoveElementAt(pos);

      // No other pending dataStores.
      if (request.mPendingDataStores.IsEmpty()) {
        GetDataStoresResolve(request.mWindow, request.mPromise,
                             request.mStores);
        requests->RemoveElementAt(i);
        continue;
      }
    }

    ++i;
  }

  // No other pending requests for this name.
  if (requests->IsEmpty()) {
    mPendingRequests.Remove(aName);
  }

  return NS_OK;
}

already_AddRefed<RetrieveRevisionsCounter>
DataStoreService::GetCounter(uint32_t aId) const
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<RetrieveRevisionsCounter> counter;
  return mPendingCounters.Get(aId, getter_AddRefs(counter))
           ? counter.forget() : nullptr;
}

void
DataStoreService::RemoveCounter(uint32_t aId)
{
  MOZ_ASSERT(NS_IsMainThread());
  mPendingCounters.Remove(aId);
}

nsresult
DataStoreService::GetDataStoresFromIPC(const nsAString& aName,
                                       const nsAString& aOwner,
                                       nsIPrincipal* aPrincipal,
                                       nsTArray<DataStoreSetting>* aValue)
{
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  uint32_t appId;
  nsresult rv = aPrincipal->GetAppId(&appId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsTArray<DataStoreInfo> stores;
  rv = GetDataStoreInfos(aName, aOwner, appId, aPrincipal, stores);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (uint32_t i = 0; i < stores.Length(); ++i) {
    DataStoreSetting* data = aValue->AppendElement();
    data->name() = stores[i].mName;
    data->originURL() = stores[i].mOriginURL;
    data->manifestURL() = stores[i].mManifestURL;
    data->readOnly() = stores[i].mReadOnly;
    data->enabled() = stores[i].mEnabled;
  }

  return NS_OK;
}

nsresult
DataStoreService::GenerateUUID(nsAString& aID)
{
  nsresult rv;

  if (!mUUIDGenerator) {
    mUUIDGenerator = do_GetService("@mozilla.org/uuid-generator;1", &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsID id;
  rv = mUUIDGenerator->GenerateUUIDInPlace(&id);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  char chars[NSID_LENGTH];
  id.ToProvidedString(chars);
  CopyASCIItoUTF16(chars, aID);

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
