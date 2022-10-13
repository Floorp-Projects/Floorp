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

void FileSystemManager::Shutdown() { mBackgroundRequestHandler->Shutdown(); }

FileSystemManagerChild* FileSystemManager::Actor() const {
  return mBackgroundRequestHandler->GetFileSystemManagerChild();
}

already_AddRefed<Promise> FileSystemManager::GetDirectory(ErrorResult& aRv) {
  MOZ_ASSERT(mGlobal);

  QM_TRY_INSPECT(const auto& principalInfo, GetPrincipalInfo(mGlobal), nullptr,
                 [&aRv](const nsresult rv) { aRv.Throw(rv); });

  RefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  MOZ_ASSERT(promise);

  if (Actor()) {
    mRequestHandler->GetRootHandle(this, promise);
    return promise.forget();
  }

  mBackgroundRequestHandler->CreateFileSystemManagerChild(principalInfo)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self = RefPtr<FileSystemManager>(this), promise](bool) {
            self->mRequestHandler->GetRootHandle(self, promise);
          },
          [promise](nsresult) {
            promise->MaybeRejectWithUnknownError(
                "Could not create the file system manager");
          });

  return promise.forget();
}

}  // namespace mozilla::dom
