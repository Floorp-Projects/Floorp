/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileHandleBase.h"

#include "ActorsChild.h"
#include "BackgroundChildImpl.h"
#include "FileRequestBase.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/PBackgroundFileHandle.h"
#include "mozilla/dom/UnionConversions.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "MutableFileBase.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

using namespace mozilla::ipc;

FileHandleBase::FileHandleBase(DEBUGONLY(PRThread* aOwningThread,)
                               FileMode aMode)
  : RefCountedThreadObject(DEBUGONLY(aOwningThread))
  , mBackgroundActor(nullptr)
  , mLocation(0)
  , mPendingRequestCount(0)
  , mReadyState(INITIAL)
  , mMode(aMode)
  , mAborted(false)
  , mCreating(false)
  DEBUGONLY(, mSentFinishOrAbort(false))
  DEBUGONLY(, mFiredCompleteOrAbort(false))
{
  AssertIsOnOwningThread();
}

FileHandleBase::~FileHandleBase()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mPendingRequestCount);
  MOZ_ASSERT(!mCreating);
  MOZ_ASSERT(mSentFinishOrAbort);
  MOZ_ASSERT_IF(mBackgroundActor, mFiredCompleteOrAbort);

  if (mBackgroundActor) {
    mBackgroundActor->SendDeleteMeInternal();

    MOZ_ASSERT(!mBackgroundActor, "SendDeleteMeInternal should have cleared!");
  }
}

// static
FileHandleBase*
FileHandleBase::GetCurrent()
{
  MOZ_ASSERT(BackgroundChild::GetForCurrentThread());

  BackgroundChildImpl::ThreadLocal* threadLocal =
    BackgroundChildImpl::GetThreadLocalForCurrentThread();
  MOZ_ASSERT(threadLocal);

  return threadLocal->mCurrentFileHandle;
}

void
FileHandleBase::SetBackgroundActor(BackgroundFileHandleChild* aActor)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!mBackgroundActor);

  mBackgroundActor = aActor;
}

void
FileHandleBase::StartRequest(FileRequestBase* aFileRequest,
                             const FileRequestParams& aParams)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFileRequest);
  MOZ_ASSERT(aParams.type() != FileRequestParams::T__None);

  BackgroundFileRequestChild* actor =
    new BackgroundFileRequestChild(DEBUGONLY(mBackgroundActor->OwningThread(),)
                                   aFileRequest);

  mBackgroundActor->SendPBackgroundFileRequestConstructor(actor, aParams);

  // Balanced in BackgroundFileRequestChild::Recv__delete__().
  OnNewRequest();
}

void
FileHandleBase::OnNewRequest()
{
  AssertIsOnOwningThread();

  if (!mPendingRequestCount) {
    MOZ_ASSERT(mReadyState == INITIAL);
    mReadyState = LOADING;
  }

  ++mPendingRequestCount;
}

void
FileHandleBase::OnRequestFinished(bool aActorDestroyedNormally)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mPendingRequestCount);

  --mPendingRequestCount;

  if (!mPendingRequestCount && !MutableFile()->IsInvalidated()) {
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

bool
FileHandleBase::IsOpen() const
{
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

void
FileHandleBase::Abort()
{
  AssertIsOnOwningThread();

  if (IsFinishingOrDone()) {
    // Already started (and maybe finished) the finish or abort so there is
    // nothing to do here.
    return;
  }

  const bool isInvalidated = MutableFile()->IsInvalidated();
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

already_AddRefed<FileRequestBase>
FileHandleBase::Read(uint64_t aSize, bool aHasEncoding,
                     const nsAString& aEncoding, ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  // State and argument checking for read
  if (!CheckStateAndArgumentsForRead(aSize, aRv)) {
    return nullptr;
  }

  // Do nothing if the window is closed
  if (!CheckWindow()) {
    return nullptr;
  }

  FileRequestReadParams params;
  params.offset() = mLocation;
  params.size() = aSize;

  nsRefPtr<FileRequestBase> fileRequest = GenerateFileRequest();
  if (aHasEncoding) {
    fileRequest->SetEncoding(aEncoding);
  }

  StartRequest(fileRequest, params);

  mLocation += aSize;

  return fileRequest.forget();
}

already_AddRefed<FileRequestBase>
FileHandleBase::Truncate(const Optional<uint64_t>& aSize, ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  // State checking for write
  if (!CheckStateForWrite(aRv)) {
    return nullptr;
  }

  // Getting location and additional state checking for truncate
  uint64_t location;
  if (aSize.WasPassed()) {
    // Just in case someone calls us from C++
    MOZ_ASSERT(aSize.Value() != UINT64_MAX, "Passed wrong size!");
    location = aSize.Value();
  } else {
    if (mLocation == UINT64_MAX) {
      aRv.Throw(NS_ERROR_DOM_FILEHANDLE_NOT_ALLOWED_ERR);
      return nullptr;
    }
    location = mLocation;
  }

  // Do nothing if the window is closed
  if (!CheckWindow()) {
    return nullptr;
  }

  FileRequestTruncateParams params;
  params.offset() = location;

  nsRefPtr<FileRequestBase> fileRequest = GenerateFileRequest();

  StartRequest(fileRequest, params);

  if (aSize.WasPassed()) {
    mLocation = aSize.Value();
  }

  return fileRequest.forget();
}

already_AddRefed<FileRequestBase>
FileHandleBase::Flush(ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  // State checking for write
  if (!CheckStateForWrite(aRv)) {
    return nullptr;
  }

  // Do nothing if the window is closed
  if (!CheckWindow()) {
    return nullptr;
  }

  FileRequestFlushParams params;

  nsRefPtr<FileRequestBase> fileRequest = GenerateFileRequest();

  StartRequest(fileRequest, params);

  return fileRequest.forget();
}

void
FileHandleBase::Abort(ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  // This method is special enough for not using generic state checking methods.

  if (IsFinishingOrDone()) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_NOT_ALLOWED_ERR);
    return;
  }

  Abort();
}

void
FileHandleBase::HandleCompleteOrAbort(bool aAborted)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mFiredCompleteOrAbort);

  mReadyState = DONE;

  DEBUGONLY(mFiredCompleteOrAbort = true;)
}

