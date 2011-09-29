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
#include "nsContentUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDOMClassInfo.h"
#include "nsEscape.h"
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
#include "IDBKeyRange.h"
#include "IndexedDatabaseManager.h"
#include "LazyIdleThread.h"

using namespace mozilla;

#define DB_SCHEMA_VERSION 4

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

class OpenDatabaseHelper : public AsyncConnectionHelper
{
public:
  OpenDatabaseHelper(IDBRequest* aRequest,
                     const nsAString& aName,
                     const nsACString& aASCIIOrigin)
  : AsyncConnectionHelper(static_cast<IDBDatabase*>(nsnull), aRequest),
    mName(aName), mASCIIOrigin(aASCIIOrigin), mDatabaseId(0),
    mLastObjectStoreId(0), mLastIndexId(0)
  { }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult GetSuccessResult(JSContext* aCx,
                            jsval* aVal);

private:
  // In-params.
  nsString mName;
  nsCString mASCIIOrigin;

  // Out-params.
  nsTArray<nsAutoPtr<ObjectStoreInfo> > mObjectStores;
  nsString mVersion;
  PRUint32 mDataVersion;
  nsString mDatabaseFilePath;
  PRUint32 mDatabaseId;
  PRInt64 mLastObjectStoreId;
  PRInt64 mLastIndexId;
};

