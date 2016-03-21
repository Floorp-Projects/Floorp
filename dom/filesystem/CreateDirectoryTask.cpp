/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CreateDirectoryTask.h"

#include "mozilla/dom/Directory.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/Promise.h"
#include "nsIFile.h"
#include "nsStringGlue.h"

namespace mozilla {
namespace dom {

/* static */ already_AddRefed<CreateDirectoryTask>
CreateDirectoryTask::Create(FileSystemBase* aFileSystem,
                            nsIFile* aTargetPath,
                            ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);

  RefPtr<CreateDirectoryTask> task =
    new CreateDirectoryTask(aFileSystem, aTargetPath);

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

/* static */ already_AddRefed<CreateDirectoryTask>
CreateDirectoryTask::Create(FileSystemBase* aFileSystem,
                            const FileSystemCreateDirectoryParams& aParam,
                            FileSystemRequestParent* aParent,
                            ErrorResult& aRv)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);

  RefPtr<CreateDirectoryTask> task =
    new CreateDirectoryTask(aFileSystem, aParam, aParent);

  NS_ConvertUTF16toUTF8 path(aParam.realPath());
  aRv = NS_NewNativeLocalFile(path, true, getter_AddRefs(task->mTargetPath));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return task.forget();
}

CreateDirectoryTask::CreateDirectoryTask(FileSystemBase* aFileSystem,
                                         nsIFile* aTargetPath)
  : FileSystemTaskBase(aFileSystem)
  , mTargetPath(aTargetPath)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);
}

CreateDirectoryTask::CreateDirectoryTask(FileSystemBase* aFileSystem,
                                         const FileSystemCreateDirectoryParams& aParam,
                                         FileSystemRequestParent* aParent)
  : FileSystemTaskBase(aFileSystem, aParam, aParent)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);
}

CreateDirectoryTask::~CreateDirectoryTask()
{
  MOZ_ASSERT(!mPromise || NS_IsMainThread(),
             "mPromise should be released on main thread!");
}

already_AddRefed<Promise>
CreateDirectoryTask::GetPromise()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return RefPtr<Promise>(mPromise).forget();
}

FileSystemParams
CreateDirectoryTask::GetRequestParams(const nsString& aSerializedDOMPath,
                                      ErrorResult& aRv) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");

  nsAutoString path;
  aRv = mTargetPath->GetPath(path);
  if (NS_WARN_IF(aRv.Failed())) {
    return FileSystemCreateDirectoryParams();
  }

  return FileSystemCreateDirectoryParams(aSerializedDOMPath, path);
}

FileSystemResponseValue
CreateDirectoryTask::GetSuccessRequestResult(ErrorResult& aRv) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");

  nsAutoString path;
  aRv = mTargetPath->GetPath(path);
  if (NS_WARN_IF(aRv.Failed())) {
    return FileSystemDirectoryResponse();
  }

  return FileSystemDirectoryResponse(path);
}

void
CreateDirectoryTask::SetSuccessRequestResult(const FileSystemResponseValue& aValue,
                                             ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  FileSystemDirectoryResponse r = aValue;

  NS_ConvertUTF16toUTF8 path(r.realPath());
  aRv = NS_NewNativeLocalFile(path, true, getter_AddRefs(mTargetPath));
  NS_WARN_IF(aRv.Failed());
}

nsresult
CreateDirectoryTask::Work()
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(!NS_IsMainThread(), "Only call on worker thread!");

  if (mFileSystem->IsShutdown()) {
    return NS_ERROR_FAILURE;
  }

  bool fileExists;
  nsresult rv = mTargetPath->Exists(&fileExists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (fileExists) {
    return NS_ERROR_DOM_FILESYSTEM_PATH_EXISTS_ERR;
  }

  rv = mTargetPath->Create(nsIFile::DIRECTORY_TYPE, 0770);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void
CreateDirectoryTask::HandlerCallback()
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
  RefPtr<Directory> dir = Directory::Create(mFileSystem->GetParentObject(),
                                            mTargetPath,
                                            Directory::eNotDOMRootDirectory,
                                            mFileSystem);
  MOZ_ASSERT(dir);

  mPromise->MaybeResolve(dir);
  mPromise = nullptr;
}

void
CreateDirectoryTask::GetPermissionAccessType(nsCString& aAccess) const
{
  aAccess.AssignLiteral("create");
}

} // namespace dom
} // namespace mozilla
