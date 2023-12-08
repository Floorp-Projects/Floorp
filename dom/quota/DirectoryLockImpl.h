/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_DIRECTORYLOCKIMPL_H_
#define DOM_QUOTA_DIRECTORYLOCKIMPL_H_

#include "mozilla/InitializedOnce.h"
#include "mozilla/MozPromise.h"
#include "mozilla/dom/FlippedOnce.h"
#include "mozilla/dom/quota/CommonMetadata.h"
#include "mozilla/dom/quota/DirectoryLock.h"
#include "mozilla/dom/quota/OriginScope.h"

namespace mozilla::dom::quota {

enum class ShouldUpdateLockIdTableFlag { No, Yes };

class DirectoryLockImpl final : public ClientDirectoryLock,
                                public UniversalDirectoryLock {
  const NotNull<RefPtr<QuotaManager>> mQuotaManager;

  const Nullable<PersistenceType> mPersistenceType;
  const nsCString mSuffix;
  const nsCString mGroup;
  const OriginScope mOriginScope;
  const nsCString mStorageOrigin;
  const Nullable<Client::Type> mClientType;
  LazyInitializedOnceEarlyDestructible<
      const NotNull<RefPtr<OpenDirectoryListener>>>
      mOpenListener;
  MozPromiseHolder<BoolPromise> mAcquirePromiseHolder;
  std::function<void()> mInvalidateCallback;

  nsTArray<NotNull<DirectoryLockImpl*>> mBlocking;
  nsTArray<NotNull<DirectoryLockImpl*>> mBlockedOn;

  const int64_t mId;

  const bool mIsPrivate;

  const bool mExclusive;

  // Internal quota manager operations use this flag to prevent directory lock
  // registraction/unregistration from updating origin access time, etc.
  const bool mInternal;

  const bool mShouldUpdateLockIdTable;

  bool mRegistered;
  FlippedOnce<true> mPending;
  FlippedOnce<false> mInvalidated;
  FlippedOnce<false> mAcquired;

 public:
  DirectoryLockImpl(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                    const Nullable<PersistenceType>& aPersistenceType,
                    const nsACString& aSuffix, const nsACString& aGroup,
                    const OriginScope& aOriginScope,
                    const nsACString& aStorageOrigin, bool aIsPrivate,
                    const Nullable<Client::Type>& aClientType, bool aExclusive,
                    bool aInternal,
                    ShouldUpdateLockIdTableFlag aShouldUpdateLockIdTableFlag);

  static RefPtr<ClientDirectoryLock> Create(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      PersistenceType aPersistenceType,
      const quota::OriginMetadata& aOriginMetadata, Client::Type aClientType,
      bool aExclusive) {
    return Create(std::move(aQuotaManager),
                  Nullable<PersistenceType>(aPersistenceType),
                  aOriginMetadata.mSuffix, aOriginMetadata.mGroup,
                  OriginScope::FromOrigin(aOriginMetadata.mOrigin),
                  aOriginMetadata.mStorageOrigin, aOriginMetadata.mIsPrivate,
                  Nullable<Client::Type>(aClientType), aExclusive, false,
                  ShouldUpdateLockIdTableFlag::Yes);
  }

  static RefPtr<OriginDirectoryLock> CreateForEviction(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      PersistenceType aPersistenceType,
      const quota::OriginMetadata& aOriginMetadata) {
    MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_INVALID);
    MOZ_ASSERT(!aOriginMetadata.mOrigin.IsEmpty());
    MOZ_ASSERT(!aOriginMetadata.mStorageOrigin.IsEmpty());

    return Create(std::move(aQuotaManager),
                  Nullable<PersistenceType>(aPersistenceType),
                  aOriginMetadata.mSuffix, aOriginMetadata.mGroup,
                  OriginScope::FromOrigin(aOriginMetadata.mOrigin),
                  aOriginMetadata.mStorageOrigin, aOriginMetadata.mIsPrivate,
                  Nullable<Client::Type>(),
                  /* aExclusive */ true, /* aInternal */ true,
                  ShouldUpdateLockIdTableFlag::No);
  }

  static RefPtr<UniversalDirectoryLock> CreateInternal(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      const Nullable<PersistenceType>& aPersistenceType,
      const OriginScope& aOriginScope,
      const Nullable<Client::Type>& aClientType, bool aExclusive) {
    return Create(std::move(aQuotaManager), aPersistenceType, ""_ns, ""_ns,
                  aOriginScope, ""_ns, false, aClientType, aExclusive, true,
                  ShouldUpdateLockIdTableFlag::Yes);
  }

  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  bool IsInternal() const { return mInternal; }

  void SetRegistered(bool aRegistered) { mRegistered = aRegistered; }

  bool IsPending() const { return mPending; }

  // Ideally, we would have just one table (instead of these two:
  // QuotaManager::mDirectoryLocks and QuotaManager::mDirectoryLockIdTable) for
  // all registered locks. However, some directory locks need to be accessed off
  // the PBackground thread, so the access must be protected by the quota mutex.
  // The problem is that directory locks for eviction must be currently created
  // while the mutex lock is already acquired. So we decided to have two tables
  // for now and to not register directory locks for eviction in
  // QuotaManager::mDirectoryLockIdTable. This can be improved in future after
  // some refactoring of the mutex locking.
  bool ShouldUpdateLockIdTable() const { return mShouldUpdateLockIdTable; }

