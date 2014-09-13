/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BlobChild.h"
#include "BlobParent.h"

#include "BackgroundParent.h"
#include "ContentChild.h"
#include "ContentParent.h"
#include "FileDescriptorSetChild.h"
#include "jsapi.h"
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Monitor.h"
#include "mozilla/unused.h"
#include "mozilla/dom/nsIContentParent.h"
#include "mozilla/dom/nsIContentChild.h"
#include "mozilla/dom/PBlobStreamChild.h"
#include "mozilla/dom/PBlobStreamParent.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseManager.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/PFileDescriptorSetParent.h"
#include "nsCOMPtr.h"
#include "nsDOMFile.h"
#include "nsIDOMFile.h"
#include "nsIInputStream.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsIMultiplexInputStream.h"
#include "nsIRemoteBlob.h"
#include "nsISeekableStream.h"
#include "nsNetCID.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#ifdef DEBUG
#include "BackgroundChild.h" // BackgroundChild::GetForCurrentThread().
#endif

#define DISABLE_ASSERTS_FOR_FUZZING 0

#if DISABLE_ASSERTS_FOR_FUZZING
#define ASSERT_UNLESS_FUZZING(...) do { } while (0)
#else
#define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false, __VA_ARGS__)
#endif

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

template <class ManagerType>
struct ConcreteManagerTypeTraits;

template <>
struct ConcreteManagerTypeTraits<nsIContentChild>
{
  typedef ContentChild Type;
};

template <>
struct ConcreteManagerTypeTraits<PBackgroundChild>
{
  typedef PBackgroundChild Type;
};

template <>
struct ConcreteManagerTypeTraits<nsIContentParent>
{
  typedef ContentParent Type;
};

template <>
struct ConcreteManagerTypeTraits<PBackgroundParent>
{
  typedef PBackgroundParent Type;
};

template <class ManagerType>
void AssertCorrectThreadForManager(ManagerType* aManager);

template <>
void AssertCorrectThreadForManager<nsIContentChild>(nsIContentChild* aManager)
{
  MOZ_ASSERT(NS_IsMainThread());
}

template <>
void AssertCorrectThreadForManager<nsIContentParent>(nsIContentParent* aManager)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(NS_IsMainThread());
}

template <>
void AssertCorrectThreadForManager<PBackgroundChild>(PBackgroundChild* aManager)
{
#ifdef DEBUG
  if (aManager) {
    PBackgroundChild* backgroundChild = BackgroundChild::GetForCurrentThread();
    MOZ_ASSERT(backgroundChild);
    MOZ_ASSERT(backgroundChild == aManager);
  }
#endif
}

template <>
void AssertCorrectThreadForManager<PBackgroundParent>(
                                                    PBackgroundParent* aManager)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  AssertIsOnBackgroundThread();
}

bool
EventTargetIsOnCurrentThread(nsIEventTarget* aEventTarget)
{
  if (!aEventTarget) {
    return NS_IsMainThread();
  }

  bool current;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(aEventTarget->IsOnCurrentThread(&current)));

  return current;
}

// Ensure that a nsCOMPtr/nsRefPtr is released on the target thread.
template <template <class> class SmartPtr, class T>
void
ReleaseOnTarget(SmartPtr<T>& aDoomed, nsIEventTarget* aTarget)
{
  MOZ_ASSERT(aDoomed);
  MOZ_ASSERT(!EventTargetIsOnCurrentThread(aTarget));

  T* doomedRaw;
  aDoomed.forget(&doomedRaw);

  auto* doomedSupports = static_cast<nsISupports*>(doomedRaw);

  nsCOMPtr<nsIRunnable> releaseRunnable =
    NS_NewNonOwningRunnableMethod(doomedSupports, &nsISupports::Release);
  MOZ_ASSERT(releaseRunnable);

  if (aTarget) {
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(aTarget->Dispatch(releaseRunnable,
                                                   NS_DISPATCH_NORMAL)));
  } else {
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(releaseRunnable)));
  }
}

