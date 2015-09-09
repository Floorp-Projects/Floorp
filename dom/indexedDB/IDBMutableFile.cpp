/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBMutableFile.h"

#include "ActorsChild.h"
#include "FileInfo.h"
#include "FileSnapshot.h"
#include "IDBDatabase.h"
#include "IDBFactory.h"
#include "IDBFileHandle.h"
#include "IDBFileRequest.h"
#include "IndexedDatabaseManager.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/IDBMutableFileBinding.h"
#include "mozilla/dom/filehandle/ActorsChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBSharedTypes.h"
#include "mozilla/dom/quota/FileStreams.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIInputStream.h"
#include "nsIPrincipal.h"
#include "ReportInternalError.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

using namespace mozilla::dom::quota;

IDBMutableFile::IDBMutableFile(IDBDatabase* aDatabase,
                               BackgroundMutableFileChild* aActor,
                               const nsAString& aName,
                               const nsAString& aType)
  : DOMEventTargetHelper(aDatabase)
  , MutableFileBase(DEBUGONLY(aDatabase->OwningThread(),)
                    aActor)
  , mDatabase(aDatabase)
  , mName(aName)
  , mType(aType)
  , mInvalidated(false)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aDatabase);
  aDatabase->AssertIsOnOwningThread();

  mDatabase->NoteLiveMutableFile(this);
}

IDBMutableFile::~IDBMutableFile()
{
  AssertIsOnOwningThread();

  mDatabase->NoteFinishedMutableFile(this);
}

int64_t
IDBMutableFile::GetFileId() const
{
  AssertIsOnOwningThread();

  int64_t fileId;
  if (!mBackgroundActor ||
      NS_WARN_IF(!mBackgroundActor->SendGetFileId(&fileId))) {
    return -1;
  }

  return fileId;
}

void
IDBMutableFile::Invalidate()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mInvalidated);

  mInvalidated = true;

  AbortFileHandles();
}

void
IDBMutableFile::RegisterFileHandle(IDBFileHandle* aFileHandle)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFileHandle);
  aFileHandle->AssertIsOnOwningThread();
  MOZ_ASSERT(!mFileHandles.Contains(aFileHandle));

  mFileHandles.PutEntry(aFileHandle);
}

void
IDBMutableFile::UnregisterFileHandle(IDBFileHandle* aFileHandle)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFileHandle);
  aFileHandle->AssertIsOnOwningThread();
  MOZ_ASSERT(mFileHandles.Contains(aFileHandle));

  mFileHandles.RemoveEntry(aFileHandle);
}

void
IDBMutableFile::AbortFileHandles()
{
  AssertIsOnOwningThread();

  class MOZ_STACK_CLASS Helper final
  {
  public:
    static void
    AbortFileHandles(nsTHashtable<nsPtrHashKey<IDBFileHandle>>& aTable)
    {
      if (!aTable.Count()) {
        return;
      }

      nsTArray<nsRefPtr<IDBFileHandle>> fileHandlesToAbort;
      fileHandlesToAbort.SetCapacity(aTable.Count());

      for (auto iter = aTable.Iter(); !iter.Done(); iter.Next()) {
        IDBFileHandle* fileHandle = iter.Get()->GetKey();
        MOZ_ASSERT(fileHandle);

        fileHandle->AssertIsOnOwningThread();

        if (!fileHandle->IsDone()) {
          fileHandlesToAbort.AppendElement(iter.Get()->GetKey());
        }
      }
      MOZ_ASSERT(fileHandlesToAbort.Length() <= aTable.Count());

      if (fileHandlesToAbort.IsEmpty()) {
        return;
      }

      for (nsRefPtr<IDBFileHandle>& fileHandle : fileHandlesToAbort) {
        MOZ_ASSERT(fileHandle);

        fileHandle->Abort();
      }
    }
  };

  Helper::AbortFileHandles(mFileHandles);
}

IDBDatabase*
IDBMutableFile::Database() const
{
  AssertIsOnOwningThread();

  return mDatabase;
}

already_AddRefed<IDBFileHandle>
IDBMutableFile::Open(FileMode aMode, ErrorResult& aError)
{
  AssertIsOnOwningThread();

  if (QuotaManager::IsShuttingDown() ||
      mDatabase->IsClosed() ||
      !GetOwner()) {
    aError.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  nsRefPtr<IDBFileHandle> fileHandle =
    IDBFileHandle::Create(this, aMode);
  if (NS_WARN_IF(!fileHandle)) {
    aError.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  BackgroundFileHandleChild* actor =
    new BackgroundFileHandleChild(DEBUGONLY(mBackgroundActor->OwningThread(),)
                                  fileHandle);

  MOZ_ALWAYS_TRUE(
    mBackgroundActor->SendPBackgroundFileHandleConstructor(actor, aMode));

  fileHandle->SetBackgroundActor(actor);

  return fileHandle.forget();
}

already_AddRefed<DOMRequest>
IDBMutableFile::GetFile(ErrorResult& aError)
{
  nsRefPtr<IDBFileHandle> fileHandle = Open(FileMode::Readonly, aError);
  if (NS_WARN_IF(aError.Failed())) {
    return nullptr;
  }

  FileRequestGetFileParams params;

  nsRefPtr<IDBFileRequest> request =
    IDBFileRequest::Create(GetOwner(),
                           fileHandle,
                           /* aWrapAsDOMRequest */ true);

  fileHandle->StartRequest(request, params);

  return request.forget();
}

NS_IMPL_ADDREF_INHERITED(IDBMutableFile, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBMutableFile, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBMutableFile)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBMutableFile)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBMutableFile,
                                                  DOMEventTargetHelper)
  tmp->AssertIsOnOwningThread();
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDatabase)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBMutableFile,
                                                DOMEventTargetHelper)
  tmp->AssertIsOnOwningThread();

  // Don't unlink mDatabase!
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

JSObject*
IDBMutableFile::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return IDBMutableFileBinding::Wrap(aCx, this, aGivenProto);
}

const nsString&
IDBMutableFile::Name() const
{
  AssertIsOnOwningThread();

  return mName;
}

const nsString&
IDBMutableFile::Type() const
{
  AssertIsOnOwningThread();

  return mType;
}

bool
IDBMutableFile::IsInvalidated()
{
  AssertIsOnOwningThread();

  return mInvalidated;
}

already_AddRefed<File>
IDBMutableFile::CreateFileFor(BlobImpl* aBlobImpl,
                              FileHandleBase* aFileHandle)
{
  AssertIsOnOwningThread();

  nsRefPtr<BlobImpl> blobImplSnapshot =
    new BlobImplSnapshot(aBlobImpl, static_cast<IDBFileHandle*>(aFileHandle));

  nsRefPtr<File> file = File::Create(GetOwner(), blobImplSnapshot);
  return file.forget();
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
