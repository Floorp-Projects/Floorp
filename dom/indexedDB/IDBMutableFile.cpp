/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBMutableFile.h"

#include "nsIDOMFile.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FileService.h"
#include "mozilla/dom/IDBMutableFileBinding.h"
#include "mozilla/dom/MetadataHelper.h"
#include "mozilla/dom/quota/FileStreams.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"

#include "FileSnapshot.h"
#include "IDBDatabase.h"
#include "IDBFileHandle.h"
#include "IDBFileRequest.h"

using namespace mozilla::dom;
USING_INDEXEDDB_NAMESPACE
USING_QUOTA_NAMESPACE

namespace {

class GetFileHelper : public MetadataHelper
{
public:
  GetFileHelper(FileHandleBase* aFileHandle,
                FileRequestBase* aFileRequest,
                MetadataParameters* aParams,
                IDBMutableFile* aMutableFile)
  : MetadataHelper(aFileHandle, aFileRequest, aParams),
    mMutableFile(aMutableFile)
  { }

  virtual nsresult
  GetSuccessResult(JSContext* aCx,
                   JS::MutableHandle<JS::Value> aVal) MOZ_OVERRIDE;

  virtual void
  ReleaseObjects() MOZ_OVERRIDE
  {
    mMutableFile = nullptr;

    MetadataHelper::ReleaseObjects();
  }

private:
  nsRefPtr<IDBMutableFile> mMutableFile;
};

inline
already_AddRefed<nsIFile>
GetFileFor(FileInfo* aFileInfo)

{
  FileManager* fileManager = aFileInfo->Manager();
  nsCOMPtr<nsIFile> directory = fileManager->GetDirectory();
  NS_ENSURE_TRUE(directory, nullptr);

  nsCOMPtr<nsIFile> file = fileManager->GetFileForId(directory,
                                                     aFileInfo->Id());
  NS_ENSURE_TRUE(file, nullptr);

  return file.forget();
}

} // anonymous namespace

namespace mozilla {
namespace dom {
namespace indexedDB {

IDBMutableFile::IDBMutableFile(IDBDatabase* aOwner)
  : DOMEventTargetHelper(aOwner)
{
}

IDBMutableFile::~IDBMutableFile()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(IDBMutableFile, DOMEventTargetHelper,
                                   mDatabase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBMutableFile)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(IDBMutableFile, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBMutableFile, DOMEventTargetHelper)

// static
already_AddRefed<IDBMutableFile>
IDBMutableFile::Create(const nsAString& aName,
                       const nsAString& aType,
                       IDBDatabase* aDatabase,
                       already_AddRefed<FileInfo> aFileInfo)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<FileInfo> fileInfo(aFileInfo);
  NS_ASSERTION(fileInfo, "Null pointer!");

  nsRefPtr<IDBMutableFile> newFile = new IDBMutableFile(aDatabase);

  newFile->mName = aName;
  newFile->mType = aType;

  newFile->mFile = GetFileFor(fileInfo);
  NS_ENSURE_TRUE(newFile->mFile, nullptr);

  newFile->mStorageId = aDatabase->Id();
  newFile->mFileName.AppendInt(fileInfo->Id());

  newFile->mDatabase = aDatabase;
  fileInfo.swap(newFile->mFileInfo);

  return newFile.forget();
}

bool
IDBMutableFile::IsInvalid()
{
  return mDatabase->IsInvalidated();
}

nsIOfflineStorage*
IDBMutableFile::Storage()
{
  return mDatabase;
}

already_AddRefed<nsISupports>
IDBMutableFile::CreateStream(bool aReadOnly)
{
  PersistenceType persistenceType = mDatabase->Type();
  const nsACString& group = mDatabase->Group();
  const nsACString& origin = mDatabase->Origin();

  nsCOMPtr<nsISupports> result;

  if (aReadOnly) {
    nsRefPtr<FileInputStream> stream =
      FileInputStream::Create(persistenceType, group, origin, mFile, -1, -1,
                              nsIFileInputStream::DEFER_OPEN);
    result = NS_ISUPPORTS_CAST(nsIFileInputStream*, stream);
  }
  else {
    nsRefPtr<FileStream> stream =
      FileStream::Create(persistenceType, group, origin, mFile, -1, -1,
                         nsIFileStream::DEFER_OPEN);
    result = NS_ISUPPORTS_CAST(nsIFileStream*, stream);
  }
  NS_ENSURE_TRUE(result, nullptr);

  return result.forget();
}

void
IDBMutableFile::SetThreadLocals()
{
  MOZ_ASSERT(mDatabase->GetOwner(), "Should have owner!");
  QuotaManager::SetCurrentWindow(mDatabase->GetOwner());
}

void
IDBMutableFile::UnsetThreadLocals()
{
  QuotaManager::SetCurrentWindow(nullptr);
}

already_AddRefed<nsIDOMFile>
IDBMutableFile::CreateFileObject(IDBFileHandle* aFileHandle, uint32_t aFileSize)
{
  nsCOMPtr<nsIDOMFile> fileSnapshot = new DOMFile(
    new FileImplSnapshot(mName, mType, aFileSize, mFile, aFileHandle,
                         mFileInfo));

  return fileSnapshot.forget();
}

// virtual
JSObject*
IDBMutableFile::WrapObject(JSContext* aCx)
{
  return IDBMutableFileBinding::Wrap(aCx, this);
}

already_AddRefed<IDBFileHandle>
IDBMutableFile::Open(FileMode aMode, ErrorResult& aError)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (QuotaManager::IsShuttingDown() || FileService::IsShuttingDown()) {
    aError.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  nsRefPtr<IDBFileHandle> fileHandle =
    IDBFileHandle::Create(aMode, FileHandleBase::NORMAL, this);
  if (!fileHandle) {
    aError.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  return fileHandle.forget();
}

already_AddRefed<DOMRequest>
IDBMutableFile::GetFile(ErrorResult& aError)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Do nothing if the window is closed
  if (!GetOwner()) {
    return nullptr;
  }

  nsRefPtr<IDBFileHandle> fileHandle =
    IDBFileHandle::Create(FileMode::Readonly, FileHandleBase::PARALLEL, this);

  nsRefPtr<IDBFileRequest> request = 
    IDBFileRequest::Create(GetOwner(), fileHandle, /* aWrapAsDOMRequest */
                           true);

  nsRefPtr<MetadataParameters> params = new MetadataParameters(true, false);

  nsRefPtr<GetFileHelper> helper =
    new GetFileHelper(fileHandle, request, params, this);

  nsresult rv = helper->Enqueue();
  if (NS_FAILED(rv)) {
    aError.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  return request.forget();
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

nsresult
GetFileHelper::GetSuccessResult(JSContext* aCx,
                                JS::MutableHandle<JS::Value> aVal)
{
  MOZ_ASSERT(NS_IsMainThread());

  auto fileHandle = static_cast<IDBFileHandle*>(mFileHandle.get());

  nsCOMPtr<nsIDOMFile> domFile =
    mMutableFile->CreateFileObject(fileHandle, mParams->Size());

  nsresult rv =
    nsContentUtils::WrapNative(aCx, domFile, &NS_GET_IID(nsIDOMFile), aVal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
  }

  return NS_OK;
}
