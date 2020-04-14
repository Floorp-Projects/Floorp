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
  using PersistenceType = quota::PersistenceType;
  using UsageInfo = quota::UsageInfo;

  CacheQuotaClient();

  static CacheQuotaClient* Get();

  virtual Type GetType() override;
  virtual nsresult InitOrigin(PersistenceType aPersistenceType,
                              const nsACString& aGroup,
                              const nsACString& aOrigin,
                              const AtomicBool& aCanceled,
                              UsageInfo* aUsageInfo,
                              bool aForGetUsage) override;

  virtual nsresult GetUsageForOrigin(PersistenceType aPersistenceType,
                                     const nsACString& aGroup,
                                     const nsACString& aOrigin,
                                     const AtomicBool& aCanceled,
                                     UsageInfo* aUsageInfo) override;

  virtual void OnOriginClearCompleted(PersistenceType aPersistenceType,
                                      const nsACString& aOrigin) override;

  virtual void ReleaseIOThreadObjects() override;

  virtual void AbortOperations(const nsACString& aOrigin) override;

  virtual void AbortOperationsForProcess(
      ContentParentId aContentParentId) override;

  virtual void StartIdleMaintenance() override;

  virtual void StopIdleMaintenance() override;

  virtual void ShutdownWorkThreads() override;

  nsresult UpgradeStorageFrom2_0To2_1(nsIFile* aDirectory) override;

  template <typename Callable>
  nsresult MaybeUpdatePaddingFileInternal(nsIFile* aBaseDir,
                                          mozIStorageConnection* aConn,
                                          const int64_t aIncreaseSize,
                                          const int64_t aDecreaseSize,
                                          Callable aCommitHook) {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_DIAGNOSTIC_ASSERT(aBaseDir);
    MOZ_DIAGNOSTIC_ASSERT(aConn);
    MOZ_DIAGNOSTIC_ASSERT(aIncreaseSize >= 0);
    MOZ_DIAGNOSTIC_ASSERT(aDecreaseSize >= 0);

    nsresult rv;

    // Temporary should be removed at the end of each action. If not, it means
    // the failure happened.
    bool temporaryPaddingFileExist =
        mozilla::dom::cache::DirectoryPaddingFileExists(
            aBaseDir, DirPaddingFile::TMP_FILE);

    if (aIncreaseSize == aDecreaseSize && !temporaryPaddingFileExist) {
      // Early return here, since most cache actions won't modify padding size.
      rv = aCommitHook();
      Unused << NS_WARN_IF(NS_FAILED(rv));
      return rv;
    }

    {
      MutexAutoLock lock(mDirPaddingFileMutex);
      rv = mozilla::dom::cache::LockedUpdateDirectoryPaddingFile(
          aBaseDir, aConn, aIncreaseSize, aDecreaseSize,
          temporaryPaddingFileExist);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        // Don't delete the temporary padding file here to force the next action
        // recalculate the padding size.
        return rv;
      }

      rv = aCommitHook();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        // Don't delete the temporary padding file here to force the next action
        // recalculate the padding size.
        return rv;
      }

      rv = mozilla::dom::cache::LockedDirectoryPaddingFinalizeWrite(aBaseDir);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        // Force restore file next time.
        Unused << mozilla::dom::cache::LockedDirectoryPaddingDeleteFile(
            aBaseDir, DirPaddingFile::FILE);

        // Ensure that we are able to force the padding file to be restored.
        MOZ_ASSERT(mozilla::dom::cache::DirectoryPaddingFileExists(
            aBaseDir, DirPaddingFile::TMP_FILE));

        // Since both the body file and header have been stored in the
        // file-system, just make the action be resolve and let the padding file
        // be restored in the next action.
        rv = NS_OK;
      }
    }

    return rv;
  }

  nsresult RestorePaddingFileInternal(nsIFile* aBaseDir,
                                      mozIStorageConnection* aConn);

  nsresult WipePaddingFileInternal(const QuotaInfo& aQuotaInfo,
                                   nsIFile* aBaseDir);

 private:
  ~CacheQuotaClient();

  nsresult GetUsageForOriginInternal(PersistenceType aPersistenceType,
                                     const nsACString& aGroup,
                                     const nsACString& aOrigin,
                                     const AtomicBool& aCanceled,
                                     UsageInfo* aUsageInfo,
                                     const bool aInitializing);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CacheQuotaClient, override)

  // Mutex lock to protect directroy padding files. It should only be acquired
  // in DOM Cache IO threads and Quota IO thread.
  mozilla::Mutex mDirPaddingFileMutex;
};

}  // namespace cache
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_cache_QuotaClientImpl_h
