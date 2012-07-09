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

#include "mozilla/dom/PBrowserChild.h"
#include "mozilla/dom/TabChild.h"
using mozilla::dom::TabChild;

#include "ipc/IndexedDBChild.h"

USING_INDEXEDDB_NAMESPACE

namespace {

struct ObjectStoreInfoMap
{
  ObjectStoreInfoMap()
  : id(LL_MININT), info(nsnull) { }

  PRInt64 id;
  ObjectStoreInfo* info;
};

} // anonymous namespace

IDBFactory::IDBFactory()
: mOwningObject(nsnull), mActorChild(nsnull), mActorParent(nsnull)
{
}

IDBFactory::~IDBFactory()
{
  NS_ASSERTION(!mActorParent, "Actor parent owns us, how can we be dying?!");
  if (mActorChild) {
    NS_ASSERTION(!IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
    mActorChild->Send__delete__(mActorChild);
    NS_ASSERTION(!mActorChild, "Should have cleared in Send__delete__!");
  }
}

// static
nsresult
IDBFactory::Create(nsPIDOMWindow* aWindow,
                   const nsACString& aASCIIOrigin,
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
  nsRefPtr<indexedDB::IndexedDatabaseManager> mgr =
    indexedDB::IndexedDatabaseManager::GetOrCreate();
  NS_ENSURE_TRUE(mgr, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsresult rv;

  nsCString origin(aASCIIOrigin);
  if (origin.IsEmpty()) {
    rv = IndexedDatabaseManager::GetASCIIOriginFromWindow(aWindow, origin);
    if (NS_FAILED(rv)) {
      // Not allowed.
      *aFactory = nsnull;
      return NS_OK;
    }
  }

  nsRefPtr<IDBFactory> factory = new IDBFactory();
  factory->mASCIIOrigin = origin;
  factory->mWindow = aWindow;

  if (!IndexedDatabaseManager::IsMainProcess()) {
    TabChild* tabChild = GetTabChildFrom(aWindow);
    NS_ENSURE_TRUE(tabChild, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    IndexedDBChild* actor = new IndexedDBChild(origin);

    bool allowed;
    tabChild->SendPIndexedDBConstructor(actor, origin, &allowed);

    if (!allowed) {
      actor->Send__delete__(actor);
      *aFactory = nsnull;
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
                   IDBFactory** aFactory)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aCx, "Null context!");
  NS_ASSERTION(aOwningObject, "Null object!");
  NS_ASSERTION(JS_GetGlobalForObject(aCx, aOwningObject) == aOwningObject,
               "Not a global object!");
  NS_ASSERTION(nsContentUtils::IsCallerChrome(), "Only for chrome!");

  nsCString origin;
  nsresult rv =
    IndexedDatabaseManager::GetASCIIOriginFromWindow(nsnull, origin);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<IDBFactory> factory = new IDBFactory();
  factory->mASCIIOrigin = origin;
  factory->mOwningObject = aOwningObject;

  factory.forget(aFactory);
  return NS_OK;
}

// static
already_AddRefed<mozIStorageConnection>
IDBFactory::GetConnection(const nsAString& aDatabaseFilePath)
{
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
  NS_ASSERTION(StringEndsWith(aDatabaseFilePath, NS_LITERAL_STRING(".sqlite")),
               "Bad file path!");

  nsCOMPtr<nsIFile> dbFile(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
  NS_ENSURE_TRUE(dbFile, nsnull);

  nsresult rv = dbFile->InitWithPath(aDatabaseFilePath);
  NS_ENSURE_SUCCESS(rv, nsnull);

  bool exists;
  rv = dbFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, nsnull);
  NS_ENSURE_TRUE(exists, nsnull);

  nsCOMPtr<mozIStorageServiceQuotaManagement> ss =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(ss, nsnull);

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenDatabaseWithVFS(dbFile, NS_LITERAL_CSTRING("quota"),
                               getter_AddRefs(connection));
  NS_ENSURE_SUCCESS(rv, nsnull);

  // Turn on foreign key constraints and recursive triggers.
  // The "INSERT OR REPLACE" statement doesn't fire the update trigger,
  // instead it fires only the insert trigger. This confuses the update
  // refcount function. This behavior changes with enabled recursive triggers,
  // so the statement fires the delete trigger first and then the insert
  // trigger.
  rv = connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "PRAGMA foreign_keys = ON; "
    "PRAGMA recursive_triggers = ON;"
  ));
  NS_ENSURE_SUCCESS(rv, nsnull);

  return connection.forget();
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
                                    PRUint64* aVersion,
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

    PRInt32 columnType;
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
    PRInt64 objectStoreId = stmt->AsInt64(0);

    ObjectStoreInfo* objectStoreInfo = nsnull;
    for (PRUint32 index = 0; index < infoMap.Length(); index++) {
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

  PRInt64 version = 0;
  rv = stmt->GetInt64(0, &version);

  *aVersion = NS_MAX<PRInt64>(version, 0);

  return rv;
}

// static
nsresult
IDBFactory::SetDatabaseMetadata(DatabaseInfo* aDatabaseInfo,
                                PRUint64 aVersion,
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

  for (PRUint32 index = 0; index < objectStores.Length(); index++) {
    nsRefPtr<ObjectStoreInfo>& info = objectStores[index];

    if (!aDatabaseInfo->PutObjectStore(info)) {
      NS_WARNING("Out of memory!");
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBFactory)

NS_IMPL_CYCLE_COLLECTING_ADDREF(IDBFactory)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IDBFactory)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBFactory)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIIDBFactory)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBFactory)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IDBFactory)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mWindow)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IDBFactory)
  if (tmp->mOwningObject) {
    tmp->mOwningObject = nsnull;
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mWindow)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IDBFactory)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mOwningObject)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

