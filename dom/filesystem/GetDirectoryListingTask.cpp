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
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "nsIFile.h"
#include "nsStringGlue.h"

namespace mozilla {
namespace dom {

/* static */ already_AddRefed<GetDirectoryListingTask>
GetDirectoryListingTask::Create(FileSystemBase* aFileSystem,
                                nsIFile* aTargetPath,
                                Directory::DirectoryType aType,
                                const nsAString& aFilters,
                                ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);

  RefPtr<GetDirectoryListingTask> task =
    new GetDirectoryListingTask(aFileSystem, aTargetPath, aType, aFilters);

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

/* static */ already_AddRefed<GetDirectoryListingTask>
GetDirectoryListingTask::Create(FileSystemBase* aFileSystem,
                                const FileSystemGetDirectoryListingParams& aParam,
                                FileSystemRequestParent* aParent,
                                ErrorResult& aRv)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);

  RefPtr<GetDirectoryListingTask> task =
    new GetDirectoryListingTask(aFileSystem, aParam, aParent);

  NS_ConvertUTF16toUTF8 path(aParam.realPath());
  aRv = NS_NewNativeLocalFile(path, true, getter_AddRefs(task->mTargetPath));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  task->mType = aParam.isRoot()
                  ? Directory::eDOMRootDirectory : Directory::eNotDOMRootDirectory;
  return task.forget();
}

GetDirectoryListingTask::GetDirectoryListingTask(FileSystemBase* aFileSystem,
                                                 nsIFile* aTargetPath,
                                                 Directory::DirectoryType aType,
                                                 const nsAString& aFilters)
  : FileSystemTaskBase(aFileSystem)
  , mTargetPath(aTargetPath)
  , mFilters(aFilters)
  , mType(aType)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);
}

GetDirectoryListingTask::GetDirectoryListingTask(FileSystemBase* aFileSystem,
                                                 const FileSystemGetDirectoryListingParams& aParam,
                                                 FileSystemRequestParent* aParent)
  : FileSystemTaskBase(aFileSystem, aParam, aParent)
  , mFilters(aParam.filters())
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);
}

GetDirectoryListingTask::~GetDirectoryListingTask()
{
  MOZ_ASSERT(!mPromise || NS_IsMainThread(),
             "mPromise should be released on main thread!");
}

already_AddRefed<Promise>
GetDirectoryListingTask::GetPromise()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return RefPtr<Promise>(mPromise).forget();
}

FileSystemParams
GetDirectoryListingTask::GetRequestParams(const nsString& aSerializedDOMPath,
                                          ErrorResult& aRv) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");

  nsAutoString path;
  aRv = mTargetPath->GetPath(path);
  if (NS_WARN_IF(aRv.Failed())) {
    return FileSystemGetDirectoryListingParams();
  }

  return FileSystemGetDirectoryListingParams(aSerializedDOMPath, path,
                                             mType == Directory::eDOMRootDirectory,
                                             mFilters);
}

FileSystemResponseValue
GetDirectoryListingTask::GetSuccessRequestResult(ErrorResult& aRv) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");

  InfallibleTArray<PBlobParent*> blobs;

  nsTArray<FileSystemDirectoryListingResponseData> inputs;

  for (unsigned i = 0; i < mTargetData.Length(); i++) {
    if (mTargetData[i].mType == Directory::BlobImplOrDirectoryPath::eBlobImpl) {
      BlobParent* blobParent = GetBlobParent(mTargetData[i].mBlobImpl);
      if (!blobParent) {
        continue;
      }

      FileSystemDirectoryListingResponseBlob blobData;
      blobData.blobParent() = blobParent;
      inputs.AppendElement(blobData);
    } else {
      MOZ_ASSERT(mTargetData[i].mType == Directory::BlobImplOrDirectoryPath::eDirectoryPath);
      FileSystemDirectoryListingResponseDirectory directoryData;
      directoryData.directoryRealPath() = mTargetData[i].mDirectoryPath;
      inputs.AppendElement(directoryData);
    }
  }

  FileSystemDirectoryListingResponse response;
  response.data().SwapElements(inputs);
  return response;
}

