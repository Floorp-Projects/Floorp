/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemBackgroundRequestHandler.h"

#include "fs/FileSystemChildFactory.h"
#include "mozilla/dom/BackgroundFileSystemChild.h"
#include "mozilla/dom/OriginPrivateFileSystemChild.h"
#include "mozilla/dom/POriginPrivateFileSystem.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsIScriptObjectPrincipal.h"

namespace mozilla::dom {

namespace {

// Not static: BackgroundFileSystemChild must be owned by calling thread
RefPtr<BackgroundFileSystemChild> CreateBackgroundFileSystemChild(
    nsIGlobalObject* aGlobal) {
  using mozilla::dom::BackgroundFileSystemChild;
  using mozilla::ipc::BackgroundChild;
  using mozilla::ipc::PBackgroundChild;
  using mozilla::ipc::PrincipalInfo;

  // TODO: It would be nice to convert all error checks to QM_TRY some time
  //       later.

  // TODO: It would be cleaner if we were called with a PrincipalInfo, instead
  //       of an nsIGlobalObject, so we wouldn't have the dependency on the
  //       workers code.

  PBackgroundChild* backgroundChild =
      BackgroundChild::GetOrCreateForCurrentThread();

  if (NS_WARN_IF(!backgroundChild)) {
    MOZ_ASSERT(false);
    return nullptr;
  }

  RefPtr<BackgroundFileSystemChild> result;

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aGlobal);
    if (!sop) {
      return nullptr;
    }

    nsCOMPtr<nsIPrincipal> principal = sop->GetEffectiveStoragePrincipal();
    if (!principal) {
      return nullptr;
    }

    auto principalInfo = MakeUnique<PrincipalInfo>();
    nsresult rv = PrincipalToPrincipalInfo(principal, principalInfo.get());
    if (NS_FAILED(rv)) {
      return nullptr;
    }

    auto* child = new BackgroundFileSystemChild();

    result = static_cast<BackgroundFileSystemChild*>(
        backgroundChild->SendPBackgroundFileSystemConstructor(child,
                                                              *principalInfo));
  } else {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    if (!workerPrivate) {
      return nullptr;
    }

    const PrincipalInfo& principalInfo =
        workerPrivate->GetEffectiveStoragePrincipalInfo();

    BackgroundFileSystemChild* child = new BackgroundFileSystemChild();

    result = static_cast<BackgroundFileSystemChild*>(
        backgroundChild->SendPBackgroundFileSystemConstructor(child,
                                                              principalInfo));
  }

  MOZ_ASSERT(result);

  return result;
}

}  // namespace

FileSystemBackgroundRequestHandler::FileSystemBackgroundRequestHandler(
    fs::FileSystemChildFactory* aChildFactory)
    : mChildFactory(aChildFactory) {}

FileSystemBackgroundRequestHandler::FileSystemBackgroundRequestHandler()
    : FileSystemBackgroundRequestHandler(new fs::FileSystemChildFactory()) {}

FileSystemBackgroundRequestHandler::~FileSystemBackgroundRequestHandler() =
    default;

RefPtr<FileSystemBackgroundRequestHandler::CreateFileSystemManagerChildPromise>
FileSystemBackgroundRequestHandler::CreateFileSystemManagerChild(
    nsIGlobalObject* aGlobal) {
  using mozilla::ipc::Endpoint;

  // Create a new IPC connection
  Endpoint<POriginPrivateFileSystemParent> parentEndpoint;
  Endpoint<POriginPrivateFileSystemChild> childEndpoint;
  MOZ_ALWAYS_SUCCEEDS(POriginPrivateFileSystem::CreateEndpoints(
      &parentEndpoint, &childEndpoint));

  RefPtr<OriginPrivateFileSystemChild> child = mChildFactory->Create();
  if (!childEndpoint.Bind(child->AsBindable())) {
    return CreateFileSystemManagerChildPromise::CreateAndReject(
        NS_ERROR_FAILURE, __func__);
  }

  // XXX do something (register with global?) so that we can Close() the actor
  // before the event queue starts to shut down.  That will cancel all
  // outstanding async requests (returns lambdas with errors).
  RefPtr<BackgroundFileSystemChild> backgroundFileSystemChild =
      CreateBackgroundFileSystemChild(aGlobal);
  if (!backgroundFileSystemChild) {
    return CreateFileSystemManagerChildPromise::CreateAndReject(
        NS_ERROR_FAILURE, __func__);
  }

  return backgroundFileSystemChild
      ->SendCreateFileSystemManagerParent(std::move(parentEndpoint))
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
