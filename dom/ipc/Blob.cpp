/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"

#include "base/basictypes.h"

#include "Blob.h"

#include "nsIDOMFile.h"
#include "nsIInputStream.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsIRemoteBlob.h"
#include "nsISeekableStream.h"

#include "mozilla/Monitor.h"
#include "mozilla/unused.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "nsDOMFile.h"
#include "nsThreadUtils.h"

#include "ContentChild.h"
#include "ContentParent.h"

#define PRIVATE_REMOTE_INPUT_STREAM_IID \
  {0x30c7699f, 0x51d2, 0x48c8, {0xad, 0x56, 0xc0, 0x16, 0xd7, 0x6f, 0x71, 0x27}}

using namespace mozilla::dom;
using namespace mozilla::dom::ipc;
using namespace mozilla::ipc;

namespace {

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

class RemoteInputStream : public nsIInputStream,
                          public nsISeekableStream,
                          public nsIIPCSerializableInputStream,
                          public IPrivateRemoteInputStream
{
  mozilla::Monitor mMonitor;
  nsCOMPtr<nsIDOMBlob> mSourceBlob;
  nsCOMPtr<nsIInputStream> mStream;
  nsCOMPtr<nsISeekableStream> mSeekableStream;
  ActorFlavorEnum mOrigin;

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  RemoteInputStream(nsIDOMBlob* aSourceBlob, ActorFlavorEnum aOrigin)
  : mMonitor("RemoteInputStream.mMonitor"), mSourceBlob(aSourceBlob),
    mOrigin(aOrigin)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aSourceBlob);
  }

  void
  Serialize(InputStreamParams& aParams)
  {
    nsCOMPtr<nsIRemoteBlob> remote = do_QueryInterface(mSourceBlob);
    MOZ_ASSERT(remote);

    if (mOrigin == Parent) {
      aParams = RemoteInputStreamParams(
        static_cast<PBlobParent*>(remote->GetPBlob()), nullptr);
    } else {
      aParams = RemoteInputStreamParams(
        nullptr, static_cast<PBlobChild*>(remote->GetPBlob()));
    }
  }

  bool
  Deserialize(const InputStreamParams& aParams)
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

    {
      mozilla::MonitorAutoLock lock(mMonitor);

      MOZ_ASSERT(!mStream);
      MOZ_ASSERT(!mSeekableStream);

      mStream.swap(stream);
      mSeekableStream.swap(seekableStream);

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

    if (!mSeekableStream) {
      NS_WARNING("Underlying blob stream is not seekable!");
      return NS_ERROR_NO_INTERFACE;
    }

    rv = mSeekableStream->Seek(aWhence, aOffset);
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

    if (!mSeekableStream) {
      NS_WARNING("Underlying blob stream is not seekable!");
      return NS_ERROR_NO_INTERFACE;
    }

    rv = mSeekableStream->Tell(aResult);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMETHOD
  SetEOF()
  {
    nsresult rv = BlockAndWaitForStream();
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mSeekableStream) {
      NS_WARNING("Underlying blob stream is not seekable!");
      return NS_ERROR_NO_INTERFACE;
    }

    rv = mSeekableStream->SetEOF();
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
  { }

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
    if (waited && mSeekableStream) {
      int64_t position;
      MOZ_ASSERT(NS_SUCCEEDED(mSeekableStream->Tell(&position)),
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

    return !!mSeekableStream;
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

template <ActorFlavorEnum ActorFlavor>
class InputStreamActor : public BlobTraits<ActorFlavor>::StreamType
{
  nsRefPtr<RemoteInputStream> mRemoteStream;

public:
  InputStreamActor(RemoteInputStream* aRemoteStream)
  : mRemoteStream(aRemoteStream)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aRemoteStream);
  }

  InputStreamActor()
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

private:
  // This method is only called by the IPDL message machinery.
  virtual bool
  Recv__delete__(const InputStreamParams& aParams) MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mRemoteStream);

    nsCOMPtr<nsIInputStream> stream = DeserializeInputStream(aParams);
    if (!stream) {
      return false;
    }

    mRemoteStream->SetStream(stream);
    return true;
  }
};

