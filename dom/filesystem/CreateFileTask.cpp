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
#include "mozilla/dom/PFileSystemParams.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsIFile.h"
#include "nsNetUtil.h"
#include "nsIOutputStream.h"
#include "nsStringGlue.h"

#define GET_PERMISSION_ACCESS_TYPE(aAccess)                \
  if (mReplace) {                                          \
    aAccess.AssignLiteral(DIRECTORY_WRITE_PERMISSION);     \
    return;                                                \
  }                                                        \
  aAccess.AssignLiteral(DIRECTORY_CREATE_PERMISSION);

namespace mozilla {
namespace dom {

/**
 *CreateFileTaskChild
 */

/* static */ already_AddRefed<CreateFileTaskChild>
CreateFileTaskChild::Create(FileSystemBase* aFileSystem,
                            nsIFile* aTargetPath,
                            Blob* aBlobData,
                            InfallibleTArray<uint8_t>& aArrayData,
                            bool aReplace,
                            ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);

  RefPtr<CreateFileTaskChild> task =
    new CreateFileTaskChild(aFileSystem, aTargetPath, aReplace);

  // aTargetPath can be null. In this case SetError will be called.

  if (aBlobData) {
    task->mBlobImpl = aBlobData->Impl();
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

CreateFileTaskChild::CreateFileTaskChild(FileSystemBase* aFileSystem,
                                         nsIFile* aTargetPath,
                                         bool aReplace)
  : FileSystemTaskChildBase(aFileSystem)
  , mTargetPath(aTargetPath)
  , mReplace(aReplace)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);
}

CreateFileTaskChild::~CreateFileTaskChild()
{
  MOZ_ASSERT(NS_IsMainThread());
}

already_AddRefed<Promise>
CreateFileTaskChild::GetPromise()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return RefPtr<Promise>(mPromise).forget();
}

FileSystemParams
CreateFileTaskChild::GetRequestParams(const nsString& aSerializedDOMPath,
                                      ErrorResult& aRv) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  FileSystemCreateFileParams param;
  param.filesystem() = aSerializedDOMPath;

  aRv = mTargetPath->GetPath(param.realPath());
  if (NS_WARN_IF(aRv.Failed())) {
    return param;
  }

  // If we are here, PBackground must be up and running: this method is called
  // when the task has been already started by FileSystemPermissionRequest
  // class and this happens only when PBackground actor has already been
  // created.
  PBackgroundChild* actor =
    mozilla::ipc::BackgroundChild::GetForCurrentThread();
  MOZ_ASSERT(actor);

  param.replace() = mReplace;
  if (mBlobImpl) {
    PBlobChild* blobActor =
      mozilla::ipc::BackgroundChild::GetOrCreateActorForBlobImpl(actor,
                                                                 mBlobImpl);
    if (blobActor) {
      param.data() = blobActor;
    }
  } else {
    param.data() = mArrayData;
  }
  return param;
}

void
CreateFileTaskChild::SetSuccessRequestResult(const FileSystemResponseValue& aValue,
                                             ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");

  const FileSystemFileResponse& r = aValue.get_FileSystemFileResponse();

  NS_ConvertUTF16toUTF8 path(r.realPath());
  aRv = NS_NewNativeLocalFile(path, true, getter_AddRefs(mTargetPath));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

void
CreateFileTaskChild::HandlerCallback()
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

  RefPtr<File> file = File::CreateFromFile(mFileSystem->GetParentObject(),
                                           mTargetPath);
  mPromise->MaybeResolve(file);
  mPromise = nullptr;
}

void
CreateFileTaskChild::GetPermissionAccessType(nsCString& aAccess) const
{
  GET_PERMISSION_ACCESS_TYPE(aAccess)
}

/**
 * CreateFileTaskParent
 */

uint32_t CreateFileTaskParent::sOutputBufferSize = 0;

/* static */ already_AddRefed<CreateFileTaskParent>
CreateFileTaskParent::Create(FileSystemBase* aFileSystem,
                             const FileSystemCreateFileParams& aParam,
                             FileSystemRequestParent* aParent,
                             ErrorResult& aRv)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aFileSystem);

  RefPtr<CreateFileTaskParent> task =
    new CreateFileTaskParent(aFileSystem, aParam, aParent);

  NS_ConvertUTF16toUTF8 path(aParam.realPath());
  aRv = NS_NewNativeLocalFile(path, true, getter_AddRefs(task->mTargetPath));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  task->mReplace = aParam.replace();

  const FileSystemFileDataValue& data = aParam.data();

  if (data.type() == FileSystemFileDataValue::TArrayOfuint8_t) {
    task->mArrayData = data;
    return task.forget();
  }

  MOZ_ASSERT(data.type() == FileSystemFileDataValue::TPBlobParent);

  BlobParent* bp = static_cast<BlobParent*>(static_cast<PBlobParent*>(data));
  task->mBlobImpl = bp->GetBlobImpl();
  MOZ_ASSERT(task->mBlobImpl, "blobData should not be null.");

  return task.forget();
}

CreateFileTaskParent::CreateFileTaskParent(FileSystemBase* aFileSystem,
                                           const FileSystemCreateFileParams& aParam,
                                           FileSystemRequestParent* aParent)
  : FileSystemTaskParentBase(aFileSystem, aParam, aParent)
  , mReplace(false)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aFileSystem);
}

FileSystemResponseValue
CreateFileTaskParent::GetSuccessRequestResult(ErrorResult& aRv) const
{
  AssertIsOnBackgroundThread();

  nsAutoString path;
  aRv = mTargetPath->GetPath(path);
  if (NS_WARN_IF(aRv.Failed())) {
    return FileSystemDirectoryResponse();
  }

  return FileSystemFileResponse(path, EmptyString());
}

nsresult
CreateFileTaskParent::IOWork()
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
  MOZ_ASSERT(sOutputBufferSize);

  nsCOMPtr<nsIOutputStream> bufferedOutputStream;
  rv = NS_NewBufferedOutputStream(getter_AddRefs(bufferedOutputStream),
                                  outputStream,
                                  sOutputBufferSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AutoClose acBufferedOutputStream(bufferedOutputStream);

  // Write the file content from blob data.
  if (mBlobImpl) {
    ErrorResult error;
    nsCOMPtr<nsIInputStream> blobStream;
    mBlobImpl->GetInternalStream(getter_AddRefs(blobStream), error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }

    uint64_t bufSize = 0;
    rv = blobStream->Available(&bufSize);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    while (bufSize && !mFileSystem->IsShutdown()) {
      uint32_t written = 0;
      uint32_t writeSize = bufSize < UINT32_MAX ? bufSize : UINT32_MAX;
      rv = bufferedOutputStream->WriteFrom(blobStream, writeSize, &written);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      bufSize -= written;
    }

    blobStream->Close();

    if (mFileSystem->IsShutdown()) {
      return NS_ERROR_FAILURE;
    }

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

  return NS_OK;
}

nsresult
CreateFileTaskParent::MainThreadWork()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!sOutputBufferSize) {
    sOutputBufferSize =
      mozilla::Preferences::GetUint("dom.filesystem.outputBufferSize", 4096 * 4);
  }

  return FileSystemTaskParentBase::MainThreadWork();
}

void
CreateFileTaskParent::GetPermissionAccessType(nsCString& aAccess) const
{
  GET_PERMISSION_ACCESS_TYPE(aAccess)
}

} // namespace dom
} // namespace mozilla
