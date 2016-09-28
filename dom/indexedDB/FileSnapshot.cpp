/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSnapshot.h"

#include "IDBFileHandle.h"
#include "mozilla/Assertions.h"
#include "nsIIPCSerializableInputStream.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

using namespace mozilla::ipc;

namespace {

class StreamWrapper final
  : public nsIInputStream
  , public nsIIPCSerializableInputStream
{
  class CloseRunnable;

  nsCOMPtr<nsIEventTarget> mOwningThread;
  nsCOMPtr<nsIInputStream> mInputStream;
  RefPtr<IDBFileHandle> mFileHandle;
  bool mFinished;

public:
  StreamWrapper(nsIInputStream* aInputStream,
                IDBFileHandle* aFileHandle)
    : mOwningThread(NS_GetCurrentThread())
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

    nsCOMPtr<nsIRunnable> destroyRunnable =
      NewNonOwningRunnableMethod(this, &StreamWrapper::Destroy);

    MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(destroyRunnable,
                                                NS_DISPATCH_NORMAL));
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
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
    : mStreamWrapper(aStreamWrapper)
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
NS_IMPL_QUERY_INTERFACE(StreamWrapper,
                        nsIInputStream,
                        nsIIPCSerializableInputStream)

NS_IMETHODIMP
StreamWrapper::Close()
{
  MOZ_ASSERT(!IsOnOwningThread());

  RefPtr<CloseRunnable> closeRunnable = new CloseRunnable(this);

  MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(closeRunnable,
                                              NS_DISPATCH_NORMAL));

  return NS_OK;
}

NS_IMETHODIMP
StreamWrapper::Available(uint64_t* _retval)
{
  // Can't assert here, this method is sometimes called on the owning thread
  // (nsInputStreamChannel::OpenContentStream calls Available before setting
  // the content length property).

  return mInputStream->Available(_retval);
}

NS_IMETHODIMP
StreamWrapper::Read(char* aBuf, uint32_t aCount, uint32_t* _retval)
{
  MOZ_ASSERT(!IsOnOwningThread());
  return mInputStream->Read(aBuf, aCount, _retval);
}

NS_IMETHODIMP
StreamWrapper::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                            uint32_t aCount, uint32_t* _retval)
{
  MOZ_ASSERT(!IsOnOwningThread());
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
  nsCOMPtr<nsIIPCSerializableInputStream> stream =
    do_QueryInterface(mInputStream);

  if (stream) {
    return stream->Deserialize(aParams, aFileDescriptors);
  }

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
