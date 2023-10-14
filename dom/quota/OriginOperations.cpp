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
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/NotNull.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/dom/Nullable.h"
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

template <class Base>
class OpenStorageDirectoryHelper : public Base {
 protected:
  OpenStorageDirectoryHelper(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                             const char* aName)
      : Base(std::move(aQuotaManager), aName) {}

  RefPtr<BoolPromise> OpenStorageDirectory(
      const Nullable<PersistenceType>& aPersistenceType,
      const OriginScope& aOriginScope,
      const Nullable<Client::Type>& aClientType, bool aExclusive);

  RefPtr<UniversalDirectoryLock> mDirectoryLock;
};

class FinalizeOriginEvictionOp : public OriginOperationBase {
  nsTArray<RefPtr<OriginDirectoryLock>> mLocks;

 public:
  FinalizeOriginEvictionOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                           nsTArray<RefPtr<OriginDirectoryLock>>&& aLocks)
      : OriginOperationBase(std::move(aQuotaManager),
                            "dom::quota::FinalizeOriginEvictionOp"),
        mLocks(std::move(aLocks)) {
    AssertIsOnOwningThread();
  }

 private:
  ~FinalizeOriginEvictionOp() = default;

  virtual RefPtr<BoolPromise> Open() override;

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  virtual void UnblockOpen() override;
};

class SaveOriginAccessTimeOp
    : public OpenStorageDirectoryHelper<NormalOriginOperationBase> {
  const OriginMetadata mOriginMetadata;
  int64_t mTimestamp;

 public:
  SaveOriginAccessTimeOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                         const OriginMetadata& aOriginMetadata,
                         int64_t aTimestamp)
      : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                   "dom::quota::SaveOriginAccessTimeOp"),
        mOriginMetadata(aOriginMetadata),
        mTimestamp(aTimestamp) {
    AssertIsOnOwningThread();
  }

 private:
  ~SaveOriginAccessTimeOp() = default;

  RefPtr<BoolPromise> OpenDirectory() override;

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  virtual void SendResults() override;

  void CloseDirectory() override;
};

class ClearPrivateRepositoryOp
    : public OpenStorageDirectoryHelper<ResolvableNormalOriginOp<bool>> {
 public:
  explicit ClearPrivateRepositoryOp(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager)
      : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                   "dom::quota::ClearPrivateRepositoryOp") {
    AssertIsOnOwningThread();
  }

 private:
  ~ClearPrivateRepositoryOp() = default;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool GetResolveValue() override { return true; }

  void CloseDirectory() override;
};

class ShutdownStorageOp : public ResolvableNormalOriginOp<bool> {
  RefPtr<UniversalDirectoryLock> mDirectoryLock;

 public:
  explicit ShutdownStorageOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager)
      : ResolvableNormalOriginOp(std::move(aQuotaManager),
                                 "dom::quota::ShutdownStorageOp") {
    AssertIsOnOwningThread();
  }

 private:
  ~ShutdownStorageOp() = default;

#ifdef DEBUG
  nsresult DirectoryOpen() override;
#endif

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool GetResolveValue() override { return true; }

  void CloseDirectory() override;
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

class GetUsageOp final
    : public OpenStorageDirectoryHelper<QuotaUsageRequestBase>,
      public TraverseRepositoryHelper {
  nsTArray<OriginUsage> mOriginUsages;
  nsTHashMap<nsCStringHashKey, uint32_t> mOriginUsagesIndex;

  bool mGetAll;

 public:
  GetUsageOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
             const UsageRequestParams& aParams);

 private:
  ~GetUsageOp() = default;

  void ProcessOriginInternal(QuotaManager* aQuotaManager,
                             const PersistenceType aPersistenceType,
                             const nsACString& aOrigin,
                             const int64_t aTimestamp, const bool aPersisted,
                             const uint64_t aUsage);

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  const Atomic<bool>& GetIsCanceledFlag() override;

  nsresult ProcessOrigin(QuotaManager& aQuotaManager, nsIFile& aOriginDir,
                         const bool aPersistent,
                         const PersistenceType aPersistenceType) override;

  void GetResponse(UsageRequestResponse& aResponse) override;

  void CloseDirectory() override;
};

class GetOriginUsageOp final
    : public OpenStorageDirectoryHelper<QuotaUsageRequestBase> {
  const OriginUsageParams mParams;
  PrincipalMetadata mPrincipalMetadata;
  UsageInfo mUsageInfo;
  bool mFromMemory;

 public:
  GetOriginUsageOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                   const UsageRequestParams& aParams);

 private:
  ~GetOriginUsageOp() = default;

  nsresult DoInit(QuotaManager& aQuotaManager) override;

  RefPtr<BoolPromise> OpenDirectory() override;

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(UsageRequestResponse& aResponse) override;

  void CloseDirectory() override;
};

class StorageNameOp final : public QuotaRequestBase {
  nsString mName;

 public:
  explicit StorageNameOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager);

 private:
  ~StorageNameOp() = default;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;

  void CloseDirectory() override;
};

class InitializedRequestBase : public QuotaRequestBase {
 protected:
  bool mInitialized;

  InitializedRequestBase(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                         const char* aName);

 private:
  RefPtr<BoolPromise> OpenDirectory() override;

  void CloseDirectory() override;
};

class StorageInitializedOp final : public InitializedRequestBase {
 public:
  explicit StorageInitializedOp(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager)
      : InitializedRequestBase(std::move(aQuotaManager),
                               "dom::quota::StorageInitializedOp") {}

 private:
  ~StorageInitializedOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class TemporaryStorageInitializedOp final : public InitializedRequestBase {
 public:
  explicit TemporaryStorageInitializedOp(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager)
      : InitializedRequestBase(std::move(aQuotaManager),
                               "dom::quota::StorageInitializedOp") {}

 private:
  ~TemporaryStorageInitializedOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class InitOp final : public ResolvableNormalOriginOp<bool> {
  RefPtr<UniversalDirectoryLock> mDirectoryLock;

 public:
  InitOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
         RefPtr<UniversalDirectoryLock> aDirectoryLock);

 private:
  ~InitOp() = default;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool GetResolveValue() override;

  void CloseDirectory() override;
};

class InitTemporaryStorageOp final : public QuotaRequestBase {
  RefPtr<UniversalDirectoryLock> mDirectoryLock;

 public:
  explicit InitTemporaryStorageOp(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager);

 private:
  ~InitTemporaryStorageOp() = default;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;