template <ActorFlavorEnum ActorFlavor>
inline
already_AddRefed<nsIDOMBlob>
GetBlobFromParams(const SlicedBlobConstructorParams& aParams)
{
  MOZ_STATIC_ASSERT(ActorFlavor == mozilla::dom::ipc::Parent,
                    "No other flavor is supported here!");

  BlobParent* actor =
    const_cast<BlobParent*>(
      static_cast<const BlobParent*>(aParams.sourceParent()));
  MOZ_ASSERT(actor);

  return actor->GetBlob();
}

template <>
inline
already_AddRefed<nsIDOMBlob>
GetBlobFromParams<Child>(const SlicedBlobConstructorParams& aParams)
{
  BlobChild* actor =
    const_cast<BlobChild*>(
      static_cast<const BlobChild*>(aParams.sourceChild()));
  MOZ_ASSERT(actor);

  return actor->GetBlob();
}

inline
void
SetBlobOnParams(BlobChild* aActor, SlicedBlobConstructorParams& aParams)
{
  aParams.sourceChild() = aActor;
}

inline
void
SetBlobOnParams(BlobParent* aActor, SlicedBlobConstructorParams& aParams)
{
  aParams.sourceParent() = aActor;
}

inline
nsDOMFileBase*
ToConcreteBlob(nsIDOMBlob* aBlob)
{
  // XXX This is only safe so long as all blob implementations in our tree
  //     inherit nsDOMFileBase. If that ever changes then this will need to grow
  //     a real interface or something.
  return static_cast<nsDOMFileBase*>(aBlob);
}

} // anonymous namespace

namespace mozilla {
namespace dom {
namespace ipc {

// Each instance of this class will be dispatched to the network stream thread
// pool to run the first time where it will open the file input stream. It will
// then dispatch itself back to the main thread to send the child process its
// response (assuming that the child has not crashed). The runnable will then
// dispatch itself to the thread pool again in order to close the file input
// stream.
class BlobTraits<Parent>::BaseType::OpenStreamRunnable : public nsRunnable
{
  friend class nsRevocableEventPtr<OpenStreamRunnable>;

  typedef BlobTraits<Parent> TraitsType;
  typedef TraitsType::BaseType BlobActorType;
  typedef TraitsType::StreamType BlobStreamProtocolType;

  // Only safe to access these pointers if mRevoked is false!
  BlobActorType* mBlobActor;
  BlobStreamProtocolType* mStreamActor;

  nsCOMPtr<nsIInputStream> mStream;
  nsCOMPtr<nsIIPCSerializableInputStream> mSerializable;
  nsCOMPtr<nsIEventTarget> mTarget;

  bool mRevoked;
  bool mClosing;

public:
  OpenStreamRunnable(BlobActorType* aBlobActor,
                     BlobStreamProtocolType* aStreamActor,
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

    nsresult rv = NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL);
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
      serializable->Serialize(params);

      MOZ_ASSERT(params.type() != InputStreamParams::T__None);

