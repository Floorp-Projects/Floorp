/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GetFilesTask.h"

#include "HTMLSplitOnSpacesTokenizer.h"
#include "js/Value.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/PFileSystemParams.h"
#include "mozilla/dom/Promise.h"
#include "nsIFile.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

/**
 * GetFilesTaskChild
 */

/* static */ already_AddRefed<GetFilesTaskChild>
GetFilesTaskChild::Create(FileSystemBase* aFileSystem,
                          Directory* aDirectory,
                          nsIFile* aTargetPath,
                          bool aRecursiveFlag,
                          ErrorResult& aRv)
{
  MOZ_ASSERT(aFileSystem);
  MOZ_ASSERT(aDirectory);
  aFileSystem->AssertIsOnOwningThread();

  nsCOMPtr<nsIGlobalObject> globalObject =
    do_QueryInterface(aFileSystem->GetParentObject());
  if (NS_WARN_IF(!globalObject)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<GetFilesTaskChild> task =
    new GetFilesTaskChild(globalObject, aFileSystem, aDirectory, aTargetPath,
                          aRecursiveFlag);

  // aTargetPath can be null. In this case SetError will be called.

  task->mPromise = Promise::Create(globalObject, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return task.forget();
}

GetFilesTaskChild::GetFilesTaskChild(nsIGlobalObject *aGlobalObject,
                                     FileSystemBase* aFileSystem,
                                     Directory* aDirectory,
                                     nsIFile* aTargetPath,
                                     bool aRecursiveFlag)
  : FileSystemTaskChildBase(aGlobalObject, aFileSystem)
  , mDirectory(aDirectory)
  , mTargetPath(aTargetPath)
  , mRecursiveFlag(aRecursiveFlag)
{
  MOZ_ASSERT(aFileSystem);
  MOZ_ASSERT(aDirectory);
  aFileSystem->AssertIsOnOwningThread();
}

GetFilesTaskChild::~GetFilesTaskChild()
{
  mFileSystem->AssertIsOnOwningThread();
}

already_AddRefed<Promise>
GetFilesTaskChild::GetPromise()
{
  mFileSystem->AssertIsOnOwningThread();
  return RefPtr<Promise>(mPromise).forget();
}

FileSystemParams
GetFilesTaskChild::GetRequestParams(const nsString& aSerializedDOMPath,
                                    ErrorResult& aRv) const
{
  mFileSystem->AssertIsOnOwningThread();

  nsAutoString path;
  aRv = mTargetPath->GetPath(path);
  if (NS_WARN_IF(aRv.Failed())) {
    return FileSystemGetFilesParams();
  }

  nsAutoString domPath;
  mDirectory->GetPath(domPath, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return FileSystemGetFilesParams();
  }

  return FileSystemGetFilesParams(aSerializedDOMPath, path, domPath,
                                  mRecursiveFlag);
}

void
GetFilesTaskChild::SetSuccessRequestResult(const FileSystemResponseValue& aValue,
                                           ErrorResult& aRv)
{
  mFileSystem->AssertIsOnOwningThread();
  MOZ_ASSERT(aValue.type() ==
               FileSystemResponseValue::TFileSystemFilesResponse);

  FileSystemFilesResponse r = aValue;

  if (!mTargetData.SetLength(r.data().Length(), mozilla::fallible_t())) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  for (uint32_t i = 0; i < r.data().Length(); ++i) {
    const FileSystemFileResponse& data = r.data()[i];
    RefPtr<BlobImpl> blobImpl = IPCBlobUtils::Deserialize(data.blob());
    MOZ_ASSERT(blobImpl);

    mTargetData[i] = File::Create(mFileSystem->GetParentObject(), blobImpl);
  }
}

void
GetFilesTaskChild::HandlerCallback()
{
  mFileSystem->AssertIsOnOwningThread();
  if (mFileSystem->IsShutdown()) {
    mPromise = nullptr;
    return;
  }

  if (HasError()) {
    mPromise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    mPromise = nullptr;
    return;
  }

  mPromise->MaybeResolve(mTargetData);
  mPromise = nullptr;
}

/**
 * GetFilesTaskParent
 */

/* static */ already_AddRefed<GetFilesTaskParent>
GetFilesTaskParent::Create(FileSystemBase* aFileSystem,
                           const FileSystemGetFilesParams& aParam,
                           FileSystemRequestParent* aParent,
                           ErrorResult& aRv)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aFileSystem);

  RefPtr<GetFilesTaskParent> task =
    new GetFilesTaskParent(aFileSystem, aParam, aParent);

  aRv = NS_NewLocalFile(aParam.realPath(), true,
                        getter_AddRefs(task->mTargetPath));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return task.forget();
}

GetFilesTaskParent::GetFilesTaskParent(FileSystemBase* aFileSystem,
                                       const FileSystemGetFilesParams& aParam,
                                       FileSystemRequestParent* aParent)
  : FileSystemTaskParentBase(aFileSystem, aParam, aParent)
  , GetFilesHelperBase(aParam.recursiveFlag())
  , mDirectoryDOMPath(aParam.domPath())
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aFileSystem);
}

FileSystemResponseValue
GetFilesTaskParent::GetSuccessRequestResult(ErrorResult& aRv) const
{
  AssertIsOnBackgroundThread();

  FallibleTArray<FileSystemFileResponse> inputs;
  if (!inputs.SetLength(mTargetBlobImplArray.Length(), mozilla::fallible_t())) {
    FileSystemFilesResponse response;
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return response;
  }

  for (unsigned i = 0; i < mTargetBlobImplArray.Length(); i++) {
    IPCBlob ipcBlob;
    aRv = IPCBlobUtils::Serialize(mTargetBlobImplArray[i],
                                  mRequestParent->Manager(), ipcBlob);
    if (NS_WARN_IF(aRv.Failed())) {
      FileSystemFilesResponse response;
      return response;
    }

    inputs[i] = FileSystemFileResponse(ipcBlob);
  }

  FileSystemFilesResponse response;
  response.data().SwapElements(inputs);
  return response;
}

nsresult
GetFilesTaskParent::IOWork()
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(!NS_IsMainThread(), "Only call on I/O thread!");

  if (mFileSystem->IsShutdown()) {
    return NS_ERROR_FAILURE;
  }

  bool exists;
  nsresult rv = mTargetPath->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    return NS_OK;
  }

  bool isDir;
  rv = mTargetPath->IsDirectory(&isDir);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!isDir) {
    return NS_ERROR_DOM_FILESYSTEM_TYPE_MISMATCH_ERR;
  }

  // Get isDirectory.
  rv = ExploreDirectory(mDirectoryDOMPath, mTargetPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
GetFilesTaskParent::GetTargetPath(nsAString& aPath) const
{
  return mTargetPath->GetPath(aPath);
}

} // namespace dom
} // namespace mozilla
