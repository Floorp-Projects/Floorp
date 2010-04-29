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
 *   Shawn Wilsher <me@shawnwilsher.com>
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

#include "AsyncDatabaseConnection.h"

#include "nsIIDBObjectStore.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIVariant.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCID.h"

#include "mozilla/Storage.h"

#include "IDBEvents.h"

#define DB_SCHEMA_VERSION 1

USING_INDEXEDDB_NAMESPACE

namespace {

PRUint32 gConnectionCount = 0;
nsIThread* gDatabaseThread = nsnull;
PRBool gXPCOMShutdown = PR_FALSE;

struct ObjectStore
{
  ObjectStore() : autoIncrement(PR_FALSE), readerCount(0), writing(PR_FALSE) { }

  nsString name;
  nsString keyPath;
  PRBool autoIncrement;

  nsTArray<nsString> mIndexes;
  PRUint32 readerCount;
  PRBool writing;
};

nsTArray<ObjectStore>* gObjectStores = nsnull;

class ShutdownObserver : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD
  Observe(nsISupports* aSubject,
          const char* aTopic,
          const PRUnichar* aData)
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    NS_ASSERTION(!gXPCOMShutdown, "Already shutdown?!");

    if (gDatabaseThread) {
      nsresult rv = gDatabaseThread->Shutdown();
      NS_RELEASE(gDatabaseThread);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    delete gObjectStores;
    gObjectStores = nsnull;

    gXPCOMShutdown = PR_TRUE;
    return NS_OK;
  }
};

already_AddRefed<nsIDOMEvent>
MakeSuccessEventBool(PRBool aResult)
{
  nsCOMPtr<nsIWritableVariant> variant =
    do_CreateInstance(NS_VARIANT_CONTRACTID);
  NS_ENSURE_TRUE(variant, nsnull);

  nsresult rv = variant->SetAsBool(aResult);
  NS_ENSURE_SUCCESS(rv, nsnull);

  rv = variant->SetWritable(PR_FALSE);
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsCOMPtr<nsIDOMEvent> event(IDBSuccessEvent::Create(variant));
  return event.forget();
}

already_AddRefed<nsIDOMEvent>
MakeSuccessEvent(nsISupports* aResult)
{
  nsCOMPtr<nsIWritableVariant> variant =
    do_CreateInstance(NS_VARIANT_CONTRACTID);
  NS_ENSURE_TRUE(variant, nsnull);

  nsresult rv = variant->SetAsISupports(aResult);
  NS_ENSURE_SUCCESS(rv, nsnull);

  rv = variant->SetWritable(PR_FALSE);
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsCOMPtr<nsIDOMEvent> event(IDBSuccessEvent::Create(variant));
  return event.forget();
}

class DatabaseWorkHelper : public nsRunnable
{
public:
  static const PRUint16 OK = PR_UINT16_MAX;

  DatabaseWorkHelper(nsIDOMEventTarget* aTarget)
  : mTarget(aTarget), mError(PR_FALSE), mErrorCode(0)
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  }

  virtual ~DatabaseWorkHelper()
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    NS_ASSERTION(!mTarget, "Badness!");
  }

  NS_IMETHOD
  Run()
  {
    if (NS_IsMainThread()) {
      if (mError) {
        // Automatically fire error events.
        nsCOMPtr<nsIDOMEvent> event(IDBErrorEvent::Create(mErrorCode));
        PRBool dummy;
        mTarget->DispatchEvent(event, &dummy);
      }
      else {
        OnSuccess();
      }

      mTarget = nsnull;
      return NS_OK;
    }

    NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

    // Don't let the function mess with mTarget on the wrong thread!
    {
      nsCOMPtr<nsIDOMEventTarget> target;
      target.swap(mTarget);

      mErrorCode = DoDatabaseWork();

      target.swap(mTarget);
    }

    mError = mErrorCode != OK;

    NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL);
    return NS_OK;
  }

  nsresult
  Dispatch()
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    if (!gDatabaseThread) {
      return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
    }
    return gDatabaseThread->Dispatch(this, NS_DISPATCH_NORMAL);
  }

protected:
  virtual PRUint16 DoDatabaseWork() = 0;
  virtual void OnSuccess() = 0;

  nsCOMPtr<nsIDOMEventTarget> mTarget;

private:
  PRBool mError;
  PRUint16 mErrorCode;
};

