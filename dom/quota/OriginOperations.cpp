/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OriginOperations.h"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "ErrorList.h"
#include "FileUtils.h"
#include "GroupInfo.h"
#include "MainThreadUtils.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/NotNull.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/dom/quota/CommonMetadata.h"
#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/Constants.h"
#include "mozilla/dom/quota/DirectoryLock.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/dom/quota/PQuota.h"
#include "mozilla/dom/quota/PQuotaRequest.h"
#include "mozilla/dom/quota/PQuotaUsageRequest.h"
#include "mozilla/dom/quota/OriginScope.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/QuotaManagerImpl.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/dom/quota/StreamUtils.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/fallible.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "NormalOriginOperationBase.h"
#include "nsCOMPtr.h"
#include "nsTHashMap.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsHashKeys.h"
#include "nsIBinaryOutputStream.h"
#include "nsIFile.h"
#include "nsIObjectOutputStream.h"
#include "nsIOutputStream.h"
#include "nsLiteralString.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsTArray.h"
#include "OriginInfo.h"
#include "OriginOperationBase.h"
#include "QuotaRequestBase.h"
#include "QuotaUsageRequestBase.h"
#include "ResolvableNormalOriginOp.h"
#include "prthread.h"
#include "prtime.h"

namespace mozilla::dom::quota {

using namespace mozilla::ipc;

namespace {

class FinalizeOriginEvictionOp : public OriginOperationBase {
  nsTArray<RefPtr<OriginDirectoryLock>> mLocks;

 public:
  FinalizeOriginEvictionOp(nsISerialEventTarget* aBackgroundThread,
                           nsTArray<RefPtr<OriginDirectoryLock>>&& aLocks)
      : OriginOperationBase(aBackgroundThread,
                            "dom::quota::FinalizeOriginEvictionOp"),
        mLocks(std::move(aLocks)) {
    MOZ_ASSERT(!NS_IsMainThread());
  }

 private:
  ~FinalizeOriginEvictionOp() = default;

  virtual void Open() override;

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  virtual void UnblockOpen() override;
};

class SaveOriginAccessTimeOp : public NormalOriginOperationBase {
  const OriginMetadata mOriginMetadata;
  int64_t mTimestamp;

 public:
  SaveOriginAccessTimeOp(const OriginMetadata& aOriginMetadata,
                         int64_t aTimestamp)
      : NormalOriginOperationBase(
            "dom::quota::SaveOriginAccessTimeOp",
            Nullable<PersistenceType>(aOriginMetadata.mPersistenceType),
            OriginScope::FromOrigin(aOriginMetadata.mOrigin),
            Nullable<Client::Type>(),
            /* aExclusive */ false),
        mOriginMetadata(aOriginMetadata),
        mTimestamp(aTimestamp) {
    AssertIsOnOwningThread();
  }

 private:
  ~SaveOriginAccessTimeOp() = default;

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  virtual void SendResults() override;
};

class ClearPrivateRepositoryOp : public ResolvableNormalOriginOp<bool> {
 public:
  ClearPrivateRepositoryOp()
      : ResolvableNormalOriginOp(
            "dom::quota::ClearPrivateRepositoryOp",
            Nullable<PersistenceType>(PERSISTENCE_TYPE_PRIVATE),
            OriginScope::FromNull(), Nullable<Client::Type>(),
            /* aExclusive */ true) {
    AssertIsOnOwningThread();
  }

 private:
  ~ClearPrivateRepositoryOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool GetResolveValue() override { return true; }
};

class ShutdownStorageOp : public ResolvableNormalOriginOp<bool> {
 public:
  ShutdownStorageOp()
      : ResolvableNormalOriginOp(
            "dom::quota::ShutdownStorageOp", Nullable<PersistenceType>(),
            OriginScope::FromNull(), Nullable<Client::Type>(),
            /* aExclusive */ true) {
    AssertIsOnOwningThread();
  }

 private:
  ~ShutdownStorageOp() = default;

#ifdef DEBUG
  nsresult DirectoryOpen() override;
#endif

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool GetResolveValue() override { return true; }
};

// A mix-in class to simplify operations that need to process every origin in
// one or more repositories. Sub-classes should call TraverseRepository in their
// DoDirectoryWork and implement a ProcessOrigin method for their per-origin
// logic.
class TraverseRepositoryHelper {
 public:
  TraverseRepositoryHelper() = default;

 protected:
  virtual ~TraverseRepositoryHelper() = default;

  // If ProcessOrigin returns an error, TraverseRepository will immediately
  // terminate and return the received error code to its caller.
  nsresult TraverseRepository(QuotaManager& aQuotaManager,
                              PersistenceType aPersistenceType);

 private:
  virtual const Atomic<bool>& GetIsCanceledFlag() = 0;

  virtual nsresult ProcessOrigin(QuotaManager& aQuotaManager,
                                 nsIFile& aOriginDir, const bool aPersistent,
                                 const PersistenceType aPersistenceType) = 0;
};

class GetUsageOp final : public QuotaUsageRequestBase,
                         public TraverseRepositoryHelper {
  nsTArray<OriginUsage> mOriginUsages;
  nsTHashMap<nsCStringHashKey, uint32_t> mOriginUsagesIndex;

  bool mGetAll;

 public:
  explicit GetUsageOp(const UsageRequestParams& aParams);

 private:
  ~GetUsageOp() = default;

  void ProcessOriginInternal(QuotaManager* aQuotaManager,
                             const PersistenceType aPersistenceType,
                             const nsACString& aOrigin,
                             const int64_t aTimestamp, const bool aPersisted,
                             const uint64_t aUsage);

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  const Atomic<bool>& GetIsCanceledFlag() override;

  nsresult ProcessOrigin(QuotaManager& aQuotaManager, nsIFile& aOriginDir,
                         const bool aPersistent,
                         const PersistenceType aPersistenceType) override;

  void GetResponse(UsageRequestResponse& aResponse) override;
};

class GetOriginUsageOp final : public QuotaUsageRequestBase {
  const OriginUsageParams mParams;
  nsCString mSuffix;
  nsCString mGroup;
  nsCString mStorageOrigin;
  uint64_t mUsage;
  uint64_t mFileUsage;
  bool mIsPrivate;
  bool mFromMemory;

 public:
  explicit GetOriginUsageOp(const UsageRequestParams& aParams);

 private:
  ~GetOriginUsageOp() = default;

  nsresult DoInit(QuotaManager& aQuotaManager) override;

  RefPtr<DirectoryLock> CreateDirectoryLock() override;

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(UsageRequestResponse& aResponse) override;
};

class StorageNameOp final : public QuotaRequestBase {
  nsString mName;

 public:
  StorageNameOp();

 private:
  ~StorageNameOp() = default;

  RefPtr<DirectoryLock> CreateDirectoryLock() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class InitializedRequestBase : public QuotaRequestBase {
 protected:
  bool mInitialized;

  InitializedRequestBase(const char* aRunnableName);

 private:
  RefPtr<DirectoryLock> CreateDirectoryLock() override;
};

class StorageInitializedOp final : public InitializedRequestBase {
 public:
  StorageInitializedOp()
      : InitializedRequestBase("dom::quota::StorageInitializedOp") {}

