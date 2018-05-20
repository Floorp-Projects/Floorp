/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GetDirectoryListingTask.h"

#include "HTMLSplitOnSpacesTokenizer.h"
#include "js/Value.h"
#include "mozilla/dom/FileBlobImpl.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/PFileSystemParams.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/UnionTypes.h"
#include "nsIFile.h"
#include "nsISimpleEnumerator.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

/**
 * GetDirectoryListingTaskChild
 */

/* static */ already_AddRefed<GetDirectoryListingTaskChild>
GetDirectoryListingTaskChild::Create(FileSystemBase* aFileSystem,
                                     Directory* aDirectory,
                                     nsIFile* aTargetPath,
                                     const nsAString& aFilters,
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

  RefPtr<GetDirectoryListingTaskChild> task =
    new GetDirectoryListingTaskChild(globalObject, aFileSystem, aDirectory,
                                     aTargetPath, aFilters);

  // aTargetPath can be null. In this case SetError will be called.

  task->mPromise = Promise::Create(globalObject, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return task.forget();
}

GetDirectoryListingTaskChild::GetDirectoryListingTaskChild(nsIGlobalObject* aGlobalObject,
                                                           FileSystemBase* aFileSystem,
                                                           Directory* aDirectory,
                                                           nsIFile* aTargetPath,
                                                           const nsAString& aFilters)
  : FileSystemTaskChildBase(aGlobalObject, aFileSystem)
  , mDirectory(aDirectory)
  , mTargetPath(aTargetPath)
  , mFilters(aFilters)
{
  MOZ_ASSERT(aFileSystem);
  aFileSystem->AssertIsOnOwningThread();
}

GetDirectoryListingTaskChild::~GetDirectoryListingTaskChild()
{
  mFileSystem->AssertIsOnOwningThread();
}

already_AddRefed<Promise>
GetDirectoryListingTaskChild::GetPromise()
{
  mFileSystem->AssertIsOnOwningThread();
  return RefPtr<Promise>(mPromise).forget();
}

FileSystemParams
GetDirectoryListingTaskChild::GetRequestParams(const nsString& aSerializedDOMPath,
                                               ErrorResult& aRv) const
{
  mFileSystem->AssertIsOnOwningThread();

  // this is the real path.
  nsAutoString path;
  aRv = mTargetPath->GetPath(path);
  if (NS_WARN_IF(aRv.Failed())) {
    return FileSystemGetDirectoryListingParams();
  }

  // this is the dom path.
  nsAutoString directoryPath;
  mDirectory->GetPath(directoryPath, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return FileSystemGetDirectoryListingParams();
  }

  return FileSystemGetDirectoryListingParams(aSerializedDOMPath, path,
                                             directoryPath, mFilters);
}

void
GetDirectoryListingTaskChild::SetSuccessRequestResult(const FileSystemResponseValue& aValue,
                                                      ErrorResult& aRv)
{
  mFileSystem->AssertIsOnOwningThread();
  MOZ_ASSERT(aValue.type() ==
               FileSystemResponseValue::TFileSystemDirectoryListingResponse);

  FileSystemDirectoryListingResponse r = aValue;
  for (uint32_t i = 0; i < r.data().Length(); ++i) {
    const FileSystemDirectoryListingResponseData& data = r.data()[i];

    OwningFileOrDirectory* ofd = mTargetData.AppendElement(fallible);
    if (!ofd) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }

    if (data.type() == FileSystemDirectoryListingResponseData::TFileSystemDirectoryListingResponseFile) {
      const FileSystemDirectoryListingResponseFile& d =
        data.get_FileSystemDirectoryListingResponseFile();

      RefPtr<BlobImpl> blobImpl = IPCBlobUtils::Deserialize(d.blob());
      MOZ_ASSERT(blobImpl);

      RefPtr<File> file = File::Create(mFileSystem->GetParentObject(), blobImpl);
      MOZ_ASSERT(file);

      ofd->SetAsFile() = file;
    } else {
      MOZ_ASSERT(data.type() == FileSystemDirectoryListingResponseData::TFileSystemDirectoryListingResponseDirectory);
      const FileSystemDirectoryListingResponseDirectory& d =
        data.get_FileSystemDirectoryListingResponseDirectory();

      nsCOMPtr<nsIFile> path;
      aRv = NS_NewLocalFile(d.directoryRealPath(), true, getter_AddRefs(path));
      if (NS_WARN_IF(aRv.Failed())) {
        return;
      }

      RefPtr<Directory> directory =
        Directory::Create(mFileSystem->GetParentObject(), path, mFileSystem);
      MOZ_ASSERT(directory);

      ofd->SetAsDirectory() = directory;
    }
  }
}

void
GetDirectoryListingTaskChild::HandlerCallback()
{
  mFileSystem->AssertIsOnOwningThread();

  if (mFileSystem->IsShutdown()) {
    mPromise = nullptr;
    return;
  }

  if (HasError()) {
    mPromise->MaybeReject(mErrorValue);
    mPromise = nullptr;
    return;
  }

  mPromise->MaybeResolve(mTargetData);
  mPromise = nullptr;
}

/**
 * GetDirectoryListingTaskParent
 */

/* static */ already_AddRefed<GetDirectoryListingTaskParent>
GetDirectoryListingTaskParent::Create(FileSystemBase* aFileSystem,
                                      const FileSystemGetDirectoryListingParams& aParam,
                                      FileSystemRequestParent* aParent,
                                      ErrorResult& aRv)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aFileSystem);

  RefPtr<GetDirectoryListingTaskParent> task =
    new GetDirectoryListingTaskParent(aFileSystem, aParam, aParent);

  aRv = NS_NewLocalFile(aParam.realPath(), true,
                        getter_AddRefs(task->mTargetPath));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return task.forget();
}