  void CloseDirectory() override;
};

class InitializeOriginRequestBase : public QuotaRequestBase {
 protected:
  const PrincipalInfo mPrincipalInfo;
  PrincipalMetadata mPrincipalMetadata;
  RefPtr<UniversalDirectoryLock> mDirectoryLock;
  const PersistenceType mPersistenceType;
  bool mCreated;

  InitializeOriginRequestBase(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                              const char* aName,
                              PersistenceType aPersistenceType,
                              const PrincipalInfo& aPrincipalInfo);

  nsresult DoInit(QuotaManager& aQuotaManager) override;

 private:
  RefPtr<BoolPromise> OpenDirectory() override;

  void CloseDirectory() override;
};

class InitializePersistentOriginOp final : public InitializeOriginRequestBase {
 public:
  InitializePersistentOriginOp(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      const RequestParams& aParams);

 private:
  ~InitializePersistentOriginOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class InitializeTemporaryOriginOp final : public InitializeOriginRequestBase {
 public:
  InitializeTemporaryOriginOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                              const RequestParams& aParams);

 private:
  ~InitializeTemporaryOriginOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class InitializeClientBase : public ResolvableNormalOriginOp<bool> {
 protected:
  const PrincipalInfo mPrincipalInfo;
  ClientMetadata mClientMetadata;
  RefPtr<UniversalDirectoryLock> mDirectoryLock;
  const PersistenceType mPersistenceType;
  const Client::Type mClientType;
  bool mCreated;

  InitializeClientBase(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                       const char* aName, PersistenceType aPersistenceType,
                       const PrincipalInfo& aPrincipalInfo,
                       Client::Type aClientType);

  nsresult DoInit(QuotaManager& aQuotaManager) override;

 private:
  RefPtr<BoolPromise> OpenDirectory() override;

  void CloseDirectory() override;
};

class InitializePersistentClientOp : public InitializeClientBase {
 public:
  InitializePersistentClientOp(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      const PrincipalInfo& aPrincipalInfo, Client::Type aClientType);

 private:
  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool GetResolveValue() override;
};

class InitializeTemporaryClientOp : public InitializeClientBase {
 public:
  InitializeTemporaryClientOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                              PersistenceType aPersistenceType,
                              const PrincipalInfo& aPrincipalInfo,
                              Client::Type aClientType);

 private:
  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool GetResolveValue() override;
};

class GetFullOriginMetadataOp
    : public OpenStorageDirectoryHelper<QuotaRequestBase> {
  const GetFullOriginMetadataParams mParams;
  // XXX Consider wrapping with LazyInitializedOnce
  OriginMetadata mOriginMetadata;
  Maybe<FullOriginMetadata> mMaybeFullOriginMetadata;

 public:
  GetFullOriginMetadataOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                          const GetFullOriginMetadataParams& aParams);

 private:
  nsresult DoInit(QuotaManager& aQuotaManager) override;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;

  void CloseDirectory() override;
};

class ClearStorageOp final
    : public OpenStorageDirectoryHelper<ResolvableNormalOriginOp<bool>> {
 public:
  explicit ClearStorageOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager);

 private:
  ~ClearStorageOp() = default;

  void DeleteFiles(QuotaManager& aQuotaManager);

  void DeleteStorageFile(QuotaManager& aQuotaManager);

  RefPtr<BoolPromise> OpenDirectory() override;

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool GetResolveValue() override;

  void CloseDirectory() override;
};

class ClearRequestBase
    : public OpenStorageDirectoryHelper<ResolvableNormalOriginOp<bool>> {
 protected:
  ClearRequestBase(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                   const char* aName)
      : OpenStorageDirectoryHelper(std::move(aQuotaManager), aName) {
    AssertIsOnOwningThread();
  }

  void DeleteFiles(QuotaManager& aQuotaManager,
                   const OriginMetadata& aOriginMetadata,
                   const Nullable<Client::Type>& aClientType);

  void DeleteFiles(QuotaManager& aQuotaManager,
                   PersistenceType aPersistenceType,
                   const OriginScope& aOriginScope,
                   const Nullable<Client::Type>& aClientType);

 private:
  template <typename FileCollector>
  void DeleteFilesInternal(QuotaManager& aQuotaManager,
                           PersistenceType aPersistenceType,
                           const OriginScope& aOriginScope,
                           const Nullable<Client::Type>& aClientType,
                           const FileCollector& aFileCollector);
};

class ClearOriginOp final : public ClearRequestBase {
  const PrincipalInfo mPrincipalInfo;
  PrincipalMetadata mPrincipalMetadata;
  const Nullable<PersistenceType> mPersistenceType;
  const Nullable<Client::Type> mClientType;

 public:
  ClearOriginOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                const mozilla::Maybe<PersistenceType>& aPersistenceType,
                const PrincipalInfo& aPrincipalInfo,
                const mozilla::Maybe<Client::Type>& aClientType);

 private:
  ~ClearOriginOp() = default;

  nsresult DoInit(QuotaManager& aQuotaManager) override;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool GetResolveValue() override;

  void CloseDirectory() override;
};

class ClearStoragesForOriginPrefixOp final
    : public OpenStorageDirectoryHelper<ClearRequestBase> {
  const nsCString mPrefix;
  const Nullable<PersistenceType> mPersistenceType;

 public:
  ClearStoragesForOriginPrefixOp(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      const Maybe<PersistenceType>& aPersistenceType,
      const PrincipalInfo& aPrincipalInfo);

 private:
  ~ClearStoragesForOriginPrefixOp() = default;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool GetResolveValue() override;

  void CloseDirectory() override;
};

class ClearDataOp final : public ClearRequestBase {
  const OriginAttributesPattern mPattern;

 public:
  ClearDataOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
              const OriginAttributesPattern& aPattern);

 private:
  ~ClearDataOp() = default;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool GetResolveValue() override;

  void CloseDirectory() override;
};

class ResetOriginOp final : public QuotaRequestBase {
  nsCString mOrigin;
  RefPtr<UniversalDirectoryLock> mDirectoryLock;
  Nullable<PersistenceType> mPersistenceType;
  Nullable<Client::Type> mClientType;

 public:
  ResetOriginOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                const RequestParams& aParams);

 private:
  ~ResetOriginOp() = default;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;

  void CloseDirectory() override;
};

class PersistRequestBase : public OpenStorageDirectoryHelper<QuotaRequestBase> {
  const PrincipalInfo mPrincipalInfo;

 protected:
  PrincipalMetadata mPrincipalMetadata;

 protected:
  PersistRequestBase(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                     const PrincipalInfo& aPrincipalInfo);

  nsresult DoInit(QuotaManager& aQuotaManager) override;

 private:
  RefPtr<BoolPromise> OpenDirectory() override;