      unused << mStreamActor->Send__delete__(mStreamActor, params);

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

BlobTraits<Parent>::BaseType::BaseType()
{
}

BlobTraits<Parent>::BaseType::~BaseType()
{
}

void
BlobTraits<Parent>::BaseType::NoteRunnableCompleted(
                    BlobTraits<Parent>::BaseType::OpenStreamRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  for (uint32_t index = 0; index < mOpenStreamRunnables.Length(); index++) {
    nsRevocableEventPtr<BaseType::OpenStreamRunnable>& runnable =
      mOpenStreamRunnables[index];

    if (runnable.get() == aRunnable) {
      runnable.Forget();
      mOpenStreamRunnables.RemoveElementAt(index);
      return;
    }
  }

  MOZ_CRASH("Runnable not in our array!");
}

template <ActorFlavorEnum ActorFlavor>
struct RemoteBlobBase;

template <>
struct RemoteBlobBase<Parent>
{
  InputStreamParams mInputStreamParams;
};

template <>
struct RemoteBlobBase<Child>
{ };

template <ActorFlavorEnum ActorFlavor>
class RemoteBlob : public RemoteBlobBase<ActorFlavor>,
                   public nsDOMFile,
                   public nsIRemoteBlob
{
public:
  typedef RemoteBlob<ActorFlavor> SelfType;
  typedef Blob<ActorFlavor> ActorType;
  typedef InputStreamActor<ActorFlavor> StreamActorType;
  typedef typename ActorType::ConstructorParamsType ConstructorParamsType;

private:
  ActorType* mActor;

  class StreamHelper : public nsRunnable
  {
    typedef Blob<ActorFlavor> ActorType;
    typedef InputStreamActor<ActorFlavor> StreamActorType;

    mozilla::Monitor mMonitor;
    ActorType* mActor;
    nsCOMPtr<nsIDOMBlob> mSourceBlob;
    nsRefPtr<RemoteInputStream> mInputStream;
    bool mDone;

  public:
    StreamHelper(ActorType* aActor, nsIDOMBlob* aSourceBlob)
    : mMonitor("RemoteBlob::StreamHelper::mMonitor"), mActor(aActor),
      mSourceBlob(aSourceBlob), mDone(false)
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

      nsRefPtr<RemoteInputStream> stream = new RemoteInputStream(mSourceBlob,
                                                                 ActorFlavor);

      StreamActorType* streamActor = new StreamActorType(stream);
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

  class SliceHelper : public nsRunnable
  {
    typedef Blob<ActorFlavor> ActorType;

    mozilla::Monitor mMonitor;
    ActorType* mActor;
    nsCOMPtr<nsIDOMBlob> mSlice;
    uint64_t mStart;
    uint64_t mLength;
    nsString mContentType;
    bool mDone;

  public:
    SliceHelper(ActorType* aActor)
    : mMonitor("RemoteBlob::SliceHelper::mMonitor"), mActor(aActor), mStart(0),
      mLength(0), mDone(false)
    {
      // This may be created on any thread.
      MOZ_ASSERT(aActor);
    }

    nsresult
    GetSlice(uint64_t aStart, uint64_t aLength, const nsAString& aContentType,
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

      NormalBlobConstructorParams normalParams;
      normalParams.contentType() = mContentType;
      normalParams.length() = mLength;

      typename ActorType::ConstructorParamsType params;
      ActorType::BaseType::SetBlobConstructorParams(params, normalParams);

      ActorType* newActor = ActorType::Create(mActor->Manager(), params);
      MOZ_ASSERT(newActor);

      SlicedBlobConstructorParams slicedParams;
      slicedParams.contentType() = mContentType;
      slicedParams.begin() = mStart;
      slicedParams.end() = mStart + mLength;
      SetBlobOnParams(mActor, slicedParams);

      typename ActorType::OtherSideConstructorParamsType otherSideParams;
      ActorType::BaseType::SetBlobConstructorParams(otherSideParams,
                                                    slicedParams);

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

public:
  NS_DECL_ISUPPORTS_INHERITED

  RemoteBlob(const nsAString& aName, const nsAString& aContentType,
             uint64_t aLength, uint64_t aModDate)
  : nsDOMFile(aName, aContentType, aLength, aModDate), mActor(nullptr)
  {
    mImmutable = true;
  }

  RemoteBlob(const nsAString& aContentType, uint64_t aLength)
  : nsDOMFile(aContentType, aLength), mActor(nullptr)
  {
    mImmutable = true;
  }

  RemoteBlob()
  : nsDOMFile(EmptyString(), EmptyString(), UINT64_MAX, UINT64_MAX)
  , mActor(nullptr)
  {
    mImmutable = true;
  }

  virtual ~RemoteBlob()
  {
    if (mActor) {
      mActor->NoteDyingRemoteBlob();
    }
  }

  void
  SetActor(ActorType* aActor)
  {
    MOZ_ASSERT(!aActor || !mActor);
    mActor = aActor;
  }

  void
  MaybeSetInputStream(const ConstructorParamsType& aParams);

  virtual already_AddRefed<nsIDOMBlob>
  CreateSlice(uint64_t aStart, uint64_t aLength, const nsAString& aContentType)
              MOZ_OVERRIDE
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

  NS_IMETHOD
  GetInternalStream(nsIInputStream** aStream) MOZ_OVERRIDE;

  virtual void*
  GetPBlob() MOZ_OVERRIDE
  {
    return static_cast<typename ActorType::ProtocolType*>(mActor);
  }

  NS_IMETHOD
  GetLastModifiedDate(JSContext* cx, JS::Value* aLastModifiedDate)
  {
    if (IsDateUnknown()) {
      aLastModifiedDate->setNull();
    } else {
      JSObject* date = JS_NewDateObjectMsec(cx, mLastModificationDate);
      if (!date) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      aLastModifiedDate->setObject(*date);
    }
    return NS_OK;
  }
};

template <>
void
RemoteBlob<Parent>::MaybeSetInputStream(const ConstructorParamsType& aParams)
{
  if (aParams.optionalInputStreamParams().type() ==
      OptionalInputStreamParams::TInputStreamParams) {
    mInputStreamParams =
      aParams.optionalInputStreamParams().get_InputStreamParams();
  }
  else {
    MOZ_ASSERT(aParams.blobParams().type() ==
               ChildBlobConstructorParams::TMysteryBlobConstructorParams);
  }
}

template <>
void
RemoteBlob<Child>::MaybeSetInputStream(const ConstructorParamsType& aParams)
{
  // Nothing needed on the child side!
}

template <>
NS_IMETHODIMP
RemoteBlob<Parent>::GetInternalStream(nsIInputStream** aStream)
{
  if (mInputStreamParams.type() != InputStreamParams::T__None) {
    nsCOMPtr<nsIInputStream> stream = DeserializeInputStream(mInputStreamParams);
    if (!stream) {
      NS_WARNING("Failed to deserialize stream!");
      return NS_ERROR_UNEXPECTED;
    }

    stream.forget(aStream);
    return NS_OK;
  }

  if (!mActor) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<StreamHelper> helper = new StreamHelper(mActor, this);
  return helper->GetStream(aStream);
}

template <>
NS_IMETHODIMP
RemoteBlob<Child>::GetInternalStream(nsIInputStream** aStream)
{
  if (!mActor) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<StreamHelper> helper = new StreamHelper(mActor, this);
  return helper->GetStream(aStream);
}

template <ActorFlavorEnum ActorFlavor>
Blob<ActorFlavor>::Blob(ContentManager* aManager, nsIDOMBlob* aBlob)
: mBlob(aBlob), mRemoteBlob(nullptr), mOwnsBlob(true)
, mBlobIsFile(false), mManager(aManager)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBlob);
  MOZ_ASSERT(aManager);
  aBlob->AddRef();

  nsCOMPtr<nsIDOMFile> file = do_QueryInterface(aBlob);
  mBlobIsFile = !!file;
}

template <ActorFlavorEnum ActorFlavor>
Blob<ActorFlavor>::Blob(ContentManager* aManager,
                        const ConstructorParamsType& aParams)
: mBlob(nullptr), mRemoteBlob(nullptr), mOwnsBlob(false)
, mBlobIsFile(false), mManager(aManager)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aManager);