GetDirectoryListingTaskParent::GetDirectoryListingTaskParent(FileSystemBase* aFileSystem,
                                                             const FileSystemGetDirectoryListingParams& aParam,
                                                             FileSystemRequestParent* aParent)
  : FileSystemTaskParentBase(aFileSystem, aParam, aParent)
  , mDOMPath(aParam.domPath())
  , mFilters(aParam.filters())
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aFileSystem);
}

FileSystemResponseValue
GetDirectoryListingTaskParent::GetSuccessRequestResult(ErrorResult& aRv) const
{
  AssertIsOnBackgroundThread();

  nsTArray<FileSystemDirectoryListingResponseData> inputs;

  for (unsigned i = 0; i < mTargetData.Length(); i++) {
    if (mTargetData[i].mType == FileOrDirectoryPath::eFilePath) {
      nsCOMPtr<nsIFile> path;
      nsresult rv = NS_NewLocalFile(mTargetData[i].mPath, true,
                                    getter_AddRefs(path));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }

      FileSystemDirectoryListingResponseFile fileData;
      RefPtr<BlobImpl> blobImpl = new FileBlobImpl(path);

      nsAutoString filePath;
      filePath.Assign(mDOMPath);

      // This is specific for unix root filesystem.
      if (!mDOMPath.EqualsLiteral(FILESYSTEM_DOM_PATH_SEPARATOR_LITERAL)) {
        filePath.AppendLiteral(FILESYSTEM_DOM_PATH_SEPARATOR_LITERAL);
      }

      nsAutoString name;
      blobImpl->GetName(name);
      filePath.Append(name);
      blobImpl->SetDOMPath(filePath);

      IPCBlob ipcBlob;
      rv =
        IPCBlobUtils::Serialize(blobImpl, mRequestParent->Manager(), ipcBlob);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }

      fileData.blob() = ipcBlob;
      inputs.AppendElement(fileData);
    } else {
      MOZ_ASSERT(mTargetData[i].mType == FileOrDirectoryPath::eDirectoryPath);
      FileSystemDirectoryListingResponseDirectory directoryData;
      directoryData.directoryRealPath() = mTargetData[i].mPath;
      inputs.AppendElement(directoryData);
    }
  }

  FileSystemDirectoryListingResponse response;
  response.data().SwapElements(inputs);
  return response;
}

nsresult
GetDirectoryListingTaskParent::IOWork()
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(!NS_IsMainThread(), "Only call on worker thread!");

  if (mFileSystem->IsShutdown()) {
    return NS_ERROR_FAILURE;
  }

  bool exists;
  nsresult rv = mTargetPath->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    if (!mFileSystem->ShouldCreateDirectory()) {
      return NS_ERROR_DOM_FILE_NOT_FOUND_ERR;
    }

    rv = mTargetPath->Create(nsIFile::DIRECTORY_TYPE, 0777);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Get isDirectory.
  bool isDir;
  rv = mTargetPath->IsDirectory(&isDir);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!isDir) {
    return NS_ERROR_DOM_FILESYSTEM_TYPE_MISMATCH_ERR;
  }

  nsCOMPtr<nsIDirectoryEnumerator> entries;
  rv = mTargetPath->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool filterOutSensitive = false;
  {
    HTMLSplitOnSpacesTokenizer tokenizer(mFilters, ';');
    nsAutoString token;
    while (tokenizer.hasMoreTokens()) {
      token = tokenizer.nextToken();
      if (token.EqualsLiteral("filter-out-sensitive")) {
        filterOutSensitive = true;
      } else {
        MOZ_CRASH("Unrecognized filter");
      }
    }
  }

  for (;;) {
    nsCOMPtr<nsIFile> currFile;
    if (NS_WARN_IF(NS_FAILED(entries->GetNextFile(getter_AddRefs(currFile)))) || !currFile) {
      break;
    }
    bool isSpecial, isFile;
    if (NS_WARN_IF(NS_FAILED(currFile->IsSpecial(&isSpecial))) ||
        isSpecial) {
      continue;
    }
    if (NS_WARN_IF(NS_FAILED(currFile->IsFile(&isFile)) ||
                   NS_FAILED(currFile->IsDirectory(&isDir))) ||
        !(isFile || isDir)) {
      continue;
    }

    if (filterOutSensitive) {
      bool isHidden;
      if (NS_WARN_IF(NS_FAILED(currFile->IsHidden(&isHidden))) || isHidden) {
        continue;
      }
      nsAutoString leafName;
      if (NS_WARN_IF(NS_FAILED(currFile->GetLeafName(leafName)))) {
        continue;
      }
      if (leafName[0] == char16_t('.')) {
        continue;
      }
    }

    nsAutoString path;
    if (NS_WARN_IF(NS_FAILED(currFile->GetPath(path)))) {
      continue;
    }

    FileOrDirectoryPath element;
    element.mPath = path;
    element.mType = isDir ? FileOrDirectoryPath::eDirectoryPath
                          : FileOrDirectoryPath::eFilePath;

    if (!mTargetData.AppendElement(element, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return NS_OK;
}

nsresult
GetDirectoryListingTaskParent::GetTargetPath(nsAString& aPath) const
{
  return mTargetPath->GetPath(aPath);
}

} // namespace dom
} // namespace mozilla
