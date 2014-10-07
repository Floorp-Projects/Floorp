/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBMutableFile.h"

#include "FileSnapshot.h"
#include "FileInfo.h"
#include "IDBDatabase.h"
#include "IDBFactory.h"
#include "IDBFileHandle.h"
#include "IDBFileRequest.h"
#include "IndexedDatabaseManager.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FileService.h"
#include "mozilla/dom/IDBMutableFileBinding.h"
#include "mozilla/dom/MetadataHelper.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBSharedTypes.h"
#include "mozilla/dom/quota/FileStreams.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIDOMFile.h"
#include "nsIPrincipal.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

using namespace mozilla::dom::quota;
using namespace mozilla::ipc;

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

already_AddRefed<nsIFile>
GetFileFor(FileInfo* aFileInfo)
{
  MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aFileInfo);

  FileManager* fileManager = aFileInfo->Manager();
  MOZ_ASSERT(fileManager);

  nsCOMPtr<nsIFile> directory = fileManager->GetDirectory();
  if (NS_WARN_IF(!directory)) {
    return nullptr;
  }

  nsCOMPtr<nsIFile> file =
    fileManager->GetFileForId(directory, aFileInfo->Id());
  if (NS_WARN_IF(!file)) {
    return nullptr;
  }

  return file.forget();
}

} // anonymous namespace

IDBMutableFile::IDBMutableFile(IDBDatabase* aDatabase,
                               const nsAString& aName,
                               const nsAString& aType,
                               already_AddRefed<FileInfo> aFileInfo,
                               const nsACString& aGroup,
                               const nsACString& aOrigin,
                               const nsACString& aStorageId,
                               PersistenceType aPersistenceType,
                               already_AddRefed<nsIFile> aFile)
  : DOMEventTargetHelper(aDatabase)
  , mDatabase(aDatabase)
  , mFileInfo(aFileInfo)
  , mGroup(aGroup)
  , mOrigin(aOrigin)
  , mPersistenceType(aPersistenceType)
  , mInvalidated(false)
{
  MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT(mFileInfo);

  mName = aName;
  mType = aType;
  mFile = aFile;
  mStorageId = aStorageId;
  mFileName.AppendInt(mFileInfo->Id());

  MOZ_ASSERT(mFile);

  mDatabase->NoteLiveMutableFile(this);
}

IDBMutableFile::~IDBMutableFile()
{
  // XXX This is always in the main process but it sometimes happens too late in
  //     shutdown and the IndexedDatabaseManager has already been torn down.
  // MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (mDatabase) {
    mDatabase->NoteFinishedMutableFile(this);
  }
}

// static
already_AddRefed<IDBMutableFile>
IDBMutableFile::Create(IDBDatabase* aDatabase,
                       const nsAString& aName,
                       const nsAString& aType,
                       already_AddRefed<FileInfo> aFileInfo)
{
  MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<FileInfo> fileInfo(aFileInfo);
  MOZ_ASSERT(fileInfo);

  PrincipalInfo* principalInfo = aDatabase->Factory()->GetPrincipalInfo();
  MOZ_ASSERT(principalInfo);

  nsCOMPtr<nsIPrincipal> principal = PrincipalInfoToPrincipal(*principalInfo);
  if (NS_WARN_IF(!principal)) {
    return nullptr;
  }

  nsCString group;
  nsCString origin;
  if (NS_WARN_IF(NS_FAILED(QuotaManager::GetInfoFromPrincipal(principal,
                                                              &group,
                                                              &origin,
                                                              nullptr,
                                                              nullptr)))) {
    return nullptr;
  }

  const DatabaseSpec* spec = aDatabase->Spec();
  MOZ_ASSERT(spec);

  PersistenceType persistenceType = spec->metadata().persistenceType();

  nsCString storageId;
  QuotaManager::GetStorageId(persistenceType,
                             origin,
                             Client::IDB,
                             aDatabase->Name(),
                             storageId);

  nsCOMPtr<nsIFile> file = GetFileFor(fileInfo);
  if (NS_WARN_IF(!file)) {
    return nullptr;
  }

  nsRefPtr<IDBMutableFile> newFile =
    new IDBMutableFile(aDatabase,
                       aName,
                       aType,
                       fileInfo.forget(),
                       group,
                       origin,
                       storageId,
                       persistenceType,
                       file.forget());

  return newFile.forget();
}

