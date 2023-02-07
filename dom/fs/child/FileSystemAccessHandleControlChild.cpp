/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemAccessHandleControlChild.h"

#include "mozilla/dom/FileSystemSyncAccessHandle.h"

namespace mozilla::dom {

void FileSystemAccessHandleControlChild::SetAccessHandle(
    FileSystemSyncAccessHandle* aAccessHandle) {
  MOZ_ASSERT(aAccessHandle);
  MOZ_ASSERT(!mAccessHandle);

  mAccessHandle = aAccessHandle;
}

void FileSystemAccessHandleControlChild::Shutdown() {
  if (!CanSend()) {
    return;
  }

  Close();
}

void FileSystemAccessHandleControlChild::ActorDestroy(
    ActorDestroyReason /* aWhy */) {
  if (mAccessHandle) {
    mAccessHandle->ClearControlActor();
    mAccessHandle = nullptr;
  }
}

}  // namespace mozilla::dom
