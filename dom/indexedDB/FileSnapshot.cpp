/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSnapshot.h"

#include "IDBDatabase.h"
#include "IDBFileHandle.h"
#include "IDBMutableFile.h"
#include "mozilla/Assertions.h"
#include "nsIAsyncInputStream.h"
#include "nsICloneableInputStream.h"
#include "nsIIPCSerializableInputStream.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

using namespace mozilla::ipc;

namespace {

class StreamWrapper final
  : public nsIAsyncInputStream
  , public nsIInputStreamCallback
  , public nsICloneableInputStream
  , public nsIIPCSerializableInputStream
{
  class CloseRunnable;

  nsCOMPtr<nsIEventTarget> mOwningThread;
  nsCOMPtr<nsIInputStream> mInputStream;
  RefPtr<IDBFileHandle> mFileHandle;
  bool mFinished;

  // This is needed to call OnInputStreamReady() with the correct inputStream.
  nsCOMPtr<nsIInputStreamCallback> mAsyncWaitCallback;

public:
  StreamWrapper(nsIInputStream* aInputStream,
                IDBFileHandle* aFileHandle)
    : mOwningThread(aFileHandle->GetMutableFile()->Database()->EventTarget())
    , mInputStream(aInputStream)
    , mFileHandle(aFileHandle)
    , mFinished(false)
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(aInputStream);
    MOZ_ASSERT(aFileHandle);
    aFileHandle->AssertIsOnOwningThread();

    mFileHandle->OnNewRequest();
  }

private:
  virtual ~StreamWrapper();

  bool
  IsOnOwningThread() const
  {
    MOZ_ASSERT(mOwningThread);

    bool current;
    return NS_SUCCEEDED(mOwningThread->
                        IsOnCurrentThread(&current)) && current;
  }

  void
  AssertIsOnOwningThread() const
  {
    MOZ_ASSERT(IsOnOwningThread());
  }

  void
  Finish()
  {
    AssertIsOnOwningThread();

    if (mFinished) {
      return;
    }

    mFinished = true;

    mFileHandle->OnRequestFinished(/* aActorDestroyedNormally */ true);
  }

  void
  Destroy()
  {
    if (IsOnOwningThread()) {
      delete this;
      return;
    }

    RefPtr<Runnable> destroyRunnable =
      NewNonOwningRunnableMethod("StreamWrapper::Destroy",
                                 this,
                                 &StreamWrapper::Destroy);

    MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(destroyRunnable,
                                                NS_DISPATCH_NORMAL));
  }

  bool
  IsCloneableInputStream() const
  {
    nsCOMPtr<nsICloneableInputStream> stream =
      do_QueryInterface(mInputStream);
    return !!stream;
  }

  bool
  IsIPCSerializableInputStream() const
  {
    nsCOMPtr<nsIIPCSerializableInputStream> stream =
      do_QueryInterface(mInputStream);
    return !!stream;
  }

  bool
  IsAsyncInputStream() const
  {
    nsCOMPtr<nsIAsyncInputStream> stream = do_QueryInterface(mInputStream);
    return !!stream;
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSICLONEABLEINPUTSTREAM
  NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM
};

class StreamWrapper::CloseRunnable final
  : public Runnable
{
  friend class StreamWrapper;

  RefPtr<StreamWrapper> mStreamWrapper;

public:
  NS_DECL_ISUPPORTS_INHERITED

private:
  explicit
  CloseRunnable(StreamWrapper* aStreamWrapper)
    : Runnable("StreamWrapper::CloseRunnable")
    , mStreamWrapper(aStreamWrapper)
  { }

  ~CloseRunnable()
  { }

  NS_IMETHOD
  Run() override;
};

} // anonymous namespace

BlobImplSnapshot::BlobImplSnapshot(BlobImpl* aFileImpl,
                                   IDBFileHandle* aFileHandle)
  : mBlobImpl(aFileImpl)
{
  MOZ_ASSERT(aFileImpl);
  MOZ_ASSERT(aFileHandle);

  mFileHandle =
    do_GetWeakReference(NS_ISUPPORTS_CAST(EventTarget*, aFileHandle));
}

BlobImplSnapshot::BlobImplSnapshot(BlobImpl* aFileImpl,
                                   nsIWeakReference* aFileHandle)
  : mBlobImpl(aFileImpl)
  , mFileHandle(aFileHandle)
{
  MOZ_ASSERT(aFileImpl);
  MOZ_ASSERT(aFileHandle);
}

BlobImplSnapshot::~BlobImplSnapshot()
{
}

NS_IMPL_ISUPPORTS_INHERITED(BlobImplSnapshot, BlobImpl, PIBlobImplSnapshot)

already_AddRefed<BlobImpl>
BlobImplSnapshot::CreateSlice(uint64_t aStart,
                              uint64_t aLength,
                              const nsAString& aContentType,
                              ErrorResult& aRv)
{
  RefPtr<BlobImpl> blobImpl =
    mBlobImpl->CreateSlice(aStart, aLength, aContentType, aRv);

  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  blobImpl = new BlobImplSnapshot(blobImpl, mFileHandle);
  return blobImpl.forget();
}

void
BlobImplSnapshot::GetInternalStream(nsIInputStream** aStream, ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> et = do_QueryReferent(mFileHandle);
  RefPtr<IDBFileHandle> fileHandle = static_cast<IDBFileHandle*>(et.get());
  if (!fileHandle || !fileHandle->IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_INACTIVE_ERR);
    return;
  }

  nsCOMPtr<nsIInputStream> stream;
  mBlobImpl->GetInternalStream(getter_AddRefs(stream), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  RefPtr<StreamWrapper> wrapper = new StreamWrapper(stream, fileHandle);

  wrapper.forget(aStream);
}

