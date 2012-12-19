/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_opendatabasehelper_h__
#define mozilla_dom_indexeddb_opendatabasehelper_h__

#include "AsyncConnectionHelper.h"
#include "DatabaseInfo.h"
#include "IDBDatabase.h"
#include "IDBRequest.h"

#include "nsIRunnable.h"

class mozIStorageConnection;

namespace mozilla {
namespace dom {
class ContentParent;
}
}

BEGIN_INDEXEDDB_NAMESPACE

class OpenDatabaseHelper : public HelperBase
{
public:
  OpenDatabaseHelper(IDBOpenDBRequest* aRequest,
                     const nsAString& aName,
                     const nsACString& aASCIIOrigin,
                     uint64_t aRequestedVersion,
                     bool aForDeletion,
                     mozilla::dom::ContentParent* aContentParent,
                     FactoryPrivilege aPrivilege)
    : HelperBase(aRequest), mOpenDBRequest(aRequest), mName(aName),
      mASCIIOrigin(aASCIIOrigin), mRequestedVersion(aRequestedVersion),
      mForDeletion(aForDeletion), mPrivilege(aPrivilege), mDatabaseId(nullptr),
      mContentParent(aContentParent), mCurrentVersion(0), mLastObjectStoreId(0),
      mLastIndexId(0), mState(eCreated), mResultCode(NS_OK),
      mLoadDBMetadata(false)
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

  virtual nsresult GetResultCode() MOZ_OVERRIDE
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

  const FactoryPrivilege& Privilege() const
  {
    return mPrivilege;
  }

  static
  nsresult CreateDatabaseConnection(nsIFile* aDBFile,
                                    nsIFile* aFMDirectory,
                                    const nsAString& aName,
                                    const nsACString& aOrigin,
                                    mozIStorageConnection** aConnection);

protected:
  // Methods only called on the main thread
  nsresult EnsureSuccessResult();
  nsresult StartSetVersion();
  nsresult StartDelete();
  virtual nsresult GetSuccessResult(JSContext* aCx,
                                    jsval* aVal) MOZ_OVERRIDE;
  void DispatchSuccessEvent();
  void DispatchErrorEvent();
  virtual void ReleaseMainThreadObjects() MOZ_OVERRIDE;

  // Methods only called on the DB thread
  nsresult DoDatabaseWork();

  // In-params.
  nsRefPtr<IDBOpenDBRequest> mOpenDBRequest;
  nsString mName;
  nsCString mASCIIOrigin;
  uint64_t mRequestedVersion;
  bool mForDeletion;
  FactoryPrivilege mPrivilege;
  nsCOMPtr<nsIAtom> mDatabaseId;
  mozilla::dom::ContentParent* mContentParent;

  // Out-params.
  nsTArray<nsRefPtr<ObjectStoreInfo> > mObjectStores;
  uint64_t mCurrentVersion;
  nsString mDatabaseFilePath;
  int64_t mLastObjectStoreId;
  int64_t mLastIndexId;
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

  nsRefPtr<FileManager> mFileManager;

  nsRefPtr<DatabaseInfo> mDBInfo;
  bool mLoadDBMetadata;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_opendatabasehelper_h__