template <class ManagerType>
PFileDescriptorSetParent*
ConstructFileDescriptorSet(ManagerType* aManager,
                           const nsTArray<FileDescriptor>& aFDs)
{
  typedef typename ConcreteManagerTypeTraits<ManagerType>::Type
          ConcreteManagerType;

  MOZ_ASSERT(aManager);

  if (aFDs.IsEmpty()) {
    return nullptr;
  }

  auto* concreteManager = static_cast<ConcreteManagerType*>(aManager);

  PFileDescriptorSetParent* fdSet =
    concreteManager->SendPFileDescriptorSetConstructor(aFDs[0]);
  if (!fdSet) {
    return nullptr;
  }

  for (uint32_t index = 1; index < aFDs.Length(); index++) {
    if (!fdSet->SendAddFileDescriptor(aFDs[index])) {
      return nullptr;
    }
  }

  return fdSet;
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
  nsRefPtr<DOMFileImplBase> mSourceBlob;
  nsCOMPtr<nsIEventTarget> mEventTarget;

  nsIMultiplexInputStream* mWeakMultiplexStream;
  nsISeekableStream* mWeakSeekableStream;
  nsIIPCSerializableInputStream* mWeakSerializableStream;

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_FORWARD_NSIINPUTSTREAM(mStream->)
  NS_FORWARD_SAFE_NSIMULTIPLEXINPUTSTREAM(mWeakMultiplexStream)
  NS_FORWARD_SAFE_NSISEEKABLESTREAM(mWeakSeekableStream)
  NS_FORWARD_SAFE_NSIIPCSERIALIZABLEINPUTSTREAM(mWeakSerializableStream)

  BlobInputStreamTether(nsIInputStream* aStream,
                        mozilla::dom::DOMFileImplBase* aSourceBlob)
    : mStream(aStream)
    , mSourceBlob(aSourceBlob)
    , mWeakMultiplexStream(nullptr)
    , mWeakSeekableStream(nullptr)
    , mWeakSerializableStream(nullptr)
  {
    MOZ_ASSERT(aStream);
    MOZ_ASSERT(aSourceBlob);

    if (!NS_IsMainThread()) {
      mEventTarget = do_GetCurrentThread();
      MOZ_ASSERT(mEventTarget);
    }

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

    if (!EventTargetIsOnCurrentThread(mEventTarget)) {
      mStream = nullptr;
      ReleaseOnTarget(mSourceBlob, mEventTarget);
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
  Monitor mMonitor;
  nsCOMPtr<nsIInputStream> mStream;
  nsRefPtr<DOMFileImplBase> mSourceBlob;
  nsCOMPtr<nsIEventTarget> mEventTarget;
  nsISeekableStream* mWeakSeekableStream;
  ActorType mOrigin;

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  RemoteInputStream(mozilla::dom::DOMFileImplBase* aSourceBlob,
                    ActorType aOrigin)
    : mMonitor("RemoteInputStream.mMonitor")
    , mSourceBlob(aSourceBlob)
    , mWeakSeekableStream(nullptr)
    , mOrigin(aOrigin)
  {
    MOZ_ASSERT(IsOnOwningThread());
    MOZ_ASSERT(aSourceBlob);

    if (!NS_IsMainThread()) {
      mEventTarget = do_GetCurrentThread();
      MOZ_ASSERT(mEventTarget);
    }
  }

  bool
  IsOnOwningThread() const
  {
    return EventTargetIsOnCurrentThread(mEventTarget);
  }

  void
  AssertIsOnOwningThread() const
  {
    MOZ_ASSERT(IsOnOwningThread());
  }

  void
  Serialize(InputStreamParams& aParams,
            FileDescriptorArray& /* aFileDescriptors */)
  {
    nsCOMPtr<nsIRemoteBlob> remote = do_QueryInterface(mSourceBlob);
    MOZ_ASSERT(remote);

    if (mOrigin == ParentActor) {
      MOZ_ASSERT(remote->GetBlobParent());

      aParams = RemoteInputStreamParams(
        remote->GetBlobParent() /* sourceParent */,
        nullptr /* sourceChild */);
    } else {
      MOZ_ASSERT(mOrigin == ChildActor);
      MOZ_ASSERT(remote->GetBlobChild());

      aParams = RemoteInputStreamParams(
        nullptr /* sourceParent */,
        remote->GetBlobChild() /* sourceChild */);
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
    AssertIsOnOwningThread();
    MOZ_ASSERT(aStream);

    nsCOMPtr<nsIInputStream> stream = aStream;
    nsCOMPtr<nsISeekableStream> seekableStream = do_QueryInterface(aStream);

    MOZ_ASSERT_IF(seekableStream, SameCOMIdentity(aStream, seekableStream));

    {
      MonitorAutoLock lock(mMonitor);

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

    nsRefPtr<mozilla::dom::DOMFileImplBase> sourceBlob;
    mSourceBlob.swap(sourceBlob);

    rv = mStream->Close();
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMETHOD
  Available(uint64_t* aAvailable) MOZ_OVERRIDE
  {
    // See large comment in FileInputStreamWrapper::Available.
    if (IsOnOwningThread()) {
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
  Tell(int64_t* aResult) MOZ_OVERRIDE
  {
    // We can cheat here and assume that we're going to start at 0 if we don't
    // yet have our stream. Though, really, this should abort since most input
    // streams could block here.
    if (IsOnOwningThread() && !mStream) {
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
  SetEOF() MOZ_OVERRIDE
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
    AssertIsOnOwningThread();

    nsresult rv = BlockAndWaitForStream();
    NS_ENSURE_SUCCESS(rv, nullptr);

    return mStream;
  }

private:
  virtual ~RemoteInputStream()
  {
    if (!IsOnOwningThread()) {
      mStream = nullptr;
      mWeakSeekableStream = nullptr;

      if (mSourceBlob) {
        ReleaseOnTarget(mSourceBlob, mEventTarget);
      }
    }
  }

  void
  ReallyBlockAndWaitForStream()
  {
    MOZ_ASSERT(!IsOnOwningThread());

    DebugOnly<bool> waited;

    {
      MonitorAutoLock lock(mMonitor);

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
    if (IsOnOwningThread()) {
      NS_WARNING("Blocking the owning thread is not supported!");
      return NS_ERROR_FAILURE;
    }

    ReallyBlockAndWaitForStream();

    return NS_OK;
  }

  bool
  IsSeekableStream()
  {
    if (IsOnOwningThread()) {
      if (!mStream) {
        NS_WARNING("Don't know if this stream is seekable yet!");
        return true;
      }
    } else {
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
  explicit InputStreamChild(RemoteInputStream* aRemoteStream)
  : mRemoteStream(aRemoteStream)
  {
    MOZ_ASSERT(aRemoteStream);
    aRemoteStream->AssertIsOnOwningThread();
  }

  InputStreamChild()
  { }

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
  explicit InputStreamParent(RemoteInputStream* aRemoteStream)
  : mRemoteStream(aRemoteStream)
  {
    MOZ_ASSERT(aRemoteStream);
    aRemoteStream->AssertIsOnOwningThread();
  }

  InputStreamParent()
  { }

protected:
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

private:
  // This method is only called by the IPDL message machinery.
  virtual bool
  Recv__delete__(const InputStreamParams& aParams,
                 const OptionalFileDescriptorSet& aFDs) MOZ_OVERRIDE;
};

} // anonymous namespace

// Each instance of this class will be dispatched to the network stream thread
// pool to run the first time where it will open the file input stream. It will
// then dispatch itself back to the owning thread to send the child process its
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
  nsCOMPtr<nsIEventTarget> mActorTarget;
  nsCOMPtr<nsIThread> mIOTarget;

  bool mRevoked;
  bool mClosing;

public:
  OpenStreamRunnable(BlobParent* aBlobActor,
                     PBlobStreamParent* aStreamActor,
                     nsIInputStream* aStream,
                     nsIIPCSerializableInputStream* aSerializable,
                     nsIThread* aIOTarget)
    : mBlobActor(aBlobActor)
    , mStreamActor(aStreamActor)
    , mStream(aStream)
    , mSerializable(aSerializable)
    , mIOTarget(aIOTarget)
    , mRevoked(false)
    , mClosing(false)
  {
    MOZ_ASSERT(aBlobActor);
    aBlobActor->AssertIsOnOwningThread();
    MOZ_ASSERT(aStreamActor);
    MOZ_ASSERT(aStream);
    // aSerializable may be null.
    MOZ_ASSERT(aIOTarget);

    if (!NS_IsMainThread()) {
      AssertIsOnBackgroundThread();

      mActorTarget = do_GetCurrentThread();
      MOZ_ASSERT(mActorTarget);
    }

    AssertIsOnOwningThread();
  }

  nsresult
  Dispatch()
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mIOTarget);

    nsresult rv = mIOTarget->Dispatch(this, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_DECL_ISUPPORTS_INHERITED

private:
  ~OpenStreamRunnable()
  { }

  bool
  IsOnOwningThread() const
  {
    return EventTargetIsOnCurrentThread(mActorTarget);
  }

  void
  AssertIsOnOwningThread() const
  {
    MOZ_ASSERT(IsOnOwningThread());
  }

  void
  Revoke()
  {
    AssertIsOnOwningThread();
#ifdef DEBUG
    mBlobActor = nullptr;
    mStreamActor = nullptr;
#endif
    mRevoked = true;
  }

  nsresult
  OpenStream()
  {
    MOZ_ASSERT(!IsOnOwningThread());
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

    if (mActorTarget) {
      nsresult rv = mActorTarget->Dispatch(this, NS_DISPATCH_NORMAL);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(this)));
    }

    return NS_OK;
  }

  nsresult
  CloseStream()
  {
    MOZ_ASSERT(!IsOnOwningThread());
    MOZ_ASSERT(mStream);

    // Going to always release here.
    nsCOMPtr<nsIInputStream> stream;
    mStream.swap(stream);

    nsCOMPtr<nsIThread> ioTarget;
    mIOTarget.swap(ioTarget);

    NS_WARN_IF_FALSE(NS_SUCCEEDED(stream->Close()), "Failed to close stream!");

    nsCOMPtr<nsIRunnable> shutdownRunnable =
      NS_NewRunnableMethod(ioTarget, &nsIThread::Shutdown);
    MOZ_ASSERT(shutdownRunnable);

    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(shutdownRunnable)));

    return NS_OK;
  }

  nsresult
  SendResponse()
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mStream);
    MOZ_ASSERT(mSerializable);
    MOZ_ASSERT(mIOTarget);
    MOZ_ASSERT(!mClosing);

    nsCOMPtr<nsIIPCSerializableInputStream> serializable;
    mSerializable.swap(serializable);

    if (mRevoked) {
      MOZ_ASSERT(!mBlobActor);
      MOZ_ASSERT(!mStreamActor);
    }
    else {
      MOZ_ASSERT(mBlobActor);
      MOZ_ASSERT(mBlobActor->HasManager());
      MOZ_ASSERT(mStreamActor);

      InputStreamParams params;
      nsAutoTArray<FileDescriptor, 10> fds;
      serializable->Serialize(params, fds);

      MOZ_ASSERT(params.type() != InputStreamParams::T__None);

      PFileDescriptorSetParent* fdSet;
      if (nsIContentParent* contentManager = mBlobActor->GetContentManager()) {
        fdSet = ConstructFileDescriptorSet(contentManager, fds);
      } else {
        fdSet = ConstructFileDescriptorSet(mBlobActor->GetBackgroundManager(),
                                           fds);
      }

      OptionalFileDescriptorSet optionalFDs;
      if (fdSet) {
        optionalFDs = fdSet;
      } else {
        optionalFDs = void_t();
      }

      unused <<
        PBlobStreamParent::Send__delete__(mStreamActor, params, optionalFDs);

      mBlobActor->NoteRunnableCompleted(this);

#ifdef DEBUG
      mBlobActor = nullptr;
      mStreamActor = nullptr;
#endif
    }

    // If our luck is *really* bad then it is possible for the CloseStream() and
    // nsIThread::Shutdown() functions to run before the Dispatch() call here
    // finishes... Keep the thread alive until this method returns.
    nsCOMPtr<nsIThread> kungFuDeathGrip = mIOTarget;

    mClosing = true;

    nsresult rv = mIOTarget->Dispatch(this, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMETHOD
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(mIOTarget);

    if (IsOnOwningThread()) {
      return SendResponse();
    }

    if (!mClosing) {
      return OpenStream();
    }

    return CloseStream();
  }
};

NS_IMPL_ISUPPORTS_INHERITED0(BlobParent::OpenStreamRunnable, nsRunnable)

/*******************************************************************************
 * BlobChild::RemoteBlob Declaration
 ******************************************************************************/

class BlobChild::RemoteBlob MOZ_FINAL : public DOMFileImplBase
                                      , public nsIRemoteBlob
{
  class StreamHelper;
  class SliceHelper;

  BlobChild* mActor;
  nsCOMPtr<nsIEventTarget> mActorTarget;

public:
  RemoteBlob(BlobChild* aActor,
             const nsAString& aName,
             const nsAString& aContentType,
             uint64_t aLength,
             uint64_t aModDate)
    : DOMFileImplBase(aName, aContentType, aLength, aModDate)
  {
    CommonInit(aActor);
  }

  RemoteBlob(BlobChild* aActor, const nsAString& aContentType, uint64_t aLength)
    : DOMFileImplBase(aContentType, aLength)
  {
    CommonInit(aActor);
  }

  RemoteBlob(BlobChild* aActor)
    : DOMFileImplBase(EmptyString(), EmptyString(), UINT64_MAX, UINT64_MAX)
  {
    CommonInit(aActor);
  }

  void
  NoteDyingActor()
  {
    MOZ_ASSERT(mActor);
    mActor->AssertIsOnOwningThread();

    mActor = nullptr;
  }

  NS_DECL_ISUPPORTS_INHERITED

  virtual nsresult
  GetMozFullPathInternal(nsAString &aFilePath) MOZ_OVERRIDE;

  virtual already_AddRefed<DOMFileImpl>
  CreateSlice(uint64_t aStart, uint64_t aLength, const nsAString& aContentType)
              MOZ_OVERRIDE;

  virtual nsresult
  GetInternalStream(nsIInputStream** aStream) MOZ_OVERRIDE;

  virtual int64_t
  GetFileId() MOZ_OVERRIDE;

  virtual nsresult
  GetLastModifiedDate(JSContext* cx,
                      JS::MutableHandle<JS::Value> aLastModifiedDate)
                      MOZ_OVERRIDE;

  virtual BlobChild*
  GetBlobChild() MOZ_OVERRIDE;

  virtual BlobParent*
  GetBlobParent() MOZ_OVERRIDE;

private:
  ~RemoteBlob()
  {
    MOZ_ASSERT_IF(mActorTarget,
                  EventTargetIsOnCurrentThread(mActorTarget));
  }

  void
  CommonInit(BlobChild* aActor)
  {
    MOZ_ASSERT(aActor);
    aActor->AssertIsOnOwningThread();

    mActor = aActor;
    mActorTarget = aActor->EventTarget();

    mImmutable = true;
  }

  void
  Destroy()
  {
    if (EventTargetIsOnCurrentThread(mActorTarget)) {
      if (mActor) {
        mActor->AssertIsOnOwningThread();
        mActor->NoteDyingRemoteBlob();
      }

      delete this;
      return;
    }

    nsCOMPtr<nsIRunnable> destroyRunnable =
      NS_NewNonOwningRunnableMethod(this, &RemoteBlob::Destroy);

    if (mActorTarget) {
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mActorTarget->Dispatch(destroyRunnable,
                                                          NS_DISPATCH_NORMAL)));
    } else {
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(destroyRunnable)));
    }
  }
};

