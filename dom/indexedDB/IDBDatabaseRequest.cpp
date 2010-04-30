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
#include "nsDirectoryServiceUtils.h"
#include "nsDOMClassInfo.h"
#include "nsDOMLists.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

#include "AsyncConnectionHelper.h"
#include "IDBEvents.h"
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

class CreateObjectStoreHelper : public AsyncConnectionHelper
{
public:
  CreateObjectStoreHelper(nsCOMPtr<mozIStorageConnection>& aConnection,
                          nsIDOMEventTarget* aTarget,
                          const nsAString& aName,
                          const nsAString& aKeyPath,
                          PRBool aAutoIncrement)
  : AsyncConnectionHelper(aConnection, aTarget), mName(aName),
    mKeyPath(aKeyPath), mAutoIncrement(aAutoIncrement)
  { }

  PRUint16 DoDatabaseWork();
  void GetSuccessResult(nsIWritableVariant* aResult);

protected:
  nsString mName;
  nsString mKeyPath;
  PRBool mAutoIncrement;
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
      "id INTEGER NOT NULL, "
      "name TEXT NOT NULL, "
      "keyPath TEXT DEFAULT NULL, "
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
      "id INTEGER NOT NULL, "
      "object_store_id INTEGER NOT NULL, "
      "data TEXT NOT NULL, "
      "key_value TEXT DEFAULT NULL, "
      "PRIMARY KEY (id), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE CASCADE"
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
      "id INTEGER NOT NULL, "
      "object_store_id INTEGER NOT NULL, "
      "data TEXT NOT NULL, "
      "PRIMARY KEY (id), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE CASCADE"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX ai_key_index "
    "ON ai_object_data (id, object_store_id);"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  return aDBConn->SetSchemaVersion(DB_SCHEMA_VERSION);
}

already_AddRefed<mozIStorageConnection>
NewConnection(const nsCString& aOrigin)
{
  NS_PRECONDITION(!NS_IsMainThread(),
                  "Opening a database on the main thread!");

  nsCOMPtr<nsIFile> dbFile;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(dbFile));
  NS_ENSURE_SUCCESS(rv, nsnull);

  rv = dbFile->Append(NS_LITERAL_STRING("indexedDB_files"));
  NS_ENSURE_SUCCESS(rv, nsnull);

  PRBool exists;
  rv = dbFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, nsnull);

  if (exists) {
    PRBool isDirectory;
    rv = dbFile->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, nsnull);
    NS_ENSURE_TRUE(isDirectory, nsnull);
  }
  else {
    rv = dbFile->Create(nsIFile::DIRECTORY_TYPE, 0755);
    NS_ENSURE_SUCCESS(rv, nsnull);
  }

  // Replace illegal filename characters.
  static const char illegalChars[] = {':', '/', '\\' };
  nsCString fname(aOrigin);
  for (size_t i = 0; i < NS_ARRAY_LENGTH(illegalChars); i++) {
    fname.ReplaceChar(illegalChars[i], '_');
  }

  fname.AppendLiteral(".sqlite");

  rv = dbFile->Append(NS_ConvertUTF8toUTF16(fname));
  NS_ENSURE_SUCCESS(rv, nsnull);

  rv = dbFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsCOMPtr<mozIStorageService> ss =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);

  nsCOMPtr<mozIStorageConnection> conn;
  rv = ss->OpenDatabase(dbFile, getter_AddRefs(conn));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // Nuke the database file.  The web services can recreate their data.
    rv = dbFile->Remove(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, nsnull);

    exists = PR_FALSE;

    rv = ss->OpenDatabase(dbFile, getter_AddRefs(conn));
  }
  NS_ENSURE_SUCCESS(rv, nsnull);

  // Check to make sure that the database schema is correct.
  PRInt32 schemaVersion;
  rv = conn->GetSchemaVersion(&schemaVersion);
  NS_ENSURE_SUCCESS(rv, nsnull);

  if (schemaVersion != DB_SCHEMA_VERSION) {
    if (exists) {
      // If the connection is not at the right schema version, nuke it.
      rv = dbFile->Remove(PR_FALSE);
      NS_ENSURE_SUCCESS(rv, nsnull);

      rv = ss->OpenDatabase(dbFile, getter_AddRefs(conn));
      NS_ENSURE_SUCCESS(rv, nsnull);
    }

    rv = CreateTables(conn);
    NS_ENSURE_SUCCESS(rv, nsnull);
  }

#ifdef DEBUG
  // Check to make sure that the database schema is correct.
  NS_ASSERTION(NS_SUCCEEDED(conn->GetSchemaVersion(&schemaVersion)) &&
               schemaVersion == DB_SCHEMA_VERSION,
               "CreateTables failed!");

  // Turn on foreign key constraints in debug builds to catch bugs!
  (void)conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "PRAGMA foreign_keys = ON;"
  ));
