/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_QuotaClientImpl_h
#define mozilla_dom_cache_QuotaClientImpl_h

#include "mozilla/dom/cache/QuotaClient.h"
#include "mozilla/dom/cache/FileUtils.h"

namespace mozilla {
namespace dom {
namespace cache {

class CacheQuotaClient final : public quota::Client {
  static CacheQuotaClient* sInstance;

 public:
  using OriginMetadata = quota::OriginMetadata;
  using PersistenceType = quota::PersistenceType;
  using UsageInfo = quota::UsageInfo;

  CacheQuotaClient();

  static CacheQuotaClient* Get();

  virtual Type GetType() override;

  virtual Result<UsageInfo, nsresult> InitOrigin(
      PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
      const AtomicBool& aCanceled) override;

  virtual nsresult InitOriginWithoutTracking(
      PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
      const AtomicBool& aCanceled) override;

  virtual Result<UsageInfo, nsresult> GetUsageForOrigin(
      PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
      const AtomicBool& aCanceled) override;

  virtual void OnOriginClearCompleted(PersistenceType aPersistenceType,
                                      const nsACString& aOrigin) override;

  virtual void ReleaseIOThreadObjects() override;

  void AbortOperationsForLocks(
      const DirectoryLockIdTable& aDirectoryLockIds) override;

  virtual void AbortOperationsForProcess(
      ContentParentId aContentParentId) override;

  virtual void AbortAllOperations() override;

  virtual void StartIdleMaintenance() override;

  virtual void StopIdleMaintenance() override;

  nsresult UpgradeStorageFrom2_0To2_1(nsIFile* aDirectory) override;

  template <typename Callable>
  nsresult MaybeUpdatePaddingFileInternal(nsIFile& aBaseDir,
                                          mozIStorageConnection& aConn,
                                          const int64_t aIncreaseSize,
                                          const int64_t aDecreaseSize,
                                          Callable&& aCommitHook) {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_DIAGNOSTIC_ASSERT(aIncreaseSize >= 0);
    MOZ_DIAGNOSTIC_ASSERT(aDecreaseSize >= 0);

    // Temporary should be removed at the end of each action. If not, it means
    // the failure happened.
    const bool temporaryPaddingFileExist =
        DirectoryPaddingFileExists(aBaseDir, DirPaddingFile::TMP_FILE);

    if (aIncreaseSize == aDecreaseSize && !temporaryPaddingFileExist) {
      // Early return here, since most cache actions won't modify padding size.
      QM_TRY(aCommitHook());

      return NS_OK;
    }

    // Don't delete the temporary padding file in case of an error to force the
    // next action recalculate the padding size.
    QM_TRY(UpdateDirectoryPaddingFile(aBaseDir, aConn, aIncreaseSize,
                                      aDecreaseSize,
                                      temporaryPaddingFileExist));

    // Don't delete the temporary padding file in case of an error to force the
    // next action recalculate the padding size.
    QM_TRY(aCommitHook());

    QM_TRY(QM_OR_ELSE_WARN(
        ToResult(DirectoryPaddingFinalizeWrite(aBaseDir)),
        ([&aBaseDir](const nsresult) -> Result<Ok, nsresult> {
          // Force restore file next time.
          Unused << DirectoryPaddingDeleteFile(aBaseDir, DirPaddingFile::FILE);

          // Ensure that we are able to force the padding file
          // to be restored.
          MOZ_ASSERT(
              DirectoryPaddingFileExists(aBaseDir, DirPaddingFile::TMP_FILE));

          // Since both the body file and header have been
          // stored in the file-system, just make the action be
          // resolve and let the padding file be restored in the
          // next action.
          return Ok{};
        })));

    return NS_OK;
  }

  nsresult RestorePaddingFileInternal(nsIFile* aBaseDir,
                                      mozIStorageConnection* aConn);

  nsresult WipePaddingFileInternal(const QuotaInfo& aQuotaInfo,
                                   nsIFile* aBaseDir);

 private:
  ~CacheQuotaClient();

  void InitiateShutdown() override;
  bool IsShutdownCompleted() const override;
  nsCString GetShutdownStatus() const override;
  void ForceKillActors() override;
  void FinalizeShutdown() override;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CacheQuotaClient, override)
};

}  // namespace cache
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_cache_QuotaClientImpl_h