BlobImpl*
BlobImplSnapshot::GetBlobImpl() const
{
  nsCOMPtr<EventTarget> et = do_QueryReferent(mFileHandle);
  RefPtr<IDBFileHandle> fileHandle = static_cast<IDBFileHandle*>(et.get());
  if (!fileHandle || !fileHandle->IsOpen()) {
    return nullptr;
  }

  return mBlobImpl;
}

StreamWrapper::~StreamWrapper()
{
  AssertIsOnOwningThread();

  Finish();
}

NS_IMPL_ADDREF(StreamWrapper)
NS_IMPL_RELEASE_WITH_DESTROY(StreamWrapper, Destroy())

NS_INTERFACE_MAP_BEGIN(StreamWrapper)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAsyncInputStream,
                                     IsAsyncInputStream())
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIInputStreamCallback,
                                     IsAsyncInputStream())
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsICloneableInputStream,
                                     IsCloneableInputStream())
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIIPCSerializableInputStream,
                                     IsIPCSerializableInputStream())
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
StreamWrapper::Close()
{
  RefPtr<CloseRunnable> closeRunnable = new CloseRunnable(this);

  MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(closeRunnable,
                                              NS_DISPATCH_NORMAL));

  return NS_OK;
}

NS_IMETHODIMP
StreamWrapper::Available(uint64_t* _retval)
{
  return mInputStream->Available(_retval);
}

NS_IMETHODIMP
StreamWrapper::Read(char* aBuf, uint32_t aCount, uint32_t* _retval)
{
  return mInputStream->Read(aBuf, aCount, _retval);
}

NS_IMETHODIMP
StreamWrapper::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                            uint32_t aCount, uint32_t* _retval)
{
  return mInputStream->ReadSegments(aWriter, aClosure, aCount, _retval);
}

NS_IMETHODIMP
StreamWrapper::IsNonBlocking(bool* _retval)
{
  return mInputStream->IsNonBlocking(_retval);
}

void
StreamWrapper::Serialize(InputStreamParams& aParams,
                         FileDescriptorArray& aFileDescriptors)
{
  nsCOMPtr<nsIIPCSerializableInputStream> stream =
    do_QueryInterface(mInputStream);

  if (stream) {
    stream->Serialize(aParams, aFileDescriptors);
  }
}

bool
StreamWrapper::Deserialize(const InputStreamParams& aParams,
                           const FileDescriptorArray& aFileDescriptors)
{
  MOZ_CRASH("This method should never be called");
  return false;
}

Maybe<uint64_t>
StreamWrapper::ExpectedSerializedLength()
{
  nsCOMPtr<nsIIPCSerializableInputStream> stream =
    do_QueryInterface(mInputStream);

  if (stream) {
    return stream->ExpectedSerializedLength();
  }
  return Nothing();
}

NS_IMETHODIMP
StreamWrapper::CloseWithStatus(nsresult aStatus)
{
  nsCOMPtr<nsIAsyncInputStream> stream = do_QueryInterface(mInputStream);
  if (!stream) {
    return NS_ERROR_NO_INTERFACE;
  }

  nsresult rv = stream->CloseWithStatus(aStatus);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return Close();
}

NS_IMETHODIMP
StreamWrapper::AsyncWait(nsIInputStreamCallback* aCallback,
                         uint32_t aFlags,
                         uint32_t aRequestedCount,
                         nsIEventTarget* aEventTarget)
{
  nsCOMPtr<nsIAsyncInputStream> stream = do_QueryInterface(mInputStream);
  if (!stream) {
    return NS_ERROR_NO_INTERFACE;
  }

  if (mAsyncWaitCallback && aCallback) {
    return NS_ERROR_FAILURE;
  }

  mAsyncWaitCallback = aCallback;

  if (!mAsyncWaitCallback) {
    return NS_OK;
  }

  return stream->AsyncWait(this, aFlags, aRequestedCount, aEventTarget);
}

// nsIInputStreamCallback

NS_IMETHODIMP
StreamWrapper::OnInputStreamReady(nsIAsyncInputStream* aStream)
{
  nsCOMPtr<nsIAsyncInputStream> stream = do_QueryInterface(mInputStream);
  if (!stream) {
    return NS_ERROR_NO_INTERFACE;
  }

  // We have been canceled in the meanwhile.
  if (!mAsyncWaitCallback) {
    return NS_OK;
  }

  nsCOMPtr<nsIInputStreamCallback> callback;
  callback.swap(mAsyncWaitCallback);

  return callback->OnInputStreamReady(this);
}

// nsICloneableInputStream

NS_IMETHODIMP
StreamWrapper::GetCloneable(bool* aCloneable)
{
  nsCOMPtr<nsICloneableInputStream> stream = do_QueryInterface(mInputStream);
  if (!stream) {
    *aCloneable = false;
    return NS_ERROR_NO_INTERFACE;
  }

  return stream->GetCloneable(aCloneable);
}

NS_IMETHODIMP
StreamWrapper::Clone(nsIInputStream** aResult)
{
  nsCOMPtr<nsICloneableInputStream> stream = do_QueryInterface(mInputStream);
  if (!stream) {
    return NS_ERROR_NO_INTERFACE;
  }

  return stream->Clone(aResult);
}

NS_IMPL_ISUPPORTS_INHERITED0(StreamWrapper::CloseRunnable,
                             Runnable)

NS_IMETHODIMP
StreamWrapper::
CloseRunnable::Run()
{
  mStreamWrapper->Finish();

  return NS_OK;
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
