/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemManagerChild.h"

#include "FileSystemAccessHandleChild.h"
#include "FileSystemBackgroundRequestHandler.h"
#include "FileSystemWritableFileStreamChild.h"
#include "mozilla/dom/FileSystemSyncAccessHandle.h"
#include "mozilla/dom/FileSystemWritableFileStream.h"

namespace mozilla::dom {

void FileSystemManagerChild::SetBackgroundRequestHandler(
    FileSystemBackgroundRequestHandler* aBackgroundRequestHandler) {
  MOZ_ASSERT(aBackgroundRequestHandler);
  MOZ_ASSERT(!mBackgroundRequestHandler);

  mBackgroundRequestHandler = aBackgroundRequestHandler;
}

void FileSystemManagerChild::CloseAllWritables(
    std::function<void()>&& aCallback) {
  nsTArray<RefPtr<BoolPromise>> promises;
  CloseAllWritablesImpl(promises);

  BoolPromise::AllSettled(GetCurrentSerialEventTarget(), promises)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [callback = std::move(aCallback)](
                 const BoolPromise::AllSettledPromiseType::ResolveOrRejectValue&
                 /* aValues */) { callback(); });
}

#ifdef DEBUG
bool FileSystemManagerChild::AllSyncAccessHandlesClosed() const {
  for (const auto& item : ManagedPFileSystemAccessHandleChild()) {
    auto* child = static_cast<FileSystemAccessHandleChild*>(item);
    auto* handle = child->MutableAccessHandlePtr();

    if (!handle->IsClosed()) {
      return false;
    }
  }

  return true;
}

bool FileSystemManagerChild::AllWritableFileStreamsClosed() const {
  for (const auto& item : ManagedPFileSystemWritableFileStreamChild()) {
    auto* const child = static_cast<FileSystemWritableFileStreamChild*>(item);
    auto* const handle = child->MutableWritableFileStreamPtr();
    if (!handle) {
      continue;
    }

    if (!handle->IsDone()) {
      return false;
    }
  }

  return true;
}

#endif

void FileSystemManagerChild::Shutdown() {
  if (!CanSend()) {
    return;
  }

  Close();
}

already_AddRefed<PFileSystemWritableFileStreamChild>
FileSystemManagerChild::AllocPFileSystemWritableFileStreamChild() {
  return MakeAndAddRef<FileSystemWritableFileStreamChild>();
}

::mozilla::ipc::IPCResult FileSystemManagerChild::RecvCloseAll(
    CloseAllResolver&& aResolver) {
  nsTArray<RefPtr<BoolPromise>> promises;

  // NOTE: getFile() creates blobs that read the data from the child;
  // we'll need to abort any reads and resolve this call only when all
  // blobs are closed.

  for (const auto& item : ManagedPFileSystemAccessHandleChild()) {
    auto* child = static_cast<FileSystemAccessHandleChild*>(item);
    auto* handle = child->MutableAccessHandlePtr();

    if (handle->IsOpen()) {
      promises.AppendElement(handle->BeginClose());
    } else if (handle->IsClosing()) {
      promises.AppendElement(handle->OnClose());
    }
  }

  CloseAllWritablesImpl(promises);

  BoolPromise::AllSettled(GetCurrentSerialEventTarget(), promises)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [resolver = std::move(aResolver)](
                 const BoolPromise::AllSettledPromiseType::ResolveOrRejectValue&
                 /* aValues */) { resolver(NS_OK); });

  return IPC_OK();
}

void FileSystemManagerChild::ActorDestroy(ActorDestroyReason aWhy) {
  if (mBackgroundRequestHandler) {
    mBackgroundRequestHandler->ClearActor();
    mBackgroundRequestHandler = nullptr;
  }
}

template <class T>
void FileSystemManagerChild::CloseAllWritablesImpl(T& aPromises) {
  for (const auto& item : ManagedPFileSystemWritableFileStreamChild()) {
    auto* const child = static_cast<FileSystemWritableFileStreamChild*>(item);
    auto* const handle = child->MutableWritableFileStreamPtr();

    if (handle) {
      if (handle->IsOpen()) {
        aPromises.AppendElement(handle->BeginAbort());
      } else if (handle->IsFinishing()) {
        aPromises.AppendElement(handle->OnDone());
      }
    }
  }
}

}  // namespace mozilla::dom
