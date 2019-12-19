/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_actorschild_h__
#define mozilla_dom_indexeddb_actorschild_h__

#include "IDBTransaction.h"
#include "js/RootingAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBCursorChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBDatabaseChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBDatabaseRequestChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBFactoryChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBFactoryRequestChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBRequestChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBSharedTypes.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBTransactionChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBVersionChangeTransactionChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIndexedDBUtilsChild.h"
#include "mozilla/dom/PBackgroundFileHandleChild.h"
#include "mozilla/dom/PBackgroundFileRequestChild.h"
#include "mozilla/dom/PBackgroundMutableFileChild.h"
#include "mozilla/UniquePtr.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

class nsIEventTarget;
struct nsID;

namespace mozilla {
namespace ipc {

class BackgroundChildImpl;

}  // namespace ipc

namespace dom {

class IDBCursor;
class IDBDatabase;
class IDBFactory;
class IDBFileHandle;
class IDBFileRequest;
class IDBMutableFile;
class IDBOpenDBRequest;
class IDBRequest;
class IndexedDatabaseManager;

namespace indexedDB {

class Key;
class PermissionRequestChild;
class PermissionRequestParent;
class SerializedStructuredCloneReadInfo;
struct CloneInfo;

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

DECLARE_USE_COPY_CONSTRUCTORS(mozilla::dom::indexedDB::CloneInfo)

namespace mozilla {
namespace dom {
namespace indexedDB {

class ThreadLocal {
  friend class nsAutoPtr<ThreadLocal>;
  friend IDBFactory;

  LoggingInfo mLoggingInfo;
  IDBTransaction* mCurrentTransaction;
  nsCString mLoggingIdString;

  NS_DECL_OWNINGTHREAD

 public:
  ThreadLocal() = delete;
  ThreadLocal(const ThreadLocal& aOther) = delete;

  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(ThreadLocal); }

  const LoggingInfo& GetLoggingInfo() const {
    AssertIsOnOwningThread();

    return mLoggingInfo;
  }

  const nsID& Id() const {
    AssertIsOnOwningThread();

    return mLoggingInfo.backgroundChildLoggingId();
  }

  const nsCString& IdString() const {
    AssertIsOnOwningThread();

    return mLoggingIdString;
  }

  int64_t NextTransactionSN(IDBTransaction::Mode aMode) {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mLoggingInfo.nextTransactionSerialNumber() < INT64_MAX);
    MOZ_ASSERT(mLoggingInfo.nextVersionChangeTransactionSerialNumber() >
               INT64_MIN);

    if (aMode == IDBTransaction::Mode::VersionChange) {
      return mLoggingInfo.nextVersionChangeTransactionSerialNumber()--;
    }

    return mLoggingInfo.nextTransactionSerialNumber()++;
  }

  uint64_t NextRequestSN() {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mLoggingInfo.nextRequestSerialNumber() < UINT64_MAX);

    return mLoggingInfo.nextRequestSerialNumber()++;
  }

  void SetCurrentTransaction(IDBTransaction* aCurrentTransaction) {
    AssertIsOnOwningThread();

    mCurrentTransaction = aCurrentTransaction;
  }

  IDBTransaction* GetCurrentTransaction() const {
    AssertIsOnOwningThread();

    return mCurrentTransaction;
  }

 private:
  explicit ThreadLocal(const nsID& aBackgroundChildLoggingId);
  ~ThreadLocal();
};

class BackgroundFactoryChild final : public PBackgroundIDBFactoryChild {
  friend class mozilla::ipc::BackgroundChildImpl;
  friend IDBFactory;

  IDBFactory* mFactory;

  NS_DECL_OWNINGTHREAD

 public:
  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(BackgroundFactoryChild);
  }

  IDBFactory* GetDOMObject() const {
    AssertIsOnOwningThread();
    return mFactory;
  }

  bool SendDeleteMe() = delete;

 private:
  // Only created by IDBFactory.
  explicit BackgroundFactoryChild(IDBFactory* aFactory);

  // Only destroyed by mozilla::ipc::BackgroundChildImpl.
  ~BackgroundFactoryChild();

  void SendDeleteMeInternal();

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  PBackgroundIDBFactoryRequestChild* AllocPBackgroundIDBFactoryRequestChild(
      const FactoryRequestParams& aParams) override;

  bool DeallocPBackgroundIDBFactoryRequestChild(
      PBackgroundIDBFactoryRequestChild* aActor) override;

