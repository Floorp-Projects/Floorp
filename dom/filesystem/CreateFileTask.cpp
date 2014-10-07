/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CreateFileTask.h"

#include <algorithm>

#include "DOMError.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "nsDOMFile.h"
#include "nsIFile.h"
#include "nsNetUtil.h"
#include "nsStringGlue.h"

namespace mozilla {
namespace dom {

uint32_t CreateFileTask::sOutputBufferSize = 0;

CreateFileTask::CreateFileTask(FileSystemBase* aFileSystem,
                               const nsAString& aPath,
                               nsIDOMBlob* aBlobData,
                               InfallibleTArray<uint8_t>& aArrayData,
                               bool replace,
                               ErrorResult& aRv)
  : FileSystemTaskBase(aFileSystem)
  , mTargetRealPath(aPath)
  , mReplace(replace)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);
  GetOutputBufferSize();
  if (aBlobData) {
    if (FileSystemUtils::IsParentProcess()) {
      nsresult rv = aBlobData->GetInternalStream(getter_AddRefs(mBlobStream));
      NS_WARN_IF(NS_FAILED(rv));
    } else {
      mBlobData = aBlobData;
    }
  }
  mArrayData.SwapElements(aArrayData);
  nsCOMPtr<nsIGlobalObject> globalObject =
    do_QueryInterface(aFileSystem->GetWindow());
  if (!globalObject) {
    return;
  }
  mPromise = Promise::Create(globalObject, aRv);
}

CreateFileTask::CreateFileTask(FileSystemBase* aFileSystem,
                       const FileSystemCreateFileParams& aParam,
                       FileSystemRequestParent* aParent)
  : FileSystemTaskBase(aFileSystem, aParam, aParent)
  , mReplace(false)
{
  MOZ_ASSERT(FileSystemUtils::IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);
  GetOutputBufferSize();

  mTargetRealPath = aParam.realPath();

  mReplace = aParam.replace();

  auto& data = aParam.data();

  if (data.type() == FileSystemFileDataValue::TArrayOfuint8_t) {
    mArrayData = data;
    return;
  }

  BlobParent* bp = static_cast<BlobParent*>(static_cast<PBlobParent*>(data));
  nsCOMPtr<nsIDOMBlob> blobData = bp->GetBlob();
  MOZ_ASSERT(blobData, "blobData should not be null.");
  nsresult rv = blobData->GetInternalStream(getter_AddRefs(mBlobStream));
  NS_WARN_IF(NS_FAILED(rv));
}

CreateFileTask::~CreateFileTask()
{
  MOZ_ASSERT((!mPromise && !mBlobData) || NS_IsMainThread(),
             "mPromise and mBlobData should be released on main thread!");

  if (mBlobStream) {
    mBlobStream->Close();
  }
}

already_AddRefed<Promise>
CreateFileTask::GetPromise()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return nsRefPtr<Promise>(mPromise).forget();
}

FileSystemParams
CreateFileTask::GetRequestParams(const nsString& aFileSystem) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  FileSystemCreateFileParams param;
  param.filesystem() = aFileSystem;
  param.realPath() = mTargetRealPath;
  param.replace() = mReplace;
  if (mBlobData) {
    BlobChild* actor
      = ContentChild::GetSingleton()->GetOrCreateActorForBlob(mBlobData);
    if (actor) {
      param.data() = actor;
    }
  } else {
    param.data() = mArrayData;
  }
  return param;
}

FileSystemResponseValue
CreateFileTask::GetSuccessRequestResult() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  nsRefPtr<DOMFile> file = new DOMFile(mTargetFileImpl);
  BlobParent* actor = GetBlobParent(file);
  if (!actor) {
    return FileSystemErrorResponse(NS_ERROR_DOM_FILESYSTEM_UNKNOWN_ERR);
  }
  FileSystemFileResponse response;
  response.blobParent() = actor;
  return response;
}

void
CreateFileTask::SetSuccessRequestResult(const FileSystemResponseValue& aValue)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  FileSystemFileResponse r = aValue;
  BlobChild* actor = static_cast<BlobChild*>(r.blobChild());
  nsCOMPtr<nsIDOMBlob> blob = actor->GetBlob();
  mTargetFileImpl = static_cast<DOMFile*>(blob.get())->Impl();
}

