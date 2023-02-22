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
#include "mozilla/dom/indexedDB/PBackgroundIDBSharedTypes.h"
#include "mozilla/dom/quota/FileStreams.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "ReportInternalError.h"

namespace mozilla::dom {

using namespace mozilla::dom::indexedDB;
using namespace mozilla::dom::quota;

IDBMutableFile::IDBMutableFile(IDBDatabase* aDatabase,
                               const nsAString& aName, const nsAString& aType)
    : DOMEventTargetHelper(aDatabase),
      mDatabase(aDatabase),
      mName(aName),
      mType(aType),
      mInvalidated(false) {
  MOZ_ASSERT(aDatabase);
  aDatabase->AssertIsOnOwningThread();

  mDatabase->NoteLiveMutableFile(*this);
}

IDBMutableFile::~IDBMutableFile() {
  AssertIsOnOwningThread();

  mDatabase->NoteFinishedMutableFile(*this);
}

#ifdef DEBUG

void IDBMutableFile::AssertIsOnOwningThread() const {
  MOZ_ASSERT(mDatabase);
  mDatabase->AssertIsOnOwningThread();
}

#endif  // DEBUG

int64_t IDBMutableFile::GetFileId() const {
  AssertIsOnOwningThread();

  return -1;
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

  mFileHandles.Insert(aFileHandle);
}

void IDBMutableFile::UnregisterFileHandle(IDBFileHandle* aFileHandle) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFileHandle);
  aFileHandle->AssertIsOnOwningThread();
  MOZ_ASSERT(mFileHandles.Contains(aFileHandle));

  mFileHandles.Remove(aFileHandle);
}

void IDBMutableFile::AbortFileHandles() {
  AssertIsOnOwningThread();

  if (!mFileHandles.Count()) {
    return;
  }

  nsTArray<RefPtr<IDBFileHandle>> fileHandlesToAbort;
  fileHandlesToAbort.SetCapacity(mFileHandles.Count());

  for (IDBFileHandle* const fileHandle : mFileHandles) {
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

}  // namespace mozilla::dom