  PBackgroundIDBDatabaseChild* AllocPBackgroundIDBDatabaseChild(
      const DatabaseSpec& aSpec,
      PBackgroundIDBFactoryRequestChild* aRequest) override;

  bool DeallocPBackgroundIDBDatabaseChild(
      PBackgroundIDBDatabaseChild* aActor) override;

  mozilla::ipc::IPCResult RecvPBackgroundIDBDatabaseConstructor(
      PBackgroundIDBDatabaseChild* aActor, const DatabaseSpec& aSpec,
      PBackgroundIDBFactoryRequestChild* aRequest) override;
};

class BackgroundDatabaseChild;

class BackgroundRequestChildBase {
 protected:
  RefPtr<IDBRequest> mRequest;

 public:
  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

 protected:
  explicit BackgroundRequestChildBase(IDBRequest* aRequest);

  virtual ~BackgroundRequestChildBase();
};

class BackgroundFactoryRequestChild final
    : public BackgroundRequestChildBase,
      public PBackgroundIDBFactoryRequestChild {
  typedef mozilla::dom::quota::PersistenceType PersistenceType;

  friend IDBFactory;
  friend class BackgroundFactoryChild;
  friend class BackgroundDatabaseChild;
  friend class PermissionRequestChild;
  friend class PermissionRequestParent;

  const RefPtr<IDBFactory> mFactory;

  // Normally when opening of a database is successful, we receive a database
  // actor in request response, so we can use it to call ReleaseDOMObject()
  // which clears temporary strong reference to IDBDatabase.
  // However, when there's an error, we don't receive a database actor and
  // IDBRequest::mTransaction is already cleared (must be). So the only way how
  // to call ReleaseDOMObject() is to have a back-reference to database actor.
  // This creates a weak ref cycle between
  // BackgroundFactoryRequestChild (using mDatabaseActor member) and
  // BackgroundDatabaseChild actor (using mOpenRequestActor member).
  // mDatabaseActor is set in EnsureDOMObject() and cleared in
  // ReleaseDOMObject().
  BackgroundDatabaseChild* mDatabaseActor;

  const uint64_t mRequestedVersion;
  const bool mIsDeleteOp;

 public:
  IDBOpenDBRequest* GetOpenDBRequest() const;

 private:
  // Only created by IDBFactory.
  BackgroundFactoryRequestChild(IDBFactory* aFactory,
                                IDBOpenDBRequest* aOpenRequest,
                                bool aIsDeleteOp, uint64_t aRequestedVersion);

  // Only destroyed by BackgroundFactoryChild.
  ~BackgroundFactoryRequestChild();

  void SetDatabaseActor(BackgroundDatabaseChild* aActor);

  bool HandleResponse(nsresult aResponse);

  bool HandleResponse(const OpenDatabaseRequestResponse& aResponse);

  bool HandleResponse(const DeleteDatabaseRequestResponse& aResponse);

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult Recv__delete__(
      const FactoryRequestResponse& aResponse) override;

  mozilla::ipc::IPCResult RecvPermissionChallenge(
      const PrincipalInfo& aPrincipalInfo) override;

  mozilla::ipc::IPCResult RecvBlocked(const uint64_t& aCurrentVersion) override;
};

class BackgroundDatabaseChild final : public PBackgroundIDBDatabaseChild {
  friend class BackgroundFactoryChild;
  friend class BackgroundFactoryRequestChild;
  friend IDBDatabase;

  UniquePtr<DatabaseSpec> mSpec;
  RefPtr<IDBDatabase> mTemporaryStrongDatabase;
  BackgroundFactoryRequestChild* mOpenRequestActor;
  IDBDatabase* mDatabase;

 public:
  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  const DatabaseSpec* Spec() const {
    AssertIsOnOwningThread();
    return mSpec.get();
  }

  IDBDatabase* GetDOMObject() const {
    AssertIsOnOwningThread();
    return mDatabase;
  }

  bool SendDeleteMe() = delete;

 private:
  // Only constructed by BackgroundFactoryChild.
  BackgroundDatabaseChild(const DatabaseSpec& aSpec,
                          BackgroundFactoryRequestChild* aOpenRequest);

  // Only destroyed by BackgroundFactoryChild.
  ~BackgroundDatabaseChild();

  void SendDeleteMeInternal();

  void EnsureDOMObject();

