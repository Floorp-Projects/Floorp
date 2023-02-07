/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemAccessHandleControlParent.h"

#include "mozilla/dom/FileSystemAccessHandle.h"
#include "mozilla/ipc/IPCCore.h"

namespace mozilla::dom {

FileSystemAccessHandleControlParent::FileSystemAccessHandleControlParent(
    RefPtr<FileSystemAccessHandle> aAccessHandle)
    : mAccessHandle(std::move(aAccessHandle)) {}

FileSystemAccessHandleControlParent::~FileSystemAccessHandleControlParent() {
  MOZ_ASSERT(mActorDestroyed);
}

mozilla::ipc::IPCResult FileSystemAccessHandleControlParent::RecvClose(
    CloseResolver&& aResolver) {
  mAccessHandle->BeginClose()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [resolver = std::move(aResolver)](
          const BoolPromise::ResolveOrRejectValue&) { resolver(void_t()); });

  return IPC_OK();
}

void FileSystemAccessHandleControlParent::ActorDestroy(
    ActorDestroyReason /* aWhy */) {
  MOZ_ASSERT(!mActorDestroyed);

#ifdef DEBUG
  mActorDestroyed = true;
#endif

  mAccessHandle->UnregisterControlActor(WrapNotNullUnchecked(this));

  mAccessHandle = nullptr;
}

}  // namespace mozilla::dom