  bool ShouldUpdateLockTable() {
    return !mInternal &&
           mPersistenceType.Value() != PERSISTENCE_TYPE_PERSISTENT;
  }

  bool Overlaps(const DirectoryLockImpl& aLock) const;

  // Test whether this DirectoryLock needs to wait for the given lock.
  bool MustWaitFor(const DirectoryLockImpl& aLock) const;

  void AddBlockingLock(DirectoryLockImpl& aLock) {
    AssertIsOnOwningThread();

    mBlocking.AppendElement(WrapNotNull(&aLock));
  }

  const nsTArray<NotNull<DirectoryLockImpl*>>& GetBlockedOnLocks() {
    return mBlockedOn;
  }

  void AddBlockedOnLock(DirectoryLockImpl& aLock) {
    AssertIsOnOwningThread();

    mBlockedOn.AppendElement(WrapNotNull(&aLock));
  }

  void MaybeUnblock(DirectoryLockImpl& aLock) {
    AssertIsOnOwningThread();

    mBlockedOn.RemoveElement(&aLock);
    if (mBlockedOn.IsEmpty()) {
      NotifyOpenListener();
    }
  }

  void NotifyOpenListener();

  void Invalidate();

  void Unregister();

  // DirectoryLock interface

  NS_INLINE_DECL_REFCOUNTING(DirectoryLockImpl, override)

  int64_t Id() const override { return mId; }

  bool Acquired() const override { return mAcquired; }

  void Acquire(RefPtr<OpenDirectoryListener> aOpenListener) override;

  RefPtr<BoolPromise> Acquire() override;

  void AcquireImmediately() override;

  void AssertIsAcquiredExclusively() override
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  void OnInvalidate(std::function<void()>&& aCallback) override;

  void Log() const override;

  // OriginDirectoryLock interface

  PersistenceType GetPersistenceType() const override {
    MOZ_DIAGNOSTIC_ASSERT(!mPersistenceType.IsNull());

    return mPersistenceType.Value();
  }

  quota::OriginMetadata OriginMetadata() const override {
    MOZ_DIAGNOSTIC_ASSERT(!mGroup.IsEmpty());

    return quota::OriginMetadata{
        mSuffix,        mGroup,     nsCString(Origin()),
        mStorageOrigin, mIsPrivate, GetPersistenceType()};
  }

  const nsACString& Origin() const override {
    MOZ_DIAGNOSTIC_ASSERT(mOriginScope.IsOrigin());
    MOZ_DIAGNOSTIC_ASSERT(!mOriginScope.GetOrigin().IsEmpty());

    return mOriginScope.GetOrigin();
  }

  // ClientDirectoryLock interface

  Client::Type ClientType() const override {
    MOZ_DIAGNOSTIC_ASSERT(!mClientType.IsNull());
    MOZ_DIAGNOSTIC_ASSERT(mClientType.Value() < Client::TypeMax());

    return mClientType.Value();
  }

  // UniversalDirectoryLock interface

  const Nullable<PersistenceType>& NullablePersistenceType() const override {
    return mPersistenceType;
  }

  const OriginScope& GetOriginScope() const override { return mOriginScope; }

  const Nullable<Client::Type>& NullableClientType() const override {
    return mClientType;
  }

  RefPtr<ClientDirectoryLock> SpecializeForClient(
      PersistenceType aPersistenceType,
      const quota::OriginMetadata& aOriginMetadata,
      Client::Type aClientType) const override;

 private:
  ~DirectoryLockImpl();

  static RefPtr<DirectoryLockImpl> Create(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      const Nullable<PersistenceType>& aPersistenceType,
      const nsACString& aSuffix, const nsACString& aGroup,
      const OriginScope& aOriginScope, const nsACString& aStorageOrigin,
      bool aIsPrivate, const Nullable<Client::Type>& aClientType,
      bool aExclusive, bool aInternal,
      ShouldUpdateLockIdTableFlag aShouldUpdateLockIdTableFlag) {
    MOZ_ASSERT_IF(aOriginScope.IsOrigin(), !aOriginScope.GetOrigin().IsEmpty());
    MOZ_ASSERT_IF(!aInternal, !aPersistenceType.IsNull());
    MOZ_ASSERT_IF(!aInternal,
                  aPersistenceType.Value() != PERSISTENCE_TYPE_INVALID);
    MOZ_ASSERT_IF(!aInternal, !aGroup.IsEmpty());
    MOZ_ASSERT_IF(!aInternal, aOriginScope.IsOrigin());
    MOZ_ASSERT_IF(!aInternal, !aStorageOrigin.IsEmpty());
    MOZ_ASSERT_IF(!aInternal, !aClientType.IsNull());
    MOZ_ASSERT_IF(!aInternal, aClientType.Value() < Client::TypeMax());

    return MakeRefPtr<DirectoryLockImpl>(
        std::move(aQuotaManager), aPersistenceType, aSuffix, aGroup,
        aOriginScope, aStorageOrigin, aIsPrivate, aClientType, aExclusive,
        aInternal, aShouldUpdateLockIdTableFlag);
  }

  void AcquireInternal();
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_DIRECTORYLOCKIMPL_H_
