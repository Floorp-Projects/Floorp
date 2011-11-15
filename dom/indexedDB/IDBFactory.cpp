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
#include "LazyIdleThread.h"
#include "nsIScriptSecurityManager.h"

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
{
  IDBFactory::NoteUsedByProcessType(XRE_GetProcessType());
}

// static
already_AddRefed<nsIIDBFactory>
IDBFactory::Create(nsPIDOMWindow* aWindow)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aWindow, "Must have a window!");

  if (aWindow->IsOuterWindow()) {
    aWindow = aWindow->GetCurrentInnerWindow();
  }
  NS_ENSURE_TRUE(aWindow, nsnull);

  nsRefPtr<IDBFactory> factory = new IDBFactory();

  factory->mWindow = do_GetWeakReference(aWindow);
  NS_ENSURE_TRUE(factory->mWindow, nsnull);

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

#ifdef DEBUG
  {
    // Check to make sure that the database schema is correct again.
    PRInt32 schemaVersion;
    NS_ASSERTION(NS_SUCCEEDED(connection->GetSchemaVersion(&schemaVersion)) &&
                 schemaVersion == DB_SCHEMA_VERSION,
                 "Wrong schema!");
  }
#endif

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
    nsAutoPtr<ObjectStoreInfo>* element =
      aObjectStores.AppendElement(new ObjectStoreInfo());
    NS_ENSURE_TRUE(element, NS_ERROR_OUT_OF_MEMORY);

    ObjectStoreInfo* info = element->get();

    rv = stmt->GetString(0, info->name);
    NS_ENSURE_SUCCESS(rv, rv);

    info->id = stmt->AsInt64(1);

    rv = stmt->GetString(2, info->keyPath);
    NS_ENSURE_SUCCESS(rv, rv);

    info->autoIncrement = !!stmt->AsInt32(3);
    info->databaseId = aDatabaseId;

    ObjectStoreInfoMap* mapEntry = infoMap.AppendElement();
    NS_ENSURE_TRUE(mapEntry, NS_ERROR_OUT_OF_MEMORY);

    mapEntry->id = info->id;
    mapEntry->info = info;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Load index information
  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT object_store_id, id, name, key_path, unique_index, "
           "object_store_autoincrement "
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

    rv = stmt->GetString(3, indexInfo->keyPath);
    NS_ENSURE_SUCCESS(rv, rv);

    indexInfo->unique = !!stmt->AsInt32(4);
    indexInfo->autoIncrement = !!stmt->AsInt32(5);
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
IDBFactory::UpdateDatabaseMetadata(DatabaseInfo* aDatabaseInfo,
                                   PRUint64 aVersion,
                                   ObjectStoreInfoArray& aObjectStores)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aDatabaseInfo, "Null pointer!");

  ObjectStoreInfoArray objectStores;
  if (!objectStores.SwapElements(aObjectStores)) {
    NS_WARNING("Out of memory!");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsAutoTArray<nsString, 10> existingNames;
  if (!aDatabaseInfo->GetObjectStoreNames(existingNames)) {
    NS_WARNING("Out of memory!");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Remove all the old ones.
  for (PRUint32 index = 0; index < existingNames.Length(); index++) {
    aDatabaseInfo->RemoveObjectStore(existingNames[index]);
  }

  aDatabaseInfo->version = aVersion;

  for (PRUint32 index = 0; index < objectStores.Length(); index++) {
    nsAutoPtr<ObjectStoreInfo>& info = objectStores[index];
    NS_ASSERTION(info->databaseId == aDatabaseInfo->id, "Huh?!");

    if (!aDatabaseInfo->PutObjectStore(info)) {
      NS_WARNING("Out of memory!");
      return NS_ERROR_OUT_OF_MEMORY;
    }

    info.forget();
  }

  return NS_OK;
}

NS_IMPL_ADDREF(IDBFactory)
NS_IMPL_RELEASE(IDBFactory)

NS_INTERFACE_MAP_BEGIN(IDBFactory)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIIDBFactory)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBFactory)
NS_INTERFACE_MAP_END

DOMCI_DATA(IDBFactory, IDBFactory)

nsresult
IDBFactory::OpenCommon(const nsAString& aName,
                       PRInt64 aVersion,
                       bool aDeleting,
                       nsIIDBOpenDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    // Force ContentChild to cache the path from the parent, so that
    // we do not end up in a side thread that asks for the path (which
    // would make ContentChild try to send a message in a thread other
    // than the main one).
    ContentChild::GetSingleton()->GetIndexedDBPath();
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(window);
  NS_ENSURE_TRUE(sgo, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsIScriptContext* context = sgo->GetContext();
  NS_ENSURE_TRUE(context, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = nsContentUtils::GetSecurityManager()->
    GetSubjectPrincipal(getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsCString origin;
  if (nsContentUtils::IsSystemPrincipal(principal)) {
    origin.AssignLiteral("chrome");
  }
  else {
    rv = nsContentUtils::GetASCIIOrigin(principal, origin);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    if (origin.EqualsLiteral("null")) {
      NS_WARNING("IndexedDB databases not allowed for this principal!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  }

  nsRefPtr<IDBOpenDBRequest> request =
    IDBOpenDBRequest::Create(context, window);
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

  *_retval = first == second ? 0 : first < second ? -1 : 1;
  return NS_OK;
}