  ChildBlobConstructorParams::Type paramType =
    BaseType::GetBlobConstructorParams(aParams).type();

  mBlobIsFile =
    paramType == ChildBlobConstructorParams::TFileBlobConstructorParams ||
    paramType == ChildBlobConstructorParams::TMysteryBlobConstructorParams;

  nsRefPtr<RemoteBlobType> remoteBlob = CreateRemoteBlob(aParams);
  MOZ_ASSERT(remoteBlob);

  remoteBlob->SetActor(this);
  remoteBlob->MaybeSetInputStream(aParams);
  remoteBlob.forget(&mRemoteBlob);

  mBlob = mRemoteBlob;
  mOwnsBlob = true;
}

template <ActorFlavorEnum ActorFlavor>
Blob<ActorFlavor>*
Blob<ActorFlavor>::Create(ContentManager* aManager,
                          const ConstructorParamsType& aParams)
{
  MOZ_ASSERT(NS_IsMainThread());

  const ChildBlobConstructorParams& blobParams =
    BaseType::GetBlobConstructorParams(aParams);

  switch (blobParams.type()) {
    case ChildBlobConstructorParams::TNormalBlobConstructorParams:
    case ChildBlobConstructorParams::TFileBlobConstructorParams:
    case ChildBlobConstructorParams::TMysteryBlobConstructorParams:
      return new Blob<ActorFlavor>(aManager, aParams);

    case ChildBlobConstructorParams::TSlicedBlobConstructorParams: {
      const SlicedBlobConstructorParams& params =
        blobParams.get_SlicedBlobConstructorParams();

      nsCOMPtr<nsIDOMBlob> source = GetBlobFromParams<ActorFlavor>(params);
      MOZ_ASSERT(source);

      nsCOMPtr<nsIDOMBlob> slice;
      nsresult rv =
        source->Slice(params.begin(), params.end(), params.contentType(), 3,
                      getter_AddRefs(slice));
      NS_ENSURE_SUCCESS(rv, nullptr);

      return new Blob<ActorFlavor>(aManager, slice);
    }

    default:
      MOZ_CRASH("Unknown params!");
  }

  return nullptr;
}

template <ActorFlavorEnum ActorFlavor>
already_AddRefed<nsIDOMBlob>
Blob<ActorFlavor>::GetBlob()
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

template <ActorFlavorEnum ActorFlavor>
bool
Blob<ActorFlavor>::SetMysteryBlobInfo(const nsString& aName,
                                      const nsString& aContentType,
                                      uint64_t aLength,
                                      uint64_t aLastModifiedDate)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(mRemoteBlob);
  MOZ_ASSERT(aLength);
  MOZ_ASSERT(aLastModifiedDate != UINT64_MAX);

