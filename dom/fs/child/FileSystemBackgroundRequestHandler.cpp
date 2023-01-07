/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemBackgroundRequestHandler.h"

#include "fs/FileSystemChildFactory.h"
#include "mozilla/dom/FileSystemManagerChild.h"
#include "mozilla/dom/PFileSystemManager.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/PBackgroundChild.h"

namespace mozilla::dom {

FileSystemBackgroundRequestHandler::FileSystemBackgroundRequestHandler(
    fs::FileSystemChildFactory* aChildFactory)
    : mChildFactory(aChildFactory), mCreatingFileSystemManagerChild(false) {}

FileSystemBackgroundRequestHandler::FileSystemBackgroundRequestHandler(
    RefPtr<FileSystemManagerChild> aFileSystemManagerChild)
    : mFileSystemManagerChild(std::move(aFileSystemManagerChild)),
      mCreatingFileSystemManagerChild(false) {}

FileSystemBackgroundRequestHandler::FileSystemBackgroundRequestHandler()
    : FileSystemBackgroundRequestHandler(new fs::FileSystemChildFactory()) {}

FileSystemBackgroundRequestHandler::~FileSystemBackgroundRequestHandler() =
    default;

void FileSystemBackgroundRequestHandler::ClearActor() {
  MOZ_ASSERT(mFileSystemManagerChild);

  mFileSystemManagerChild = nullptr;
}

void FileSystemBackgroundRequestHandler::Shutdown() {
  mShutdown.Flip();

  if (mFileSystemManagerChild) {
    MOZ_ASSERT(!mCreatingFileSystemManagerChild);

    mFileSystemManagerChild->Shutdown();

    mFileSystemManagerChild = nullptr;

    return;
  }

  if (mCreatingFileSystemManagerChild) {
    MOZ_ASSERT(!mFileSystemManagerChild);

    mCreateFileSystemManagerParentPromiseRequestHolder.Disconnect();

    mCreatingFileSystemManagerChild = false;

    // We must either resolve/reject the promise or steal the internal promise
    // before the holder is destroyed. The former isn't possible during
    // shutdown.
    Unused << mCreateFileSystemManagerChildPromiseHolder.Steal();
  }
}

const RefPtr<FileSystemManagerChild>&
FileSystemBackgroundRequestHandler::FileSystemManagerChildStrongRef() const {
  return mFileSystemManagerChild;
}

RefPtr<BoolPromise>
FileSystemBackgroundRequestHandler::CreateFileSystemManagerChild(
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo) {
  MOZ_ASSERT(!mFileSystemManagerChild);
  MOZ_ASSERT(!mShutdown);

  using mozilla::ipc::BackgroundChild;
  using mozilla::ipc::Endpoint;
  using mozilla::ipc::PBackgroundChild;

  if (!mCreatingFileSystemManagerChild) {
    PBackgroundChild* backgroundChild =
        BackgroundChild::GetOrCreateForCurrentThread();
    if (NS_WARN_IF(!backgroundChild)) {
      return BoolPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
    }

    // Create a new IPC connection
    Endpoint<PFileSystemManagerParent> parentEndpoint;
    Endpoint<PFileSystemManagerChild> childEndpoint;
    MOZ_ALWAYS_SUCCEEDS(
        PFileSystemManager::CreateEndpoints(&parentEndpoint, &childEndpoint));

    RefPtr<FileSystemManagerChild> child = mChildFactory->Create();
    if (!childEndpoint.Bind(child)) {
      return BoolPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
    }

    mCreatingFileSystemManagerChild = true;

    backgroundChild
        ->SendCreateFileSystemManagerParent(aPrincipalInfo,
                                            std::move(parentEndpoint))
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [self = RefPtr<FileSystemBackgroundRequestHandler>(this),
             child](nsresult rv) {
              self->mCreateFileSystemManagerParentPromiseRequestHolder
                  .Complete();

              self->mCreatingFileSystemManagerChild = false;

              if (NS_FAILED(rv)) {
                self->mCreateFileSystemManagerChildPromiseHolder.RejectIfExists(
                    rv, __func__);
              } else {
                self->mFileSystemManagerChild = child;

                self->mFileSystemManagerChild->SetBackgroundRequestHandler(
                    self);

                self->mCreateFileSystemManagerChildPromiseHolder
                    .ResolveIfExists(true, __func__);
              }
            },
            [self = RefPtr<FileSystemBackgroundRequestHandler>(this)](
                const mozilla::ipc::ResponseRejectReason&) {
              self->mCreateFileSystemManagerParentPromiseRequestHolder
                  .Complete();

              self->mCreatingFileSystemManagerChild = false;

              self->mCreateFileSystemManagerChildPromiseHolder.RejectIfExists(
                  NS_ERROR_FAILURE, __func__);
            })
        ->Track(mCreateFileSystemManagerParentPromiseRequestHolder);
  }

  return mCreateFileSystemManagerChildPromiseHolder.Ensure(__func__);
}

}  // namespace mozilla::dom