  void CloseDirectory() override;
};

class PersistedOp final : public PersistRequestBase {
  bool mPersisted;

 public:
  PersistedOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
              const RequestParams& aParams);

 private:
  ~PersistedOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class PersistOp final : public PersistRequestBase {
 public:
  PersistOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
            const RequestParams& aParams);

 private:
  ~PersistOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class EstimateOp final : public OpenStorageDirectoryHelper<QuotaRequestBase> {
  const EstimateParams mParams;
  OriginMetadata mOriginMetadata;
  std::pair<uint64_t, uint64_t> mUsageAndLimit;

 public:
  EstimateOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
             const EstimateParams& aParams);

 private:
  ~EstimateOp() = default;

  nsresult DoInit(QuotaManager& aQuotaManager) override;

  RefPtr<BoolPromise> OpenDirectory() override;

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;

  void CloseDirectory() override;
};

class ListOriginsOp final : public OpenStorageDirectoryHelper<QuotaRequestBase>,
                            public TraverseRepositoryHelper {
  // XXX Bug 1521541 will make each origin has it's own state.
  nsTArray<nsCString> mOrigins;

 public:
  explicit ListOriginsOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager);

 private:
  ~ListOriginsOp() = default;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  const Atomic<bool>& GetIsCanceledFlag() override;

  nsresult ProcessOrigin(QuotaManager& aQuotaManager, nsIFile& aOriginDir,
                         const bool aPersistent,
                         const PersistenceType aPersistenceType) override;

  void GetResponse(RequestResponse& aResponse) override;

  void CloseDirectory() override;
};

RefPtr<OriginOperationBase> CreateFinalizeOriginEvictionOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    nsTArray<RefPtr<OriginDirectoryLock>>&& aLocks) {
  return MakeRefPtr<FinalizeOriginEvictionOp>(std::move(aQuotaManager),
                                              std::move(aLocks));
}

RefPtr<NormalOriginOperationBase> CreateSaveOriginAccessTimeOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const OriginMetadata& aOriginMetadata, int64_t aTimestamp) {
  return MakeRefPtr<SaveOriginAccessTimeOp>(std::move(aQuotaManager),
                                            aOriginMetadata, aTimestamp);
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateClearPrivateRepositoryOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager) {
  return MakeRefPtr<ClearPrivateRepositoryOp>(std::move(aQuotaManager));
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateShutdownStorageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager) {
  return MakeRefPtr<ShutdownStorageOp>(std::move(aQuotaManager));
}

RefPtr<QuotaUsageRequestBase> CreateGetUsageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const UsageRequestParams& aParams) {
  return MakeRefPtr<GetUsageOp>(std::move(aQuotaManager), aParams);
}

RefPtr<QuotaUsageRequestBase> CreateGetOriginUsageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const UsageRequestParams& aParams) {
  return MakeRefPtr<GetOriginUsageOp>(std::move(aQuotaManager), aParams);
}

RefPtr<QuotaRequestBase> CreateStorageNameOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager) {
  return MakeRefPtr<StorageNameOp>(std::move(aQuotaManager));
}

RefPtr<QuotaRequestBase> CreateStorageInitializedOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager) {
  return MakeRefPtr<StorageInitializedOp>(std::move(aQuotaManager));
}

RefPtr<QuotaRequestBase> CreateTemporaryStorageInitializedOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager) {
  return MakeRefPtr<TemporaryStorageInitializedOp>(std::move(aQuotaManager));
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateInitOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    RefPtr<UniversalDirectoryLock> aDirectoryLock) {
  return MakeRefPtr<InitOp>(std::move(aQuotaManager),
                            std::move(aDirectoryLock));
}

RefPtr<QuotaRequestBase> CreateInitTemporaryStorageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager) {
  return MakeRefPtr<InitTemporaryStorageOp>(std::move(aQuotaManager));
}

RefPtr<QuotaRequestBase> CreateInitializePersistentOriginOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const RequestParams& aParams) {
  return MakeRefPtr<InitializePersistentOriginOp>(std::move(aQuotaManager),
                                                  aParams);
}

RefPtr<QuotaRequestBase> CreateInitializeTemporaryOriginOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const RequestParams& aParams) {
  return MakeRefPtr<InitializeTemporaryOriginOp>(std::move(aQuotaManager),
                                                 aParams);
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateInitializePersistentClientOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
    const Client::Type aClientType) {
  return MakeRefPtr<InitializePersistentClientOp>(std::move(aQuotaManager),
                                                  aPrincipalInfo, aClientType);
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateInitializeTemporaryClientOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const PersistenceType aPersistenceType, const PrincipalInfo& aPrincipalInfo,
    const Client::Type aClientType) {
  return MakeRefPtr<InitializeTemporaryClientOp>(
      std::move(aQuotaManager), aPersistenceType, aPrincipalInfo, aClientType);
}

RefPtr<QuotaRequestBase> CreateGetFullOriginMetadataOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const GetFullOriginMetadataParams& aParams) {
  return MakeRefPtr<GetFullOriginMetadataOp>(std::move(aQuotaManager), aParams);
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateClearStorageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager) {
  return MakeRefPtr<ClearStorageOp>(std::move(aQuotaManager));
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateClearOriginOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const Maybe<PersistenceType>& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo,
    const Maybe<Client::Type>& aClientType) {
  return MakeRefPtr<ClearOriginOp>(std::move(aQuotaManager), aPersistenceType,
                                   aPrincipalInfo, aClientType);
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateClearStoragesForOriginPrefixOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const Maybe<PersistenceType>& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo) {
  return MakeRefPtr<ClearStoragesForOriginPrefixOp>(
      std::move(aQuotaManager), aPersistenceType, aPrincipalInfo);
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateClearDataOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const OriginAttributesPattern& aPattern) {
  return MakeRefPtr<ClearDataOp>(std::move(aQuotaManager), aPattern);
}

RefPtr<QuotaRequestBase> CreateResetOriginOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const RequestParams& aParams) {
  return MakeRefPtr<ResetOriginOp>(std::move(aQuotaManager), aParams);
}

RefPtr<QuotaRequestBase> CreatePersistedOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const RequestParams& aParams) {
  return MakeRefPtr<PersistedOp>(std::move(aQuotaManager), aParams);
}

RefPtr<QuotaRequestBase> CreatePersistOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const RequestParams& aParams) {
  return MakeRefPtr<PersistOp>(std::move(aQuotaManager), aParams);
}

RefPtr<QuotaRequestBase> CreateEstimateOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const EstimateParams& aParams) {
  return MakeRefPtr<EstimateOp>(std::move(aQuotaManager), aParams);
}

