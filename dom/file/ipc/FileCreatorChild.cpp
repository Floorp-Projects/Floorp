/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileCreatorChild.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/IPCBlobUtils.h"

namespace mozilla {
namespace dom {

FileCreatorChild::FileCreatorChild() = default;

FileCreatorChild::~FileCreatorChild() { MOZ_ASSERT(!mPromise); }

void FileCreatorChild::SetPromise(Promise* aPromise) {
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(!mPromise);

  mPromise = aPromise;
}

mozilla::ipc::IPCResult FileCreatorChild::Recv__delete__(
    const FileCreationResult& aResult) {
  MOZ_ASSERT(mPromise);

  RefPtr<Promise> promise;
  promise.swap(mPromise);

  if (aResult.type() == FileCreationResult::TFileCreationErrorResult) {
    promise->MaybeReject(aResult.get_FileCreationErrorResult().errorCode());
    return IPC_OK();
  }

  MOZ_ASSERT(aResult.type() == FileCreationResult::TFileCreationSuccessResult);

  RefPtr<dom::BlobImpl> impl = dom::IPCBlobUtils::Deserialize(
      aResult.get_FileCreationSuccessResult().blob());

  RefPtr<File> file = File::Create(promise->GetParentObject(), impl);
  promise->MaybeResolve(file);
  return IPC_OK();
}

void FileCreatorChild::ActorDestroy(ActorDestroyReason aWhy) {
  if (mPromise) {
    mPromise->MaybeReject(NS_ERROR_FAILURE);
    mPromise = nullptr;
  }
};

}  // namespace dom
}  // namespace mozilla