class BlobChild::RemoteBlob::StreamHelper MOZ_FINAL
  : public nsRunnable
{
  Monitor mMonitor;
  BlobChild* mActor;
  nsRefPtr<mozilla::dom::DOMFileImplBase> mSourceBlob;
  nsRefPtr<RemoteInputStream> mInputStream;
  bool mDone;

public:
  StreamHelper(BlobChild* aActor, mozilla::dom::DOMFileImplBase* aSourceBlob)
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

    if (mActor->IsOnOwningThread()) {
      RunInternal(false);
    } else {
      nsCOMPtr<nsIEventTarget> target = mActor->EventTarget();
      if (!target) {
        target = do_GetMainThread();
      }

      MOZ_ASSERT(target);

      nsresult rv = target->Dispatch(this, NS_DISPATCH_NORMAL);
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
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(mActor);
    mActor->AssertIsOnOwningThread();

    RunInternal(true);
    return NS_OK;
  }

private:
  void
  RunInternal(bool aNotify)
  {
    MOZ_ASSERT(mActor);
    mActor->AssertIsOnOwningThread();
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
  Monitor mMonitor;
  BlobChild* mActor;
  nsRefPtr<DOMFileImpl> mSlice;
  uint64_t mStart;
  uint64_t mLength;
  nsString mContentType;
  bool mDone;

public:
  explicit SliceHelper(BlobChild* aActor)
    : mMonitor("BlobChild::RemoteBlob::SliceHelper::mMonitor")
    , mActor(aActor)
    , mStart(0)
    , mLength(0)
    , mDone(false)
  {
    // This may be created on any thread.
    MOZ_ASSERT(aActor);
  }

  DOMFileImpl*
  GetSlice(uint64_t aStart,
           uint64_t aLength,
           const nsAString& aContentType)
  {
    // This may be called on any thread.
    MOZ_ASSERT(mActor);
    MOZ_ASSERT(!mSlice);
    MOZ_ASSERT(!mDone);

    mStart = aStart;
    mLength = aLength;
    mContentType = aContentType;

    if (mActor->IsOnOwningThread()) {
      RunInternal(false);
    } else {
      nsCOMPtr<nsIEventTarget> target = mActor->EventTarget();
      if (!target) {
        target = do_GetMainThread();
      }

      MOZ_ASSERT(target);

      nsresult rv = target->Dispatch(this, NS_DISPATCH_NORMAL);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }

      {
        MonitorAutoLock lock(mMonitor);
        while (!mDone) {
          lock.Wait();
        }
      }
    }

    MOZ_ASSERT(!mActor);
    MOZ_ASSERT(mDone);

    if (NS_WARN_IF(!mSlice)) {
      return nullptr;
    }

    return mSlice;
  }

  NS_IMETHOD
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(mActor);
    mActor->AssertIsOnOwningThread();

    RunInternal(true);
    return NS_OK;
  }