RefPtr<QuotaRequestBase> CreateListOriginsOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager) {
  return MakeRefPtr<ListOriginsOp>(std::move(aQuotaManager));
}

template <class Base>
RefPtr<BoolPromise> OpenStorageDirectoryHelper<Base>::OpenStorageDirectory(
    const Nullable<PersistenceType>& aPersistenceType,
    const OriginScope& aOriginScope, const Nullable<Client::Type>& aClientType,
    bool aExclusive) {
  return Base::mQuotaManager
      ->OpenStorageDirectory(aPersistenceType, aOriginScope, aClientType,
                             aExclusive)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = RefPtr(this)](
                 UniversalDirectoryLockPromise::ResolveOrRejectValue&& aValue) {
               if (aValue.IsReject()) {
                 return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                     __func__);
               }

               self->mDirectoryLock = std::move(aValue.ResolveValue());

               return BoolPromise::CreateAndResolve(true, __func__);
             });
}

RefPtr<BoolPromise> FinalizeOriginEvictionOp::Open() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mLocks.IsEmpty());

  return BoolPromise::CreateAndResolve(true, __func__);
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

#ifdef DEBUG
  NoteActorDestroyed();
#endif

  mLocks.Clear();
}

RefPtr<BoolPromise> SaveOriginAccessTimeOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(
      Nullable<PersistenceType>(mOriginMetadata.mPersistenceType),
      OriginScope::FromOrigin(mOriginMetadata.mOrigin),
      Nullable<Client::Type>(),
      /* aExclusive */ false);
}

nsresult SaveOriginAccessTimeOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  AUTO_PROFILER_LABEL("SaveOriginAccessTimeOp::DoDirectoryWork", OTHER);

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()), NS_ERROR_ABORT);

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

void SaveOriginAccessTimeOp::CloseDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = nullptr;
}

RefPtr<BoolPromise> ClearPrivateRepositoryOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(
      Nullable<PersistenceType>(PERSISTENCE_TYPE_PRIVATE),
      OriginScope::FromNull(), Nullable<Client::Type>(),
      /* aExclusive */ true);
}

nsresult ClearPrivateRepositoryOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  AUTO_PROFILER_LABEL("ClearPrivateRepositoryOp::DoDirectoryWork", OTHER);

  QM_TRY_INSPECT(
      const auto& directory,
      QM_NewLocalFile(aQuotaManager.GetStoragePath(PERSISTENCE_TYPE_PRIVATE)));

  nsresult rv = directory->Remove(true);
  if (rv != NS_ERROR_FILE_NOT_FOUND && NS_FAILED(rv)) {
    // This should never fail if we've closed all storage connections
    // correctly...
    MOZ_ASSERT(false, "Failed to remove directory!");
  }

  aQuotaManager.RemoveQuotaForRepository(PERSISTENCE_TYPE_PRIVATE);

  aQuotaManager.RepositoryClearCompleted(PERSISTENCE_TYPE_PRIVATE);

  return NS_OK;
}

void ClearPrivateRepositoryOp::CloseDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = nullptr;
}

RefPtr<BoolPromise> ShutdownStorageOp::OpenDirectory() {
  AssertIsOnOwningThread();

  // Clear directory lock tables (which also saves origin access time) before
  // acquiring the exclusive lock below. Otherwise, saving of origin access
  // time would be scheduled after storage shutdown and that would initialize
  // storage again in the end.
  mQuotaManager->ClearDirectoryLockTables();

  mDirectoryLock = mQuotaManager->CreateDirectoryLockInternal(
      Nullable<PersistenceType>(), OriginScope::FromNull(),
      Nullable<Client::Type>(),
      /* aExclusive */ true);

  return mDirectoryLock->Acquire();
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

  aQuotaManager.MaybeRecordQuotaManagerShutdownStep(
      "ShutdownStorageOp::DoDirectoryWork -> ShutdownStorageInternal."_ns);

  aQuotaManager.ShutdownStorageInternal();

  return NS_OK;
}

void ShutdownStorageOp::CloseDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = nullptr;
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

GetUsageOp::GetUsageOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                       const UsageRequestParams& aParams)
    : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                 "dom::quota::GetUsageOp"),
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

RefPtr<BoolPromise> GetUsageOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(Nullable<PersistenceType>(),
                              OriginScope::FromNull(), Nullable<Client::Type>(),
                              /* aExclusive */ false);
}

nsresult GetUsageOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  AUTO_PROFILER_LABEL("GetUsageOp::DoDirectoryWork", OTHER);

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

void GetUsageOp::CloseDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = nullptr;
}

GetOriginUsageOp::GetOriginUsageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const UsageRequestParams& aParams)
    : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                 "dom::quota::GetOriginUsageOp"),
      mParams(aParams.get_OriginUsageParams()) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() == UsageRequestParams::TOriginUsageParams);

  // Overwrite GetOriginUsageOp default values.
  mFromMemory = mParams.fromMemory();
}

nsresult GetOriginUsageOp::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  QM_TRY_UNWRAP(
      mPrincipalMetadata,
      aQuotaManager.GetInfoFromValidatedPrincipalInfo(mParams.principalInfo()));

  mPrincipalMetadata.AssertInvariants();

  return NS_OK;
}

RefPtr<BoolPromise> GetOriginUsageOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(
      Nullable<PersistenceType>(),
      OriginScope::FromOrigin(mPrincipalMetadata.mOrigin),
      Nullable<Client::Type>(),
      /* aExclusive */ false);
}

nsresult GetOriginUsageOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();
  MOZ_ASSERT(mUsageInfo.TotalUsage().isNothing());

  AUTO_PROFILER_LABEL("GetOriginUsageOp::DoDirectoryWork", OTHER);

  if (mFromMemory) {
    // Ensure temporary storage is initialized. If temporary storage hasn't been
    // initialized yet, the method will initialize it by traversing the
    // repositories for temporary and default storage (including our origin).
    QM_TRY(MOZ_TO_RESULT(aQuotaManager.EnsureTemporaryStorageIsInitialized()));

    // Get cached usage (the method doesn't have to stat any files). File usage
    // is not tracked in memory separately, so just add to the database usage.
    mUsageInfo += DatabaseUsageType(
        Some(aQuotaManager.GetOriginUsage(mPrincipalMetadata)));

    return NS_OK;
  }

  // Add all the persistent/temporary/default storage files we care about.
  for (const PersistenceType type : kAllPersistenceTypes) {
    const OriginMetadata originMetadata = {mPrincipalMetadata, type};

    auto usageInfoOrErr =
        GetUsageForOrigin(aQuotaManager, type, originMetadata);
    if (NS_WARN_IF(usageInfoOrErr.isErr())) {
      return usageInfoOrErr.unwrapErr();
    }

    mUsageInfo += usageInfoOrErr.unwrap();
  }

  return NS_OK;
}

