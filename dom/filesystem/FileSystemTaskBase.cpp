/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileSystemTaskBase.h"

#include "nsNetCID.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemRequestParent.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/Unused.h"
#include "nsProxyRelease.h"

namespace mozilla {
namespace dom {

namespace {

nsresult
FileSystemErrorFromNsError(const nsresult& aErrorValue)
{
  uint16_t module = NS_ERROR_GET_MODULE(aErrorValue);
  if (module == NS_ERROR_MODULE_DOM_FILESYSTEM ||
      module == NS_ERROR_MODULE_DOM_FILE ||
      module == NS_ERROR_MODULE_DOM) {
    return aErrorValue;
  }

  switch (aErrorValue) {
    case NS_OK:
      return NS_OK;

    case NS_ERROR_FILE_INVALID_PATH:
    case NS_ERROR_FILE_UNRECOGNIZED_PATH:
      return NS_ERROR_DOM_FILESYSTEM_INVALID_PATH_ERR;

    case NS_ERROR_FILE_DESTINATION_NOT_DIR:
      return NS_ERROR_DOM_FILESYSTEM_INVALID_MODIFICATION_ERR;

    case NS_ERROR_FILE_ACCESS_DENIED:
    case NS_ERROR_FILE_DIR_NOT_EMPTY:
      return NS_ERROR_DOM_FILESYSTEM_NO_MODIFICATION_ALLOWED_ERR;

    case NS_ERROR_FILE_TARGET_DOES_NOT_EXIST:
    case NS_ERROR_NOT_AVAILABLE:
      return NS_ERROR_DOM_FILE_NOT_FOUND_ERR;

    case NS_ERROR_FILE_ALREADY_EXISTS:
      return NS_ERROR_DOM_FILESYSTEM_PATH_EXISTS_ERR;

    case NS_ERROR_FILE_NOT_DIRECTORY:
      return NS_ERROR_DOM_FILESYSTEM_TYPE_MISMATCH_ERR;

    case NS_ERROR_UNEXPECTED:
    default:
      return NS_ERROR_DOM_FILESYSTEM_UNKNOWN_ERR;
  }
}

nsresult
DispatchToIOThread(nsIRunnable* aRunnable)
{
  MOZ_ASSERT(aRunnable);

  nsCOMPtr<nsIEventTarget> target
    = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target);

  return target->Dispatch(aRunnable, NS_DISPATCH_NORMAL);
}

// This runnable is used when an error value is set before doing any real
// operation on the I/O thread. In this case we skip all and we directly
// communicate the error.
class ErrorRunnable final : public CancelableRunnable
{
public:
  explicit ErrorRunnable(FileSystemTaskChildBase* aTask)
    : mTask(aTask)
  {
    MOZ_ASSERT(aTask);
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mTask->HasError());

    mTask->HandlerCallback();
    return NS_OK;
  }

private:
  RefPtr<FileSystemTaskChildBase> mTask;
};

} // anonymous namespace

/**
 * FileSystemTaskBase class
 */

FileSystemTaskChildBase::FileSystemTaskChildBase(FileSystemBase* aFileSystem)
  : mErrorValue(NS_OK)
  , mFileSystem(aFileSystem)
{
  MOZ_ASSERT(aFileSystem, "aFileSystem should not be null.");
  aFileSystem->AssertIsOnOwningThread();
}

FileSystemTaskChildBase::~FileSystemTaskChildBase()
{
  mFileSystem->AssertIsOnOwningThread();
}

FileSystemBase*
FileSystemTaskChildBase::GetFileSystem() const
{
  mFileSystem->AssertIsOnOwningThread();
  return mFileSystem.get();
}

void
FileSystemTaskChildBase::Start()
{
  mFileSystem->AssertIsOnOwningThread();

  if (HasError()) {
    // In this case we don't want to use IPC at all.
    RefPtr<ErrorRunnable> runnable = new ErrorRunnable(this);
    nsresult rv = NS_DispatchToCurrentThread(runnable);
    NS_WARN_IF(NS_FAILED(rv));
    return;
  }

  if (mFileSystem->IsShutdown()) {
    return;
  }

  nsAutoString serialization;
  mFileSystem->SerializeDOMPath(serialization);

  ErrorResult rv;
  FileSystemParams params = GetRequestParams(serialization, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
    return;
  }

  // Retain a reference so the task object isn't deleted without IPDL's
  // knowledge. The reference will be released by
  // mozilla::ipc::BackgroundChildImpl::DeallocPFileSystemRequestChild.
  NS_ADDREF_THIS();

  // If we are here, PBackground must be up and running, because Start() is
  // called only by FileSystemPermissionRequest, and that class takes care of
  // PBackground initialization.
  PBackgroundChild* actor =
    mozilla::ipc::BackgroundChild::GetForCurrentThread();
  MOZ_ASSERT(actor);

  actor->SendPFileSystemRequestConstructor(this, params);
}