  void ReleaseDOMObject();

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  PBackgroundIDBDatabaseFileChild* AllocPBackgroundIDBDatabaseFileChild(
      const IPCBlob& aIPCBlob) override;

  bool DeallocPBackgroundIDBDatabaseFileChild(
      PBackgroundIDBDatabaseFileChild* aActor) override;

  PBackgroundIDBDatabaseRequestChild* AllocPBackgroundIDBDatabaseRequestChild(
      const DatabaseRequestParams& aParams) override;

  bool DeallocPBackgroundIDBDatabaseRequestChild(
      PBackgroundIDBDatabaseRequestChild* aActor) override;

  PBackgroundIDBTransactionChild* AllocPBackgroundIDBTransactionChild(
      const nsTArray<nsString>& aObjectStoreNames, const Mode& aMode) override;

  bool DeallocPBackgroundIDBTransactionChild(
      PBackgroundIDBTransactionChild* aActor) override;

  PBackgroundIDBVersionChangeTransactionChild*
  AllocPBackgroundIDBVersionChangeTransactionChild(
      const uint64_t& aCurrentVersion, const uint64_t& aRequestedVersion,
      const int64_t& aNextObjectStoreId, const int64_t& aNextIndexId) override;

  mozilla::ipc::IPCResult RecvPBackgroundIDBVersionChangeTransactionConstructor(
      PBackgroundIDBVersionChangeTransactionChild* aActor,
      const uint64_t& aCurrentVersion, const uint64_t& aRequestedVersion,
      const int64_t& aNextObjectStoreId, const int64_t& aNextIndexId) override;

  bool DeallocPBackgroundIDBVersionChangeTransactionChild(
      PBackgroundIDBVersionChangeTransactionChild* aActor) override;

  PBackgroundMutableFileChild* AllocPBackgroundMutableFileChild(
      const nsString& aName, const nsString& aType) override;

  bool DeallocPBackgroundMutableFileChild(
      PBackgroundMutableFileChild* aActor) override;

  mozilla::ipc::IPCResult RecvVersionChange(
      const uint64_t& aOldVersion, const Maybe<uint64_t>& aNewVersion) override;

  mozilla::ipc::IPCResult RecvInvalidate() override;

  mozilla::ipc::IPCResult RecvCloseAfterInvalidationComplete() override;
};

class BackgroundDatabaseRequestChild final
    : public BackgroundRequestChildBase,
      public PBackgroundIDBDatabaseRequestChild {
  friend class BackgroundDatabaseChild;
  friend IDBDatabase;

  RefPtr<IDBDatabase> mDatabase;

 private:
  // Only created by IDBDatabase.
  BackgroundDatabaseRequestChild(IDBDatabase* aDatabase, IDBRequest* aRequest);

  // Only destroyed by BackgroundDatabaseChild.
  ~BackgroundDatabaseRequestChild();

  bool HandleResponse(nsresult aResponse);

  bool HandleResponse(const CreateFileRequestResponse& aResponse);

  // IPDL methods are only called by IPDL.
  mozilla::ipc::IPCResult Recv__delete__(
      const DatabaseRequestResponse& aResponse) override;
};

class BackgroundVersionChangeTransactionChild;

class BackgroundTransactionBase {
  friend class BackgroundVersionChangeTransactionChild;

  // mTemporaryStrongTransaction is strong and is only valid until the end of
  // NoteComplete() member function or until the NoteActorDestroyed() member
  // function is called.
  RefPtr<IDBTransaction> mTemporaryStrongTransaction;

 protected:
  // mTransaction is weak and is valid until the NoteActorDestroyed() member
  // function is called.
  IDBTransaction* mTransaction;

 public:
#ifdef DEBUG
  virtual void AssertIsOnOwningThread() const = 0;
#else
  void AssertIsOnOwningThread() const {}
#endif

  IDBTransaction* GetDOMObject() const {
    AssertIsOnOwningThread();
    return mTransaction;
  }

 protected:
  BackgroundTransactionBase();
  explicit BackgroundTransactionBase(IDBTransaction* aTransaction);

  virtual ~BackgroundTransactionBase();

  void NoteActorDestroyed();

  void NoteComplete();