#endif

  return conn.forget();
}

} // anonymous namespace

// static
already_AddRefed<nsIIDBDatabaseRequest>
IDBDatabaseRequest::Create(const nsAString& aName,
                           const nsAString& aDescription,
                           PRBool aReadOnly)
{
  nsRefPtr<IDBDatabaseRequest> db(new IDBDatabaseRequest());

  db->mStorageThread = new LazyIdleThread(kDefaultThreadTimeoutMS);

  db->mName.Assign(aName);
  db->mDescription.Assign(aDescription);
  db->mReadOnly = aReadOnly;

  db->mObjectStores = new nsDOMStringList();
  db->mIndexes = new nsDOMStringList();

  nsIIDBDatabaseRequest* result;
  db.forget(&result);
  return result;
}

IDBDatabaseRequest::IDBDatabaseRequest()
{
  
}

IDBDatabaseRequest::~IDBDatabaseRequest()
{
  if (mStorageThread) {
    mStorageThread->Shutdown();
  }
}

NS_IMPL_ADDREF(IDBDatabaseRequest)
NS_IMPL_RELEASE(IDBDatabaseRequest)

NS_INTERFACE_MAP_BEGIN(IDBDatabaseRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIIDBDatabaseRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBDatabaseRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBDatabase)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBDatabaseRequest)
NS_INTERFACE_MAP_END

DOMCI_DATA(IDBDatabaseRequest, IDBDatabaseRequest)

NS_IMETHODIMP
IDBDatabaseRequest::GetName(nsAString& aName)
{
  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetDescription(nsAString& aDescription)
{
  aDescription.Assign(mDescription);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetVersion(nsAString& aVersion)
{
  aVersion.Assign(mVersion);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetObjectStores(nsIDOMDOMStringList** aObjectStores)
{
  nsCOMPtr<nsIDOMDOMStringList> objectStores(mObjectStores);
  objectStores.forget(aObjectStores);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetIndexes(nsIDOMDOMStringList** aIndexes)
{
  nsCOMPtr<nsIDOMDOMStringList> indexes(mIndexes);
  indexes.forget(aIndexes);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetCurrentTransaction(nsIIDBTransaction** aTransaction)
{
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
  nsRefPtr<IDBRequest> request = GenerateRequest();
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<CreateObjectStoreHelper> helper =
    new CreateObjectStoreHelper(mStorage, request, aName, aKeyPath,
                                aAutoIncrement);
  nsresult rv = helper->Dispatch(mStorageThread);
  NS_ENSURE_SUCCESS(rv, rv);

  IDBRequest* retval;
  request.forget(&retval);
  *_retval = retval;
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::OpenObjectStore(const nsAString& aName,
                                    PRUint16 aMode,
                                    nsIIDBRequest** _retval)
{
  nsRefPtr<IDBRequest> request = GenerateRequest();
  /*
  nsresult rv = mDatabase->OpenObjectStore(aName, aMode, request);
  NS_ENSURE_SUCCESS(rv, rv);
  */
  IDBRequest* retval;
  request.forget(&retval);
  *_retval = retval;
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::CreateIndex(const nsAString& aName,
                                const nsAString& aStoreName,
                                const nsAString& aKeyPath,
                                PRBool aUnique,
                                nsIIDBRequest** _retval)
{
  NS_NOTYETIMPLEMENTED("Implement me!");

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::OpenIndex(const nsAString& aName,
                              nsIIDBRequest** _retval)
{
  NS_NOTYETIMPLEMENTED("Implement me!");

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::RemoveObjectStore(const nsAString& aStoreName,
                                      nsIIDBRequest** _retval)
{
  NS_NOTYETIMPLEMENTED("Implement me!");

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::RemoveIndex(const nsAString& aIndexName,
                                nsIIDBRequest** _retval)
{
  NS_NOTYETIMPLEMENTED("Implement me!");

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::SetVersion(const nsAString& aVersion,
                               nsIIDBRequest** _retval)
{
  NS_NOTYETIMPLEMENTED("Implement me!");

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::OpenTransaction(nsIDOMDOMStringList* aStoreNames,
                                    PRUint32 aTimeout,
                                    PRUint8 aArgCount,
                                    nsIIDBRequest** _retval)
{
  NS_NOTYETIMPLEMENTED("Implement me!");

  if (aArgCount < 2) {
    aTimeout = kDefaultDatabaseTimeoutMS;
  }

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

PRUint16
CreateObjectStoreHelper::DoDatabaseWork()
{
  mConnection = NewConnection(NS_LITERAL_CSTRING("http://foo.com/bar.html"));
  return mConnection ? OK : nsIIDBDatabaseError::UNKNOWN_ERR;
}

void
CreateObjectStoreHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  aResult->SetAsBool(PR_TRUE);
}
