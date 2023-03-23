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
#include "mozilla/dom/fs/ManagedMozPromiseRequestHolder.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"

namespace mozilla::dom {

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

FileSystemManager::~FileSystemManager() { MOZ_ASSERT(mShutdown); }

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileSystemManager)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTING_ADDREF(FileSystemManager);
NS_IMPL_CYCLE_COLLECTING_RELEASE(FileSystemManager);
NS_IMPL_CYCLE_COLLECTION(FileSystemManager, mGlobal, mStorageManager);

void FileSystemManager::Shutdown() {
  mShutdown.Flip();

  auto shutdownAndDisconnect = [self = RefPtr(this)]() {
    self->mBackgroundRequestHandler->Shutdown();

    for (RefPtr<PromiseRequestHolder<BoolPromise>> holder :
         self->mPromiseRequestHolders.ForwardRange()) {
      holder->DisconnectIfExists();
    }
  };

  if (NS_IsMainThread()) {
    if (mBackgroundRequestHandler->FileSystemManagerChildStrongRef()) {
      mBackgroundRequestHandler->FileSystemManagerChildStrongRef()
          ->CloseAllWritables(
              [shutdownAndDisconnect = std::move(shutdownAndDisconnect)]() {
                shutdownAndDisconnect();
              });
    } else {
      shutdownAndDisconnect();
    }
  } else {
    if (mBackgroundRequestHandler->FileSystemManagerChildStrongRef()) {
      // FileSystemAccessHandles and FileSystemWritableFileStreams prevent
      // shutdown until they are full closed, so at this point, they all should
      // be closed.
      MOZ_ASSERT(mBackgroundRequestHandler->FileSystemManagerChildStrongRef()
                     ->AllSyncAccessHandlesClosed());
      MOZ_ASSERT(mBackgroundRequestHandler->FileSystemManagerChildStrongRef()
                     ->AllWritableFileStreamsClosed());
    }

    shutdownAndDisconnect();
  }
}

const RefPtr<FileSystemManagerChild>& FileSystemManager::ActorStrongRef()
    const {
  return mBackgroundRequestHandler->FileSystemManagerChildStrongRef();
}

void FileSystemManager::RegisterPromiseRequestHolder(
    PromiseRequestHolder<BoolPromise>* aHolder) {
  mPromiseRequestHolders.AppendElement(aHolder);
}

void FileSystemManager::UnregisterPromiseRequestHolder(
    PromiseRequestHolder<BoolPromise>* aHolder) {
  mPromiseRequestHolders.RemoveElement(aHolder);
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

  QM_TRY_INSPECT(const auto& principalInfo, mGlobal->GetStorageKey(), QM_VOID,
                 [&aFailure](nsresult rv) { aFailure(rv); });

  auto holder = MakeRefPtr<PromiseRequestHolder<BoolPromise>>(this);

  mBackgroundRequestHandler->CreateFileSystemManagerChild(principalInfo)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = RefPtr<FileSystemManager>(this), holder,
              success = std::move(aSuccess), failure = std::move(aFailure)](
                 const BoolPromise::ResolveOrRejectValue& aValue) {
               holder->Complete();

               if (aValue.IsResolve()) {
                 success(self->mBackgroundRequestHandler
                             ->FileSystemManagerChildStrongRef());
               } else {
                 failure(aValue.RejectValue());
               }
             })
      ->Track(*holder);
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
