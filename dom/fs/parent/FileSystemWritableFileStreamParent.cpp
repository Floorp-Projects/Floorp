/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemWritableFileStreamParent.h"

#include "FileSystemDataManager.h"
#include "mozilla/dom/FileSystemLog.h"
#include "mozilla/dom/FileSystemManagerParent.h"

namespace mozilla::dom {

FileSystemWritableFileStreamParent::FileSystemWritableFileStreamParent(
    RefPtr<FileSystemManagerParent> aManager, const fs::EntryId& aEntryId)
    : mManager(std::move(aManager)), mEntryId(aEntryId) {}

FileSystemWritableFileStreamParent::~FileSystemWritableFileStreamParent() {
  MOZ_ASSERT(mClosed);
}

mozilla::ipc::IPCResult FileSystemWritableFileStreamParent::RecvClose() {
  Close();

  return IPC_OK();
}

void FileSystemWritableFileStreamParent::ActorDestroy(ActorDestroyReason aWhy) {
  if (!IsClosed()) {
    Close();
  }
}

void FileSystemWritableFileStreamParent::Close() {
  LOG(("Closing WritableFileStream"));

  mClosed.Flip();

  mManager->DataManagerStrongRef()->UnlockShared(mEntryId);
}

}  // namespace mozilla::dom
