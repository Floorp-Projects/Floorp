/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_actorschild_h__
#define mozilla_dom_indexeddb_actorschild_h__

#include "js/RootingAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/IDBCursorType.h"
#include "mozilla/dom/IDBTransaction.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBCursorChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBDatabaseChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBFactoryChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBFactoryRequestChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBRequestChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBSharedTypes.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBTransactionChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBVersionChangeTransactionChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIndexedDBUtilsChild.h"
#include "mozilla/InitializedOnce.h"
#include "mozilla/UniquePtr.h"
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

MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(mozilla::dom::indexedDB::CloneInfo)

namespace mozilla::dom::indexedDB {

class BackgroundFactoryChild final : public PBackgroundIDBFactoryChild {
  friend class mozilla::ipc::BackgroundChildImpl;
  friend IDBFactory;

  // TODO: This long-lived raw pointer is very suspicious, in particular as it
  // is used in BackgroundDatabaseChild::EnsureDOMObject to reacquire a strong
  // reference. What ensures it is kept alive, and why can't we store a strong
  // reference here?
  IDBFactory* mFactory;

 public:
  NS_INLINE_DECL_REFCOUNTING(BackgroundFactoryChild, override)

  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(BackgroundFactoryChild);
  }

  IDBFactory& GetDOMObject() const {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mFactory);
    return *mFactory;
  }

  bool SendDeleteMe() = delete;

 private:
  // Only created by IDBFactory.
  explicit BackgroundFactoryChild(IDBFactory& aFactory);

  // Only destroyed by mozilla::ipc::BackgroundChildImpl.
  ~BackgroundFactoryChild();

  void SendDeleteMeInternal();

 public:
  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  PBackgroundIDBFactoryRequestChild* AllocPBackgroundIDBFactoryRequestChild(
      const FactoryRequestParams& aParams);

  bool DeallocPBackgroundIDBFactoryRequestChild(
      PBackgroundIDBFactoryRequestChild* aActor);

  already_AddRefed<PBackgroundIDBDatabaseChild>
  AllocPBackgroundIDBDatabaseChild(
      const DatabaseSpec& aSpec,
      PBackgroundIDBFactoryRequestChild* aRequest) const;

  mozilla::ipc::IPCResult RecvPBackgroundIDBDatabaseConstructor(
      PBackgroundIDBDatabaseChild* aActor, const DatabaseSpec& aSpec,
      NotNull<PBackgroundIDBFactoryRequestChild*> aRequest) override;
};

class BackgroundDatabaseChild;

class BackgroundRequestChildBase {
 protected:
  const NotNull<RefPtr<IDBRequest>> mRequest;

 public:
  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

 protected:
  explicit BackgroundRequestChildBase(
      MovingNotNull<RefPtr<IDBRequest>> aRequest);

  virtual ~BackgroundRequestChildBase();
};

class BackgroundFactoryRequestChild final
    : public BackgroundRequestChildBase,
      public PBackgroundIDBFactoryRequestChild {
  using PersistenceType = mozilla::dom::quota::PersistenceType;

  friend IDBFactory;
  friend class BackgroundFactoryChild;
  friend class BackgroundDatabaseChild;
  friend class PermissionRequestChild;
  friend class PermissionRequestParent;

  const SafeRefPtr<IDBFactory> mFactory;

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
  NotNull<IDBOpenDBRequest*> GetOpenDBRequest() const;

 private:
  // Only created by IDBFactory.
  BackgroundFactoryRequestChild(
      SafeRefPtr<IDBFactory> aFactory,
      MovingNotNull<RefPtr<IDBOpenDBRequest>> aOpenRequest, bool aIsDeleteOp,
      uint64_t aRequestedVersion);

  // Only destroyed by BackgroundFactoryChild.
  ~BackgroundFactoryRequestChild();

  void SetDatabaseActor(BackgroundDatabaseChild* aActor);

  void HandleResponse(nsresult aResponse);

  void HandleResponse(const OpenDatabaseRequestResponse& aResponse);

  void HandleResponse(const DeleteDatabaseRequestResponse& aResponse);

 public:
  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult Recv__delete__(
      const FactoryRequestResponse& aResponse);

  mozilla::ipc::IPCResult RecvPermissionChallenge(
      PrincipalInfo&& aPrincipalInfo);

  mozilla::ipc::IPCResult RecvBlocked(uint64_t aCurrentVersion);
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
  NS_INLINE_DECL_REFCOUNTING(BackgroundDatabaseChild, override)

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

  ~BackgroundDatabaseChild();

  void SendDeleteMeInternal();

  [[nodiscard]] bool EnsureDOMObject();

  void ReleaseDOMObject();

 public:
  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  PBackgroundIDBDatabaseFileChild* AllocPBackgroundIDBDatabaseFileChild(
      const IPCBlob& aIPCBlob);

  bool DeallocPBackgroundIDBDatabaseFileChild(
      PBackgroundIDBDatabaseFileChild* aActor) const;

  already_AddRefed<PBackgroundIDBVersionChangeTransactionChild>
  AllocPBackgroundIDBVersionChangeTransactionChild(uint64_t aCurrentVersion,
                                                   uint64_t aRequestedVersion,
                                                   int64_t aNextObjectStoreId,
                                                   int64_t aNextIndexId);

  mozilla::ipc::IPCResult RecvPBackgroundIDBVersionChangeTransactionConstructor(
      PBackgroundIDBVersionChangeTransactionChild* aActor,
      const uint64_t& aCurrentVersion, const uint64_t& aRequestedVersion,
      const int64_t& aNextObjectStoreId, const int64_t& aNextIndexId) override;

  mozilla::ipc::IPCResult RecvVersionChange(uint64_t aOldVersion,
                                            Maybe<uint64_t> aNewVersion);

  mozilla::ipc::IPCResult RecvInvalidate();

  mozilla::ipc::IPCResult RecvCloseAfterInvalidationComplete();
};