void GetOriginUsageOp::GetResponse(UsageRequestResponse& aResponse) {
  AssertIsOnOwningThread();

  OriginUsageResponse usageResponse;

  usageResponse.usageInfo() = mUsageInfo;

  aResponse = usageResponse;
}

void GetOriginUsageOp::CloseDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = nullptr;
}

StorageNameOp::StorageNameOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager)
    : QuotaRequestBase(std::move(aQuotaManager), "dom::quota::StorageNameOp") {
  AssertIsOnOwningThread();
}

RefPtr<BoolPromise> StorageNameOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return BoolPromise::CreateAndResolve(true, __func__);
}

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

void StorageNameOp::CloseDirectory() { AssertIsOnOwningThread(); }

InitializedRequestBase::InitializedRequestBase(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager, const char* aName)
    : QuotaRequestBase(std::move(aQuotaManager), aName), mInitialized(false) {
  AssertIsOnOwningThread();
}

RefPtr<BoolPromise> InitializedRequestBase::OpenDirectory() {
  AssertIsOnOwningThread();

  return BoolPromise::CreateAndResolve(true, __func__);
}

void InitializedRequestBase::CloseDirectory() { AssertIsOnOwningThread(); }

nsresult StorageInitializedOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("StorageInitializedOp::DoDirectoryWork", OTHER);

  mInitialized = aQuotaManager.IsStorageInitializedInternal();

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

InitOp::InitOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
               RefPtr<UniversalDirectoryLock> aDirectoryLock)
    : ResolvableNormalOriginOp(std::move(aQuotaManager), "dom::quota::InitOp"),
      mDirectoryLock(std::move(aDirectoryLock)) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mDirectoryLock);
}

RefPtr<BoolPromise> InitOp::OpenDirectory() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mDirectoryLock);

  return BoolPromise::CreateAndResolve(true, __func__);
}

nsresult InitOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitOp::DoDirectoryWork", OTHER);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.EnsureStorageIsInitializedInternal()));

  return NS_OK;
}

bool InitOp::GetResolveValue() { return true; }

void InitOp::CloseDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = nullptr;
}

InitTemporaryStorageOp::InitTemporaryStorageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager)
    : QuotaRequestBase(std::move(aQuotaManager),
                       "dom::quota::InitTemporaryStorageOp") {
  AssertIsOnOwningThread();
}

RefPtr<BoolPromise> InitTemporaryStorageOp::OpenDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = mQuotaManager->CreateDirectoryLockInternal(
      Nullable<PersistenceType>(), OriginScope::FromNull(),
      Nullable<Client::Type>(), /* aExclusive */ false);

  return mDirectoryLock->Acquire();
}

nsresult InitTemporaryStorageOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitTemporaryStorageOp::DoDirectoryWork", OTHER);

  QM_TRY(OkIf(aQuotaManager.IsStorageInitializedInternal()),
         NS_ERROR_NOT_INITIALIZED);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.EnsureTemporaryStorageIsInitialized()));

  return NS_OK;
}

void InitTemporaryStorageOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = InitTemporaryStorageResponse();
}

void InitTemporaryStorageOp::CloseDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = nullptr;
}

InitializeOriginRequestBase::InitializeOriginRequestBase(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager, const char* aName,
    const PersistenceType aPersistenceType, const PrincipalInfo& aPrincipalInfo)
    : QuotaRequestBase(std::move(aQuotaManager), aName),
      mPrincipalInfo(aPrincipalInfo),
      mPersistenceType(aPersistenceType),
      mCreated(false) {
  AssertIsOnOwningThread();
}

nsresult InitializeOriginRequestBase::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  QM_TRY_UNWRAP(
      mPrincipalMetadata,
      aQuotaManager.GetInfoFromValidatedPrincipalInfo(mPrincipalInfo));

  mPrincipalMetadata.AssertInvariants();

  return NS_OK;
}

RefPtr<BoolPromise> InitializeOriginRequestBase::OpenDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = mQuotaManager->CreateDirectoryLockInternal(
      Nullable<PersistenceType>(mPersistenceType),
      OriginScope::FromOrigin(mPrincipalMetadata.mOrigin),
      Nullable<Client::Type>(), /* aExclusive */ false);

  return mDirectoryLock->Acquire();
}

void InitializeOriginRequestBase::CloseDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = nullptr;
}

InitializePersistentOriginOp::InitializePersistentOriginOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const RequestParams& aParams)
    : InitializeOriginRequestBase(
          std::move(aQuotaManager), "dom::quota::InitializePersistentOriginOp",
          PERSISTENCE_TYPE_PERSISTENT,
          aParams.get_InitializePersistentOriginParams().principalInfo()) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() ==
             RequestParams::TInitializePersistentOriginParams);
}

nsresult InitializePersistentOriginOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitializePersistentOriginOp::DoDirectoryWork", OTHER);

  QM_TRY(OkIf(aQuotaManager.IsStorageInitializedInternal()),
         NS_ERROR_NOT_INITIALIZED);

  QM_TRY_UNWRAP(mCreated,
                (aQuotaManager
                     .EnsurePersistentOriginIsInitialized(OriginMetadata{
                         mPrincipalMetadata, PERSISTENCE_TYPE_PERSISTENT})
                     .map([](const auto& res) { return res.second; })));

  return NS_OK;
}

void InitializePersistentOriginOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = InitializePersistentOriginResponse(mCreated);
}

InitializeTemporaryOriginOp::InitializeTemporaryOriginOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const RequestParams& aParams)
    : InitializeOriginRequestBase(
          std::move(aQuotaManager), "dom::quota::InitializeTemporaryOriginOp",
          aParams.get_InitializeTemporaryOriginParams().persistenceType(),
          aParams.get_InitializeTemporaryOriginParams().principalInfo()) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() == RequestParams::TInitializeTemporaryOriginParams);
}

nsresult InitializeTemporaryOriginOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitializeTemporaryOriginOp::DoDirectoryWork", OTHER);

  QM_TRY(OkIf(aQuotaManager.IsStorageInitializedInternal()),
         NS_ERROR_NOT_INITIALIZED);

  QM_TRY(OkIf(aQuotaManager.IsTemporaryStorageInitialized()),
         NS_ERROR_NOT_INITIALIZED);

  QM_TRY_UNWRAP(mCreated,
                (aQuotaManager
                     .EnsureTemporaryOriginIsInitialized(
                         mPersistenceType,
                         OriginMetadata{mPrincipalMetadata, mPersistenceType})
                     .map([](const auto& res) { return res.second; })));

  return NS_OK;
}

void InitializeTemporaryOriginOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = InitializeTemporaryOriginResponse(mCreated);
}

InitializeClientBase::InitializeClientBase(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager, const char* aName,
    const PersistenceType aPersistenceType, const PrincipalInfo& aPrincipalInfo,
    Client::Type aClientType)
    : ResolvableNormalOriginOp(std::move(aQuotaManager), aName),
      mPrincipalInfo(aPrincipalInfo),
      mPersistenceType(aPersistenceType),
      mClientType(aClientType),
      mCreated(false) {
  AssertIsOnOwningThread();
}

nsresult InitializeClientBase::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  QM_TRY_UNWRAP(
      PrincipalMetadata principalMetadata,
      aQuotaManager.GetInfoFromValidatedPrincipalInfo(mPrincipalInfo));

  principalMetadata.AssertInvariants();

  mClientMetadata = {
      OriginMetadata{std::move(principalMetadata), mPersistenceType},
      mClientType};

  return NS_OK;
}

RefPtr<BoolPromise> InitializeClientBase::OpenDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = mQuotaManager->CreateDirectoryLockInternal(
      Nullable(mPersistenceType),
      OriginScope::FromOrigin(mClientMetadata.mOrigin),
      Nullable(mClientMetadata.mClientType), /* aExclusive */ false);

  return mDirectoryLock->Acquire();
}

void InitializeClientBase::CloseDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = nullptr;
}

InitializePersistentClientOp::InitializePersistentClientOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const PrincipalInfo& aPrincipalInfo, Client::Type aClientType)
    : InitializeClientBase(
          std::move(aQuotaManager), "dom::quota::InitializePersistentClientOp",
          PERSISTENCE_TYPE_PERSISTENT, aPrincipalInfo, aClientType) {
  AssertIsOnOwningThread();
}

nsresult InitializePersistentClientOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitializePersistentClientOp::DoDirectoryWork", OTHER);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.IsStorageInitializedInternal()),
         NS_ERROR_FAILURE);

  QM_TRY(
      MOZ_TO_RESULT(aQuotaManager.IsOriginInitialized(mClientMetadata.mOrigin)),
      NS_ERROR_FAILURE);

  QM_TRY_UNWRAP(
      mCreated,
      (aQuotaManager.EnsurePersistentClientIsInitialized(mClientMetadata)
           .map([](const auto& res) { return res.second; })));

  return NS_OK;
}

bool InitializePersistentClientOp::GetResolveValue() {
  AssertIsOnOwningThread();

  return mCreated;
}

InitializeTemporaryClientOp::InitializeTemporaryClientOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    PersistenceType aPersistenceType, const PrincipalInfo& aPrincipalInfo,
    Client::Type aClientType)
    : InitializeClientBase(std::move(aQuotaManager),
                           "dom::quota::InitializeTemporaryClientOp",
                           aPersistenceType, aPrincipalInfo, aClientType) {
  AssertIsOnOwningThread();
}

nsresult InitializeTemporaryClientOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitializeTemporaryClientOp::DoDirectoryWork", OTHER);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.IsStorageInitializedInternal()),
         NS_ERROR_FAILURE);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.IsTemporaryStorageInitialized()),
         NS_ERROR_FAILURE);

  QM_TRY(MOZ_TO_RESULT(
             aQuotaManager.IsTemporaryOriginInitialized(mClientMetadata)),
         NS_ERROR_FAILURE);

  QM_TRY_UNWRAP(
      mCreated,
      (aQuotaManager.EnsureTemporaryClientIsInitialized(mClientMetadata)
           .map([](const auto& res) { return res.second; })));

  return NS_OK;
}

bool InitializeTemporaryClientOp::GetResolveValue() {
  AssertIsOnOwningThread();

  return mCreated;
}

GetFullOriginMetadataOp::GetFullOriginMetadataOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const GetFullOriginMetadataParams& aParams)
    : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                 "dom::quota::GetFullOriginMetadataOp"),
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

RefPtr<BoolPromise> GetFullOriginMetadataOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(
      Nullable<PersistenceType>(mOriginMetadata.mPersistenceType),
      OriginScope::FromOrigin(mOriginMetadata.mOrigin),
      Nullable<Client::Type>(),
      /* aExclusive */ false);
}

nsresult GetFullOriginMetadataOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  AUTO_PROFILER_LABEL("GetFullOriginMetadataOp::DoDirectoryWork", OTHER);

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

void GetFullOriginMetadataOp::CloseDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = nullptr;
}

ClearStorageOp::ClearStorageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager)
    : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                 "dom::quota::ClearStorageOp") {
  AssertIsOnOwningThread();
}

void ClearStorageOp::DeleteFiles(QuotaManager& aQuotaManager) {
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

void ClearStorageOp::DeleteStorageFile(QuotaManager& aQuotaManager) {
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

RefPtr<BoolPromise> ClearStorageOp::OpenDirectory() {
  AssertIsOnOwningThread();

  // Clear directory lock tables (which also saves origin access time) before
  // acquiring the exclusive lock below. Otherwise, saving of origin access
  // time would be scheduled after storage clearing and that would initialize
  // storage again in the end.
  mQuotaManager->ClearDirectoryLockTables();

  return OpenStorageDirectory(Nullable<PersistenceType>(),
                              OriginScope::FromNull(), Nullable<Client::Type>(),
                              /* aExclusive */ true);
}

nsresult ClearStorageOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  AUTO_PROFILER_LABEL("ClearStorageOp::DoDirectoryWork", OTHER);

  DeleteFiles(aQuotaManager);

  aQuotaManager.RemoveQuota();

  aQuotaManager.ShutdownStorageInternal();

  DeleteStorageFile(aQuotaManager);

  return NS_OK;
}

bool ClearStorageOp::GetResolveValue() {
  AssertIsOnOwningThread();

  return true;
}

void ClearStorageOp::CloseDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = nullptr;
}

void ClearRequestBase::DeleteFiles(QuotaManager& aQuotaManager,
                                   const OriginMetadata& aOriginMetadata,
                                   const Nullable<Client::Type>& aClientType) {
  AssertIsOnIOThread();

  DeleteFilesInternal(
      aQuotaManager, aOriginMetadata.mPersistenceType,
      OriginScope::FromOrigin(aOriginMetadata.mOrigin), aClientType,
      [&aQuotaManager, &aOriginMetadata](
          const std::function<Result<Ok, nsresult>(nsCOMPtr<nsIFile>&&)>& aBody)
          -> Result<Ok, nsresult> {
        QM_TRY_UNWRAP(auto directory,
                      aQuotaManager.GetOriginDirectory(aOriginMetadata));

        QM_TRY_INSPECT(const bool& exists,
                       MOZ_TO_RESULT_INVOKE_MEMBER(directory, Exists));

        if (!exists) {
          return Ok{};
        }

        QM_TRY_RETURN(aBody(std::move(directory)));
      });
}

