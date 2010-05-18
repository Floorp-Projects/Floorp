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

#ifndef mozilla_dom_indexeddb_idbdatabaserequest_h__
#define mozilla_dom_indexeddb_idbdatabaserequest_h__

#include "mozilla/dom/indexedDB/IDBRequest.h"
#include "mozilla/dom/indexedDB/LazyIdleThread.h"

#include "mozIStorageConnection.h"
#include "nsIIDBDatabaseRequest.h"
#include "nsIObserver.h"

#include "nsDOMLists.h"

BEGIN_INDEXEDDB_NAMESPACE

class AsyncConnectionHelper;
class IDBObjectStoreRequest;

class IDBDatabaseRequest : public IDBRequest::Generator,
                           public nsIIDBDatabaseRequest,
                           public nsIObserver
{
  friend class AsyncConnectionHelper;
  friend class IDBObjectStoreRequest;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIDBDATABASE
  NS_DECL_NSIIDBDATABASEREQUEST
  NS_DECL_NSIOBSERVER

  static already_AddRefed<IDBDatabaseRequest>
  Create(const nsAString& aName,
         const nsAString& aDescription,
         nsTArray<nsString>& aObjectStoreNames,
         const nsAString& aVersion,
         LazyIdleThread* aThread,
         const nsAString& aDatabaseFilePath,
         nsCOMPtr<mozIStorageConnection>& aConnection);

  /**
   * Obtains a cached statement for the put operation on object stores.
   *
   * @pre Called from mStorageThread.
   *
   * @param aOverwrite
   *        Indicating if the operation should overwrite an existing entry or
   *        not.
   * @param aAutoIncrement
   *        Indicating if the operation should use our key generator or not.
   * @returns a mozIStorageStatement to use for the put operation.
   */
  already_AddRefed<mozIStorageStatement> PutStatement(bool aOverwrite,
                                                      bool aAutoIncrement);

  /**
   * Obtains a cached statement for the remove operation on object stores.
   *
   * @pre Called from mStorageThread.
   *
   * @param aAutoIncrement
   *        Indicating if an auto increment table is used for the object store
   *        or not.
   * @returns a mozIStorageStatement to use for the remove operation.
   */
  already_AddRefed<mozIStorageStatement> RemoveStatement(bool aAutoIncrement);

  /**
   * Obtains a cached statement for the get operation on object stores.
   *
   * @pre Called from mStorageThread.
   *
   * @param aAutoIncrement
   *        Indicating if an auto increment table is used for the object store
   *        or not.
   * @returns a mozIStorageStatement to use for the get operation.
   */
  already_AddRefed<mozIStorageStatement> GetStatement(bool aAutoIncrement);

  nsIThread* ConnectionThread() {
    return mConnectionThread;
  }

  void FireCloseConnectionRunnable();

  void OnVersionSet(const nsString& aVersion);
  void OnObjectStoreCreated(const nsAString& aName);
  void OnIndexCreated(const nsAString& aName);
  void OnObjectStoreRemoved(const nsAString& aName);

protected:
  IDBDatabaseRequest();
  ~IDBDatabaseRequest();

  // Only meant to be called on mStorageThread!
  nsCOMPtr<mozIStorageConnection>& Connection();

  // Only meant to be called on mStorageThread!
  nsresult EnsureConnection();

private:
  nsString mName;
  nsString mDescription;
  nsString mVersion;
  nsString mDatabaseFilePath;

  nsTArray<nsString> mObjectStoreNames;

  nsRefPtr<LazyIdleThread> mConnectionThread;

  // Only touched on mStorageThread! These must be destroyed in the
  // FireCloseConnectionRunnable method.
  nsCOMPtr<mozIStorageConnection> mConnection;
  nsCOMPtr<mozIStorageStatement> mPutStmt;
  nsCOMPtr<mozIStorageStatement> mPutAutoIncrementStmt;
  nsCOMPtr<mozIStorageStatement> mPutOverwriteStmt;
  nsCOMPtr<mozIStorageStatement> mPutOverwriteAutoIncrementStmt;
  nsCOMPtr<mozIStorageStatement> mRemoveStmt;
  nsCOMPtr<mozIStorageStatement> mRemoveAutoIncrementStmt;
  nsCOMPtr<mozIStorageStatement> mGetStmt;
  nsCOMPtr<mozIStorageStatement> mGetAutoIncrementStmt;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbdatabaserequest_h__
