/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "IDBFactory.h"
#include "nsIFile.h"
#include "nsIPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIXPConnect.h"
#include "nsIXPCScriptable.h"

#include "jsdbgapi.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/IDBFactoryBinding.h"
#include "mozilla/dom/PBrowserChild.h"
#include "mozilla/dom/quota/OriginOrPatternString.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/storage.h"
#include "nsComponentManagerUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsGlobalWindow.h"
#include "nsHashKeys.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCID.h"

#include "AsyncConnectionHelper.h"
#include "CheckPermissionsHelper.h"
#include "DatabaseInfo.h"
#include "IDBDatabase.h"
#include "IDBEvents.h"
#include "IDBKeyRange.h"
#include "IndexedDatabaseManager.h"
#include "Key.h"

#include "ipc/IndexedDBChild.h"
#include <algorithm>

USING_INDEXEDDB_NAMESPACE
USING_QUOTA_NAMESPACE

using mozilla::dom::ContentChild;
using mozilla::dom::ContentParent;
using mozilla::dom::NonNull;
using mozilla::dom::Optional;
using mozilla::dom::TabChild;
using mozilla::ErrorResult;

namespace {

struct ObjectStoreInfoMap
{
  ObjectStoreInfoMap()
  : id(INT64_MIN), info(nullptr) { }

  int64_t id;
  ObjectStoreInfo* info;
};

} // anonymous namespace

IDBFactory::IDBFactory()
: mOwningObject(nullptr), mActorChild(nullptr), mActorParent(nullptr),
  mContentParent(nullptr), mRootedOwningObject(false)
{
  SetIsDOMBinding();
}

