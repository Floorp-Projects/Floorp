/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoveTask.h"

#include "DOMError.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "nsDOMFile.h"
#include "nsIFile.h"
#include "nsStringGlue.h"

namespace mozilla {
namespace dom {

RemoveTask::RemoveTask(FileSystemBase* aFileSystem,
                       const nsAString& aDirPath,
                       DOMFileImpl* aTargetFile,
                       const nsAString& aTargetPath,
                       bool aRecursive,
                       ErrorResult& aRv)
  : FileSystemTaskBase(aFileSystem)
  , mDirRealPath(aDirPath)
  , mTargetFileImpl(aTargetFile)
  , mTargetRealPath(aTargetPath)
  , mRecursive(aRecursive)
  , mReturnValue(false)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);
  nsCOMPtr<nsIGlobalObject> globalObject =
    do_QueryInterface(aFileSystem->GetWindow());
  if (!globalObject) {
    return;
  }
  mPromise = Promise::Create(globalObject, aRv);
}

RemoveTask::RemoveTask(FileSystemBase* aFileSystem,
                       const FileSystemRemoveParams& aParam,
                       FileSystemRequestParent* aParent)
  : FileSystemTaskBase(aFileSystem, aParam, aParent)
  , mRecursive(false)
  , mReturnValue(false)
{
  MOZ_ASSERT(FileSystemUtils::IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);

  mDirRealPath = aParam.directory();

  mRecursive = aParam.recursive();

  const FileSystemPathOrFileValue& target = aParam.target();

  if (target.type() == FileSystemPathOrFileValue::TnsString) {
    mTargetRealPath = target;
    return;
  }

  BlobParent* bp = static_cast<BlobParent*>(static_cast<PBlobParent*>(target));
  mTargetFileImpl = bp->GetBlobImpl();
  MOZ_ASSERT(mTargetFileImpl);
}

RemoveTask::~RemoveTask()
{
  MOZ_ASSERT(!mPromise || NS_IsMainThread(),
             "mPromise should be released on main thread!");
}

already_AddRefed<Promise>
RemoveTask::GetPromise()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return nsRefPtr<Promise>(mPromise).forget();
}

FileSystemParams
RemoveTask::GetRequestParams(const nsString& aFileSystem) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  FileSystemRemoveParams param;
  param.filesystem() = aFileSystem;
  param.directory() = mDirRealPath;
  param.recursive() = mRecursive;
  if (mTargetFileImpl) {
    nsRefPtr<DOMFile> file = new DOMFile(mFileSystem->GetWindow(),
                                         mTargetFileImpl);
    BlobChild* actor
      = ContentChild::GetSingleton()->GetOrCreateActorForBlob(file);
    if (actor) {
      param.target() = actor;
    }
  } else {
    param.target() = mTargetRealPath;
  }
  return param;
}

FileSystemResponseValue
RemoveTask::GetSuccessRequestResult() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return FileSystemBooleanResponse(mReturnValue);
}

void
RemoveTask::SetSuccessRequestResult(const FileSystemResponseValue& aValue)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  FileSystemBooleanResponse r = aValue;
  mReturnValue = r.success();
}

nsresult
RemoveTask::Work()
{
  MOZ_ASSERT(FileSystemUtils::IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(!NS_IsMainThread(), "Only call on worker thread!");

  if (mFileSystem->IsShutdown()) {
    return NS_ERROR_FAILURE;
  }

  // Get the DOM path if a DOMFile is passed as the target.
  if (mTargetFileImpl) {
    if (!mFileSystem->GetRealPath(mTargetFileImpl, mTargetRealPath)) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }
    if (!FileSystemUtils::IsDescendantPath(mDirRealPath, mTargetRealPath)) {
      return NS_ERROR_DOM_FILESYSTEM_NO_MODIFICATION_ALLOWED_ERR;
    }
  }

  nsCOMPtr<nsIFile> file = mFileSystem->GetLocalFile(mTargetRealPath);
  if (!file) {
    return NS_ERROR_DOM_FILESYSTEM_INVALID_PATH_ERR;
  }

  bool exists = false;
  nsresult rv = file->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    mReturnValue = false;
    return NS_OK;
  }

  bool isFile = false;
  rv = file->IsFile(&isFile);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (isFile && !mFileSystem->IsSafeFile(file)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  rv = file->Remove(mRecursive);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mReturnValue = true;

  return NS_OK;
}

void
RemoveTask::HandlerCallback()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  if (mFileSystem->IsShutdown()) {
    mPromise = nullptr;
    return;
  }

  if (HasError()) {
    nsRefPtr<DOMError> domError = new DOMError(mFileSystem->GetWindow(),
      mErrorValue);
    mPromise->MaybeRejectBrokenly(domError);
    mPromise = nullptr;
    return;
  }

  mPromise->MaybeResolve(mReturnValue);
  mPromise = nullptr;
}

void
RemoveTask::GetPermissionAccessType(nsCString& aAccess) const
{
  aAccess.AssignLiteral("write");
}

} // namespace dom
} // namespace mozilla