 private:
  ~StorageInitializedOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class TemporaryStorageInitializedOp final : public InitializedRequestBase {
 public:
  TemporaryStorageInitializedOp()
      : InitializedRequestBase("dom::quota::StorageInitializedOp") {}

 private:
  ~TemporaryStorageInitializedOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class InitOp final : public QuotaRequestBase {
 public:
  InitOp();

 private:
  ~InitOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class InitTemporaryStorageOp final : public QuotaRequestBase {
 public:
  InitTemporaryStorageOp();

 private:
  ~InitTemporaryStorageOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class InitializeOriginRequestBase : public QuotaRequestBase {
 protected:
  const PrincipalInfo mPrincipalInfo;
  nsCString mSuffix;
  nsCString mGroup;
  nsCString mStorageOrigin;
  bool mIsPrivate;
  bool mCreated;

  InitializeOriginRequestBase(const char* aRunnableName,
                              PersistenceType aPersistenceType,
                              const PrincipalInfo& aPrincipalInfo);

  nsresult DoInit(QuotaManager& aQuotaManager) override;
};

class InitializePersistentOriginOp final : public InitializeOriginRequestBase {
 public:
  explicit InitializePersistentOriginOp(const RequestParams& aParams);

 private:
  ~InitializePersistentOriginOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class InitializeTemporaryOriginOp final : public InitializeOriginRequestBase {
 public:
  explicit InitializeTemporaryOriginOp(const RequestParams& aParams);

 private:
  ~InitializeTemporaryOriginOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class GetFullOriginMetadataOp : public QuotaRequestBase {
  const GetFullOriginMetadataParams mParams;
  // XXX Consider wrapping with LazyInitializedOnce
  OriginMetadata mOriginMetadata;
  Maybe<FullOriginMetadata> mMaybeFullOriginMetadata;

 public:
  explicit GetFullOriginMetadataOp(const GetFullOriginMetadataParams& aParams);

 private:
  nsresult DoInit(QuotaManager& aQuotaManager) override;

  RefPtr<DirectoryLock> CreateDirectoryLock() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class ResetOrClearOp final : public QuotaRequestBase {
  const bool mClear;

 public:
  explicit ResetOrClearOp(bool aClear);

 private:
  ~ResetOrClearOp() = default;

  void DeleteFiles(QuotaManager& aQuotaManager);

  void DeleteStorageFile(QuotaManager& aQuotaManager);

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  virtual void GetResponse(RequestResponse& aResponse) override;
};

class ClearRequestBase : public QuotaRequestBase {
 protected:
  explicit ClearRequestBase(const char* aRunnableName, bool aExclusive)
      : QuotaRequestBase(aRunnableName, aExclusive) {
    AssertIsOnOwningThread();
  }

  ClearRequestBase(const char* aRunnableName,
                   const Nullable<PersistenceType>& aPersistenceType,
                   const OriginScope& aOriginScope,
                   const Nullable<Client::Type>& aClientType, bool aExclusive)
      : QuotaRequestBase(aRunnableName, aPersistenceType, aOriginScope,
                         aClientType, aExclusive) {}

  void DeleteFiles(QuotaManager& aQuotaManager,
                   PersistenceType aPersistenceType);

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;
};

class ClearOriginOp final : public ClearRequestBase {
  const ClearResetOriginParams mParams;
  const bool mMatchAll;

 public:
  explicit ClearOriginOp(const RequestParams& aParams);

 private:
  ~ClearOriginOp() = default;

  void GetResponse(RequestResponse& aResponse) override;
};

class ClearDataOp final : public ClearRequestBase {
  const ClearDataParams mParams;

 public:
  explicit ClearDataOp(const RequestParams& aParams);

 private:
  ~ClearDataOp() = default;

  void GetResponse(RequestResponse& aResponse) override;
};

class ResetOriginOp final : public QuotaRequestBase {
 public:
  explicit ResetOriginOp(const RequestParams& aParams);

 private:
  ~ResetOriginOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class PersistRequestBase : public QuotaRequestBase {
  const PrincipalInfo mPrincipalInfo;

 protected:
  nsCString mSuffix;
  nsCString mGroup;
  nsCString mStorageOrigin;
  bool mIsPrivate;

 protected:
  explicit PersistRequestBase(const PrincipalInfo& aPrincipalInfo);

  nsresult DoInit(QuotaManager& aQuotaManager) override;
};

class PersistedOp final : public PersistRequestBase {
  bool mPersisted;

 public:
  explicit PersistedOp(const RequestParams& aParams);

 private:
  ~PersistedOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class PersistOp final : public PersistRequestBase {
 public:
  explicit PersistOp(const RequestParams& aParams);

 private:
  ~PersistOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class EstimateOp final : public QuotaRequestBase {
  const EstimateParams mParams;
  OriginMetadata mOriginMetadata;
  std::pair<uint64_t, uint64_t> mUsageAndLimit;

 public:
  explicit EstimateOp(const EstimateParams& aParams);

 private:
  ~EstimateOp() = default;

  nsresult DoInit(QuotaManager& aQuotaManager) override;

  RefPtr<DirectoryLock> CreateDirectoryLock() override;

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class ListOriginsOp final : public QuotaRequestBase,
                            public TraverseRepositoryHelper {
  // XXX Bug 1521541 will make each origin has it's own state.
  nsTArray<nsCString> mOrigins;

 public:
  ListOriginsOp();

 private:
  ~ListOriginsOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  const Atomic<bool>& GetIsCanceledFlag() override;

  nsresult ProcessOrigin(QuotaManager& aQuotaManager, nsIFile& aOriginDir,
                         const bool aPersistent,
                         const PersistenceType aPersistenceType) override;

  void GetResponse(RequestResponse& aResponse) override;
};

}  // namespace

RefPtr<OriginOperationBase> CreateFinalizeOriginEvictionOp(
    nsISerialEventTarget* aOwningThread,
    nsTArray<RefPtr<OriginDirectoryLock>>&& aLocks) {
  return MakeRefPtr<FinalizeOriginEvictionOp>(aOwningThread, std::move(aLocks));
}

RefPtr<NormalOriginOperationBase> CreateSaveOriginAccessTimeOp(
    const OriginMetadata& aOriginMetadata, int64_t aTimestamp) {
  return MakeRefPtr<SaveOriginAccessTimeOp>(aOriginMetadata, aTimestamp);
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateClearPrivateRepositoryOp() {
  return MakeRefPtr<ClearPrivateRepositoryOp>();
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateShutdownStorageOp() {
  return MakeRefPtr<ShutdownStorageOp>();
}

RefPtr<QuotaUsageRequestBase> CreateGetUsageOp(
    const UsageRequestParams& aParams) {
  return MakeRefPtr<GetUsageOp>(aParams);
}

RefPtr<QuotaUsageRequestBase> CreateGetOriginUsageOp(
    const UsageRequestParams& aParams) {
  return MakeRefPtr<GetOriginUsageOp>(aParams);
}

RefPtr<QuotaRequestBase> CreateStorageNameOp() {
  return MakeRefPtr<StorageNameOp>();
}

RefPtr<QuotaRequestBase> CreateStorageInitializedOp() {
  return MakeRefPtr<StorageInitializedOp>();
}

RefPtr<QuotaRequestBase> CreateTemporaryStorageInitializedOp() {
  return MakeRefPtr<TemporaryStorageInitializedOp>();
}

RefPtr<QuotaRequestBase> CreateInitOp() { return MakeRefPtr<InitOp>(); }

RefPtr<QuotaRequestBase> CreateInitTemporaryStorageOp() {
  return MakeRefPtr<InitTemporaryStorageOp>();
}

RefPtr<QuotaRequestBase> CreateInitializePersistentOriginOp(
    const RequestParams& aParams) {
  return MakeRefPtr<InitializePersistentOriginOp>(aParams);
}

RefPtr<QuotaRequestBase> CreateInitializeTemporaryOriginOp(
    const RequestParams& aParams) {
  return MakeRefPtr<InitializeTemporaryOriginOp>(aParams);
}

RefPtr<QuotaRequestBase> CreateGetFullOriginMetadataOp(
    const GetFullOriginMetadataParams& aParams) {
  return MakeRefPtr<GetFullOriginMetadataOp>(aParams);
}

RefPtr<QuotaRequestBase> CreateResetOrClearOp(bool aClear) {
  return MakeRefPtr<ResetOrClearOp>(aClear);
}

RefPtr<QuotaRequestBase> CreateClearOriginOp(const RequestParams& aParams) {
  return MakeRefPtr<ClearOriginOp>(aParams);
}

RefPtr<QuotaRequestBase> CreateClearDataOp(const RequestParams& aParams) {
  return MakeRefPtr<ClearDataOp>(aParams);
}

RefPtr<QuotaRequestBase> CreateResetOriginOp(const RequestParams& aParams) {
  return MakeRefPtr<ResetOriginOp>(aParams);
}

RefPtr<QuotaRequestBase> CreatePersistedOp(const RequestParams& aParams) {
  return MakeRefPtr<PersistedOp>(aParams);
}

RefPtr<QuotaRequestBase> CreatePersistOp(const RequestParams& aParams) {
  return MakeRefPtr<PersistOp>(aParams);
}

RefPtr<QuotaRequestBase> CreateEstimateOp(const EstimateParams& aParams) {
  return MakeRefPtr<EstimateOp>(aParams);
}

RefPtr<QuotaRequestBase> CreateListOriginsOp() {
  return MakeRefPtr<ListOriginsOp>();
}

void FinalizeOriginEvictionOp::Open() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(GetState() == State_Initial);

  AdvanceState();

  QM_TRY(MOZ_TO_RESULT(DirectoryOpen()), QM_VOID,
         [this](const nsresult rv) { Finish(rv); });
}

nsresult FinalizeOriginEvictionOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("FinalizeOriginEvictionOp::DoDirectoryWork", OTHER);

  for (const auto& lock : mLocks) {
    aQuotaManager.OriginClearCompleted(
        lock->GetPersistenceType(), lock->Origin(), Nullable<Client::Type>());
  }

  return NS_OK;
}

void FinalizeOriginEvictionOp::UnblockOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(GetState() == State_UnblockingOpen);

#ifdef DEBUG
  NoteActorDestroyed();
#endif

  mLocks.Clear();

  AdvanceState();
}

nsresult SaveOriginAccessTimeOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!mPersistenceType.IsNull());
  MOZ_ASSERT(mOriginScope.IsOrigin());

  AUTO_PROFILER_LABEL("SaveOriginAccessTimeOp::DoDirectoryWork", OTHER);

  QM_TRY_INSPECT(const auto& file,
                 aQuotaManager.GetOriginDirectory(mOriginMetadata));

  // The origin directory might not exist
  // anymore, because it was deleted by a clear operation.
  QM_TRY_INSPECT(const bool& exists, MOZ_TO_RESULT_INVOKE_MEMBER(file, Exists));

  if (exists) {
    QM_TRY(MOZ_TO_RESULT(file->Append(nsLiteralString(METADATA_V2_FILE_NAME))));

    QM_TRY_INSPECT(const auto& stream,
                   GetBinaryOutputStream(*file, FileFlag::Update));
    MOZ_ASSERT(stream);

    QM_TRY(MOZ_TO_RESULT(stream->Write64(mTimestamp)));
  }

  return NS_OK;
}

void SaveOriginAccessTimeOp::SendResults() {
#ifdef DEBUG
  NoteActorDestroyed();
#endif
}

nsresult ClearPrivateRepositoryOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!mPersistenceType.IsNull());
  MOZ_ASSERT(mPersistenceType.Value() == PERSISTENCE_TYPE_PRIVATE);

  AUTO_PROFILER_LABEL("ClearPrivateRepositoryOp::DoDirectoryWork", OTHER);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.EnsureStorageIsInitialized()));

  QM_TRY_INSPECT(
      const auto& directory,
      QM_NewLocalFile(aQuotaManager.GetStoragePath(mPersistenceType.Value())));

  nsresult rv = directory->Remove(true);
  if (rv != NS_ERROR_FILE_NOT_FOUND && NS_FAILED(rv)) {
    // This should never fail if we've closed all storage connections
    // correctly...
    MOZ_ASSERT(false, "Failed to remove directory!");
  }

  aQuotaManager.RemoveQuotaForRepository(mPersistenceType.Value());

  aQuotaManager.RepositoryClearCompleted(mPersistenceType.Value());

  return NS_OK;
}

#ifdef DEBUG
nsresult ShutdownStorageOp::DirectoryOpen() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDirectoryLock);
  mDirectoryLock->AssertIsAcquiredExclusively();

  return NormalOriginOperationBase::DirectoryOpen();
}
#endif

nsresult ShutdownStorageOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("ShutdownStorageOp::DoDirectoryWork", OTHER);

