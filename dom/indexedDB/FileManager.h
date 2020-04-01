/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_filemanager_h__
#define mozilla_dom_indexeddb_filemanager_h__

#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/InitializedOnce.h"
#include "FileManagerBase.h"

class nsIFile;
class mozIStorageConnection;

namespace mozilla {
namespace dom {
namespace indexedDB {

// Implemented in ActorsParent.cpp.
class FileManager final : public FileManagerBase<FileManager> {
  using PersistenceType = mozilla::dom::quota::PersistenceType;
  using FileManagerBase<FileManager>::MutexType;

  const PersistenceType mPersistenceType;
  const nsCString mGroup;
  const nsCString mOrigin;
  const nsString mDatabaseName;

  LazyInitializedOnce<const nsString> mDirectoryPath;
  LazyInitializedOnce<const nsString> mJournalDirectoryPath;

  const bool mEnforcingQuota;

  // Lock protecting FileManager.mFileInfos.
  // It's s also used to atomically update FileInfo.mRefCnt and
  // FileInfo.mDBRefCnt
  static MutexType sMutex;

 public:
  static MOZ_MUST_USE nsCOMPtr<nsIFile> GetFileForId(nsIFile* aDirectory,
                                                     int64_t aId);

  static MOZ_MUST_USE nsCOMPtr<nsIFile> GetCheckedFileForId(nsIFile* aDirectory,
                                                            int64_t aId);

  static nsresult InitDirectory(nsIFile* aDirectory, nsIFile* aDatabaseFile,
                                const nsACString& aOrigin,
                                uint32_t aTelemetryId);

  static nsresult GetUsage(nsIFile* aDirectory, Maybe<uint64_t>& aUsage);

  static nsresult GetUsage(nsIFile* aDirectory, uint64_t& aUsage);

  FileManager(PersistenceType aPersistenceType, const nsACString& aGroup,
              const nsACString& aOrigin, const nsAString& aDatabaseName,
              bool aEnforcingQuota);

  PersistenceType Type() const { return mPersistenceType; }

  const nsACString& Group() const { return mGroup; }

  const nsACString& Origin() const { return mOrigin; }

  const nsAString& DatabaseName() const { return mDatabaseName; }

  bool EnforcingQuota() const { return mEnforcingQuota; }

  nsresult Init(nsIFile* aDirectory, mozIStorageConnection* aConnection);

  MOZ_MUST_USE nsCOMPtr<nsIFile> GetDirectory();

  MOZ_MUST_USE nsCOMPtr<nsIFile> GetCheckedDirectory();

  MOZ_MUST_USE nsCOMPtr<nsIFile> GetJournalDirectory();

  MOZ_MUST_USE nsCOMPtr<nsIFile> EnsureJournalDirectory();

  MOZ_MUST_USE nsresult SyncDeleteFile(int64_t aId);

  // XXX When getting rid of FileHelper, this method should be removed/made
  // private.
  MOZ_MUST_USE nsresult SyncDeleteFile(nsIFile& aFile, nsIFile& aJournalFile);

  MOZ_MUST_USE nsresult AsyncDeleteFile(int64_t aFileId);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FileManager)

  static StaticMutex& Mutex() { return sMutex; }

 private:
  ~FileManager() = default;
};

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_indexeddb_filemanager_h__