void
FileHandleBase::OnReturnToEventLoop()
{
  AssertIsOnOwningThread();

  // We're back at the event loop, no longer newborn.
  mCreating = false;

  // Maybe finish if there were no requests generated.
  if (mReadyState == INITIAL) {
    mReadyState = DONE;

    SendFinish();
  }
}

bool
FileHandleBase::CheckState(ErrorResult& aRv)
{
  if (!IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_INACTIVE_ERR);
    return false;
  }

  return true;
}

bool
FileHandleBase::CheckStateAndArgumentsForRead(uint64_t aSize, ErrorResult& aRv)
{
  // Common state checking
  if (!CheckState(aRv)) {
    return false;
  }

  // Additional state checking for read
  if (mLocation == UINT64_MAX) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_NOT_ALLOWED_ERR);
    return false;
  }

  // Argument checking for read
  if (!aSize) {
    aRv.ThrowTypeError(MSG_INVALID_READ_SIZE);
    return false;
  }

  return true;
}

bool
FileHandleBase::CheckStateForWrite(ErrorResult& aRv)
{
  // Common state checking
  if (!CheckState(aRv)) {
    return false;
  }

  // Additional state checking for write
  if (mMode != FileMode::Readwrite) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_READ_ONLY_ERR);
    return false;
  }

  return true;
}

bool
FileHandleBase::CheckStateForWriteOrAppend(bool aAppend, ErrorResult& aRv)
{
  // State checking for write
  if (!CheckStateForWrite(aRv)) {
    return false;
  }

  // Additional state checking for write
  if (!aAppend && mLocation == UINT64_MAX) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_NOT_ALLOWED_ERR);
    return false;
  }

  return true;
}

already_AddRefed<FileRequestBase>
FileHandleBase::WriteOrAppend(
                       const StringOrArrayBufferOrArrayBufferViewOrBlob& aValue,
                       bool aAppend,
                       ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  if (aValue.IsString()) {
    return WriteOrAppend(aValue.GetAsString(), aAppend, aRv);
  }

  if (aValue.IsArrayBuffer()) {
    return WriteOrAppend(aValue.GetAsArrayBuffer(), aAppend, aRv);
  }

  if (aValue.IsArrayBufferView()) {
    return WriteOrAppend(aValue.GetAsArrayBufferView(), aAppend, aRv);
  }

  MOZ_ASSERT(aValue.IsBlob());
  return WriteOrAppend(aValue.GetAsBlob(), aAppend, aRv);
}

already_AddRefed<FileRequestBase>
FileHandleBase::WriteOrAppend(const nsAString& aValue,
                              bool aAppend,
                              ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  // State checking for write or append
  if (!CheckStateForWriteOrAppend(aAppend, aRv)) {
    return nullptr;
  }

  NS_ConvertUTF16toUTF8 cstr(aValue);

  uint64_t dataLength = cstr.Length();;
  if (!dataLength) {
    return nullptr;
  }

  FileRequestStringData stringData(cstr);

  // Do nothing if the window is closed
  if (!CheckWindow()) {
    return nullptr;
  }

  return WriteInternal(stringData, dataLength, aAppend, aRv);
}