IDBFactory::~IDBFactory()
{
  NS_ASSERTION(!mActorParent, "Actor parent owns us, how can we be dying?!");
  if (mActorChild) {
    NS_ASSERTION(!IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
    mActorChild->Send__delete__(mActorChild);
    NS_ASSERTION(!mActorChild, "Should have cleared in Send__delete__!");
  }
  if (mRootedOwningObject) {
    mOwningObject = nullptr;
    NS_DROP_JS_OBJECTS(this, IDBFactory);
  }
}

// static
nsresult
IDBFactory::Create(nsPIDOMWindow* aWindow,
                   const nsACString& aASCIIOrigin,
                   ContentParent* aContentParent,
                   IDBFactory** aFactory)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aASCIIOrigin.IsEmpty() || nsContentUtils::IsCallerChrome(),
               "Non-chrome may not supply their own origin!");

  NS_ENSURE_TRUE(aWindow, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (aWindow->IsOuterWindow()) {
    aWindow = aWindow->GetCurrentInnerWindow();
    NS_ENSURE_TRUE(aWindow, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  // Make sure that the manager is up before we do anything here since lots of
  // decisions depend on which process we're running in.
  indexedDB::IndexedDatabaseManager* mgr =
    indexedDB::IndexedDatabaseManager::GetOrCreate();
  NS_ENSURE_TRUE(mgr, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsresult rv;

  nsCString origin(aASCIIOrigin);
  if (origin.IsEmpty()) {
    rv = QuotaManager::GetASCIIOriginFromWindow(aWindow, origin);
    if (NS_FAILED(rv)) {
      // Not allowed.
      *aFactory = nullptr;
      return NS_OK;
    }
  }

  nsRefPtr<IDBFactory> factory = new IDBFactory();
  factory->mASCIIOrigin = origin;
  factory->mWindow = aWindow;
  factory->mContentParent = aContentParent;

  if (!IndexedDatabaseManager::IsMainProcess()) {
    TabChild* tabChild = GetTabChildFrom(aWindow);
    NS_ENSURE_TRUE(tabChild, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    IndexedDBChild* actor = new IndexedDBChild(origin);

    bool allowed;
    tabChild->SendPIndexedDBConstructor(actor, origin, &allowed);

    if (!allowed) {
      actor->Send__delete__(actor);
      *aFactory = nullptr;
      return NS_OK;
    }

    actor->SetFactory(factory);
  }

  factory.forget(aFactory);
  return NS_OK;
}

// static
nsresult
IDBFactory::Create(JSContext* aCx,
                   JSObject* aOwningObject,
                   ContentParent* aContentParent,
                   IDBFactory** aFactory)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aCx, "Null context!");
  NS_ASSERTION(aOwningObject, "Null object!");
  NS_ASSERTION(JS_GetGlobalForObject(aCx, aOwningObject) == aOwningObject,
               "Not a global object!");
  NS_ASSERTION(nsContentUtils::IsCallerChrome(), "Only for chrome!");

  nsCString origin;
  nsresult rv = QuotaManager::GetASCIIOriginFromWindow(nullptr, origin);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<IDBFactory> factory = new IDBFactory();
  factory->mASCIIOrigin = origin;
  factory->mOwningObject = aOwningObject;
  factory->mContentParent = aContentParent;

  if (!IndexedDatabaseManager::IsMainProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    NS_ENSURE_TRUE(contentChild, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    IndexedDBChild* actor = new IndexedDBChild(origin);

    contentChild->SendPIndexedDBConstructor(actor);

    actor->SetFactory(factory);
  }

  factory.forget(aFactory);
  return NS_OK;
}

// static
nsresult
IDBFactory::Create(ContentParent* aContentParent,
                   IDBFactory** aFactory)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
  NS_ASSERTION(nsContentUtils::IsCallerChrome(), "Only for chrome!");
  NS_ASSERTION(aContentParent, "Null ContentParent!");

  NS_ASSERTION(!nsContentUtils::GetCurrentJSContext(), "Should be called from C++");

  nsCOMPtr<nsIPrincipal> principal =
    do_CreateInstance("@mozilla.org/nullprincipal;1");
  NS_ENSURE_TRUE(principal, NS_ERROR_FAILURE);

  SafeAutoJSContext cx;
  JSAutoRequest ar(cx);

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  NS_ASSERTION(xpc, "This should never be null!");

  nsCOMPtr<nsIXPConnectJSObjectHolder> globalHolder;
  nsresult rv = xpc->CreateSandbox(cx, principal, getter_AddRefs(globalHolder));
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject* global;
  rv = globalHolder->GetJSObject(&global);
  NS_ENSURE_SUCCESS(rv, rv);

  // The CreateSandbox call returns a proxy to the actual sandbox object. We
  // don't need a proxy here.
  global = JS_UnwrapObject(global);

  JSAutoCompartment ac(cx, global);

  nsRefPtr<IDBFactory> factory;
  rv = Create(cx, global, aContentParent, getter_AddRefs(factory));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_HOLD_JS_OBJECTS(factory, IDBFactory);
  factory->mRootedOwningObject = true;

  factory.forget(aFactory);
  return NS_OK;
}

// static
already_AddRefed<nsIFileURL>
IDBFactory::GetDatabaseFileURL(nsIFile* aDatabaseFile, const nsACString& aOrigin)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewFileURI(getter_AddRefs(uri), aDatabaseFile);
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIFileURL> fileUrl = do_QueryInterface(uri);
  NS_ASSERTION(fileUrl, "This should always succeed!");

  rv = fileUrl->SetQuery(NS_LITERAL_CSTRING("origin=") + aOrigin);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return fileUrl.forget();
}

// static
already_AddRefed<mozIStorageConnection>
IDBFactory::GetConnection(const nsAString& aDatabaseFilePath,
                          const nsACString& aOrigin)
{
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
  NS_ASSERTION(StringEndsWith(aDatabaseFilePath, NS_LITERAL_STRING(".sqlite")),
               "Bad file path!");

  nsCOMPtr<nsIFile> dbFile(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
  NS_ENSURE_TRUE(dbFile, nullptr);

  nsresult rv = dbFile->InitWithPath(aDatabaseFilePath);
  NS_ENSURE_SUCCESS(rv, nullptr);

  bool exists;
  rv = dbFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, nullptr);
  NS_ENSURE_TRUE(exists, nullptr);

  nsCOMPtr<nsIFileURL> dbFileUrl = GetDatabaseFileURL(dbFile, aOrigin);
  NS_ENSURE_TRUE(dbFileUrl, nullptr);

  nsCOMPtr<mozIStorageService> ss =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(ss, nullptr);

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenDatabaseWithFileURL(dbFileUrl, getter_AddRefs(connection));
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = SetDefaultPragmas(connection);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return connection.forget();
}

// static
nsresult
IDBFactory::SetDefaultPragmas(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Null connection!");

  static const char query[] =
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
    // Switch the journaling mode to TRUNCATE to avoid changing the directory
    // structure at the conclusion of every transaction for devices with slower
    // file systems.
    "PRAGMA journal_mode = TRUNCATE; "
#endif
    // We use foreign keys in lots of places.
    "PRAGMA foreign_keys = ON; "
    // The "INSERT OR REPLACE" statement doesn't fire the update trigger,
    // instead it fires only the insert trigger. This confuses the update
    // refcount function. This behavior changes with enabled recursive triggers,
    // so the statement fires the delete trigger first and then the insert
    // trigger.
    "PRAGMA recursive_triggers = ON;";

  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(query));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

inline
bool
IgnoreWhitespace(PRUnichar c)
{
  return false;
}

// static
nsresult
IDBFactory::LoadDatabaseInformation(mozIStorageConnection* aConnection,
                                    nsIAtom* aDatabaseId,
                                    uint64_t* aVersion,
                                    ObjectStoreInfoArray& aObjectStores)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aConnection, "Null pointer!");

  aObjectStores.Clear();

   // Load object store names and ids.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT name, id, key_path, auto_increment "
    "FROM object_store"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoTArray<ObjectStoreInfoMap, 20> infoMap;

  bool hasResult;
  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    nsRefPtr<ObjectStoreInfo>* element =
      aObjectStores.AppendElement(new ObjectStoreInfo());

    ObjectStoreInfo* info = element->get();

    rv = stmt->GetString(0, info->name);
    NS_ENSURE_SUCCESS(rv, rv);

    info->id = stmt->AsInt64(1);

    int32_t columnType;
    nsresult rv = stmt->GetTypeOfIndex(2, &columnType);
    NS_ENSURE_SUCCESS(rv, rv);

    // NB: We don't have to handle the NULL case, since that is the default
    // for a new KeyPath.
    if (columnType != mozIStorageStatement::VALUE_TYPE_NULL) {
      NS_ASSERTION(columnType == mozIStorageStatement::VALUE_TYPE_TEXT,
                   "Should be a string");
      nsString keyPathSerialization;
      rv = stmt->GetString(2, keyPathSerialization);
      NS_ENSURE_SUCCESS(rv, rv);

      info->keyPath = KeyPath::DeserializeFromString(keyPathSerialization);
    }

    info->nextAutoIncrementId = stmt->AsInt64(3);
    info->comittedAutoIncrementId = info->nextAutoIncrementId;

    info->autoIncrement = !!info->nextAutoIncrementId;

    ObjectStoreInfoMap* mapEntry = infoMap.AppendElement();
    NS_ENSURE_TRUE(mapEntry, NS_ERROR_OUT_OF_MEMORY);

    mapEntry->id = info->id;
    mapEntry->info = info;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Load index information
  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT object_store_id, id, name, key_path, unique_index, multientry "
    "FROM object_store_index"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    int64_t objectStoreId = stmt->AsInt64(0);

    ObjectStoreInfo* objectStoreInfo = nullptr;
    for (uint32_t index = 0; index < infoMap.Length(); index++) {
      if (infoMap[index].id == objectStoreId) {
        objectStoreInfo = infoMap[index].info;
        break;
      }
    }

    if (!objectStoreInfo) {
      NS_ERROR("Index for nonexistant object store!");
      return NS_ERROR_UNEXPECTED;
    }

    IndexInfo* indexInfo = objectStoreInfo->indexes.AppendElement();
    NS_ENSURE_TRUE(indexInfo, NS_ERROR_OUT_OF_MEMORY);

    indexInfo->id = stmt->AsInt64(1);

    rv = stmt->GetString(2, indexInfo->name);
    NS_ENSURE_SUCCESS(rv, rv);

    nsString keyPathSerialization;
    rv = stmt->GetString(3, keyPathSerialization);
    NS_ENSURE_SUCCESS(rv, rv);

    // XXX bent wants to assert here
    indexInfo->keyPath = KeyPath::DeserializeFromString(keyPathSerialization);
    indexInfo->unique = !!stmt->AsInt32(4);
    indexInfo->multiEntry = !!stmt->AsInt32(5);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Load version information.
  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT version "
    "FROM database"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!hasResult) {
    NS_ERROR("Database has no version!");
    return NS_ERROR_UNEXPECTED;
  }

  int64_t version = 0;
  rv = stmt->GetInt64(0, &version);

  *aVersion = std::max<int64_t>(version, 0);

  return rv;
}