  aQuotaManager.ShutdownStorageInternal();

  return NS_OK;
}

nsresult TraverseRepositoryHelper::TraverseRepository(
    QuotaManager& aQuotaManager, PersistenceType aPersistenceType) {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(
      const auto& directory,
      QM_NewLocalFile(aQuotaManager.GetStoragePath(aPersistenceType)));

  QM_TRY_INSPECT(const bool& exists,
                 MOZ_TO_RESULT_INVOKE_MEMBER(directory, Exists));

  if (!exists) {
    return NS_OK;
  }

  QM_TRY(CollectEachFileAtomicCancelable(
      *directory, GetIsCanceledFlag(),
      [this, aPersistenceType, &aQuotaManager,
       persistent = aPersistenceType == PERSISTENCE_TYPE_PERSISTENT](
          const nsCOMPtr<nsIFile>& originDir) -> Result<Ok, nsresult> {
        QM_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(*originDir));

        switch (dirEntryKind) {
          case nsIFileKind::ExistsAsDirectory:
            QM_TRY(MOZ_TO_RESULT(ProcessOrigin(aQuotaManager, *originDir,
                                               persistent, aPersistenceType)));
            break;

          case nsIFileKind::ExistsAsFile: {
            QM_TRY_INSPECT(const auto& leafName,
                           MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(
                               nsAutoString, originDir, GetLeafName));

            // Unknown files during getting usages are allowed. Just warn if we
            // find them.
            if (!IsOSMetadata(leafName)) {
              UNKNOWN_FILE_WARNING(leafName);
            }

            break;
          }

          case nsIFileKind::DoesNotExist:
            // Ignore files that got removed externally while iterating.
            break;
        }

        return Ok{};
      }));

  return NS_OK;
}

GetUsageOp::GetUsageOp(const UsageRequestParams& aParams)
    : QuotaUsageRequestBase("dom::quota::GetUsageOp"),
      mGetAll(aParams.get_AllUsageParams().getAll()) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() == UsageRequestParams::TAllUsageParams);
}

void GetUsageOp::ProcessOriginInternal(QuotaManager* aQuotaManager,
                                       const PersistenceType aPersistenceType,
                                       const nsACString& aOrigin,
                                       const int64_t aTimestamp,
                                       const bool aPersisted,
                                       const uint64_t aUsage) {
  if (!mGetAll && aQuotaManager->IsOriginInternal(aOrigin)) {
    return;
  }

  // We can't store pointers to OriginUsage objects in the hashtable
  // since AppendElement() reallocates its internal array buffer as number
  // of elements grows.
  const auto& originUsage =
      mOriginUsagesIndex.WithEntryHandle(aOrigin, [&](auto&& entry) {
        if (entry) {
          return WrapNotNullUnchecked(&mOriginUsages[entry.Data()]);
        }

        entry.Insert(mOriginUsages.Length());

        return mOriginUsages.EmplaceBack(nsCString{aOrigin}, false, 0, 0);
      });

  if (aPersistenceType == PERSISTENCE_TYPE_DEFAULT) {
    originUsage->persisted() = aPersisted;
  }

  originUsage->usage() = originUsage->usage() + aUsage;

  originUsage->lastAccessed() =
      std::max<int64_t>(originUsage->lastAccessed(), aTimestamp);
}

