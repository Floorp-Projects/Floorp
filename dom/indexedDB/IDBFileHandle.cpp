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
#include "mozilla/dom/IDBFileHandleBinding.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/PBackgroundFileHandle.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "nsContentUtils.h"
#include "nsQueryObject.h"
#include "nsServiceManagerUtils.h"
#include "nsWidgetsCID.h"

namespace mozilla {
namespace dom {

using namespace mozilla::dom::indexedDB;
using namespace mozilla::ipc;

namespace {

already_AddRefed<IDBFileRequest>
GenerateFileRequest(IDBFileHandle* aFileHandle)
{
  MOZ_ASSERT(aFileHandle);
  aFileHandle->AssertIsOnOwningThread();

  RefPtr<IDBFileRequest> fileRequest =
    IDBFileRequest::Create(aFileHandle, /* aWrapAsDOMRequest */ false);
  MOZ_ASSERT(fileRequest);

  return fileRequest.forget();
}

} // namespace

IDBFileHandle::IDBFileHandle(IDBMutableFile* aMutableFile,
                             FileMode aMode)
  : mMutableFile(aMutableFile)
  , mBackgroundActor(nullptr)
  , mLocation(0)
  , mPendingRequestCount(0)
  , mReadyState(INITIAL)
  , mMode(aMode)
  , mAborted(false)
  , mCreating(false)
#ifdef DEBUG
  , mSentFinishOrAbort(false)
  , mFiredCompleteOrAbort(false)
#endif
{
  MOZ_ASSERT(aMutableFile);
  aMutableFile->AssertIsOnOwningThread();
}

IDBFileHandle::~IDBFileHandle()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mPendingRequestCount);
  MOZ_ASSERT(!mCreating);
  MOZ_ASSERT(mSentFinishOrAbort);
  MOZ_ASSERT_IF(mBackgroundActor, mFiredCompleteOrAbort);

  mMutableFile->UnregisterFileHandle(this);

  if (mBackgroundActor) {
    mBackgroundActor->SendDeleteMeInternal();

    MOZ_ASSERT(!mBackgroundActor, "SendDeleteMeInternal should have cleared!");
  }
}

// static
already_AddRefed<IDBFileHandle>
IDBFileHandle::Create(IDBMutableFile* aMutableFile,
                      FileMode aMode)
{
  MOZ_ASSERT(aMutableFile);
  aMutableFile->AssertIsOnOwningThread();
  MOZ_ASSERT(aMode == FileMode::Readonly || aMode == FileMode::Readwrite);

  RefPtr<IDBFileHandle> fileHandle =
    new IDBFileHandle(aMutableFile, aMode);

  fileHandle->BindToOwner(aMutableFile);

  // XXX Fix!
  MOZ_ASSERT(NS_IsMainThread(), "This won't work on non-main threads!");

  nsCOMPtr<nsIRunnable> runnable = do_QueryObject(fileHandle);
  nsContentUtils::RunInMetastableState(runnable.forget());

  fileHandle->mCreating = true;

  aMutableFile->RegisterFileHandle(fileHandle);

  return fileHandle.forget();
}

// static
IDBFileHandle*
IDBFileHandle::GetCurrent()
{
  MOZ_ASSERT(BackgroundChild::GetForCurrentThread());

  BackgroundChildImpl::ThreadLocal* threadLocal =
    BackgroundChildImpl::GetThreadLocalForCurrentThread();
  MOZ_ASSERT(threadLocal);

  return threadLocal->mCurrentFileHandle;
}

#ifdef DEBUG

void
IDBFileHandle::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(mMutableFile);
  mMutableFile->AssertIsOnOwningThread();
}

#endif // DEBUG

void
IDBFileHandle::SetBackgroundActor(BackgroundFileHandleChild* aActor)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!mBackgroundActor);

  mBackgroundActor = aActor;
}

void
IDBFileHandle::StartRequest(IDBFileRequest* aFileRequest,
                            const FileRequestParams& aParams)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFileRequest);
  MOZ_ASSERT(aParams.type() != FileRequestParams::T__None);

  BackgroundFileRequestChild* actor =
    new BackgroundFileRequestChild(aFileRequest);

  mBackgroundActor->SendPBackgroundFileRequestConstructor(actor, aParams);

  // Balanced in BackgroundFileRequestChild::Recv__delete__().
  OnNewRequest();
}

void
IDBFileHandle::OnNewRequest()
{
  AssertIsOnOwningThread();

  if (!mPendingRequestCount) {
    MOZ_ASSERT(mReadyState == INITIAL);
    mReadyState = LOADING;
  }

  ++mPendingRequestCount;
}

void
IDBFileHandle::OnRequestFinished(bool aActorDestroyedNormally)
{
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

void
IDBFileHandle::FireCompleteOrAbortEvents(bool aAborted)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mFiredCompleteOrAbort);

  mReadyState = DONE;

#ifdef DEBUG
  mFiredCompleteOrAbort = true;
#endif

  nsCOMPtr<nsIDOMEvent> event;
  if (aAborted) {
    event = CreateGenericEvent(this, nsDependentString(kAbortEventType),
                               eDoesBubble, eNotCancelable);
  } else {
    event = CreateGenericEvent(this, nsDependentString(kCompleteEventType),
                               eDoesNotBubble, eNotCancelable);
  }
  if (NS_WARN_IF(!event)) {
    return;
  }

  bool dummy;
  if (NS_FAILED(DispatchEvent(event, &dummy))) {
    NS_WARNING("DispatchEvent failed!");
  }
}

