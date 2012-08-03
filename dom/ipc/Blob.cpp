/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "Blob.h"

#include "nsIDOMFile.h"
#include "nsIInputStream.h"
#include "nsIRemoteBlob.h"
#include "nsISeekableStream.h"

#include "mozilla/Assertions.h"
#include "mozilla/Monitor.h"
#include "mozilla/unused.h"
#include "mozilla/net/NeckoMessageUtils.h"
#include "nsDOMFile.h"
#include "nsThreadUtils.h"

#include "ContentChild.h"
#include "ContentParent.h"

using namespace mozilla::dom;
using namespace mozilla::dom::ipc;

namespace {

class RemoteInputStream : public nsIInputStream,
                          public nsISeekableStream
{
  mozilla::Monitor mMonitor;
  nsCOMPtr<nsIDOMBlob> mSourceBlob;
  nsCOMPtr<nsIInputStream> mStream;
  nsCOMPtr<nsISeekableStream> mSeekableStream;

public:
  NS_DECL_ISUPPORTS

  RemoteInputStream(nsIDOMBlob* aSourceBlob)
  : mMonitor("RemoteInputStream.mMonitor"), mSourceBlob(aSourceBlob)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aSourceBlob);
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
  Available(PRUint32* aAvailable) MOZ_OVERRIDE
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
  Read(char* aBuffer, PRUint32 aCount, PRUint32* aResult) MOZ_OVERRIDE
  {
    nsresult rv = BlockAndWaitForStream();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStream->Read(aBuffer, aCount, aResult);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMETHOD
  ReadSegments(nsWriteSegmentFun aWriter, void* aClosure, PRUint32 aCount,
               PRUint32* aResult) MOZ_OVERRIDE
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
  Seek(PRInt32 aWhence, PRInt64 aOffset) MOZ_OVERRIDE
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
  Tell(PRInt64* aResult)
  {
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

private:
  virtual ~RemoteInputStream()
  { }

  void
  ReallyBlockAndWaitForStream()
  {
    mozilla::MonitorAutoLock lock(mMonitor);
    while (!mStream) {
      mMonitor.Wait();
    }
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

template <ActorFlavorEnum ActorFlavor>
class InputStreamActor : public BlobTraits<ActorFlavor>::StreamType
{
  typedef typename BlobTraits<ActorFlavor>::StreamType::InputStream InputStream;
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
  Recv__delete__(const InputStream& aStream) MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mRemoteStream);

    mRemoteStream->SetStream(aStream);
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

template <ActorFlavorEnum ActorFlavor>
class RemoteBlob : public nsDOMFile,
                   public nsIRemoteBlob
{
public:
  typedef RemoteBlob<ActorFlavor> SelfType;
  typedef Blob<ActorFlavor> ActorType;
  typedef InputStreamActor<ActorFlavor> StreamActorType;

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

      nsRefPtr<RemoteInputStream> stream = new RemoteInputStream(mSourceBlob);

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
    PRUint64 mStart;
    PRUint64 mLength;
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
    GetSlice(PRUint64 aStart, PRUint64 aLength, const nsAString& aContentType,
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

      ActorType* newActor = ActorType::Create(normalParams);
      MOZ_ASSERT(newActor);

      SlicedBlobConstructorParams slicedParams;
      slicedParams.contentType() = mContentType;
      slicedParams.begin() = mStart;
      slicedParams.end() = mStart + mLength;
      SetBlobOnParams(mActor, slicedParams);

      if (mActor->Manager()->SendPBlobConstructor(newActor, slicedParams)) {
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
             PRUint64 aLength)
  : nsDOMFile(aName, aContentType, aLength), mActor(nullptr)
  {
    mImmutable = true;
  }

  RemoteBlob(const nsAString& aContentType, PRUint64 aLength)
  : nsDOMFile(aContentType, aLength), mActor(nullptr)
  {
    mImmutable = true;
  }

  RemoteBlob()
  : nsDOMFile(EmptyString(), EmptyString(), UINT64_MAX), mActor(nullptr)
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

  virtual already_AddRefed<nsIDOMBlob>
  CreateSlice(PRUint64 aStart, PRUint64 aLength, const nsAString& aContentType)
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
  GetInternalStream(nsIInputStream** aStream) MOZ_OVERRIDE
  {
    if (!mActor) {
      return NS_ERROR_UNEXPECTED;
    }

    nsRefPtr<StreamHelper> helper = new StreamHelper(mActor, this);
    return helper->GetStream(aStream);
  }

  virtual void*
  GetPBlob() MOZ_OVERRIDE
  {
    return static_cast<typename ActorType::BaseType*>(mActor);
  }
};

} // namespace ipc
} // namespace dom
} // namespace mozilla

template <ActorFlavorEnum ActorFlavor>
Blob<ActorFlavor>::Blob(nsIDOMBlob* aBlob)
: mBlob(aBlob), mRemoteBlob(nullptr), mOwnsBlob(true), mBlobIsFile(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBlob);
  aBlob->AddRef();

  nsCOMPtr<nsIDOMFile> file = do_QueryInterface(aBlob);
  mBlobIsFile = !!file;
}

template <ActorFlavorEnum ActorFlavor>
Blob<ActorFlavor>::Blob(const BlobConstructorParams& aParams)
: mBlob(nullptr), mRemoteBlob(nullptr), mOwnsBlob(false), mBlobIsFile(false)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<RemoteBlobType> remoteBlob;