const Atomic<bool>& GetUsageOp::GetIsCanceledFlag() {
  AssertIsOnIOThread();

  return mCanceled;
}

// XXX Remove aPersistent
// XXX Remove aPersistenceType once GetUsageForOrigin uses the persistence
// type from OriginMetadata
nsresult GetUsageOp::ProcessOrigin(QuotaManager& aQuotaManager,
                                   nsIFile& aOriginDir, const bool aPersistent,
                                   const PersistenceType aPersistenceType) {
  AssertIsOnIOThread();

  QM_TRY_UNWRAP(auto maybeMetadata,
                QM_OR_ELSE_WARN_IF(
                    // Expression
                    aQuotaManager.LoadFullOriginMetadataWithRestore(&aOriginDir)
                        .map([](auto metadata) -> Maybe<FullOriginMetadata> {
                          return Some(std::move(metadata));
                        }),
                    // Predicate.
                    IsSpecificError<NS_ERROR_MALFORMED_URI>,
                    // Fallback.
                    ErrToDefaultOk<Maybe<FullOriginMetadata>>));

  if (!maybeMetadata) {
    // Unknown directories during getting usage are allowed. Just warn if we
    // find them.
    QM_TRY_INSPECT(const auto& leafName,
                   MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsAutoString, aOriginDir,
                                                     GetLeafName));

    UNKNOWN_FILE_WARNING(leafName);
    return NS_OK;
  }

  auto metadata = maybeMetadata.extract();

  QM_TRY_INSPECT(const auto& usageInfo,
                 GetUsageForOrigin(aQuotaManager, aPersistenceType, metadata));

  ProcessOriginInternal(&aQuotaManager, aPersistenceType, metadata.mOrigin,
                        metadata.mLastAccessTime, metadata.mPersisted,
                        usageInfo.TotalUsage().valueOr(0));

  return NS_OK;
}

nsresult GetUsageOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("GetUsageOp::DoDirectoryWork", OTHER);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.EnsureStorageIsInitialized()));

  nsresult rv;

  for (const PersistenceType type : kAllPersistenceTypes) {
    rv = TraverseRepository(aQuotaManager, type);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // TraverseRepository above only consulted the filesystem. We also need to
  // consider origins which may have pending quota usage, such as buffered
  // LocalStorage writes for an origin which didn't previously have any
  // LocalStorage data.

  aQuotaManager.CollectPendingOriginsForListing(
      [this, &aQuotaManager](const auto& originInfo) {
        ProcessOriginInternal(
            &aQuotaManager, originInfo->GetGroupInfo()->GetPersistenceType(),
            originInfo->Origin(), originInfo->LockedAccessTime(),
            originInfo->LockedPersisted(), originInfo->LockedUsage());
      });

  return NS_OK;
}

void GetUsageOp::GetResponse(UsageRequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = AllUsageResponse();

  aResponse.get_AllUsageResponse().originUsages() = std::move(mOriginUsages);
}

GetOriginUsageOp::GetOriginUsageOp(const UsageRequestParams& aParams)
    : QuotaUsageRequestBase("dom::quota::GetOriginUsageOp"),
      mParams(aParams.get_OriginUsageParams()),
      mUsage(0),
      mFileUsage(0) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() == UsageRequestParams::TOriginUsageParams);

  // Overwrite GetOriginUsageOp default values.
  mFromMemory = mParams.fromMemory();
}

nsresult GetOriginUsageOp::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  QM_TRY_UNWRAP(
      PrincipalMetadata principalMetadata,
      aQuotaManager.GetInfoFromValidatedPrincipalInfo(mParams.principalInfo()));

  principalMetadata.AssertInvariants();

  mSuffix = std::move(principalMetadata.mSuffix);
  mGroup = std::move(principalMetadata.mGroup);
  mOriginScope.SetFromOrigin(principalMetadata.mOrigin);
  mStorageOrigin = std::move(principalMetadata.mStorageOrigin);
  mIsPrivate = principalMetadata.mIsPrivate;

  return NS_OK;
}

RefPtr<DirectoryLock> GetOriginUsageOp::CreateDirectoryLock() {
  if (mFromMemory) {
    return nullptr;
  }

  return QuotaUsageRequestBase::CreateDirectoryLock();
}

nsresult GetOriginUsageOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(mUsage == 0);
  MOZ_ASSERT(mFileUsage == 0);

  AUTO_PROFILER_LABEL("GetOriginUsageOp::DoDirectoryWork", OTHER);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.EnsureStorageIsInitialized()));

  if (mFromMemory) {
    const PrincipalMetadata principalMetadata = {
        mSuffix, mGroup, nsCString{mOriginScope.GetOrigin()}, mStorageOrigin,
        mIsPrivate};

    // Ensure temporary storage is initialized. If temporary storage hasn't been
    // initialized yet, the method will initialize it by traversing the
    // repositories for temporary and default storage (including our origin).
    QM_TRY(MOZ_TO_RESULT(aQuotaManager.EnsureTemporaryStorageIsInitialized()));

    // Get cached usage (the method doesn't have to stat any files). File usage
    // is not tracked in memory separately, so just add to the total usage.
    mUsage = aQuotaManager.GetOriginUsage(principalMetadata);

    return NS_OK;
  }

  UsageInfo usageInfo;

  // Add all the persistent/temporary/default storage files we care about.
  for (const PersistenceType type : kAllPersistenceTypes) {
    const OriginMetadata originMetadata = {
        mSuffix,        mGroup,     nsCString{mOriginScope.GetOrigin()},
        mStorageOrigin, mIsPrivate, type};

    auto usageInfoOrErr =
        GetUsageForOrigin(aQuotaManager, type, originMetadata);
    if (NS_WARN_IF(usageInfoOrErr.isErr())) {
      return usageInfoOrErr.unwrapErr();
    }

    usageInfo += usageInfoOrErr.unwrap();
  }

  mUsage = usageInfo.TotalUsage().valueOr(0);
  mFileUsage = usageInfo.FileUsage().valueOr(0);

  return NS_OK;
}

void GetOriginUsageOp::GetResponse(UsageRequestResponse& aResponse) {
  AssertIsOnOwningThread();

  OriginUsageResponse usageResponse;

  usageResponse.usage() = mUsage;
  usageResponse.fileUsage() = mFileUsage;

  aResponse = usageResponse;
}

StorageNameOp::StorageNameOp()
    : QuotaRequestBase("dom::quota::StorageNameOp", /* aExclusive */ false) {
  AssertIsOnOwningThread();
}

RefPtr<DirectoryLock> StorageNameOp::CreateDirectoryLock() { return nullptr; }

nsresult StorageNameOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("StorageNameOp::DoDirectoryWork", OTHER);

  mName = aQuotaManager.GetStorageName();

  return NS_OK;
}

void StorageNameOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  StorageNameResponse storageNameResponse;

  storageNameResponse.name() = mName;

  aResponse = storageNameResponse;
}

InitializedRequestBase::InitializedRequestBase(const char* aRunnableName)
    : QuotaRequestBase(aRunnableName, /* aExclusive */ false),
      mInitialized(false) {
  AssertIsOnOwningThread();
}

RefPtr<DirectoryLock> InitializedRequestBase::CreateDirectoryLock() {
  return nullptr;
}

