/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsParent.h"

#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/FixedBufferOutputStream.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/indexedDB/ActorsParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBDatabaseParent.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIEventTarget.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIRunnable.h"
#include "nsISeekableStream.h"
#include "nsIThread.h"
#include "nsIThreadPool.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsTArray.h"
#include "nsThreadPool.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCIDInternal.h"

namespace mozilla::dom {

using namespace mozilla::dom::quota;
using namespace mozilla::ipc;

namespace {

/******************************************************************************
 * Constants
 ******************************************************************************/

const uint32_t kThreadLimit = 5;
const uint32_t kIdleThreadLimit = 1;
const uint32_t kIdleThreadTimeoutMs = 30000;

}  // namespace

/******************************************************************************
 * Actor class declarations
 ******************************************************************************/
/*******************************************************************************
 * BackgroundMutableFileParentBase
 ******************************************************************************/

BackgroundMutableFileParentBase::BackgroundMutableFileParentBase(
    FileHandleStorage aStorage, const nsACString& aDirectoryId,
    const nsAString& aFileName, nsIFile* aFile)
    : mDirectoryId(aDirectoryId),
      mFileName(aFileName),
      mStorage(aStorage),
      mInvalidated(false),
      mActorWasAlive(false),
      mActorDestroyed(false),
      mFile(aFile) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aStorage != FILE_HANDLE_STORAGE_MAX);
  MOZ_ASSERT(!aDirectoryId.IsEmpty());
  MOZ_ASSERT(!aFileName.IsEmpty());
  MOZ_ASSERT(aFile);
}

BackgroundMutableFileParentBase::~BackgroundMutableFileParentBase() {
  MOZ_ASSERT_IF(mActorWasAlive, mActorDestroyed);
}

void BackgroundMutableFileParentBase::Invalidate() {
  AssertIsOnBackgroundThread();

  if (mInvalidated) {
    return;
  }

  mInvalidated = true;
}

void BackgroundMutableFileParentBase::SetActorAlive() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorWasAlive);
  MOZ_ASSERT(!mActorDestroyed);

  mActorWasAlive = true;

  // This reference will be absorbed by IPDL and released when the actor is
  // destroyed.
  AddRef();
}

already_AddRefed<nsISupports> BackgroundMutableFileParentBase::CreateStream(
    bool aReadOnly) {
  AssertIsOnBackgroundThread();

  nsresult rv;

  if (aReadOnly) {
    nsCOMPtr<nsIInputStream> stream;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), mFile, -1, -1,
                                    nsIFileInputStream::DEFER_OPEN);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
    return stream.forget();
  }

  nsCOMPtr<nsIRandomAccessStream> stream;
  rv = NS_NewLocalFileRandomAccessStream(getter_AddRefs(stream), mFile, -1, -1,
                                         nsIFileRandomAccessStream::DEFER_OPEN);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }
  return stream.forget();
}

void BackgroundMutableFileParentBase::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  if (!IsInvalidated()) {
    Invalidate();
  }
}

mozilla::ipc::IPCResult BackgroundMutableFileParentBase::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  IProtocol* mgr = Manager();
  if (!PBackgroundMutableFileParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BackgroundMutableFileParentBase::RecvGetFileId(
    int64_t* aFileId) {
  AssertIsOnBackgroundThread();

  *aFileId = -1;
  return IPC_OK();
}

}  // namespace mozilla::dom
