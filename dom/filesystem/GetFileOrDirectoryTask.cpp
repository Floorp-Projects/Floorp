/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GetFileOrDirectoryTask.h"

#include "js/Value.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/Promise.h"
#include "nsDOMFile.h"
#include "nsIFile.h"
#include "nsStringGlue.h"

namespace mozilla {
namespace dom {

GetFileOrDirectoryTask::GetFileOrDirectoryTask(
  FileSystemBase* aFileSystem,
  const nsAString& aTargetPath,
  bool aDirectoryOnly)
  : FileSystemTaskBase(aFileSystem)
  , mTargetRealPath(aTargetPath)
  , mIsDirectory(aDirectoryOnly)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  if (!aFileSystem) {
    return;
  }
  nsCOMPtr<nsIGlobalObject> globalObject =
    do_QueryInterface(aFileSystem->GetWindow());
  if (!globalObject) {
    return;
  }
  mPromise = new Promise(globalObject);
}

GetFileOrDirectoryTask::GetFileOrDirectoryTask(
  FileSystemBase* aFileSystem,
  const FileSystemGetFileOrDirectoryParams& aParam,
  FileSystemRequestParent* aParent)
  : FileSystemTaskBase(aFileSystem, aParam, aParent)
  , mIsDirectory(false)
{
  MOZ_ASSERT(FileSystemUtils::IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  mTargetRealPath = aParam.realPath();
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
  return nsRefPtr<Promise>(mPromise).forget();
}

FileSystemParams
GetFileOrDirectoryTask::GetRequestParams(const nsString& aFileSystem) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return FileSystemGetFileOrDirectoryParams(aFileSystem, mTargetRealPath);
}

FileSystemResponseValue
GetFileOrDirectoryTask::GetSuccessRequestResult() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  if (mIsDirectory) {
    return FileSystemDirectoryResponse(mTargetRealPath);
  }

  ContentParent* cp = static_cast<ContentParent*>(mRequestParent->Manager());
  BlobParent* actor = cp->GetOrCreateActorForBlob(mTargetFile);
  if (!actor) {
    return FileSystemErrorResponse(NS_ERROR_DOM_FILESYSTEM_UNKNOWN_ERR);
  }
  FileSystemFileResponse response;
  response.blobParent() = actor;
  return response;
}

void
GetFileOrDirectoryTask::SetSuccessRequestResult(const FileSystemResponseValue& aValue)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  switch (aValue.type()) {
    case FileSystemResponseValue::TFileSystemFileResponse: {
      FileSystemFileResponse r = aValue;
      BlobChild* actor = static_cast<BlobChild*>(r.blobChild());
      nsCOMPtr<nsIDOMBlob> blob = actor->GetBlob();
      mTargetFile = do_QueryInterface(blob);
      mIsDirectory = false;
      break;
    }
    case FileSystemResponseValue::TFileSystemDirectoryResponse: {
      FileSystemDirectoryResponse r = aValue;
      mTargetRealPath = r.realPath();
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
GetFileOrDirectoryTask::Work()
{
  MOZ_ASSERT(FileSystemUtils::IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(!NS_IsMainThread(), "Only call on worker thread!");

  nsRefPtr<FileSystemBase> filesystem = do_QueryReferent(mFileSystem);
  if (!filesystem) {
    return;
  }

  // Whether we want to get the root directory.
  bool getRoot = mTargetRealPath.IsEmpty();

  nsCOMPtr<nsIFile> file = filesystem->GetLocalFile(mTargetRealPath);
  if (!file) {
    SetError(NS_ERROR_DOM_FILESYSTEM_INVALID_PATH_ERR);
    return;
  }

  bool ret;
  nsresult rv = file->Exists(&ret);
  if (NS_FAILED(rv)) {
    SetError(rv);
    return;
  }

  if (!ret) {
    if (!getRoot) {
      SetError(NS_ERROR_DOM_FILE_NOT_FOUND_ERR);
      return;
    }

    // If the root directory doesn't exit, create it.
    rv = file->Create(nsIFile::DIRECTORY_TYPE, 0777);
    if (NS_FAILED(rv)) {
      SetError(rv);
      return;
    }
  }

  // Get isDirectory.
  rv = file->IsDirectory(&mIsDirectory);
  if (NS_FAILED(rv)) {
    SetError(rv);
    return;
  }

  if (!mIsDirectory) {
    // Check if the root is a directory.
    if (getRoot) {
      SetError(NS_ERROR_DOM_FILESYSTEM_TYPE_MISMATCH_ERR);
      return;
    }

    // Get isFile
    rv = file->IsFile(&ret);
    if (NS_FAILED(rv)) {
      SetError(rv);
      return;
    }
    if (!ret) {
      // Neither directory or file.
      SetError(NS_ERROR_DOM_FILESYSTEM_TYPE_MISMATCH_ERR);
      return;
    }

    if (!filesystem->IsSafeFile(file)) {
      SetError(NS_ERROR_DOM_SECURITY_ERR);
      return;
    }

    mTargetFile = new nsDOMFileFile(file);
  }
}

void
GetFileOrDirectoryTask::HandlerCallback()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  nsRefPtr<FileSystemBase> filesystem = do_QueryReferent(mFileSystem);
  if (!filesystem) {
    mPromise = nullptr;
    return;
  }

  if (HasError()) {
    nsRefPtr<DOMError> domError = new DOMError(filesystem->GetWindow(),
      mErrorValue);
    mPromise->MaybeReject(domError);
    mPromise = nullptr;
    return;
  }

  if (mIsDirectory) {
    nsRefPtr<Directory> dir = new Directory(filesystem, mTargetRealPath);
    mPromise->MaybeResolve(dir);
    mPromise = nullptr;
    return;
  }

  mPromise->MaybeResolve(mTargetFile);
  mPromise = nullptr;
}

void
GetFileOrDirectoryTask::GetPermissionAccessType(nsCString& aAccess) const
{
  aAccess.AssignLiteral("read");
}

} // namespace dom
} // namespace mozilla
