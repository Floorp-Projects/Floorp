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

#include "AsyncConnectionHelper.h"

#include "mozIStorageConnection.h"

#include "mozilla/Storage.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsHashKeys.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

#include "IDBEvents.h"

#define DB_SCHEMA_VERSION 1

USING_INDEXEDDB_NAMESPACE

namespace {

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

} //anonymous namespace

AsyncConnectionHelper::AsyncConnectionHelper(
                                   const nsACString& aASCIIOrigin,
                                   nsCOMPtr<mozIStorageConnection>& aConnection,
                                   nsIDOMEventTarget* aTarget)
: mConnection(aConnection),
  mASCIIOrigin(aASCIIOrigin),
  mTarget(aTarget),
  mErrorCode(0),
  mError(PR_FALSE)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mTarget, "Null target!");
}

AsyncConnectionHelper::~AsyncConnectionHelper()
{
  if (!NS_IsMainThread()) {
    NS_ASSERTION(mErrorCode == NOREPLY,
                 "This should only happen if NOREPLY was returned!");

    nsIDOMEventTarget* target;
    mTarget.forget(&target);

    nsCOMPtr<nsIThread> mainThread;
    NS_GetMainThread(getter_AddRefs(mainThread));

    if (mainThread) {
      NS_ProxyRelease(mainThread, target);
    }
    else {
      NS_WARNING("Couldn't get the main thread?! Leaking instead of crashing.");
    }
  }

  NS_ASSERTION(!mTarget, "Should have been released before now!");
  NS_ASSERTION(!mDatabaseThread, "Should have been released before now!");
}

NS_IMPL_THREADSAFE_ISUPPORTS1(AsyncConnectionHelper, nsIRunnable)

NS_IMETHODIMP
AsyncConnectionHelper::Run()
{
  if (NS_IsMainThread()) {
    if (mError) {
      OnError(mTarget, mErrorCode);
    }
    else {
      OnSuccess(mTarget);
    }

    mTarget = nsnull;
    return NS_OK;
  }

#ifdef DEBUG
  {
    PRBool ok;
    NS_ASSERTION(NS_SUCCEEDED(mDatabaseThread->IsOnCurrentThread(&ok)) && ok,
                 "Wrong thread!");
    mDatabaseThread = nsnull;
  }
#endif

  mErrorCode = DoDatabaseWork();

  if (mErrorCode != NOREPLY) {
    mError = mErrorCode != OK;

    return NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL);
  }

  return NS_OK;
}

nsresult
AsyncConnectionHelper::Dispatch(nsIThread* aDatabaseThread)
{
#ifdef DEBUG
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  {
    PRBool sameThread;
    nsresult rv = aDatabaseThread->IsOnCurrentThread(&sameThread);
    NS_ASSERTION(NS_SUCCEEDED(rv), "IsOnCurrentThread failed!");
    NS_ASSERTION(!sameThread, "Dispatching to main thread not supported!");
  }
  mDatabaseThread = aDatabaseThread;
#endif

  return aDatabaseThread->Dispatch(this, NS_DISPATCH_NORMAL);
}

void
AsyncConnectionHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIWritableVariant> variant =
    do_CreateInstance(NS_VARIANT_CONTRACTID);
  if (!variant) {
    NS_ERROR("Couldn't create variant!");
    return;
  }

  GetSuccessResult(variant);

  if (NS_FAILED(variant->SetWritable(PR_FALSE))) {
    NS_ERROR("Failed to make variant readonly!");
    return;
  }

  nsCOMPtr<nsIDOMEvent> event(IDBSuccessEvent::Create(variant));
  if (!event) {
    NS_ERROR("Failed to create event!");
    return;
  }

  PRBool dummy;
  aTarget->DispatchEvent(event, &dummy);
}

void
AsyncConnectionHelper::OnError(nsIDOMEventTarget* aTarget,
                               PRUint16 aErrorCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // Make an error event and fire it at the target.
  nsCOMPtr<nsIDOMEvent> event(IDBErrorEvent::Create(aErrorCode));
  if (!event) {
    NS_ERROR("Failed to create event!");
    return;
  }

  PRBool dummy;
  aTarget->DispatchEvent(event, &dummy);
}

void
AsyncConnectionHelper::GetSuccessResult(nsIWritableVariant* /* aResult */)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // Leave the variant set to empty.
}

nsresult
AsyncConnectionHelper::EnsureConnection()
{
  NS_PRECONDITION(!NS_IsMainThread(),
                  "Opening a database on the main thread!");

  if (mConnection) {
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

  // Build the filename.
  nsCString filename;
  filename.AppendInt(HashString(mASCIIOrigin));
  filename.AppendLiteral(".sqlite");

  rv = dbFile->Append(NS_ConvertASCIItoUTF16(filename));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageService> ss =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);

  nsCOMPtr<mozIStorageConnection> conn;
  rv = ss->OpenDatabase(dbFile, getter_AddRefs(conn));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // Nuke the database file.  The web services can recreate their data.
    rv = dbFile->Remove(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    exists = PR_FALSE;

    rv = ss->OpenDatabase(dbFile, getter_AddRefs(conn));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Check to make sure that the database schema is correct.
  PRInt32 schemaVersion;
  rv = conn->GetSchemaVersion(&schemaVersion);
  NS_ENSURE_SUCCESS(rv, rv);

  if (schemaVersion != DB_SCHEMA_VERSION) {
    if (exists) {
      // If the connection is not at the right schema version, nuke it.
      rv = dbFile->Remove(PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = ss->OpenDatabase(dbFile, getter_AddRefs(conn));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = CreateTables(conn);
    NS_ENSURE_SUCCESS(rv, rv);
  }

#ifdef DEBUG
  // Check to make sure that the database schema is correct again.
  NS_ASSERTION(NS_SUCCEEDED(conn->GetSchemaVersion(&schemaVersion)) &&
               schemaVersion == DB_SCHEMA_VERSION,
               "CreateTables failed!");

  // Turn on foreign key constraints in debug builds to catch bugs!
  (void)conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "PRAGMA foreign_keys = ON;"
  ));
#endif

  conn.swap(mConnection);
  return NS_OK;
}

