/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_INDEXEDDB_DATABASEFILEMANAGER_H_
#define DOM_INDEXEDDB_DATABASEFILEMANAGER_H_

#include "FileInfoManager.h"
#include "IndexedDBCipherKeyManager.h"
#include "mozilla/dom/FlippedOnce.h"
#include "mozilla/dom/quota/CommonMetadata.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/InitializedOnce.h"

class nsIFile;
class mozIStorageConnection;

namespace mozilla::dom::indexedDB {

// Implemented in ActorsParent.cpp.
class DatabaseFileManager final
    : public FileInfoManager<DatabaseFileManager>,
      public AtomicSafeRefCounted<DatabaseFileManager> {
  using PersistenceType = mozilla::dom::quota::PersistenceType;
  using FileInfoManager<DatabaseFileManager>::MutexType;

  const PersistenceType mPersistenceType;
  const quota::OriginMetadata mOriginMetadata;
  const nsString mDatabaseName;
  const nsCString mDatabaseID;
  const nsString mDatabaseFilePath;

  RefPtr<IndexedDBCipherKeyManager> mCipherKeyManager;

  LazyInitializedOnce<const nsString> mDirectoryPath;
  LazyInitializedOnce<const nsString> mJournalDirectoryPath;

  uint64_t mDatabaseVersion;

  const bool mEnforcingQuota;
  const bool mIsInPrivateBrowsingMode;

  FlippedOnce<false> mInitialized;

  // Lock protecting DatabaseFileManager.mFileInfos.
  // It's s also used to atomically update DatabaseFileInfo.mRefCnt and
  // DatabaseFileInfo.mDBRefCnt
  static MutexType sMutex;

 public:
  [[nodiscard]] static nsCOMPtr<nsIFile> GetFileForId(nsIFile* aDirectory,
                                                      int64_t aId);

  [[nodiscard]] static nsCOMPtr<nsIFile> GetCheckedFileForId(
      nsIFile* aDirectory, int64_t aId);

  static nsresult InitDirectory(nsIFile& aDirectory, nsIFile& aDatabaseFile,
                                const nsACString& aOrigin,
                                uint32_t aTelemetryId);

  template <typename KnownDirEntryOp, typename UnknownDirEntryOp>
  static Result<Ok, nsresult> TraverseFiles(
      nsIFile& aDirectory, KnownDirEntryOp&& aKnownDirEntryOp,
      UnknownDirEntryOp&& aUnknownDirEntryOp);

  static Result<quota::FileUsageType, nsresult> GetUsage(nsIFile* aDirectory);

  DatabaseFileManager(PersistenceType aPersistenceType,
                      const quota::OriginMetadata& aOriginMetadata,
                      const nsAString& aDatabaseName,
                      const nsCString& aDatabaseID,
                      const nsAString& aDatabaseFilePath, bool aEnforcingQuota,
                      bool aIsInPrivateBrowsingMode);

  PersistenceType Type() const { return mPersistenceType; }

  const quota::OriginMetadata& OriginMetadata() const {
    return mOriginMetadata;
  }

  const nsACString& Origin() const { return mOriginMetadata.mOrigin; }

  const nsAString& DatabaseName() const { return mDatabaseName; }

  const nsCString& DatabaseID() const { return mDatabaseID; }

  const nsAString& DatabaseFilePath() const { return mDatabaseFilePath; }

  uint64_t DatabaseVersion() const;

  void UpdateDatabaseVersion(uint64_t aDatabaseVersion);

  IndexedDBCipherKeyManager& MutableCipherKeyManagerRef() const {
    MOZ_ASSERT(mIsInPrivateBrowsingMode);
    MOZ_ASSERT(mCipherKeyManager);

    return *mCipherKeyManager;
  }

  auto IsInPrivateBrowsingMode() const { return mIsInPrivateBrowsingMode; }

  bool EnforcingQuota() const { return mEnforcingQuota; }

  bool Initialized() const { return mInitialized; }

  nsresult Init(nsIFile* aDirectory, const uint64_t aDatabaseVersion,
                mozIStorageConnection& aConnection);

  [[nodiscard]] nsCOMPtr<nsIFile> GetDirectory();

  [[nodiscard]] nsCOMPtr<nsIFile> GetCheckedDirectory();

  [[nodiscard]] nsCOMPtr<nsIFile> GetJournalDirectory();

  [[nodiscard]] nsCOMPtr<nsIFile> EnsureJournalDirectory();

  [[nodiscard]] nsresult SyncDeleteFile(int64_t aId);

  // XXX When getting rid of FileHelper, this method should be removed/made
  // private.
  [[nodiscard]] nsresult SyncDeleteFile(nsIFile& aFile,
                                        nsIFile& aJournalFile) const;

  [[nodiscard]] nsresult AsyncDeleteFile(int64_t aFileId);

  nsresult Invalidate() override;

  MOZ_DECLARE_REFCOUNTED_TYPENAME(DatabaseFileManager)

  static StaticMutex& Mutex() { return sMutex; }

  ~DatabaseFileManager() = default;
};

}  // namespace mozilla::dom::indexedDB

#endif  // DOM_INDEXEDDB_DATABASEFILEMANAGER_H_