void
FileSystemTaskChildBase::SetRequestResult(const FileSystemResponseValue& aValue)
{
  mFileSystem->AssertIsOnOwningThread();

  if (aValue.type() == FileSystemResponseValue::TFileSystemErrorResponse) {
    FileSystemErrorResponse r = aValue;
    mErrorValue = r.error();
  } else {
    ErrorResult rv;
    SetSuccessRequestResult(aValue, rv);
    mErrorValue = rv.StealNSResult();
  }
}

bool
FileSystemTaskChildBase::Recv__delete__(const FileSystemResponseValue& aValue)
{
  mFileSystem->AssertIsOnOwningThread();

  SetRequestResult(aValue);
  HandlerCallback();
  return true;
}

void
FileSystemTaskChildBase::SetError(const nsresult& aErrorValue)
{
  mErrorValue = FileSystemErrorFromNsError(aErrorValue);
}

/**
 * FileSystemTaskParentBase class
 */

FileSystemTaskParentBase::FileSystemTaskParentBase(FileSystemBase* aFileSystem,
                                                  const FileSystemParams& aParam,
                                                  FileSystemRequestParent* aParent)
  : mErrorValue(NS_OK)
  , mFileSystem(aFileSystem)
  , mRequestParent(aParent)
  , mBackgroundEventTarget(NS_GetCurrentThread())
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(aFileSystem, "aFileSystem should not be null.");
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(mBackgroundEventTarget);
  AssertIsOnBackgroundThread();
}

FileSystemTaskParentBase::~FileSystemTaskParentBase()
{
  // This task can be released on different threads because we dispatch it (as
  // runnable) to main-thread, I/O and then back to the PBackground thread.
  NS_ProxyRelease(mBackgroundEventTarget, mFileSystem.forget());
  NS_ProxyRelease(mBackgroundEventTarget, mRequestParent.forget());
}

void
FileSystemTaskParentBase::Start()
{
  AssertIsOnBackgroundThread();
  mFileSystem->AssertIsOnOwningThread();

  if (NeedToGoToMainThread()) {
    nsresult rv = NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL);
    NS_WARN_IF(NS_FAILED(rv));
    return;
  }

  nsresult rv = DispatchToIOThread(this);
  NS_WARN_IF(NS_FAILED(rv));
}

void
FileSystemTaskParentBase::HandleResult()
{
  AssertIsOnBackgroundThread();
  mFileSystem->AssertIsOnOwningThread();

  if (mFileSystem->IsShutdown()) {
    return;
  }

  MOZ_ASSERT(mRequestParent);
  Unused << mRequestParent->Send__delete__(mRequestParent, GetRequestResult());
}

FileSystemResponseValue
FileSystemTaskParentBase::GetRequestResult() const
{
  AssertIsOnBackgroundThread();
  mFileSystem->AssertIsOnOwningThread();

  if (HasError()) {
    return FileSystemErrorResponse(mErrorValue);
  }

  ErrorResult rv;
  FileSystemResponseValue value = GetSuccessRequestResult(rv);
  if (NS_WARN_IF(rv.Failed())) {
    return FileSystemErrorResponse(rv.StealNSResult());
  }

  return value;
}

void
FileSystemTaskParentBase::SetError(const nsresult& aErrorValue)
{
  mErrorValue = FileSystemErrorFromNsError(aErrorValue);
}

bool
FileSystemTaskParentBase::NeedToGoToMainThread() const
{
  return mFileSystem->NeedToGoToMainThread();
}

nsresult
FileSystemTaskParentBase::MainThreadWork()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mFileSystem->MainThreadWork();
}

NS_IMETHODIMP
FileSystemTaskParentBase::Run()
{
  // This method can run in 3 different threads. Here why:
  // 1. if we are on the main-thread it's because the task must do something
  //    here. If no errors are returned we go the step 2.
  // 2. We can be here directly if the task doesn't have nothing to do on the
  //    main-thread. We are are on the I/O thread and we call IOWork().
  // 3. Both step 1 (in case of error) and step 2 end up here where return the
  //    value back to the PBackground thread.
  if (NS_IsMainThread()) {
    MOZ_ASSERT(NeedToGoToMainThread());

    nsresult rv = MainThreadWork();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      SetError(rv);

      // Something when wrong. Let's go to the Background thread directly
      // skipping the I/O thread step.
      rv = mBackgroundEventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    // Next step must happen on the I/O thread.
    rv = DispatchToIOThread(this);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  // Run I/O thread tasks
  if (!IsOnBackgroundThread()) {
    nsresult rv = IOWork();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      SetError(rv);
    }

    // Let's go back to PBackground thread to finish the work.
    rv = mBackgroundEventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  // If we are here, it's because the I/O work has been done and we have to
  // handle the result back via IPC.
  AssertIsOnBackgroundThread();
  HandleResult();
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