nsresult
CreateFileTask::Work()
{
  class AutoClose
  {
  public:
    explicit AutoClose(nsIOutputStream* aStream)
      : mStream(aStream)
    {
      MOZ_ASSERT(aStream);
    }

    ~AutoClose()
    {
      mStream->Close();
    }
  private:
    nsCOMPtr<nsIOutputStream> mStream;
  };

  MOZ_ASSERT(FileSystemUtils::IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(!NS_IsMainThread(), "Only call on worker thread!");

  if (mFileSystem->IsShutdown()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIFile> file = mFileSystem->GetLocalFile(mTargetRealPath);
  if (!file) {
    return NS_ERROR_DOM_FILESYSTEM_INVALID_PATH_ERR;
  }

  if (!mFileSystem->IsSafeFile(file)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  bool exists = false;
  nsresult rv = file->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    bool isFile = false;
    rv = file->IsFile(&isFile);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!isFile) {
      return NS_ERROR_DOM_FILESYSTEM_TYPE_MISMATCH_ERR;
    }

    if (!mReplace) {
      return NS_ERROR_DOM_FILESYSTEM_PATH_EXISTS_ERR;
    }

    // Remove the old file before creating.
    rv = file->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = file->Create(nsIFile::NORMAL_FILE_TYPE, 0600);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIOutputStream> outputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), file);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AutoClose acOutputStream(outputStream);

  nsCOMPtr<nsIOutputStream> bufferedOutputStream;
  rv = NS_NewBufferedOutputStream(getter_AddRefs(bufferedOutputStream),
                                  outputStream,
                                  sOutputBufferSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AutoClose acBufferedOutputStream(bufferedOutputStream);

  if (mBlobStream) {
    // Write the file content from blob data.

    uint64_t bufSize = 0;
    rv = mBlobStream->Available(&bufSize);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    while (bufSize && !mFileSystem->IsShutdown()) {
      uint32_t written = 0;
      uint32_t writeSize = bufSize < UINT32_MAX ? bufSize : UINT32_MAX;
      rv = bufferedOutputStream->WriteFrom(mBlobStream, writeSize, &written);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      bufSize -= written;
    }

    mBlobStream->Close();
    mBlobStream = nullptr;

    if (mFileSystem->IsShutdown()) {
      return NS_ERROR_FAILURE;
    }

    mTargetFileImpl = new DOMFileImplFile(file);
    return NS_OK;
  }

  // Write file content from array data.

  uint32_t written;
  rv = bufferedOutputStream->Write(
    reinterpret_cast<char*>(mArrayData.Elements()),
    mArrayData.Length(),
    &written);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mArrayData.Length() != written) {
    return NS_ERROR_DOM_FILESYSTEM_UNKNOWN_ERR;
  }

  mTargetFileImpl = new DOMFileImplFile(file);
  return NS_OK;
}

void
CreateFileTask::HandlerCallback()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  if (mFileSystem->IsShutdown()) {
    mPromise = nullptr;
    mBlobData = nullptr;
    return;
  }

  if (HasError()) {
    nsRefPtr<DOMError> domError = new DOMError(mFileSystem->GetWindow(),
      mErrorValue);
    mPromise->MaybeRejectBrokenly(domError);
    mPromise = nullptr;
    mBlobData = nullptr;
    return;
  }

  nsCOMPtr<nsIDOMFile> file = new DOMFile(mTargetFileImpl);
  mPromise->MaybeResolve(file);
  mPromise = nullptr;
  mBlobData = nullptr;
}

void
CreateFileTask::GetPermissionAccessType(nsCString& aAccess) const
{
  if (mReplace) {
    aAccess.AssignLiteral("write");
    return;
  }

  aAccess.AssignLiteral("create");
}

void
CreateFileTask::GetOutputBufferSize() const
{
  if (sOutputBufferSize || !FileSystemUtils::IsParentProcess()) {
    return;
  }
  sOutputBufferSize =
    mozilla::Preferences::GetUint("dom.filesystem.outputBufferSize", 4096 * 4);
}

} // namespace dom
} // namespace mozilla
