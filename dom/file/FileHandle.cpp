/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileHandle.h"

#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"

#include "nsIDOMFile.h"
#include "nsIFileStorage.h"

#include "FileRequest.h"
#include "FileService.h"
#include "LockedFile.h"
#include "MetadataHelper.h"

USING_FILE_NAMESPACE

namespace {

class GetFileHelper : public MetadataHelper
{
public:
  GetFileHelper(LockedFile* aLockedFile,
                FileRequest* aFileRequest,
                MetadataParameters* aParams,
                FileHandle* aFileHandle)
  : MetadataHelper(aLockedFile, aFileRequest, aParams),
    mFileHandle(aFileHandle)
  { }

  nsresult
  GetSuccessResult(JSContext* aCx, jsval* aVal);

  void
  ReleaseObjects()
  {
    mFileHandle = nsnull;

    MetadataHelper::ReleaseObjects();
  }

private:
  nsRefPtr<FileHandle> mFileHandle;
};

} // anonymous namespace

NS_IMPL_CYCLE_COLLECTION_CLASS(FileHandle)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(FileHandle,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFileStorage)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(abort)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(error)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(FileHandle,
                                                nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFileStorage)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(abort)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(error)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FileHandle)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFileHandle)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(FileHandle, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(FileHandle, nsDOMEventTargetHelper)

NS_IMPL_EVENT_HANDLER(FileHandle, abort)
NS_IMPL_EVENT_HANDLER(FileHandle, error)

NS_IMETHODIMP
FileHandle::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
FileHandle::GetType(nsAString& aType)
{
  aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
FileHandle::Open(const nsAString& aMode,
                 PRUint8 aOptionalArgCount,
                 nsIDOMLockedFile** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (FileService::IsShuttingDown() || mFileStorage->IsStorageShuttingDown()) {
    return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
  }

  LockedFile::Mode mode;
  if (aOptionalArgCount) {
    if (aMode.EqualsLiteral("readwrite")) {
      mode = LockedFile::READ_WRITE;
    }
    else if (aMode.EqualsLiteral("readonly")) {
      mode = LockedFile::READ_ONLY;
    }
    else {
      return NS_ERROR_TYPE_ERR;
    }
  }
  else {
    mode = LockedFile::READ_ONLY;
  }

  nsRefPtr<LockedFile> lockedFile = LockedFile::Create(this, mode);
  NS_ENSURE_TRUE(lockedFile, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  lockedFile.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
FileHandle::GetFile(nsIDOMDOMRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // Do nothing if the window is closed
  if (!GetOwner()) {
    return NS_OK;
  }

  nsRefPtr<LockedFile> lockedFile =
    LockedFile::Create(this, LockedFile::READ_ONLY, LockedFile::PARALLEL);
  NS_ENSURE_TRUE(lockedFile, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  nsRefPtr<FileRequest> request =
    FileRequest::Create(GetOwner(), lockedFile, false);

  nsRefPtr<MetadataParameters> params = new MetadataParameters();
  params->Init(true, false);

  nsRefPtr<GetFileHelper> helper =
    new GetFileHelper(lockedFile, request, params, this);

  nsresult rv = helper->Enqueue();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  nsCOMPtr<nsIDOMDOMRequest> result = static_cast<DOMRequest*>(request);
  result.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP_(PRInt64)
FileHandle::GetFileId()
{
  return -1;
}

NS_IMETHODIMP_(mozilla::dom::indexedDB::FileInfo*)
FileHandle::GetFileInfo()
{
  return nsnull;
}

nsresult
GetFileHelper::GetSuccessResult(JSContext* aCx, jsval* aVal)
{
  nsCOMPtr<nsIDOMFile> domFile =
    mFileHandle->CreateFileObject(mLockedFile, mParams->Size());

  nsresult rv =
    nsContentUtils::WrapNative(aCx, JS_GetGlobalForScopeChain(aCx), domFile,
                               &NS_GET_IID(nsIDOMFile), aVal);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  return NS_OK;
}