  ToConcreteBlob(mBlob)->SetLazyData(aName, aContentType,
                                     aLength, aLastModifiedDate);

  FileBlobConstructorParams params(aName, aContentType,
                                   aLength, aLastModifiedDate);
  return ProtocolType::SendResolveMystery(params);
}

template <ActorFlavorEnum ActorFlavor>
bool
Blob<ActorFlavor>::SetMysteryBlobInfo(const nsString& aContentType,
                                      uint64_t aLength)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(mRemoteBlob);
  MOZ_ASSERT(aLength);

  nsString voidString;
  voidString.SetIsVoid(true);

  ToConcreteBlob(mBlob)->SetLazyData(voidString, aContentType,
                                     aLength, UINT64_MAX);

  NormalBlobConstructorParams params(aContentType, aLength);
  return ProtocolType::SendResolveMystery(params);
}

template <ActorFlavorEnum ActorFlavor>
already_AddRefed<typename Blob<ActorFlavor>::RemoteBlobType>
Blob<ActorFlavor>::CreateRemoteBlob(const ConstructorParamsType& aParams)
{
  MOZ_ASSERT(NS_IsMainThread());

  const ChildBlobConstructorParams& blobParams =
    BaseType::GetBlobConstructorParams(aParams);

  nsRefPtr<RemoteBlobType> remoteBlob;

  switch (blobParams.type()) {
    case ChildBlobConstructorParams::TNormalBlobConstructorParams: {
      const NormalBlobConstructorParams& params =
        blobParams.get_NormalBlobConstructorParams();
      remoteBlob = new RemoteBlobType(params.contentType(), params.length());
      break;
    }

    case ChildBlobConstructorParams::TFileBlobConstructorParams: {
      const FileBlobConstructorParams& params =
        blobParams.get_FileBlobConstructorParams();
      remoteBlob =
        new RemoteBlobType(params.name(), params.contentType(),
                           params.length(), params.modDate());
      break;
    }

    case ChildBlobConstructorParams::TMysteryBlobConstructorParams: {
      remoteBlob = new RemoteBlobType();
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

template <ActorFlavorEnum ActorFlavor>
void
Blob<ActorFlavor>::NoteDyingRemoteBlob()
{
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(mRemoteBlob);
  MOZ_ASSERT(!mOwnsBlob);

  // This may be called on any thread due to the fact that RemoteBlob is
  // designed to be passed between threads. We must start the shutdown process
  // on the main thread, so we proxy here if necessary.
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewNonOwningRunnableMethod(this,
                                    &Blob<ActorFlavor>::NoteDyingRemoteBlob);
    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      MOZ_ASSERT(false, "Should never fail!");
    }

    return;
  }

  // Must do this before calling Send__delete__ or we'll crash there trying to
  // access a dangling pointer.
  mRemoteBlob = nullptr;

  mozilla::unused << ProtocolType::Send__delete__(this);
}

template <ActorFlavorEnum ActorFlavor>
void
Blob<ActorFlavor>::ActorDestroy(ActorDestroyReason aWhy)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mBlob);

