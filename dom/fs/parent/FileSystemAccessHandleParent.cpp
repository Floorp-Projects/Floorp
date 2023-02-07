/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemAccessHandleParent.h"

#include "mozilla/dom/FileSystemAccessHandle.h"

namespace mozilla::dom {

FileSystemAccessHandleParent::FileSystemAccessHandleParent(
    RefPtr<FileSystemAccessHandle> aAccessHandle)
    : mAccessHandle(std::move(aAccessHandle)) {}

FileSystemAccessHandleParent::~FileSystemAccessHandleParent() {
  MOZ_ASSERT(mActorDestroyed);
}

mozilla::ipc::IPCResult FileSystemAccessHandleParent::RecvClose() {
  mAccessHandle->BeginClose();

  return IPC_OK();
}

void FileSystemAccessHandleParent::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_ASSERT(!mActorDestroyed);

  DEBUGONLY(mActorDestroyed = true);

  mAccessHandle->UnregisterActor(WrapNotNullUnchecked(this));

  mAccessHandle = nullptr;
}

}  // namespace mozilla::dom
