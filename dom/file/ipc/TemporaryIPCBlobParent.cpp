/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TemporaryIPCBlobParent.h"

#include "mozilla/dom/FileBlobImpl.h"
#include "nsAnonymousTemporaryFile.h"
#include "TemporaryFileBlobImpl.h"

namespace mozilla {
namespace dom {

TemporaryIPCBlobParent::TemporaryIPCBlobParent() : mActive(true) {}

TemporaryIPCBlobParent::~TemporaryIPCBlobParent() {
  // If we still have mFile, let's remove it.
  if (mFile) {
    mFile->Remove(false);
  }
}

mozilla::ipc::IPCResult TemporaryIPCBlobParent::CreateAndShareFile() {
  MOZ_ASSERT(mActive);
  MOZ_ASSERT(!mFile);

  nsresult rv = NS_OpenAnonymousTemporaryNsIFile(getter_AddRefs(mFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return SendDeleteError(rv);
  }

  PRFileDesc* fd;
  rv = mFile->OpenNSPRFileDesc(PR_RDWR, PR_IRWXU, &fd);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return SendDeleteError(rv);
  }

  FileDescriptor fdd = FileDescriptor(
      FileDescriptor::PlatformHandleType(PR_FileDesc2NativeHandle(fd)));

  // The FileDescriptor object owns a duplicate of the file handle; we
  // must close the original (and clean up the NSPR descriptor).
  PR_Close(fd);

  Unused << SendFileDesc(fdd);
  return IPC_OK();
}

mozilla::ipc::IPCResult TemporaryIPCBlobParent::RecvOperationFailed() {
  MOZ_ASSERT(mActive);
  mActive = false;

  // Nothing to do.
  Unused << Send__delete__(this, NS_ERROR_FAILURE);
  return IPC_OK();
}

mozilla::ipc::IPCResult TemporaryIPCBlobParent::RecvOperationDone(
    const nsCString& aContentType, const FileDescriptor& aFD) {
  MOZ_ASSERT(mActive);
  mActive = false;

  // We have received a file descriptor because in this way we have kept the
  // file locked on windows during the IPC communication. After the creation of
  // the TemporaryFileBlobImpl, this prfile can be closed.
  auto rawFD = aFD.ClonePlatformHandle();
  PRFileDesc* prfile = PR_ImportFile(PROsfd(rawFD.release()));

  // Let's create the BlobImpl.
  nsCOMPtr<nsIFile> file = std::move(mFile);

  RefPtr<TemporaryFileBlobImpl> blobImpl =
      new TemporaryFileBlobImpl(file, NS_ConvertUTF8toUTF16(aContentType));

  PR_Close(prfile);

  IPCBlob ipcBlob;
  nsresult rv = IPCBlobUtils::Serialize(blobImpl, Manager(), ipcBlob);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Unused << Send__delete__(this, NS_ERROR_FAILURE);
    return IPC_OK();
  }

  Unused << Send__delete__(this, ipcBlob);
  return IPC_OK();
}

void TemporaryIPCBlobParent::ActorDestroy(ActorDestroyReason aWhy) {
  mActive = false;
}

mozilla::ipc::IPCResult TemporaryIPCBlobParent::SendDeleteError(nsresult aRv) {
  MOZ_ASSERT(mActive);
  mActive = false;

  Unused << Send__delete__(this, aRv);
  return IPC_OK();
}

}  // namespace dom
}  // namespace mozilla