  switch (aParams.type()) {
    case BlobConstructorParams::TNormalBlobConstructorParams: {
      const NormalBlobConstructorParams& params =
        aParams.get_NormalBlobConstructorParams();
      remoteBlob = new RemoteBlobType(params.contentType(), params.length());
      break;
    }

    case BlobConstructorParams::TFileBlobConstructorParams: {
      const FileBlobConstructorParams& params =
        aParams.get_FileBlobConstructorParams();
      remoteBlob =
        new RemoteBlobType(params.name(), params.contentType(),
                           params.length());
      mBlobIsFile = true;
      break;
    }

    case BlobConstructorParams::TMysteryBlobConstructorParams: {
      remoteBlob = new RemoteBlobType();
      mBlobIsFile = true;
      break;
    }

    default:
      MOZ_NOT_REACHED("Unknown params!");
  }

  MOZ_ASSERT(remoteBlob);

  SetRemoteBlob(remoteBlob);
}

template <ActorFlavorEnum ActorFlavor>
Blob<ActorFlavor>*
Blob<ActorFlavor>::Create(const BlobConstructorParams& aParams)
{
  MOZ_ASSERT(NS_IsMainThread());

  switch (aParams.type()) {
    case BlobConstructorParams::TNormalBlobConstructorParams:
    case BlobConstructorParams::TFileBlobConstructorParams:
    case BlobConstructorParams::TMysteryBlobConstructorParams:
      return new Blob<ActorFlavor>(aParams);

    case BlobConstructorParams::TSlicedBlobConstructorParams: {
      const SlicedBlobConstructorParams& params =
        aParams.get_SlicedBlobConstructorParams();

      nsCOMPtr<nsIDOMBlob> source = GetBlobFromParams<ActorFlavor>(params);
      MOZ_ASSERT(source);

      nsCOMPtr<nsIDOMBlob> slice;
      nsresult rv =
        source->Slice(params.begin(), params.end(), params.contentType(), 3,
                      getter_AddRefs(slice));
      NS_ENSURE_SUCCESS(rv, nullptr);

      return new Blob<ActorFlavor>(slice);
    }

    default:
      MOZ_NOT_REACHED("Unknown params!");
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
                                      PRUint64 aLength)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(mRemoteBlob);
  MOZ_ASSERT(aLength);

  ToConcreteBlob(mBlob)->SetLazyData(aName, aContentType, aLength);

  FileBlobConstructorParams params(aName, aContentType, aLength);
  return BaseType::SendResolveMystery(params);
}

template <ActorFlavorEnum ActorFlavor>
bool
Blob<ActorFlavor>::SetMysteryBlobInfo(const nsString& aContentType,
                                      PRUint64 aLength)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(mRemoteBlob);
  MOZ_ASSERT(aLength);

  nsString voidString;
  voidString.SetIsVoid(true);

  ToConcreteBlob(mBlob)->SetLazyData(voidString, aContentType, aLength);

  NormalBlobConstructorParams params(aContentType, aLength);
  return BaseType::SendResolveMystery(params);
}

template <ActorFlavorEnum ActorFlavor>
void
Blob<ActorFlavor>::SetRemoteBlob(nsRefPtr<RemoteBlobType>& aRemoteBlob)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mBlob);
  MOZ_ASSERT(!mRemoteBlob);
  MOZ_ASSERT(!mOwnsBlob);
  MOZ_ASSERT(aRemoteBlob);

  if (NS_FAILED(aRemoteBlob->SetMutable(false))) {
    MOZ_NOT_REACHED("Failed to make remote blob immutable!");
  }

  aRemoteBlob->SetActor(this);
  aRemoteBlob.forget(&mRemoteBlob);

  mBlob = mRemoteBlob;
  mOwnsBlob = true;
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

  mozilla::unused << BaseType::Send__delete__(this);
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
      blob->SetLazyData(voidString, params.contentType(), params.length());
      break;
    }

    case ResolveMysteryParams::TFileBlobConstructorParams: {
      const FileBlobConstructorParams& params =
        aParams.get_FileBlobConstructorParams();
      blob->SetLazyData(params.name(), params.contentType(), params.length());
      break;
    }

    default:
      MOZ_NOT_REACHED("Unknown params!");
  }

  return true;
}

template <ActorFlavorEnum ActorFlavor>
bool
Blob<ActorFlavor>::RecvPBlobStreamConstructor(StreamType* aActor)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(!mRemoteBlob);

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = mBlob->GetInternalStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, false);

  return aActor->Send__delete__(aActor, stream.get());
}

template <ActorFlavorEnum ActorFlavor>
typename Blob<ActorFlavor>::StreamType*
Blob<ActorFlavor>::AllocPBlobStream()
{
  MOZ_ASSERT(NS_IsMainThread());
  return new InputStreamActor<ActorFlavor>();
}

template <ActorFlavorEnum ActorFlavor>
bool
Blob<ActorFlavor>::DeallocPBlobStream(StreamType* aActor)
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

NS_IMPL_THREADSAFE_ADDREF(RemoteInputStream)
NS_IMPL_THREADSAFE_RELEASE(RemoteInputStream)

NS_INTERFACE_MAP_BEGIN(RemoteInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISeekableStream, IsSeekableStream())
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

namespace mozilla {
namespace dom {
namespace ipc {

// Explicit instantiation of both classes.
template class Blob<Parent>;
template class Blob<Child>;

} // namespace ipc
} // namespace dom
} // namespace mozilla
