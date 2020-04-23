/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_idbtransaction_h__
#define mozilla_dom_idbtransaction_h__

#include "FlippedOnce.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/IDBTransactionBinding.h"
#include "mozilla/dom/quota/CheckedUnsafePtr.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIRunnable.h"
#include "nsString.h"
#include "nsTArray.h"
#include "SafeRefPtr.h"

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
class PBackgroundIDBCursorChild;
class BackgroundRequestChild;
class BackgroundTransactionChild;
class BackgroundVersionChangeTransactionChild;
class IndexMetadata;
class ObjectStoreSpec;
class OpenCursorParams;
class RequestParams;
}  // namespace indexedDB

class IDBTransaction final
    : public DOMEventTargetHelper,
      public nsIRunnable,
      public SupportsCheckedUnsafePtr<CheckIf<DiagnosticAssertEnabled>> {
  friend class indexedDB::BackgroundRequestChild;

 public:
  enum struct Mode {
    ReadOnly = 0,
    ReadWrite,
    ReadWriteFlush,
    Cleanup,
    VersionChange,

    // Only needed for IPC serialization helper, should never be used in code.
    Invalid
  };

  enum struct ReadyState { Active, Inactive, Committing, Finished };

 private:
  // TODO: Only non-const because of Bug 1575173.
  RefPtr<IDBDatabase> mDatabase;
  RefPtr<DOMException> mError;
  const nsTArray<nsString> mObjectStoreNames;
  nsTArray<RefPtr<IDBObjectStore>> mObjectStores;
  nsTArray<RefPtr<IDBObjectStore>> mDeletedObjectStores;
  RefPtr<StrongWorkerRef> mWorkerRef;
  nsTArray<IDBCursor*> mCursors;

  // Tagged with mMode. If mMode is Mode::VersionChange then mBackgroundActor
  // will be a BackgroundVersionChangeTransactionChild. Otherwise it will be a
  // BackgroundTransactionChild.
  union {
    indexedDB::BackgroundTransactionChild* mNormalBackgroundActor;
    indexedDB::BackgroundVersionChangeTransactionChild*
        mVersionChangeBackgroundActor;
  } mBackgroundActor;

  const int64_t mLoggingSerialNumber;

  // Only used for Mode::VersionChange transactions.
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

  ReadyState mReadyState = ReadyState::Active;
  FlippedOnce<false> mStarted;
  const Mode mMode;

  bool mRegistered;  ///< Whether mDatabase->RegisterTransaction() has been
                     ///< called (which may not be the case if construction was
                     ///< incomplete).
  FlippedOnce<false> mAbortedByScript;
  bool mNotedActiveTransaction;
  FlippedOnce<false> mSentCommitOrAbort;

#ifdef DEBUG
  FlippedOnce<false> mFiredCompleteOrAbort;
  FlippedOnce<false> mWasExplicitlyCommitted;
#endif

 public:
  [[nodiscard]] static SafeRefPtr<IDBTransaction> CreateVersionChange(
      IDBDatabase* aDatabase,
      indexedDB::BackgroundVersionChangeTransactionChild* aActor,
      IDBOpenDBRequest* aOpenRequest, int64_t aNextObjectStoreId,
      int64_t aNextIndexId);

  [[nodiscard]] static SafeRefPtr<IDBTransaction> Create(
      JSContext* aCx, IDBDatabase* aDatabase,
      const nsTArray<nsString>& aObjectStoreNames, Mode aMode);

  static Maybe<IDBTransaction&> MaybeCurrent();

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

    if (mMode == Mode::VersionChange) {
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

  void OpenCursor(indexedDB::PBackgroundIDBCursorChild* aBackgroundActor,
                  const indexedDB::OpenCursorParams& aParams);

  void RefreshSpec(bool aMayDelete);

  bool IsCommittingOrFinished() const {
    AssertIsOnOwningThread();

    return mReadyState == ReadyState::Committing ||
           mReadyState == ReadyState::Finished;
  }

  bool IsActive() const {
    AssertIsOnOwningThread();

    return mReadyState == ReadyState::Active;
  }

  bool IsInactive() const {
    AssertIsOnOwningThread();

    return mReadyState == ReadyState::Inactive;
  }

  bool IsFinished() const {
    AssertIsOnOwningThread();

    return mReadyState == ReadyState::Finished;
  }

  bool IsWriteAllowed() const {
    AssertIsOnOwningThread();
    return mMode == Mode::ReadWrite || mMode == Mode::ReadWriteFlush ||
           mMode == Mode::Cleanup || mMode == Mode::VersionChange;
  }

  bool IsAborted() const {
    AssertIsOnOwningThread();
    return NS_FAILED(mAbortCode);
  }

#ifdef DEBUG
  bool WasExplicitlyCommitted() const { return mWasExplicitlyCommitted; }
#endif

  template <ReadyState OriginalState, ReadyState TemporaryState>
  class AutoRestoreState {
   public:
    explicit AutoRestoreState(IDBTransaction& aOwner) : mOwner { aOwner }
#ifdef DEBUG
    , mSavedPendingRequestCount { mOwner.mPendingRequestCount }
#endif
    {
      mOwner.AssertIsOnOwningThread();
      MOZ_ASSERT(mOwner.mReadyState == OriginalState);
      mOwner.mReadyState = TemporaryState;
    }

    ~AutoRestoreState() {
      mOwner.AssertIsOnOwningThread();
      MOZ_ASSERT(mOwner.mReadyState == TemporaryState);
      MOZ_ASSERT(mOwner.mPendingRequestCount == mSavedPendingRequestCount);

      mOwner.mReadyState = OriginalState;
    }

   private:
    IDBTransaction& mOwner;
#ifdef DEBUG
    const uint32_t mSavedPendingRequestCount;
#endif
  };

  AutoRestoreState<ReadyState::Inactive, ReadyState::Active>
  TemporarilyTransitionToActive();
  AutoRestoreState<ReadyState::Active, ReadyState::Inactive>
  TemporarilyTransitionToInactive();

  void TransitionToActive() {
    MOZ_ASSERT(mReadyState == ReadyState::Inactive);
    mReadyState = ReadyState::Active;
  }

  void TransitionToInactive() {
    MOZ_ASSERT(mReadyState == ReadyState::Active);
    mReadyState = ReadyState::Inactive;
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

  [[nodiscard]] RefPtr<IDBObjectStore> CreateObjectStore(
      indexedDB::ObjectStoreSpec& aSpec);

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

  // Only for Mode::VersionChange transactions.
  int64_t NextObjectStoreId();

  // Only for Mode::VersionChange transactions.
  int64_t NextIndexId();

  void InvalidateCursorCaches();
  void RegisterCursor(IDBCursor* aCursor);
  void UnregisterCursor(IDBCursor* aCursor);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBTransaction, DOMEventTargetHelper)

  void CommitIfNotStarted();

  // nsWrapperCache
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // Methods bound via WebIDL.
  IDBDatabase* Db() const { return Database(); }

  IDBTransactionMode GetMode(ErrorResult& aRv) const;

  DOMException* GetError() const;

  [[nodiscard]] RefPtr<IDBObjectStore> ObjectStore(const nsAString& aName,
                                                   ErrorResult& aRv);

  void Commit(ErrorResult& aRv);

  void Abort(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(abort)
  IMPL_EVENT_HANDLER(complete)
  IMPL_EVENT_HANDLER(error)

  [[nodiscard]] RefPtr<DOMStringList> ObjectStoreNames() const;

  // EventTarget
  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;

 private:
  struct CreatedFromFactoryFunction {};

 public:
  IDBTransaction(IDBDatabase* aDatabase,
                 const nsTArray<nsString>& aObjectStoreNames, Mode aMode,
                 nsString aFilename, uint32_t aLineNo, uint32_t aColumn,
                 CreatedFromFactoryFunction aDummy);

 private:
  ~IDBTransaction();

  void AbortInternal(nsresult aAbortCode, RefPtr<DOMException> aError);

  void SendCommit(bool aAutoCommit);

  void SendAbort(nsresult aResultCode);

  void NoteActiveTransaction();

  void MaybeNoteInactiveTransaction();

  // TODO consider making private again, or move to the right place
 public:
  void OnNewRequest();

  void OnRequestFinished(bool aRequestCompletedSuccessfully);

 private:
  template <typename Func>
  auto DoWithTransactionChild(const Func& aFunc) const;

  bool HasTransactionChild() const;
};

inline bool ReferenceEquals(const Maybe<IDBTransaction&>& aLHS,
                            const Maybe<IDBTransaction&>& aRHS) {
  if (aLHS.isNothing() != aRHS.isNothing()) {
    return false;
  }
  return aLHS.isNothing() || &aLHS.ref() == &aRHS.ref();
}

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_idbtransaction_h__
