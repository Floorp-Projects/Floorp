/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileHandle.h"

#include "File.h"
#include "FileRequest.h"
#include "FileService.h"
#include "LockedFile.h"
#include "MetadataHelper.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/FileHandleBinding.h"
#include "mozilla/ErrorResult.h"
#include "nsAutoPtr.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIDOMFile.h"
#include "nsIFile.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

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

  virtual nsresult
  GetSuccessResult(JSContext* aCx,
                   JS::MutableHandle<JS::Value> aVal) MOZ_OVERRIDE;

  virtual void
  ReleaseObjects() MOZ_OVERRIDE
  {
    mFileHandle = nullptr;

    MetadataHelper::ReleaseObjects();
  }

private:
  nsRefPtr<FileHandle> mFileHandle;
};

} // anonymous namespace

FileHandle::FileHandle(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
{
}

FileHandle::FileHandle(DOMEventTargetHelper* aOwner)
  : DOMEventTargetHelper(aOwner)
{
}

FileHandle::~FileHandle()
{
}

bool
FileHandle::IsShuttingDown()
{
  return FileService::IsShuttingDown();
}

// virtual
already_AddRefed<nsISupports>
FileHandle::CreateStream(nsIFile* aFile, bool aReadOnly)
{
  nsresult rv;

  if (aReadOnly) {
    nsCOMPtr<nsIInputStream> stream;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), aFile, -1, -1,
                                    nsIFileInputStream::DEFER_OPEN);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
    return stream.forget();
  }

  nsCOMPtr<nsIFileStream> stream;
  rv = NS_NewLocalFileStream(getter_AddRefs(stream), aFile, -1, -1,
                             nsIFileStream::DEFER_OPEN);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }
  return stream.forget();
}

// virtual
already_AddRefed<nsIDOMFile>
FileHandle::CreateFileObject(LockedFile* aLockedFile, uint32_t aFileSize)
{
  nsCOMPtr<nsIDOMFile> file =
    new File(mName, mType, aFileSize, mFile, aLockedFile);

  return file.forget();
}

// virtual
JSObject*
FileHandle::WrapObject(JSContext* aCx)
{
  return FileHandleBinding::Wrap(aCx, this);
}

already_AddRefed<LockedFile>
FileHandle::Open(FileMode aMode, ErrorResult& aError)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (IsShuttingDown()) {
    aError.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  nsRefPtr<LockedFile> lockedFile = LockedFile::Create(this, aMode);
  if (!lockedFile) {
    aError.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  return lockedFile.forget();
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
    LockedFile::Create(this, FileMode::Readonly, LockedFile::PARALLEL);
  if (!lockedFile) {
    aError.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  nsRefPtr<FileRequest> request =
    FileRequest::Create(GetOwner(), lockedFile, /* aWrapAsDOMRequest */ true);

  nsRefPtr<MetadataParameters> params = new MetadataParameters(true, false);

  nsRefPtr<GetFileHelper> helper =
    new GetFileHelper(lockedFile, request, params, this);

  nsresult rv = helper->Enqueue();
  if (NS_FAILED(rv)) {
    aError.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  return request.forget();
}

nsresult
GetFileHelper::GetSuccessResult(JSContext* aCx,
                                JS::MutableHandle<JS::Value> aVal)
{
  nsCOMPtr<nsIDOMFile> domFile =
    mFileHandle->CreateFileObject(mLockedFile, mParams->Size());

  nsresult rv =
    nsContentUtils::WrapNative(aCx, domFile, &NS_GET_IID(nsIDOMFile), aVal);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
