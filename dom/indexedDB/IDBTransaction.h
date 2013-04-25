/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbtransaction_h__
#define mozilla_dom_indexeddb_idbtransaction_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "mozIStorageFunction.h"
#include "nsIIDBTransaction.h"
#include "nsIDOMDOMError.h"
#include "nsIRunnable.h"

#include "nsAutoPtr.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsInterfaceHashtable.h"
#include "nsRefPtrHashtable.h"

#include "mozilla/dom/indexedDB/IDBDatabase.h"
#include "mozilla/dom/indexedDB/IDBWrapperCache.h"
#include "mozilla/dom/indexedDB/FileInfo.h"

class nsIThread;

BEGIN_INDEXEDDB_NAMESPACE

class AsyncConnectionHelper;
class CommitHelper;
class IDBRequest;
class IndexedDBDatabaseChild;
class IndexedDBTransactionChild;
class IndexedDBTransactionParent;
struct ObjectStoreInfo;
class TransactionThreadPool;
class UpdateRefcountFunction;

class IDBTransactionListener
{
public:
  NS_IMETHOD_(nsrefcnt) AddRef() = 0;
  NS_IMETHOD_(nsrefcnt) Release() = 0;

  // Called just before dispatching the final events on the transaction.
  virtual nsresult NotifyTransactionPreComplete(IDBTransaction* aTransaction) = 0;
  // Called just after dispatching the final events on the transaction.
  virtual nsresult NotifyTransactionPostComplete(IDBTransaction* aTransaction) = 0;
};

class IDBTransaction : public IDBWrapperCache,
                       public nsIIDBTransaction,
                       public nsIRunnable
{
  friend class AsyncConnectionHelper;
  friend class CommitHelper;
  friend class IndexedDBDatabaseChild;
  friend class ThreadObserver;
  friend class TransactionThreadPool;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIIDBTRANSACTION
  NS_DECL_NSIRUNNABLE

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBTransaction, IDBWrapperCache)

  enum Mode
  {
    READ_ONLY = 0,
    READ_WRITE,
    VERSION_CHANGE,

    // Only needed for IPC serialization helper, should never be used in code.
    MODE_INVALID
  };

  enum ReadyState
  {
    INITIAL = 0,
    LOADING,
    COMMITTING,
    DONE
  };

  static already_AddRefed<IDBTransaction>
  Create(IDBDatabase* aDatabase,
         nsTArray<nsString>& aObjectStoreNames,
         Mode aMode,
         bool aDispatchDelayed)
  {
    return CreateInternal(aDatabase, aObjectStoreNames, aMode, aDispatchDelayed,
                          false);
  }

  // nsIDOMEventTarget
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);

  void OnNewRequest();
  void OnRequestFinished();
  void OnRequestDisconnected();

  void RemoveObjectStore(const nsAString& aName);

  void SetTransactionListener(IDBTransactionListener* aListener);

  bool StartSavepoint();
  nsresult ReleaseSavepoint();
  void RollbackSavepoint();

  // Only meant to be called on mStorageThread!
  nsresult GetOrCreateConnection(mozIStorageConnection** aConnection);

  already_AddRefed<mozIStorageStatement>
  GetCachedStatement(const nsACString& aQuery);

  template<int N>
  already_AddRefed<mozIStorageStatement>
  GetCachedStatement(const char (&aQuery)[N])
  {
    return GetCachedStatement(NS_LITERAL_CSTRING(aQuery));
  }

  bool IsOpen() const;

  bool IsFinished() const
  {
    return mReadyState > LOADING;
  }

  bool IsWriteAllowed() const
  {
    return mMode == READ_WRITE || mMode == VERSION_CHANGE;
  }

  bool IsAborted() const
  {
    return NS_FAILED(mAbortCode);
  }

  // 'Get' prefix is to avoid name collisions with the enum
  Mode GetMode()
  {
    return mMode;
  }

  IDBDatabase* Database()
  {
    NS_ASSERTION(mDatabase, "This should never be null!");
    return mDatabase;
  }

  DatabaseInfo* DBInfo() const
  {
    return mDatabaseInfo;
  }

  already_AddRefed<IDBObjectStore>
  GetOrCreateObjectStore(const nsAString& aName,
                         ObjectStoreInfo* aObjectStoreInfo,
                         bool aCreating);

  already_AddRefed<FileInfo> GetFileInfo(nsIDOMBlob* aBlob);
  void AddFileInfo(nsIDOMBlob* aBlob, FileInfo* aFileInfo);

  void ClearCreatedFileInfos();

  void
  SetActor(IndexedDBTransactionChild* aActorChild)
  {
    NS_ASSERTION(!aActorChild || !mActorChild, "Shouldn't have more than one!");
    mActorChild = aActorChild;
  }

  void
  SetActor(IndexedDBTransactionParent* aActorParent)
  {
    NS_ASSERTION(!aActorParent || !mActorParent,
                 "Shouldn't have more than one!");
    mActorParent = aActorParent;
  }

  IndexedDBTransactionChild*
  GetActorChild() const
  {
    return mActorChild;
  }

  IndexedDBTransactionParent*
  GetActorParent() const
  {
    return mActorParent;
  }

  nsresult
  ObjectStoreInternal(const nsAString& aName,
                      IDBObjectStore** _retval);

  nsresult
  Abort(IDBRequest* aRequest);

  nsresult
  Abort(nsresult aAbortCode);

  nsresult
  GetAbortCode() const
  {
    return mAbortCode;
  }