 private:
  // Only called by BackgroundVersionChangeTransactionChild.
  void SetDOMTransaction(IDBTransaction* aTransaction);
};

class BackgroundTransactionChild final : public BackgroundTransactionBase,
                                         public PBackgroundIDBTransactionChild {
  friend class BackgroundDatabaseChild;
  friend IDBDatabase;

 public:
#ifdef DEBUG
  void AssertIsOnOwningThread() const override;
#endif

  void SendDeleteMeInternal();

  bool SendDeleteMe() = delete;

 private:
  // Only created by IDBDatabase.
  explicit BackgroundTransactionChild(IDBTransaction* aTransaction);

  // Only destroyed by BackgroundDatabaseChild.
  ~BackgroundTransactionChild();

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvComplete(const nsresult& aResult) override;

  PBackgroundIDBRequestChild* AllocPBackgroundIDBRequestChild(
      const RequestParams& aParams) override;

  bool DeallocPBackgroundIDBRequestChild(
      PBackgroundIDBRequestChild* aActor) override;

  PBackgroundIDBCursorChild* AllocPBackgroundIDBCursorChild(
      const OpenCursorParams& aParams) override;

  bool DeallocPBackgroundIDBCursorChild(
      PBackgroundIDBCursorChild* aActor) override;
};

class BackgroundVersionChangeTransactionChild final
    : public BackgroundTransactionBase,
      public PBackgroundIDBVersionChangeTransactionChild {
  friend class BackgroundDatabaseChild;

  IDBOpenDBRequest* mOpenDBRequest;

 public:
#ifdef DEBUG
  void AssertIsOnOwningThread() const override;
#endif

  void SendDeleteMeInternal(bool aFailedConstructor);

  bool SendDeleteMe() = delete;

 private:
  // Only created by BackgroundDatabaseChild.
  explicit BackgroundVersionChangeTransactionChild(
      IDBOpenDBRequest* aOpenDBRequest);

  // Only destroyed by BackgroundDatabaseChild.
  ~BackgroundVersionChangeTransactionChild();

  // Only called by BackgroundDatabaseChild.
  void SetDOMTransaction(IDBTransaction* aDOMObject) {
    BackgroundTransactionBase::SetDOMTransaction(aDOMObject);
  }

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvComplete(const nsresult& aResult) override;

  PBackgroundIDBRequestChild* AllocPBackgroundIDBRequestChild(
      const RequestParams& aParams) override;

  bool DeallocPBackgroundIDBRequestChild(
      PBackgroundIDBRequestChild* aActor) override;

  PBackgroundIDBCursorChild* AllocPBackgroundIDBCursorChild(
      const OpenCursorParams& aParams) override;

  bool DeallocPBackgroundIDBCursorChild(
      PBackgroundIDBCursorChild* aActor) override;
};

class BackgroundMutableFileChild final : public PBackgroundMutableFileChild {
  friend class BackgroundDatabaseChild;
  friend IDBMutableFile;

  RefPtr<IDBMutableFile> mTemporaryStrongMutableFile;
  IDBMutableFile* mMutableFile;
  nsString mName;
  nsString mType;

 public:
  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  void EnsureDOMObject();

  IDBMutableFile* GetDOMObject() const {
    AssertIsOnOwningThread();
    return mMutableFile;
  }

  void ReleaseDOMObject();

  bool SendDeleteMe() = delete;

 private:
  // Only constructed by BackgroundDatabaseChild.
  BackgroundMutableFileChild(const nsAString& aName, const nsAString& aType);

  // Only destroyed by BackgroundDatabaseChild.
  ~BackgroundMutableFileChild();

  void SendDeleteMeInternal();

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  PBackgroundFileHandleChild* AllocPBackgroundFileHandleChild(
      const FileMode& aMode) override;

