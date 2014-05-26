/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Blob.h"

#include "ContentChild.h"
#include "ContentParent.h"
#include "FileDescriptorSetChild.h"
#include "jsapi.h"
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Monitor.h"
#include "mozilla/unused.h"
#include "mozilla/dom/PBlobStreamChild.h"
#include "mozilla/dom/PBlobStreamParent.h"
#include "mozilla/dom/PFileDescriptorSetParent.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "nsCOMPtr.h"
#include "nsDOMFile.h"
#include "nsIDOMFile.h"
#include "nsIInputStream.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsIMultiplexInputStream.h"
#include "nsIRemoteBlob.h"
#include "nsISeekableStream.h"
#include "nsNetCID.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

#define PRIVATE_REMOTE_INPUT_STREAM_IID \
  {0x30c7699f, 0x51d2, 0x48c8, {0xad, 0x56, 0xc0, 0x16, 0xd7, 0x6f, 0x71, 0x27}}

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace {

enum ActorType
{
  ChildActor,
  ParentActor
};

// Ensure that a nsCOMPtr/nsRefPtr is released on the main thread.
template <template <class> class SmartPtr, class T>
void
ProxyReleaseToMainThread(SmartPtr<T>& aDoomed)
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
  NS_ENSURE_TRUE_VOID(mainThread);

  if (NS_FAILED(NS_ProxyRelease(mainThread, aDoomed, true))) {
    NS_WARNING("Failed to proxy release to main thread!");
  }
}

class NS_NO_VTABLE IPrivateRemoteInputStream : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(PRIVATE_REMOTE_INPUT_STREAM_IID)

  // This will return the underlying stream.
  virtual nsIInputStream*
  BlockAndGetInternalStream() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(IPrivateRemoteInputStream,
                              PRIVATE_REMOTE_INPUT_STREAM_IID)

// This class exists to keep a blob alive at least as long as its internal
// stream.
class BlobInputStreamTether : public nsIMultiplexInputStream,
                              public nsISeekableStream,
                              public nsIIPCSerializableInputStream
{
  nsCOMPtr<nsIInputStream> mStream;
  nsCOMPtr<nsIDOMBlob> mSourceBlob;

  nsIMultiplexInputStream* mWeakMultiplexStream;
  nsISeekableStream* mWeakSeekableStream;
  nsIIPCSerializableInputStream* mWeakSerializableStream;

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_FORWARD_NSIINPUTSTREAM(mStream->)
  NS_FORWARD_SAFE_NSIMULTIPLEXINPUTSTREAM(mWeakMultiplexStream)
  NS_FORWARD_SAFE_NSISEEKABLESTREAM(mWeakSeekableStream)
  NS_FORWARD_SAFE_NSIIPCSERIALIZABLEINPUTSTREAM(mWeakSerializableStream)

  BlobInputStreamTether(nsIInputStream* aStream, nsIDOMBlob* aSourceBlob)
  : mStream(aStream), mSourceBlob(aSourceBlob), mWeakMultiplexStream(nullptr),
    mWeakSeekableStream(nullptr), mWeakSerializableStream(nullptr)
  {
    MOZ_ASSERT(aStream);
    MOZ_ASSERT(aSourceBlob);

    nsCOMPtr<nsIMultiplexInputStream> multiplexStream =
      do_QueryInterface(aStream);
    if (multiplexStream) {
      MOZ_ASSERT(SameCOMIdentity(aStream, multiplexStream));
      mWeakMultiplexStream = multiplexStream;
    }

    nsCOMPtr<nsISeekableStream> seekableStream = do_QueryInterface(aStream);
    if (seekableStream) {
      MOZ_ASSERT(SameCOMIdentity(aStream, seekableStream));
      mWeakSeekableStream = seekableStream;
    }

    nsCOMPtr<nsIIPCSerializableInputStream> serializableStream =
      do_QueryInterface(aStream);
    if (serializableStream) {
      MOZ_ASSERT(SameCOMIdentity(aStream, serializableStream));
      mWeakSerializableStream = serializableStream;
    }
  }

protected:
  virtual ~BlobInputStreamTether()
  {
    MOZ_ASSERT(mStream);
    MOZ_ASSERT(mSourceBlob);

    if (!NS_IsMainThread()) {
      mStream = nullptr;
      ProxyReleaseToMainThread(mSourceBlob);
    }
  }
};

NS_IMPL_ADDREF(BlobInputStreamTether)
NS_IMPL_RELEASE(BlobInputStreamTether)

NS_INTERFACE_MAP_BEGIN(BlobInputStreamTether)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIMultiplexInputStream,
                                     mWeakMultiplexStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISeekableStream, mWeakSeekableStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIIPCSerializableInputStream,
                                     mWeakSerializableStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