  if (mRemoteBlob) {
    mRemoteBlob->SetActor(nullptr);
  }

  if (mOwnsBlob) {
    mBlob->Release();
  }
}

template <ActorFlavorEnum ActorFlavor>
bool
Blob<ActorFlavor>::RecvResolveMystery(const ResolveMysteryParams& aParams)
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

template <>
bool
Blob<Parent>::RecvPBlobStreamConstructor(StreamType* aActor)
{
  MOZ_ASSERT(NS_IsMainThread());
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
      static_cast<ProtocolType*>(remoteBlob->GetPBlob()) == this ||
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

  nsRefPtr<BaseType::OpenStreamRunnable> runnable =
    new BaseType::OpenStreamRunnable(this, aActor, stream, serializableStream,
                                     target);

  rv = runnable->Dispatch();
  NS_ENSURE_SUCCESS(rv, false);

  nsRevocableEventPtr<BaseType::OpenStreamRunnable>* arrayMember =
    mOpenStreamRunnables.AppendElement();
  *arrayMember = runnable;
  return true;
}

template <>
bool
Blob<Child>::RecvPBlobStreamConstructor(StreamType* aActor)
{
  MOZ_ASSERT(NS_IsMainThread());
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
  serializable->Serialize(params);

  MOZ_ASSERT(params.type() != InputStreamParams::T__None);

  return aActor->Send__delete__(aActor, params);
}

BlobTraits<Parent>::StreamType*
BlobTraits<Parent>::BaseType::AllocPBlobStreamParent()
{
  MOZ_ASSERT(NS_IsMainThread());
  return new InputStreamActor<Parent>();
}

BlobTraits<Child>::StreamType*
BlobTraits<Child>::BaseType::AllocPBlobStreamChild()
{
  MOZ_ASSERT(NS_IsMainThread());
  return new InputStreamActor<Child>();
}

bool
BlobTraits<Parent>::BaseType::DeallocPBlobStreamParent(BlobTraits<Parent>::StreamType* aActor)
{
  MOZ_ASSERT(NS_IsMainThread());
  delete aActor;
  return true;
}

bool
BlobTraits<Child>::BaseType::DeallocPBlobStreamChild(BlobTraits<Child>::StreamType* aActor)
{
  MOZ_ASSERT(NS_IsMainThread());
  delete aActor;
  return true;
}

template <ActorFlavorEnum ActorFlavor>
NS_IMPL_ADDREF_INHERITED(RemoteBlob<ActorFlavor>, nsDOMFile)

template <ActorFlavorEnum ActorFlavor>
NS_IMPL_RELEASE_INHERITED(RemoteBlob<ActorFlavor>, nsDOMFile)

template <ActorFlavorEnum ActorFlavor>
NS_IMPL_QUERY_INTERFACE_INHERITED1(RemoteBlob<ActorFlavor>, nsDOMFile,
                                                            nsIRemoteBlob)

// Explicit instantiation of both classes.
template class Blob<Parent>;
template class Blob<Child>;

} // namespace ipc
} // namespace dom
} // namespace mozilla