class CreateObjectStoreHelper : public DatabaseWorkHelper
{
public:
  CreateObjectStoreHelper(const nsAString& aName,
                          const nsAString& aKeyPath,
                          PRBool aAutoIncrement,
                          nsIDOMEventTarget* aTarget)
  : DatabaseWorkHelper(aTarget), mName(aName), mKeyPath(aKeyPath),
    mAutoIncrement(aAutoIncrement)
  { }

  PRUint16
  DoDatabaseWork()
  {
    PRUint32 count = gObjectStores->Length();
    for (PRUint32 index = 0; index < count; index++) {
      if (gObjectStores->ElementAt(index).name.Equals(mName)) {
        return nsIIDBDatabaseError::CONSTRAINT_ERR;
      }
    }

    ObjectStore* newStore = gObjectStores->AppendElement();
    newStore->name.Assign(mName);
    newStore->keyPath.Assign(mKeyPath);
    newStore->autoIncrement = mAutoIncrement;

    return OK;
  }

  void
  OnSuccess()
  {
    nsCOMPtr<nsIDOMEvent> event(MakeSuccessEventBool(PR_TRUE));
    PRBool dummy;
    mTarget->DispatchEvent(event, &dummy);
  }

protected:
  nsString mName;
  nsString mKeyPath;
  PRBool mAutoIncrement;
};

class OpenObjectStoreHelper : public DatabaseWorkHelper
{
public:
  OpenObjectStoreHelper(const nsAString& aName,
                        PRUint16 aMode,
                        nsIDOMEventTarget* aTarget)
  : DatabaseWorkHelper(aTarget), mName(aName), mMode(aMode)
  { }

  PRUint16
  DoDatabaseWork()
  {
    PRUint32 count = gObjectStores->Length();
    for (PRUint32 index = 0; index < count; index++) {
      ObjectStore& store = gObjectStores->ElementAt(index);
      if (store.name.Equals(mName)) {
        if (mMode == nsIIDBObjectStore::READ_WRITE) {
          store.writing = PR_TRUE;
        }
        store.readerCount++;
        return OK;
      }
    }

    return nsIIDBDatabaseError::NOT_FOUND_ERR;
  }

  void
  OnSuccess()
  {
    nsCOMPtr<nsIDOMEvent> event(MakeSuccessEventBool(PR_TRUE));
    PRBool dummy;
    mTarget->DispatchEvent(event, &dummy);
  }

protected:
  nsString mName;
  PRUint16 mMode;
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
      "id INTEGER DEFAULT NULL, "
      "name TEXT NOT NULL, "
      "key_path TEXT DEFAULT NULL, "
      "autoincrement INTEGER NOT NULL DEFAULT 0, "
      "readers INTEGER NOT NULL DEFAULT 0, "
      "isWriting INTEGER NOT NULL DEFAULT 0, "
      "PRIMARY KEY (id), "
      "UNIQUE (name)"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Table `object_data`
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE object_data ("
      "id INTEGER NOT NULL AUTOINCREMENT, "
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
      "id INTEGER NOT NULL AUTOINCREMENT, "
      "object_store_id INTEGER NOT NULL, "
      "data TEXT NOT NULL, "
      "PRIMARY KEY (id), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE CASCADE"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX key_index "
    "ON ai_object_data (id, object_store_id);"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  return aDBConn->SetSchemaVersion(DB_SCHEMA_VERSION);
}

already_AddRefed<mozIStorageConnection>
NewConnection(const nsCString& aOrigin, bool aDeleteIfExists = false);

bool
ConnectionReady(nsCOMPtr<mozIStorageConnection>& aDBConn,
                const nsCString& aOrigin)
{
  NS_PRECONDITION(!NS_IsMainThread(),
                  "Checking a database on the main thread!");
  NS_PRECONDITION(aDBConn, "Passing a null database connection!");

  // Check to make sure that the database schema is correct.
  PRInt32 schemaVersion;
  nsresult rv = aDBConn->GetSchemaVersion(&schemaVersion);
  NS_ENSURE_SUCCESS(rv, false);

  // If the connection is not at the right schema version, nuke it.
  if (schemaVersion != DB_SCHEMA_VERSION) {
    aDBConn = NewConnection(aOrigin, true);
    NS_ENSURE_TRUE(aDBConn, false);
    rv = CreateTables(aDBConn);
    NS_ENSURE_SUCCESS(rv, false);
  }

#ifdef DEBUG
  // Turn on foreign key constraints in debug builds to catch bugs!
  (void)aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "PRAGMA foreign_keys = ON;"
  ));
