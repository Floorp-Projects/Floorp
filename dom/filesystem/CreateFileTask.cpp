/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CreateFileTask.h"

#include <algorithm>

#include "mozilla/Preferences.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "nsIFile.h"
#include "nsNetUtil.h"
#include "nsIOutputStream.h"
#include "nsStringGlue.h"

namespace mozilla {
namespace dom {

uint32_t CreateFileTask::sOutputBufferSize = 0;

/* static */ already_AddRefed<CreateFileTask>
CreateFileTask::Create(FileSystemBase* aFileSystem,
                       nsIFile* aTargetPath,
                       Blob* aBlobData,
                       InfallibleTArray<uint8_t>& aArrayData,
                       bool aReplace,
                       ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);

  RefPtr<CreateFileTask> task =
    new CreateFileTask(aFileSystem, aTargetPath, aReplace);

  // aTargetPath can be null. In this case SetError will be called.

  task->GetOutputBufferSize();

  if (aBlobData) {
    if (XRE_IsParentProcess()) {
      aBlobData->GetInternalStream(getter_AddRefs(task->mBlobStream), aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return nullptr;
      }
    } else {
      task->mBlobData = aBlobData;
    }
  }

  task->mArrayData.SwapElements(aArrayData);

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

/* static */ already_AddRefed<CreateFileTask>
CreateFileTask::Create(FileSystemBase* aFileSystem,
                       const FileSystemCreateFileParams& aParam,
                       FileSystemRequestParent* aParent,
                       ErrorResult& aRv)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);

  RefPtr<CreateFileTask> task =
    new CreateFileTask(aFileSystem, aParam, aParent);

  task->GetOutputBufferSize();

  NS_ConvertUTF16toUTF8 path(aParam.realPath());
  aRv = NS_NewNativeLocalFile(path, true, getter_AddRefs(task->mTargetPath));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  task->mReplace = aParam.replace();

  auto& data = aParam.data();

  if (data.type() == FileSystemFileDataValue::TArrayOfuint8_t) {
    task->mArrayData = data;
    return task.forget();
  }

  BlobParent* bp = static_cast<BlobParent*>(static_cast<PBlobParent*>(data));
  RefPtr<BlobImpl> blobImpl = bp->GetBlobImpl();
  MOZ_ASSERT(blobImpl, "blobData should not be null.");

  ErrorResult rv;
  blobImpl->GetInternalStream(getter_AddRefs(task->mBlobStream), rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
  }

  return task.forget();
}

CreateFileTask::CreateFileTask(FileSystemBase* aFileSystem,
                               nsIFile* aTargetPath,
                               bool aReplace)
  : FileSystemTaskBase(aFileSystem)
  , mTargetPath(aTargetPath)
  , mReplace(aReplace)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);
}

CreateFileTask::CreateFileTask(FileSystemBase* aFileSystem,
                               const FileSystemCreateFileParams& aParam,
                               FileSystemRequestParent* aParent)
  : FileSystemTaskBase(aFileSystem, aParam, aParent)
  , mReplace(false)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);
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
  return RefPtr<Promise>(mPromise).forget();
}

FileSystemParams
CreateFileTask::GetRequestParams(const nsString& aSerializedDOMPath,
                                 ErrorResult& aRv) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  FileSystemCreateFileParams param;
  param.filesystem() = aSerializedDOMPath;

  aRv = mTargetPath->GetPath(param.realPath());
  if (NS_WARN_IF(aRv.Failed())) {
    return param;
  }

  param.replace() = mReplace;
  if (mBlobData) {
    BlobChild* actor =
      ContentChild::GetSingleton()->GetOrCreateActorForBlob(mBlobData);
    if (actor) {
      param.data() = actor;
    }
  } else {
    param.data() = mArrayData;
  }
  return param;
}

FileSystemResponseValue
CreateFileTask::GetSuccessRequestResult(ErrorResult& aRv) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  BlobParent* actor = GetBlobParent(mTargetBlobImpl);
  if (!actor) {
    return FileSystemErrorResponse(NS_ERROR_DOM_FILESYSTEM_UNKNOWN_ERR);
  }
  FileSystemFileResponse response;
  response.blobParent() = actor;
  return response;
}

void
CreateFileTask::SetSuccessRequestResult(const FileSystemResponseValue& aValue,
                                        ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  FileSystemFileResponse r = aValue;
  BlobChild* actor = static_cast<BlobChild*>(r.blobChild());
  mTargetBlobImpl = actor->GetBlobImpl();
}

nsresult
CreateFileTask::Work()
{
  class MOZ_RAII AutoClose final
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

  MOZ_ASSERT(XRE_IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(!NS_IsMainThread(), "Only call on worker thread!");

  if (mFileSystem->IsShutdown()) {
    return NS_ERROR_FAILURE;
  }

  if (!mFileSystem->IsSafeFile(mTargetPath)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  bool exists = false;
  nsresult rv = mTargetPath->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    bool isFile = false;
    rv = mTargetPath->IsFile(&isFile);
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
    rv = mTargetPath->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = mTargetPath->Create(nsIFile::NORMAL_FILE_TYPE, 0600);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIOutputStream> outputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), mTargetPath);
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

    mTargetBlobImpl = new BlobImplFile(mTargetPath);
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

  mTargetBlobImpl = new BlobImplFile(mTargetPath);
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
    mPromise->MaybeReject(mErrorValue);
    mPromise = nullptr;
    mBlobData = nullptr;
    return;
  }

  RefPtr<Blob> blob = Blob::Create(mFileSystem->GetParentObject(),
                                   mTargetBlobImpl);
  mPromise->MaybeResolve(blob);
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
  if (sOutputBufferSize || !XRE_IsParentProcess()) {
    return;
  }
  sOutputBufferSize =
    mozilla::Preferences::GetUint("dom.filesystem.outputBufferSize", 4096 * 4);
}

} // namespace dom
} // namespace mozilla
