/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileSystemTaskBase.h"

#include "nsNetUtil.h" // Stream transport service.
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemRequestParent.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PContent.h"
#include "mozilla/unused.h"
#include "nsDOMFile.h"

namespace mozilla {
namespace dom {

FileSystemTaskBase::FileSystemTaskBase(FileSystemBase* aFileSystem)
  : mErrorValue(NS_OK)
  , mFileSystem(aFileSystem)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem, "aFileSystem should not be null.");
}

FileSystemTaskBase::FileSystemTaskBase(FileSystemBase* aFileSystem,
                                       const FileSystemParams& aParam,
                                       FileSystemRequestParent* aParent)
  : mErrorValue(NS_OK)
  , mFileSystem(aFileSystem)
  , mRequestParent(aParent)
{
  MOZ_ASSERT(FileSystemUtils::IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem, "aFileSystem should not be null.");
}

FileSystemTaskBase::~FileSystemTaskBase()
{
}

FileSystemBase*
FileSystemTaskBase::GetFileSystem() const
{
  return mFileSystem.get();
}

void
FileSystemTaskBase::Start()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");

  if (HasError()) {
    NS_DispatchToMainThread(this);
    return;
  }

  if (FileSystemUtils::IsParentProcess()) {
    // Run in parent process.
    // Start worker thread.
    nsCOMPtr<nsIEventTarget> target
      = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    NS_ASSERTION(target, "Must have stream transport service.");
    target->Dispatch(this, NS_DISPATCH_NORMAL);
    return;
  }

  // Run in child process.
  if (mFileSystem->IsShutdown()) {
    return;
  }

  // Retain a reference so the task object isn't deleted without IPDL's
  // knowledge. The reference will be released by
  // mozilla::dom::ContentChild::DeallocPFileSystemRequestChild.
  NS_ADDREF_THIS();
  ContentChild::GetSingleton()->SendPFileSystemRequestConstructor(this,
    GetRequestParams(mFileSystem->ToString()));
}

NS_IMETHODIMP
FileSystemTaskBase::Run()
{
  if (!NS_IsMainThread()) {
    // Run worker thread tasks
    nsresult rv = Work();
    if (NS_FAILED(rv)) {
      SetError(rv);
    }
    // Dispatch itself to main thread
    NS_DispatchToMainThread(this);
    return NS_OK;
  }

  // Run main thread tasks
  HandleResult();
  return NS_OK;
}

void
FileSystemTaskBase::HandleResult()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  if (mFileSystem->IsShutdown()) {
    return;
  }
  if (mRequestParent && mRequestParent->IsRunning()) {
    unused << mRequestParent->Send__delete__(mRequestParent,
      GetRequestResult());
  } else {
    HandlerCallback();
  }
}

FileSystemResponseValue
FileSystemTaskBase::GetRequestResult() const
{
  MOZ_ASSERT(FileSystemUtils::IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  if (HasError()) {
    return FileSystemErrorResponse(mErrorValue);
  } else {
    return GetSuccessRequestResult();
  }
}

void
FileSystemTaskBase::SetRequestResult(const FileSystemResponseValue& aValue)
{
  MOZ_ASSERT(!FileSystemUtils::IsParentProcess(),
             "Only call from child process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  if (aValue.type() == FileSystemResponseValue::TFileSystemErrorResponse) {
    FileSystemErrorResponse r = aValue;
    mErrorValue = r.error();
  } else {
    SetSuccessRequestResult(aValue);
  }
}

bool
FileSystemTaskBase::Recv__delete__(const FileSystemResponseValue& aValue)
{
  SetRequestResult(aValue);
  HandlerCallback();
  return true;
}

BlobParent*
FileSystemTaskBase::GetBlobParent(nsIDOMFile* aFile) const
{
  MOZ_ASSERT(FileSystemUtils::IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFile);

  // Load the lazy dom file data from the parent before sending to the child.
  nsString mimeType;
  aFile->GetType(mimeType);
  uint64_t fileSize;
  aFile->GetSize(&fileSize);
  uint64_t lastModifiedDate;
  aFile->GetMozLastModifiedDate(&lastModifiedDate);

  ContentParent* cp = static_cast<ContentParent*>(mRequestParent->Manager());
  return cp->GetOrCreateActorForBlob(aFile);
}

void
FileSystemTaskBase::SetError(const nsresult& aErrorValue)
{
  uint16_t module = NS_ERROR_GET_MODULE(aErrorValue);
  if (module == NS_ERROR_MODULE_DOM_FILESYSTEM ||
      module == NS_ERROR_MODULE_DOM_FILE ||
      module == NS_ERROR_MODULE_DOM) {
    mErrorValue = aErrorValue;
    return;
  }

  switch (aErrorValue) {
    case NS_OK:
      mErrorValue = NS_OK;
      return;

    case NS_ERROR_FILE_INVALID_PATH:
    case NS_ERROR_FILE_UNRECOGNIZED_PATH:
      mErrorValue = NS_ERROR_DOM_FILESYSTEM_INVALID_PATH_ERR;
      return;

    case NS_ERROR_FILE_DESTINATION_NOT_DIR:
      mErrorValue = NS_ERROR_DOM_FILESYSTEM_INVALID_MODIFICATION_ERR;
      return;

    case NS_ERROR_FILE_ACCESS_DENIED:
    case NS_ERROR_FILE_DIR_NOT_EMPTY:
      mErrorValue = NS_ERROR_DOM_FILESYSTEM_NO_MODIFICATION_ALLOWED_ERR;
      return;

    case NS_ERROR_FILE_TARGET_DOES_NOT_EXIST:
    case NS_ERROR_NOT_AVAILABLE:
      mErrorValue = NS_ERROR_DOM_FILE_NOT_FOUND_ERR;
      return;

    case NS_ERROR_FILE_ALREADY_EXISTS:
      mErrorValue = NS_ERROR_DOM_FILESYSTEM_PATH_EXISTS_ERR;
      return;

    case NS_ERROR_FILE_NOT_DIRECTORY:
      mErrorValue = NS_ERROR_DOM_FILESYSTEM_TYPE_MISMATCH_ERR;
      return;

    case NS_ERROR_UNEXPECTED:
    default:
      mErrorValue = NS_ERROR_DOM_FILESYSTEM_UNKNOWN_ERR;
      return;
  }
}

} // namespace dom
} // namespace mozilla
