/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBFileHandle.h"

#include "ActorsChild.h"
#include "BackgroundChildImpl.h"
#include "IDBEvents.h"
#include "IDBMutableFile.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "nsContentUtils.h"
#include "nsQueryObject.h"
#include "nsServiceManagerUtils.h"
#include "nsWidgetsCID.h"

namespace mozilla::dom {

using namespace mozilla::dom::indexedDB;
using namespace mozilla::ipc;

IDBFileHandle::IDBFileHandle(IDBMutableFile* aMutableFile, FileMode aMode)
    : DOMEventTargetHelper(aMutableFile),
      mMutableFile(aMutableFile),
      mLocation(0),
      mPendingRequestCount(0),
      mReadyState(INITIAL),
      mMode(aMode),
      mAborted(false),
      mCreating(false)
#ifdef DEBUG
      ,
      mSentFinishOrAbort(false),
      mFiredCompleteOrAbort(false)
#endif
{
  MOZ_ASSERT(aMutableFile);
  aMutableFile->AssertIsOnOwningThread();
}

IDBFileHandle::~IDBFileHandle() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mPendingRequestCount);
  MOZ_ASSERT(!mCreating);
  MOZ_ASSERT(mSentFinishOrAbort);

  mMutableFile->UnregisterFileHandle(this);
}

// static
RefPtr<IDBFileHandle> IDBFileHandle::Create(IDBMutableFile* aMutableFile,
                                            FileMode aMode) {
  MOZ_ASSERT(aMutableFile);
  aMutableFile->AssertIsOnOwningThread();
  MOZ_ASSERT(aMode == FileMode::Readonly || aMode == FileMode::Readwrite);

  RefPtr<IDBFileHandle> fileHandle = new IDBFileHandle(aMutableFile, aMode);

  // XXX Fix!
  MOZ_ASSERT(NS_IsMainThread(), "This won't work on non-main threads!");

  nsCOMPtr<nsIRunnable> runnable = do_QueryObject(fileHandle);
  nsContentUtils::AddPendingIDBTransaction(runnable.forget());

  fileHandle->mCreating = true;

  aMutableFile->RegisterFileHandle(fileHandle);

  return fileHandle;
}

// static
IDBFileHandle* IDBFileHandle::GetCurrent() {
  MOZ_ASSERT(BackgroundChild::GetForCurrentThread());

  BackgroundChildImpl::ThreadLocal* threadLocal =
      BackgroundChildImpl::GetThreadLocalForCurrentThread();
  MOZ_ASSERT(threadLocal);

  return threadLocal->mCurrentFileHandle;
}

#ifdef DEBUG

void IDBFileHandle::AssertIsOnOwningThread() const {
  MOZ_ASSERT(mMutableFile);
  mMutableFile->AssertIsOnOwningThread();
}

#endif  // DEBUG

void IDBFileHandle::OnRequestFinished(bool aActorDestroyedNormally) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mPendingRequestCount);

  --mPendingRequestCount;

  if (!mPendingRequestCount && !mMutableFile->IsInvalidated()) {
    mReadyState = FINISHING;

    if (aActorDestroyedNormally) {
      if (!mAborted) {
        SendFinish();
      } else {
        SendAbort();
      }
    } else {
      // Don't try to send any more messages to the parent if the request actor
      // was killed.
#ifdef DEBUG
      MOZ_ASSERT(!mSentFinishOrAbort);
      mSentFinishOrAbort = true;
#endif
    }
  }
}

void IDBFileHandle::FireCompleteOrAbortEvents(bool aAborted) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mFiredCompleteOrAbort);

  mReadyState = DONE;

#ifdef DEBUG
  mFiredCompleteOrAbort = true;
#endif

  // TODO: Why is it necessary to create the Event on the heap at all?
  const auto event = CreateGenericEvent(
      this,
      aAborted ? nsDependentString(kAbortEventType)
               : nsDependentString(kCompleteEventType),
      aAborted ? eDoesBubble : eDoesNotBubble, eNotCancelable);
  MOZ_ASSERT(event);

  IgnoredErrorResult rv;
  DispatchEvent(*event, rv);
  if (rv.Failed()) {
    NS_WARNING("DispatchEvent failed!");
  }
}

bool IDBFileHandle::IsOpen() const {
  AssertIsOnOwningThread();

  // If we haven't started anything then we're open.
  if (mReadyState == INITIAL) {
    return true;
  }

  // If we've already started then we need to check to see if we still have the
  // mCreating flag set. If we do (i.e. we haven't returned to the event loop
  // from the time we were created) then we are open. Otherwise check the
  // currently running file handles to see if it's the same. We only allow other
  // requests to be made if this file handle is currently running.
  if (mReadyState == LOADING && (mCreating || GetCurrent() == this)) {
    return true;
  }

  return false;
}

void IDBFileHandle::Abort() {
  AssertIsOnOwningThread();

  if (IsFinishingOrDone()) {
    // Already started (and maybe finished) the finish or abort so there is
    // nothing to do here.
    return;
  }

  const bool isInvalidated = mMutableFile->IsInvalidated();
  bool needToSendAbort = mReadyState == INITIAL && !isInvalidated;

#ifdef DEBUG
  if (isInvalidated) {
    mSentFinishOrAbort = true;
  }
#endif

  mAborted = true;
  mReadyState = DONE;

  // Fire the abort event if there are no outstanding requests. Otherwise the
  // abort event will be fired when all outstanding requests finish.
  if (needToSendAbort) {
    SendAbort();
  }
}

void IDBFileHandle::SendFinish() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mAborted);
  MOZ_ASSERT(IsFinishingOrDone());
  MOZ_ASSERT(!mSentFinishOrAbort);
  MOZ_ASSERT(!mPendingRequestCount);

#ifdef DEBUG
  mSentFinishOrAbort = true;
#endif
}

void IDBFileHandle::SendAbort() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mAborted);
  MOZ_ASSERT(IsFinishingOrDone());
  MOZ_ASSERT(!mSentFinishOrAbort);

#ifdef DEBUG
  mSentFinishOrAbort = true;
#endif
}

NS_IMPL_ADDREF_INHERITED(IDBFileHandle, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBFileHandle, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBFileHandle)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBFileHandle)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBFileHandle,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMutableFile)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBFileHandle,
                                                DOMEventTargetHelper)
  // Don't unlink mMutableFile!
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMETHODIMP
IDBFileHandle::Run() {
  AssertIsOnOwningThread();

  // We're back at the event loop, no longer newborn.
  mCreating = false;

  // Maybe finish if there were no requests generated.
  if (mReadyState == INITIAL) {
    mReadyState = DONE;

    SendFinish();
  }

  return NS_OK;
}

void IDBFileHandle::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  AssertIsOnOwningThread();

  aVisitor.mCanHandle = true;
  aVisitor.SetParentTarget(mMutableFile, false);
}

}  // namespace mozilla::dom