class RemoteInputStream : public nsIInputStream,
                          public nsISeekableStream,
                          public nsIIPCSerializableInputStream,
                          public IPrivateRemoteInputStream
{
  mozilla::Monitor mMonitor;
  nsCOMPtr<nsIInputStream> mStream;
  nsCOMPtr<nsIDOMBlob> mSourceBlob;
  nsISeekableStream* mWeakSeekableStream;
  ActorType mOrigin;

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  RemoteInputStream(nsIDOMBlob* aSourceBlob, ActorType aOrigin)
  : mMonitor("RemoteInputStream.mMonitor"), mSourceBlob(aSourceBlob),
    mWeakSeekableStream(nullptr), mOrigin(aOrigin)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aSourceBlob);
  }

  void
  Serialize(InputStreamParams& aParams,
            FileDescriptorArray& /* aFileDescriptors */)
  {
    nsCOMPtr<nsIRemoteBlob> remote = do_QueryInterface(mSourceBlob);
    MOZ_ASSERT(remote);

    if (mOrigin == ParentActor) {
      aParams = RemoteInputStreamParams(
        static_cast<PBlobParent*>(remote->GetPBlob()), nullptr);
    } else {
      MOZ_ASSERT(mOrigin == ChildActor);
      aParams = RemoteInputStreamParams(
        nullptr, static_cast<PBlobChild*>(remote->GetPBlob()));
    }
  }

  bool
  Deserialize(const InputStreamParams& aParams,
              const FileDescriptorArray& /* aFileDescriptors */)
  {
    // See InputStreamUtils.cpp to see how deserialization of a
    // RemoteInputStream is special-cased.
    MOZ_CRASH("RemoteInputStream should never be deserialized");
  }

  void
  SetStream(nsIInputStream* aStream)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aStream);

    nsCOMPtr<nsIInputStream> stream = aStream;
    nsCOMPtr<nsISeekableStream> seekableStream = do_QueryInterface(aStream);

    MOZ_ASSERT_IF(seekableStream, SameCOMIdentity(aStream, seekableStream));

    {
      mozilla::MonitorAutoLock lock(mMonitor);

      MOZ_ASSERT(!mStream);
      MOZ_ASSERT(!mWeakSeekableStream);

      mStream.swap(stream);
      mWeakSeekableStream = seekableStream;

      mMonitor.Notify();
    }
  }

  NS_IMETHOD
  Close() MOZ_OVERRIDE
  {
    nsresult rv = BlockAndWaitForStream();
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMBlob> sourceBlob;
    mSourceBlob.swap(sourceBlob);

    rv = mStream->Close();
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMETHOD
  Available(uint64_t* aAvailable) MOZ_OVERRIDE
  {
    // See large comment in FileInputStreamWrapper::Available.
    if (NS_IsMainThread()) {
      return NS_BASE_STREAM_CLOSED;
    }

    nsresult rv = BlockAndWaitForStream();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStream->Available(aAvailable);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMETHOD
  Read(char* aBuffer, uint32_t aCount, uint32_t* aResult) MOZ_OVERRIDE
  {
    nsresult rv = BlockAndWaitForStream();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStream->Read(aBuffer, aCount, aResult);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMETHOD
  ReadSegments(nsWriteSegmentFun aWriter, void* aClosure, uint32_t aCount,
               uint32_t* aResult) MOZ_OVERRIDE
  {
    nsresult rv = BlockAndWaitForStream();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStream->ReadSegments(aWriter, aClosure, aCount, aResult);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMETHOD
  IsNonBlocking(bool* aNonBlocking) MOZ_OVERRIDE
  {
    NS_ENSURE_ARG_POINTER(aNonBlocking);

    *aNonBlocking = false;
    return NS_OK;
  }

  NS_IMETHOD
  Seek(int32_t aWhence, int64_t aOffset) MOZ_OVERRIDE
  {
    nsresult rv = BlockAndWaitForStream();
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mWeakSeekableStream) {
      NS_WARNING("Underlying blob stream is not seekable!");
      return NS_ERROR_NO_INTERFACE;
    }

    rv = mWeakSeekableStream->Seek(aWhence, aOffset);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMETHOD
  Tell(int64_t* aResult)
  {
    // We can cheat here and assume that we're going to start at 0 if we don't
    // yet have our stream. Though, really, this should abort since most input
    // streams could block here.
    if (NS_IsMainThread() && !mStream) {
      *aResult = 0;
      return NS_OK;
    }

    nsresult rv = BlockAndWaitForStream();
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mWeakSeekableStream) {
      NS_WARNING("Underlying blob stream is not seekable!");
      return NS_ERROR_NO_INTERFACE;
    }

    rv = mWeakSeekableStream->Tell(aResult);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMETHOD
  SetEOF()
  {
    nsresult rv = BlockAndWaitForStream();
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mWeakSeekableStream) {
      NS_WARNING("Underlying blob stream is not seekable!");
      return NS_ERROR_NO_INTERFACE;
    }

    rv = mWeakSeekableStream->SetEOF();
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  virtual nsIInputStream*
  BlockAndGetInternalStream()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    nsresult rv = BlockAndWaitForStream();
    NS_ENSURE_SUCCESS(rv, nullptr);

    return mStream;
  }

private:
  virtual ~RemoteInputStream()
  {
    if (!NS_IsMainThread()) {
      mStream = nullptr;
      mWeakSeekableStream = nullptr;
      ProxyReleaseToMainThread(mSourceBlob);
    }
  }

  void
  ReallyBlockAndWaitForStream()
  {
    mozilla::DebugOnly<bool> waited;

    {
      mozilla::MonitorAutoLock lock(mMonitor);

      waited = !mStream;

      while (!mStream) {
        mMonitor.Wait();
      }
    }

    MOZ_ASSERT(mStream);

#ifdef DEBUG
    if (waited && mWeakSeekableStream) {
      int64_t position;
      MOZ_ASSERT(NS_SUCCEEDED(mWeakSeekableStream->Tell(&position)),
                 "Failed to determine initial stream position!");
      MOZ_ASSERT(!position, "Stream not starting at 0!");
    }
#endif
  }

  nsresult
  BlockAndWaitForStream()
  {
    if (NS_IsMainThread()) {
      NS_WARNING("Blocking the main thread is not supported!");
      return NS_ERROR_FAILURE;
    }

    ReallyBlockAndWaitForStream();

    return NS_OK;
  }

  bool
  IsSeekableStream()
  {
    if (NS_IsMainThread()) {
      if (!mStream) {
        NS_WARNING("Don't know if this stream is seekable yet!");
        return true;
      }
    }
    else {
      ReallyBlockAndWaitForStream();
    }

    return !!mWeakSeekableStream;
  }
};

NS_IMPL_ADDREF(RemoteInputStream)
NS_IMPL_RELEASE(RemoteInputStream)

NS_INTERFACE_MAP_BEGIN(RemoteInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIIPCSerializableInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISeekableStream, IsSeekableStream())
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(IPrivateRemoteInputStream)
NS_INTERFACE_MAP_END

class InputStreamChild MOZ_FINAL
  : public PBlobStreamChild
{
  nsRefPtr<RemoteInputStream> mRemoteStream;

public:
  InputStreamChild(RemoteInputStream* aRemoteStream)
  : mRemoteStream(aRemoteStream)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aRemoteStream);
  }

  InputStreamChild()
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

private:
  // This method is only called by the IPDL message machinery.
  virtual bool
  Recv__delete__(const InputStreamParams& aParams,
                 const OptionalFileDescriptorSet& aFDs) MOZ_OVERRIDE;
};

class InputStreamParent MOZ_FINAL
  : public PBlobStreamParent
{
  nsRefPtr<RemoteInputStream> mRemoteStream;

public:
  InputStreamParent(RemoteInputStream* aRemoteStream)
  : mRemoteStream(aRemoteStream)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aRemoteStream);
  }

  InputStreamParent()
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

protected:
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

private:
  // This method is only called by the IPDL message machinery.
  virtual bool
  Recv__delete__(const InputStreamParams& aParams,
                 const OptionalFileDescriptorSet& aFDs) MOZ_OVERRIDE;
};

nsDOMFileBase*
ToConcreteBlob(nsIDOMBlob* aBlob)
{
  // XXX This is only safe so long as all blob implementations in our tree
  //     inherit nsDOMFileBase. If that ever changes then this will need to grow
  //     a real interface or something.
  return static_cast<nsDOMFileBase*>(aBlob);
}

} // anonymous namespace

