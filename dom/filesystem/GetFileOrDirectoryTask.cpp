/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GetFileOrDirectoryTask.h"

#include "js/Value.h"
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

/* static */ already_AddRefed<GetFileOrDirectoryTask>
GetFileOrDirectoryTask::Create(FileSystemBase* aFileSystem,
                               nsIFile* aTargetPath,
                               Directory::DirectoryType aType,
                               bool aDirectoryOnly,
                               ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);

  RefPtr<GetFileOrDirectoryTask> task =
    new GetFileOrDirectoryTask(aFileSystem, aTargetPath, aType, aDirectoryOnly);

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

/* static */ already_AddRefed<GetFileOrDirectoryTask>
GetFileOrDirectoryTask::Create(FileSystemBase* aFileSystem,
                               const FileSystemGetFileOrDirectoryParams& aParam,
                               FileSystemRequestParent* aParent,
                               ErrorResult& aRv)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);

  RefPtr<GetFileOrDirectoryTask> task =
    new GetFileOrDirectoryTask(aFileSystem, aParam, aParent);

  NS_ConvertUTF16toUTF8 path(aParam.realPath());
  aRv = NS_NewNativeLocalFile(path, true, getter_AddRefs(task->mTargetPath));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  task->mType = aParam.isRoot()
                  ? Directory::eDOMRootDirectory : Directory::eNotDOMRootDirectory;
  return task.forget();
}

GetFileOrDirectoryTask::GetFileOrDirectoryTask(FileSystemBase* aFileSystem,
                                               nsIFile* aTargetPath,
                                               Directory::DirectoryType aType,
                                               bool aDirectoryOnly)
  : FileSystemTaskBase(aFileSystem)
  , mTargetPath(aTargetPath)
  , mIsDirectory(aDirectoryOnly)
  , mType(aType)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);
}

GetFileOrDirectoryTask::GetFileOrDirectoryTask(FileSystemBase* aFileSystem,
                                               const FileSystemGetFileOrDirectoryParams& aParam,
                                               FileSystemRequestParent* aParent)
  : FileSystemTaskBase(aFileSystem, aParam, aParent)
  , mIsDirectory(false)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);
}

GetFileOrDirectoryTask::~GetFileOrDirectoryTask()
{
  MOZ_ASSERT(!mPromise || NS_IsMainThread(),
             "mPromise should be released on main thread!");
}

already_AddRefed<Promise>
GetFileOrDirectoryTask::GetPromise()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return RefPtr<Promise>(mPromise).forget();
}

FileSystemParams
GetFileOrDirectoryTask::GetRequestParams(const nsString& aSerializedDOMPath,
                                         ErrorResult& aRv) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");

  nsAutoString path;
  aRv = mTargetPath->GetPath(path);
  if (NS_WARN_IF(aRv.Failed())) {
    return FileSystemGetFileOrDirectoryParams();
  }

  return FileSystemGetFileOrDirectoryParams(aSerializedDOMPath, path,
                                            mType == Directory::eDOMRootDirectory);
}

FileSystemResponseValue
GetFileOrDirectoryTask::GetSuccessRequestResult(ErrorResult& aRv) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  if (mIsDirectory) {
    nsAutoString path;
    aRv = mTargetPath->GetPath(path);
    if (NS_WARN_IF(aRv.Failed())) {
      return FileSystemDirectoryResponse();
    }

    return FileSystemDirectoryResponse(path);
  }

  BlobParent* actor = GetBlobParent(mTargetBlobImpl);
  if (!actor) {
    return FileSystemErrorResponse(NS_ERROR_DOM_FILESYSTEM_UNKNOWN_ERR);
  }
  FileSystemFileResponse response;
  response.blobParent() = actor;
  return response;
}

void
GetFileOrDirectoryTask::SetSuccessRequestResult(const FileSystemResponseValue& aValue,
                                                ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  switch (aValue.type()) {
    case FileSystemResponseValue::TFileSystemFileResponse: {
      FileSystemFileResponse r = aValue;
      BlobChild* actor = static_cast<BlobChild*>(r.blobChild());
      mTargetBlobImpl = actor->GetBlobImpl();
      mIsDirectory = false;
      break;
    }
    case FileSystemResponseValue::TFileSystemDirectoryResponse: {
      FileSystemDirectoryResponse r = aValue;

      NS_ConvertUTF16toUTF8 path(r.realPath());
      aRv = NS_NewNativeLocalFile(path, true, getter_AddRefs(mTargetPath));
      if (NS_WARN_IF(aRv.Failed())) {
        return;
      }

      mIsDirectory = true;
      break;
    }
    default: {
      NS_RUNTIMEABORT("not reached");
      break;
    }
  }
}

nsresult
GetFileOrDirectoryTask::Work()
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(!NS_IsMainThread(), "Only call on worker thread!");

  if (mFileSystem->IsShutdown()) {
    return NS_ERROR_FAILURE;
  }

  // Whether we want to get the root directory.
  bool exists;
  nsresult rv = mTargetPath->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    if (mType == Directory::eNotDOMRootDirectory) {
      return NS_ERROR_DOM_FILE_NOT_FOUND_ERR;
    }

    // If the root directory doesn't exit, create it.
    rv = mTargetPath->Create(nsIFile::DIRECTORY_TYPE, 0777);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Get isDirectory.
  rv = mTargetPath->IsDirectory(&mIsDirectory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mIsDirectory) {
    return NS_OK;
  }

  // Check if the root is a directory.
  if (mType == Directory::eDOMRootDirectory) {
    return NS_ERROR_DOM_FILESYSTEM_TYPE_MISMATCH_ERR;
  }

  bool isFile;
  // Get isFile
  rv = mTargetPath->IsFile(&isFile);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!isFile) {
    // Neither directory or file.
    return NS_ERROR_DOM_FILESYSTEM_TYPE_MISMATCH_ERR;
  }

  if (!mFileSystem->IsSafeFile(mTargetPath)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  mTargetBlobImpl = new BlobImplFile(mTargetPath);

  return NS_OK;
}

void
GetFileOrDirectoryTask::HandlerCallback()
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

  if (mIsDirectory) {
    RefPtr<Directory> dir = Directory::Create(mFileSystem->GetParentObject(),
                                              mTargetPath,
                                              mType,
                                              mFileSystem);
    MOZ_ASSERT(dir);

    mPromise->MaybeResolve(dir);
    mPromise = nullptr;
    return;
  }

  RefPtr<Blob> blob = Blob::Create(mFileSystem->GetParentObject(),
                                   mTargetBlobImpl);
  mPromise->MaybeResolve(blob);
  mPromise = nullptr;
}

void
GetFileOrDirectoryTask::GetPermissionAccessType(nsCString& aAccess) const
{
  aAccess.AssignLiteral("read");
}

} // namespace dom
} // namespace mozilla
