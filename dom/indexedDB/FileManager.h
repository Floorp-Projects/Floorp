/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_filemanager_h__
#define mozilla_dom_indexeddb_filemanager_h__

#include "IndexedDatabase.h"

#include "nsIDOMFile.h"
#include "nsIFile.h"

#include "mozilla/dom/quota/StoragePrivilege.h"
#include "nsDataHashtable.h"

class mozIStorageConnection;

BEGIN_INDEXEDDB_NAMESPACE

class FileInfo;

class FileManager
{
  friend class FileInfo;

  typedef mozilla::dom::quota::StoragePrivilege StoragePrivilege;

public:
  FileManager(const nsACString& aOrigin, StoragePrivilege aPrivilege,
              const nsAString& aDatabaseName)
  : mOrigin(aOrigin), mPrivilege(aPrivilege), mDatabaseName(aDatabaseName),
    mLastFileId(0), mInvalidated(false)
  { }

  ~FileManager()
  { }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FileManager)

  const nsACString& Origin() const
  {
    return mOrigin;
  }

  const StoragePrivilege& Privilege() const
  {
    return mPrivilege;
  }

  const nsAString& DatabaseName() const
  {
    return mDatabaseName;
  }

  bool Invalidated() const
  {
    return mInvalidated;
  }

  nsresult Init(nsIFile* aDirectory,
                mozIStorageConnection* aConnection);

  nsresult Invalidate();

  already_AddRefed<nsIFile> GetDirectory();

  already_AddRefed<nsIFile> GetJournalDirectory();

  already_AddRefed<nsIFile> EnsureJournalDirectory();

  already_AddRefed<FileInfo> GetFileInfo(int64_t aId);

  already_AddRefed<FileInfo> GetNewFileInfo();

  static already_AddRefed<nsIFile> GetFileForId(nsIFile* aDirectory,
                                                int64_t aId);

  static nsresult InitDirectory(nsIFile* aDirectory,
                                nsIFile* aDatabaseFile,
                                const nsACString& aOrigin);

  static nsresult GetUsage(nsIFile* aDirectory, uint64_t* aUsage);

private:
  nsCString mOrigin;
  StoragePrivilege mPrivilege;
  nsString mDatabaseName;

  nsString mDirectoryPath;
  nsString mJournalDirectoryPath;

  int64_t mLastFileId;

  // Protected by IndexedDatabaseManager::FileMutex()
  nsDataHashtable<nsUint64HashKey, FileInfo*> mFileInfos;

  bool mInvalidated;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_filemanager_h__