// Each instance of this class will be dispatched to the network stream thread
// pool to run the first time where it will open the file input stream. It will
// then dispatch itself back to the main thread to send the child process its
// response (assuming that the child has not crashed). The runnable will then
// dispatch itself to the thread pool again in order to close the file input
// stream.
class BlobParent::OpenStreamRunnable MOZ_FINAL
  : public nsRunnable
{
  friend class nsRevocableEventPtr<OpenStreamRunnable>;

  // Only safe to access these pointers if mRevoked is false!
  BlobParent* mBlobActor;
  PBlobStreamParent* mStreamActor;

  nsCOMPtr<nsIInputStream> mStream;
  nsCOMPtr<nsIIPCSerializableInputStream> mSerializable;
  nsCOMPtr<nsIEventTarget> mTarget;

  bool mRevoked;
  bool mClosing;

public:
  OpenStreamRunnable(BlobParent* aBlobActor,
                     PBlobStreamParent* aStreamActor,
                     nsIInputStream* aStream,
                     nsIIPCSerializableInputStream* aSerializable,
                     nsIEventTarget* aTarget)
  : mBlobActor(aBlobActor), mStreamActor(aStreamActor), mStream(aStream),
    mSerializable(aSerializable), mTarget(aTarget), mRevoked(false),
    mClosing(false)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aBlobActor);
    MOZ_ASSERT(aStreamActor);
    MOZ_ASSERT(aStream);
    // aSerializable may be null.
    MOZ_ASSERT(aTarget);
  }

  NS_IMETHOD
  Run()
  {
    if (NS_IsMainThread()) {
      return SendResponse();
    }

    if (!mClosing) {
      return OpenStream();
    }

    return CloseStream();
  }

  nsresult
  Dispatch()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mTarget);

    nsresult rv = mTarget->Dispatch(this, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

private:
  void
  Revoke()
  {
    MOZ_ASSERT(NS_IsMainThread());
#ifdef DEBUG
    mBlobActor = nullptr;
    mStreamActor = nullptr;
#endif
    mRevoked = true;
  }

  nsresult
  OpenStream()
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mStream);

    if (!mSerializable) {
      nsCOMPtr<IPrivateRemoteInputStream> remoteStream =
        do_QueryInterface(mStream);
      MOZ_ASSERT(remoteStream, "Must QI to IPrivateRemoteInputStream here!");

      nsCOMPtr<nsIInputStream> realStream =
        remoteStream->BlockAndGetInternalStream();
      NS_ENSURE_TRUE(realStream, NS_ERROR_FAILURE);

      mSerializable = do_QueryInterface(realStream);
      if (!mSerializable) {
        MOZ_ASSERT(false, "Must be serializable!");
        return NS_ERROR_FAILURE;
      }

      mStream.swap(realStream);
    }

    // To force the stream open we call Available(). We don't actually care
    // how much data is available.
    uint64_t available;
    if (NS_FAILED(mStream->Available(&available))) {
      NS_WARNING("Available failed on this stream!");
    }

    nsresult rv = NS_DispatchToMainThread(this);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  nsresult
  CloseStream()
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mStream);

    // Going to always release here.
    nsCOMPtr<nsIInputStream> stream;
    mStream.swap(stream);

    nsresult rv = stream->Close();
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  nsresult
  SendResponse()
  {
    MOZ_ASSERT(NS_IsMainThread());

    MOZ_ASSERT(mStream);
    MOZ_ASSERT(mSerializable);
    MOZ_ASSERT(mTarget);
    MOZ_ASSERT(!mClosing);

    nsCOMPtr<nsIIPCSerializableInputStream> serializable;
    mSerializable.swap(serializable);

    if (mRevoked) {
      MOZ_ASSERT(!mBlobActor);
      MOZ_ASSERT(!mStreamActor);
    }
    else {
      MOZ_ASSERT(mBlobActor);
      MOZ_ASSERT(mStreamActor);

      InputStreamParams params;
      nsAutoTArray<FileDescriptor, 10> fds;
      serializable->Serialize(params, fds);

      MOZ_ASSERT(params.type() != InputStreamParams::T__None);

      PFileDescriptorSetParent* fdSet = nullptr;

      if (!fds.IsEmpty()) {
        auto* manager = static_cast<ContentParent*>(mBlobActor->Manager());
        MOZ_ASSERT(manager);

        fdSet = manager->SendPFileDescriptorSetConstructor(fds[0]);
        if (fdSet) {
          for (uint32_t index = 1; index < fds.Length(); index++) {
            unused << fdSet->SendAddFileDescriptor(fds[index]);
          }
        }
      }

      OptionalFileDescriptorSet optionalFDs;
      if (fdSet) {
        optionalFDs = fdSet;
      } else {
        optionalFDs = mozilla::void_t();
      }

      unused <<
        PBlobStreamParent::Send__delete__(mStreamActor, params, optionalFDs);

      mBlobActor->NoteRunnableCompleted(this);

#ifdef DEBUG
      mBlobActor = nullptr;
      mStreamActor = nullptr;
#endif
    }

    mClosing = true;

    nsCOMPtr<nsIEventTarget> target;
    mTarget.swap(target);

    nsresult rv = target->Dispatch(this, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }
};

/*******************************************************************************
 * BlobChild::RemoteBlob Declaration
 ******************************************************************************/

class BlobChild::RemoteBlob MOZ_FINAL
  : public nsDOMFile
  , public nsIRemoteBlob
{
  class StreamHelper;
  class SliceHelper;

  BlobChild* mActor;

public:
  RemoteBlob(const nsAString& aName,
             const nsAString& aContentType,
             uint64_t aLength,
             uint64_t aModDate)
    : nsDOMFile(aName, aContentType, aLength, aModDate)
    , mActor(nullptr)
  {
    mImmutable = true;
  }

  RemoteBlob(const nsAString& aContentType, uint64_t aLength)
    : nsDOMFile(aContentType, aLength)
    , mActor(nullptr)
  {
    mImmutable = true;
  }

  RemoteBlob()
    : nsDOMFile(EmptyString(), EmptyString(), UINT64_MAX, UINT64_MAX)
    , mActor(nullptr)
  {
    mImmutable = true;
  }

  void
  SetActor(BlobChild* aActor)
  {
    MOZ_ASSERT(!aActor || !mActor);
    mActor = aActor;
  }

  NS_DECL_ISUPPORTS_INHERITED

  virtual already_AddRefed<nsIDOMBlob>
  CreateSlice(uint64_t aStart, uint64_t aLength, const nsAString& aContentType)
              MOZ_OVERRIDE;

  NS_IMETHOD
  GetInternalStream(nsIInputStream** aStream) MOZ_OVERRIDE;

  NS_IMETHOD
  GetLastModifiedDate(JSContext* cx,
                      JS::MutableHandle<JS::Value> aLastModifiedDate)
                      MOZ_OVERRIDE;

  virtual void*
  GetPBlob() MOZ_OVERRIDE;

private:
  ~RemoteBlob()
  {
    if (mActor) {
      mActor->NoteDyingRemoteBlob();
    }
  }
};

class BlobChild::RemoteBlob::StreamHelper MOZ_FINAL
  : public nsRunnable
{
  mozilla::Monitor mMonitor;
  BlobChild* mActor;
  nsCOMPtr<nsIDOMBlob> mSourceBlob;
  nsRefPtr<RemoteInputStream> mInputStream;
  bool mDone;

public:
  StreamHelper(BlobChild* aActor, nsIDOMBlob* aSourceBlob)
    : mMonitor("BlobChild::RemoteBlob::StreamHelper::mMonitor")
    , mActor(aActor)
    , mSourceBlob(aSourceBlob)
    , mDone(false)
  {
    // This may be created on any thread.
    MOZ_ASSERT(aActor);
    MOZ_ASSERT(aSourceBlob);
  }

  nsresult
  GetStream(nsIInputStream** aInputStream)
  {
    // This may be called on any thread.
    MOZ_ASSERT(aInputStream);
    MOZ_ASSERT(mActor);
    MOZ_ASSERT(!mInputStream);
    MOZ_ASSERT(!mDone);

    if (NS_IsMainThread()) {
      RunInternal(false);
    }
    else {
      nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
      NS_ENSURE_TRUE(mainThread, NS_ERROR_FAILURE);

      nsresult rv = mainThread->Dispatch(this, NS_DISPATCH_NORMAL);
      NS_ENSURE_SUCCESS(rv, rv);

      {
        MonitorAutoLock lock(mMonitor);
        while (!mDone) {
          lock.Wait();
        }
      }
    }

    MOZ_ASSERT(!mActor);
    MOZ_ASSERT(mDone);

    if (!mInputStream) {
      return NS_ERROR_UNEXPECTED;
    }

    mInputStream.forget(aInputStream);
    return NS_OK;
  }

  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    RunInternal(true);
    return NS_OK;
  }