nsresult
CreateTables(mozIStorageConnection* aDBConn)
{
  NS_PRECONDITION(!NS_IsMainThread(),
                  "Creating tables on the main thread!");
  NS_PRECONDITION(aDBConn, "Passing a null database connection!");

  // Table `database`
  nsresult rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE database ("
      "name TEXT NOT NULL, "
      "version TEXT DEFAULT NULL, "
      "dataVersion INTEGER NOT NULL"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Table `object_store`
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE object_store ("
      "id INTEGER, "
      "name TEXT NOT NULL, "
      "key_path TEXT NOT NULL, "
      "auto_increment INTEGER NOT NULL DEFAULT 0, "
      "PRIMARY KEY (id), "
      "UNIQUE (name)"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Table `object_data`
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE object_data ("
      "id INTEGER, "
      "object_store_id INTEGER NOT NULL, "
      "data BLOB NOT NULL, "
      "key_value DEFAULT NULL, " // NONE affinity
      "PRIMARY KEY (id), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE UNIQUE INDEX key_index "
    "ON object_data (key_value, object_store_id);"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Table `ai_object_data`
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE ai_object_data ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "object_store_id INTEGER NOT NULL, "
      "data BLOB NOT NULL, "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE UNIQUE INDEX ai_key_index "
    "ON ai_object_data (id, object_store_id);"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Table `index`
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE object_store_index ("
      "id INTEGER, "
      "object_store_id INTEGER NOT NULL, "
      "name TEXT NOT NULL, "
      "key_path TEXT NOT NULL, "
      "unique_index INTEGER NOT NULL, "
      "object_store_autoincrement INTERGER NOT NULL, "
      "PRIMARY KEY (id), "
      "UNIQUE (object_store_id, name), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Table `index_data`
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE index_data ("
      "id INTEGER, "
      "index_id INTEGER NOT NULL, "
      "object_data_id INTEGER NOT NULL, "
      "object_data_key NOT NULL, " // NONE affinity
      "value NOT NULL, "
      "PRIMARY KEY (id), "
      "FOREIGN KEY (index_id) REFERENCES object_store_index(id) ON DELETE "
        "CASCADE, "
      "FOREIGN KEY (object_data_id) REFERENCES object_data(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX value_index "
    "ON index_data (index_id, value);"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Table `unique_index_data`
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE unique_index_data ("
      "id INTEGER, "
      "index_id INTEGER NOT NULL, "
      "object_data_id INTEGER NOT NULL, "
      "object_data_key NOT NULL, " // NONE affinity
      "value NOT NULL, "
      "PRIMARY KEY (id), "
      "UNIQUE (index_id, value), "
      "FOREIGN KEY (index_id) REFERENCES object_store_index(id) ON DELETE "
        "CASCADE "
      "FOREIGN KEY (object_data_id) REFERENCES object_data(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Table `ai_index_data`
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE ai_index_data ("
      "id INTEGER, "
      "index_id INTEGER NOT NULL, "
      "ai_object_data_id INTEGER NOT NULL, "
      "value NOT NULL, "
      "PRIMARY KEY (id), "
      "FOREIGN KEY (index_id) REFERENCES object_store_index(id) ON DELETE "
        "CASCADE, "
      "FOREIGN KEY (ai_object_data_id) REFERENCES ai_object_data(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX ai_value_index "
    "ON ai_index_data (index_id, value);"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Table `ai_unique_index_data`
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE ai_unique_index_data ("
      "id INTEGER, "
      "index_id INTEGER NOT NULL, "
      "ai_object_data_id INTEGER NOT NULL, "
      "value NOT NULL, "
      "PRIMARY KEY (id), "
      "UNIQUE (index_id, value), "
      "FOREIGN KEY (index_id) REFERENCES object_store_index(id) ON DELETE "
        "CASCADE, "
      "FOREIGN KEY (ai_object_data_id) REFERENCES ai_object_data(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDBConn->SetSchemaVersion(DB_SCHEMA_VERSION);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
CreateMetaData(mozIStorageConnection* aConnection,
               const nsAString& aName)
{
  NS_PRECONDITION(!NS_IsMainThread(), "Wrong thread!");
  NS_PRECONDITION(aConnection, "Null database!");

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT OR REPLACE INTO database (name, dataVersion) "
    "VALUES (:name, :dataVersion)"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), aName);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("dataVersion"),
                             JS_STRUCTURED_CLONE_VERSION);
  NS_ENSURE_SUCCESS(rv, rv);

  return stmt->Execute();
}

nsresult
GetDatabaseFile(const nsACString& aASCIIOrigin,
                const nsAString& aName,
                nsIFile** aDatabaseFile)
{
  NS_ASSERTION(!aASCIIOrigin.IsEmpty() && !aName.IsEmpty(), "Bad arguments!");

  nsCOMPtr<nsIFile> dbFile;
  nsresult rv = IDBFactory::GetDirectory(getter_AddRefs(dbFile));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ConvertASCIItoUTF16 originSanitized(aASCIIOrigin);
  originSanitized.ReplaceChar(":/", '+');

  rv = dbFile->Append(originSanitized);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString filename;
  filename.AppendInt(HashString(aName));

  nsCString escapedName;
  if (!NS_Escape(NS_ConvertUTF16toUTF8(aName), escapedName, url_XPAlphas)) {
    NS_WARNING("Can't escape database name!");
    return NS_ERROR_UNEXPECTED;
  }

  const char* forwardIter = escapedName.BeginReading();
  const char* backwardIter = escapedName.EndReading() - 1;

  nsCString substring;
  while (forwardIter <= backwardIter && substring.Length() < 21) {
    if (substring.Length() % 2) {
      substring.Append(*backwardIter--);
    }
    else {
      substring.Append(*forwardIter++);
    }
  }

  filename.Append(NS_ConvertASCIItoUTF16(substring));
  filename.AppendLiteral(".sqlite");

  rv = dbFile->Append(filename);
  NS_ENSURE_SUCCESS(rv, rv);

  dbFile.forget(aDatabaseFile);
  return NS_OK;
}

nsresult
CreateDatabaseConnection(const nsAString& aName,
                         nsIFile* aDBFile,
                         mozIStorageConnection** aConnection)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIFile> dbDirectory;
  nsresult rv = aDBFile->GetParent(getter_AddRefs(dbDirectory));
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = aDBFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_NAMED_LITERAL_CSTRING(quotaVFSName, "quota");

  nsCOMPtr<mozIStorageServiceQuotaManagement> ss =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(ss, NS_ERROR_FAILURE);

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenDatabaseWithVFS(aDBFile, quotaVFSName,
                               getter_AddRefs(connection));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // Nuke the database file.  The web services can recreate their data.
    rv = aDBFile->Remove(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    exists = PR_FALSE;

    rv = ss->OpenDatabaseWithVFS(aDBFile, quotaVFSName,
                                 getter_AddRefs(connection));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Check to make sure that the database schema is correct.
  PRInt32 schemaVersion;
  rv = connection->GetSchemaVersion(&schemaVersion);
  NS_ENSURE_SUCCESS(rv, rv);

  if (schemaVersion != DB_SCHEMA_VERSION) {
    if (exists) {
      // If the connection is not at the right schema version, nuke it.
      rv = aDBFile->Remove(PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = ss->OpenDatabaseWithVFS(aDBFile, quotaVFSName,
                                   getter_AddRefs(connection));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    mozStorageTransaction transaction(connection, PR_FALSE,
                                      mozIStorageConnection::TRANSACTION_IMMEDIATE);

    rv = CreateTables(connection);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = CreateMetaData(connection, aName);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = transaction.Commit();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Check to make sure that the database schema is correct again.
  NS_ASSERTION(NS_SUCCEEDED(connection->GetSchemaVersion(&schemaVersion)) &&
               schemaVersion == DB_SCHEMA_VERSION,
               "CreateTables failed!");

  // Turn on foreign key constraints.
  rv = connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "PRAGMA foreign_keys = ON;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  connection.forget(aConnection);
  return NS_OK;
}

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
                                    PRUint32 aDatabaseId,
                                    nsAString& aVersion,
                                    ObjectStoreInfoArray& aObjectStores)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aConnection, "Null pointer!");

  aVersion.Truncate();
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

  nsString version;
  rv = stmt->GetString(0, version);
  NS_ENSURE_SUCCESS(rv, rv);

  if (version.IsVoid()) {
    version.SetIsVoid(PR_FALSE);
  }
  aVersion = version;
  return NS_OK;
}

// static
nsresult
IDBFactory::UpdateDatabaseMetadata(DatabaseInfo* aDatabaseInfo,
                                   const nsAString& aVersion,
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
    ObjectStoreInfo::Remove(aDatabaseInfo->id, existingNames[index]);
  }

  aDatabaseInfo->version = aVersion;

  for (PRUint32 index = 0; index < objectStores.Length(); index++) {
    nsAutoPtr<ObjectStoreInfo>& info = objectStores[index];
    NS_ASSERTION(info->databaseId == aDatabaseInfo->id, "Huh?!");

    if (!ObjectStoreInfo::Put(info)) {
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

NS_IMETHODIMP
IDBFactory::Open(const nsAString& aName,
                 JSContext* aCx,
                 nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    // Force ContentChild to cache the path from the parent, so that
    // we do not end up in a side thread that asks for the path (which
    // would make ContentChild try to send a message in a thread other
    // than the main one).
    ContentChild::GetSingleton()->GetIndexedDBPath();
  }

  if (aName.IsEmpty()) {
    return NS_ERROR_DOM_INDEXEDDB_NON_TRANSIENT_ERR;
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

  nsRefPtr<IDBRequest> request = IDBRequest::Create(this, context, window,
                                                    nsnull);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<OpenDatabaseHelper> openHelper =
    new OpenDatabaseHelper(request, aName, origin);

  nsRefPtr<CheckPermissionsHelper> permissionHelper =
    new CheckPermissionsHelper(openHelper, window, aName, origin);

  nsRefPtr<IndexedDatabaseManager> mgr = IndexedDatabaseManager::GetOrCreate();
  NS_ENSURE_TRUE(mgr, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = mgr->WaitForOpenAllowed(aName, origin, permissionHelper);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

nsresult
OpenDatabaseHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
#ifdef DEBUG
  {
    bool correctThread;
    NS_ASSERTION(NS_SUCCEEDED(IndexedDatabaseManager::Get()->IOThread()->
                              IsOnCurrentThread(&correctThread)) &&
                 correctThread,
                 "Running on the wrong thread!");
  }
#endif
  NS_ASSERTION(!aConnection, "Huh?!");

  if (IndexedDatabaseManager::IsShuttingDown()) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsCOMPtr<nsIFile> dbFile;
  nsresult rv = GetDatabaseFile(mASCIIOrigin, mName, getter_AddRefs(dbFile));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = dbFile->GetPath(mDatabaseFilePath);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsCOMPtr<nsIFile> dbDirectory;
  rv = dbFile->GetParent(getter_AddRefs(dbDirectory));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  bool exists;
  rv = dbDirectory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    bool isDirectory;
    rv = dbDirectory->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    NS_ENSURE_TRUE(isDirectory, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }
  else {
    rv = dbDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  NS_ASSERTION(mgr, "This should never be null!");

  rv = mgr->EnsureQuotaManagementForDirectory(dbDirectory);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsCOMPtr<mozIStorageConnection> connection;
  rv = CreateDatabaseConnection(mName, dbFile, getter_AddRefs(connection));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  // Get the data version.
  nsCOMPtr<mozIStorageStatement> stmt;
  rv = connection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT dataVersion "
    "FROM database"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (!hasResult) {
    NS_ERROR("Database has no dataVersion!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  PRInt64 dataVersion;
  rv = stmt->GetInt64(0, &dataVersion);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (dataVersion > JS_STRUCTURED_CLONE_VERSION) {
    NS_ERROR("Bad data version!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  if (dataVersion < JS_STRUCTURED_CLONE_VERSION) {
    // Need to upgrade the database, here, before returning to the main thread.
    NS_NOTYETIMPLEMENTED("Implement me!");
  }

  mDatabaseId = HashString(mDatabaseFilePath);
  NS_ASSERTION(mDatabaseId, "HashString gave us 0?!");

  rv = IDBFactory::LoadDatabaseInformation(connection, mDatabaseId, mVersion,
                                           mObjectStores);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  for (PRUint32 i = 0; i < mObjectStores.Length(); i++) {
    nsAutoPtr<ObjectStoreInfo>& objectStoreInfo = mObjectStores[i];
    for (PRUint32 j = 0; j < objectStoreInfo->indexes.Length(); j++) {
      IndexInfo& indexInfo = objectStoreInfo->indexes[j];
      mLastIndexId = NS_MAX(indexInfo.id, mLastIndexId);
    }
    mLastObjectStoreId = NS_MAX(objectStoreInfo->id, mLastObjectStoreId);
  }

  return NS_OK;
}

nsresult
OpenDatabaseHelper::GetSuccessResult(JSContext* aCx,
                                     jsval *aVal)
{
  DatabaseInfo* dbInfo;
  if (DatabaseInfo::Get(mDatabaseId, &dbInfo)) {
    NS_ASSERTION(dbInfo->referenceCount, "Bad reference count!");
    ++dbInfo->referenceCount;

#ifdef DEBUG
    {
      NS_ASSERTION(dbInfo->name == mName &&
                   dbInfo->version == mVersion &&
                   dbInfo->id == mDatabaseId &&
                   dbInfo->filePath == mDatabaseFilePath,
                   "Metadata mismatch!");

      PRUint32 objectStoreCount = mObjectStores.Length();
      for (PRUint32 index = 0; index < objectStoreCount; index++) {
        nsAutoPtr<ObjectStoreInfo>& info = mObjectStores[index];
        NS_ASSERTION(info->databaseId == mDatabaseId, "Huh?!");

        ObjectStoreInfo* otherInfo;
        NS_ASSERTION(ObjectStoreInfo::Get(mDatabaseId, info->name, &otherInfo),
                     "ObjectStore not known!");

        NS_ASSERTION(info->name == otherInfo->name &&
                     info->id == otherInfo->id &&
                     info->keyPath == otherInfo->keyPath &&
                     info->autoIncrement == otherInfo->autoIncrement &&
                     info->databaseId == otherInfo->databaseId,
                     "Metadata mismatch!");
        NS_ASSERTION(dbInfo->ContainsStoreName(info->name),
                     "Object store names out of date!");
        NS_ASSERTION(info->indexes.Length() == otherInfo->indexes.Length(),
                     "Bad index length!");

        PRUint32 indexCount = info->indexes.Length();
        for (PRUint32 indexIndex = 0; indexIndex < indexCount; indexIndex++) {
          const IndexInfo& indexInfo = info->indexes[indexIndex];
          const IndexInfo& otherIndexInfo = otherInfo->indexes[indexIndex];
          NS_ASSERTION(indexInfo.id == otherIndexInfo.id,
                       "Bad index id!");
          NS_ASSERTION(indexInfo.name == otherIndexInfo.name,
                       "Bad index name!");
          NS_ASSERTION(indexInfo.keyPath == otherIndexInfo.keyPath,
                       "Bad index keyPath!");
          NS_ASSERTION(indexInfo.unique == otherIndexInfo.unique,
                       "Bad index unique value!");
          NS_ASSERTION(indexInfo.autoIncrement == otherIndexInfo.autoIncrement,
                       "Bad index autoIncrement value!");
        }
      }
    }
#endif

  }
  else {
    nsAutoPtr<DatabaseInfo> newInfo(new DatabaseInfo());

    newInfo->name = mName;
    newInfo->id = mDatabaseId;
    newInfo->filePath = mDatabaseFilePath;
    newInfo->referenceCount = 1;

    if (!DatabaseInfo::Put(newInfo)) {
      NS_ERROR("Failed to add to hash!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    dbInfo = newInfo.forget();

    nsresult rv = IDBFactory::UpdateDatabaseMetadata(dbInfo, mVersion,
                                                     mObjectStores);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    NS_ASSERTION(mObjectStores.IsEmpty(), "Should have swapped!");
  }

  dbInfo->nextObjectStoreId = mLastObjectStoreId + 1;
  dbInfo->nextIndexId = mLastIndexId + 1;

  nsRefPtr<IDBDatabase> database =
    IDBDatabase::Create(mRequest->ScriptContext(), mRequest->Owner(), dbInfo,
                        mASCIIOrigin);
  if (!database) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return WrapNative(aCx, NS_ISUPPORTS_CAST(nsIDOMEventTarget*, database),
                    aVal);
}
