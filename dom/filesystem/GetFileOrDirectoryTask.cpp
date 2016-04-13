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
#include "mozilla/dom/PFileSystemParams.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "nsIFile.h"
#include "nsStringGlue.h"

namespace mozilla {
namespace dom {

/**
 * GetFileOrDirectoryTaskChild
 */

/* static */ already_AddRefed<GetFileOrDirectoryTaskChild>
GetFileOrDirectoryTaskChild::Create(FileSystemBase* aFileSystem,
                                    nsIFile* aTargetPath,
                                    Directory::DirectoryType aType,
                                    bool aDirectoryOnly,
                                    ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);

  RefPtr<GetFileOrDirectoryTaskChild> task =
    new GetFileOrDirectoryTaskChild(aFileSystem, aTargetPath, aType,
                                    aDirectoryOnly);

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

GetFileOrDirectoryTaskChild::GetFileOrDirectoryTaskChild(FileSystemBase* aFileSystem,
                                                         nsIFile* aTargetPath,
                                                         Directory::DirectoryType aType,
                                                         bool aDirectoryOnly)
  : FileSystemTaskChildBase(aFileSystem)
  , mTargetPath(aTargetPath)
  , mIsDirectory(aDirectoryOnly)
  , mType(aType)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);
}

GetFileOrDirectoryTaskChild::~GetFileOrDirectoryTaskChild()
{
  MOZ_ASSERT(NS_IsMainThread());
}

already_AddRefed<Promise>
GetFileOrDirectoryTaskChild::GetPromise()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return RefPtr<Promise>(mPromise).forget();
}

FileSystemParams
GetFileOrDirectoryTaskChild::GetRequestParams(const nsString& aSerializedDOMPath,
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

void
GetFileOrDirectoryTaskChild::SetSuccessRequestResult(const FileSystemResponseValue& aValue,
                                                     ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  switch (aValue.type()) {
    case FileSystemResponseValue::TFileSystemFileResponse: {
      FileSystemFileResponse r = aValue;

      NS_ConvertUTF16toUTF8 path(r.realPath());
      aRv = NS_NewNativeLocalFile(path, true, getter_AddRefs(mTargetPath));
      if (NS_WARN_IF(aRv.Failed())) {
        return;
      }

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

void
GetFileOrDirectoryTaskChild::HandlerCallback()
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

  RefPtr<File> file = File::CreateFromFile(mFileSystem->GetParentObject(),
                                           mTargetPath);
  mPromise->MaybeResolve(file);
  mPromise = nullptr;
}

void
GetFileOrDirectoryTaskChild::GetPermissionAccessType(nsCString& aAccess) const
{
  aAccess.AssignLiteral(DIRECTORY_READ_PERMISSION);
}

/**
 * GetFileOrDirectoryTaskParent
 */

/* static */ already_AddRefed<GetFileOrDirectoryTaskParent>
GetFileOrDirectoryTaskParent::Create(FileSystemBase* aFileSystem,
                                     const FileSystemGetFileOrDirectoryParams& aParam,
                                     FileSystemRequestParent* aParent,
                                     ErrorResult& aRv)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aFileSystem);

  RefPtr<GetFileOrDirectoryTaskParent> task =
    new GetFileOrDirectoryTaskParent(aFileSystem, aParam, aParent);

  NS_ConvertUTF16toUTF8 path(aParam.realPath());
  aRv = NS_NewNativeLocalFile(path, true, getter_AddRefs(task->mTargetPath));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  task->mType = aParam.isRoot()
                  ? Directory::eDOMRootDirectory : Directory::eNotDOMRootDirectory;
  return task.forget();
}

GetFileOrDirectoryTaskParent::GetFileOrDirectoryTaskParent(FileSystemBase* aFileSystem,
                                                           const FileSystemGetFileOrDirectoryParams& aParam,
                                                           FileSystemRequestParent* aParent)
  : FileSystemTaskParentBase(aFileSystem, aParam, aParent)
  , mIsDirectory(false)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aFileSystem);
}

FileSystemResponseValue
GetFileOrDirectoryTaskParent::GetSuccessRequestResult(ErrorResult& aRv) const
{
  AssertIsOnBackgroundThread();

  nsAutoString path;
  aRv = mTargetPath->GetPath(path);
  if (NS_WARN_IF(aRv.Failed())) {
    return FileSystemDirectoryResponse();
  }

  if (mIsDirectory) {
    return FileSystemDirectoryResponse(path);
  }

  return FileSystemFileResponse(path);
}

nsresult
GetFileOrDirectoryTaskParent::IOWork()
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

  return NS_OK;
}

void
GetFileOrDirectoryTaskParent::GetPermissionAccessType(nsCString& aAccess) const
{
  aAccess.AssignLiteral(DIRECTORY_READ_PERMISSION);
}

} // namespace dom
} // namespace mozilla
