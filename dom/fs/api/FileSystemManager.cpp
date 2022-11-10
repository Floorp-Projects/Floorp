/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileSystemManager.h"

#include "FileSystemBackgroundRequestHandler.h"
#include "fs/FileSystemRequestHandler.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FileSystemManagerChild.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/StorageManager.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsIScriptObjectPrincipal.h"

namespace mozilla::dom {

namespace {

Result<mozilla::ipc::PrincipalInfo, nsresult> GetPrincipalInfo(
    nsIGlobalObject* aGlobal) {
  using mozilla::ipc::PrincipalInfo;

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aGlobal);
    QM_TRY(MOZ_TO_RESULT(sop));

    nsCOMPtr<nsIPrincipal> principal = sop->GetEffectiveStoragePrincipal();
    QM_TRY(MOZ_TO_RESULT(principal));

    PrincipalInfo principalInfo;
    QM_TRY(MOZ_TO_RESULT(PrincipalToPrincipalInfo(principal, &principalInfo)));

    return std::move(principalInfo);
  }

  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  QM_TRY(MOZ_TO_RESULT(workerPrivate));

  const PrincipalInfo& principalInfo =
      workerPrivate->GetEffectiveStoragePrincipalInfo();

  return principalInfo;
}

}  // namespace

FileSystemManager::FileSystemManager(
    nsIGlobalObject* aGlobal, RefPtr<StorageManager> aStorageManager,
    RefPtr<FileSystemBackgroundRequestHandler> aBackgroundRequestHandler)
    : mGlobal(aGlobal),
      mStorageManager(std::move(aStorageManager)),
      mBackgroundRequestHandler(std::move(aBackgroundRequestHandler)),
      mRequestHandler(new fs::FileSystemRequestHandler()) {}

FileSystemManager::FileSystemManager(nsIGlobalObject* aGlobal,
                                     RefPtr<StorageManager> aStorageManager)
    : FileSystemManager(aGlobal, std::move(aStorageManager),
                        MakeRefPtr<FileSystemBackgroundRequestHandler>()) {}

FileSystemManager::~FileSystemManager() = default;

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileSystemManager)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTING_ADDREF(FileSystemManager);
NS_IMPL_CYCLE_COLLECTING_RELEASE(FileSystemManager);
NS_IMPL_CYCLE_COLLECTION(FileSystemManager, mGlobal, mStorageManager);

void FileSystemManager::Shutdown() {
  mShutdown.Flip();

  if (mBackgroundRequestHandler->FileSystemManagerChildStrongRef()) {
    mBackgroundRequestHandler->FileSystemManagerChildStrongRef()->CloseAll();
  }

  mBackgroundRequestHandler->Shutdown();

  mCreateFileSystemManagerChildPromiseRequestHolder.DisconnectIfExists();
}

void FileSystemManager::BeginRequest(
    std::function<void(const RefPtr<FileSystemManagerChild>&)>&& aSuccess,
    std::function<void(nsresult)>&& aFailure) {
  MOZ_ASSERT(!mShutdown);
  MOZ_ASSERT(mGlobal);

  // Check if we're allowed to use storage
  if (mGlobal->GetStorageAccess() < StorageAccess::eSessionScoped) {
    aFailure(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  if (mBackgroundRequestHandler->FileSystemManagerChildStrongRef()) {
    aSuccess(mBackgroundRequestHandler->FileSystemManagerChildStrongRef());
    return;
  }

  QM_TRY_INSPECT(const auto& principalInfo, GetPrincipalInfo(mGlobal), QM_VOID,
                 [&aFailure](nsresult rv) { aFailure(rv); });

  mBackgroundRequestHandler->CreateFileSystemManagerChild(principalInfo)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self = RefPtr<FileSystemManager>(this),
           success = std::move(aSuccess), failure = std::move(aFailure)](
              const BoolPromise::ResolveOrRejectValue& aValue) {
            self->mCreateFileSystemManagerChildPromiseRequestHolder.Complete();

            if (aValue.IsResolve()) {
              success(self->mBackgroundRequestHandler
                          ->FileSystemManagerChildStrongRef());
            } else {
              failure(aValue.RejectValue());
            }
          })
      ->Track(mCreateFileSystemManagerChildPromiseRequestHolder);
}

already_AddRefed<Promise> FileSystemManager::GetDirectory(ErrorResult& aError) {
  MOZ_ASSERT(mGlobal);

  RefPtr<Promise> promise = Promise::Create(mGlobal, aError);
  if (NS_WARN_IF(aError.Failed())) {
    return nullptr;
  }

  MOZ_ASSERT(promise);

  mRequestHandler->GetRootHandle(this, promise, aError);
  if (NS_WARN_IF(aError.Failed())) {
    return nullptr;
  }

  return promise.forget();
}

}  // namespace mozilla::dom
