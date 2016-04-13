/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GetDirectoryListingTask.h"

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
 * GetDirectoryListingTaskChild
 */

/* static */ already_AddRefed<GetDirectoryListingTaskChild>
GetDirectoryListingTaskChild::Create(FileSystemBase* aFileSystem,
                                     nsIFile* aTargetPath,
                                     Directory::DirectoryType aType,
                                     const nsAString& aFilters,
                                     ErrorResult& aRv)
{
  MOZ_ASSERT(aFileSystem);
  aFileSystem->AssertIsOnOwningThread();

  RefPtr<GetDirectoryListingTaskChild> task =
    new GetDirectoryListingTaskChild(aFileSystem, aTargetPath, aType, aFilters);

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

GetDirectoryListingTaskChild::GetDirectoryListingTaskChild(FileSystemBase* aFileSystem,
                                                           nsIFile* aTargetPath,
                                                           Directory::DirectoryType aType,
                                                           const nsAString& aFilters)
  : FileSystemTaskChildBase(aFileSystem)
  , mTargetPath(aTargetPath)
  , mFilters(aFilters)
  , mType(aType)
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

  nsAutoString path;
  aRv = mTargetPath->GetPath(path);
  if (NS_WARN_IF(aRv.Failed())) {
    return FileSystemGetDirectoryListingParams();
  }

  return FileSystemGetDirectoryListingParams(aSerializedDOMPath, path,
                                             mType == Directory::eDOMRootDirectory,
                                             mFilters);
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

    Directory::FileOrDirectoryPath element;

    if (data.type() == FileSystemDirectoryListingResponseData::TFileSystemDirectoryListingResponseFile) {
      element.mType = Directory::FileOrDirectoryPath::eFilePath;
      element.mPath = data.get_FileSystemDirectoryListingResponseFile().fileRealPath();
    } else {
      MOZ_ASSERT(data.type() == FileSystemDirectoryListingResponseData::TFileSystemDirectoryListingResponseDirectory);

      element.mType = Directory::FileOrDirectoryPath::eDirectoryPath;
      element.mPath = data.get_FileSystemDirectoryListingResponseDirectory().directoryRealPath();
    }

    if (!mTargetData.AppendElement(element, fallible)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
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

  size_t count = mTargetData.Length();

  Sequence<OwningFileOrDirectory> listing;

  if (!listing.SetLength(count, mozilla::fallible_t())) {
    mPromise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
    mPromise = nullptr;
    return;
  }

  for (unsigned i = 0; i < count; i++) {
    nsCOMPtr<nsIFile> path;
    NS_ConvertUTF16toUTF8 fullPath(mTargetData[i].mPath);
    nsresult rv = NS_NewNativeLocalFile(fullPath, true, getter_AddRefs(path));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPromise->MaybeReject(rv);
      mPromise = nullptr;
      return;
    }

#ifdef DEBUG
    nsCOMPtr<nsIFile> rootPath;
    rv = NS_NewLocalFile(mFileSystem->LocalOrDeviceStorageRootPath(), false,
                         getter_AddRefs(rootPath));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPromise->MaybeReject(rv);
      mPromise = nullptr;
      return;
    }

    MOZ_ASSERT(FileSystemUtils::IsDescendantPath(rootPath, path));
#endif

    if (mTargetData[i].mType == Directory::FileOrDirectoryPath::eDirectoryPath) {
      RefPtr<Directory> directory =
        Directory::Create(mFileSystem->GetParentObject(), path,
                          Directory::eNotDOMRootDirectory, mFileSystem);
      MOZ_ASSERT(directory);

      // Propogate mFilter onto sub-Directory object:
      directory->SetContentFilters(mFilters);
      listing[i].SetAsDirectory() = directory;
    } else {
      MOZ_ASSERT(mTargetData[i].mType == Directory::FileOrDirectoryPath::eFilePath);

      RefPtr<File> file =
        File::CreateFromFile(mFileSystem->GetParentObject(), path);
      MOZ_ASSERT(file);

      listing[i].SetAsFile() = file;
    }
  }

  mPromise->MaybeResolve(listing);
  mPromise = nullptr;
}

void
GetDirectoryListingTaskChild::GetPermissionAccessType(nsCString& aAccess) const
{
  aAccess.AssignLiteral(DIRECTORY_READ_PERMISSION);
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

  NS_ConvertUTF16toUTF8 path(aParam.realPath());
  aRv = NS_NewNativeLocalFile(path, true, getter_AddRefs(task->mTargetPath));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  task->mType = aParam.isRoot()
                  ? Directory::eDOMRootDirectory : Directory::eNotDOMRootDirectory;
  return task.forget();
}

GetDirectoryListingTaskParent::GetDirectoryListingTaskParent(FileSystemBase* aFileSystem,
                                                             const FileSystemGetDirectoryListingParams& aParam,
                                                             FileSystemRequestParent* aParent)
  : FileSystemTaskParentBase(aFileSystem, aParam, aParent)
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

  InfallibleTArray<PBlobParent*> blobs;

  nsTArray<FileSystemDirectoryListingResponseData> inputs;

  for (unsigned i = 0; i < mTargetData.Length(); i++) {
    if (mTargetData[i].mType == Directory::FileOrDirectoryPath::eFilePath) {
      FileSystemDirectoryListingResponseFile fileData;
      fileData.fileRealPath() = mTargetData[i].mPath;
      inputs.AppendElement(fileData);
    } else {
      MOZ_ASSERT(mTargetData[i].mType == Directory::FileOrDirectoryPath::eDirectoryPath);
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
  bool isDir;
  rv = mTargetPath->IsDirectory(&isDir);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!isDir) {
    return NS_ERROR_DOM_FILESYSTEM_TYPE_MISMATCH_ERR;
  }

  nsCOMPtr<nsISimpleEnumerator> entries;
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

    Directory::FileOrDirectoryPath element;
    element.mPath = path;
    element.mType = isDir ? Directory::FileOrDirectoryPath::eDirectoryPath
                          : Directory::FileOrDirectoryPath::eFilePath;

    if (!mTargetData.AppendElement(element, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return NS_OK;
}

void
GetDirectoryListingTaskParent::GetPermissionAccessType(nsCString& aAccess) const
{
  aAccess.AssignLiteral(DIRECTORY_READ_PERMISSION);
}

} // namespace dom
} // namespace mozilla
