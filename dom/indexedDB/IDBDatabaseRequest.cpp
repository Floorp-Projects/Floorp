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

#include "IDBDatabaseRequest.h"

#include "mozilla/Storage.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsContentUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDOMClassInfo.h"
#include "nsHashKeys.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

#include "AsyncConnectionHelper.h"
#include "IDBEvents.h"
#include "IDBObjectStoreRequest.h"
#include "IDBRequest.h"
#include "LazyIdleThread.h"

#define DB_SCHEMA_VERSION 1

USING_INDEXEDDB_NAMESPACE

namespace {

const PRUint32 kDefaultDatabaseTimeoutMS = 5000;
const PRUint32 kDefaultThreadTimeoutMS = 30000;

inline
nsISupports*
isupports_cast(IDBDatabaseRequest* aClassPtr)
{
  return static_cast<nsISupports*>(
    static_cast<IDBRequest::Generator*>(aClassPtr));
}

class CloseConnectionRunnable : public nsRunnable
{
public:
  CloseConnectionRunnable(nsCOMPtr<mozIStorageConnection>& aConnection)
  {
    mConnection.swap(aConnection);
  }

  NS_IMETHOD Run()
  {
    if (mConnection && NS_FAILED(mConnection->Close())) {
      NS_WARNING("Failed to close connection!");
    }
    return NS_OK;
  }

private:
  nsCOMPtr<mozIStorageConnection> mConnection;
};

class CreateObjectStoreHelper : public AsyncConnectionHelper
{
public:
  CreateObjectStoreHelper(IDBDatabaseRequest* aDatabase,
                          nsIDOMEventTarget* aTarget,
                          const nsAString& aName,
                          const nsAString& aKeyPath,
                          bool aAutoIncrement)
  : AsyncConnectionHelper(aDatabase, aTarget), mName(aName), mKeyPath(aKeyPath),
    mAutoIncrement(aAutoIncrement), mId(LL_MININT)
  { }

  PRUint16 DoDatabaseWork();
  PRUint16 OnSuccess(nsIDOMEventTarget* aTarget);
  void GetSuccessResult(nsIWritableVariant* aResult);

protected:
  // In-params.
  nsString mName;
  nsString mKeyPath;
  bool mAutoIncrement;

  // Created during Init.
  nsRefPtr<IDBObjectStoreRequest> mObjectStore;

  // Out-params.
  PRInt64 mId;
};

class OpenObjectStoreHelper : public CreateObjectStoreHelper
{
public:
  OpenObjectStoreHelper(IDBDatabaseRequest* aDatabase,
                        nsIDOMEventTarget* aTarget,
                        const nsAString& aName,
                        PRUint16 aMode)
  : CreateObjectStoreHelper(aDatabase, aTarget, aName, EmptyString(), false),
    mMode(aMode)
  { }

  PRUint16 DoDatabaseWork();
  PRUint16 OnSuccess(nsIDOMEventTarget* aTarget);
  void GetSuccessResult(nsIWritableVariant* aResult);

protected:
  // In-params.
  PRUint16 mMode;
};

class RemoveObjectStoreHelper : public AsyncConnectionHelper
{
public:
  RemoveObjectStoreHelper(IDBDatabaseRequest* aDatabase,
                          nsIDOMEventTarget* aTarget,
                          const nsAString& aName)
  : AsyncConnectionHelper(aDatabase, aTarget), mName(aName)
  { }

