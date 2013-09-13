/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBFactory.h"

#include "nsIFile.h"
#include "nsIPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsIXPConnect.h"
#include "nsIXPCScriptable.h"

#include <algorithm>
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/IDBFactoryBinding.h"
#include "mozilla/dom/PBrowserChild.h"
#include "mozilla/dom/quota/OriginOrPatternString.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/storage.h"
#include "nsComponentManagerUtils.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
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
#include "ProfilerHelpers.h"

#include "ipc/IndexedDBChild.h"

#define PREF_INDEXEDDB_ENABLED "dom.indexedDB.enabled"

USING_INDEXEDDB_NAMESPACE
USING_QUOTA_NAMESPACE

using mozilla::dom::ContentChild;
using mozilla::dom::ContentParent;
using mozilla::dom::IDBOpenDBOptions;
using mozilla::dom::NonNull;
using mozilla::dom::Optional;
using mozilla::dom::TabChild;
using mozilla::ErrorResult;
using mozilla::Preferences;

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
: mPrivilege(Content), mDefaultPersistenceType(PERSISTENCE_TYPE_TEMPORARY),
  mOwningObject(nullptr), mActorChild(nullptr), mActorParent(nullptr),
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
    mozilla::DropJSObjects(this);
  }
}

// static
nsresult
IDBFactory::Create(nsPIDOMWindow* aWindow,
                   const nsACString& aGroup,
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

  nsCString group(aGroup);
  nsCString origin(aASCIIOrigin);
  StoragePrivilege privilege;
  PersistenceType defaultPersistenceType;
  if (origin.IsEmpty()) {
    NS_ASSERTION(aGroup.IsEmpty(), "Should be empty too!");

    rv = QuotaManager::GetInfoFromWindow(aWindow, &group, &origin, &privilege,
                                         &defaultPersistenceType);
  }
  else {
    rv = QuotaManager::GetInfoFromWindow(aWindow, nullptr, nullptr, &privilege,
                                         &defaultPersistenceType);
  }
  if (NS_FAILED(rv)) {
    // Not allowed.
    *aFactory = nullptr;
    return NS_OK;
  }

  nsRefPtr<IDBFactory> factory = new IDBFactory();
  factory->mGroup = group;
  factory->mASCIIOrigin = origin;
  factory->mPrivilege = privilege;
  factory->mDefaultPersistenceType = defaultPersistenceType;
  factory->mWindow = aWindow;
  factory->mContentParent = aContentParent;

  if (!IndexedDatabaseManager::IsMainProcess()) {
    TabChild* tabChild = TabChild::GetFrom(aWindow);
    NS_ENSURE_TRUE(tabChild, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    IndexedDBChild* actor = new IndexedDBChild(origin);

    bool allowed;
    tabChild->SendPIndexedDBConstructor(actor, group, origin, &allowed);

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
                   JS::Handle<JSObject*> aOwningObject,
                   ContentParent* aContentParent,
                   IDBFactory** aFactory)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aCx, "Null context!");
  NS_ASSERTION(aOwningObject, "Null object!");
  NS_ASSERTION(JS_GetGlobalForObject(aCx, aOwningObject) == aOwningObject,
               "Not a global object!");
  NS_ASSERTION(nsContentUtils::IsCallerChrome(), "Only for chrome!");

  nsCString group;
  nsCString origin;
  StoragePrivilege privilege;
  PersistenceType defaultPersistenceType;
  nsresult rv =
    QuotaManager::GetInfoFromWindow(nullptr, &group, &origin, &privilege,
                                    &defaultPersistenceType);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<IDBFactory> factory = new IDBFactory();
  factory->mGroup = group;
  factory->mASCIIOrigin = origin;
  factory->mPrivilege = privilege;
  factory->mDefaultPersistenceType = defaultPersistenceType;
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

  AutoSafeJSContext cx;

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  NS_ASSERTION(xpc, "This should never be null!");

  nsCOMPtr<nsIXPConnectJSObjectHolder> globalHolder;
  nsresult rv = xpc->CreateSandbox(cx, principal, getter_AddRefs(globalHolder));
  NS_ENSURE_SUCCESS(rv, rv);

  JS::Rooted<JSObject*> global(cx, globalHolder->GetJSObject());
  NS_ENSURE_STATE(global);

  // The CreateSandbox call returns a proxy to the actual sandbox object. We
  // don't need a proxy here.
  global = js::UncheckedUnwrap(global);

  JSAutoCompartment ac(cx, global);

  nsRefPtr<IDBFactory> factory;
  rv = Create(cx, global, aContentParent, getter_AddRefs(factory));
  NS_ENSURE_SUCCESS(rv, rv);

  mozilla::HoldJSObjects(factory.get());
  factory->mRootedOwningObject = true;

  factory.forget(aFactory);
  return NS_OK;
}

