/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_idbtransaction_h__
#define mozilla_dom_idbtransaction_h__

#include "mozilla/Attributes.h"
#include "mozilla/dom/IDBTransactionBinding.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIRunnable.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {

class ErrorResult;
class EventChainPreVisitor;

namespace dom {

class DOMException;
class DOMStringList;
class IDBCursor;
class IDBDatabase;
class IDBObjectStore;
class IDBOpenDBRequest;
class IDBRequest;
class StrongWorkerRef;

namespace indexedDB {
class BackgroundCursorChild;
class BackgroundRequestChild;
class BackgroundTransactionChild;
class BackgroundVersionChangeTransactionChild;
class IndexMetadata;
class ObjectStoreSpec;
class OpenCursorParams;
class RequestParams;
}  // namespace indexedDB

class IDBTransaction final : public DOMEventTargetHelper, public nsIRunnable {
  friend class indexedDB::BackgroundCursorChild;
  friend class indexedDB::BackgroundRequestChild;

 public:
  enum Mode {
    READ_ONLY = 0,
    READ_WRITE,
    READ_WRITE_FLUSH,
    CLEANUP,
    VERSION_CHANGE,

    // Only needed for IPC serialization helper, should never be used in code.
    MODE_INVALID
  };

  enum ReadyState { INITIAL = 0, LOADING, INACTIVE, COMMITTING, DONE };

 private:
  // TODO: Only non-const because of Bug 1575173.
  RefPtr<IDBDatabase> mDatabase;
  RefPtr<DOMException> mError;
  const nsTArray<nsString> mObjectStoreNames;
  nsTArray<RefPtr<IDBObjectStore>> mObjectStores;
  nsTArray<RefPtr<IDBObjectStore>> mDeletedObjectStores;
  RefPtr<StrongWorkerRef> mWorkerRef;
  nsTArray<IDBCursor*> mCursors;

  // Tagged with mMode. If mMode is VERSION_CHANGE then mBackgroundActor will be
  // a BackgroundVersionChangeTransactionChild. Otherwise it will be a
  // BackgroundTransactionChild.
  union {
    indexedDB::BackgroundTransactionChild* mNormalBackgroundActor;
    indexedDB::BackgroundVersionChangeTransactionChild*
        mVersionChangeBackgroundActor;
  } mBackgroundActor;

  const int64_t mLoggingSerialNumber;

  // Only used for VERSION_CHANGE transactions.
  int64_t mNextObjectStoreId;
  int64_t mNextIndexId;

  nsresult mAbortCode;  ///< The result that caused the transaction to be
                        ///< aborted, or NS_OK if not aborted.
                        ///< NS_ERROR_DOM_INDEXEDDB_ABORT_ERR indicates that the
                        ///< user explicitly requested aborting. Should be
                        ///< renamed to mResult or so, because it is actually
                        ///< used to check if the transaction has been aborted.
  uint32_t mPendingRequestCount;  ///< Counted via OnNewRequest and
                                  ///< OnRequestFinished, so that the
                                  ///< transaction can auto-commit when the last
                                  ///< pending request finished.

  const nsString mFilename;
  const uint32_t mLineNo;
  const uint32_t mColumn;

  ReadyState mReadyState;
  const Mode mMode;

  bool mCreating;    ///< Set between successful creation until the transaction
                     ///< has run on the event-loop.
  bool mRegistered;  ///< Whether mDatabase->RegisterTransaction() has been
                     ///< called (which may not be the case if construction was
                     ///< incomplete).
  bool mAbortedByScript;
  bool mNotedActiveTransaction;

#ifdef DEBUG
  bool mSentCommitOrAbort;
  bool mFiredCompleteOrAbort;
#endif

 public:
  static already_AddRefed<IDBTransaction> CreateVersionChange(
      IDBDatabase* aDatabase,
      indexedDB::BackgroundVersionChangeTransactionChild* aActor,
      IDBOpenDBRequest* aOpenRequest, int64_t aNextObjectStoreId,
      int64_t aNextIndexId);

  static already_AddRefed<IDBTransaction> Create(
      JSContext* aCx, IDBDatabase* aDatabase,
      const nsTArray<nsString>& aObjectStoreNames, Mode aMode);

  static IDBTransaction* GetCurrent();

  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  void SetBackgroundActor(
      indexedDB::BackgroundTransactionChild* aBackgroundActor);

  void ClearBackgroundActor() {
    AssertIsOnOwningThread();

    if (mMode == VERSION_CHANGE) {
      mBackgroundActor.mVersionChangeBackgroundActor = nullptr;
    } else {
      mBackgroundActor.mNormalBackgroundActor = nullptr;
    }

    // Note inactive transaction here if we didn't receive the Complete message
    // from the parent.
    MaybeNoteInactiveTransaction();
  }

  indexedDB::BackgroundRequestChild* StartRequest(
      IDBRequest* aRequest, const indexedDB::RequestParams& aParams);

  void OpenCursor(indexedDB::BackgroundCursorChild* aBackgroundActor,
                  const indexedDB::OpenCursorParams& aParams);

