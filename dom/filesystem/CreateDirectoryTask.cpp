/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CreateDirectoryTask.h"

#include "DOMError.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/Promise.h"
#include "nsIFile.h"
#include "nsStringGlue.h"

namespace mozilla {
namespace dom {

CreateDirectoryTask::CreateDirectoryTask(FileSystemBase* aFileSystem,
                                         const nsAString& aPath,
                                         ErrorResult& aRv)
  : FileSystemTaskBase(aFileSystem)
  , mTargetRealPath(aPath)
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

CreateDirectoryTask::CreateDirectoryTask(
  FileSystemBase* aFileSystem,
  const FileSystemCreateDirectoryParams& aParam,
  FileSystemRequestParent* aParent)
  : FileSystemTaskBase(aFileSystem, aParam, aParent)
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);
  mTargetRealPath = aParam.realPath();
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
  return nsRefPtr<Promise>(mPromise).forget();
}

FileSystemParams
CreateDirectoryTask::GetRequestParams(const nsString& aFileSystem) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return FileSystemCreateDirectoryParams(aFileSystem, mTargetRealPath);
}

FileSystemResponseValue
CreateDirectoryTask::GetSuccessRequestResult() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return FileSystemDirectoryResponse(mTargetRealPath);
}

void
CreateDirectoryTask::SetSuccessRequestResult(const FileSystemResponseValue& aValue)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  FileSystemDirectoryResponse r = aValue;
  mTargetRealPath = r.realPath();
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

  nsCOMPtr<nsIFile> file = mFileSystem->GetLocalFile(mTargetRealPath);
  if (!file) {
    return NS_ERROR_DOM_FILESYSTEM_INVALID_PATH_ERR;
  }

  bool fileExists;
  nsresult rv = file->Exists(&fileExists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (fileExists) {
    return NS_ERROR_DOM_FILESYSTEM_PATH_EXISTS_ERR;
  }

  rv = file->Create(nsIFile::DIRECTORY_TYPE, 0770);
  return rv;
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
    nsRefPtr<DOMError> domError = new DOMError(mFileSystem->GetWindow(),
      mErrorValue);
    mPromise->MaybeRejectBrokenly(domError);
    mPromise = nullptr;
    return;
  }
  nsRefPtr<Directory> dir = new Directory(mFileSystem, mTargetRealPath);
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