class BackgroundVersionChangeTransactionChild;

class BackgroundTransactionBase {
  friend class BackgroundVersionChangeTransactionChild;

  // mTemporaryStrongTransaction is strong and is only valid until the end of
  // NoteComplete() member function or until the NoteActorDestroyed() member
  // function is called.
  SafeRefPtr<IDBTransaction> mTemporaryStrongTransaction;

 protected:
  // mTransaction is weak and is valid until the NoteActorDestroyed() member
  // function is called.
  IDBTransaction* mTransaction = nullptr;

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
  MOZ_COUNTED_DEFAULT_CTOR(BackgroundTransactionBase);

  explicit BackgroundTransactionBase(SafeRefPtr<IDBTransaction> aTransaction);

  MOZ_COUNTED_DTOR_VIRTUAL(BackgroundTransactionBase);

  void NoteActorDestroyed();

  void NoteComplete();

 private:
  // Only called by BackgroundVersionChangeTransactionChild.
  void SetDOMTransaction(SafeRefPtr<IDBTransaction> aTransaction);
};

class BackgroundTransactionChild final : public BackgroundTransactionBase,
                                         public PBackgroundIDBTransactionChild {
  friend class BackgroundDatabaseChild;
  friend IDBDatabase;

 public:
  NS_INLINE_DECL_REFCOUNTING(BackgroundTransactionChild, override)

#ifdef DEBUG
  void AssertIsOnOwningThread() const override;
#endif

  void SendDeleteMeInternal();

  bool SendDeleteMe() = delete;

 private:
  // Only created by IDBDatabase.
  explicit BackgroundTransactionChild(SafeRefPtr<IDBTransaction> aTransaction);

  // Only destroyed by BackgroundDatabaseChild.
  ~BackgroundTransactionChild();

 public:
  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvComplete(nsresult aResult);

  PBackgroundIDBRequestChild* AllocPBackgroundIDBRequestChild(
      const RequestParams& aParams);

  bool DeallocPBackgroundIDBRequestChild(PBackgroundIDBRequestChild* aActor);

  PBackgroundIDBCursorChild* AllocPBackgroundIDBCursorChild(
      const OpenCursorParams& aParams);

  bool DeallocPBackgroundIDBCursorChild(PBackgroundIDBCursorChild* aActor);
};

class BackgroundVersionChangeTransactionChild final
    : public BackgroundTransactionBase,
      public PBackgroundIDBVersionChangeTransactionChild {
  friend class BackgroundDatabaseChild;

  IDBOpenDBRequest* mOpenDBRequest;

 public:
  NS_INLINE_DECL_REFCOUNTING(BackgroundVersionChangeTransactionChild, override)

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
  using BackgroundTransactionBase::SetDOMTransaction;

 public:
  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvComplete(nsresult aResult);

  PBackgroundIDBRequestChild* AllocPBackgroundIDBRequestChild(
      const RequestParams& aParams);

  bool DeallocPBackgroundIDBRequestChild(PBackgroundIDBRequestChild* aActor);

  PBackgroundIDBCursorChild* AllocPBackgroundIDBCursorChild(
      const OpenCursorParams& aParams);

  bool DeallocPBackgroundIDBCursorChild(PBackgroundIDBCursorChild* aActor);
};