private:
  void
  RunInternal(bool aNotify)
  {
    MOZ_ASSERT(mActor);
    mActor->AssertIsOnOwningThread();
    MOZ_ASSERT(!mSlice);
    MOZ_ASSERT(!mDone);

    NS_ENSURE_TRUE_VOID(mActor->HasManager());

    NormalBlobConstructorParams params(mContentType /* contentType */,
                                       mLength /* length */);

    ParentBlobConstructorParams otherSideParams(
      SlicedBlobConstructorParams(nullptr /* sourceParent */,
                                  mActor /* sourceChild */,
                                  mStart /* begin */,
                                  mStart + mLength /* end */,
                                  mContentType /* contentType */),
      void_t() /* optionalInputStream */);

    BlobChild* newActor;
    if (nsIContentChild* contentManager = mActor->GetContentManager()) {
      newActor = SendSliceConstructor(contentManager, params, otherSideParams);
    } else {
      newActor = SendSliceConstructor(mActor->GetBackgroundManager(),
                                      params,
                                      otherSideParams);
    }

    if (newActor) {
      nsCOMPtr<nsIDOMBlob> blob = newActor->GetBlob();
      mSlice = static_cast<DOMFile*>(blob.get())->Impl();
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

NS_IMPL_ADDREF(BlobChild::RemoteBlob)
NS_IMPL_RELEASE_WITH_DESTROY(BlobChild::RemoteBlob, Destroy())
NS_IMPL_QUERY_INTERFACE_INHERITED(BlobChild::RemoteBlob,
                                  DOMFileImpl,
                                  nsIRemoteBlob)

nsresult
BlobChild::
RemoteBlob::GetMozFullPathInternal(nsAString &aFilePath)
{
  if (!mActor) {
    return NS_ERROR_UNEXPECTED;
  }

  nsString filePath;
  if (!mActor->SendGetFilePath(&filePath)) {
    return NS_ERROR_FAILURE;
  }

  aFilePath = filePath;
  return NS_OK;
}

already_AddRefed<DOMFileImpl>
BlobChild::
RemoteBlob::CreateSlice(uint64_t aStart,
                        uint64_t aLength,
                        const nsAString& aContentType)
{
  if (!mActor) {
    return nullptr;
  }

  nsRefPtr<SliceHelper> helper = new SliceHelper(mActor);

  nsRefPtr<DOMFileImpl> impl = helper->GetSlice(aStart, aLength, aContentType);
  if (NS_WARN_IF(!impl)) {
    return nullptr;
  }

  return impl.forget();
}

nsresult
BlobChild::
RemoteBlob::GetInternalStream(nsIInputStream** aStream)
{
  if (!mActor) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<StreamHelper> helper = new StreamHelper(mActor, this);
  return helper->GetStream(aStream);
}

int64_t
BlobChild::
RemoteBlob::GetFileId()
{
  int64_t fileId;
  if (mActor && mActor->SendGetFileId(&fileId)) {
    return fileId;
  }

  return -1;
}

nsresult
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

BlobChild*
BlobChild::
RemoteBlob::GetBlobChild()
{
  return mActor;
}

BlobParent*
BlobChild::
RemoteBlob::GetBlobParent()
{
  return nullptr;
}

/*******************************************************************************
 * BlobChild
 ******************************************************************************/

BlobChild::BlobChild(nsIContentChild* aManager, nsIDOMBlob* aBlob)
  : mBlob(aBlob)
  , mRemoteBlob(nullptr)
  , mBackgroundManager(nullptr)
  , mContentManager(aManager)
  , mOwnsBlob(true)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aBlob);

  MOZ_COUNT_CTOR(BlobChild);

  CommonInit(aBlob);
}

BlobChild::BlobChild(PBackgroundChild* aManager, nsIDOMBlob* aBlob)
  : mBlob(aBlob)
  , mRemoteBlob(nullptr)
  , mBackgroundManager(aManager)
  , mContentManager(nullptr)
  , mOwnsBlob(true)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aBlob);

  MOZ_COUNT_CTOR(BlobChild);

  if (!NS_IsMainThread()) {
    mEventTarget = do_GetCurrentThread();
    MOZ_ASSERT(mEventTarget);
  }

  CommonInit(aBlob);
}

BlobChild::BlobChild(nsIContentChild* aManager,
                     const ChildBlobConstructorParams& aParams)
  : mBlob(nullptr)
  , mRemoteBlob(nullptr)
  , mBackgroundManager(nullptr)
  , mContentManager(aManager)
  , mOwnsBlob(false)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aParams.type() != ChildBlobConstructorParams::T__None);

  MOZ_COUNT_CTOR(BlobChild);

  CommonInit(aParams);
}

BlobChild::BlobChild(PBackgroundChild* aManager,
                     const ChildBlobConstructorParams& aParams)
  : mBlob(nullptr)
  , mRemoteBlob(nullptr)
  , mBackgroundManager(aManager)
  , mContentManager(nullptr)
  , mOwnsBlob(false)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aParams.type() != ChildBlobConstructorParams::T__None);

  MOZ_COUNT_CTOR(BlobChild);

  if (!NS_IsMainThread()) {
    mEventTarget = do_GetCurrentThread();
    MOZ_ASSERT(mEventTarget);
  }

  CommonInit(aParams);
}

BlobChild::~BlobChild()
{
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(BlobChild);
}

void
BlobChild::CommonInit(nsIDOMBlob* aBlob)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aBlob);
  MOZ_ASSERT(!mRemoteBlob);

  aBlob->AddRef();
}

void
BlobChild::CommonInit(const ChildBlobConstructorParams& aParams)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mRemoteBlob);

  ChildBlobConstructorParams::Type paramsType = aParams.type();
  MOZ_ASSERT(paramsType != ChildBlobConstructorParams::T__None);

  nsRefPtr<RemoteBlob> remoteBlob;

  switch (paramsType) {
    case ChildBlobConstructorParams::TNormalBlobConstructorParams: {
      const NormalBlobConstructorParams& params =
        aParams.get_NormalBlobConstructorParams();
      remoteBlob = new RemoteBlob(this, params.contentType(), params.length());
      break;
    }

    case ChildBlobConstructorParams::TFileBlobConstructorParams: {
      const FileBlobConstructorParams& params =
        aParams.get_FileBlobConstructorParams();
      remoteBlob = new RemoteBlob(this,
                                  params.name(),
                                  params.contentType(),
                                  params.length(),
                                  params.modDate());
      break;
    }

    case ChildBlobConstructorParams::TMysteryBlobConstructorParams: {
      remoteBlob = new RemoteBlob(this);
      break;
    }

    default:
      MOZ_CRASH("Unknown params!");
  }

  MOZ_ASSERT(remoteBlob);

  DebugOnly<bool> isMutable;
  MOZ_ASSERT(NS_SUCCEEDED(remoteBlob->GetMutable(&isMutable)));
  MOZ_ASSERT(!isMutable);

  nsRefPtr<DOMFile> blob = new DOMFile(remoteBlob);
  blob.forget(&mBlob);

  mRemoteBlob = remoteBlob;
  mOwnsBlob = true;
}

#ifdef DEBUG

void
BlobChild::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(IsOnOwningThread());
}

#endif // DEBUG

// static
BlobChild*
BlobChild::Create(nsIContentChild* aManager,
                  const ChildBlobConstructorParams& aParams)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  return CreateFromParams(aManager, aParams);
}

// static
BlobChild*
BlobChild::Create(PBackgroundChild* aManager,
                  const ChildBlobConstructorParams& aParams)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  return CreateFromParams(aManager, aParams);
}

