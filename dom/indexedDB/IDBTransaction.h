/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbtransaction_h__
#define mozilla_dom_indexeddb_idbtransaction_h__

#include "mozilla/Attributes.h"
#include "mozilla/dom/IDBTransactionBinding.h"
#include "mozilla/dom/indexedDB/IDBWrapperCache.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIRunnable.h"
#include "nsString.h"
#include "nsTArray.h"

class nsPIDOMWindow;

namespace mozilla {

class ErrorResult;
class EventChainPreVisitor;

namespace dom {

class DOMError;
class DOMStringList;
class PBlobChild;

namespace indexedDB {

class BackgroundCursorChild;
class BackgroundRequestChild;
class BackgroundTransactionChild;
class BackgroundVersionChangeTransactionChild;
class IDBDatabase;
class IDBObjectStore;
class IDBRequest;
class IndexMetadata;
class ObjectStoreSpec;
class OpenCursorParams;
class PBackgroundIDBDatabaseFileChild;
class RequestParams;

class IDBTransaction MOZ_FINAL
  : public IDBWrapperCache
  , public nsIRunnable
{
public:
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

private:
  nsRefPtr<IDBDatabase> mDatabase;
  nsRefPtr<DOMError> mError;
  nsTArray<nsString> mObjectStoreNames;
  nsTArray<nsRefPtr<IDBObjectStore>> mObjectStores;
  nsTArray<nsRefPtr<IDBObjectStore>> mDeletedObjectStores;

  // Tagged with mMode. If mMode is VERSION_CHANGE then mBackgroundActor will be
  // a BackgroundVersionChangeTransactionChild. Otherwise it will be a
  // BackgroundTransactionChild.
  union {
    BackgroundTransactionChild* mNormalBackgroundActor;
    BackgroundVersionChangeTransactionChild* mVersionChangeBackgroundActor;
  } mBackgroundActor;


  // Only used for VERSION_CHANGE transactions.
  int64_t mNextObjectStoreId;
  int64_t mNextIndexId;

#ifdef MOZ_ENABLE_PROFILER_SPS
  uint64_t mSerialNumber;
#endif

  nsresult mAbortCode;
  uint32_t mPendingRequestCount;

  ReadyState mReadyState;
  Mode mMode;

  bool mCreating;
  bool mAbortedByScript;

#ifdef DEBUG
  bool mSentCommitOrAbort;
  bool mFiredCompleteOrAbort;
#endif

public:
  static already_AddRefed<IDBTransaction>
  CreateVersionChange(IDBDatabase* aDatabase,
                      BackgroundVersionChangeTransactionChild* aActor,
                      int64_t aNextObjectStoreId,
                      int64_t aNextIndexId);

  static already_AddRefed<IDBTransaction>
  Create(IDBDatabase* aDatabase,
         const nsTArray<nsString>& aObjectStoreNames,
         Mode aMode);

  static IDBTransaction*
  GetCurrent();

  void
  AssertIsOnOwningThread() const
#ifdef DEBUG
  ;
#else
  { }
#endif

  void
  SetBackgroundActor(BackgroundTransactionChild* aBackgroundActor);

  void
  ClearBackgroundActor()
  {
    AssertIsOnOwningThread();

    if (mMode == VERSION_CHANGE) {
      mBackgroundActor.mVersionChangeBackgroundActor = nullptr;
    } else {
      mBackgroundActor.mNormalBackgroundActor = nullptr;
    }
  }

  void
  StartRequest(BackgroundRequestChild* aBackgroundActor,
               const RequestParams& aParams);

  void
  OpenCursor(BackgroundCursorChild* aBackgroundActor,
             const OpenCursorParams& aParams);

  void
  RefreshSpec(bool aMayDelete);

  void
  OnNewRequest();

  void
  OnRequestFinished();

  bool
  IsOpen() const;

  bool
  IsFinished() const
  {
    AssertIsOnOwningThread();
    return mReadyState > LOADING;
  }

  bool
  IsWriteAllowed() const
  {
    AssertIsOnOwningThread();
    return mMode == READ_WRITE || mMode == VERSION_CHANGE;
  }

  bool
  IsAborted() const
  {
    AssertIsOnOwningThread();
    return NS_FAILED(mAbortCode);
  }
  
  nsresult
  AbortCode() const
  {
    AssertIsOnOwningThread();
    return mAbortCode;
  }

  // 'Get' prefix is to avoid name collisions with the enum
  Mode
  GetMode() const
  {
    AssertIsOnOwningThread();
    return mMode;
  }

  IDBDatabase*
  Database() const
  {
    AssertIsOnOwningThread();
    return mDatabase;
  }

  IDBDatabase*
  Db() const
  {
    return Database();
  }

  const nsTArray<nsString>&
  ObjectStoreNamesInternal() const
  {
    AssertIsOnOwningThread();
    return mObjectStoreNames;
  }

  already_AddRefed<IDBObjectStore>
  CreateObjectStore(const ObjectStoreSpec& aSpec);

  void
  DeleteObjectStore(int64_t aObjectStoreId);

  void
  CreateIndex(IDBObjectStore* aObjectStore, const IndexMetadata& aMetadata);

  void
  DeleteIndex(IDBObjectStore* aObjectStore, int64_t aIndexId);

  void
  Abort(IDBRequest* aRequest);

  void
  Abort(nsresult aAbortCode);

#ifdef MOZ_ENABLE_PROFILER_SPS
  uint32_t
  GetSerialNumber() const
  {
    AssertIsOnOwningThread();
    return mSerialNumber;
  }
#endif

  nsPIDOMWindow*
  GetParentObject() const;

  IDBTransactionMode
  GetMode(ErrorResult& aRv) const;

  DOMError*
  GetError() const;

  already_AddRefed<IDBObjectStore>
  ObjectStore(const nsAString& aName, ErrorResult& aRv);

  void
  Abort(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(abort)
  IMPL_EVENT_HANDLER(complete)
  IMPL_EVENT_HANDLER(error)

  already_AddRefed<DOMStringList>
  ObjectStoreNames();

  void
  FireCompleteOrAbortEvents(nsresult aResult);

  // Only for VERSION_CHANGE transactions.
  int64_t
  NextObjectStoreId();

  // Only for VERSION_CHANGE transactions.
  int64_t
  NextIndexId();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBTransaction, IDBWrapperCache)

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // nsIDOMEventTarget
  virtual nsresult
  PreHandleEvent(EventChainPreVisitor& aVisitor) MOZ_OVERRIDE;

private:
  IDBTransaction(IDBDatabase* aDatabase,
                 const nsTArray<nsString>& aObjectStoreNames,
                 Mode aMode);
  ~IDBTransaction();

  void
  AbortInternal(nsresult aAbortCode, already_AddRefed<DOMError> aError);

  void
  SendCommit();

  void
  SendAbort(nsresult aResultCode);
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_idbtransaction_h__