// static
already_AddRefed<nsIFileURL>
IDBFactory::GetDatabaseFileURL(nsIFile* aDatabaseFile,
                               PersistenceType aPersistenceType,
                               const nsACString& aGroup,
                               const nsACString& aOrigin)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewFileURI(getter_AddRefs(uri), aDatabaseFile);
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIFileURL> fileUrl = do_QueryInterface(uri);
  NS_ASSERTION(fileUrl, "This should always succeed!");

  nsAutoCString type;
  PersistenceTypeToText(aPersistenceType, type);

  rv = fileUrl->SetQuery(NS_LITERAL_CSTRING("persistenceType=") + type +
                         NS_LITERAL_CSTRING("&group=") + aGroup +
                         NS_LITERAL_CSTRING("&origin=") + aOrigin);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return fileUrl.forget();
}

// static
already_AddRefed<mozIStorageConnection>
IDBFactory::GetConnection(const nsAString& aDatabaseFilePath,
                          PersistenceType aPersistenceType,
                          const nsACString& aGroup,
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

  nsCOMPtr<nsIFileURL> dbFileUrl =
    GetDatabaseFileURL(dbFile, aPersistenceType, aGroup, aOrigin);
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
  AssertIsOnIOThread();
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

DOMCI_DATA(IDBFactory, IDBFactory)

nsresult
IDBFactory::OpenInternal(const nsAString& aName,
                         int64_t aVersion,
                         PersistenceType aPersistenceType,
                         const nsACString& aGroup,
                         const nsACString& aASCIIOrigin,
                         StoragePrivilege aPrivilege,
                         bool aDeleting,
                         IDBOpenDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mWindow || mOwningObject, "Must have one of these!");

  AutoJSContext cx;
  nsCOMPtr<nsPIDOMWindow> window;
  JS::Rooted<JSObject*> scriptOwner(cx);

  if (mWindow) {
    window = mWindow;
    scriptOwner =
      static_cast<nsGlobalWindow*>(window.get())->FastGetGlobalJSObject();
  }
  else {
    scriptOwner = mOwningObject;
  }

  if (aPrivilege == Chrome) {
    // Chrome privilege, ignore the persistence type parameter.
    aPersistenceType = PERSISTENCE_TYPE_PERSISTENT;
  }

  nsRefPtr<IDBOpenDBRequest> request =
    IDBOpenDBRequest::Create(this, window, scriptOwner);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsresult rv;

  if (IndexedDatabaseManager::IsMainProcess()) {
    nsRefPtr<OpenDatabaseHelper> openHelper =
      new OpenDatabaseHelper(request, aName, aGroup, aASCIIOrigin, aVersion,
                             aPersistenceType, aDeleting, mContentParent,
                             aPrivilege);

    rv = openHelper->Init();
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    if (!Preferences::GetBool(PREF_INDEXEDDB_ENABLED)) {
      openHelper->SetError(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
      rv = openHelper->WaitForOpenAllowed();
    }
    else {
      StoragePrivilege openerPrivilege;
      rv = QuotaManager::GetInfoFromWindow(window, nullptr, nullptr,
                                           &openerPrivilege, nullptr);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      if (openerPrivilege != Chrome &&
          aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
        nsRefPtr<CheckPermissionsHelper> permissionHelper =
          new CheckPermissionsHelper(openHelper, window);

        QuotaManager* quotaManager = QuotaManager::Get();
        NS_ASSERTION(quotaManager, "This should never be null!");

        rv = quotaManager->
          WaitForOpenAllowed(OriginOrPatternString::FromOrigin(aASCIIOrigin),
                             Nullable<PersistenceType>(aPersistenceType),
                             openHelper->Id(), permissionHelper);
      }
      else {
        // Chrome and temporary storage doesn't need to check the permission.
        rv = openHelper->WaitForOpenAllowed();
      }
    }
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }
  else if (aDeleting) {
    nsCOMPtr<nsIAtom> databaseId =
      QuotaManager::GetStorageId(aPersistenceType, aASCIIOrigin, aName);
    NS_ENSURE_TRUE(databaseId, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    IndexedDBDeleteDatabaseRequestChild* actor =
      new IndexedDBDeleteDatabaseRequestChild(this, request, databaseId);

    mActorChild->SendPIndexedDBDeleteDatabaseRequestConstructor(
                                                              actor,
                                                              nsString(aName),
                                                              aPersistenceType);
  }
  else {
    IndexedDBDatabaseChild* dbActor =
      static_cast<IndexedDBDatabaseChild*>(
        mActorChild->SendPIndexedDBDatabaseConstructor(nsString(aName),
                                                       aVersion,
                                                       aPersistenceType));

    dbActor->SetRequest(request);
  }

#ifdef IDB_PROFILER_USE_MARKS
  {
    NS_ConvertUTF16toUTF8 profilerName(aName);
    if (aDeleting) {
      IDB_PROFILER_MARK("IndexedDB Request %llu: deleteDatabase(\"%s\")",
                        "MT IDBFactory.deleteDatabase()",
                        request->GetSerialNumber(), profilerName.get());
    }
    else {
      IDB_PROFILER_MARK("IndexedDB Request %llu: open(\"%s\", %lld)",
                        "MT IDBFactory.open()",
                        request->GetSerialNumber(), profilerName.get(),
                        aVersion);
    }
  }
#endif

  request.forget(_retval);
  return NS_OK;
}

JSObject*
IDBFactory::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return IDBFactoryBinding::Wrap(aCx, aScope, this);
}

already_AddRefed<IDBOpenDBRequest>
IDBFactory::Open(const nsAString& aName, const IDBOpenDBOptions& aOptions,
                 ErrorResult& aRv)
{
  return Open(nullptr, aName, aOptions.mVersion, aOptions.mStorage, false, aRv);
}

already_AddRefed<IDBOpenDBRequest>
IDBFactory::DeleteDatabase(const nsAString& aName,
                           const IDBOpenDBOptions& aOptions,
                           ErrorResult& aRv)
{
  return Open(nullptr, aName, Optional<uint64_t>(), aOptions.mStorage, true,
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
IDBFactory::OpenForPrincipal(nsIPrincipal* aPrincipal, const nsAString& aName,
                             uint64_t aVersion, ErrorResult& aRv)
{
  // Just to be on the extra-safe side
  if (!nsContentUtils::IsCallerChrome()) {
    MOZ_CRASH();
  }

  return Open(aPrincipal, aName, Optional<uint64_t>(aVersion),
              Optional<mozilla::dom::StorageType>(), false, aRv);
}

already_AddRefed<IDBOpenDBRequest>
IDBFactory::OpenForPrincipal(nsIPrincipal* aPrincipal, const nsAString& aName,
                             const IDBOpenDBOptions& aOptions, ErrorResult& aRv)
{
  // Just to be on the extra-safe side
  if (!nsContentUtils::IsCallerChrome()) {
    MOZ_CRASH();
  }

  return Open(aPrincipal, aName, aOptions.mVersion, aOptions.mStorage, false,
              aRv);
}

already_AddRefed<IDBOpenDBRequest>
IDBFactory::DeleteForPrincipal(nsIPrincipal* aPrincipal, const nsAString& aName,
                               const IDBOpenDBOptions& aOptions,
                               ErrorResult& aRv)
{
  // Just to be on the extra-safe side
  if (!nsContentUtils::IsCallerChrome()) {
    MOZ_CRASH();
  }

  return Open(aPrincipal, aName, Optional<uint64_t>(), aOptions.mStorage, true,
              aRv);
}

already_AddRefed<IDBOpenDBRequest>
IDBFactory::Open(nsIPrincipal* aPrincipal, const nsAString& aName,
                 const Optional<uint64_t>& aVersion,
                 const Optional<mozilla::dom::StorageType>& aStorageType,
                 bool aDelete, ErrorResult& aRv)
{
  nsresult rv;

  nsCString group;
  nsCString origin;
  StoragePrivilege privilege;
  PersistenceType defaultPersistenceType;
  if (aPrincipal) {
    rv = QuotaManager::GetInfoFromPrincipal(aPrincipal, &group, &origin,
                                            &privilege,
                                            &defaultPersistenceType);
    if (NS_FAILED(rv)) {
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      return nullptr;
    }
  }
  else {
    group = mGroup;
    origin = mASCIIOrigin;
    privilege = mPrivilege;
    defaultPersistenceType = mDefaultPersistenceType;
  }

  uint64_t version = 0;
  if (!aDelete && aVersion.WasPassed()) {
    if (aVersion.Value() < 1) {
      aRv.ThrowTypeError(MSG_INVALID_VERSION);
      return nullptr;
    }
    version = aVersion.Value();
  }

  PersistenceType persistenceType =
    PersistenceTypeFromStorage(aStorageType, defaultPersistenceType);

  nsRefPtr<IDBOpenDBRequest> request;
  rv = OpenInternal(aName, version, persistenceType, group, origin, privilege,
                    aDelete, getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}