// static
template <class ChildManagerType>
BlobChild*
BlobChild::CreateFromParams(ChildManagerType* aManager,
                            const ChildBlobConstructorParams& aParams)
{
  AssertCorrectThreadForManager(aManager);
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

// static
template <class ChildManagerType>
BlobChild*
BlobChild::SendSliceConstructor(
                            ChildManagerType* aManager,
                            const NormalBlobConstructorParams& aParams,
                            const ParentBlobConstructorParams& aOtherSideParams)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  BlobChild* newActor = BlobChild::Create(aManager, aParams);
  MOZ_ASSERT(newActor);

  if (aManager->SendPBlobConstructor(newActor, aOtherSideParams)) {
    return newActor;
  }

  return nullptr;
}

already_AddRefed<nsIDOMBlob>
BlobChild::GetBlob()
{
  AssertIsOnOwningThread();
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

already_AddRefed<DOMFileImpl>
BlobChild::GetBlobImpl()
{
  AssertIsOnOwningThread();

  nsCOMPtr<nsIDOMBlob> blob = GetBlob();
  MOZ_ASSERT(blob);

  nsRefPtr<DOMFileImpl> impl = static_cast<DOMFile*>(blob.get())->Impl();
  MOZ_ASSERT(impl);

  return impl.forget();
}

bool
BlobChild::SetMysteryBlobInfo(const nsString& aName,
                              const nsString& aContentType,
                              uint64_t aLength,
                              uint64_t aLastModifiedDate)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(mRemoteBlob);
  MOZ_ASSERT(aLastModifiedDate != UINT64_MAX);

  static_cast<DOMFile*>(mBlob)->SetLazyData(aName, aContentType, aLength,
                                            aLastModifiedDate);

  FileBlobConstructorParams params(aName, aContentType, aLength,
                                   aLastModifiedDate);
  return SendResolveMystery(params);
}

bool
BlobChild::SetMysteryBlobInfo(const nsString& aContentType, uint64_t aLength)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(mRemoteBlob);

  nsString voidString;
  voidString.SetIsVoid(true);

  static_cast<DOMFile*>(mBlob)->SetLazyData(voidString, aContentType, aLength,
                                            UINT64_MAX);

  NormalBlobConstructorParams params(aContentType, aLength);
  return SendResolveMystery(params);
}

void
BlobChild::NoteDyingRemoteBlob()
{
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(mRemoteBlob);
  MOZ_ASSERT(!mOwnsBlob);

  // This may be called on any thread due to the fact that RemoteBlob is
  // designed to be passed between threads. We must start the shutdown process
  // on the owning thread, so we proxy here if necessary.
  if (!IsOnOwningThread()) {
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewNonOwningRunnableMethod(this, &BlobChild::NoteDyingRemoteBlob);

    if (mEventTarget) {
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mEventTarget->Dispatch(runnable,
                                                          NS_DISPATCH_NORMAL)));
    } else {
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(runnable)));
    }

    return;
  }

  // Must do this before calling Send__delete__ or we'll crash there trying to
  // access a dangling pointer.
  mBlob = nullptr;
  mRemoteBlob = nullptr;

  PBlobChild::Send__delete__(this);
}

bool
BlobChild::IsOnOwningThread() const
{
  return EventTargetIsOnCurrentThread(mEventTarget);
}

void
BlobChild::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  if (mRemoteBlob) {
    mRemoteBlob->NoteDyingActor();
  }

  if (mBlob && mOwnsBlob) {
    mBlob->Release();
  }

  mBackgroundManager = nullptr;
  mContentManager = nullptr;
}

PBlobStreamChild*
BlobChild::AllocPBlobStreamChild()
{
  AssertIsOnOwningThread();

  return new InputStreamChild();
}

bool
BlobChild::RecvPBlobStreamConstructor(PBlobStreamChild* aActor)
{
  AssertIsOnOwningThread();
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

  return aActor->Send__delete__(aActor, params, void_t());
}

bool
BlobChild::DeallocPBlobStreamChild(PBlobStreamChild* aActor)
{
  AssertIsOnOwningThread();

  delete static_cast<InputStreamChild*>(aActor);
  return true;
}

/*******************************************************************************
 * BlobParent::RemoteBlob Declaration
 ******************************************************************************/

class BlobParent::RemoteBlob MOZ_FINAL : public mozilla::dom::DOMFileImplBase
                                       , public nsIRemoteBlob
{
  class StreamHelper;
  class SliceHelper;

  BlobParent* mActor;
  nsCOMPtr<nsIEventTarget> mActorTarget;
  InputStreamParams mInputStreamParams;

public:
  RemoteBlob(BlobParent* aActor,
             const OptionalInputStreamParams& aOptionalInputStreamParams,
             const nsAString& aName,
             const nsAString& aContentType,
             uint64_t aLength,
             uint64_t aModDate)
    : DOMFileImplBase(aName, aContentType, aLength, aModDate)
  {
    CommonInit(aActor, aOptionalInputStreamParams);
  }

  RemoteBlob(BlobParent* aActor,
             const OptionalInputStreamParams& aOptionalInputStreamParams,
             const nsAString& aContentType,
             uint64_t aLength)
    : DOMFileImplBase(aContentType, aLength)
  {
    CommonInit(aActor, aOptionalInputStreamParams);
  }

  RemoteBlob(BlobParent* aActor,
             const OptionalInputStreamParams& aOptionalInputStreamParams)
    : DOMFileImplBase(EmptyString(), EmptyString(), UINT64_MAX, UINT64_MAX)
  {
    CommonInit(aActor, aOptionalInputStreamParams);
  }

  void
  NoteDyingActor()
  {
    MOZ_ASSERT(mActor);
    mActor->AssertIsOnOwningThread();

    mActor = nullptr;
  }

  NS_DECL_ISUPPORTS_INHERITED

  virtual already_AddRefed<DOMFileImpl>
  CreateSlice(uint64_t aStart, uint64_t aLength, const nsAString& aContentType)
              MOZ_OVERRIDE;

  virtual nsresult
  GetInternalStream(nsIInputStream** aStream) MOZ_OVERRIDE;

  virtual nsresult
  GetLastModifiedDate(JSContext* cx,
                      JS::MutableHandle<JS::Value> aLastModifiedDate)
                      MOZ_OVERRIDE;

  virtual BlobChild*
  GetBlobChild() MOZ_OVERRIDE;

  virtual BlobParent*
  GetBlobParent() MOZ_OVERRIDE;

private:
  ~RemoteBlob()
  {
    MOZ_ASSERT_IF(mActorTarget,
                  EventTargetIsOnCurrentThread(mActorTarget));
  }

  void
  CommonInit(BlobParent* aActor,
             const OptionalInputStreamParams& aOptionalInputStreamParams)
  {
    MOZ_ASSERT(aActor);
    aActor->AssertIsOnOwningThread();
    MOZ_ASSERT(aOptionalInputStreamParams.type() !=
                 OptionalInputStreamParams::T__None);

    mActor = aActor;
    mActorTarget = aActor->EventTarget();

    if (aOptionalInputStreamParams.type() ==
          OptionalInputStreamParams::TInputStreamParams) {
      mInputStreamParams = aOptionalInputStreamParams.get_InputStreamParams();
    }

    mImmutable = true;
  }

  void
  Destroy()
  {
    if (EventTargetIsOnCurrentThread(mActorTarget)) {
      if (mActor) {
        mActor->AssertIsOnOwningThread();
        mActor->NoteDyingRemoteBlob();
      }

      delete this;
      return;
    }

    nsCOMPtr<nsIRunnable> destroyRunnable =
      NS_NewNonOwningRunnableMethod(this, &RemoteBlob::Destroy);

    if (mActorTarget) {
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mActorTarget->Dispatch(destroyRunnable,
                                                          NS_DISPATCH_NORMAL)));
    } else {
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(destroyRunnable)));
    }
  }
};

