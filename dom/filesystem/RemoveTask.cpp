/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoveTask.h"

#include "mozilla/dom/File.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "nsIFile.h"
#include "nsStringGlue.h"

namespace mozilla {
namespace dom {

/* static */ already_AddRefed<RemoveTask>
RemoveTask::Create(FileSystemBase* aFileSystem,
                   nsIFile* aDirPath,
                   nsIFile* aTargetPath,
                   bool aRecursive,
                   ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);
  MOZ_ASSERT(aDirPath);
  MOZ_ASSERT(aTargetPath);

  RefPtr<RemoveTask> task =
    new RemoveTask(aFileSystem, aDirPath, aTargetPath, aRecursive);

  // aTargetPath can be null. In this case SetError will be called.

  nsCOMPtr<nsIGlobalObject> globalObject =
    do_QueryInterface(aFileSystem->GetParentObject());
  if (NS_WARN_IF(!globalObject)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  task->mPromise = Promise::Create(globalObject, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return task.forget();
}

/* static */ already_AddRefed<RemoveTask>
RemoveTask::Create(FileSystemBase* aFileSystem,
                   const FileSystemRemoveParams& aParam,
                   FileSystemRequestParent* aParent,
                   ErrorResult& aRv)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);

  RefPtr<RemoveTask> task =
    new RemoveTask(aFileSystem, aParam, aParent);

  NS_ConvertUTF16toUTF8 directoryPath(aParam.directory());
  aRv = NS_NewNativeLocalFile(directoryPath, true,
                              getter_AddRefs(task->mDirPath));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  task->mRecursive = aParam.recursive();

  NS_ConvertUTF16toUTF8 path(aParam.targetDirectory());
  aRv = NS_NewNativeLocalFile(path, true, getter_AddRefs(task->mTargetPath));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (!FileSystemUtils::IsDescendantPath(task->mDirPath, task->mTargetPath)) {
    aRv.Throw(NS_ERROR_DOM_FILESYSTEM_NO_MODIFICATION_ALLOWED_ERR);
    return nullptr;
  }

  return task.forget();
}

RemoveTask::RemoveTask(FileSystemBase* aFileSystem,
                       nsIFile* aDirPath,
                       nsIFile* aTargetPath,
                       bool aRecursive)
  : FileSystemTaskBase(aFileSystem)
  , mDirPath(aDirPath)
  , mTargetPath(aTargetPath)
  , mRecursive(aRecursive)
  , mReturnValue(false)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);
  MOZ_ASSERT(aDirPath);
  MOZ_ASSERT(aTargetPath);
}

RemoveTask::RemoveTask(FileSystemBase* aFileSystem,
                       const FileSystemRemoveParams& aParam,
                       FileSystemRequestParent* aParent)
  : FileSystemTaskBase(aFileSystem, aParam, aParent)
  , mRecursive(false)
  , mReturnValue(false)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);
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
  return RefPtr<Promise>(mPromise).forget();
}

FileSystemParams
RemoveTask::GetRequestParams(const nsString& aSerializedDOMPath,
                             ErrorResult& aRv) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  FileSystemRemoveParams param;
  param.filesystem() = aSerializedDOMPath;

  aRv = mDirPath->GetPath(param.directory());
  if (NS_WARN_IF(aRv.Failed())) {
    return param;
  }

  param.recursive() = mRecursive;

  nsAutoString path;
  aRv = mTargetPath->GetPath(path);
  if (NS_WARN_IF(aRv.Failed())) {
    return param;
  }

  param.targetDirectory() = path;

  return param;
}

FileSystemResponseValue
RemoveTask::GetSuccessRequestResult(ErrorResult& aRv) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return FileSystemBooleanResponse(mReturnValue);
}

void
RemoveTask::SetSuccessRequestResult(const FileSystemResponseValue& aValue,
                                    ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  FileSystemBooleanResponse r = aValue;
  mReturnValue = r.success();
}

nsresult
RemoveTask::Work()
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(!NS_IsMainThread(), "Only call on worker thread!");

  if (mFileSystem->IsShutdown()) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(FileSystemUtils::IsDescendantPath(mDirPath, mTargetPath));

  bool exists = false;
  nsresult rv = mTargetPath->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    mReturnValue = false;
    return NS_OK;
  }

  bool isFile = false;
  rv = mTargetPath->IsFile(&isFile);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (isFile && !mFileSystem->IsSafeFile(mTargetPath)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  rv = mTargetPath->Remove(mRecursive);
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
    mPromise->MaybeReject(mErrorValue);
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