// static
nsresult
IDBFactory::SetDatabaseMetadata(DatabaseInfo* aDatabaseInfo,
                                uint64_t aVersion,
                                ObjectStoreInfoArray& aObjectStores)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aDatabaseInfo, "Null pointer!");

  ObjectStoreInfoArray objectStores;
  objectStores.SwapElements(aObjectStores);

#ifdef DEBUG
  {
    nsTArray<nsString> existingNames;
    aDatabaseInfo->GetObjectStoreNames(existingNames);
    NS_ASSERTION(existingNames.IsEmpty(), "Should be an empty DatabaseInfo");
  }
#endif

  aDatabaseInfo->version = aVersion;

  for (uint32_t index = 0; index < objectStores.Length(); index++) {
    nsRefPtr<ObjectStoreInfo>& info = objectStores[index];

    if (!aDatabaseInfo->PutObjectStore(info)) {
      NS_WARNING("Out of memory!");
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(IDBFactory)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IDBFactory)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBFactory)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

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
    NS_DROP_JS_OBJECTS(tmp, IDBFactory);
    tmp->mRootedOwningObject = false;
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IDBFactory)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mOwningObject)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

DOMCI_DATA(IDBFactory, IDBFactory)

nsresult
IDBFactory::OpenInternal(const nsAString& aName,
                         int64_t aVersion,
                         const nsACString& aASCIIOrigin,
                         bool aDeleting,
                         JSContext* aCallingCx,
                         IDBOpenDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mWindow || mOwningObject, "Must have one of these!");

  nsCOMPtr<nsPIDOMWindow> window;
  JSObject* scriptOwner = nullptr;
  StoragePrivilege privilege;

  if (mWindow) {
    window = mWindow;
    scriptOwner =
      static_cast<nsGlobalWindow*>(window.get())->FastGetGlobalJSObject();
    privilege = Content;
  }
  else {
    scriptOwner = mOwningObject;
    privilege = Chrome;
  }

  nsRefPtr<IDBOpenDBRequest> request =
    IDBOpenDBRequest::Create(this, window, scriptOwner, aCallingCx);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsresult rv;

  if (IndexedDatabaseManager::IsMainProcess()) {
    nsRefPtr<OpenDatabaseHelper> openHelper =
      new OpenDatabaseHelper(request, aName, aASCIIOrigin, aVersion, aDeleting,
                             mContentParent, privilege);

    rv = openHelper->Init();
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    nsRefPtr<CheckPermissionsHelper> permissionHelper =
      new CheckPermissionsHelper(openHelper, window, aDeleting);

    QuotaManager* quotaManager = QuotaManager::Get();
    NS_ASSERTION(quotaManager, "This should never be null!");

    rv = quotaManager->WaitForOpenAllowed(OriginOrPatternString::FromOrigin(
                                          aASCIIOrigin), openHelper->Id(),
                                          permissionHelper);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }
  else if (aDeleting) {
    nsCOMPtr<nsIAtom> databaseId =
      QuotaManager::GetStorageId(aASCIIOrigin, aName);
    NS_ENSURE_TRUE(databaseId, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    IndexedDBDeleteDatabaseRequestChild* actor =
      new IndexedDBDeleteDatabaseRequestChild(this, request, databaseId);

    mActorChild->SendPIndexedDBDeleteDatabaseRequestConstructor(
                                                               actor,
                                                               nsString(aName));
  }
  else {
    IndexedDBDatabaseChild* dbActor =
      static_cast<IndexedDBDatabaseChild*>(
        mActorChild->SendPIndexedDBDatabaseConstructor(nsString(aName),
                                                       aVersion));

    dbActor->SetRequest(request);
  }

  request.forget(_retval);
  return NS_OK;
}