nsresult StorageInitializedOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("StorageInitializedOp::DoDirectoryWork", OTHER);

  mInitialized = aQuotaManager.IsStorageInitialized();

  return NS_OK;
}

void StorageInitializedOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  StorageInitializedResponse storageInitializedResponse;

  storageInitializedResponse.initialized() = mInitialized;

  aResponse = storageInitializedResponse;
}

nsresult TemporaryStorageInitializedOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("TemporaryStorageInitializedOp::DoDirectoryWork", OTHER);

  mInitialized = aQuotaManager.IsTemporaryStorageInitialized();

  return NS_OK;
}

void TemporaryStorageInitializedOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  TemporaryStorageInitializedResponse temporaryStorageInitializedResponse;

  temporaryStorageInitializedResponse.initialized() = mInitialized;

  aResponse = temporaryStorageInitializedResponse;
}

InitOp::InitOp()
    : QuotaRequestBase("dom::quota::InitOp", /* aExclusive */ false) {
  AssertIsOnOwningThread();
}

nsresult InitOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitOp::DoDirectoryWork", OTHER);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.EnsureStorageIsInitialized()));

  return NS_OK;
}

void InitOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = InitResponse();
}

InitTemporaryStorageOp::InitTemporaryStorageOp()
    : QuotaRequestBase("dom::quota::InitTemporaryStorageOp",
                       /* aExclusive */ false) {
  AssertIsOnOwningThread();
}

nsresult InitTemporaryStorageOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitTemporaryStorageOp::DoDirectoryWork", OTHER);

  QM_TRY(OkIf(aQuotaManager.IsStorageInitialized()), NS_ERROR_NOT_INITIALIZED);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.EnsureTemporaryStorageIsInitialized()));

  return NS_OK;
}

void InitTemporaryStorageOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = InitTemporaryStorageResponse();
}

InitializeOriginRequestBase::InitializeOriginRequestBase(
    const char* aRunnableName, const PersistenceType aPersistenceType,
    const PrincipalInfo& aPrincipalInfo)
    : QuotaRequestBase(aRunnableName,
                       /* aExclusive */ false),
      mPrincipalInfo(aPrincipalInfo),
      mCreated(false) {
  AssertIsOnOwningThread();

  // Overwrite NormalOriginOperationBase default values.
  mPersistenceType.SetValue(aPersistenceType);
}

nsresult InitializeOriginRequestBase::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  QM_TRY_UNWRAP(
      auto principalMetadata,
      aQuotaManager.GetInfoFromValidatedPrincipalInfo(mPrincipalInfo));

  principalMetadata.AssertInvariants();

  mSuffix = std::move(principalMetadata.mSuffix);
  mGroup = std::move(principalMetadata.mGroup);
  mOriginScope.SetFromOrigin(principalMetadata.mOrigin);
  mStorageOrigin = std::move(principalMetadata.mStorageOrigin);
  mIsPrivate = principalMetadata.mIsPrivate;

  return NS_OK;
}

InitializePersistentOriginOp::InitializePersistentOriginOp(
    const RequestParams& aParams)
    : InitializeOriginRequestBase(
          "dom::quota::InitializePersistentOriginOp",
          PERSISTENCE_TYPE_PERSISTENT,
          aParams.get_InitializePersistentOriginParams().principalInfo()) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() ==
             RequestParams::TInitializePersistentOriginParams);
}

nsresult InitializePersistentOriginOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!mPersistenceType.IsNull());

  AUTO_PROFILER_LABEL("InitializePersistentOriginOp::DoDirectoryWork", OTHER);

  QM_TRY(OkIf(aQuotaManager.IsStorageInitialized()), NS_ERROR_NOT_INITIALIZED);

  QM_TRY_UNWRAP(
      mCreated,
      (aQuotaManager
           .EnsurePersistentOriginIsInitialized(OriginMetadata{
               mSuffix, mGroup, nsCString{mOriginScope.GetOrigin()},
               mStorageOrigin, mIsPrivate, PERSISTENCE_TYPE_PERSISTENT})
           .map([](const auto& res) { return res.second; })));

  return NS_OK;
}

void InitializePersistentOriginOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = InitializePersistentOriginResponse(mCreated);
}

InitializeTemporaryOriginOp::InitializeTemporaryOriginOp(
    const RequestParams& aParams)
    : InitializeOriginRequestBase(
          "dom::quota::InitializeTemporaryOriginOp",
          aParams.get_InitializeTemporaryOriginParams().persistenceType(),
          aParams.get_InitializeTemporaryOriginParams().principalInfo()) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() == RequestParams::TInitializeTemporaryOriginParams);
}

nsresult InitializeTemporaryOriginOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!mPersistenceType.IsNull());

  AUTO_PROFILER_LABEL("InitializeTemporaryOriginOp::DoDirectoryWork", OTHER);

  QM_TRY(OkIf(aQuotaManager.IsStorageInitialized()), NS_ERROR_NOT_INITIALIZED);

  QM_TRY(OkIf(aQuotaManager.IsTemporaryStorageInitialized()),
         NS_ERROR_NOT_INITIALIZED);

  QM_TRY_UNWRAP(
      mCreated,
      (aQuotaManager
           .EnsureTemporaryOriginIsInitialized(
               mPersistenceType.Value(),
               OriginMetadata{
                   mSuffix, mGroup, nsCString{mOriginScope.GetOrigin()},
                   mStorageOrigin, mIsPrivate, mPersistenceType.Value()})
           .map([](const auto& res) { return res.second; })));

  return NS_OK;
}

void InitializeTemporaryOriginOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = InitializeTemporaryOriginResponse(mCreated);
}

GetFullOriginMetadataOp::GetFullOriginMetadataOp(
    const GetFullOriginMetadataParams& aParams)
    : QuotaRequestBase("dom::quota::GetFullOriginMetadataOp",
                       /* aExclusive */ false),
      mParams(aParams) {
  AssertIsOnOwningThread();
}

nsresult GetFullOriginMetadataOp::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  QM_TRY_UNWRAP(
      PrincipalMetadata principalMetadata,
      aQuotaManager.GetInfoFromValidatedPrincipalInfo(mParams.principalInfo()));

  principalMetadata.AssertInvariants();

  mOriginMetadata = {std::move(principalMetadata), mParams.persistenceType()};

  return NS_OK;
}

RefPtr<DirectoryLock> GetFullOriginMetadataOp::CreateDirectoryLock() {
  return nullptr;
}

nsresult GetFullOriginMetadataOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("GetFullOriginMetadataOp::DoDirectoryWork", OTHER);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.EnsureStorageIsInitialized()));

  // Ensure temporary storage is initialized. If temporary storage hasn't
  // been initialized yet, the method will initialize it by traversing the
  // repositories for temporary and default storage (including our origin).
  QM_TRY(MOZ_TO_RESULT(aQuotaManager.EnsureTemporaryStorageIsInitialized()));

  // Get metadata cached in memory (the method doesn't have to stat any
  // files).
  mMaybeFullOriginMetadata =
      aQuotaManager.GetFullOriginMetadata(mOriginMetadata);

  return NS_OK;
}

void GetFullOriginMetadataOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = GetFullOriginMetadataResponse();
  aResponse.get_GetFullOriginMetadataResponse().maybeFullOriginMetadata() =
      std::move(mMaybeFullOriginMetadata);
}

ResetOrClearOp::ResetOrClearOp(bool aClear)
    : QuotaRequestBase("dom::quota::ResetOrClearOp", /* aExclusive */ true),
      mClear(aClear) {
  AssertIsOnOwningThread();
}