  bool DeallocPBackgroundFileHandleChild(
      PBackgroundFileHandleChild* aActor) override;
};

class BackgroundRequestChild final : public BackgroundRequestChildBase,
                                     public PBackgroundIDBRequestChild {
  friend class BackgroundTransactionChild;
  friend class BackgroundVersionChangeTransactionChild;
  friend struct CloneInfo;
  friend IDBTransaction;

  class PreprocessHelper;

  RefPtr<IDBTransaction> mTransaction;
  nsTArray<CloneInfo> mCloneInfos;
  uint32_t mRunningPreprocessHelpers;
  uint32_t mCurrentCloneDataIndex;
  nsresult mPreprocessResultCode;
  bool mGetAll;

 private:
  // Only created by IDBTransaction.
  explicit BackgroundRequestChild(IDBRequest* aRequest);

  // Only destroyed by BackgroundTransactionChild or
  // BackgroundVersionChangeTransactionChild.
  ~BackgroundRequestChild();

  void MaybeSendContinue();

  void OnPreprocessFinished(uint32_t aCloneDataIndex,
                            UniquePtr<JSStructuredCloneData> aCloneData);

  void OnPreprocessFailed(uint32_t aCloneDataIndex, nsresult aErrorCode);

  UniquePtr<JSStructuredCloneData> GetNextCloneData();

  void HandleResponse(nsresult aResponse);

  void HandleResponse(const Key& aResponse);

  void HandleResponse(const nsTArray<Key>& aResponse);

  void HandleResponse(const SerializedStructuredCloneReadInfo& aResponse);

  void HandleResponse(
      const nsTArray<SerializedStructuredCloneReadInfo>& aResponse);

  void HandleResponse(JS::Handle<JS::Value> aResponse);

  void HandleResponse(uint64_t aResponse);

  nsresult HandlePreprocess(const PreprocessInfo& aPreprocessInfo);

  nsresult HandlePreprocess(const nsTArray<PreprocessInfo>& aPreprocessInfos);

  nsresult HandlePreprocessInternal(
      const nsTArray<PreprocessInfo>& aPreprocessInfos);

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult Recv__delete__(
      const RequestResponse& aResponse) override;

  mozilla::ipc::IPCResult RecvPreprocess(
      const PreprocessParams& aParams) override;
};

struct CloneInfo {
  RefPtr<BackgroundRequestChild::PreprocessHelper> mPreprocessHelper;
  UniquePtr<JSStructuredCloneData> mCloneData;
};

// TODO: Consider defining different subclasses for the different cursor types,
// possibly using the CRTP, which would remove the need for various case
// distinctions.
class BackgroundCursorChild final : public PBackgroundIDBCursorChild {
  friend class BackgroundTransactionChild;
  friend class BackgroundVersionChangeTransactionChild;

  class DelayedActionRunnable;

  struct CachedResponse {
    CachedResponse() = delete;

    CachedResponse(Key aKey, StructuredCloneReadInfo&& aCloneInfo);
    CachedResponse(Key aKey, Key aLocaleAwareKey, Key aObjectStoreKey,
                   StructuredCloneReadInfo&& aCloneInfo);
    explicit CachedResponse(Key aKey);
    CachedResponse(Key aKey, Key aLocaleAwareKey, Key aObjectStoreKey);

    CachedResponse(CachedResponse&& aOther) = default;
    CachedResponse& operator=(CachedResponse&& aOther) = default;
    CachedResponse(const CachedResponse& aOther) = delete;
    CachedResponse& operator=(const CachedResponse& aOther) = delete;

    Key mKey;
    Key mLocaleAwareKey;
    Key mObjectStoreKey;
    StructuredCloneReadInfo mCloneInfo;
  };

  IDBRequest* mRequest;
  IDBTransaction* mTransaction;
  IDBObjectStore* mObjectStore;
  IDBIndex* mIndex;
  IDBCursor* mCursor;

  // These are only set while a request is in progress.
  RefPtr<IDBRequest> mStrongRequest;
  RefPtr<IDBCursor> mStrongCursor;

  Direction mDirection;

  NS_DECL_OWNINGTHREAD

  std::deque<CachedResponse> mCachedResponses, mDelayedResponses;
  bool mInFlightResponseInvalidationNeeded;

 public:
  BackgroundCursorChild(IDBRequest* aRequest, IDBObjectStore* aObjectStore,
                        Direction aDirection);

  BackgroundCursorChild(IDBRequest* aRequest, IDBIndex* aIndex,
                        Direction aDirection);

  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(BackgroundCursorChild);
  }

  void SendContinueInternal(const CursorRequestParams& aParams,
                            const Key& aCurrentKey,
                            const Key& aCurrentObjectStoreKey);

  void SendDeleteMeInternal();

  void InvalidateCachedResponses();

  template <typename Condition>
  void DiscardCachedResponses(const Condition& aConditionFunc);

  IDBRequest* GetRequest() const {
    AssertIsOnOwningThread();

    return mRequest;
  }

  IDBObjectStore* GetObjectStore() const {
    AssertIsOnOwningThread();

    return mObjectStore;
  }

  IDBIndex* GetIndex() const {
    AssertIsOnOwningThread();

    return mIndex;
  }

