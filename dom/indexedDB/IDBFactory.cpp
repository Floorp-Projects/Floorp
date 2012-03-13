/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "base/basictypes.h"

#include "IDBFactory.h"

#include "nsILocalFile.h"
#include "nsIScriptContext.h"

#include "mozilla/storage.h"
#include "mozilla/dom/ContentChild.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsIPrincipal.h"
#include "nsHashKeys.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCID.h"
#include "nsXULAppAPI.h"

#include "AsyncConnectionHelper.h"
#include "CheckPermissionsHelper.h"
#include "DatabaseInfo.h"
#include "IDBDatabase.h"
#include "IDBEvents.h"
#include "IDBKeyRange.h"
#include "IndexedDatabaseManager.h"
#include "Key.h"

using namespace mozilla;

USING_INDEXEDDB_NAMESPACE

namespace {

GeckoProcessType gAllowedProcessType = GeckoProcessType_Invalid;

struct ObjectStoreInfoMap
{
  ObjectStoreInfoMap()
  : id(LL_MININT), info(nsnull) { }

  PRInt64 id;
  ObjectStoreInfo* info;
};

} // anonymous namespace

IDBFactory::IDBFactory()
: mOwningObject(nsnull)
{
  IDBFactory::NoteUsedByProcessType(XRE_GetProcessType());
}

IDBFactory::~IDBFactory()
{
}

// static
already_AddRefed<nsIIDBFactory>
IDBFactory::Create(nsPIDOMWindow* aWindow)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_TRUE(aWindow, nsnull);

  if (aWindow->IsOuterWindow()) {
    aWindow = aWindow->GetCurrentInnerWindow();
    NS_ENSURE_TRUE(aWindow, nsnull);
  }

  nsRefPtr<IDBFactory> factory = new IDBFactory();
  factory->mWindow = aWindow;
  return factory.forget();
}

// static
already_AddRefed<nsIIDBFactory>
IDBFactory::Create(JSContext* aCx,
                   JSObject* aOwningObject)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aCx, "Null context!");
  NS_ASSERTION(aOwningObject, "Null object!");
  NS_ASSERTION(JS_GetGlobalForObject(aCx, aOwningObject) == aOwningObject,
               "Not a global object!");

  nsRefPtr<IDBFactory> factory = new IDBFactory();
  factory->mOwningObject = aOwningObject;
  return factory.forget();
}

// static
already_AddRefed<mozIStorageConnection>
IDBFactory::GetConnection(const nsAString& aDatabaseFilePath)
{
  NS_ASSERTION(StringEndsWith(aDatabaseFilePath, NS_LITERAL_STRING(".sqlite")),
               "Bad file path!");

  nsCOMPtr<nsILocalFile> dbFile(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
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

  // Turn on foreign key constraints!
  rv = connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "PRAGMA foreign_keys = ON;"
  ));
  NS_ENSURE_SUCCESS(rv, nsnull);

  return connection.forget();
}

// static
void
IDBFactory::NoteUsedByProcessType(GeckoProcessType aProcessType)
{
  if (gAllowedProcessType == GeckoProcessType_Invalid) {
    gAllowedProcessType = aProcessType;
  } else if (aProcessType != gAllowedProcessType) {
    NS_RUNTIMEABORT("More than one process type is accessing IndexedDB!");
  }
}

// static
nsresult
IDBFactory::GetDirectory(nsIFile** aDirectory)
{
  nsresult rv;
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, aDirectory);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = (*aDirectory)->Append(NS_LITERAL_STRING("indexedDB"));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    nsCOMPtr<nsILocalFile> localDirectory =
      do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
    rv = localDirectory->InitWithPath(
      ContentChild::GetSingleton()->GetIndexedDBPath());
    NS_ENSURE_SUCCESS(rv, rv);
    localDirectory.forget((nsILocalFile**)aDirectory);
  }
  return NS_OK;
}

