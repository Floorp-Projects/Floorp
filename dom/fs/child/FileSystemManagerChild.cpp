/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemManagerChild.h"

#include "FileSystemAccessHandleChild.h"

namespace mozilla::dom {

already_AddRefed<PFileSystemAccessHandleChild>
FileSystemManagerChild::AllocPFileSystemAccessHandleChild(
    const FileDescriptor& aFileDescriptor) {
  return MakeAndAddRef<FileSystemAccessHandleChild>(aFileDescriptor);
}

::mozilla::ipc::IPCResult FileSystemManagerChild::RecvCloseAll(
    CloseAllResolver&& aResolver) {
  // NOTE: getFile() creates blobs that read the data from the child;
  // we'll need to abort any reads and resolve this call only when all
  // blobs are closed.
  for (const auto& item : ManagedPFileSystemAccessHandleChild()) {
    auto* child = static_cast<FileSystemAccessHandleChild*>(item);
    child->Close();
  }

  aResolver(NS_OK);

  return IPC_OK();
}

}  // namespace mozilla::dom