#endif

  return true;
}

already_AddRefed<mozIStorageConnection>
NewConnection(const nsCString& aOrigin,
              bool aDeleteIfExists)
{
  NS_PRECONDITION(!NS_IsMainThread(),
                  "Opening a database on the main thread!");

  nsCOMPtr<nsIFile> dbFile;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(dbFile));
  NS_ENSURE_SUCCESS(rv, nsnull);
  rv = dbFile->Append(NS_LITERAL_STRING("indexedDB_files"));
  NS_ENSURE_SUCCESS(rv, nsnull);
  // Replace illegal filename characters.
  char illegalChars[] = {':', '/', '\\' };
  nsCString fname(aOrigin);
  for (size_t i = 0; i < NS_ARRAY_LENGTH(illegalChars); i++) {
    fname.ReplaceChar(illegalChars[i], '_');
  }
  fname.AppendLiteral(".sqlite");
  rv = dbFile->Append(NS_ConvertUTF8toUTF16(fname));
  NS_ENSURE_SUCCESS(rv, nsnull);

  if (aDeleteIfExists) {
    rv = dbFile->Remove(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, nsnull);
  }

  nsCOMPtr<mozIStorageService> ss =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);

  nsCOMPtr<mozIStorageConnection> conn;
  rv = ss->OpenDatabase(dbFile, getter_AddRefs(conn));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // Nuke the database file.  The web services can recreate their data.
    rv = dbFile->Remove(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, nsnull);
    rv = ss->OpenDatabase(dbFile, getter_AddRefs(conn));
  }
  NS_ENSURE_SUCCESS(rv, nsnull);

  NS_ENSURE_TRUE(ConnectionReady(conn, aOrigin), false);
  return conn.forget();
}

} // anonymous namespace

NS_IMPL_ISUPPORTS1(ShutdownObserver, nsIObserver);

// static
AsyncDatabaseConnection*
AsyncDatabaseConnection::OpenConnection(const nsAString& aName,
                                        const nsAString& aDescription,
                                        PRBool aReadOnly)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  nsAutoPtr<AsyncDatabaseConnection> connection(new AsyncDatabaseConnection());

  if (!gConnectionCount++) {
    if (gXPCOMShutdown) {
      NS_ERROR("Creating a new connection after shutdown!");
      return nsnull;
    }

    nsCOMPtr<nsIObserverService> obs =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    NS_ENSURE_TRUE(obs, nsnull);

    nsCOMPtr<nsIObserver> observer(new ShutdownObserver());
    nsresult rv = obs->AddObserver(observer, "xpcom-shutdown-threads",
                                   PR_FALSE);
    NS_ENSURE_SUCCESS(rv, nsnull);

    NS_ASSERTION(!gObjectStores, "Already got a store?!");
    gObjectStores = new nsAutoTArray<ObjectStore, 10>;

    rv = NS_NewThread(&gDatabaseThread);
    NS_ENSURE_SUCCESS(rv, nsnull);
  }

  NS_ASSERTION(gDatabaseThread, "No thread?!");
  return connection.forget();
}

AsyncDatabaseConnection::AsyncDatabaseConnection()
: mReadOnly(PR_FALSE)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

AsyncDatabaseConnection::~AsyncDatabaseConnection()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

nsresult
AsyncDatabaseConnection::CreateObjectStore(const nsAString& aName,
                                           const nsAString& aKeyPath,
                                           PRBool aAutoIncrement,
                                           nsIDOMEventTarget* aTarget)
{
  nsRefPtr<CreateObjectStoreHelper> helper =
    new CreateObjectStoreHelper(aName, aKeyPath, aAutoIncrement, aTarget);
  return helper->Dispatch();
}

nsresult
AsyncDatabaseConnection::OpenObjectStore(const nsAString& aName,
                                         PRUint16 aMode,
                                         nsIDOMEventTarget* aTarget)
{
  nsRefPtr<OpenObjectStoreHelper> helper =
    new OpenObjectStoreHelper(aName, aMode, aTarget);
  return helper->Dispatch();
}