void
GetDirectoryListingTask::SetSuccessRequestResult(const FileSystemResponseValue& aValue,
                                                 ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aValue.type() ==
               FileSystemResponseValue::TFileSystemDirectoryListingResponse);

  FileSystemDirectoryListingResponse r = aValue;
  for (uint32_t i = 0; i < r.data().Length(); ++i) {
    const FileSystemDirectoryListingResponseData& data = r.data()[i];

    if (data.type() == FileSystemDirectoryListingResponseData::TFileSystemDirectoryListingResponseBlob) {
      PBlobChild* blob = data.get_FileSystemDirectoryListingResponseBlob().blobChild();

      Directory::BlobImplOrDirectoryPath* element = mTargetData.AppendElement();
      element->mType = Directory::BlobImplOrDirectoryPath::eBlobImpl;
      element->mBlobImpl = static_cast<BlobChild*>(blob)->GetBlobImpl();
    } else {
      MOZ_ASSERT(data.type() == FileSystemDirectoryListingResponseData::TFileSystemDirectoryListingResponseDirectory);

      Directory::BlobImplOrDirectoryPath* element = mTargetData.AppendElement();
      element->mType = Directory::BlobImplOrDirectoryPath::eDirectoryPath;
      element->mDirectoryPath = data.get_FileSystemDirectoryListingResponseDirectory().directoryRealPath();
    }
  }
}

nsresult
GetDirectoryListingTask::Work()
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

    if (isDir) {
      nsAutoString path;
      if (NS_WARN_IF(NS_FAILED(currFile->GetPath(path)))) {
        continue;
      }

      Directory::BlobImplOrDirectoryPath* element = mTargetData.AppendElement();
      element->mType = Directory::BlobImplOrDirectoryPath::eDirectoryPath;
      element->mDirectoryPath = path;
    } else {
      BlobImplFile* impl = new BlobImplFile(currFile);

      Directory::BlobImplOrDirectoryPath* element = mTargetData.AppendElement();
      element->mType = Directory::BlobImplOrDirectoryPath::eBlobImpl;
      element->mBlobImpl = impl;
    }
  }
  return NS_OK;
}

void
GetDirectoryListingTask::HandlerCallback()
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

  size_t count = mTargetData.Length();

  Sequence<OwningFileOrDirectory> listing;

  if (!listing.SetLength(count, mozilla::fallible_t())) {
    mPromise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
    mPromise = nullptr;
    return;
  }

  for (unsigned i = 0; i < count; i++) {
    if (mTargetData[i].mType == Directory::BlobImplOrDirectoryPath::eDirectoryPath) {
      nsCOMPtr<nsIFile> directoryPath;
      NS_ConvertUTF16toUTF8 path(mTargetData[i].mDirectoryPath);
      nsresult rv = NS_NewNativeLocalFile(path, true,
                                          getter_AddRefs(directoryPath));
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

      MOZ_ASSERT(FileSystemUtils::IsDescendantPath(rootPath, directoryPath));
#endif

      RefPtr<Directory> directory =
        Directory::Create(mFileSystem->GetParentObject(),
                          directoryPath,
                          Directory::eNotDOMRootDirectory,
                          mFileSystem);
      MOZ_ASSERT(directory);

      // Propogate mFilter onto sub-Directory object:
      directory->SetContentFilters(mFilters);
      listing[i].SetAsDirectory() = directory;
    } else {
      MOZ_ASSERT(mTargetData[i].mType == Directory::BlobImplOrDirectoryPath::eBlobImpl);
      listing[i].SetAsFile() =
        File::Create(mFileSystem->GetParentObject(), mTargetData[i].mBlobImpl);
    }
  }

  mPromise->MaybeResolve(listing);
  mPromise = nullptr;
}

void
GetDirectoryListingTask::GetPermissionAccessType(nsCString& aAccess) const
{
  aAccess.AssignLiteral("read");
}

} // namespace dom
} // namespace mozilla