class BlobParent::RemoteBlob::StreamHelper MOZ_FINAL
  : public nsRunnable
{
  Monitor mMonitor;
  BlobParent* mActor;
  nsRefPtr<mozilla::dom::DOMFileImplBase> mSourceBlob;
  nsRefPtr<RemoteInputStream> mInputStream;
  bool mDone;

public:
  StreamHelper(BlobParent* aActor, mozilla::dom::DOMFileImplBase* aSourceBlob)
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

    if (mActor->IsOnOwningThread()) {
      RunInternal(false);
    } else {
      nsCOMPtr<nsIEventTarget> target = mActor->EventTarget();
      if (!target) {
        target = do_GetMainThread();
      }

      MOZ_ASSERT(target);

      nsresult rv = target->Dispatch(this, NS_DISPATCH_NORMAL);
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
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(mActor);
    mActor->AssertIsOnOwningThread();

    RunInternal(true);
    return NS_OK;
  }

private:
  void
  RunInternal(bool aNotify)
  {
    MOZ_ASSERT(mActor);
    mActor->AssertIsOnOwningThread();
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
  Monitor mMonitor;
  BlobParent* mActor;
  nsRefPtr<DOMFileImpl> mSlice;
  uint64_t mStart;
  uint64_t mLength;
  nsString mContentType;
  bool mDone;

public:
  explicit SliceHelper(BlobParent* aActor)
    : mMonitor("BlobParent::RemoteBlob::SliceHelper::mMonitor")
    , mActor(aActor)
    , mStart(0)
    , mLength(0)
    , mDone(false)
  {
    // This may be created on any thread.
    MOZ_ASSERT(aActor);
  }

  DOMFileImpl*
  GetSlice(uint64_t aStart,
           uint64_t aLength,
           const nsAString& aContentType)
  {
    // This may be called on any thread.
    MOZ_ASSERT(mActor);
    MOZ_ASSERT(!mSlice);
    MOZ_ASSERT(!mDone);

    mStart = aStart;
    mLength = aLength;
    mContentType = aContentType;

    if (mActor->IsOnOwningThread()) {
      RunInternal(false);
    } else {
      nsCOMPtr<nsIEventTarget> target = mActor->EventTarget();
      if (!target) {
        target = do_GetMainThread();
      }

      MOZ_ASSERT(target);

      nsresult rv = target->Dispatch(this, NS_DISPATCH_NORMAL);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }

      {
        MonitorAutoLock lock(mMonitor);
        while (!mDone) {
          lock.Wait();
        }
      }
    }

    MOZ_ASSERT(!mActor);
    MOZ_ASSERT(mDone);

    if (NS_WARN_IF(!mSlice)) {
      return nullptr;
    }

    return mSlice;
  }

  NS_IMETHOD
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(mActor);
    mActor->AssertIsOnOwningThread();

    RunInternal(true);
    return NS_OK;
  }

private:
  void
  RunInternal(bool aNotify)
  {
    MOZ_ASSERT(mActor);
    mActor->AssertIsOnOwningThread();
    MOZ_ASSERT(!mSlice);
    MOZ_ASSERT(!mDone);

    NS_ENSURE_TRUE_VOID(mActor->HasManager());

    ParentBlobConstructorParams params(
      NormalBlobConstructorParams(mContentType /* contentType */,
                                  mLength /* length */),
      void_t() /* optionalInputStreamParams */);

    ChildBlobConstructorParams otherSideParams(
      SlicedBlobConstructorParams(mActor /* sourceParent*/,
                                  nullptr /* sourceChild */,
                                  mStart /* begin */,
                                  mStart + mLength /* end */,
                                  mContentType /* contentType */));

    BlobParent* newActor;
    if (nsIContentParent* contentManager = mActor->GetContentManager()) {
      newActor = SendSliceConstructor(contentManager, params, otherSideParams);
    } else {
      newActor = SendSliceConstructor(mActor->GetBackgroundManager(),
                                      params,
                                      otherSideParams);
    }

    if (newActor) {
      nsCOMPtr<nsIDOMBlob> blob =newActor->GetBlob();
      mSlice = static_cast<DOMFile*>(blob.get())->Impl();
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

NS_IMPL_ADDREF(BlobParent::RemoteBlob)
NS_IMPL_RELEASE_WITH_DESTROY(BlobParent::RemoteBlob, Destroy())
NS_IMPL_QUERY_INTERFACE_INHERITED(BlobParent::RemoteBlob,
                                  DOMFileImpl,
                                  nsIRemoteBlob)

already_AddRefed<DOMFileImpl>
BlobParent::
RemoteBlob::CreateSlice(uint64_t aStart,
                        uint64_t aLength,
                        const nsAString& aContentType)
{
  if (!mActor) {
    return nullptr;
  }

  nsRefPtr<SliceHelper> helper = new SliceHelper(mActor);

  nsRefPtr<DOMFileImpl> impl = helper->GetSlice(aStart, aLength, aContentType);
  if (NS_WARN_IF(!impl)) {
    return nullptr;
  }

  return impl.forget();
}

nsresult
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

nsresult
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

BlobChild*
BlobParent::
RemoteBlob::GetBlobChild()
{
  return nullptr;
}

BlobParent*
BlobParent::
RemoteBlob::GetBlobParent()
{
  return mActor;
}

/*******************************************************************************
 * BlobParent
 ******************************************************************************/

BlobParent::BlobParent(nsIContentParent* aManager, nsIDOMBlob* aBlob)
  : mBlob(aBlob)
  , mRemoteBlob(nullptr)
  , mBackgroundManager(nullptr)
  , mContentManager(aManager)
  , mOwnsBlob(true)
  , mOwnsRemoteBlob(false)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aBlob);

  MOZ_COUNT_CTOR(BlobParent);

  CommonInit(aBlob);
}

BlobParent::BlobParent(PBackgroundParent* aManager, nsIDOMBlob* aBlob)
  : mBlob(aBlob)
  , mRemoteBlob(nullptr)
  , mBackgroundManager(aManager)
  , mContentManager(nullptr)
  , mEventTarget(do_GetCurrentThread())
  , mOwnsBlob(true)
  , mOwnsRemoteBlob(false)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aBlob);
  MOZ_ASSERT(mEventTarget);

  MOZ_COUNT_CTOR(BlobParent);

  CommonInit(aBlob);
}

BlobParent::BlobParent(PBackgroundParent* aManager, DOMFileImpl* aBlobImpl)
  : mBlob(nullptr)
  , mRemoteBlob(nullptr)
  , mBackgroundManager(aManager)
  , mContentManager(nullptr)
  , mBlobImpl(aBlobImpl)
  , mEventTarget(do_GetCurrentThread())
  , mOwnsBlob(false)
  , mOwnsRemoteBlob(false)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aBlobImpl);
  MOZ_ASSERT(mEventTarget);

  MOZ_COUNT_CTOR(BlobParent);
}

BlobParent::BlobParent(nsIContentParent* aManager,
                       const ParentBlobConstructorParams& aParams)
  : mBlob(nullptr)
  , mRemoteBlob(nullptr)
  , mBackgroundManager(nullptr)
  , mContentManager(aManager)
  , mOwnsBlob(false)
  , mOwnsRemoteBlob(false)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aParams.blobParams().type() !=
             ChildBlobConstructorParams::T__None);

  MOZ_COUNT_CTOR(BlobParent);

  CommonInit(aParams);
}

