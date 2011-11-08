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
 *   Kyle Huey <me@kylehuey.com>
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

#ifndef mozilla_dom_indexeddb_opendatabasehelper_h__
#define mozilla_dom_indexeddb_opendatabasehelper_h__

#include "AsyncConnectionHelper.h"
#include "DatabaseInfo.h"
#include "IDBDatabase.h"
#include "IDBRequest.h"

#include "nsIRunnable.h"

class mozIStorageConnection;

BEGIN_INDEXEDDB_NAMESPACE

class OpenDatabaseHelper : public HelperBase
{
public:
  OpenDatabaseHelper(IDBOpenDBRequest* aRequest,
                     const nsAString& aName,
                     const nsACString& aASCIIOrigin,
                     PRUint64 aRequestedVersion,
                     bool aForDeletion)
    : HelperBase(aRequest), mOpenDBRequest(aRequest), mName(aName),
      mASCIIOrigin(aASCIIOrigin), mRequestedVersion(aRequestedVersion),
      mForDeletion(aForDeletion), mCurrentVersion(0),
      mDataVersion(DB_SCHEMA_VERSION), mDatabaseId(0), mLastObjectStoreId(0),
      mLastIndexId(0), mState(eCreated), mResultCode(NS_OK)
  {
    NS_ASSERTION(!aForDeletion || !aRequestedVersion,
                 "Can't be for deletion and request a version!");
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  nsresult Init();

  nsresult Dispatch(nsIEventTarget* aDatabaseThread);
  nsresult RunImmediately();

  void SetError(nsresult rv)
  {
    NS_ASSERTION(NS_FAILED(rv), "Why are you telling me?");
    mResultCode = rv;
  }

  nsresult GetResultCode()
  {
    return mResultCode;
  }

  nsresult NotifySetVersionFinished();
  nsresult NotifyDeleteFinished();
  void BlockDatabase();

  nsIAtom* Id() const
  {
    return mDatabaseId.get();
  }

  IDBDatabase* Database() const
  {
    NS_ASSERTION(mDatabase, "Calling at the wrong time!");
    return mDatabase;
  }

protected:
  // Methods only called on the main thread
  nsresult EnsureSuccessResult();
  nsresult StartSetVersion();
  nsresult StartDelete();
  nsresult GetSuccessResult(JSContext* aCx,
                          jsval* aVal);
  void DispatchSuccessEvent();
  void DispatchErrorEvent();
  void ReleaseMainThreadObjects();

  // Methods only called on the DB thread
  nsresult DoDatabaseWork();

private:
  // In-params.
  nsRefPtr<IDBOpenDBRequest> mOpenDBRequest;
  nsString mName;
  nsCString mASCIIOrigin;
  PRUint64 mRequestedVersion;
  bool mForDeletion;
  nsCOMPtr<nsIAtom> mDatabaseId;

  // Out-params.
  nsTArray<nsAutoPtr<ObjectStoreInfo> > mObjectStores;
  PRUint64 mCurrentVersion;
  PRUint32 mDataVersion;
  nsString mDatabaseFilePath;
  PRInt64 mLastObjectStoreId;
  PRInt64 mLastIndexId;
  nsRefPtr<IDBDatabase> mDatabase;

  // State variables
  enum OpenDatabaseState {
    eCreated = 0, // Not yet dispatched to the DB thread
    eDBWork, // Waiting to do/doing work on the DB thread
    eFiringEvents, // Waiting to fire/firing events on the main thread
    eSetVersionPending, // Waiting on a SetVersionHelper
    eSetVersionCompleted, // SetVersionHelper is done
    eDeletePending, // Waiting on a DeleteDatabaseHelper
    eDeleteCompleted, // DeleteDatabaseHelper is done
  };
  OpenDatabaseState mState;
  nsresult mResultCode;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_opendatabasehelper_h__
