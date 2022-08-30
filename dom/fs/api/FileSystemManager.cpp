/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileSystemManager.h"

#include "FileSystemBackgroundRequestHandler.h"
#include "fs/FileSystemRequestHandler.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Promise.h"

namespace mozilla::dom {

FileSystemManager::FileSystemManager(nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal),
      mBackgroundRequestHandler(new FileSystemBackgroundRequestHandler()),
      mRequestHandler(new fs::FileSystemRequestHandler()) {}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileSystemManager)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTING_ADDREF(FileSystemManager);
NS_IMPL_CYCLE_COLLECTING_RELEASE(FileSystemManager);
NS_IMPL_CYCLE_COLLECTION(FileSystemManager, mGlobal);

already_AddRefed<Promise> FileSystemManager::GetDirectory(ErrorResult& aRv) {
  MOZ_ASSERT(mGlobal);

  RefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  MOZ_ASSERT(promise);

  mBackgroundRequestHandler->CreateFileSystemManagerChild(mGlobal)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self = RefPtr<FileSystemManager>(this),
       promise](const RefPtr<OriginPrivateFileSystemChild>& child) {
        RefPtr<FileSystemActorHolder> actorHolder =
            MakeAndAddRef<FileSystemActorHolder>(child);
        self->mRequestHandler->GetRootHandle(actorHolder, promise);
      },
      [promise](nsresult) {
        promise->MaybeRejectWithUnknownError(
            "Could not create the file system manager");
      });

  return promise.forget();
}

}  // namespace mozilla::dom