class BackgroundRequestChild final : public BackgroundRequestChildBase,
                                     public PBackgroundIDBRequestChild {
  friend class BackgroundTransactionChild;
  friend class BackgroundVersionChangeTransactionChild;
  friend struct CloneInfo;
  friend IDBTransaction;

  class PreprocessHelper;

  SafeRefPtr<IDBTransaction> mTransaction;
  nsTArray<CloneInfo> mCloneInfos;
  uint32_t mRunningPreprocessHelpers;
  uint32_t mCurrentCloneDataIndex;
  nsresult mPreprocessResultCode;
  bool mGetAll;

 private:
  // Only created by IDBTransaction.
  explicit BackgroundRequestChild(MovingNotNull<RefPtr<IDBRequest>> aRequest);

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

  void HandleResponse(SerializedStructuredCloneReadInfo&& aResponse);

  void HandleResponse(nsTArray<SerializedStructuredCloneReadInfo>&& aResponse);

  void HandleResponse(JS::Handle<JS::Value> aResponse);

  void HandleResponse(uint64_t aResponse);

  nsresult HandlePreprocess(const PreprocessInfo& aPreprocessInfo);

  nsresult HandlePreprocess(const nsTArray<PreprocessInfo>& aPreprocessInfos);

  nsresult HandlePreprocessInternal(
      const nsTArray<PreprocessInfo>& aPreprocessInfos);

  SafeRefPtr<IDBTransaction> AcquireTransaction() const {
    return mTransaction.clonePtr();
  }

 public:
  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult Recv__delete__(RequestResponse&& aResponse);

  mozilla::ipc::IPCResult RecvPreprocess(const PreprocessParams& aParams);
};

struct CloneInfo {
  RefPtr<BackgroundRequestChild::PreprocessHelper> mPreprocessHelper;
  UniquePtr<JSStructuredCloneData> mCloneData;
};

class BackgroundCursorChildBase
    : public PBackgroundIDBCursorChild,
      public SafeRefCounted<BackgroundCursorChildBase> {
 private:
  NS_DECL_OWNINGTHREAD

 public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(
      mozilla::dom::indexedDB::BackgroundCursorChildBase)
  MOZ_INLINE_DECL_SAFEREFCOUNTING_INHERITED(BackgroundCursorChildBase,
                                            SafeRefCounted)

 protected:
  ~BackgroundCursorChildBase();

  InitializedOnce<const NotNull<IDBRequest*>> mRequest;
  Maybe<IDBTransaction&> mTransaction;

  // These are only set while a request is in progress.
  RefPtr<IDBRequest> mStrongRequest;
  RefPtr<IDBCursor> mStrongCursor;

  const Direction mDirection;

  BackgroundCursorChildBase(NotNull<IDBRequest*> aRequest,
                            Direction aDirection);

  void HandleResponse(nsresult aResponse);

 public:
  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(BackgroundCursorChildBase);
  }

  MovingNotNull<RefPtr<IDBRequest>> AcquireRequest() const;

  NotNull<IDBRequest*> GetRequest() const {
    AssertIsOnOwningThread();

    return *mRequest;
  }

  Direction GetDirection() const {
    AssertIsOnOwningThread();

    return mDirection;
  }

  virtual void SendDeleteMeInternal() = 0;

  virtual mozilla::ipc::IPCResult RecvResponse(CursorResponse&& aResponse) = 0;
};

template <IDBCursorType CursorType>
class BackgroundCursorChild final : public BackgroundCursorChildBase {
 public:
  using SourceType = CursorSourceType<CursorType>;
  using ResponseType = typename CursorTypeTraits<CursorType>::ResponseType;

 private:
  friend class BackgroundTransactionChild;
  friend class BackgroundVersionChangeTransactionChild;

  InitializedOnce<const NotNull<SourceType*>> mSource;
  IDBCursorImpl<CursorType>* mCursor;

  std::deque<CursorData<CursorType>> mCachedResponses, mDelayedResponses;
  bool mInFlightResponseInvalidationNeeded;

 public:
  BackgroundCursorChild(NotNull<IDBRequest*> aRequest, SourceType* aSource,
                        Direction aDirection);

  void SendContinueInternal(const CursorRequestParams& aParams,
                            const CursorData<CursorType>& aCurrentData);

  void InvalidateCachedResponses();

  template <typename Condition>
  void DiscardCachedResponses(const Condition& aConditionFunc);

  SourceType* GetSource() const {
    AssertIsOnOwningThread();

    return *mSource;
  }

  void SendDeleteMeInternal() final;

 private:
  // Only destroyed by BackgroundTransactionChild or
  // BackgroundVersionChangeTransactionChild.
  ~BackgroundCursorChild();

  void CompleteContinueRequestFromCache();

  using BackgroundCursorChildBase::HandleResponse;

  void HandleResponse(const void_t& aResponse);

  void HandleResponse(nsTArray<ResponseType>&& aResponses);

  template <typename Func>
  void HandleMultipleCursorResponses(nsTArray<ResponseType>&& aResponses,
                                     const Func& aHandleRecord);

  template <typename... Args>
  [[nodiscard]] RefPtr<IDBCursor> HandleIndividualCursorResponse(
      bool aUseAsCurrentResult, Args&&... aArgs);

  SafeRefPtr<BackgroundCursorChild> SafeRefPtrFromThis();

 public:
  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvResponse(CursorResponse&& aResponse) override;

  // Force callers to use SendContinueInternal.
  bool SendContinue(const CursorRequestParams& aParams, const Key& aCurrentKey,
                    const Key& aCurrentObjectStoreKey) = delete;

  bool SendDeleteMe() = delete;
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

 public:
  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;
};

}  // namespace mozilla::dom::indexedDB

#endif  // mozilla_dom_indexeddb_actorschild_h__
