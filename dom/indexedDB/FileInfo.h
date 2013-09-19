/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_fileinfo_h__
#define mozilla_dom_indexeddb_fileinfo_h__

#include "IndexedDatabase.h"

#include "nsThreadUtils.h"

#include "FileManager.h"
#include "IndexedDatabaseManager.h"

BEGIN_INDEXEDDB_NAMESPACE

class FileInfo
{
  friend class FileManager;

public:
  FileInfo(FileManager* aFileManager)
  : mFileManager(aFileManager)
  { }

  virtual ~FileInfo()
  { }

  static
  FileInfo* Create(FileManager* aFileManager, int64_t aId);

  void AddRef()
  {
    if (IndexedDatabaseManager::IsClosed()) {
      ++mRefCnt;
    }
    else {
      UpdateReferences(mRefCnt, 1);
    }
  }

  void Release()
  {
    if (IndexedDatabaseManager::IsClosed()) {
      nsrefcnt count = --mRefCnt;
      if (count == 0) {
        mRefCnt = 1;
        delete this;
      }
    }
    else {
      UpdateReferences(mRefCnt, -1);
    }
  }

  void UpdateDBRefs(int32_t aDelta)
  {
    UpdateReferences(mDBRefCnt, aDelta);
  }

  void ClearDBRefs()
  {
    UpdateReferences(mDBRefCnt, 0, true);
  }

  void UpdateSliceRefs(int32_t aDelta)
  {
    UpdateReferences(mSliceRefCnt, aDelta);
  }

  void GetReferences(int32_t* aRefCnt, int32_t* aDBRefCnt,
                     int32_t* aSliceRefCnt);

  FileManager* Manager() const
  {
    return mFileManager;
  }

  virtual int64_t Id() const = 0;

private:
  void UpdateReferences(ThreadSafeAutoRefCnt& aRefCount, int32_t aDelta,
                        bool aClear = false);
  void Cleanup();

  ThreadSafeAutoRefCnt mRefCnt;
  ThreadSafeAutoRefCnt mDBRefCnt;
  ThreadSafeAutoRefCnt mSliceRefCnt;

  nsRefPtr<FileManager> mFileManager;
};

#define FILEINFO_SUBCLASS(_bits)                                              \
class FileInfo##_bits : public FileInfo                                       \
{                                                                             \
public:                                                                       \
  FileInfo##_bits(FileManager* aFileManager, int##_bits##_t aId)              \
  : FileInfo(aFileManager), mId(aId)                                          \
  { }                                                                         \
                                                                              \
  virtual int64_t Id() const                                                  \
  {                                                                           \
    return mId;                                                               \
  }                                                                           \
                                                                              \
private:                                                                      \
  int##_bits##_t mId;                                                         \
};

FILEINFO_SUBCLASS(16)
FILEINFO_SUBCLASS(32)
FILEINFO_SUBCLASS(64)

#undef FILEINFO_SUBCLASS

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_fileinfo_h__
