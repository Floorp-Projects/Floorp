/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_filemanager_h__
#define mozilla_dom_indexeddb_filemanager_h__

#include "IndexedDatabase.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsIDOMFile.h"
#include "nsDataHashtable.h"

class mozIStorageConnection;

BEGIN_INDEXEDDB_NAMESPACE

class FileInfo;

class FileManager
{
  friend class FileInfo;

public:
  FileManager(const nsACString& aOrigin,
              const nsAString& aDatabaseName)
  : mOrigin(aOrigin), mDatabaseName(aDatabaseName), mLastFileId(0),
    mLoaded(false), mInvalidated(false)
  { }

  ~FileManager()
  { }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FileManager)

  const nsACString& Origin() const
  {
    return mOrigin;
  }

  const nsAString& DatabaseName() const
  {
    return mDatabaseName;
  }

  bool Inited() const
  {
    return !mDirectoryPath.IsEmpty();
  }

  bool Loaded() const
  {
    return mLoaded;
  }

  bool Invalidated() const
  {
    return mInvalidated;
  }

  nsresult Init(nsIFile* aDirectory,
                mozIStorageConnection* aConnection);

  nsresult Load(mozIStorageConnection* aConnection);

  nsresult Invalidate();

  already_AddRefed<nsIFile> GetDirectory();

  already_AddRefed<FileInfo> GetFileInfo(PRInt64 aId);

  already_AddRefed<FileInfo> GetNewFileInfo();

  static already_AddRefed<nsIFile> GetFileForId(nsIFile* aDirectory,
                                                PRInt64 aId);

private:
  nsCString mOrigin;
  nsString mDatabaseName;

  nsString mDirectoryPath;

  PRInt64 mLastFileId;

  // Protected by IndexedDatabaseManager::FileMutex()
  nsDataHashtable<nsUint64HashKey, FileInfo*> mFileInfos;

  bool mLoaded;
  bool mInvalidated;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_filemanager_h__
