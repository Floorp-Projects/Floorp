/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_fileinfot_h__
#define mozilla_dom_indexeddb_fileinfot_h__

#include "nsISupportsImpl.h"
#include "nsCOMPtr.h"
#include "SafeRefPtr.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

template <typename FileManager>
class FileInfoT final {
 public:
  using AutoLock = typename FileManager::AutoLock;

  FileInfoT(const typename FileManager::FileManagerGuard& aGuard,
            SafeRefPtr<FileManager> aFileManager, const int64_t aFileId,
            const nsrefcnt aInitialDBRefCnt = 0);

  void AddRef();
  void Release(const bool aSyncDeleteFile = false);

  void UpdateDBRefs(int32_t aDelta);

  void GetReferences(int32_t* aRefCnt, int32_t* aDBRefCnt);

  FileManager& Manager() const;

  int64_t Id() const;

  nsCOMPtr<nsIFile> GetFileForFileInfo() const;

  bool LockedClearDBRefs(const typename FileManager::FileManagerGuard& aGuard);

 private:
  void UpdateReferences(ThreadSafeAutoRefCnt& aRefCount, int32_t aDelta,
                        bool aSyncDeleteFile = false);

  void Cleanup();

  const int64_t mFileId;

  ThreadSafeAutoRefCnt mRefCnt;
  ThreadSafeAutoRefCnt mDBRefCnt;

  const SafeRefPtr<FileManager> mFileManager;
};

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_indexeddb_fileinfot_h__