JSObject*
IDBFactory::WrapObject(JSContext* aCx, JSObject* aScope)
{
  return IDBFactoryBinding::Wrap(aCx, aScope, this);
}

int16_t
IDBFactory::Cmp(JSContext* aCx, JS::Value aFirst, JS::Value aSecond,
                ErrorResult& aRv)
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

already_AddRefed<nsIIDBOpenDBRequest>
IDBFactory::OpenForPrincipal(JSContext* aCx, nsIPrincipal* aPrincipal,
                             const NonNull<nsAString>& aName,
                             const Optional<uint64_t>& aVersion,
                             ErrorResult& aRv)
{
  // Just to be on the extra-safe side
  if (!nsContentUtils::IsCallerChrome()) {
    MOZ_CRASH();
  }

  return Open(aCx, aPrincipal, aName, aVersion, false, aRv);
}

already_AddRefed<nsIIDBOpenDBRequest>
IDBFactory::DeleteForPrincipal(JSContext* aCx, nsIPrincipal* aPrincipal,
                               const NonNull<nsAString>& aName,
                               ErrorResult& aRv)
{
  // Just to be on the extra-safe side
  if (!nsContentUtils::IsCallerChrome()) {
    MOZ_CRASH();
  }

  return Open(aCx, aPrincipal, aName, Optional<uint64_t>(), true, aRv);
}

already_AddRefed<nsIIDBOpenDBRequest>
IDBFactory::Open(JSContext* aCx, nsIPrincipal* aPrincipal,
                 const nsAString& aName, const Optional<uint64_t>& aVersion,
                 bool aDelete, ErrorResult& aRv)
{
  nsresult rv;

  nsCString origin;
  if (aPrincipal) {
    rv = QuotaManager::GetASCIIOriginFromPrincipal(aPrincipal, origin);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }
  }
  else {
    origin = mASCIIOrigin;
  }

  uint64_t version;
  if (!aDelete && aVersion.WasPassed()) {
    version = aVersion.Value();
    if (version < 1) {
      aRv.ThrowTypeError(MSG_INVALID_VERSION);
      return nullptr;
    }
  }
  else {
    version = 0;
  }

  nsRefPtr<IDBOpenDBRequest> request;
  rv = OpenInternal(aName, version, origin, aDelete, aCx,
                    getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}