already_AddRefed<FileRequestBase>
FileHandleBase::WriteOrAppend(const ArrayBuffer& aValue,
                              bool aAppend,
                              ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  // State checking for write or append
  if (!CheckStateForWriteOrAppend(aAppend, aRv)) {
    return nullptr;
  }

  aValue.ComputeLengthAndData();

  uint64_t dataLength = aValue.Length();;
  if (!dataLength) {
    return nullptr;
  }

  const char* data = reinterpret_cast<const char*>(aValue.Data());

  FileRequestStringData stringData;
  if (NS_WARN_IF(!stringData.string().Assign(data, aValue.Length(),
                                             fallible_t()))) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  // Do nothing if the window is closed
  if (!CheckWindow()) {
    return nullptr;
  }

  return WriteInternal(stringData, dataLength, aAppend, aRv);
}

already_AddRefed<FileRequestBase>
FileHandleBase::WriteOrAppend(const ArrayBufferView& aValue,
                              bool aAppend,
                              ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  // State checking for write or append
  if (!CheckStateForWriteOrAppend(aAppend, aRv)) {
    return nullptr;
  }

  aValue.ComputeLengthAndData();

  uint64_t dataLength = aValue.Length();;
  if (!dataLength) {
    return nullptr;
  }

  const char* data = reinterpret_cast<const char*>(aValue.Data());

  FileRequestStringData stringData;
  if (NS_WARN_IF(!stringData.string().Assign(data, aValue.Length(),
                                             fallible_t()))) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  // Do nothing if the window is closed
  if (!CheckWindow()) {
    return nullptr;
  }

  return WriteInternal(stringData, dataLength, aAppend, aRv);
}

already_AddRefed<FileRequestBase>
FileHandleBase::WriteOrAppend(Blob& aValue,
                              bool aAppend,
                              ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  // State checking for write or append
  if (!CheckStateForWriteOrAppend(aAppend, aRv)) {
    return nullptr;
  }

  ErrorResult rv;
  uint64_t dataLength = aValue.GetSize(rv);
  if (NS_WARN_IF(rv.Failed())) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  if (!dataLength) {
    return nullptr;
  }

  PBackgroundChild* backgroundActor = BackgroundChild::GetForCurrentThread();
  MOZ_ASSERT(backgroundActor);

  PBlobChild* blobActor =
    BackgroundChild::GetOrCreateActorForBlob(backgroundActor, &aValue);
  if (NS_WARN_IF(!blobActor)) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  FileRequestBlobData blobData;
  blobData.blobChild() = blobActor;

  // Do nothing if the window is closed
  if (!CheckWindow()) {
    return nullptr;
  }

  return WriteInternal(blobData, dataLength, aAppend, aRv);
}

already_AddRefed<FileRequestBase>
FileHandleBase::WriteInternal(const FileRequestData& aData,
                              uint64_t aDataLength,
                              bool aAppend,
                              ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  DebugOnly<ErrorResult> error;
  MOZ_ASSERT(CheckStateForWrite(error));
  MOZ_ASSERT_IF(!aAppend, mLocation != UINT64_MAX);
  MOZ_ASSERT(aDataLength);
  MOZ_ASSERT(CheckWindow());

  FileRequestWriteParams params;
  params.offset() = aAppend ? UINT64_MAX : mLocation;
  params.data() = aData;
  params.dataLength() = aDataLength;

  nsRefPtr<FileRequestBase> fileRequest = GenerateFileRequest();
  MOZ_ASSERT(fileRequest);

  StartRequest(fileRequest, params);

  if (aAppend) {
    mLocation = UINT64_MAX;
  }
  else {
    mLocation += aDataLength;
  }

  return fileRequest.forget();
}

void
FileHandleBase::SendFinish()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mAborted);
  MOZ_ASSERT(IsFinishingOrDone());
  MOZ_ASSERT(!mSentFinishOrAbort);
  MOZ_ASSERT(!mPendingRequestCount);

  MOZ_ASSERT(mBackgroundActor);
  mBackgroundActor->SendFinish();

  DEBUGONLY(mSentFinishOrAbort = true;)
}

void
FileHandleBase::SendAbort()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mAborted);
  MOZ_ASSERT(IsFinishingOrDone());
  MOZ_ASSERT(!mSentFinishOrAbort);

  MOZ_ASSERT(mBackgroundActor);
  mBackgroundActor->SendAbort();

  DEBUGONLY(mSentFinishOrAbort = true;)
}

} // namespace dom
} // namespace mozilla