void ClearRequestBase::DeleteFiles(QuotaManager& aQuotaManager,
                                   PersistenceType aPersistenceType,
                                   const OriginScope& aOriginScope,
                                   const Nullable<Client::Type>& aClientType) {
  AssertIsOnIOThread();

  DeleteFilesInternal(
      aQuotaManager, aPersistenceType, aOriginScope, aClientType,
      [&aQuotaManager, &aPersistenceType](
          const std::function<Result<Ok, nsresult>(nsCOMPtr<nsIFile>&&)>& aBody)
          -> Result<Ok, nsresult> {
        QM_TRY_INSPECT(
            const auto& directory,
            QM_NewLocalFile(aQuotaManager.GetStoragePath(aPersistenceType)));

        QM_TRY_INSPECT(const bool& exists,
                       MOZ_TO_RESULT_INVOKE_MEMBER(directory, Exists));

        if (!exists) {
          return Ok{};
        }

        QM_TRY_RETURN(CollectEachFile(*directory, aBody));
      });
}

template <typename FileCollector>
void ClearRequestBase::DeleteFilesInternal(
    QuotaManager& aQuotaManager, PersistenceType aPersistenceType,
    const OriginScope& aOriginScope, const Nullable<Client::Type>& aClientType,
    const FileCollector& aFileCollector) {
  AssertIsOnIOThread();

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.AboutToClearOrigins(
             Nullable<PersistenceType>(aPersistenceType), aOriginScope,
             aClientType)),
         QM_VOID);

  nsTArray<nsCOMPtr<nsIFile>> directoriesForRemovalRetry;

  aQuotaManager.MaybeRecordQuotaManagerShutdownStep(
      "ClearRequestBase: Starting deleting files"_ns);

  QM_TRY(
      aFileCollector([&aClientType, &originScope = aOriginScope,
                      aPersistenceType, &aQuotaManager,
                      &directoriesForRemovalRetry](nsCOMPtr<nsIFile>&& file)
                         -> mozilla::Result<Ok, nsresult> {
        QM_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(*file));

        QM_TRY_INSPECT(
            const auto& leafName,
            MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsAutoString, file, GetLeafName));

        switch (dirEntryKind) {
          case nsIFileKind::ExistsAsDirectory: {
            QM_TRY_UNWRAP(auto maybeMetadata,
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
              // Unknown directories during clearing are allowed. Just
              // warn if we find them.
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

            if (!aClientType.IsNull()) {
              nsAutoString clientDirectoryName;
              QM_TRY(OkIf(Client::TypeToText(aClientType.Value(),
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
                aQuotaManager.RemoveOriginDirectory(*file), [&](const auto&) {
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
              if (aClientType.IsNull()) {
                aQuotaManager.RemoveQuotaForOrigin(aPersistenceType, metadata);
              } else {
                aQuotaManager.ResetUsageForClient(
                    ClientMetadata{metadata, aClientType.Value()});
              }
            }

            aQuotaManager.OriginClearCompleted(aPersistenceType,
                                               metadata.mOrigin, aClientType);

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
          aQuotaManager.RemoveOriginDirectory(*file),
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

ClearOriginOp::ClearOriginOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const mozilla::Maybe<PersistenceType>& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo,
    const mozilla::Maybe<Client::Type>& aClientType)
    : ClearRequestBase(std::move(aQuotaManager), "dom::quota::ClearOriginOp"),
      mPrincipalInfo(aPrincipalInfo),
      mPersistenceType(aPersistenceType
                           ? Nullable<PersistenceType>(*aPersistenceType)
                           : Nullable<PersistenceType>()),
      mClientType(aClientType ? Nullable<Client::Type>(*aClientType)
                              : Nullable<Client::Type>()) {
  AssertIsOnOwningThread();
}

nsresult ClearOriginOp::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  QM_TRY_UNWRAP(
      mPrincipalMetadata,
      aQuotaManager.GetInfoFromValidatedPrincipalInfo(mPrincipalInfo));

  mPrincipalMetadata.AssertInvariants();

  return NS_OK;
}

RefPtr<BoolPromise> ClearOriginOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(
      mPersistenceType, OriginScope::FromOrigin(mPrincipalMetadata.mOrigin),
      mClientType,
      /* aExclusive */ true);
}

nsresult ClearOriginOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  AUTO_PROFILER_LABEL("ClearRequestBase::DoDirectoryWork", OTHER);

  if (mPersistenceType.IsNull()) {
    for (const PersistenceType type : kAllPersistenceTypes) {
      DeleteFiles(aQuotaManager, OriginMetadata(mPrincipalMetadata, type),
                  mClientType);
    }
  } else {
    DeleteFiles(aQuotaManager,
                OriginMetadata(mPrincipalMetadata, mPersistenceType.Value()),
                mClientType);
  }

  return NS_OK;
}

bool ClearOriginOp::GetResolveValue() {
  AssertIsOnOwningThread();

  return true;
}

void ClearOriginOp::CloseDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = nullptr;
}

ClearStoragesForOriginPrefixOp::ClearStoragesForOriginPrefixOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const Maybe<PersistenceType>& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo)
    : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                 "dom::quota::ClearStoragesForOriginPrefixOp"),
      mPrefix(
          QuotaManager::GetOriginFromValidatedPrincipalInfo(aPrincipalInfo)),
      mPersistenceType(aPersistenceType
                           ? Nullable<PersistenceType>(*aPersistenceType)
                           : Nullable<PersistenceType>()) {
  AssertIsOnOwningThread();
}

RefPtr<BoolPromise> ClearStoragesForOriginPrefixOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(mPersistenceType,
                              OriginScope::FromPrefix(mPrefix),
                              Nullable<Client::Type>(),
                              /* aExclusive */ true);
}

nsresult ClearStoragesForOriginPrefixOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("ClearStoragesForOriginPrefixOp::DoDirectoryWork", OTHER);

  if (mPersistenceType.IsNull()) {
    for (const PersistenceType type : kAllPersistenceTypes) {
      DeleteFiles(aQuotaManager, type, OriginScope::FromPrefix(mPrefix),
                  Nullable<Client::Type>());
    }
  } else {
    DeleteFiles(aQuotaManager, mPersistenceType.Value(),
                OriginScope::FromPrefix(mPrefix), Nullable<Client::Type>());
  }

  return NS_OK;
}

