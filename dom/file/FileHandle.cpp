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

using namespace mozilla;
using namespace mozilla::dom;
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
  GetSuccessResult(JSContext* aCx, JS::Value* aVal);

  void
  ReleaseObjects()
  {
    mFileHandle = nullptr;

    MetadataHelper::ReleaseObjects();
  }

private:
  nsRefPtr<FileHandle> mFileHandle;
};

} // anonymous namespace

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(FileHandle, nsDOMEventTargetHelper,
                                     mFileStorage)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FileHandle)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFileHandle)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(FileHandle, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(FileHandle, nsDOMEventTargetHelper)

NS_IMPL_EVENT_HANDLER(FileHandle, abort)
NS_IMPL_EVENT_HANDLER(FileHandle, error)

NS_IMETHODIMP
FileHandle::GetDOMName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
FileHandle::GetDOMType(nsAString& aType)
{
  aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
FileHandle::Open(const nsAString& aMode,
                 uint8_t aOptionalArgCount,
                 nsIDOMLockedFile** _retval)
{
  FileMode mode;
  if (aOptionalArgCount) {
    if (aMode.EqualsLiteral("readwrite")) {
      mode = FileModeValues::Readwrite;
    } else if (aMode.EqualsLiteral("readonly")) {
      mode = FileModeValues::Readonly;
    } else {
      return NS_ERROR_TYPE_ERR;
    }
  } else {
    mode = FileModeValues::Readonly;
  }

  ErrorResult rv;
  nsCOMPtr<nsIDOMLockedFile> lockedFile = Open(mode, rv);
  lockedFile.forget(_retval);
  return rv.ErrorCode();
}

already_AddRefed<nsIDOMLockedFile>
FileHandle::Open(FileMode aMode, ErrorResult& aError)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (FileService::IsShuttingDown() || mFileStorage->IsShuttingDown()) {
    aError.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  MOZ_STATIC_ASSERT(static_cast<uint32_t>(FileModeValues::Readonly) ==
                    static_cast<uint32_t>(LockedFile::READ_ONLY),
                    "Enum values should match.");
  MOZ_STATIC_ASSERT(static_cast<uint32_t>(FileModeValues::Readwrite) ==
                    static_cast<uint32_t>(LockedFile::READ_WRITE),
                    "Enum values should match.");

  nsRefPtr<LockedFile> lockedFile =
    LockedFile::Create(this, static_cast<LockedFile::Mode>(aMode));
  if (!lockedFile) {
    aError.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  return lockedFile.forget();
}

NS_IMETHODIMP
FileHandle::GetFile(nsIDOMDOMRequest** _retval)
{
  ErrorResult rv;
  nsRefPtr<DOMRequest> request = GetFile(rv);
  request.forget(_retval);
  return rv.ErrorCode();
}

already_AddRefed<DOMRequest>
FileHandle::GetFile(ErrorResult& aError)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Do nothing if the window is closed
  if (!GetOwner()) {
    return nullptr;
  }

  nsRefPtr<LockedFile> lockedFile =
    LockedFile::Create(this, LockedFile::READ_ONLY, LockedFile::PARALLEL);
  if (!lockedFile) {
    aError.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  nsRefPtr<FileRequest> request =
    FileRequest::Create(GetOwner(), lockedFile, false);

  nsRefPtr<MetadataParameters> params = new MetadataParameters();
  params->Init(true, false);

  nsRefPtr<GetFileHelper> helper =
    new GetFileHelper(lockedFile, request, params, this);

  nsresult rv = helper->Enqueue();
  if (NS_FAILED(rv)) {
    aError.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  return request.forget();
}

NS_IMETHODIMP_(int64_t)
FileHandle::GetFileId()
{
  return -1;
}

NS_IMETHODIMP_(mozilla::dom::indexedDB::FileInfo*)
FileHandle::GetFileInfo()
{
  return nullptr;
}

nsresult
GetFileHelper::GetSuccessResult(JSContext* aCx, JS::Value* aVal)
{
  nsCOMPtr<nsIDOMFile> domFile =
    mFileHandle->CreateFileObject(mLockedFile, mParams->Size());

  nsresult rv =
    nsContentUtils::WrapNative(aCx, JS_GetGlobalForScopeChain(aCx), domFile,
                               &NS_GET_IID(nsIDOMFile), aVal);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  return NS_OK;
}

/* virtual */
JSObject*
FileHandle::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return FileHandleBinding::Wrap(aCx, aScope, this);
}
