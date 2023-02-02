/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemAccessHandle.h"

#include "mozilla/Result.h"
#include "mozilla/dom/FileSystemDataManager.h"
#include "mozilla/dom/FileSystemLog.h"

namespace mozilla::dom {

FileSystemAccessHandle::FileSystemAccessHandle(
    RefPtr<fs::data::FileSystemDataManager> aDataManager,
    const fs::EntryId& aEntryId)
    : mEntryId(aEntryId),
      mDataManager(std::move(aDataManager)),
      mClosed(false) {}

FileSystemAccessHandle::~FileSystemAccessHandle() {
  if (IsOpen()) {
    Close();
  }
}

// static
Result<RefPtr<FileSystemAccessHandle>, nsresult> FileSystemAccessHandle::Create(
    RefPtr<fs::data::FileSystemDataManager> aDataManager,
    const fs::EntryId& aEntryId) {
  MOZ_ASSERT(aDataManager);
  aDataManager->AssertIsOnIOTarget();

  if (!aDataManager->LockExclusive(aEntryId)) {
    return Err(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
  }

  RefPtr<FileSystemAccessHandle> accessHandle =
      new FileSystemAccessHandle(std::move(aDataManager), aEntryId);

  return accessHandle;
}

bool FileSystemAccessHandle::IsOpen() const { return !mClosed; }

void FileSystemAccessHandle::Close() {
  MOZ_ASSERT(!mClosed);

  LOG(("Closing AccessHandle"));

  mClosed = true;

  mDataManager->UnlockExclusive(mEntryId);
}

}  // namespace mozilla::dom