void ResetOrClearOp::DeleteFiles(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  nsresult rv = aQuotaManager.AboutToClearOrigins(Nullable<PersistenceType>(),
                                                  OriginScope::FromNull(),
                                                  Nullable<Client::Type>());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  auto directoryOrErr = QM_NewLocalFile(aQuotaManager.GetStoragePath());
  if (NS_WARN_IF(directoryOrErr.isErr())) {
    return;
  }

  nsCOMPtr<nsIFile> directory = directoryOrErr.unwrap();

  rv = directory->Remove(true);
  if (rv != NS_ERROR_FILE_NOT_FOUND && NS_FAILED(rv)) {
    // This should never fail if we've closed all storage connections
    // correctly...
    MOZ_ASSERT(false, "Failed to remove storage directory!");
  }
}

void ResetOrClearOp::DeleteStorageFile(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(const auto& storageFile,
                 QM_NewLocalFile(aQuotaManager.GetBasePath()), QM_VOID);

  QM_TRY(MOZ_TO_RESULT(storageFile->Append(aQuotaManager.GetStorageName() +
                                           kSQLiteSuffix)),
         QM_VOID);

  const nsresult rv = storageFile->Remove(true);
  if (rv != NS_ERROR_FILE_NOT_FOUND && NS_FAILED(rv)) {
    // This should never fail if we've closed the storage connection
    // correctly...
    MOZ_ASSERT(false, "Failed to remove storage file!");
  }
}

nsresult ResetOrClearOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("ResetOrClearOp::DoDirectoryWork", OTHER);

  if (mClear) {
    DeleteFiles(aQuotaManager);

    aQuotaManager.RemoveQuota();
  }

  aQuotaManager.ShutdownStorageInternal();

  if (mClear) {
    DeleteStorageFile(aQuotaManager);
  }

  return NS_OK;
}

void ResetOrClearOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();
  if (mClear) {
    aResponse = ClearAllResponse();
  } else {
    aResponse = ResetAllResponse();
  }
}

static Result<nsCOMPtr<nsIFile>, QMResult> OpenToBeRemovedDirectory(
    const nsAString& aStoragePath) {
  QM_TRY_INSPECT(const auto& dir,
                 QM_TO_RESULT_TRANSFORM(QM_NewLocalFile(aStoragePath)));
  QM_TRY(QM_TO_RESULT(dir->Append(u"to-be-removed"_ns)));

  nsresult rv = dir->Create(nsIFile::DIRECTORY_TYPE, 0700);
  if (NS_SUCCEEDED(rv) || rv == NS_ERROR_FILE_ALREADY_EXISTS) {
    return dir;
  }
  return Err(QMResult(rv));
}

static Result<Ok, QMResult> RemoveOrMoveToDir(nsIFile& aFile,
                                              nsIFile* aMoveTargetDir) {
  if (!aMoveTargetDir) {
    QM_TRY(QM_TO_RESULT(aFile.Remove(true)));
    return Ok();
  }

  nsIDToCString uuid(nsID::GenerateUUID());
  NS_ConvertUTF8toUTF16 subDirName(uuid.get(), NSID_LENGTH - 1);
  QM_TRY(QM_TO_RESULT(aFile.MoveTo(aMoveTargetDir, subDirName)));
  return Ok();
}

void ClearRequestBase::DeleteFiles(QuotaManager& aQuotaManager,
                                   PersistenceType aPersistenceType) {
  AssertIsOnIOThread();

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.AboutToClearOrigins(
             Nullable<PersistenceType>(aPersistenceType), mOriginScope,
             mClientType)),
         QM_VOID);

  QM_TRY_INSPECT(
      const auto& directory,
      QM_NewLocalFile(aQuotaManager.GetStoragePath(aPersistenceType)), QM_VOID);

  QM_TRY_INSPECT(const bool& exists,
                 MOZ_TO_RESULT_INVOKE_MEMBER(directory, Exists), QM_VOID);

  if (!exists) {
    return;
  }

  nsTArray<nsCOMPtr<nsIFile>> directoriesForRemovalRetry;

  aQuotaManager.MaybeRecordQuotaManagerShutdownStep(
      "ClearRequestBase: Starting deleting files"_ns);
  nsCOMPtr<nsIFile> toBeRemovedDir;
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownTeardown)) {
    QM_WARNONLY_TRY_UNWRAP(
        auto result, OpenToBeRemovedDirectory(aQuotaManager.GetStoragePath()));
    toBeRemovedDir = result.valueOr(nullptr);
  }
  QM_TRY(
      CollectEachFile(
          *directory,
          [&originScope = mOriginScope, aPersistenceType, &aQuotaManager,
           &directoriesForRemovalRetry, &toBeRemovedDir,
           this](nsCOMPtr<nsIFile>&& file) -> mozilla::Result<Ok, nsresult> {
            QM_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(*file));

            QM_TRY_INSPECT(const auto& leafName,
                           MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsAutoString, file,
                                                             GetLeafName));

            switch (dirEntryKind) {
              case nsIFileKind::ExistsAsDirectory: {
                QM_TRY_UNWRAP(
                    auto maybeMetadata,
                    QM_OR_ELSE_WARN_IF(
                        // Expression
                        aQuotaManager.GetOriginMetadata(file).map(
                            [](auto metadata) -> Maybe<OriginMetadata> {
                              return Some(std::move(metadata));
                            }),
                        // Predicate.
                        IsSpecificError<NS_ERROR_MALFORMED_URI>,
                        // Fallback.
                        ErrToDefaultOk<Maybe<OriginMetadata>>));

                if (!maybeMetadata) {
                  // Unknown directories during clearing are allowed. Just warn
                  // if we find them.
                  UNKNOWN_FILE_WARNING(leafName);
                  break;
                }

                auto metadata = maybeMetadata.extract();

                MOZ_ASSERT(metadata.mPersistenceType == aPersistenceType);

                // Skip the origin directory if it doesn't match the pattern.
                if (!originScope.Matches(
                        OriginScope::FromOrigin(metadata.mOrigin))) {
                  break;
                }

                if (!mClientType.IsNull()) {
                  nsAutoString clientDirectoryName;
                  QM_TRY(
                      OkIf(Client::TypeToText(mClientType.Value(),
                                              clientDirectoryName, fallible)),
                      Err(NS_ERROR_FAILURE));

                  QM_TRY(MOZ_TO_RESULT(file->Append(clientDirectoryName)));

                  QM_TRY_INSPECT(const bool& exists,
                                 MOZ_TO_RESULT_INVOKE_MEMBER(file, Exists));

                  if (!exists) {
                    break;
                  }
                }

                // We can't guarantee that this will always succeed on
                // Windows...
                QM_WARNONLY_TRY(
                    RemoveOrMoveToDir(*file, toBeRemovedDir), [&](const auto&) {
                      directoriesForRemovalRetry.AppendElement(std::move(file));
                    });

                const bool initialized =
                    aPersistenceType == PERSISTENCE_TYPE_PERSISTENT
                        ? aQuotaManager.IsOriginInitialized(metadata.mOrigin)
                        : aQuotaManager.IsTemporaryStorageInitialized();

                // If it hasn't been initialized, we don't need to update the
                // quota and notify the removing client.
                if (!initialized) {
                  break;
                }

                if (aPersistenceType != PERSISTENCE_TYPE_PERSISTENT) {
                  if (mClientType.IsNull()) {
                    aQuotaManager.RemoveQuotaForOrigin(aPersistenceType,
                                                       metadata);
                  } else {
                    aQuotaManager.ResetUsageForClient(
                        ClientMetadata{metadata, mClientType.Value()});
                  }
                }

                aQuotaManager.OriginClearCompleted(
                    aPersistenceType, metadata.mOrigin, mClientType);

                break;
              }

              case nsIFileKind::ExistsAsFile: {
                // Unknown files during clearing are allowed. Just warn if we
                // find them.
                if (!IsOSMetadata(leafName)) {
                  UNKNOWN_FILE_WARNING(leafName);
                }

                break;
              }

              case nsIFileKind::DoesNotExist:
                // Ignore files that got removed externally while iterating.
                break;
            }

            return Ok{};
          }),
      QM_VOID);

  // Retry removing any directories that failed to be removed earlier now.
  //
  // XXX This will still block this operation. We might instead dispatch a
  // runnable to our own thread for each retry round with a timer. We must
  // ensure that the directory lock is upheld until we complete or give up
  // though.
  for (uint32_t index = 0; index < 10; index++) {
    aQuotaManager.MaybeRecordQuotaManagerShutdownStepWith([index]() {
      return nsPrintfCString(
          "ClearRequestBase: Starting repeated directory removal #%d", index);
    });

    for (auto&& file : std::exchange(directoriesForRemovalRetry,
                                     nsTArray<nsCOMPtr<nsIFile>>{})) {
      QM_WARNONLY_TRY(
          RemoveOrMoveToDir(*file, toBeRemovedDir),
          ([&directoriesForRemovalRetry, &file](const auto&) {
            directoriesForRemovalRetry.AppendElement(std::move(file));
          }));
    }

    aQuotaManager.MaybeRecordQuotaManagerShutdownStepWith([index]() {
      return nsPrintfCString(
          "ClearRequestBase: Completed repeated directory removal #%d", index);
    });

    if (directoriesForRemovalRetry.IsEmpty()) {
      break;
    }

    aQuotaManager.MaybeRecordQuotaManagerShutdownStepWith([index]() {
      return nsPrintfCString("ClearRequestBase: Before sleep #%d", index);
    });

    PR_Sleep(PR_MillisecondsToInterval(200));

    aQuotaManager.MaybeRecordQuotaManagerShutdownStepWith([index]() {
      return nsPrintfCString("ClearRequestBase: After sleep #%d", index);
    });
  }

  QM_WARNONLY_TRY(OkIf(directoriesForRemovalRetry.IsEmpty()));

  aQuotaManager.MaybeRecordQuotaManagerShutdownStep(
      "ClearRequestBase: Completed deleting files"_ns);
}