bool ClearStoragesForOriginPrefixOp::GetResolveValue() {
  AssertIsOnOwningThread();

  return true;
}

void ClearStoragesForOriginPrefixOp::CloseDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = nullptr;
}

ClearDataOp::ClearDataOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                         const OriginAttributesPattern& aPattern)
    : ClearRequestBase(std::move(aQuotaManager), "dom::quota::ClearDataOp"),
      mPattern(aPattern) {}

RefPtr<BoolPromise> ClearDataOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(Nullable<PersistenceType>(),
                              OriginScope::FromPattern(mPattern),
                              Nullable<Client::Type>(),
                              /* aExclusive */ true);
}

nsresult ClearDataOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("ClearRequestBase::DoDirectoryWork", OTHER);

  for (const PersistenceType type : kAllPersistenceTypes) {
    DeleteFiles(aQuotaManager, type, OriginScope::FromPattern(mPattern),
                Nullable<Client::Type>());
  }

  return NS_OK;
}

bool ClearDataOp::GetResolveValue() {
  AssertIsOnOwningThread();

  return true;
}

void ClearDataOp::CloseDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = nullptr;
}

ResetOriginOp::ResetOriginOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                             const RequestParams& aParams)
    : QuotaRequestBase(std::move(aQuotaManager), "dom::quota::ResetOriginOp") {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() == RequestParams::TResetOriginParams);

  const ClearResetOriginParams& params =
      aParams.get_ResetOriginParams().commonParams();

  mOrigin =
      QuotaManager::GetOriginFromValidatedPrincipalInfo(params.principalInfo());

  if (params.persistenceTypeIsExplicit()) {
    mPersistenceType.SetValue(params.persistenceType());
  }

  if (params.clientTypeIsExplicit()) {
    mClientType.SetValue(params.clientType());
  }
}

RefPtr<BoolPromise> ResetOriginOp::OpenDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = mQuotaManager->CreateDirectoryLockInternal(
      mPersistenceType, OriginScope::FromOrigin(mOrigin), mClientType,
      /* aExclusive */ true);

  return mDirectoryLock->Acquire();
}

nsresult ResetOriginOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("ResetOriginOp::DoDirectoryWork", OTHER);

  // All the work is handled by NormalOriginOperationBase parent class. In
  // this particular case, we just needed to acquire an exclusive directory
  // lock and that's it.

  return NS_OK;
}

void ResetOriginOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = ResetOriginResponse();
}

void ResetOriginOp::CloseDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = nullptr;
}

PersistRequestBase::PersistRequestBase(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const PrincipalInfo& aPrincipalInfo)
    : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                 "dom::quota::PersistRequestBase"),
      mPrincipalInfo(aPrincipalInfo) {
  AssertIsOnOwningThread();
}

nsresult PersistRequestBase::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  // Figure out which origin we're dealing with.
  QM_TRY_UNWRAP(
      mPrincipalMetadata,
      aQuotaManager.GetInfoFromValidatedPrincipalInfo(mPrincipalInfo));

  mPrincipalMetadata.AssertInvariants();

  return NS_OK;
}

RefPtr<BoolPromise> PersistRequestBase::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(
      Nullable<PersistenceType>(PERSISTENCE_TYPE_DEFAULT),
      OriginScope::FromOrigin(mPrincipalMetadata.mOrigin),
      Nullable<Client::Type>(),
      /* aExclusive */ false);
}

void PersistRequestBase::CloseDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = nullptr;
}

PersistedOp::PersistedOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                         const RequestParams& aParams)
    : PersistRequestBase(std::move(aQuotaManager),
                         aParams.get_PersistedParams().principalInfo()),
      mPersisted(false) {
  MOZ_ASSERT(aParams.type() == RequestParams::TPersistedParams);
}

nsresult PersistedOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  AUTO_PROFILER_LABEL("PersistedOp::DoDirectoryWork", OTHER);

  const OriginMetadata originMetadata = {mPrincipalMetadata,
                                         PERSISTENCE_TYPE_DEFAULT};

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

PersistOp::PersistOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                     const RequestParams& aParams)
    : PersistRequestBase(std::move(aQuotaManager),
                         aParams.get_PersistParams().principalInfo()) {
  MOZ_ASSERT(aParams.type() == RequestParams::TPersistParams);
}

nsresult PersistOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  const OriginMetadata originMetadata = {mPrincipalMetadata,
                                         PERSISTENCE_TYPE_DEFAULT};

  AUTO_PROFILER_LABEL("PersistOp::DoDirectoryWork", OTHER);

  // Update directory metadata on disk first. Then, create/update the
  // originInfo if needed.
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

EstimateOp::EstimateOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                       const EstimateParams& aParams)
    : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                 "dom::quota::EstimateOp"),
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

RefPtr<BoolPromise> EstimateOp::OpenDirectory() {
  AssertIsOnOwningThread();

  // XXX In theory, we should be locking entire group, not just one origin.
  return OpenStorageDirectory(
      Nullable<PersistenceType>(mOriginMetadata.mPersistenceType),
      OriginScope::FromOrigin(mOriginMetadata.mOrigin),
      Nullable<Client::Type>(),
      /* aExclusive */ false);
}

nsresult EstimateOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  AUTO_PROFILER_LABEL("EstimateOp::DoDirectoryWork", OTHER);

  // Ensure temporary storage is initialized. If temporary storage hasn't been
  // initialized yet, the method will initialize it by traversing the
  // repositories for temporary and default storage (including origins
  // belonging to our group).
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

void EstimateOp::CloseDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = nullptr;
}

ListOriginsOp::ListOriginsOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager)
    : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                 "dom::quota::ListOriginsOp") {
  AssertIsOnOwningThread();
}

RefPtr<BoolPromise> ListOriginsOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(Nullable<PersistenceType>(),
                              OriginScope::FromNull(), Nullable<Client::Type>(),
                              /* aExclusive */ false);
}

nsresult ListOriginsOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  AUTO_PROFILER_LABEL("ListOriginsOp::DoDirectoryWork", OTHER);

  for (const PersistenceType type : kAllPersistenceTypes) {
    QM_TRY(MOZ_TO_RESULT(TraverseRepository(aQuotaManager, type)));
  }

  // TraverseRepository above only consulted the file-system to get a list of
  // known origins, but we also need to include origins that have pending
  // quota usage.

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

void ListOriginsOp::CloseDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = nullptr;
}

}  // namespace mozilla::dom::quota