  PRUint16 DoDatabaseWork();
  void GetSuccessResult(nsIWritableVariant* aResult);

private:
  // In-params.
  nsString mName;
};

/**
 * Creates the needed tables and their indexes.
 *
 * @param aDBConn
 *        The database connection to create the tables on.
 */
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
      "description TEXT NOT NULL, "
      "version TEXT DEFAULT NULL, "
      "UNIQUE (name)"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Table `object_store`
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE object_store ("
      "id INTEGER, "
      "name TEXT NOT NULL, "
      "key_path TEXT DEFAULT NULL, "
      "auto_increment INTEGER NOT NULL DEFAULT 0, "
      "readers INTEGER NOT NULL DEFAULT 0, "
      "is_writing INTEGER NOT NULL DEFAULT 0, "
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
      "data TEXT NOT NULL, "
      "key_value TEXT DEFAULT NULL, "
      "PRIMARY KEY (id), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX key_index "
    "ON object_data (id, object_store_id);"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Table `ai_object_data`
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE ai_object_data ("
      "id INTEGER, "
      "object_store_id INTEGER NOT NULL, "
      "data TEXT NOT NULL, "
      "PRIMARY KEY (id), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX ai_key_index "
    "ON ai_object_data (id, object_store_id);"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDBConn->SetSchemaVersion(DB_SCHEMA_VERSION);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

} // anonymous namespace

// static
already_AddRefed<IDBDatabaseRequest>
IDBDatabaseRequest::Create(const nsAString& aName,
                           const nsAString& aDescription,
                           PRBool aReadOnly)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = nsContentUtils::GetSecurityManager()->
    GetSubjectPrincipal(getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsCString origin;
  if (nsContentUtils::IsSystemPrincipal(principal)) {
    origin.AssignLiteral("chrome");
  }
  else {
    rv = nsContentUtils::GetASCIIOrigin(principal, origin);
    NS_ENSURE_SUCCESS(rv, nsnull);
  }

  nsRefPtr<IDBDatabaseRequest> db(new IDBDatabaseRequest());

  db->mConnectionThread = new LazyIdleThread(kDefaultThreadTimeoutMS);
  db->mConnectionThread->SetWeakIdleObserver(db);

  db->mASCIIOrigin.Assign(origin);
  db->mName.Assign(aName);
  db->mDescription.Assign(aDescription);
  db->mReadOnly = aReadOnly;

#if 1
  // XXX Do this before we load the page! This is all duplicated code that needs
  // to be totally removed before this code sees the light of day!
  NS_WARNING("Using a sync algorithm to open indexedDB data! Fix this now!");

  nsCOMPtr<nsIFile> dbFile;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(dbFile));
  NS_ENSURE_SUCCESS(rv, nsnull);

  rv = dbFile->Append(NS_LITERAL_STRING("indexedDB"));
  NS_ENSURE_SUCCESS(rv, nsnull);

  PRBool exists;
  rv = dbFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, nsnull);

  if (exists) {
    PRBool isDirectory;
    rv = dbFile->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, nsnull);

    if (isDirectory) {
      nsAutoString filename;
      filename.AppendInt(HashString(origin));

      rv = dbFile->Append(filename);
      NS_ENSURE_SUCCESS(rv, nsnull);

      rv = dbFile->Exists(&exists);
      NS_ENSURE_SUCCESS(rv, nsnull);

      if (exists) {
        rv = dbFile->IsDirectory(&isDirectory);
        NS_ENSURE_SUCCESS(rv, nsnull);

        if (isDirectory) {
          filename.Truncate();
          filename.AppendInt(HashString(aName));
          filename.AppendLiteral(".sqlite");

          rv = dbFile->Append(filename);
          NS_ENSURE_SUCCESS(rv, nsnull);

          rv = dbFile->Exists(&exists);
          NS_ENSURE_SUCCESS(rv, nsnull);

          if (exists) {
            nsCOMPtr<mozIStorageService> ss =
              do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);

            nsCOMPtr<mozIStorageConnection> connection;
            rv = ss->OpenDatabase(dbFile, getter_AddRefs(connection));
            NS_ENSURE_SUCCESS(rv, nsnull);

            // Check to make sure that the database schema is correct.
            PRInt32 schemaVersion;
            rv = connection->GetSchemaVersion(&schemaVersion);
            NS_ENSURE_SUCCESS(rv, nsnull);

            if (schemaVersion == DB_SCHEMA_VERSION) {
              nsCOMPtr<mozIStorageStatement> stmt;
              rv = connection->CreateStatement(NS_LITERAL_CSTRING(
                "SELECT name "
                "FROM object_store"
              ), getter_AddRefs(stmt));
              NS_ENSURE_SUCCESS(rv, nsnull);

              PRBool hasResult;
              while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
                nsString name;
                (void)stmt->GetString(0, name);
                NS_ENSURE_TRUE(db->mObjectStoreNames.AppendElement(name),
                               nsnull);
              }
            }
          }
        }
      }
    }
  }
#endif

  return db.forget();
}

IDBDatabaseRequest::IDBDatabaseRequest()
: mReadOnly(PR_FALSE)
{
  
}

IDBDatabaseRequest::~IDBDatabaseRequest()
{
  if (mConnectionThread) {
    mConnectionThread->Shutdown();
  }
}

nsCOMPtr<mozIStorageConnection>&
IDBDatabaseRequest::Connection()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  return mConnection;
}