bool
IDBFileHandle::IsOpen() const
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
IDBFileHandle::Abort()
{
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

already_AddRefed<IDBFileRequest>
IDBFileHandle::GetMetadata(const IDBFileMetadataParameters& aParameters,
                           ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  // Common state checking
  if (!CheckState(aRv)) {
    return nullptr;
  }

  // Argument checking for get metadata.
  if (!aParameters.mSize && !aParameters.mLastModified) {
    aRv.ThrowTypeError<MSG_METADATA_NOT_CONFIGURED>();
    return nullptr;
  }

 // Do nothing if the window is closed
  if (!CheckWindow()) {
    return nullptr;
  }

  FileRequestGetMetadataParams params;
  params.size() = aParameters.mSize;
  params.lastModified() = aParameters.mLastModified;

  RefPtr<IDBFileRequest> fileRequest = GenerateFileRequest(this);

  StartRequest(fileRequest, params);

  return fileRequest.forget();
}

already_AddRefed<IDBFileRequest>
IDBFileHandle::Truncate(const Optional<uint64_t>& aSize, ErrorResult& aRv)
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

  RefPtr<IDBFileRequest> fileRequest = GenerateFileRequest(this);

  StartRequest(fileRequest, params);

  if (aSize.WasPassed()) {
    mLocation = aSize.Value();
  }

  return fileRequest.forget();
}

already_AddRefed<IDBFileRequest>
IDBFileHandle::Flush(ErrorResult& aRv)
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

  RefPtr<IDBFileRequest> fileRequest = GenerateFileRequest(this);

  StartRequest(fileRequest, params);

  return fileRequest.forget();
}

void
IDBFileHandle::Abort(ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  // This method is special enough for not using generic state checking methods.

  if (IsFinishingOrDone()) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_NOT_ALLOWED_ERR);
    return;
  }

  Abort();
}

bool
IDBFileHandle::CheckState(ErrorResult& aRv)
{
  if (!IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_INACTIVE_ERR);
    return false;
  }

  return true;
}

bool
IDBFileHandle::CheckStateAndArgumentsForRead(uint64_t aSize, ErrorResult& aRv)
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
    aRv.ThrowTypeError<MSG_INVALID_READ_SIZE>();
    return false;
  }

  return true;
}

bool
IDBFileHandle::CheckStateForWrite(ErrorResult& aRv)
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
IDBFileHandle::CheckStateForWriteOrAppend(bool aAppend, ErrorResult& aRv)
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

bool
IDBFileHandle::CheckWindow()
{
  AssertIsOnOwningThread();

  return GetOwner();
}

already_AddRefed<IDBFileRequest>
IDBFileHandle::Read(uint64_t aSize, bool aHasEncoding,
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

  RefPtr<IDBFileRequest> fileRequest = GenerateFileRequest(this);
  if (aHasEncoding) {
    fileRequest->SetEncoding(aEncoding);
  }

  StartRequest(fileRequest, params);

  mLocation += aSize;

  return fileRequest.forget();
}

already_AddRefed<IDBFileRequest>
IDBFileHandle::WriteOrAppend(
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

already_AddRefed<IDBFileRequest>
IDBFileHandle::WriteOrAppend(const nsAString& aValue,
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

already_AddRefed<IDBFileRequest>
IDBFileHandle::WriteOrAppend(const ArrayBuffer& aValue,
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

already_AddRefed<IDBFileRequest>
IDBFileHandle::WriteOrAppend(const ArrayBufferView& aValue,
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

already_AddRefed<IDBFileRequest>
IDBFileHandle::WriteOrAppend(Blob& aValue,
                             bool aAppend,
                             ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  // State checking for write or append
  if (!CheckStateForWriteOrAppend(aAppend, aRv)) {
    return nullptr;
  }

  ErrorResult error;
  uint64_t dataLength = aValue.GetSize(error);
  if (NS_WARN_IF(error.Failed())) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  if (!dataLength) {
    return nullptr;
  }

  PBackgroundChild* backgroundActor = BackgroundChild::GetForCurrentThread();
  MOZ_ASSERT(backgroundActor);

  IPCBlob ipcBlob;
  nsresult rv =
    IPCBlobUtils::Serialize(aValue.Impl(), backgroundActor, ipcBlob);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  FileRequestBlobData blobData;
  blobData.blob() = ipcBlob;

  // Do nothing if the window is closed
  if (!CheckWindow()) {
    return nullptr;
  }

  return WriteInternal(blobData, dataLength, aAppend, aRv);
}

already_AddRefed<IDBFileRequest>
IDBFileHandle::WriteInternal(const FileRequestData& aData,
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

  RefPtr<IDBFileRequest> fileRequest = GenerateFileRequest(this);
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
IDBFileHandle::SendFinish()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mAborted);
  MOZ_ASSERT(IsFinishingOrDone());
  MOZ_ASSERT(!mSentFinishOrAbort);
  MOZ_ASSERT(!mPendingRequestCount);

  MOZ_ASSERT(mBackgroundActor);
  mBackgroundActor->SendFinish();

#ifdef DEBUG
  mSentFinishOrAbort = true;
#endif
}

void
IDBFileHandle::SendAbort()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mAborted);
  MOZ_ASSERT(IsFinishingOrDone());
  MOZ_ASSERT(!mSentFinishOrAbort);

  MOZ_ASSERT(mBackgroundActor);
  mBackgroundActor->SendAbort();

#ifdef DEBUG
  mSentFinishOrAbort = true;
#endif
}

NS_IMPL_ADDREF_INHERITED(IDBFileHandle, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBFileHandle, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBFileHandle)
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
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMETHODIMP
IDBFileHandle::Run()
{
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

nsresult
IDBFileHandle::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  AssertIsOnOwningThread();

  aVisitor.mCanHandle = true;
  aVisitor.mParentTarget = mMutableFile;
  return NS_OK;
}

// virtual
JSObject*
IDBFileHandle::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  AssertIsOnOwningThread();

  return IDBFileHandleBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