BlobParent::BlobParent(PBackgroundParent* aManager,
                       const ParentBlobConstructorParams& aParams)
  : mBlob(nullptr)
  , mRemoteBlob(nullptr)
  , mBackgroundManager(aManager)
  , mContentManager(nullptr)
  , mEventTarget(do_GetCurrentThread())
  , mOwnsBlob(false)
  , mOwnsRemoteBlob(false)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aParams.blobParams().type() !=
             ChildBlobConstructorParams::T__None);
  MOZ_ASSERT(mEventTarget);

  MOZ_COUNT_CTOR(BlobParent);

  CommonInit(aParams);
}

BlobParent::~BlobParent()
{
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(BlobParent);
}

void
BlobParent::CommonInit(nsIDOMBlob* aBlob)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aBlob);

  aBlob->AddRef();
}

void
BlobParent::CommonInit(const ParentBlobConstructorParams& aParams)
{
  AssertIsOnOwningThread();

  const ChildBlobConstructorParams& blobParams = aParams.blobParams();

  ChildBlobConstructorParams::Type paramsType = blobParams.type();
  MOZ_ASSERT(paramsType != ChildBlobConstructorParams::T__None);

  nsRefPtr<RemoteBlob> remoteBlob;

  switch (paramsType) {
    case ChildBlobConstructorParams::TNormalBlobConstructorParams: {
      const NormalBlobConstructorParams& params =
        blobParams.get_NormalBlobConstructorParams();
      remoteBlob = new RemoteBlob(this,
                                  aParams.optionalInputStreamParams(),
                                  params.contentType(),
                                  params.length());
      break;
    }

    case ChildBlobConstructorParams::TFileBlobConstructorParams: {
      const FileBlobConstructorParams& params =
        blobParams.get_FileBlobConstructorParams();
      remoteBlob = new RemoteBlob(this,
                                  aParams.optionalInputStreamParams(),
                                  params.name(),
                                  params.contentType(),
                                  params.length(),
                                  params.modDate());
      break;
    }

    case ChildBlobConstructorParams::TMysteryBlobConstructorParams: {
      remoteBlob = new RemoteBlob(this, aParams.optionalInputStreamParams());
      break;
    }

    default:
      MOZ_CRASH("Unknown params!");
  }

  MOZ_ASSERT(remoteBlob);

  DebugOnly<bool> isMutable;
  MOZ_ASSERT(NS_SUCCEEDED(remoteBlob->GetMutable(&isMutable)));
  MOZ_ASSERT(!isMutable);

  remoteBlob.forget(&mRemoteBlob);
  mOwnsRemoteBlob = true;
}

#ifdef DEBUG

void
BlobParent::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(IsOnOwningThread());
}

#endif // DEBUG

// static
BlobParent*
BlobParent::Create(nsIContentParent* aManager,
                   const ParentBlobConstructorParams& aParams)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  return CreateFromParams(aManager, aParams);
}

// static
BlobParent*
BlobParent::Create(PBackgroundParent* aManager,
                   const ParentBlobConstructorParams& aParams)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  return CreateFromParams(aManager, aParams);
}

// static
BlobParent*
BlobParent::CreateFromParams(nsIContentParent* aManager,
                             const ParentBlobConstructorParams& aParams)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  const ChildBlobConstructorParams& blobParams = aParams.blobParams();

  MOZ_ASSERT(blobParams.type() !=
             ChildBlobConstructorParams::TMysteryBlobConstructorParams);

  switch (blobParams.type()) {
    case ChildBlobConstructorParams::TMysteryBlobConstructorParams:
      ASSERT_UNLESS_FUZZING();
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

      nsRefPtr<DOMFileImpl> source = actor->GetBlobImpl();
      MOZ_ASSERT(source);

      nsRefPtr<DOMFileImpl> slicedImpl;
      nsresult rv =
        source->Slice(params.begin(), params.end(), params.contentType(), 3,
                      getter_AddRefs(slicedImpl));
      NS_ENSURE_SUCCESS(rv, nullptr);

      nsRefPtr<DOMFile> slice = new DOMFile(slicedImpl);

      return new BlobParent(aManager, slice);
    }

    default:
      MOZ_CRASH("Unknown params!");
  }

  MOZ_CRASH("Should never get here!");
}

BlobParent*
BlobParent::CreateFromParams(PBackgroundParent* aManager,
                             const ParentBlobConstructorParams& aParams)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  const ChildBlobConstructorParams& blobParams = aParams.blobParams();

  MOZ_ASSERT(blobParams.type() !=
             ChildBlobConstructorParams::TMysteryBlobConstructorParams);

  switch (blobParams.type()) {
    case ChildBlobConstructorParams::TMysteryBlobConstructorParams:
      ASSERT_UNLESS_FUZZING();
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

      nsRefPtr<DOMFileImpl> source = actor->GetBlobImpl();
      MOZ_ASSERT(source);

      nsRefPtr<DOMFileImpl> slice;
      nsresult rv =
        source->Slice(params.begin(), params.end(), params.contentType(), 3,
                      getter_AddRefs(slice));
      NS_ENSURE_SUCCESS(rv, nullptr);

      return new BlobParent(aManager, slice);
    }

    default:
      MOZ_CRASH("Unknown params!");
  }

  MOZ_CRASH("Should never get here!");
}

// static
template <class ParentManagerType>
BlobParent*
BlobParent::SendSliceConstructor(
                             ParentManagerType* aManager,
                             const ParentBlobConstructorParams& aParams,
                             const ChildBlobConstructorParams& aOtherSideParams)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  BlobParent* newActor = BlobParent::Create(aManager, aParams);
  MOZ_ASSERT(newActor);

  if (aManager->SendPBlobConstructor(newActor, aOtherSideParams)) {
    return newActor;
  }

  return nullptr;
}

already_AddRefed<nsIDOMBlob>
BlobParent::GetBlob()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mContentManager,
             "Use GetBlobImpl() instead for background-managed blobs");

  nsCOMPtr<nsIDOMBlob> blob = mBlob;
  if (blob) {
    return blob.forget();
  }

  MOZ_ASSERT(mRemoteBlob);
  MOZ_ASSERT(!mOwnsBlob);
  MOZ_ASSERT(mOwnsRemoteBlob);

  nsRefPtr<DOMFileImpl> impl = GetBlobImpl();
  MOZ_ASSERT(impl);
  MOZ_ASSERT(impl == mRemoteBlob);

  MOZ_ASSERT(!mOwnsRemoteBlob);

  blob = new DOMFile(impl);
  mBlob = blob;

  return blob.forget();
}

already_AddRefed<DOMFileImpl>
BlobParent::GetBlobImpl()
{
  AssertIsOnOwningThread();

  nsRefPtr<DOMFileImpl> impl;
  if (mBlobImpl) {
    // If we own a DOMFileImpl then just hand it out.
    impl = mBlobImpl;
  } else if (mBlob) {
    // If we already have a blob (either a normal blob that we always own or a
    // remote blob that we created) then just return its impl.
    nsCOMPtr<nsIDOMBlob> blob = mBlob;
    impl = static_cast<DOMFile*>(blob.get())->Impl();

    // Make sure the impl hasn't changed somehow if this is a remote blob that
    // we created.
    MOZ_ASSERT_IF(mRemoteBlob, impl == mRemoteBlob);
  } else if (mOwnsRemoteBlob) {
    // If this is a remote blob and this is the first time a caller is asking
    // for the impl then we drop our strong reference and only hold a weak
    // reference hereafter.
    impl = dont_AddRef(mRemoteBlob);
    mOwnsRemoteBlob = false;
  } else {
    // Hand out our weak reference.
    impl = mRemoteBlob;
  }

  MOZ_ASSERT(impl);

  return impl.forget();
}