nsresult
IDBDatabaseRequest::EnsureConnection()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (Connection()) {
    return NS_OK;
  }

  nsCOMPtr<nsIFile> dbFile;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(dbFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbFile->Append(NS_LITERAL_STRING("indexedDB"));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists;
  rv = dbFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    PRBool isDirectory;
    rv = dbFile->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(isDirectory, NS_ERROR_UNEXPECTED);
  }
  else {
    rv = dbFile->Create(nsIFile::DIRECTORY_TYPE, 0755);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsAutoString filename;
  filename.AppendInt(HashString(mASCIIOrigin));

  rv = dbFile->Append(filename);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    PRBool isDirectory;
    rv = dbFile->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(isDirectory, NS_ERROR_UNEXPECTED);
  }
  else {
    rv = dbFile->Create(nsIFile::DIRECTORY_TYPE, 0755);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  filename.Truncate();
  filename.AppendInt(HashString(mName));
  filename.AppendLiteral(".sqlite");

  rv = dbFile->Append(filename);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageService> ss =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenDatabase(dbFile, getter_AddRefs(connection));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // Nuke the database file.  The web services can recreate their data.
    rv = dbFile->Remove(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    exists = PR_FALSE;

    rv = ss->OpenDatabase(dbFile, getter_AddRefs(connection));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Check to make sure that the database schema is correct.
  PRInt32 schemaVersion;
  rv = connection->GetSchemaVersion(&schemaVersion);
  NS_ENSURE_SUCCESS(rv, rv);

  if (schemaVersion != DB_SCHEMA_VERSION) {
    if (exists) {
      // If the connection is not at the right schema version, nuke it.
      rv = dbFile->Remove(PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = ss->OpenDatabase(dbFile, getter_AddRefs(connection));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = CreateTables(connection);
    NS_ENSURE_SUCCESS(rv, rv);
  }

#ifdef DEBUG
  // Check to make sure that the database schema is correct again.
  NS_ASSERTION(NS_SUCCEEDED(connection->GetSchemaVersion(&schemaVersion)) &&
               schemaVersion == DB_SCHEMA_VERSION,
               "CreateTables failed!");

  // Turn on foreign key constraints in debug builds to catch bugs!
  (void)connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "PRAGMA foreign_keys = ON;"
  ));
#endif

  connection.swap(mConnection);
  return NS_OK;
}

void
IDBDatabaseRequest::OnObjectStoreCreated(const nsAString& aName)
{
  NS_ASSERTION(!mObjectStoreNames.Contains(aName),
               "Already got this object store in the list!");
  if (!mObjectStoreNames.AppendElement(aName)) {
    NS_ERROR("Failed to add object store name! OOM?");
  }
}

void
IDBDatabaseRequest::OnIndexCreated(const nsAString& aName)
{
  NS_ASSERTION(!mIndexNames.Contains(aName),
               "Already got this index in the list!");
  if (!mIndexNames.AppendElement(aName)) {
    NS_ERROR("Failed to add index name! OOM?");
  }
}

void
IDBDatabaseRequest::OnObjectStoreRemoved(const nsAString& aName)
{
  NS_ASSERTION(mObjectStoreNames.Contains(aName),
               "Didn't know about this object store before!");
  PRUint32 count = mObjectStoreNames.Length();
  for (PRUint32 index = 0; index < count; index++) {
    if (mObjectStoreNames[index].Equals(aName)) {
      mObjectStoreNames.RemoveElementAt(index);
      return;
    }
  }
  NS_NOTREACHED("Shouldn't get here!");
}

NS_IMPL_ADDREF(IDBDatabaseRequest)
NS_IMPL_RELEASE(IDBDatabaseRequest)

NS_INTERFACE_MAP_BEGIN(IDBDatabaseRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIIDBDatabaseRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBDatabaseRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBDatabase)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBDatabaseRequest)
NS_INTERFACE_MAP_END

DOMCI_DATA(IDBDatabaseRequest, IDBDatabaseRequest)

