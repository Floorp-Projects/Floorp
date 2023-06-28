/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemWritableFileStreamParent.h"

#include "FileSystemDataManager.h"
#include "FileSystemStreamCallbacks.h"
#include "mozilla/dom/FileSystemLog.h"
#include "mozilla/dom/FileSystemManagerParent.h"
#include "mozilla/dom/quota/RemoteQuotaObjectParent.h"

namespace mozilla::dom {

class FileSystemWritableFileStreamParent::FileSystemWritableFileStreamCallbacks
    : public FileSystemStreamCallbacks {
 public:
  void CloseRemoteQuotaObjectParent() {
    if (mRemoteQuotaObjectParent) {
      mRemoteQuotaObjectParent->Close();
    }
  }
};

FileSystemWritableFileStreamParent::FileSystemWritableFileStreamParent(
    RefPtr<FileSystemManagerParent> aManager, const fs::EntryId& aEntryId,
    const fs::FileId& aTemporaryFileId, bool aIsExclusive)
    : mManager(std::move(aManager)),
      mEntryId(aEntryId),
      mTemporaryFileId(aTemporaryFileId),
      mIsExclusive(aIsExclusive) {}

FileSystemWritableFileStreamParent::~FileSystemWritableFileStreamParent() {
  MOZ_ASSERT(mClosed);
}

mozilla::ipc::IPCResult FileSystemWritableFileStreamParent::RecvClose(
    bool aAbort, CloseResolver&& aResolver) {
  Close(aAbort);

  aResolver(void_t());

  return IPC_OK();
}

void FileSystemWritableFileStreamParent::ActorDestroy(ActorDestroyReason aWhy) {
  if (mStreamCallbacks) {
    mStreamCallbacks->CloseRemoteQuotaObjectParent();
    mStreamCallbacks = nullptr;
  }

  if (!IsClosed()) {
    Close(/* aAbort */ true);
  }
}

nsIInterfaceRequestor*
FileSystemWritableFileStreamParent::GetOrCreateStreamCallbacks() {
  if (!mStreamCallbacks) {
    if (mClosed) {
      return nullptr;
    }

    mStreamCallbacks = MakeRefPtr<FileSystemWritableFileStreamCallbacks>();
  }

  return mStreamCallbacks.get();
}

void FileSystemWritableFileStreamParent::Close(bool aAbort) {
  LOG(("Closing WritableFileStream"));

  mClosed.Flip();

  if (mIsExclusive) {
    mManager->DataManagerStrongRef()->UnlockExclusive(mEntryId);
  } else {
    mManager->DataManagerStrongRef()->UnlockShared(mEntryId, mTemporaryFileId,
                                                   aAbort);
  }
}

}  // namespace mozilla::dom
