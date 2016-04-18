/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CreateDirectoryTask.h"

#include "mozilla/dom/Directory.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/PFileSystemParams.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "nsIFile.h"
#include "nsStringGlue.h"

namespace mozilla {

using namespace ipc;

namespace dom {

/**
 * CreateDirectoryTaskChild
 */

/* static */ already_AddRefed<CreateDirectoryTaskChild>
CreateDirectoryTaskChild::Create(FileSystemBase* aFileSystem,
                                 nsIFile* aTargetPath,
                                 ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);

  RefPtr<CreateDirectoryTaskChild> task =
    new CreateDirectoryTaskChild(aFileSystem, aTargetPath);

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

CreateDirectoryTaskChild::CreateDirectoryTaskChild(FileSystemBase* aFileSystem,
                                                   nsIFile* aTargetPath)
  : FileSystemTaskChildBase(aFileSystem)
  , mTargetPath(aTargetPath)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);
}

CreateDirectoryTaskChild::~CreateDirectoryTaskChild()
{
  MOZ_ASSERT(NS_IsMainThread());
}

already_AddRefed<Promise>
CreateDirectoryTaskChild::GetPromise()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return RefPtr<Promise>(mPromise).forget();
}

FileSystemParams
CreateDirectoryTaskChild::GetRequestParams(const nsString& aSerializedDOMPath,
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

void
CreateDirectoryTaskChild::SetSuccessRequestResult(const FileSystemResponseValue& aValue,
                                                  ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");

  const FileSystemDirectoryResponse& r =
    aValue.get_FileSystemDirectoryResponse();

  aRv = NS_NewNativeLocalFile(NS_ConvertUTF16toUTF8(r.realPath()), true,
                              getter_AddRefs(mTargetPath));
  NS_WARN_IF(aRv.Failed());
}

void
CreateDirectoryTaskChild::HandlerCallback()
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
                                            mTargetPath, mFileSystem);
  MOZ_ASSERT(dir);

  mPromise->MaybeResolve(dir);
  mPromise = nullptr;
}

void
CreateDirectoryTaskChild::GetPermissionAccessType(nsCString& aAccess) const
{
  aAccess.AssignLiteral(DIRECTORY_CREATE_PERMISSION);
}

/**
 * CreateDirectoryTaskParent
 */

/* static */ already_AddRefed<CreateDirectoryTaskParent>
CreateDirectoryTaskParent::Create(FileSystemBase* aFileSystem,
                                  const FileSystemCreateDirectoryParams& aParam,
                                  FileSystemRequestParent* aParent,
                                  ErrorResult& aRv)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aFileSystem);

  RefPtr<CreateDirectoryTaskParent> task =
    new CreateDirectoryTaskParent(aFileSystem, aParam, aParent);

  aRv = NS_NewNativeLocalFile(NS_ConvertUTF16toUTF8(aParam.realPath()), true,
                              getter_AddRefs(task->mTargetPath));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return task.forget();
}

CreateDirectoryTaskParent::CreateDirectoryTaskParent(FileSystemBase* aFileSystem,
                                                     const FileSystemCreateDirectoryParams& aParam,
                                                     FileSystemRequestParent* aParent)
  : FileSystemTaskParentBase(aFileSystem, aParam, aParent)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aFileSystem);
}

FileSystemResponseValue
CreateDirectoryTaskParent::GetSuccessRequestResult(ErrorResult& aRv) const
{
  AssertIsOnBackgroundThread();

  nsAutoString path;
  aRv = mTargetPath->GetPath(path);
  if (NS_WARN_IF(aRv.Failed())) {
    return FileSystemDirectoryResponse();
  }

  return FileSystemDirectoryResponse(path);
}

nsresult
CreateDirectoryTaskParent::IOWork()
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
CreateDirectoryTaskParent::GetPermissionAccessType(nsCString& aAccess) const
{
  aAccess.AssignLiteral(DIRECTORY_CREATE_PERMISSION);
}

} // namespace dom
} // namespace mozilla