NS_IMETHODIMP
IDBDatabaseRequest::GetName(nsAString& aName)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetDescription(nsAString& aDescription)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aDescription.Assign(mDescription);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetVersion(nsAString& aVersion)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aVersion.Assign(mVersion);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetObjectStores(nsIDOMDOMStringList** aObjectStores)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<nsDOMStringList> list(new nsDOMStringList());
  PRUint32 count = mObjectStoreNames.Length();
  for (PRUint32 index = 0; index < count; index++) {
    NS_ENSURE_TRUE(list->Add(mObjectStoreNames[index]), NS_ERROR_OUT_OF_MEMORY);
  }

  list.forget(aObjectStores);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetIndexes(nsIDOMDOMStringList** aIndexes)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<nsDOMStringList> list(new nsDOMStringList());
  PRUint32 count = mIndexNames.Length();
  for (PRUint32 index = 0; index < count; index++) {
    NS_ENSURE_TRUE(list->Add(mIndexNames[index]), NS_ERROR_OUT_OF_MEMORY);
  }

  
  list.forget(aIndexes);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetCurrentTransaction(nsIIDBTransaction** aTransaction)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIIDBTransaction> transaction(mCurrentTransaction);
  transaction.forget(aTransaction);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::CreateObjectStore(const nsAString& aName,
                                      const nsAString& aKeyPath,
                                      PRBool aAutoIncrement,
                                      nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (aName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest();
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  bool exists = false;
  PRUint32 count = mObjectStoreNames.Length();
  for (PRUint32 index = 0; index < count; index++) {
    if (mObjectStoreNames[index].Equals(aName)) {
      exists = true;
      break;
    }
  }

  nsresult rv;
  if (NS_UNLIKELY(exists)) {
    nsCOMPtr<nsIRunnable> runnable =
      IDBErrorEvent::CreateRunnable(request,
                                    nsIIDBDatabaseError::CONSTRAINT_ERR);
    NS_ENSURE_TRUE(runnable, NS_ERROR_UNEXPECTED);
    rv = NS_DispatchToCurrentThread(runnable);
  }
  else {
    nsRefPtr<CreateObjectStoreHelper> helper =
      new CreateObjectStoreHelper(this, request, aName, aKeyPath,
                                  !!aAutoIncrement);
    rv = helper->Dispatch(mConnectionThread);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::OpenObjectStore(const nsAString& aName,
                                    PRUint16 aMode,
                                    PRUint8 aOptionalArgCount,
                                    nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (aName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aOptionalArgCount) {
    if (aMode != nsIIDBObjectStore::READ_WRITE &&
        aMode != nsIIDBObjectStore::READ_ONLY &&
        aMode != nsIIDBObjectStore::SNAPSHOT_READ) {
      return NS_ERROR_INVALID_ARG;
    }
  }
  else {
    aMode = nsIIDBObjectStore::READ_WRITE;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest();

  bool exists = false;
  PRUint32 count = mObjectStoreNames.Length();
  for (PRUint32 index = 0; index < count; index++) {
    if (mObjectStoreNames[index].Equals(aName)) {
      exists = true;
      break;
    }
  }

  nsresult rv;
  if (NS_UNLIKELY(!exists)) {
    nsCOMPtr<nsIRunnable> runnable =
      IDBErrorEvent::CreateRunnable(request,
                                    nsIIDBDatabaseError::CONSTRAINT_ERR);
    NS_ENSURE_TRUE(runnable, NS_ERROR_UNEXPECTED);
    rv = NS_DispatchToCurrentThread(runnable);
  }
  else {
    nsRefPtr<OpenObjectStoreHelper> helper =
      new OpenObjectStoreHelper(this, request, aName, aMode);
    rv = helper->Dispatch(mConnectionThread);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::CreateIndex(const nsAString& aName,
                                const nsAString& aStoreName,
                                const nsAString& aKeyPath,
                                PRBool aUnique,
                                nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_NOTYETIMPLEMENTED("Implement me!");

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::OpenIndex(const nsAString& aName,
                              nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_NOTYETIMPLEMENTED("Implement me!");

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::RemoveObjectStore(const nsAString& aName,
                                      nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (aName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest();

  bool exists = false;
  PRUint32 count = mObjectStoreNames.Length();
  for (PRUint32 index = 0; index < count; index++) {
    if (mObjectStoreNames[index].Equals(aName)) {
      exists = true;
      break;
    }
  }

  nsresult rv;
  if (NS_UNLIKELY(!exists)) {
    nsCOMPtr<nsIRunnable> runnable =
      IDBErrorEvent::CreateRunnable(request,
                                    nsIIDBDatabaseError::NOT_FOUND_ERR);
    NS_ENSURE_TRUE(runnable, NS_ERROR_UNEXPECTED);
    rv = NS_DispatchToCurrentThread(runnable);
  }
  else {
    nsRefPtr<RemoveObjectStoreHelper> helper =
      new RemoveObjectStoreHelper(this, request, aName);
    rv = helper->Dispatch(mConnectionThread);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::RemoveIndex(const nsAString& aIndexName,
                                nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_NOTYETIMPLEMENTED("Implement me!");

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::SetVersion(const nsAString& aVersion,
                               nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_NOTYETIMPLEMENTED("Implement me!");

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::OpenTransaction(nsIDOMDOMStringList* aStoreNames,
                                    PRUint32 aTimeout,
                                    PRUint8 aOptionalArgCount,
                                    nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_NOTYETIMPLEMENTED("Implement me!");

  if (!aOptionalArgCount) {
    aTimeout = kDefaultDatabaseTimeoutMS;
  }

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::Observe(nsISupports* aSubject,
                            const char* aTopic,
                            const PRUnichar* aData)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ENSURE_FALSE(strcmp(aTopic, IDLE_THREAD_TOPIC), NS_ERROR_UNEXPECTED);

  // This should be safe to clear mStorage before we're deleted since we own
  // the thread and must join with it before we can be deleted.
  nsCOMPtr<nsIRunnable> runnable = new CloseConnectionRunnable(mConnection);

  nsCOMPtr<nsIThread> thread(do_QueryInterface(aSubject));
  NS_ENSURE_TRUE(thread, NS_NOINTERFACE);

  nsresult rv = thread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

PRUint16
CreateObjectStoreHelper::DoDatabaseWork()
{
  nsresult rv = mDatabase->EnsureConnection();
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  nsCOMPtr<mozIStorageConnection> connection = mDatabase->Connection();

  // Rollback on any errors.
  mozStorageTransaction transaction(connection, PR_FALSE);

  // Insert the data into the database.
  nsCOMPtr<mozIStorageStatement> stmt;
  rv = connection->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO object_store (name, key_path, auto_increment) "
    "VALUES (:name, :key_path, :auto_increment)"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mName);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  if (mKeyPath.IsVoid()) {
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("key_path"));
  } else {
    rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key_path"), mKeyPath);
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("auto_increment"),
                             mAutoIncrement ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  if (NS_FAILED(stmt->Execute())) {
    return nsIIDBDatabaseError::CONSTRAINT_ERR;
  }

  // Get the id of this object store, and store it for future use.
  (void)connection->GetLastInsertRowID(&mId);

  return NS_SUCCEEDED(transaction.Commit()) ?
         OK :
         nsIIDBDatabaseError::UNKNOWN_ERR;
}

PRUint16
CreateObjectStoreHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  mObjectStore =
    IDBObjectStoreRequest::Create(mDatabase, mName, mKeyPath, mAutoIncrement,
                                  nsIIDBObjectStore::READ_WRITE, mId);
  NS_ENSURE_TRUE(mObjectStore, nsIIDBDatabaseError::UNKNOWN_ERR);

  mDatabase->OnObjectStoreCreated(mName);

  return AsyncConnectionHelper::OnSuccess(aTarget);
}

void
CreateObjectStoreHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  nsCOMPtr<nsISupports> result =
    do_QueryInterface(static_cast<nsIIDBObjectStoreRequest*>(mObjectStore));
  NS_ASSERTION(result, "Failed to QI!");

  aResult->SetAsISupports(result);
}

PRUint16
OpenObjectStoreHelper::DoDatabaseWork()
{
  nsresult rv = mDatabase->EnsureConnection();
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  nsCOMPtr<mozIStorageConnection> connection = mDatabase->Connection();

  // TODO pull this up to the connection and cache it so opening these is
  // cheaper.
  nsCOMPtr<mozIStorageStatement> stmt;
  rv = connection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id, key_path, auto_increment "
    "FROM object_store "
    "WHERE name = :name"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mName);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);
  NS_ENSURE_TRUE(hasResult, nsIIDBDatabaseError::NOT_FOUND_ERR);

  mId = stmt->AsInt64(0);
  (void)stmt->GetString(1, mKeyPath);
  mAutoIncrement = !!stmt->AsInt32(2);

  return OK;
}

PRUint16
OpenObjectStoreHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  mObjectStore =
    IDBObjectStoreRequest::Create(mDatabase, mName, mKeyPath, mAutoIncrement,
                                  mMode, mId);
  NS_ENSURE_TRUE(mObjectStore, nsIIDBDatabaseError::UNKNOWN_ERR);

  return AsyncConnectionHelper::OnSuccess(aTarget);
}

void
OpenObjectStoreHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  nsCOMPtr<nsISupports> result =
    do_QueryInterface(static_cast<nsIIDBObjectStoreRequest*>(mObjectStore));
  NS_ASSERTION(result, "Failed to QI!");

  aResult->SetAsISupports(result);
}

PRUint16
RemoveObjectStoreHelper::DoDatabaseWork()
{
  nsresult rv = mDatabase->EnsureConnection();
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  nsCOMPtr<mozIStorageConnection> connection = mDatabase->Connection();

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = connection->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM object_store "
    "WHERE name = :name "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mName);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  if (NS_FAILED(stmt->Execute())) {
    return nsIIDBDatabaseError::NOT_FOUND_ERR;
  }

  return OK;
}

void
RemoveObjectStoreHelper::GetSuccessResult(nsIWritableVariant* /* aResult */)
{
  mDatabase->OnObjectStoreRemoved(mName);
}