nsresult ClearRequestBase::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("ClearRequestBase::DoDirectoryWork", OTHER);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.EnsureStorageIsInitialized()));

  if (mPersistenceType.IsNull()) {
    for (const PersistenceType type : kAllPersistenceTypes) {
      DeleteFiles(aQuotaManager, type);
    }
  } else {
    DeleteFiles(aQuotaManager, mPersistenceType.Value());
  }

  return NS_OK;
}

ClearOriginOp::ClearOriginOp(const RequestParams& aParams)
    : ClearRequestBase("dom::quota::ClearOriginOp", /* aExclusive */ true),
      mParams(aParams.get_ClearOriginParams().commonParams()),
      mMatchAll(aParams.get_ClearOriginParams().matchAll()) {
  MOZ_ASSERT(aParams.type() == RequestParams::TClearOriginParams);

  if (mParams.persistenceTypeIsExplicit()) {
    mPersistenceType.SetValue(mParams.persistenceType());
  }

  // Figure out which origin we're dealing with.
  const auto origin = QuotaManager::GetOriginFromValidatedPrincipalInfo(
      mParams.principalInfo());

  if (mMatchAll) {
    mOriginScope.SetFromPrefix(origin);
  } else {
    mOriginScope.SetFromOrigin(origin);
  }

  if (mParams.clientTypeIsExplicit()) {
    mClientType.SetValue(mParams.clientType());
  }
}

void ClearOriginOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = ClearOriginResponse();
}

ClearDataOp::ClearDataOp(const RequestParams& aParams)
    : ClearRequestBase("dom::quota::ClearDataOp", /* aExclusive */ true),
      mParams(aParams) {
  MOZ_ASSERT(aParams.type() == RequestParams::TClearDataParams);

  mOriginScope.SetFromPattern(mParams.pattern());
}

void ClearDataOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = ClearDataResponse();
}

ResetOriginOp::ResetOriginOp(const RequestParams& aParams)
    : QuotaRequestBase("dom::quota::ResetOriginOp", /* aExclusive */ true) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() == RequestParams::TResetOriginParams);

  const ClearResetOriginParams& params =
      aParams.get_ResetOriginParams().commonParams();

  const auto origin =
      QuotaManager::GetOriginFromValidatedPrincipalInfo(params.principalInfo());

  // Overwrite NormalOriginOperationBase default values.
  if (params.persistenceTypeIsExplicit()) {
    mPersistenceType.SetValue(params.persistenceType());
  }

  mOriginScope.SetFromOrigin(origin);

  if (params.clientTypeIsExplicit()) {
    mClientType.SetValue(params.clientType());
  }
}

nsresult ResetOriginOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("ResetOriginOp::DoDirectoryWork", OTHER);

  // All the work is handled by NormalOriginOperationBase parent class. In this
  // particular case, we just needed to acquire an exclusive directory lock and
  // that's it.

  return NS_OK;
}

void ResetOriginOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = ResetOriginResponse();
}

PersistRequestBase::PersistRequestBase(const PrincipalInfo& aPrincipalInfo)
    : QuotaRequestBase("dom::quota::PersistRequestBase",
                       /* aExclusive */ false),
      mPrincipalInfo(aPrincipalInfo) {
  AssertIsOnOwningThread();

  mPersistenceType.SetValue(PERSISTENCE_TYPE_DEFAULT);
}

nsresult PersistRequestBase::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  // Figure out which origin we're dealing with.
  QM_TRY_UNWRAP(
      PrincipalMetadata principalMetadata,
      aQuotaManager.GetInfoFromValidatedPrincipalInfo(mPrincipalInfo));

  principalMetadata.AssertInvariants();

  mSuffix = std::move(principalMetadata.mSuffix);
  mGroup = std::move(principalMetadata.mGroup);
  mOriginScope.SetFromOrigin(principalMetadata.mOrigin);
  mStorageOrigin = std::move(principalMetadata.mStorageOrigin);
  mIsPrivate = principalMetadata.mIsPrivate;

  return NS_OK;
}

PersistedOp::PersistedOp(const RequestParams& aParams)
    : PersistRequestBase(aParams.get_PersistedParams().principalInfo()),
      mPersisted(false) {
  MOZ_ASSERT(aParams.type() == RequestParams::TPersistedParams);
}

nsresult PersistedOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!mPersistenceType.IsNull());
  MOZ_ASSERT(mPersistenceType.Value() == PERSISTENCE_TYPE_DEFAULT);
  MOZ_ASSERT(mOriginScope.IsOrigin());

  AUTO_PROFILER_LABEL("PersistedOp::DoDirectoryWork", OTHER);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.EnsureStorageIsInitialized()));

  const OriginMetadata originMetadata = {
      mSuffix,        mGroup,     nsCString{mOriginScope.GetOrigin()},
      mStorageOrigin, mIsPrivate, mPersistenceType.Value()};

  Nullable<bool> persisted = aQuotaManager.OriginPersisted(originMetadata);

  if (!persisted.IsNull()) {
    mPersisted = persisted.Value();
    return NS_OK;
  }

  // If we get here, it means the origin hasn't been initialized yet.
  // Try to get the persisted flag from directory metadata on disk.

  QM_TRY_INSPECT(const auto& directory,
                 aQuotaManager.GetOriginDirectory(originMetadata));

  QM_TRY_INSPECT(const bool& exists,
                 MOZ_TO_RESULT_INVOKE_MEMBER(directory, Exists));

  if (exists) {
    // Get the metadata. We only use the persisted flag.
    QM_TRY_INSPECT(const auto& metadata,
                   aQuotaManager.LoadFullOriginMetadataWithRestore(directory));

    mPersisted = metadata.mPersisted;
  } else {
    // The directory has not been created yet.
    mPersisted = false;
  }

  return NS_OK;
}

void PersistedOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  PersistedResponse persistedResponse;
  persistedResponse.persisted() = mPersisted;

  aResponse = persistedResponse;
}

PersistOp::PersistOp(const RequestParams& aParams)
    : PersistRequestBase(aParams.get_PersistParams().principalInfo()) {
  MOZ_ASSERT(aParams.type() == RequestParams::TPersistParams);
}

nsresult PersistOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!mPersistenceType.IsNull());
  MOZ_ASSERT(mPersistenceType.Value() == PERSISTENCE_TYPE_DEFAULT);
  MOZ_ASSERT(mOriginScope.IsOrigin());

  const OriginMetadata originMetadata = {
      mSuffix,        mGroup,     nsCString{mOriginScope.GetOrigin()},
      mStorageOrigin, mIsPrivate, mPersistenceType.Value()};

  AUTO_PROFILER_LABEL("PersistOp::DoDirectoryWork", OTHER);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.EnsureStorageIsInitialized()));

  // Update directory metadata on disk first. Then, create/update the originInfo
  // if needed.
  QM_TRY_INSPECT(const auto& directory,
                 aQuotaManager.GetOriginDirectory(originMetadata));

  QM_TRY_INSPECT(const bool& created,
                 aQuotaManager.EnsureOriginDirectory(*directory));

  if (created) {
    int64_t timestamp;

    // Origin directory has been successfully created.
    // Create OriginInfo too if temporary storage was already initialized.
    if (aQuotaManager.IsTemporaryStorageInitialized()) {
      timestamp = aQuotaManager.NoteOriginDirectoryCreated(
          originMetadata, /* aPersisted */ true);
    } else {
      timestamp = PR_Now();
    }

    QM_TRY(MOZ_TO_RESULT(QuotaManager::CreateDirectoryMetadata2(
        *directory, timestamp,
        /* aPersisted */ true, originMetadata)));
  } else {
    // Get the metadata (restore the metadata file if necessary). We only use
    // the persisted flag.
    QM_TRY_INSPECT(const auto& metadata,
                   aQuotaManager.LoadFullOriginMetadataWithRestore(directory));

    if (!metadata.mPersisted) {
      QM_TRY_INSPECT(const auto& file,
                     CloneFileAndAppend(
                         *directory, nsLiteralString(METADATA_V2_FILE_NAME)));

      QM_TRY_INSPECT(const auto& stream,
                     GetBinaryOutputStream(*file, FileFlag::Update));

      MOZ_ASSERT(stream);

      // Update origin access time while we are here.
      QM_TRY(MOZ_TO_RESULT(stream->Write64(PR_Now())));

      // Set the persisted flag to true.
      QM_TRY(MOZ_TO_RESULT(stream->WriteBoolean(true)));
    }

    // Directory metadata has been successfully updated.
    // Update OriginInfo too if temporary storage was already initialized.
    if (aQuotaManager.IsTemporaryStorageInitialized()) {
      aQuotaManager.PersistOrigin(originMetadata);
    }
  }

  return NS_OK;
}

void PersistOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = PersistResponse();
}

EstimateOp::EstimateOp(const EstimateParams& aParams)
    : QuotaRequestBase("dom::quota::EstimateOp", /* aExclusive */ false),
      mParams(aParams) {
  AssertIsOnOwningThread();
}

nsresult EstimateOp::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  QM_TRY_UNWRAP(
      PrincipalMetadata principalMetadata,
      aQuotaManager.GetInfoFromValidatedPrincipalInfo(mParams.principalInfo()));

  principalMetadata.AssertInvariants();

  mOriginMetadata = {std::move(principalMetadata), PERSISTENCE_TYPE_DEFAULT};

  return NS_OK;
}

RefPtr<DirectoryLock> EstimateOp::CreateDirectoryLock() { return nullptr; }

nsresult EstimateOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("EstimateOp::DoDirectoryWork", OTHER);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.EnsureStorageIsInitialized()));

  // Ensure temporary storage is initialized. If temporary storage hasn't been
  // initialized yet, the method will initialize it by traversing the
  // repositories for temporary and default storage (including origins belonging
  // to our group).
  QM_TRY(MOZ_TO_RESULT(aQuotaManager.EnsureTemporaryStorageIsInitialized()));

  // Get cached usage (the method doesn't have to stat any files).
  mUsageAndLimit = aQuotaManager.GetUsageAndLimitForEstimate(mOriginMetadata);

  return NS_OK;
}

void EstimateOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  EstimateResponse estimateResponse;

  estimateResponse.usage() = mUsageAndLimit.first;
  estimateResponse.limit() = mUsageAndLimit.second;

  aResponse = estimateResponse;
}

ListOriginsOp::ListOriginsOp()
    : QuotaRequestBase("dom::quota::ListOriginsOp", /* aExclusive */ false),
      TraverseRepositoryHelper() {
  AssertIsOnOwningThread();
}

nsresult ListOriginsOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("ListOriginsOp::DoDirectoryWork", OTHER);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.EnsureStorageIsInitialized()));

  for (const PersistenceType type : kAllPersistenceTypes) {
    QM_TRY(MOZ_TO_RESULT(TraverseRepository(aQuotaManager, type)));
  }

  // TraverseRepository above only consulted the file-system to get a list of
  // known origins, but we also need to include origins that have pending quota
  // usage.

  aQuotaManager.CollectPendingOriginsForListing([this](const auto& originInfo) {
    mOrigins.AppendElement(originInfo->Origin());
  });

  return NS_OK;
}

const Atomic<bool>& ListOriginsOp::GetIsCanceledFlag() {
  AssertIsOnIOThread();

  return mCanceled;
}

nsresult ListOriginsOp::ProcessOrigin(QuotaManager& aQuotaManager,
                                      nsIFile& aOriginDir,
                                      const bool aPersistent,
                                      const PersistenceType aPersistenceType) {
  AssertIsOnIOThread();

  QM_TRY_UNWRAP(auto maybeMetadata,
                QM_OR_ELSE_WARN_IF(
                    // Expression
                    aQuotaManager.GetOriginMetadata(&aOriginDir)
                        .map([](auto metadata) -> Maybe<OriginMetadata> {
                          return Some(std::move(metadata));
                        }),
                    // Predicate.
                    IsSpecificError<NS_ERROR_MALFORMED_URI>,
                    // Fallback.
                    ErrToDefaultOk<Maybe<OriginMetadata>>));

  if (!maybeMetadata) {
    // Unknown directories during listing are allowed. Just warn if we find
    // them.
    QM_TRY_INSPECT(const auto& leafName,
                   MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsAutoString, aOriginDir,
                                                     GetLeafName));

    UNKNOWN_FILE_WARNING(leafName);
    return NS_OK;
  }

  auto metadata = maybeMetadata.extract();

  if (aQuotaManager.IsOriginInternal(metadata.mOrigin)) {
    return NS_OK;
  }

  mOrigins.AppendElement(std::move(metadata.mOrigin));

  return NS_OK;
}

void ListOriginsOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = ListOriginsResponse();
  if (mOrigins.IsEmpty()) {
    return;
  }

  nsTArray<nsCString>& origins = aResponse.get_ListOriginsResponse().origins();
  mOrigins.SwapElements(origins);
}

}  // namespace mozilla::dom::quota