private:
  void
  RunInternal(bool aNotify)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mActor);
    MOZ_ASSERT(!mInputStream);
    MOZ_ASSERT(!mDone);

    nsRefPtr<RemoteInputStream> stream =
      new RemoteInputStream(mSourceBlob, ChildActor);

    InputStreamChild* streamActor = new InputStreamChild(stream);
    if (mActor->SendPBlobStreamConstructor(streamActor)) {
      stream.swap(mInputStream);
    }

    mActor = nullptr;

    if (aNotify) {
      MonitorAutoLock lock(mMonitor);
      mDone = true;
      lock.Notify();
    }
    else {
      mDone = true;
    }
  }
};

class BlobChild::RemoteBlob::SliceHelper MOZ_FINAL
  : public nsRunnable
{
  mozilla::Monitor mMonitor;
  BlobChild* mActor;
  nsCOMPtr<nsIDOMBlob> mSlice;
  uint64_t mStart;
  uint64_t mLength;
  nsString mContentType;
  bool mDone;

public:
  SliceHelper(BlobChild* aActor)
    : mMonitor("BlobChild::RemoteBlob::SliceHelper::mMonitor")
    , mActor(aActor)
    , mStart(0)
    , mLength(0)
    , mDone(false)
  {
    // This may be created on any thread.
    MOZ_ASSERT(aActor);
  }

  nsresult
  GetSlice(uint64_t aStart,
           uint64_t aLength,
           const nsAString& aContentType,
           nsIDOMBlob** aSlice)
  {
    // This may be called on any thread.
    MOZ_ASSERT(aSlice);
    MOZ_ASSERT(mActor);
    MOZ_ASSERT(!mSlice);
    MOZ_ASSERT(!mDone);

    mStart = aStart;
    mLength = aLength;
    mContentType = aContentType;

    if (NS_IsMainThread()) {
      RunInternal(false);
    }
    else {
      nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
      NS_ENSURE_TRUE(mainThread, NS_ERROR_FAILURE);

      nsresult rv = mainThread->Dispatch(this, NS_DISPATCH_NORMAL);
      NS_ENSURE_SUCCESS(rv, rv);

      {
        MonitorAutoLock lock(mMonitor);
        while (!mDone) {
          lock.Wait();
        }
      }
    }

    MOZ_ASSERT(!mActor);
    MOZ_ASSERT(mDone);

    if (!mSlice) {
      return NS_ERROR_UNEXPECTED;
    }

    mSlice.forget(aSlice);
    return NS_OK;
  }

  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    RunInternal(true);
    return NS_OK;
  }

private:
  void
  RunInternal(bool aNotify)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mActor);
    MOZ_ASSERT(!mSlice);
    MOZ_ASSERT(!mDone);

    NS_ENSURE_TRUE_VOID(mActor->Manager());

    NormalBlobConstructorParams normalParams;
    normalParams.contentType() = mContentType;
    normalParams.length() = mLength;

    auto* manager = static_cast<ContentChild*>(mActor->Manager());
    MOZ_ASSERT(manager);

    BlobChild* newActor = BlobChild::Create(manager, normalParams);
    MOZ_ASSERT(newActor);

    SlicedBlobConstructorParams slicedParams;
    slicedParams.contentType() = mContentType;
    slicedParams.begin() = mStart;
    slicedParams.end() = mStart + mLength;
    slicedParams.sourceChild() = mActor;

    ParentBlobConstructorParams otherSideParams;
    otherSideParams.blobParams() = slicedParams;
    otherSideParams.optionalInputStreamParams() = mozilla::void_t();

    if (mActor->Manager()->SendPBlobConstructor(newActor, otherSideParams)) {
      mSlice = newActor->GetBlob();
    }

    mActor = nullptr;

    if (aNotify) {
      MonitorAutoLock lock(mMonitor);
      mDone = true;
      lock.Notify();
    }
    else {
      mDone = true;
    }
  }
};

/*******************************************************************************
 * BlobChild::RemoteBlob Implementation
 ******************************************************************************/

NS_IMPL_ISUPPORTS_INHERITED(BlobChild::RemoteBlob, nsDOMFile, nsIRemoteBlob)

already_AddRefed<nsIDOMBlob>
BlobChild::
RemoteBlob::CreateSlice(uint64_t aStart,
                        uint64_t aLength,
                        const nsAString& aContentType)
{
  if (!mActor) {
    return nullptr;
  }

  nsRefPtr<SliceHelper> helper = new SliceHelper(mActor);

  nsCOMPtr<nsIDOMBlob> slice;
  nsresult rv =
    helper->GetSlice(aStart, aLength, aContentType, getter_AddRefs(slice));
  NS_ENSURE_SUCCESS(rv, nullptr);

  return slice.forget();
}

NS_IMETHODIMP
BlobChild::
RemoteBlob::GetInternalStream(nsIInputStream** aStream)
{
  if (!mActor) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<StreamHelper> helper = new StreamHelper(mActor, this);
  return helper->GetStream(aStream);
}

NS_IMETHODIMP
BlobChild::
RemoteBlob::GetLastModifiedDate(JSContext* cx,
                                JS::MutableHandle<JS::Value> aLastModifiedDate)
{
  if (IsDateUnknown()) {
    aLastModifiedDate.setNull();
  } else {
    JSObject* date = JS_NewDateObjectMsec(cx, mLastModificationDate);
    if (!date) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    aLastModifiedDate.setObject(*date);
  }
  return NS_OK;
}

void*
BlobChild::
RemoteBlob::GetPBlob()
{
  return static_cast<PBlobChild*>(mActor);
}

/*******************************************************************************
 * BlobChild
 ******************************************************************************/

BlobChild::BlobChild(ContentChild* aManager, nsIDOMBlob* aBlob)
  : mBlob(aBlob)
  , mRemoteBlob(nullptr)
  , mStrongManager(aManager)
  , mOwnsBlob(true)
  , mBlobIsFile(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aBlob);

  aBlob->AddRef();

  nsCOMPtr<nsIDOMFile> file = do_QueryInterface(aBlob);
  mBlobIsFile = !!file;
}

BlobChild::BlobChild(ContentChild* aManager,
                     const ChildBlobConstructorParams& aParams)
  : mBlob(nullptr)
  , mRemoteBlob(nullptr)
  , mStrongManager(aManager)
  , mOwnsBlob(false)
  , mBlobIsFile(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aManager);

  ChildBlobConstructorParams::Type paramsType = aParams.type();

  mBlobIsFile =
    paramsType == ChildBlobConstructorParams::TFileBlobConstructorParams ||
    paramsType == ChildBlobConstructorParams::TMysteryBlobConstructorParams;

  nsRefPtr<RemoteBlob> remoteBlob = CreateRemoteBlob(aParams);
  MOZ_ASSERT(remoteBlob);

  remoteBlob->SetActor(this);
  remoteBlob.forget(&mRemoteBlob);

  mBlob = mRemoteBlob;
  mOwnsBlob = true;
}

BlobChild::~BlobChild()
{
}