private:
  nsresult
  AbortInternal(nsresult aAbortCode, already_AddRefed<nsIDOMDOMError> aError);

  // Should only be called directly through IndexedDBDatabaseChild.
  static already_AddRefed<IDBTransaction>
  CreateInternal(IDBDatabase* aDatabase,
                 nsTArray<nsString>& aObjectStoreNames,
                 Mode aMode,
                 bool aDispatchDelayed,
                 bool aIsVersionChangeTransactionChild);

  IDBTransaction();
  ~IDBTransaction();

  nsresult CommitOrRollback();

  nsRefPtr<IDBDatabase> mDatabase;
  nsRefPtr<DatabaseInfo> mDatabaseInfo;
  nsCOMPtr<nsIDOMDOMError> mError;
  nsTArray<nsString> mObjectStoreNames;
  ReadyState mReadyState;
  Mode mMode;
  uint32_t mPendingRequests;

  nsInterfaceHashtable<nsCStringHashKey, mozIStorageStatement>
    mCachedStatements;

  nsRefPtr<IDBTransactionListener> mListener;

  // Only touched on the database thread.
  nsCOMPtr<mozIStorageConnection> mConnection;

  // Only touched on the database thread.
  uint32_t mSavepointCount;

  nsTArray<nsRefPtr<IDBObjectStore> > mCreatedObjectStores;
  nsTArray<nsRefPtr<IDBObjectStore> > mDeletedObjectStores;

  nsRefPtr<UpdateRefcountFunction> mUpdateFileRefcountFunction;
  nsRefPtrHashtable<nsISupportsHashKey, FileInfo> mCreatedFileInfos;

  IndexedDBTransactionChild* mActorChild;
  IndexedDBTransactionParent* mActorParent;

  nsresult mAbortCode;
  bool mCreating;

#ifdef DEBUG
  bool mFiredCompleteOrAbort;
#endif
};