  Direction GetDirection() const {
    AssertIsOnOwningThread();

    return mDirection;
  }

 private:
  // Only destroyed by BackgroundTransactionChild or
  // BackgroundVersionChangeTransactionChild.
  ~BackgroundCursorChild();

  void CompleteContinueRequestFromCache();

  void HandleResponse(nsresult aResponse);

  void HandleResponse(const void_t& aResponse);

  void HandleResponse(const nsTArray<ObjectStoreCursorResponse>& aResponses);

  void HandleResponse(const nsTArray<ObjectStoreKeyCursorResponse>& aResponses);

  void HandleResponse(const nsTArray<IndexCursorResponse>& aResponses);

  template <typename T, typename Func>
  void HandleMultipleCursorResponses(const nsTArray<T>& aResponses,
                                     const Func& aHandleRecord);

  void HandleResponse(const nsTArray<IndexKeyCursorResponse>& aResponses);

  template <typename... Args>
  MOZ_MUST_USE RefPtr<IDBCursor> HandleIndividualCursorResponse(
      bool aUseAsCurrentResult, Args&&... aArgs);

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvResponse(
      const CursorResponse& aResponse) override;

 public:
  // Force callers to use SendContinueInternal.
  bool SendContinue(const CursorRequestParams& aParams, const Key& aCurrentKey,
                    const Key& aCurrentObjectStoreKey) = delete;

  bool SendDeleteMe() = delete;
};

class BackgroundFileHandleChild : public PBackgroundFileHandleChild {
  friend class BackgroundMutableFileChild;
  friend IDBMutableFile;

  // mTemporaryStrongFileHandle is strong and is only valid until the end of
  // NoteComplete() member function or until the NoteActorDestroyed() member
  // function is called.
  RefPtr<IDBFileHandle> mTemporaryStrongFileHandle;

  // mFileHandle is weak and is valid until the NoteActorDestroyed() member
  // function is called.
  IDBFileHandle* mFileHandle;

 public:
  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  void SendDeleteMeInternal();

  bool SendDeleteMe() = delete;

 private:
  // Only created by IDBMutableFile.
  explicit BackgroundFileHandleChild(IDBFileHandle* aFileHandle);

  ~BackgroundFileHandleChild();

  void NoteActorDestroyed();

  void NoteComplete();

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvComplete(const bool& aAborted) override;

  PBackgroundFileRequestChild* AllocPBackgroundFileRequestChild(
      const FileRequestParams& aParams) override;

  bool DeallocPBackgroundFileRequestChild(
      PBackgroundFileRequestChild* aActor) override;
};

class BackgroundFileRequestChild final : public PBackgroundFileRequestChild {
  friend class BackgroundFileHandleChild;
  friend IDBFileHandle;

  RefPtr<IDBFileRequest> mFileRequest;
  RefPtr<IDBFileHandle> mFileHandle;
  bool mActorDestroyed;

 public:
  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

 private:
  // Only created by IDBFileHandle.
  explicit BackgroundFileRequestChild(IDBFileRequest* aFileRequest);

  // Only destroyed by BackgroundFileHandleChild.
  ~BackgroundFileRequestChild();

  void HandleResponse(nsresult aResponse);

  void HandleResponse(const FileRequestGetFileResponse& aResponse);

  void HandleResponse(const nsCString& aResponse);

  void HandleResponse(const FileRequestMetadata& aResponse);

  void HandleResponse(JS::Handle<JS::Value> aResponse);

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult Recv__delete__(
      const FileRequestResponse& aResponse) override;

  mozilla::ipc::IPCResult RecvProgress(const uint64_t& aProgress,
                                       const uint64_t& aProgressMax) override;
};

class BackgroundUtilsChild final : public PBackgroundIndexedDBUtilsChild {
  friend class mozilla::ipc::BackgroundChildImpl;
  friend IndexedDatabaseManager;

  IndexedDatabaseManager* mManager;

  NS_DECL_OWNINGTHREAD

 public:
  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(BackgroundUtilsChild);
  }

  bool SendDeleteMe() = delete;

 private:
  // Only created by IndexedDatabaseManager.
  explicit BackgroundUtilsChild(IndexedDatabaseManager* aManager);

  // Only destroyed by mozilla::ipc::BackgroundChildImpl.
  ~BackgroundUtilsChild();

  void SendDeleteMeInternal();

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;
};

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_indexeddb_actorschild_h__