BlobChild*
BlobChild::Create(ContentChild* aManager,
                  const ChildBlobConstructorParams& aParams)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aManager);

  switch (aParams.type()) {
    case ChildBlobConstructorParams::TNormalBlobConstructorParams:
    case ChildBlobConstructorParams::TFileBlobConstructorParams:
    case ChildBlobConstructorParams::TMysteryBlobConstructorParams:
      return new BlobChild(aManager, aParams);

    case ChildBlobConstructorParams::TSlicedBlobConstructorParams: {
      const SlicedBlobConstructorParams& params =
        aParams.get_SlicedBlobConstructorParams();

      auto* actor =
        const_cast<BlobChild*>(
          static_cast<const BlobChild*>(params.sourceChild()));
      MOZ_ASSERT(actor);

      nsCOMPtr<nsIDOMBlob> source = actor->GetBlob();
      MOZ_ASSERT(source);

      nsCOMPtr<nsIDOMBlob> slice;
      nsresult rv =
        source->Slice(params.begin(), params.end(), params.contentType(), 3,
                      getter_AddRefs(slice));
      NS_ENSURE_SUCCESS(rv, nullptr);

      return new BlobChild(aManager, slice);
    }

    default:
      MOZ_CRASH("Unknown params!");
  }

  return nullptr;
}

already_AddRefed<nsIDOMBlob>
BlobChild::GetBlob()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mBlob);

  nsCOMPtr<nsIDOMBlob> blob;

  // Remote blobs are held alive until the first call to GetBlob. Thereafter we
  // only hold a weak reference. Normal blobs are held alive until the actor is
  // destroyed.
  if (mRemoteBlob && mOwnsBlob) {
    blob = dont_AddRef(mBlob);
    mOwnsBlob = false;
  }
  else {
    blob = mBlob;
  }

  MOZ_ASSERT(blob);

  return blob.forget();
}

bool
BlobChild::SetMysteryBlobInfo(const nsString& aName,
                              const nsString& aContentType,
                              uint64_t aLength,
                              uint64_t aLastModifiedDate)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(mRemoteBlob);
  MOZ_ASSERT(aLastModifiedDate != UINT64_MAX);

  ToConcreteBlob(mBlob)->SetLazyData(aName, aContentType, aLength,
                                     aLastModifiedDate);

  FileBlobConstructorParams params(aName, aContentType, aLength,
                                   aLastModifiedDate);
  return SendResolveMystery(params);
}

bool
BlobChild::SetMysteryBlobInfo(const nsString& aContentType, uint64_t aLength)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(mRemoteBlob);

  nsString voidString;
  voidString.SetIsVoid(true);

  ToConcreteBlob(mBlob)->SetLazyData(voidString, aContentType, aLength,
                                     UINT64_MAX);

  NormalBlobConstructorParams params(aContentType, aLength);
  return SendResolveMystery(params);
}

already_AddRefed<BlobChild::RemoteBlob>
BlobChild::CreateRemoteBlob(const ChildBlobConstructorParams& aParams)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<RemoteBlob> remoteBlob;

  switch (aParams.type()) {
    case ChildBlobConstructorParams::TNormalBlobConstructorParams: {
      const NormalBlobConstructorParams& params =
        aParams.get_NormalBlobConstructorParams();
      remoteBlob = new RemoteBlob(params.contentType(), params.length());
      break;
    }

    case ChildBlobConstructorParams::TFileBlobConstructorParams: {
      const FileBlobConstructorParams& params =
        aParams.get_FileBlobConstructorParams();
      remoteBlob =
        new RemoteBlob(params.name(), params.contentType(), params.length(),
                       params.modDate());
      break;
    }

    case ChildBlobConstructorParams::TMysteryBlobConstructorParams: {
      remoteBlob = new RemoteBlob();
      break;
    }

    default:
      MOZ_CRASH("Unknown params!");
  }

  MOZ_ASSERT(remoteBlob);

  if (NS_FAILED(remoteBlob->SetMutable(false))) {
    MOZ_CRASH("Failed to make remote blob immutable!");
  }

  return remoteBlob.forget();
}

void
BlobChild::NoteDyingRemoteBlob()
{
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(mRemoteBlob);
  MOZ_ASSERT(!mOwnsBlob);

  // This may be called on any thread due to the fact that RemoteBlob is
  // designed to be passed between threads. We must start the shutdown process
  // on the main thread, so we proxy here if necessary.
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewNonOwningRunnableMethod(this, &BlobChild::NoteDyingRemoteBlob);
    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      MOZ_ASSERT(false, "Should never fail!");
    }

    return;
  }

  // Must do this before calling Send__delete__ or we'll crash there trying to
  // access a dangling pointer.
  mBlob = nullptr;
  mRemoteBlob = nullptr;

  PBlobChild::Send__delete__(this);
}

void
BlobChild::ActorDestroy(ActorDestroyReason aWhy)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mRemoteBlob) {
    mRemoteBlob->SetActor(nullptr);
  }

  if (mBlob && mOwnsBlob) {
    mBlob->Release();
  }

  mStrongManager = nullptr;
}

PBlobStreamChild*
BlobChild::AllocPBlobStreamChild()
{
  MOZ_ASSERT(NS_IsMainThread());

  return new InputStreamChild();
}

bool
BlobChild::RecvPBlobStreamConstructor(PBlobStreamChild* aActor)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(!mRemoteBlob);

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = mBlob->GetInternalStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsIIPCSerializableInputStream> serializable =
    do_QueryInterface(stream);
  if (!serializable) {
    MOZ_ASSERT(false, "Must be serializable!");
    return false;
  }

  InputStreamParams params;
  nsTArray<FileDescriptor> fds;
  serializable->Serialize(params, fds);

  MOZ_ASSERT(params.type() != InputStreamParams::T__None);
  MOZ_ASSERT(fds.IsEmpty());

  return aActor->Send__delete__(aActor, params, mozilla::void_t());
}

bool
BlobChild::DeallocPBlobStreamChild(PBlobStreamChild* aActor)
{
  MOZ_ASSERT(NS_IsMainThread());

  delete static_cast<InputStreamChild*>(aActor);
  return true;
}

bool
BlobChild::RecvResolveMystery(const ResolveMysteryParams& aParams)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(!mRemoteBlob);
  MOZ_ASSERT(mOwnsBlob);

  if (!mBlobIsFile) {
    MOZ_ASSERT(false, "Must always be a file!");
    return false;
  }

  nsDOMFileBase* blob = ToConcreteBlob(mBlob);

  switch (aParams.type()) {
    case ResolveMysteryParams::TNormalBlobConstructorParams: {
      const NormalBlobConstructorParams& params =
        aParams.get_NormalBlobConstructorParams();
      nsString voidString;
      voidString.SetIsVoid(true);
      blob->SetLazyData(voidString, params.contentType(), params.length(),
                        UINT64_MAX);
      break;
    }

    case ResolveMysteryParams::TFileBlobConstructorParams: {
      const FileBlobConstructorParams& params =
        aParams.get_FileBlobConstructorParams();
      blob->SetLazyData(params.name(), params.contentType(), params.length(),
                        params.modDate());
      break;
    }

    default:
      MOZ_CRASH("Unknown params!");
  }

  return true;
}

/*******************************************************************************
 * BlobParent::RemoteBlob Declaration
 ******************************************************************************/