// static
nsresult
IDBFactory::GetDirectoryForOrigin(const nsACString& aASCIIOrigin,
                                  nsIFile** aDirectory)
{
  nsCOMPtr<nsIFile> directory;
  nsresult rv = GetDirectory(getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ConvertASCIItoUTF16 originSanitized(aASCIIOrigin);
  originSanitized.ReplaceChar(":/", '+');

  rv = directory->Append(originSanitized);
  NS_ENSURE_SUCCESS(rv, rv);

  directory.forget(aDirectory);
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
    if (columnType == mozIStorageStatement::VALUE_TYPE_NULL) {
      info->keyPath.SetIsVoid(true);
    }
    else {
      NS_ASSERTION(columnType == mozIStorageStatement::VALUE_TYPE_TEXT,
                   "Should be a string");
      nsString keyPath;
      rv = stmt->GetString(2, keyPath);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!keyPath.IsEmpty() && keyPath.First() == ',') {
        // We use a comma in the beginning to indicate that it's an array of
        // key paths. This is to be able to tell a string-keypath from an
        // array-keypath which contains only one item.
        nsCharSeparatedTokenizerTemplate<IgnoreWhitespace>
          tokenizer(keyPath, ',');
        tokenizer.nextToken();
        while (tokenizer.hasMoreTokens()) {
          info->keyPathArray.AppendElement(tokenizer.nextToken());
        }
        NS_ASSERTION(!info->keyPathArray.IsEmpty(),
                     "Should have at least one keypath");
      }
      else {
        info->keyPath = keyPath;
      }

    }

    info->nextAutoIncrementId = stmt->AsInt64(3);
    info->comittedAutoIncrementId = info->nextAutoIncrementId;

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

    nsString keyPath;
    rv = stmt->GetString(3, keyPath);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!keyPath.IsEmpty() && keyPath.First() == ',') {
      // We use a comma in the beginning to indicate that it's an array of
      // key paths. This is to be able to tell a string-keypath from an
      // array-keypath which contains only one item.
      nsCharSeparatedTokenizerTemplate<IgnoreWhitespace>
        tokenizer(keyPath, ',');
      tokenizer.nextToken();
      while (tokenizer.hasMoreTokens()) {
        indexInfo->keyPathArray.AppendElement(tokenizer.nextToken());
      }
      NS_ASSERTION(!indexInfo->keyPathArray.IsEmpty(),
                   "Should have at least one keypath");
    }
    else {
      indexInfo->keyPath = keyPath;
    }

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
  if (tmp->mOwningObject) {
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_CALLBACK(tmp->mOwningObject,
                                               "mOwningObject")
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

DOMCI_DATA(IDBFactory, IDBFactory)

nsresult
IDBFactory::OpenCommon(const nsAString& aName,
                       PRInt64 aVersion,
                       bool aDeleting,
                       nsIIDBOpenDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mWindow || mOwningObject, "Must have one of these!");

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    // Force ContentChild to cache the path from the parent, so that
    // we do not end up in a side thread that asks for the path (which
    // would make ContentChild try to send a message in a thread other
    // than the main one).
    ContentChild::GetSingleton()->GetIndexedDBPath();
  }

  nsCOMPtr<nsPIDOMWindow> window;
  nsCOMPtr<nsIScriptGlobalObject> sgo;
  JSObject* scriptOwner = nsnull;

  if (mWindow) {
    window = mWindow;
  }
  else {
    scriptOwner = mOwningObject;
  }

  nsCString origin;
  nsresult rv =
    IndexedDatabaseManager::GetASCIIOriginFromWindow(window, origin);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<IDBOpenDBRequest> request =
    IDBOpenDBRequest::Create(window, scriptOwner);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<OpenDatabaseHelper> openHelper =
    new OpenDatabaseHelper(request, aName, origin, aVersion, aDeleting);

  rv = openHelper->Init();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<CheckPermissionsHelper> permissionHelper =
    new CheckPermissionsHelper(openHelper, window, origin, aDeleting);

  nsRefPtr<IndexedDatabaseManager> mgr = IndexedDatabaseManager::GetOrCreate();
  NS_ENSURE_TRUE(mgr, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = mgr->WaitForOpenAllowed(origin, openHelper->Id(), permissionHelper);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBFactory::Open(const nsAString& aName,
                 PRInt64 aVersion,
                 PRUint8 aArgc,
                 nsIIDBOpenDBRequest** _retval)
{
  if (aVersion < 1 && aArgc) {
    return NS_ERROR_DOM_INDEXEDDB_NON_TRANSIENT_ERR;
  }

  return OpenCommon(aName, aVersion, false, _retval);
}

NS_IMETHODIMP
IDBFactory::DeleteDatabase(const nsAString& aName,
                           nsIIDBOpenDBRequest** _retval)
{
  return OpenCommon(aName, 0, true, _retval);
}

NS_IMETHODIMP
IDBFactory::Cmp(const jsval& aFirst,
                const jsval& aSecond,
                JSContext* aCx,
                PRInt16* _retval)
{
  Key first, second;
  nsresult rv = first.SetFromJSVal(aCx, aFirst);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = second.SetFromJSVal(aCx, aSecond);
  NS_ENSURE_SUCCESS(rv, rv);

  if (first.IsUnset() || second.IsUnset()) {
    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  *_retval = Key::CompareKeys(first, second);
  return NS_OK;
}
