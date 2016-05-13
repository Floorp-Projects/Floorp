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
#include "mozilla/dom/PFileSystemParams.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "nsIFile.h"
#include "nsStringGlue.h"

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
    new GetFilesTaskChild(aFileSystem, aDirectory, aTargetPath,
                          aRecursiveFlag);

  // aTargetPath can be null. In this case SetError will be called.

  task->mPromise = Promise::Create(globalObject, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return task.forget();
}

GetFilesTaskChild::GetFilesTaskChild(FileSystemBase* aFileSystem,
                                     Directory* aDirectory,
                                     nsIFile* aTargetPath,
                                     bool aRecursiveFlag)
  : FileSystemTaskChildBase(aFileSystem)
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
    mTargetData[i].mRealPath = data.realPath();
    mTargetData[i].mDOMPath = data.domPath();
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

  size_t count = mTargetData.Length();

  Sequence<RefPtr<File>> listing;

  if (!listing.SetLength(count, mozilla::fallible_t())) {
    mPromise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
    mPromise = nullptr;
    return;
  }

  for (unsigned i = 0; i < count; i++) {
    nsCOMPtr<nsIFile> path;
    NS_ConvertUTF16toUTF8 fullPath(mTargetData[i].mRealPath);
    nsresult rv = NS_NewNativeLocalFile(fullPath, true, getter_AddRefs(path));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPromise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
      mPromise = nullptr;
      return;
    }

#ifdef DEBUG
    nsCOMPtr<nsIFile> rootPath;
    rv = NS_NewLocalFile(mFileSystem->LocalOrDeviceStorageRootPath(), false,
                         getter_AddRefs(rootPath));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPromise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
      mPromise = nullptr;
      return;
    }

    MOZ_ASSERT(FileSystemUtils::IsDescendantPath(rootPath, path));
#endif

    RefPtr<File> file =
      File::CreateFromFile(mFileSystem->GetParentObject(), path);
    MOZ_ASSERT(file);
    file->SetPath(mTargetData[i].mDOMPath);

    listing[i] = file;
  }

  mPromise->MaybeResolve(listing);
  mPromise = nullptr;
}

void
GetFilesTaskChild::GetPermissionAccessType(nsCString& aAccess) const
{
  aAccess.AssignLiteral("read");
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

  NS_ConvertUTF16toUTF8 path(aParam.realPath());
  aRv = NS_NewNativeLocalFile(path, true, getter_AddRefs(task->mTargetPath));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return task.forget();
}

GetFilesTaskParent::GetFilesTaskParent(FileSystemBase* aFileSystem,
                                       const FileSystemGetFilesParams& aParam,
                                       FileSystemRequestParent* aParent)
  : FileSystemTaskParentBase(aFileSystem, aParam, aParent)
  , mDirectoryDOMPath(aParam.domPath())
  , mRecursiveFlag(aParam.recursiveFlag())
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aFileSystem);
}

FileSystemResponseValue
GetFilesTaskParent::GetSuccessRequestResult(ErrorResult& aRv) const
{
  AssertIsOnBackgroundThread();

  InfallibleTArray<PBlobParent*> blobs;

  FallibleTArray<FileSystemFileResponse> inputs;
  if (!inputs.SetLength(mTargetData.Length(), mozilla::fallible_t())) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    FileSystemFilesResponse response;
    return response;
  }

  for (unsigned i = 0; i < mTargetData.Length(); i++) {
    FileSystemFileResponse fileData;
    fileData.realPath() = mTargetData[i].mRealPath;
    fileData.domPath() = mTargetData[i].mDOMPath;
    inputs[i] = fileData;
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

  // Get isDirectory.
  rv = ExploreDirectory(mDirectoryDOMPath, mTargetPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
GetFilesTaskParent::ExploreDirectory(const nsAString& aDOMPath, nsIFile* aPath)
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(!NS_IsMainThread(), "Only call on worker thread!");
  MOZ_ASSERT(aPath);

  bool isDir;
  nsresult rv = aPath->IsDirectory(&isDir);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!isDir) {
    return NS_ERROR_DOM_FILESYSTEM_TYPE_MISMATCH_ERR;
  }

  nsCOMPtr<nsISimpleEnumerator> entries;
  rv = aPath->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (;;) {
    bool hasMore = false;
    if (NS_WARN_IF(NS_FAILED(entries->HasMoreElements(&hasMore))) || !hasMore) {
      break;
    }
    nsCOMPtr<nsISupports> supp;
    if (NS_WARN_IF(NS_FAILED(entries->GetNext(getter_AddRefs(supp))))) {
      break;
    }

    nsCOMPtr<nsIFile> currFile = do_QueryInterface(supp);
    MOZ_ASSERT(currFile);

    bool isLink, isSpecial, isFile;
    if (NS_WARN_IF(NS_FAILED(currFile->IsSymlink(&isLink)) ||
                   NS_FAILED(currFile->IsSpecial(&isSpecial))) ||
        isLink || isSpecial) {
      continue;
    }

    if (NS_WARN_IF(NS_FAILED(currFile->IsFile(&isFile)) ||
                   NS_FAILED(currFile->IsDirectory(&isDir))) ||
        !(isFile || isDir)) {
      continue;
    }

    nsAutoString domPath;
    domPath.Assign(aDOMPath);

    // This is specific for unix root filesystem.
    if (!aDOMPath.EqualsLiteral(FILESYSTEM_DOM_PATH_SEPARATOR_LITERAL)) {
      domPath.AppendLiteral(FILESYSTEM_DOM_PATH_SEPARATOR_LITERAL);
    }

    nsAutoString leafName;
    if (NS_WARN_IF(NS_FAILED(currFile->GetLeafName(leafName)))) {
      continue;
    }
    domPath.Append(leafName);

    if (isFile) {
      FileData data;
      data.mDOMPath.Append(domPath);

      if (NS_WARN_IF(NS_FAILED(currFile->GetPath(data.mRealPath)))) {
        continue;
      }

      if (!mTargetData.AppendElement(data, fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      continue;
    }

    MOZ_ASSERT(isDir);

    if (!mRecursiveFlag) {
      continue;
    }

    // Recursive.
    rv = ExploreDirectory(domPath, currFile);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

void
GetFilesTaskParent::GetPermissionAccessType(nsCString& aAccess) const
{
  aAccess.AssignLiteral(DIRECTORY_READ_PERMISSION);
}

} // namespace dom
} // namespace mozilla