class BlobParent::RemoteBlob MOZ_FINAL
  : public nsDOMFile
  , public nsIRemoteBlob
{
  class StreamHelper;
  class SliceHelper;

  BlobParent* mActor;
  InputStreamParams mInputStreamParams;

public:
  RemoteBlob(const nsAString& aName,
             const nsAString& aContentType,
             uint64_t aLength,
             uint64_t aModDate)
    : nsDOMFile(aName, aContentType, aLength, aModDate)
    , mActor(nullptr)
  {
    mImmutable = true;
  }

  RemoteBlob(const nsAString& aContentType, uint64_t aLength)
    : nsDOMFile(aContentType, aLength)
    , mActor(nullptr)
  {
    mImmutable = true;
  }

  RemoteBlob()
    : nsDOMFile(EmptyString(), EmptyString(), UINT64_MAX, UINT64_MAX)
    , mActor(nullptr)
  {
    mImmutable = true;
  }

  void
  SetActor(BlobParent* aActor)
  {
    MOZ_ASSERT(!aActor || !mActor);
    mActor = aActor;
  }

  void
  MaybeSetInputStream(const ParentBlobConstructorParams& aParams)
  {
    if (aParams.optionalInputStreamParams().type() ==
        OptionalInputStreamParams::TInputStreamParams) {
      mInputStreamParams =
        aParams.optionalInputStreamParams().get_InputStreamParams();
    }
  }

  NS_DECL_ISUPPORTS_INHERITED

  virtual already_AddRefed<nsIDOMBlob>
  CreateSlice(uint64_t aStart, uint64_t aLength, const nsAString& aContentType)
              MOZ_OVERRIDE;

  NS_IMETHOD
  GetInternalStream(nsIInputStream** aStream) MOZ_OVERRIDE;

  NS_IMETHOD
  GetLastModifiedDate(JSContext* cx,
                      JS::MutableHandle<JS::Value> aLastModifiedDate)
                      MOZ_OVERRIDE;

  virtual void*
  GetPBlob() MOZ_OVERRIDE;

private:
  ~RemoteBlob()
  {
    if (mActor) {
      mActor->NoteDyingRemoteBlob();
    }
  }
};

class BlobParent::RemoteBlob::StreamHelper MOZ_FINAL
  : public nsRunnable
{
  mozilla::Monitor mMonitor;
  BlobParent* mActor;
  nsCOMPtr<nsIDOMBlob> mSourceBlob;
  nsRefPtr<RemoteInputStream> mInputStream;
  bool mDone;

public:
  StreamHelper(BlobParent* aActor, nsIDOMBlob* aSourceBlob)
    : mMonitor("BlobParent::RemoteBlob::StreamHelper::mMonitor")
    , mActor(aActor)
    , mSourceBlob(aSourceBlob)
    , mDone(false)
  {
    // This may be created on any thread.
    MOZ_ASSERT(aActor);
    MOZ_ASSERT(aSourceBlob);
  }

  nsresult
  GetStream(nsIInputStream** aInputStream)
  {
    // This may be called on any thread.
    MOZ_ASSERT(aInputStream);
    MOZ_ASSERT(mActor);
    MOZ_ASSERT(!mInputStream);
    MOZ_ASSERT(!mDone);

    if (NS_IsMainThread()) {
      RunInternal(false);
    }
    else {
      nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
      NS_ENSURE_TRUE(mainThread, NS_ERROR_FAILURE);

      nsresult rv = mainThread->Dispatch(this, NS_DISPATCH_NORMAL);
      NS_ENSURE_SUCCESS(rv, rv);

      {
        MonitorAutoLock lock(mMonitor);
        while (!mDone) {
          lock.Wait();
        }
      }
    }

    MOZ_ASSERT(!mActor);
    MOZ_ASSERT(mDone);

    if (!mInputStream) {
      return NS_ERROR_UNEXPECTED;
    }

    mInputStream.forget(aInputStream);
    return NS_OK;
  }

  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    RunInternal(true);
    return NS_OK;
  }

private:
  void
  RunInternal(bool aNotify)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mActor);
    MOZ_ASSERT(!mInputStream);
    MOZ_ASSERT(!mDone);

    nsRefPtr<RemoteInputStream> stream =
      new RemoteInputStream(mSourceBlob, ParentActor);

    InputStreamParent* streamActor = new InputStreamParent(stream);
    if (mActor->SendPBlobStreamConstructor(streamActor)) {
      stream.swap(mInputStream);
    }

    mActor = nullptr;

    if (aNotify) {
      MonitorAutoLock lock(mMonitor);
      mDone = true;
      lock.Notify();
    }
    else {
      mDone = true;
    }
  }
};

class BlobParent::RemoteBlob::SliceHelper MOZ_FINAL
  : public nsRunnable
{
  mozilla::Monitor mMonitor;
  BlobParent* mActor;
  nsCOMPtr<nsIDOMBlob> mSlice;
  uint64_t mStart;
  uint64_t mLength;
  nsString mContentType;
  bool mDone;

public:
  SliceHelper(BlobParent* aActor)
    : mMonitor("BlobParent::RemoteBlob::SliceHelper::mMonitor")
    , mActor(aActor)
    , mStart(0)
    , mLength(0)
    , mDone(false)
  {
    // This may be created on any thread.
    MOZ_ASSERT(aActor);
  }

  nsresult
  GetSlice(uint64_t aStart,
           uint64_t aLength,
           const nsAString& aContentType,
           nsIDOMBlob** aSlice)
  {
    // This may be called on any thread.
    MOZ_ASSERT(aSlice);
    MOZ_ASSERT(mActor);
    MOZ_ASSERT(!mSlice);
    MOZ_ASSERT(!mDone);

    mStart = aStart;
    mLength = aLength;
    mContentType = aContentType;

    if (NS_IsMainThread()) {
      RunInternal(false);
    }
    else {
      nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
      NS_ENSURE_TRUE(mainThread, NS_ERROR_FAILURE);

      nsresult rv = mainThread->Dispatch(this, NS_DISPATCH_NORMAL);
      NS_ENSURE_SUCCESS(rv, rv);

      {
        MonitorAutoLock lock(mMonitor);
        while (!mDone) {
          lock.Wait();
        }
      }
    }

    MOZ_ASSERT(!mActor);
    MOZ_ASSERT(mDone);

    if (!mSlice) {
      return NS_ERROR_UNEXPECTED;
    }

    mSlice.forget(aSlice);
    return NS_OK;
  }

  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    RunInternal(true);
    return NS_OK;
  }

private:
  void
  RunInternal(bool aNotify)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mActor);
    MOZ_ASSERT(!mSlice);
    MOZ_ASSERT(!mDone);

    NS_ENSURE_TRUE_VOID(mActor->Manager());

    NormalBlobConstructorParams normalParams;
    normalParams.contentType() = mContentType;
    normalParams.length() = mLength;

    ParentBlobConstructorParams params;
    params.blobParams() = normalParams;
    params.optionalInputStreamParams() = void_t();

    auto* manager = static_cast<ContentParent*>(mActor->Manager());
    MOZ_ASSERT(manager);

    BlobParent* newActor = BlobParent::Create(manager, params);
    MOZ_ASSERT(newActor);

    SlicedBlobConstructorParams slicedParams;
    slicedParams.contentType() = mContentType;
    slicedParams.begin() = mStart;
    slicedParams.end() = mStart + mLength;
    slicedParams.sourceParent() = mActor;

    ChildBlobConstructorParams otherSideParams = slicedParams;

    if (mActor->Manager()->SendPBlobConstructor(newActor, otherSideParams)) {
      mSlice = newActor->GetBlob();
    }

    mActor = nullptr;

    if (aNotify) {
      MonitorAutoLock lock(mMonitor);
      mDone = true;
      lock.Notify();
    }
    else {
      mDone = true;
    }
  }
};