class CommitHelper MOZ_FINAL : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  CommitHelper(IDBTransaction* aTransaction,
               IDBTransactionListener* aListener,
               const nsTArray<nsRefPtr<IDBObjectStore> >& mUpdatedObjectStores);
  CommitHelper(IDBTransaction* aTransaction,
               nsresult aAbortCode);
  ~CommitHelper();

  template<class T>
  bool AddDoomedObject(nsCOMPtr<T>& aCOMPtr)
  {
    if (aCOMPtr) {
      if (!mDoomedObjects.AppendElement(do_QueryInterface(aCOMPtr))) {
        NS_ERROR("Out of memory!");
        return false;
      }
      aCOMPtr = nullptr;
    }
    return true;
  }

private:
  // Writes new autoincrement counts to database
  nsresult WriteAutoIncrementCounts();

  // Updates counts after a successful commit
  void CommitAutoIncrementCounts();

  // Reverts counts when a transaction is aborted
  void RevertAutoIncrementCounts();

  nsRefPtr<IDBTransaction> mTransaction;
  nsRefPtr<IDBTransactionListener> mListener;
  nsCOMPtr<mozIStorageConnection> mConnection;
  nsRefPtr<UpdateRefcountFunction> mUpdateFileRefcountFunction;
  nsAutoTArray<nsCOMPtr<nsISupports>, 10> mDoomedObjects;
  nsAutoTArray<nsRefPtr<IDBObjectStore>, 10> mAutoIncrementObjectStores;

  nsresult mAbortCode;
};

class UpdateRefcountFunction MOZ_FINAL : public mozIStorageFunction
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  UpdateRefcountFunction(FileManager* aFileManager)
  : mFileManager(aFileManager)
  { }

  ~UpdateRefcountFunction()
  { }

  nsresult Init();

  void ClearFileInfoEntries()
  {
    mFileInfoEntries.Clear();
  }

  nsresult WillCommit(mozIStorageConnection* aConnection);
  void DidCommit();
  void DidAbort();

private:
  class FileInfoEntry
  {
  public:
    FileInfoEntry(FileInfo* aFileInfo)
    : mFileInfo(aFileInfo), mDelta(0)
    { }

    ~FileInfoEntry()
    { }

    nsRefPtr<FileInfo> mFileInfo;
    int32_t mDelta;
  };

  enum UpdateType {
    eIncrement,
    eDecrement
  };

  class DatabaseUpdateFunction
  {
  public:
    DatabaseUpdateFunction(mozIStorageConnection* aConnection,
                           UpdateRefcountFunction* aFunction)
    : mConnection(aConnection), mFunction(aFunction), mErrorCode(NS_OK)
    { }

    bool Update(int64_t aId, int32_t aDelta);
    nsresult ErrorCode()
    {
      return mErrorCode;
    }

  private:
    nsresult UpdateInternal(int64_t aId, int32_t aDelta);

    nsCOMPtr<mozIStorageConnection> mConnection;
    nsCOMPtr<mozIStorageStatement> mUpdateStatement;
    nsCOMPtr<mozIStorageStatement> mSelectStatement;
    nsCOMPtr<mozIStorageStatement> mInsertStatement;

    UpdateRefcountFunction* mFunction;

    nsresult mErrorCode;
  };

  nsresult ProcessValue(mozIStorageValueArray* aValues,
                        int32_t aIndex,
                        UpdateType aUpdateType);

  nsresult CreateJournals();

  nsresult RemoveJournals(const nsTArray<int64_t>& aJournals);

  static PLDHashOperator
  DatabaseUpdateCallback(const uint64_t& aKey,
                         FileInfoEntry* aValue,
                         void* aUserArg);

  static PLDHashOperator
  FileInfoUpdateCallback(const uint64_t& aKey,
                         FileInfoEntry* aValue,
                         void* aUserArg);

  FileManager* mFileManager;
  nsClassHashtable<nsUint64HashKey, FileInfoEntry> mFileInfoEntries;

  nsTArray<int64_t> mJournalsToCreateBeforeCommit;
  nsTArray<int64_t> mJournalsToRemoveAfterCommit;
  nsTArray<int64_t> mJournalsToRemoveAfterAbort;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbtransaction_h__