  void RefreshSpec(bool aMayDelete);

  bool IsOpen() const;

  bool IsCommittingOrDone() const {
    AssertIsOnOwningThread();

    return mReadyState == COMMITTING || mReadyState == DONE;
  }

  bool IsDone() const {
    AssertIsOnOwningThread();

    return mReadyState == DONE;
  }

  bool IsWriteAllowed() const {
    AssertIsOnOwningThread();
    return mMode == READ_WRITE || mMode == READ_WRITE_FLUSH ||
           mMode == CLEANUP || mMode == VERSION_CHANGE;
  }

  bool IsAborted() const {
    AssertIsOnOwningThread();
    return NS_FAILED(mAbortCode);
  }

  auto TemporarilyProceedToInactive() {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mReadyState == INITIAL || mReadyState == LOADING);
    const auto savedReadyState = mReadyState;
    mReadyState = INACTIVE;

    struct AutoRestoreState {
      IDBTransaction& mOwner;
      ReadyState mSavedReadyState;
#ifdef DEBUG
      uint32_t mSavedPendingRequestCount;
#endif

      ~AutoRestoreState() {
        mOwner.AssertIsOnOwningThread();
        MOZ_ASSERT(mOwner.mReadyState == INACTIVE);
        MOZ_ASSERT(mOwner.mPendingRequestCount == mSavedPendingRequestCount);

        mOwner.mReadyState = mSavedReadyState;
      }
    };

    return AutoRestoreState{*this, savedReadyState
#ifdef DEBUG
                            ,
                            mPendingRequestCount
#endif
    };
  }

  nsresult AbortCode() const {
    AssertIsOnOwningThread();
    return mAbortCode;
  }

  void GetCallerLocation(nsAString& aFilename, uint32_t* aLineNo,
                         uint32_t* aColumn) const;

  // 'Get' prefix is to avoid name collisions with the enum
  Mode GetMode() const {
    AssertIsOnOwningThread();
    return mMode;
  }

  IDBDatabase* Database() const {
    AssertIsOnOwningThread();
    return mDatabase;
  }

  // Only for use by ProfilerHelpers.h
  const nsTArray<nsString>& ObjectStoreNamesInternal() const {
    AssertIsOnOwningThread();
    return mObjectStoreNames;
  }

  already_AddRefed<IDBObjectStore> CreateObjectStore(
      const indexedDB::ObjectStoreSpec& aSpec);

  void DeleteObjectStore(int64_t aObjectStoreId);

  void RenameObjectStore(int64_t aObjectStoreId, const nsAString& aName);

  void CreateIndex(IDBObjectStore* aObjectStore,
                   const indexedDB::IndexMetadata& aMetadata);

  void DeleteIndex(IDBObjectStore* aObjectStore, int64_t aIndexId);

  void RenameIndex(IDBObjectStore* aObjectStore, int64_t aIndexId,
                   const nsAString& aName);

  void Abort(IDBRequest* aRequest);

  void Abort(nsresult aErrorCode);

  int64_t LoggingSerialNumber() const {
    AssertIsOnOwningThread();

    return mLoggingSerialNumber;
  }

  nsIGlobalObject* GetParentObject() const;

  void FireCompleteOrAbortEvents(nsresult aResult);

  // Only for VERSION_CHANGE transactions.
  int64_t NextObjectStoreId();

  // Only for VERSION_CHANGE transactions.
  int64_t NextIndexId();

  void InvalidateCursorCaches();
  void RegisterCursor(IDBCursor* aCursor);
  void UnregisterCursor(IDBCursor* aCursor);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBTransaction, DOMEventTargetHelper)

  // nsWrapperCache
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // Methods bound via WebIDL.
  IDBDatabase* Db() const { return Database(); }

  IDBTransactionMode GetMode(ErrorResult& aRv) const;

  DOMException* GetError() const;

  already_AddRefed<IDBObjectStore> ObjectStore(const nsAString& aName,
                                               ErrorResult& aRv);

  void Abort(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(abort)
  IMPL_EVENT_HANDLER(complete)
  IMPL_EVENT_HANDLER(error)

  already_AddRefed<DOMStringList> ObjectStoreNames() const;

  // EventTarget
  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;

 private:
  IDBTransaction(IDBDatabase* aDatabase,
                 const nsTArray<nsString>& aObjectStoreNames, Mode aMode,
                 nsString aFilename, uint32_t aLineNo, uint32_t aColumn);
  ~IDBTransaction();

  void AbortInternal(nsresult aAbortCode,
                     already_AddRefed<DOMException> aError);

  void SendCommit();

  void SendAbort(nsresult aResultCode);

  void NoteActiveTransaction();

  void MaybeNoteInactiveTransaction();

  void OnNewRequest();

  void OnRequestFinished(bool aRequestCompletedSuccessfully);

  template <typename Func>
  auto DoWithTransactionChild(const Func& aFunc) const;

  bool HasTransactionChild() const;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_idbtransaction_h__