DOMCI_DATA(IDBFactory, IDBFactory)

nsresult
IDBFactory::OpenCommon(const nsAString& aName,
                       PRInt64 aVersion,
                       bool aDeleting,
                       JSContext* aCallingCx,
                       IDBOpenDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mWindow || mOwningObject, "Must have one of these!");

  nsCOMPtr<nsPIDOMWindow> window;
  nsCOMPtr<nsIScriptGlobalObject> sgo;
  JSObject* scriptOwner = nsnull;

  if (mWindow) {
    window = mWindow;
    scriptOwner =
      static_cast<nsGlobalWindow*>(window.get())->FastGetGlobalJSObject();
  }
  else {
    scriptOwner = mOwningObject;
  }

  nsRefPtr<IDBOpenDBRequest> request =
    IDBOpenDBRequest::Create(window, scriptOwner, aCallingCx);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsresult rv;

  if (IndexedDatabaseManager::IsMainProcess()) {
    nsRefPtr<OpenDatabaseHelper> openHelper =
      new OpenDatabaseHelper(request, aName, mASCIIOrigin, aVersion, aDeleting);

    rv = openHelper->Init();
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    nsRefPtr<CheckPermissionsHelper> permissionHelper =
      new CheckPermissionsHelper(openHelper, window, mASCIIOrigin, aDeleting);

    IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
    NS_ASSERTION(mgr, "This should never be null!");

    rv = 
      mgr->WaitForOpenAllowed(mASCIIOrigin, openHelper->Id(), permissionHelper);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }
  else if (aDeleting) {
    nsCOMPtr<nsIAtom> databaseId =
      IndexedDatabaseManager::GetDatabaseId(mASCIIOrigin, aName);
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

NS_IMETHODIMP
IDBFactory::Open(const nsAString& aName,
                 PRInt64 aVersion,
                 JSContext* aCx,
                 PRUint8 aArgc,
                 nsIIDBOpenDBRequest** _retval)
{
  if (aVersion < 1 && aArgc) {
    return NS_ERROR_TYPE_ERR;
  }

  nsRefPtr<IDBOpenDBRequest> request;
  nsresult rv = OpenCommon(aName, aVersion, false, aCx,
                           getter_AddRefs(request));
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBFactory::DeleteDatabase(const nsAString& aName,
                           JSContext* aCx,
                           nsIIDBOpenDBRequest** _retval)
{
  nsRefPtr<IDBOpenDBRequest> request;
  nsresult rv = OpenCommon(aName, 0, true, aCx, getter_AddRefs(request));
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBFactory::Cmp(const jsval& aFirst,
                const jsval& aSecond,
                JSContext* aCx,
                PRInt16* _retval)
{
  Key first, second;
  nsresult rv = first.SetFromJSVal(aCx, aFirst);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = second.SetFromJSVal(aCx, aSecond);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (first.IsUnset() || second.IsUnset()) {
    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  *_retval = Key::CompareKeys(first, second);
  return NS_OK;
}
