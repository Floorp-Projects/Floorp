/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TemporaryIPCBlobChild.h"
#include "mozilla/dom/MutableBlobStorage.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include <private/pprio.h>

namespace mozilla {
namespace dom {

TemporaryIPCBlobChild::TemporaryIPCBlobChild(MutableBlobStorage* aStorage)
    : mMutableBlobStorage(aStorage), mActive(true) {
  MOZ_ASSERT(aStorage);
}

TemporaryIPCBlobChild::~TemporaryIPCBlobChild() = default;

mozilla::ipc::IPCResult TemporaryIPCBlobChild::RecvFileDesc(
    const FileDescriptor& aFD) {
  MOZ_ASSERT(mActive);

  auto rawFD = aFD.ClonePlatformHandle();
  PRFileDesc* prfile = PR_ImportFile(PROsfd(rawFD.release()));

  mMutableBlobStorage->TemporaryFileCreated(prfile);
  mMutableBlobStorage = nullptr;
  return IPC_OK();
}

mozilla::ipc::IPCResult TemporaryIPCBlobChild::Recv__delete__(
    const IPCBlobOrError& aData) {
  mActive = false;
  mMutableBlobStorage = nullptr;

  if (aData.type() == IPCBlobOrError::TIPCBlob) {
    // This must be always deserialized.
    RefPtr<BlobImpl> blobImpl = IPCBlobUtils::Deserialize(aData.get_IPCBlob());
    MOZ_ASSERT(blobImpl);

    if (mCallback) {
      mCallback->OperationSucceeded(blobImpl);
    }
  } else if (mCallback) {
    MOZ_ASSERT(aData.type() == IPCBlobOrError::Tnsresult);
    mCallback->OperationFailed(aData.get_nsresult());
  }

  mCallback = nullptr;

  return IPC_OK();
}

void TemporaryIPCBlobChild::ActorDestroy(ActorDestroyReason aWhy) {
  mActive = false;
  mMutableBlobStorage = nullptr;

  if (mCallback) {
    mCallback->OperationFailed(NS_ERROR_FAILURE);
    mCallback = nullptr;
  }
}

void TemporaryIPCBlobChild::AskForBlob(TemporaryIPCBlobChildCallback* aCallback,
                                       const nsACString& aContentType,
                                       PRFileDesc* aFD) {
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(!mCallback);

  if (!mActive) {
    aCallback->OperationFailed(NS_ERROR_FAILURE);
    return;
  }

  FileDescriptor fdd = FileDescriptor(
      FileDescriptor::PlatformHandleType(PR_FileDesc2NativeHandle(aFD)));

  mCallback = aCallback;
  SendOperationDone(nsCString(aContentType), fdd);
}

}  // namespace dom
}  // namespace mozilla