/*******************************************************************************
 * BlobChild::RemoteBlob Implementation
 ******************************************************************************/

NS_IMPL_ISUPPORTS_INHERITED(BlobParent::RemoteBlob, nsDOMFile, nsIRemoteBlob)

already_AddRefed<nsIDOMBlob>
BlobParent::
RemoteBlob::CreateSlice(uint64_t aStart,
                        uint64_t aLength,
                        const nsAString& aContentType)
{
  if (!mActor) {
    return nullptr;
  }

  nsRefPtr<SliceHelper> helper = new SliceHelper(mActor);

  nsCOMPtr<nsIDOMBlob> slice;
  nsresult rv =
    helper->GetSlice(aStart, aLength, aContentType, getter_AddRefs(slice));
  NS_ENSURE_SUCCESS(rv, nullptr);

  return slice.forget();
}

NS_IMETHODIMP
BlobParent::
RemoteBlob::GetInternalStream(nsIInputStream** aStream)
{
  if (mInputStreamParams.type() != InputStreamParams::T__None) {
    nsTArray<FileDescriptor> fds;
    nsCOMPtr<nsIInputStream> realStream =
      DeserializeInputStream(mInputStreamParams, fds);
    if (!realStream) {
      NS_WARNING("Failed to deserialize stream!");
      return NS_ERROR_UNEXPECTED;
    }

    nsCOMPtr<nsIInputStream> stream =
      new BlobInputStreamTether(realStream, this);
    stream.forget(aStream);
    return NS_OK;
  }

  if (!mActor) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<StreamHelper> helper = new StreamHelper(mActor, this);
  return helper->GetStream(aStream);
}

NS_IMETHODIMP
BlobParent::
RemoteBlob::GetLastModifiedDate(JSContext* cx,
                                JS::MutableHandle<JS::Value> aLastModifiedDate)
{
  if (IsDateUnknown()) {
    aLastModifiedDate.setNull();
  } else {
    JSObject* date = JS_NewDateObjectMsec(cx, mLastModificationDate);
    if (!date) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    aLastModifiedDate.setObject(*date);
  }
  return NS_OK;
}

void*
BlobParent::
RemoteBlob::GetPBlob()
{
  return static_cast<PBlobParent*>(mActor);
}

/*******************************************************************************
 * BlobParent
 ******************************************************************************/

BlobParent::BlobParent(ContentParent* aManager, nsIDOMBlob* aBlob)
  : mBlob(aBlob)
  , mRemoteBlob(nullptr)
  , mStrongManager(aManager)
  , mOwnsBlob(true)
  , mBlobIsFile(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aBlob);

  aBlob->AddRef();

  nsCOMPtr<nsIDOMFile> file = do_QueryInterface(aBlob);
  mBlobIsFile = !!file;
}

BlobParent::BlobParent(ContentParent* aManager,
                       const ParentBlobConstructorParams& aParams)
  : mBlob(nullptr)
  , mRemoteBlob(nullptr)
  , mStrongManager(aManager)
  , mOwnsBlob(false)
  , mBlobIsFile(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aManager);

  ChildBlobConstructorParams::Type paramsType = aParams.blobParams().type();

  mBlobIsFile =
    paramsType == ChildBlobConstructorParams::TFileBlobConstructorParams ||
    paramsType == ChildBlobConstructorParams::TMysteryBlobConstructorParams;

  nsRefPtr<RemoteBlob> remoteBlob = CreateRemoteBlob(aParams);
  MOZ_ASSERT(remoteBlob);

  remoteBlob->SetActor(this);
  remoteBlob->MaybeSetInputStream(aParams);
  remoteBlob.forget(&mRemoteBlob);

  mBlob = mRemoteBlob;
  mOwnsBlob = true;
}

BlobParent::~BlobParent()
{
}

BlobParent*
BlobParent::Create(ContentParent* aManager,
                   const ParentBlobConstructorParams& aParams)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aManager);

  const ChildBlobConstructorParams& blobParams = aParams.blobParams();

  MOZ_ASSERT(blobParams.type() !=
             ChildBlobConstructorParams::TMysteryBlobConstructorParams);

  switch (blobParams.type()) {
    case ChildBlobConstructorParams::TMysteryBlobConstructorParams:
      return nullptr;

    case ChildBlobConstructorParams::TNormalBlobConstructorParams:
    case ChildBlobConstructorParams::TFileBlobConstructorParams:
      return new BlobParent(aManager, aParams);

    case ChildBlobConstructorParams::TSlicedBlobConstructorParams: {
      const SlicedBlobConstructorParams& params =
        blobParams.get_SlicedBlobConstructorParams();

      auto* actor =
        const_cast<BlobParent*>(
          static_cast<const BlobParent*>(params.sourceParent()));
      MOZ_ASSERT(actor);

      nsCOMPtr<nsIDOMBlob> source = actor->GetBlob();
      MOZ_ASSERT(source);

      nsCOMPtr<nsIDOMBlob> slice;
      nsresult rv =
        source->Slice(params.begin(), params.end(), params.contentType(), 3,
                      getter_AddRefs(slice));
      NS_ENSURE_SUCCESS(rv, nullptr);

      return new BlobParent(aManager, slice);
    }

    default:
      MOZ_CRASH("Unknown params!");
  }

  return nullptr;
}

already_AddRefed<nsIDOMBlob>
BlobParent::GetBlob()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mBlob);

  nsCOMPtr<nsIDOMBlob> blob;

  // Remote blobs are held alive until the first call to GetBlob. Thereafter we
  // only hold a weak reference. Normal blobs are held alive until the actor is
  // destroyed.
  if (mRemoteBlob && mOwnsBlob) {
    blob = dont_AddRef(mBlob);
    mOwnsBlob = false;
  }
  else {
    blob = mBlob;
  }

  MOZ_ASSERT(blob);

  return blob.forget();
}

bool
BlobParent::SetMysteryBlobInfo(const nsString& aName,
                               const nsString& aContentType,
                               uint64_t aLength,
                               uint64_t aLastModifiedDate)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(mRemoteBlob);
  MOZ_ASSERT(aLastModifiedDate != UINT64_MAX);

  ToConcreteBlob(mBlob)->SetLazyData(aName, aContentType, aLength,
                                     aLastModifiedDate);

  FileBlobConstructorParams params(aName, aContentType, aLength,
                                   aLastModifiedDate);
  return SendResolveMystery(params);
}

bool
BlobParent::SetMysteryBlobInfo(const nsString& aContentType, uint64_t aLength)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(mRemoteBlob);

  nsString voidString;
  voidString.SetIsVoid(true);

  ToConcreteBlob(mBlob)->SetLazyData(voidString, aContentType, aLength,
                                     UINT64_MAX);

  NormalBlobConstructorParams params(aContentType, aLength);
  return SendResolveMystery(params);
}

