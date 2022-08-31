/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemBackgroundRequestHandler.h"

#include "fs/FileSystemChildFactory.h"
#include "mozilla/dom/OriginPrivateFileSystemChild.h"
#include "mozilla/dom/POriginPrivateFileSystem.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/PBackgroundChild.h"

namespace mozilla::dom {

FileSystemBackgroundRequestHandler::FileSystemBackgroundRequestHandler(
    fs::FileSystemChildFactory* aChildFactory)
    : mChildFactory(aChildFactory) {}

FileSystemBackgroundRequestHandler::FileSystemBackgroundRequestHandler()
    : FileSystemBackgroundRequestHandler(new fs::FileSystemChildFactory()) {}

FileSystemBackgroundRequestHandler::~FileSystemBackgroundRequestHandler() =
    default;

RefPtr<FileSystemBackgroundRequestHandler::CreateFileSystemManagerChildPromise>
FileSystemBackgroundRequestHandler::CreateFileSystemManagerChild(
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo) {
  using mozilla::ipc::BackgroundChild;
  using mozilla::ipc::Endpoint;
  using mozilla::ipc::PBackgroundChild;

  PBackgroundChild* backgroundChild =
      BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!backgroundChild)) {
    return CreateFileSystemManagerChildPromise::CreateAndReject(
        NS_ERROR_FAILURE, __func__);
  }

  // Create a new IPC connection
  Endpoint<POriginPrivateFileSystemParent> parentEndpoint;
  Endpoint<POriginPrivateFileSystemChild> childEndpoint;
  MOZ_ALWAYS_SUCCEEDS(POriginPrivateFileSystem::CreateEndpoints(
      &parentEndpoint, &childEndpoint));

  RefPtr<OriginPrivateFileSystemChild> child = mChildFactory->Create();
  if (!childEndpoint.Bind(child)) {
    return CreateFileSystemManagerChildPromise::CreateAndReject(
        NS_ERROR_FAILURE, __func__);
  }

  // XXX do something (register with global?) so that we can Close() the actor
  // before the event queue starts to shut down.  That will cancel all
  // outstanding async requests (returns lambdas with errors).
  return backgroundChild
      ->SendCreateFileSystemManagerParent(aPrincipalInfo,
                                          std::move(parentEndpoint))
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [child](nsresult rv) {
            if (NS_FAILED(rv)) {
              return CreateFileSystemManagerChildPromise::CreateAndReject(
                  rv, __func__);
            }

            return CreateFileSystemManagerChildPromise::CreateAndResolve(
                child, __func__);
          },
          [](const mozilla::ipc::ResponseRejectReason&) {
            return CreateFileSystemManagerChildPromise::CreateAndReject(
                NS_ERROR_FAILURE, __func__);
          });
}

}  // namespace mozilla::dom
