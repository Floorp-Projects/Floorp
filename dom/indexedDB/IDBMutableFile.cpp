/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBMutableFile.h"

#include "ActorsChild.h"
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
#include "mozilla/dom/indexedDB/PBackgroundIDBSharedTypes.h"
#include "mozilla/dom/quota/FileStreams.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "ReportInternalError.h"

namespace mozilla {
namespace dom {

using namespace mozilla::dom::indexedDB;
using namespace mozilla::dom::quota;

IDBMutableFile::IDBMutableFile(IDBDatabase* aDatabase,
                               BackgroundMutableFileChild* aActor,
                               const nsAString& aName, const nsAString& aType)
    : DOMEventTargetHelper(aDatabase),
      mDatabase(aDatabase),
      mBackgroundActor(aActor),
      mName(aName),
      mType(aType),
      mInvalidated(false) {
  MOZ_ASSERT(aDatabase);
  aDatabase->AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);

  mDatabase->NoteLiveMutableFile(this);
}

IDBMutableFile::~IDBMutableFile() {
  AssertIsOnOwningThread();

  mDatabase->NoteFinishedMutableFile(this);

  if (mBackgroundActor) {
    mBackgroundActor->SendDeleteMeInternal();
    MOZ_ASSERT(!mBackgroundActor, "SendDeleteMeInternal should have cleared!");
  }
}

#ifdef DEBUG

void IDBMutableFile::AssertIsOnOwningThread() const {
  MOZ_ASSERT(mDatabase);
  mDatabase->AssertIsOnOwningThread();
}

#endif  // DEBUG

int64_t IDBMutableFile::GetFileId() const {
  AssertIsOnOwningThread();

  int64_t fileId;
  if (!mBackgroundActor ||
      NS_WARN_IF(!mBackgroundActor->SendGetFileId(&fileId))) {
    return -1;
  }

  return fileId;
}

void IDBMutableFile::Invalidate() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mInvalidated);

  mInvalidated = true;

  AbortFileHandles();
}

void IDBMutableFile::RegisterFileHandle(IDBFileHandle* aFileHandle) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFileHandle);
  aFileHandle->AssertIsOnOwningThread();
  MOZ_ASSERT(!mFileHandles.Contains(aFileHandle));

  mFileHandles.PutEntry(aFileHandle);
}

void IDBMutableFile::UnregisterFileHandle(IDBFileHandle* aFileHandle) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFileHandle);
  aFileHandle->AssertIsOnOwningThread();
  MOZ_ASSERT(mFileHandles.Contains(aFileHandle));

  mFileHandles.RemoveEntry(aFileHandle);
}

void IDBMutableFile::AbortFileHandles() {
  AssertIsOnOwningThread();

  if (!mFileHandles.Count()) {
    return;
  }

  nsTArray<RefPtr<IDBFileHandle>> fileHandlesToAbort;
  fileHandlesToAbort.SetCapacity(mFileHandles.Count());

  for (const auto& entry : mFileHandles) {
    IDBFileHandle* const fileHandle = entry.GetKey();
    MOZ_ASSERT(fileHandle);

    fileHandle->AssertIsOnOwningThread();

    if (!fileHandle->IsDone()) {
      fileHandlesToAbort.AppendElement(fileHandle);
    }
  }
  MOZ_ASSERT(fileHandlesToAbort.Length() <= mFileHandles.Count());

  if (fileHandlesToAbort.IsEmpty()) {
    return;
  }

  for (const auto& fileHandle : fileHandlesToAbort) {
    MOZ_ASSERT(fileHandle);

    fileHandle->Abort();
  }
}

IDBDatabase* IDBMutableFile::Database() const {
  AssertIsOnOwningThread();

  return mDatabase;
}

RefPtr<IDBFileHandle> IDBMutableFile::Open(FileMode aMode,
                                           ErrorResult& aError) {
  AssertIsOnOwningThread();

  if (QuotaManager::IsShuttingDown() || mDatabase->IsClosed() || !GetOwner()) {
    aError.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  auto fileHandle = IDBFileHandle::Create(this, aMode);
  if (NS_WARN_IF(!fileHandle)) {
    aError.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  BackgroundFileHandleChild* actor = new BackgroundFileHandleChild(fileHandle);

  MOZ_ALWAYS_TRUE(
      mBackgroundActor->SendPBackgroundFileHandleConstructor(actor, aMode));

  fileHandle->SetBackgroundActor(actor);

  return fileHandle;
}

NS_IMPL_ADDREF_INHERITED(IDBMutableFile, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBMutableFile, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBMutableFile)
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

JSObject* IDBMutableFile::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return IDBMutableFile_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
