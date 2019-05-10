/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileCreatorParent.h"
#include "mozilla/dom/FileBlobImpl.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/MultipartBlobImpl.h"

namespace mozilla {
namespace dom {

FileCreatorParent::FileCreatorParent()
    : mBackgroundEventTarget(GetCurrentThreadEventTarget()), mIPCActive(true) {}

FileCreatorParent::~FileCreatorParent() = default;

mozilla::ipc::IPCResult FileCreatorParent::CreateAndShareFile(
    const nsString& aFullPath, const nsString& aType, const nsString& aName,
    const Maybe<int64_t>& aLastModified, const bool& aExistenceCheck,
    const bool& aIsFromNsIFile) {
  RefPtr<dom::BlobImpl> blobImpl;
  nsresult rv =
      CreateBlobImpl(aFullPath, aType, aName, aLastModified.isSome(),
                     aLastModified.isSome() ? aLastModified.value() : 0,
                     aExistenceCheck, aIsFromNsIFile, getter_AddRefs(blobImpl));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Unused << Send__delete__(this, FileCreationErrorResult(rv));
    return IPC_OK();
  }

  MOZ_ASSERT(blobImpl);

  // FileBlobImpl is unable to return the correct type on this thread because
  // nsIMIMEService is not thread-safe. We must exec the 'type' getter on
  // main-thread before send the blob to the child actor.

  RefPtr<FileCreatorParent> self = this;
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "FileCreatorParent::CreateAndShareFile", [self, blobImpl]() {
        nsAutoString type;
        blobImpl->GetType(type);

        self->mBackgroundEventTarget->Dispatch(NS_NewRunnableFunction(
            "FileCreatorParent::CreateAndShareFile return", [self, blobImpl]() {
              if (self->mIPCActive) {
                IPCBlob ipcBlob;
                nsresult rv = dom::IPCBlobUtils::Serialize(
                    blobImpl, self->Manager(), ipcBlob);
                if (NS_WARN_IF(NS_FAILED(rv))) {
                  Unused << Send__delete__(self, FileCreationErrorResult(rv));
                  return;
                }

                Unused << Send__delete__(self,
                                         FileCreationSuccessResult(ipcBlob));
              }
            }));
      }));

  return IPC_OK();
}

void FileCreatorParent::ActorDestroy(ActorDestroyReason aWhy) {
  mIPCActive = false;
}

/* static */
nsresult FileCreatorParent::CreateBlobImpl(
    const nsAString& aPath, const nsAString& aType, const nsAString& aName,
    bool aLastModifiedPassed, int64_t aLastModified, bool aExistenceCheck,
    bool aIsFromNsIFile, BlobImpl** aBlobImpl) {
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_NewLocalFile(aPath, true, getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = file->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aExistenceCheck) {
    if (!exists) {
      return NS_ERROR_FILE_NOT_FOUND;
    }

    bool isDir;
    rv = file->IsDirectory(&isDir);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (isDir) {
      return NS_ERROR_FILE_IS_DIRECTORY;
    }
  }

  RefPtr<FileBlobImpl> impl = new FileBlobImpl(file);

  // If the file doesn't exist, we cannot have its path, its size and so on.
  // Let's set them now.
  if (!exists) {
    MOZ_ASSERT(!aExistenceCheck);

    impl->SetMozFullPath(aPath);
    impl->SetLastModified(0);
    impl->SetEmptySize();
  }

  if (!aName.IsEmpty()) {
    impl->SetName(aName);
  }

  if (!aType.IsEmpty()) {
    impl->SetType(aType);
  }

  if (aLastModifiedPassed) {
    impl->SetLastModified(aLastModified);
  }

  if (!aIsFromNsIFile) {
    impl->SetMozFullPath(EmptyString());
  }

  impl.forget(aBlobImpl);
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