void
IDBMutableFile::Invalidate()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mInvalidated);

  mInvalidated = true;
}

NS_IMPL_ADDREF_INHERITED(IDBMutableFile, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBMutableFile, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBMutableFile)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBMutableFile)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBMutableFile,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDatabase)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBMutableFile,
                                                DOMEventTargetHelper)
  MOZ_ASSERT(tmp->mDatabase);
  tmp->mDatabase->NoteFinishedMutableFile(tmp);

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDatabase)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

bool
IDBMutableFile::IsInvalid()
{
  return mInvalidated;
}

nsIOfflineStorage*
IDBMutableFile::Storage()
{
  MOZ_CRASH("Don't call me!");
}

already_AddRefed<nsISupports>
IDBMutableFile::CreateStream(bool aReadOnly)
{
  MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());

  nsCOMPtr<nsISupports> result;

  if (aReadOnly) {
    nsRefPtr<FileInputStream> stream =
      FileInputStream::Create(mPersistenceType,
                              mGroup,
                              mOrigin,
                              mFile,
                              -1,
                              -1,
                              nsIFileInputStream::DEFER_OPEN);
    result = NS_ISUPPORTS_CAST(nsIFileInputStream*, stream);
  } else {
    nsRefPtr<FileStream> stream =
      FileStream::Create(mPersistenceType,
                         mGroup,
                         mOrigin,
                         mFile,
                         -1,
                         -1,
                         nsIFileStream::DEFER_OPEN);
    result = NS_ISUPPORTS_CAST(nsIFileStream*, stream);
  }

  if (NS_WARN_IF(!result)) {
    return nullptr;
  }

  return result.forget();
}

void
IDBMutableFile::SetThreadLocals()
{
  MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());
  MOZ_ASSERT(mDatabase->GetOwner(), "Should have owner!");

  QuotaManager::SetCurrentWindow(mDatabase->GetOwner());
}

void
IDBMutableFile::UnsetThreadLocals()
{
  MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());

  QuotaManager::SetCurrentWindow(nullptr);
}

JSObject*
IDBMutableFile::WrapObject(JSContext* aCx)
{
  MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());
  MOZ_ASSERT(NS_IsMainThread());

  return IDBMutableFileBinding::Wrap(aCx, this);
}

IDBDatabase*
IDBMutableFile::Database() const
{
  MOZ_ASSERT(NS_IsMainThread());

  return mDatabase;
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

int64_t
IDBMutableFile::GetFileId() const
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mFileInfo);

  return mFileInfo->Id();
}

already_AddRefed<nsIDOMFile>
IDBMutableFile::CreateFileObject(IDBFileHandle* aFileHandle,
                                 MetadataParameters* aMetadataParams)
{
  nsRefPtr<DOMFileImpl> impl =
    new FileImplSnapshot(mName,
                         mType,
                         aMetadataParams,
                         mFile,
                         aFileHandle,
                         mFileInfo);

  nsCOMPtr<nsIDOMFile> fileSnapshot = new DOMFile(GetOwner(), impl);
  return fileSnapshot.forget();
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
    IDBFileRequest::Create(GetOwner(),
                           fileHandle,
                           /* aWrapAsDOMRequest */ true);

  nsRefPtr<MetadataParameters> params = new MetadataParameters(true, true);

  nsRefPtr<GetFileHelper> helper =
    new GetFileHelper(fileHandle, request, params, this);

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
  MOZ_ASSERT(NS_IsMainThread());

  auto fileHandle = static_cast<IDBFileHandle*>(mFileHandle.get());

  nsCOMPtr<nsIDOMFile> domFile =
    mMutableFile->CreateFileObject(fileHandle, mParams);

  nsresult rv =
    nsContentUtils::WrapNative(aCx, domFile, &NS_GET_IID(nsIDOMFile), aVal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
  }

  return NS_OK;
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