already_AddRefed<BlobParent::RemoteBlob>
BlobParent::CreateRemoteBlob(const ParentBlobConstructorParams& aParams)
{
  MOZ_ASSERT(NS_IsMainThread());

  const ChildBlobConstructorParams& blobParams = aParams.blobParams();

  nsRefPtr<RemoteBlob> remoteBlob;

  switch (blobParams.type()) {
    case ChildBlobConstructorParams::TNormalBlobConstructorParams: {
      const NormalBlobConstructorParams& params =
        blobParams.get_NormalBlobConstructorParams();
      remoteBlob = new RemoteBlob(params.contentType(), params.length());
      break;
    }

    case ChildBlobConstructorParams::TFileBlobConstructorParams: {
      const FileBlobConstructorParams& params =
        blobParams.get_FileBlobConstructorParams();
      remoteBlob =
        new RemoteBlob(params.name(), params.contentType(), params.length(),
                       params.modDate());
      break;
    }

    case ChildBlobConstructorParams::TMysteryBlobConstructorParams: {
      remoteBlob = new RemoteBlob();
      break;
    }

    default:
      MOZ_CRASH("Unknown params!");
  }

  MOZ_ASSERT(remoteBlob);

  if (NS_FAILED(remoteBlob->SetMutable(false))) {
    MOZ_CRASH("Failed to make remote blob immutable!");
  }

  return remoteBlob.forget();
}

void
BlobParent::NoteDyingRemoteBlob()
{
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(mRemoteBlob);
  MOZ_ASSERT(!mOwnsBlob);

  // This may be called on any thread due to the fact that RemoteBlob is
  // designed to be passed between threads. We must start the shutdown process
  // on the main thread, so we proxy here if necessary.
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewNonOwningRunnableMethod(this, &BlobParent::NoteDyingRemoteBlob);
    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      MOZ_ASSERT(false, "Should never fail!");
    }

    return;
  }

  // Must do this before calling Send__delete__ or we'll crash there trying to
  // access a dangling pointer.
  mBlob = nullptr;
  mRemoteBlob = nullptr;

  mozilla::unused << PBlobParent::Send__delete__(this);
}

void
BlobParent::NoteRunnableCompleted(OpenStreamRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  for (uint32_t index = 0; index < mOpenStreamRunnables.Length(); index++) {
    nsRevocableEventPtr<OpenStreamRunnable>& runnable =
      mOpenStreamRunnables[index];

    if (runnable.get() == aRunnable) {
      runnable.Forget();
      mOpenStreamRunnables.RemoveElementAt(index);
      return;
    }
  }

  MOZ_CRASH("Runnable not in our array!");
}

void
BlobParent::ActorDestroy(ActorDestroyReason aWhy)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mRemoteBlob) {
    mRemoteBlob->SetActor(nullptr);
  }

  if (mBlob && mOwnsBlob) {
    mBlob->Release();
  }

  mStrongManager = nullptr;
}

PBlobStreamParent*
BlobParent::AllocPBlobStreamParent()
{
  MOZ_ASSERT(NS_IsMainThread());

  return new InputStreamParent();
}

bool
BlobParent::RecvPBlobStreamConstructor(PBlobStreamParent* aActor)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(!mRemoteBlob);

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = mBlob->GetInternalStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsIRemoteBlob> remoteBlob = do_QueryInterface(mBlob);

  nsCOMPtr<IPrivateRemoteInputStream> remoteStream;
  if (remoteBlob) {
    remoteStream = do_QueryInterface(stream);
  }

  // There are three cases in which we can use the stream obtained from the blob
  // directly as our serialized stream:
  //
  //   1. The blob is not a remote blob.
  //   2. The blob is a remote blob that represents this actor.
  //   3. The blob is a remote blob representing a different actor but we
  //      already have a non-remote, i.e. serialized, serialized stream.
  //
  // In all other cases we need to be on a background thread before we can get
  // to the real stream.
  nsCOMPtr<nsIIPCSerializableInputStream> serializableStream;
  if (!remoteBlob ||
      static_cast<PBlobParent*>(remoteBlob->GetPBlob()) == this ||
      !remoteStream) {
    serializableStream = do_QueryInterface(stream);
    if (!serializableStream) {
      MOZ_ASSERT(false, "Must be serializable!");
      return false;
    }
  }

  nsCOMPtr<nsIEventTarget> target =
    do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(target, false);

  nsRefPtr<OpenStreamRunnable> runnable =
    new OpenStreamRunnable(this, aActor, stream, serializableStream, target);

  rv = runnable->Dispatch();
  NS_ENSURE_SUCCESS(rv, false);

  // nsRevocableEventPtr lacks some of the operators needed for anything nicer.
  *mOpenStreamRunnables.AppendElement() = runnable;
  return true;
}

bool
BlobParent::DeallocPBlobStreamParent(PBlobStreamParent* aActor)
{
  MOZ_ASSERT(NS_IsMainThread());

  delete static_cast<InputStreamParent*>(aActor);
  return true;
}

bool
BlobParent::RecvResolveMystery(const ResolveMysteryParams& aParams)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(!mRemoteBlob);
  MOZ_ASSERT(mOwnsBlob);

  if (!mBlobIsFile) {
    MOZ_ASSERT(false, "Must always be a file!");
    return false;
  }

  nsDOMFileBase* blob = ToConcreteBlob(mBlob);

  switch (aParams.type()) {
    case ResolveMysteryParams::TNormalBlobConstructorParams: {
      const NormalBlobConstructorParams& params =
        aParams.get_NormalBlobConstructorParams();
      nsString voidString;
      voidString.SetIsVoid(true);
      blob->SetLazyData(voidString, params.contentType(),
                        params.length(), UINT64_MAX);
      break;
    }

    case ResolveMysteryParams::TFileBlobConstructorParams: {
      const FileBlobConstructorParams& params =
        aParams.get_FileBlobConstructorParams();
      blob->SetLazyData(params.name(), params.contentType(),
                        params.length(), params.modDate());
      break;
    }

    default:
      MOZ_CRASH("Unknown params!");
  }

  return true;
}

bool
InputStreamChild::Recv__delete__(const InputStreamParams& aParams,
                                 const OptionalFileDescriptorSet& aFDs)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mRemoteStream);

  nsTArray<FileDescriptor> fds;
  if (aFDs.type() == OptionalFileDescriptorSet::TPFileDescriptorSetChild) {
    FileDescriptorSetChild* fdSetActor =
      static_cast<FileDescriptorSetChild*>(aFDs.get_PFileDescriptorSetChild());
    MOZ_ASSERT(fdSetActor);

    fdSetActor->ForgetFileDescriptors(fds);
    MOZ_ASSERT(!fds.IsEmpty());

    fdSetActor->Send__delete__(fdSetActor);
  }

  nsCOMPtr<nsIInputStream> stream = DeserializeInputStream(aParams, fds);
  if (!stream) {
    return false;
  }

  mRemoteStream->SetStream(stream);
  return true;
}

void
InputStreamParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Implement me! Bug 1005150
}

bool
InputStreamParent::Recv__delete__(const InputStreamParams& aParams,
                                  const OptionalFileDescriptorSet& aFDs)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mRemoteStream);

  if (aFDs.type() != OptionalFileDescriptorSet::Tvoid_t) {
    NS_WARNING("Child cannot send FileDescriptors to the parent!");
    return false;
  }

  nsTArray<FileDescriptor> fds;
  nsCOMPtr<nsIInputStream> stream = DeserializeInputStream(aParams, fds);
  if (!stream) {
    return false;
  }

  MOZ_ASSERT(fds.IsEmpty());

  mRemoteStream->SetStream(stream);
  return true;
}