void
BlobParent::NoteDyingRemoteBlob()
{
  MOZ_ASSERT(mRemoteBlob);
  MOZ_ASSERT(!mOwnsBlob);
  MOZ_ASSERT(!mOwnsRemoteBlob);

  // This may be called on any thread due to the fact that RemoteBlob is
  // designed to be passed between threads. We must start the shutdown process
  // on the main thread, so we proxy here if necessary.
  if (!IsOnOwningThread()) {
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewNonOwningRunnableMethod(this, &BlobParent::NoteDyingRemoteBlob);

    if (mEventTarget) {
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mEventTarget->Dispatch(runnable,
                                                          NS_DISPATCH_NORMAL)));
    } else {
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(runnable)));
    }

    return;
  }

  // Must do this before calling Send__delete__ or we'll crash there trying to
  // access a dangling pointer.
  mBlob = nullptr;
  mRemoteBlob = nullptr;

  mBlobImpl = nullptr;

  unused << PBlobParent::Send__delete__(this);
}

void
BlobParent::NoteRunnableCompleted(OpenStreamRunnable* aRunnable)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aRunnable);

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

bool
BlobParent::IsOnOwningThread() const
{
  return EventTargetIsOnCurrentThread(mEventTarget);
}

void
BlobParent::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  if (mRemoteBlob) {
    mRemoteBlob->NoteDyingActor();
  }

  if (mBlob && mOwnsBlob) {
    mBlob->Release();
  }

  if (mRemoteBlob && mOwnsRemoteBlob) {
    mRemoteBlob->Release();
  }

  mBlobImpl = nullptr;

  mBackgroundManager = nullptr;
  mContentManager = nullptr;
}

PBlobStreamParent*
BlobParent::AllocPBlobStreamParent()
{
  AssertIsOnOwningThread();

  return new InputStreamParent();
}

bool
BlobParent::RecvPBlobStreamConstructor(PBlobStreamParent* aActor)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT_IF(mBlob, !mBlobImpl);
  MOZ_ASSERT_IF(!mBlob, mBlobImpl);
  MOZ_ASSERT(!mRemoteBlob);

  nsRefPtr<DOMFile> blob = static_cast<DOMFile*>(mBlob);

  nsRefPtr<DOMFileImpl> impl;
  if (blob) {
    impl = blob->Impl();
  } else {
    impl = mBlobImpl;
  }

  MOZ_ASSERT(impl);

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = impl->GetInternalStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, false);

  // If the stream is entirely backed by memory then we can serialize and send
  // it immediately.
  if (impl->IsMemoryFile()) {
    InputStreamParams params;
    nsTArray<FileDescriptor> fds;
    SerializeInputStream(stream, params, fds);

    MOZ_ASSERT(params.type() != InputStreamParams::T__None);
    MOZ_ASSERT(fds.IsEmpty());

    return PBlobStreamParent::Send__delete__(aActor, params, void_t());
  }

  nsCOMPtr<nsIRemoteBlob> remoteBlob = do_QueryInterface(impl);
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
      remoteBlob->GetBlobParent() == this ||
      !remoteStream) {
    serializableStream = do_QueryInterface(stream);
    if (!serializableStream) {
      MOZ_ASSERT(false, "Must be serializable!");
      return false;
    }
  }

  nsCOMPtr<nsIThread> target;
  rv = NS_NewNamedThread("Blob Opener", getter_AddRefs(target));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

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
  AssertIsOnOwningThread();

  delete static_cast<InputStreamParent*>(aActor);
  return true;
}

bool
BlobParent::RecvResolveMystery(const ResolveMysteryParams& aParams)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() != ResolveMysteryParams::T__None);
  MOZ_ASSERT_IF(mBlob, !mBlobImpl);
  MOZ_ASSERT_IF(mBlob, mOwnsBlob);
  MOZ_ASSERT_IF(mBlobImpl, !mBlob);
  MOZ_ASSERT_IF(mBlobImpl, mBackgroundManager);
  MOZ_ASSERT(!mRemoteBlob);
  MOZ_ASSERT(!mOwnsRemoteBlob);

  nsRefPtr<DOMFileImpl> blobImpl;
  if (mBlob) {
    blobImpl = static_cast<DOMFile*>(mBlob)->Impl();
  } else {
    blobImpl = mBlobImpl;
  }

  MOZ_ASSERT(blobImpl);

  switch (aParams.type()) {
    case ResolveMysteryParams::TNormalBlobConstructorParams: {
      const NormalBlobConstructorParams& params =
        aParams.get_NormalBlobConstructorParams();

      if (NS_WARN_IF(params.length() == UINT64_MAX)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      nsString voidString;
      voidString.SetIsVoid(true);

      blobImpl->SetLazyData(voidString,
                            params.contentType(),
                            params.length(),
                            UINT64_MAX);
      return true;
    }

    case ResolveMysteryParams::TFileBlobConstructorParams: {
      const FileBlobConstructorParams& params =
        aParams.get_FileBlobConstructorParams();
      if (NS_WARN_IF(params.name().IsVoid())) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      if (NS_WARN_IF(params.length() == UINT64_MAX)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      if (NS_WARN_IF(params.modDate() == UINT64_MAX)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      blobImpl->SetLazyData(params.name(),
                            params.contentType(),
                            params.length(),
                            params.modDate());
      return true;
    }

    default:
      MOZ_CRASH("Unknown params!");
  }

  MOZ_CRASH("Should never get here!");
}

bool
BlobParent::RecvGetFileId(int64_t* aFileId)
{
  using namespace mozilla::dom::indexedDB;

  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(mBlob, !mBlobImpl);
  MOZ_ASSERT_IF(!mBlob, mBlobImpl);
  MOZ_ASSERT(!mRemoteBlob);

  if (NS_WARN_IF(!IndexedDatabaseManager::InTestingMode())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  nsRefPtr<DOMFileImpl> blobImpl;
  if (mBlob) {
    blobImpl = static_cast<DOMFile*>(mBlob)->Impl();
  } else {
    blobImpl = mBlobImpl;
  }

  MOZ_ASSERT(blobImpl);

  *aFileId = blobImpl->GetFileId();
  return true;
}

bool
BlobParent::RecvGetFilePath(nsString* aFilePath)
{
  using namespace mozilla::dom::indexedDB;

  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(mBlob, !mBlobImpl);
  MOZ_ASSERT_IF(!mBlob, mBlobImpl);
  MOZ_ASSERT(!mRemoteBlob);

  if (NS_WARN_IF(!IndexedDatabaseManager::InTestingMode())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  nsRefPtr<DOMFileImpl> blobImpl;
  if (mBlob) {
    blobImpl = static_cast<DOMFile*>(mBlob)->Impl();
  } else {
    blobImpl = mBlobImpl;
  }

  MOZ_ASSERT(blobImpl);

  nsString filePath;
  nsresult rv = blobImpl->GetMozFullPathInternal(filePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  *aFilePath = filePath;
  return true;
}

bool
InputStreamChild::Recv__delete__(const InputStreamParams& aParams,
                                 const OptionalFileDescriptorSet& aFDs)
{
  MOZ_ASSERT(mRemoteStream);
  mRemoteStream->AssertIsOnOwningThread();

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
  MOZ_ASSERT(stream);

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
  MOZ_ASSERT(mRemoteStream);
  mRemoteStream->AssertIsOnOwningThread();

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
