/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteLazyInputStream.h"
#include "RemoteLazyInputStreamChild.h"
#include "RemoteLazyInputStreamParent.h"
#include "chrome/common/ipc_message_utils.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/Logging.h"
#include "mozilla/PRemoteLazyInputStream.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/InputStreamParams.h"
#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/ProtocolMessageUtils.h"
#include "mozilla/net/SocketProcessParent.h"
#include "mozilla/SlicedInputStream.h"
#include "mozilla/NonBlockingAsyncInputStream.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsID.h"
#include "nsIInputStream.h"
#include "nsIPipe.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "RemoteLazyInputStreamStorage.h"
#include "RemoteLazyInputStreamThread.h"

namespace mozilla {

mozilla::LazyLogModule gRemoteLazyStreamLog("RemoteLazyStream");

namespace {

class InputStreamCallbackRunnable final : public DiscardableRunnable {
 public:
  // Note that the execution can be synchronous in case the event target is
  // null.
  static void Execute(already_AddRefed<nsIInputStreamCallback> aCallback,
                      already_AddRefed<nsIEventTarget> aEventTarget,
                      RemoteLazyInputStream* aStream) {
    RefPtr<InputStreamCallbackRunnable> runnable =
        new InputStreamCallbackRunnable(std::move(aCallback), aStream);

    nsCOMPtr<nsIEventTarget> target = std::move(aEventTarget);
    if (target) {
      target->Dispatch(runnable, NS_DISPATCH_NORMAL);
    } else {
      runnable->Run();
    }
  }

  NS_IMETHOD
  Run() override {
    mCallback->OnInputStreamReady(mStream);
    mCallback = nullptr;
    mStream = nullptr;
    return NS_OK;
  }

 private:
  InputStreamCallbackRunnable(
      already_AddRefed<nsIInputStreamCallback> aCallback,
      RemoteLazyInputStream* aStream)
      : DiscardableRunnable("dom::InputStreamCallbackRunnable"),
        mCallback(std::move(aCallback)),
        mStream(aStream) {
    MOZ_ASSERT(mCallback);
    MOZ_ASSERT(mStream);
  }

  RefPtr<nsIInputStreamCallback> mCallback;
  RefPtr<RemoteLazyInputStream> mStream;
};

class FileMetadataCallbackRunnable final : public DiscardableRunnable {
 public:
  static void Execute(nsIFileMetadataCallback* aCallback,
                      nsIEventTarget* aEventTarget,
                      RemoteLazyInputStream* aStream) {
    MOZ_ASSERT(aCallback);
    MOZ_ASSERT(aEventTarget);

    RefPtr<FileMetadataCallbackRunnable> runnable =
        new FileMetadataCallbackRunnable(aCallback, aStream);

    nsCOMPtr<nsIEventTarget> target = aEventTarget;
    target->Dispatch(runnable, NS_DISPATCH_NORMAL);
  }

  NS_IMETHOD
  Run() override {
    mCallback->OnFileMetadataReady(mStream);
    mCallback = nullptr;
    mStream = nullptr;
    return NS_OK;
  }

 private:
  FileMetadataCallbackRunnable(nsIFileMetadataCallback* aCallback,
                               RemoteLazyInputStream* aStream)
      : DiscardableRunnable("dom::FileMetadataCallbackRunnable"),
        mCallback(aCallback),
        mStream(aStream) {
    MOZ_ASSERT(mCallback);
    MOZ_ASSERT(mStream);
  }

  nsCOMPtr<nsIFileMetadataCallback> mCallback;
  RefPtr<RemoteLazyInputStream> mStream;
};

}  // namespace

NS_IMPL_ADDREF(RemoteLazyInputStream);
NS_IMPL_RELEASE(RemoteLazyInputStream);

NS_INTERFACE_MAP_BEGIN(RemoteLazyInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStreamCallback)
  NS_INTERFACE_MAP_ENTRY(nsICloneableInputStream)
  NS_INTERFACE_MAP_ENTRY(nsICloneableInputStreamWithRange)
  NS_INTERFACE_MAP_ENTRY(nsIIPCSerializableInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIFileMetadata)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncFileMetadata)
  NS_INTERFACE_MAP_ENTRY(nsIInputStreamLength)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncInputStreamLength)
  NS_INTERFACE_MAP_ENTRY(mozIRemoteLazyInputStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

RemoteLazyInputStream::RemoteLazyInputStream(RemoteLazyInputStreamChild* aActor,
                                             uint64_t aStart, uint64_t aLength)
    : mStart(aStart), mLength(aLength), mState(eInit), mActor(aActor) {
  MOZ_ASSERT(aActor);

  mActor->StreamCreated();

  auto storage = RemoteLazyInputStreamStorage::Get().unwrapOr(nullptr);
  if (storage) {
    nsCOMPtr<nsIInputStream> stream;
    storage->GetStream(mActor->StreamID(), mStart, mLength,
                       getter_AddRefs(stream));
    if (stream) {
      mState = eRunning;
      mInnerStream = stream;
    }
  }
}

RemoteLazyInputStream::RemoteLazyInputStream(nsIInputStream* aStream)
    : mStart(0), mLength(UINT64_MAX), mState(eRunning), mInnerStream(aStream) {}

static already_AddRefed<RemoteLazyInputStreamChild> BindChildActor(
    nsID aId, mozilla::ipc::Endpoint<PRemoteLazyInputStreamChild> aEndpoint) {
  auto* thread = RemoteLazyInputStreamThread::GetOrCreate();
  if (NS_WARN_IF(!thread)) {
    return nullptr;
  }
  auto actor = MakeRefPtr<RemoteLazyInputStreamChild>(aId);
  thread->Dispatch(
      NS_NewRunnableFunction("RemoteLazyInputStream::BindChildActor",
                             [actor, childEp = std::move(aEndpoint)]() mutable {
                               bool ok = childEp.Bind(actor);
                               MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Debug,
                                       ("Binding child actor for %s (%p): %s",
                                        nsIDToCString(actor->StreamID()).get(),
                                        actor.get(), ok ? "OK" : "ERROR"));
                             }));

  return actor.forget();
}

already_AddRefed<RemoteLazyInputStream> RemoteLazyInputStream::WrapStream(
    nsIInputStream* aInputStream) {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (nsCOMPtr<mozIRemoteLazyInputStream> lazyStream =
          do_QueryInterface(aInputStream)) {
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Debug,
            ("Returning already-wrapped stream"));
    return lazyStream.forget().downcast<RemoteLazyInputStream>();
  }

  // If we have a stream and are in the parent process, create a new actor pair
  // and transfer ownership of the stream into storage.
  auto streamStorage = RemoteLazyInputStreamStorage::Get();
  if (NS_WARN_IF(streamStorage.isErr())) {
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Warning,
            ("Cannot wrap with no storage!"));
    return nullptr;
  }

  nsID id = nsID::GenerateUUID();
  MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
          ("Wrapping stream %p as %s", aInputStream, nsIDToCString(id).get()));
  streamStorage.inspect()->AddStream(aInputStream, id);

  mozilla::ipc::Endpoint<PRemoteLazyInputStreamParent> parentEp;
  mozilla::ipc::Endpoint<PRemoteLazyInputStreamChild> childEp;
  MOZ_ALWAYS_SUCCEEDS(
      PRemoteLazyInputStream::CreateEndpoints(&parentEp, &childEp));

  // Bind the actor on our background thread.
  streamStorage.inspect()->TaskQueue()->Dispatch(NS_NewRunnableFunction(
      "RemoteLazyInputStreamParent::Bind",
      [parentEp = std::move(parentEp), id]() mutable {
        auto actor = MakeRefPtr<RemoteLazyInputStreamParent>(id);
        bool ok = parentEp.Bind(actor);
        MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Debug,
                ("Binding parent actor for %s (%p): %s",
                 nsIDToCString(id).get(), actor.get(), ok ? "OK" : "ERROR"));
      }));

  RefPtr<RemoteLazyInputStreamChild> actor =
      BindChildActor(id, std::move(childEp));
  return do_AddRef(new RemoteLazyInputStream(actor));
}

NS_IMETHODIMP RemoteLazyInputStream::TakeInternalStream(
    nsIInputStream** aStream) {
  RefPtr<RemoteLazyInputStreamChild> actor;
  {
    MutexAutoLock lock(mMutex);
    if (mState == eInit || mState == ePending) {
      return NS_BASE_STREAM_WOULD_BLOCK;
    }
    if (mState == eClosed) {
      return NS_BASE_STREAM_CLOSED;
    }
    if (mInputStreamCallback) {
      MOZ_ASSERT_UNREACHABLE(
          "Do not call TakeInternalStream after calling AsyncWait");
      return NS_ERROR_UNEXPECTED;
    }

    // Take the inner stream and return it, then close ourselves.
    if (mInnerStream) {
      mInnerStream.forget(aStream);
    } else if (mAsyncInnerStream) {
      mAsyncInnerStream.forget(aStream);
    }
    mState = eClosed;
    actor = mActor.forget();
  }
  if (actor) {
    actor->StreamConsumed();
  }
  return NS_OK;
}

NS_IMETHODIMP RemoteLazyInputStream::GetInternalStreamID(nsID& aID) {
  MutexAutoLock lock(mMutex);
  if (!mActor) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  aID = mActor->StreamID();
  return NS_OK;
}

RemoteLazyInputStream::~RemoteLazyInputStream() { Close(); }

nsCString RemoteLazyInputStream::Describe() {
  const char* state = "?";
  switch (mState) {
    case eInit:
      state = "i";
      break;
    case ePending:
      state = "p";
      break;
    case eRunning:
      state = "r";
      break;
    case eClosed:
      state = "c";
      break;
  }
  return nsPrintfCString(
      "[%p, %s, %s, %p%s, %s%s|%s%s]", this, state,
      mActor ? nsIDToCString(mActor->StreamID()).get() : "<no actor>",
      mInnerStream ? mInnerStream.get() : mAsyncInnerStream.get(),
      mAsyncInnerStream ? "(A)" : "", mInputStreamCallback ? "I" : "",
      mInputStreamCallbackEventTarget ? "+" : "",
      mFileMetadataCallback ? "F" : "",
      mFileMetadataCallbackEventTarget ? "+" : "");
}

// nsIInputStream interface

NS_IMETHODIMP
RemoteLazyInputStream::Available(uint64_t* aLength) {
  nsCOMPtr<nsIAsyncInputStream> stream;
  {
    MutexAutoLock lock(mMutex);

    // We don't have a remoteStream yet: let's return 0.
    if (mState == eInit || mState == ePending) {
      *aLength = 0;
      return NS_OK;
    }

    if (mState == eClosed) {
      return NS_BASE_STREAM_CLOSED;
    }

    MOZ_ASSERT(mState == eRunning);
    MOZ_ASSERT(mInnerStream || mAsyncInnerStream);

    nsresult rv = EnsureAsyncRemoteStream();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    stream = mAsyncInnerStream;
  }

  MOZ_ASSERT(stream);
  return stream->Available(aLength);
}

NS_IMETHODIMP
RemoteLazyInputStream::Read(char* aBuffer, uint32_t aCount,
                            uint32_t* aReadCount) {
  nsCOMPtr<nsIAsyncInputStream> stream;
  {
    MutexAutoLock lock(mMutex);

    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
            ("Read(%u) %s", aCount, Describe().get()));

    // Read is not available is we don't have a remoteStream.
    if (mState == eInit || mState == ePending) {
      return NS_BASE_STREAM_WOULD_BLOCK;
    }

    if (mState == eClosed) {
      return NS_BASE_STREAM_CLOSED;
    }

    MOZ_ASSERT(mState == eRunning);
    MOZ_ASSERT(mInnerStream || mAsyncInnerStream);

    nsresult rv = EnsureAsyncRemoteStream();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    stream = mAsyncInnerStream;
  }

  MOZ_ASSERT(stream);
  nsresult rv = stream->Read(aBuffer, aCount, aReadCount);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // If some data has been read, we mark the stream as consumed.
  if (*aReadCount > 0) {
    MarkConsumed();
  }

  MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
          ("Read %u/%u bytes", *aReadCount, aCount));

  return NS_OK;
}

NS_IMETHODIMP
RemoteLazyInputStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                    uint32_t aCount, uint32_t* aResult) {
  nsCOMPtr<nsIAsyncInputStream> stream;
  {
    MutexAutoLock lock(mMutex);

    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
            ("ReadSegments(%u) %s", aCount, Describe().get()));

    // ReadSegments is not available is we don't have a remoteStream.
    if (mState == eInit || mState == ePending) {
      return NS_BASE_STREAM_WOULD_BLOCK;
    }

    if (mState == eClosed) {
      return NS_BASE_STREAM_CLOSED;
    }

    MOZ_ASSERT(mState == eRunning);
    MOZ_ASSERT(mInnerStream || mAsyncInnerStream);

    nsresult rv = EnsureAsyncRemoteStream();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Warning,
              ("EnsureAsyncRemoteStream failed! %s %s",
               mozilla::GetStaticErrorName(rv), Describe().get()));
      return rv;
    }

    stream = mAsyncInnerStream;
  }

  MOZ_ASSERT(stream);
  nsresult rv = stream->ReadSegments(aWriter, aClosure, aCount, aResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // If some data has been read, we mark the stream as consumed.
  if (*aResult != 0) {
    MarkConsumed();
  }

  MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
          ("ReadSegments %u/%u bytes", *aResult, aCount));

  return NS_OK;
}

void RemoteLazyInputStream::MarkConsumed() {
  RefPtr<RemoteLazyInputStreamChild> actor;
  {
    MutexAutoLock lock(mMutex);
    if (mActor) {
      MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Debug,
              ("MarkConsumed %s", Describe().get()));
    }

    actor = mActor.forget();
  }
  if (actor) {
    actor->StreamConsumed();
  }
}

NS_IMETHODIMP
RemoteLazyInputStream::IsNonBlocking(bool* aNonBlocking) {
  *aNonBlocking = true;
  return NS_OK;
}

NS_IMETHODIMP
RemoteLazyInputStream::Close() {
  RefPtr<RemoteLazyInputStreamChild> actor;

  nsCOMPtr<nsIAsyncInputStream> asyncInnerStream;
  nsCOMPtr<nsIInputStream> innerStream;

  RefPtr<nsIInputStreamCallback> inputStreamCallback;
  nsCOMPtr<nsIEventTarget> inputStreamCallbackEventTarget;

  {
    MutexAutoLock lock(mMutex);
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Debug,
            ("Close %s", Describe().get()));

    actor = mActor.forget();

    asyncInnerStream = mAsyncInnerStream.forget();
    innerStream = mInnerStream.forget();

    // TODO(Bug 1737783): Notify to the mFileMetadataCallback that this
    // lazy input stream has been closed.
    mFileMetadataCallback = nullptr;
    mFileMetadataCallbackEventTarget = nullptr;

    inputStreamCallback = mInputStreamCallback.forget();
    inputStreamCallbackEventTarget = mInputStreamCallbackEventTarget.forget();

    mState = eClosed;
  }

  if (actor) {
    actor->StreamConsumed();
  }

  if (inputStreamCallback) {
    InputStreamCallbackRunnable::Execute(
        inputStreamCallback.forget(), inputStreamCallbackEventTarget.forget(),
        this);
  }

  if (asyncInnerStream) {
    asyncInnerStream->CloseWithStatus(NS_BASE_STREAM_CLOSED);
  }

  if (innerStream) {
    innerStream->Close();
  }

  return NS_OK;
}

// nsICloneableInputStream interface

NS_IMETHODIMP
RemoteLazyInputStream::GetCloneable(bool* aCloneable) {
  *aCloneable = true;
  return NS_OK;
}

NS_IMETHODIMP
RemoteLazyInputStream::Clone(nsIInputStream** aResult) {
  return CloneWithRange(0, UINT64_MAX, aResult);
}

// nsICloneableInputStreamWithRange interface

NS_IMETHODIMP
RemoteLazyInputStream::CloneWithRange(uint64_t aStart, uint64_t aLength,
                                      nsIInputStream** aResult) {
  MutexAutoLock lock(mMutex);
  MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Debug,
          ("CloneWithRange %" PRIu64 " %" PRIu64 " %s", aStart, aLength,
           Describe().get()));

  nsresult rv;

  RefPtr<RemoteLazyInputStream> stream;
  if (mState == eClosed) {
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose, ("Cloning closed stream"));
    stream = new RemoteLazyInputStream();
    stream.forget(aResult);
    return NS_OK;
  }

  uint64_t start = 0;
  uint64_t length = 0;
  auto maxLength = CheckedUint64(mLength) - aStart;
  if (maxLength.isValid()) {
    start = mStart + aStart;
    length = std::min(maxLength.value(), aLength);
  }

  // If the slice would be empty, wrap an empty input stream and return it.
  if (length == 0) {
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose, ("Creating empty stream"));

    nsCOMPtr<nsIInputStream> emptyStream;
    rv = NS_NewCStringInputStream(getter_AddRefs(emptyStream), ""_ns);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    stream = new RemoteLazyInputStream(emptyStream);
    stream.forget(aResult);
    return NS_OK;
  }

  // If we still have a connection to our actor, that means we haven't read any
  // data yet, and can clone + slice by building a new stream backed by the same
  // actor.
  if (mActor) {
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
            ("Cloning stream with actor"));

    stream = new RemoteLazyInputStream(mActor, start, length);
    stream.forget(aResult);
    return NS_OK;
  }

  // We no longer have our actor, either because we were constructed without
  // one, or we've already begun reading. Perform the clone locally on our inner
  // input stream.

  nsCOMPtr<nsIInputStream> innerStream = mInnerStream;
  if (mAsyncInnerStream) {
    innerStream = mAsyncInnerStream;
  }

  nsCOMPtr<nsICloneableInputStream> cloneable = do_QueryInterface(innerStream);
  if (!cloneable || !cloneable->GetCloneable()) {
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
            ("Cloning non-cloneable stream - copying to pipe"));

    // If our internal stream isn't cloneable, to perform a clone we'll need to
    // copy into a pipe and replace our internal stream.
    nsCOMPtr<nsIAsyncInputStream> pipeIn;
    nsCOMPtr<nsIAsyncOutputStream> pipeOut;
    rv = NS_NewPipe2(getter_AddRefs(pipeIn), getter_AddRefs(pipeOut), true,
                     true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    RefPtr<RemoteLazyInputStreamThread> thread =
        RemoteLazyInputStreamThread::GetOrCreate();
    if (NS_WARN_IF(!thread)) {
      return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
    }

    mAsyncInnerStream = pipeIn;
    mInnerStream = nullptr;

    // If we have a callback pending, we need to re-call AsyncWait on the inner
    // stream. This should not re-enter us immediately, as `pipeIn` hasn't been
    // sent any data yet, but we may be called again as soon as `NS_AsyncCopy`
    // has begun copying.
    if (mInputStreamCallback) {
      mAsyncInnerStream->AsyncWait(this, mInputStreamCallbackFlags,
                                   mInputStreamCallbackRequestedCount,
                                   mInputStreamCallbackEventTarget);
    }

    rv = NS_AsyncCopy(innerStream, pipeOut, thread,
                      NS_ASYNCCOPY_VIA_WRITESEGMENTS);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // The copy failed, revert the changes we did and restore our previous
      // inner stream.
      mAsyncInnerStream = nullptr;
      mInnerStream = innerStream;
      return rv;
    }

    cloneable = do_QueryInterface(mAsyncInnerStream);
  }

  MOZ_ASSERT(cloneable && cloneable->GetCloneable());

  // Check if we can clone more efficiently with a range.
  if (length < UINT64_MAX) {
    if (nsCOMPtr<nsICloneableInputStreamWithRange> cloneableWithRange =
            do_QueryInterface(cloneable)) {
      MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose, ("Cloning with range"));
      nsCOMPtr<nsIInputStream> cloned;
      rv = cloneableWithRange->CloneWithRange(start, length,
                                              getter_AddRefs(cloned));
      if (NS_FAILED(rv)) {
        return rv;
      }

      stream = new RemoteLazyInputStream(cloned);
      stream.forget(aResult);
      return NS_OK;
    }
  }

  // Directly clone our inner stream, and then slice it if needed.
  nsCOMPtr<nsIInputStream> cloned;
  rv = cloneable->Clone(getter_AddRefs(cloned));
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (length < UINT64_MAX) {
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
            ("Slicing stream with %" PRIu64 " %" PRIu64, start, length));
    cloned = new SlicedInputStream(cloned.forget(), start, length);
  }

  stream = new RemoteLazyInputStream(cloned);
  stream.forget(aResult);
  return NS_OK;
}

// nsIAsyncInputStream interface

NS_IMETHODIMP
RemoteLazyInputStream::CloseWithStatus(nsresult aStatus) { return Close(); }

NS_IMETHODIMP
RemoteLazyInputStream::AsyncWait(nsIInputStreamCallback* aCallback,
                                 uint32_t aFlags, uint32_t aRequestedCount,
                                 nsIEventTarget* aEventTarget) {
  // Ensure we always have an event target for AsyncWait callbacks, so that
  // calls to `AsyncWait` cannot reenter us with `OnInputStreamReady`.
  nsCOMPtr<nsIEventTarget> eventTarget = aEventTarget;
  if (aCallback && !eventTarget) {
    eventTarget = RemoteLazyInputStreamThread::GetOrCreate();
    if (NS_WARN_IF(!eventTarget)) {
      return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
    }
  }

  {
    MutexAutoLock lock(mMutex);

    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
            ("AsyncWait(%p, %u, %u, %p) %s", aCallback, aFlags, aRequestedCount,
             aEventTarget, Describe().get()));

    // See RemoteLazyInputStream.h for more information about this state
    // machine.

    nsCOMPtr<nsIAsyncInputStream> stream;
    switch (mState) {
      // First call, we need to retrieve the stream from the parent actor.
      case eInit:
        MOZ_ASSERT(mActor);

        mInputStreamCallback = aCallback;
        mInputStreamCallbackEventTarget = eventTarget;
        mInputStreamCallbackFlags = aFlags;
        mInputStreamCallbackRequestedCount = aRequestedCount;
        mState = ePending;

        StreamNeeded();
        return NS_OK;

      // We are still waiting for the remote inputStream
      case ePending: {
        if (NS_WARN_IF(mInputStreamCallback && aCallback &&
                       mInputStreamCallback != aCallback)) {
          return NS_ERROR_FAILURE;
        }

        mInputStreamCallback = aCallback;
        mInputStreamCallbackEventTarget = eventTarget;
        mInputStreamCallbackFlags = aFlags;
        mInputStreamCallbackRequestedCount = aRequestedCount;
        return NS_OK;
      }

      // We have the remote inputStream, let's check if we can execute the
      // callback.
      case eRunning: {
        if (NS_WARN_IF(mInputStreamCallback && aCallback &&
                       mInputStreamCallback != aCallback)) {
          return NS_ERROR_FAILURE;
        }

        nsresult rv = EnsureAsyncRemoteStream();
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        mInputStreamCallback = aCallback;
        mInputStreamCallbackEventTarget = eventTarget;
        mInputStreamCallbackFlags = aFlags;
        mInputStreamCallbackRequestedCount = aRequestedCount;

        stream = mAsyncInnerStream;
        break;
      }

      case eClosed:
        [[fallthrough]];
      default:
        MOZ_ASSERT(mState == eClosed);
        if (NS_WARN_IF(mInputStreamCallback && aCallback &&
                       mInputStreamCallback != aCallback)) {
          return NS_ERROR_FAILURE;
        }
        break;
    }

    if (stream) {
      return stream->AsyncWait(aCallback ? this : nullptr, aFlags,
                               aRequestedCount, eventTarget);
    }
  }

  if (aCallback) {
    // if stream is nullptr here, that probably means the stream has
    // been closed and the callback can be executed immediately
    InputStreamCallbackRunnable::Execute(do_AddRef(aCallback),
                                         do_AddRef(eventTarget), this);
  }
  return NS_OK;
}

void RemoteLazyInputStream::StreamNeeded() {
  MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Debug,
          ("StreamNeeded %s", Describe().get()));

  auto* thread = RemoteLazyInputStreamThread::GetOrCreate();
  if (NS_WARN_IF(!thread)) {
    return;
  }
  thread->Dispatch(NS_NewRunnableFunction(
      "RemoteLazyInputStream::StreamNeeded",
      [self = RefPtr{this}, actor = mActor, start = mStart, length = mLength] {
        MOZ_LOG(
            gRemoteLazyStreamLog, LogLevel::Debug,
            ("Sending StreamNeeded(%" PRIu64 " %" PRIu64 ") %s %d", start,
             length, nsIDToCString(actor->StreamID()).get(), actor->CanSend()));

        actor->SendStreamNeeded(
            start, length,
            [self](const Maybe<mozilla::ipc::IPCStream>& aStream) {
              // Try to deserialize the stream from our remote, and close our
              // stream if it fails.
              nsCOMPtr<nsIInputStream> stream =
                  mozilla::ipc::DeserializeIPCStream(aStream);
              if (NS_WARN_IF(!stream)) {
                NS_WARNING("Failed to deserialize IPC stream");
                self->Close();
              }

              // Lock our mutex to update the inner stream, and collect any
              // callbacks which we need to invoke.
              MutexAutoLock lock(self->mMutex);

              MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Debug,
                      ("ResolveStreamNeeded(%p) %s", stream.get(),
                       self->Describe().get()));

              if (self->mState == ePending) {
                self->mInnerStream = stream.forget();
                self->mState = eRunning;

                // Notify any listeners that we've now acquired the underlying
                // stream, so file metadata information will be available.
                nsCOMPtr<nsIFileMetadataCallback> fileMetadataCallback =
                    self->mFileMetadataCallback.forget();
                nsCOMPtr<nsIEventTarget> fileMetadataCallbackEventTarget =
                    self->mFileMetadataCallbackEventTarget.forget();
                if (fileMetadataCallback) {
                  FileMetadataCallbackRunnable::Execute(
                      fileMetadataCallback, fileMetadataCallbackEventTarget,
                      self);
                }

                // **NOTE** we can re-enter this class here **NOTE**
                // If we already have an input stream callback, attempt to
                // register ourselves with AsyncWait on the underlying stream.
                if (self->mInputStreamCallback) {
                  if (NS_FAILED(self->EnsureAsyncRemoteStream()) ||
                      NS_FAILED(self->mAsyncInnerStream->AsyncWait(
                          self, self->mInputStreamCallbackFlags,
                          self->mInputStreamCallbackRequestedCount,
                          self->mInputStreamCallbackEventTarget))) {
                    InputStreamCallbackRunnable::Execute(
                        self->mInputStreamCallback.forget(),
                        self->mInputStreamCallbackEventTarget.forget(), self);
                  }
                }
              }

              if (stream) {
                NS_WARNING("Failed to save stream, closing it");
                stream->Close();
              }
            },
            [self](mozilla::ipc::ResponseRejectReason) {
              NS_WARNING("SendStreamNeeded rejected");
              self->Close();
            });
      }));
}

// nsIInputStreamCallback

NS_IMETHODIMP
RemoteLazyInputStream::OnInputStreamReady(nsIAsyncInputStream* aStream) {
  RefPtr<nsIInputStreamCallback> callback;
  nsCOMPtr<nsIEventTarget> callbackEventTarget;
  {
    MutexAutoLock lock(mMutex);
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Debug,
            ("OnInputStreamReady %s", Describe().get()));

    // We have been closed in the meantime.
    if (mState == eClosed) {
      return NS_OK;
    }

    // We got a callback from the wrong stream, likely due to a `CloneWithRange`
    // call while we were waiting. Ignore this callback.
    if (mAsyncInnerStream != aStream) {
      return NS_OK;
    }

    MOZ_ASSERT(mState == eRunning);

    // The callback has been canceled in the meantime.
    if (!mInputStreamCallback) {
      return NS_OK;
    }

    callback.swap(mInputStreamCallback);
    callbackEventTarget.swap(mInputStreamCallbackEventTarget);
  }

  // This must be the last operation because the execution of the callback can
  // be synchronous.
  MOZ_ASSERT(callback);
  InputStreamCallbackRunnable::Execute(callback.forget(),
                                       callbackEventTarget.forget(), this);
  return NS_OK;
}

// nsIIPCSerializableInputStream

void RemoteLazyInputStream::SerializedComplexity(uint32_t aMaxSize,
                                                 uint32_t* aSizeUsed,
                                                 uint32_t* aNewPipes,
                                                 uint32_t* aTransferables) {
  *aTransferables = 1;
}

void RemoteLazyInputStream::Serialize(mozilla::ipc::InputStreamParams& aParams,
                                      uint32_t aMaxSize, uint32_t* aSizeUsed) {
  *aSizeUsed = 0;
  aParams = mozilla::ipc::RemoteLazyInputStreamParams(this);
}

bool RemoteLazyInputStream::Deserialize(
    const mozilla::ipc::InputStreamParams& aParams) {
  MOZ_CRASH("This should never be called.");
  return false;
}

// nsIAsyncFileMetadata

NS_IMETHODIMP
RemoteLazyInputStream::AsyncFileMetadataWait(nsIFileMetadataCallback* aCallback,
                                             nsIEventTarget* aEventTarget) {
  MOZ_ASSERT(!!aCallback == !!aEventTarget);

  // If we have the callback, we must have the event target.
  if (NS_WARN_IF(!!aCallback != !!aEventTarget)) {
    return NS_ERROR_FAILURE;
  }

  // See RemoteLazyInputStream.h for more information about this state
  // machine.

  {
    MutexAutoLock lock(mMutex);
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Debug,
            ("AsyncFileMetadataWait(%p, %p) %s", aCallback, aEventTarget,
             Describe().get()));

    switch (mState) {
      // First call, we need to retrieve the stream from the parent actor.
      case eInit:
        MOZ_ASSERT(mActor);

        mFileMetadataCallback = aCallback;
        mFileMetadataCallbackEventTarget = aEventTarget;
        mState = ePending;

        StreamNeeded();
        return NS_OK;

      // We are still waiting for the remote inputStream
      case ePending:
        if (mFileMetadataCallback && aCallback) {
          return NS_ERROR_FAILURE;
        }

        mFileMetadataCallback = aCallback;
        mFileMetadataCallbackEventTarget = aEventTarget;
        return NS_OK;

      // We have the remote inputStream, let's check if we can execute the
      // callback.
      case eRunning:
        break;

      // Stream is closed.
      default:
        MOZ_ASSERT(mState == eClosed);
        return NS_BASE_STREAM_CLOSED;
    }

    MOZ_ASSERT(mState == eRunning);
  }

  FileMetadataCallbackRunnable::Execute(aCallback, aEventTarget, this);
  return NS_OK;
}

// nsIFileMetadata

NS_IMETHODIMP
RemoteLazyInputStream::GetSize(int64_t* aRetval) {
  nsCOMPtr<nsIFileMetadata> fileMetadata;
  {
    MutexAutoLock lock(mMutex);
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
            ("GetSize %s", Describe().get()));

    fileMetadata = do_QueryInterface(mInnerStream);
    if (!fileMetadata) {
      return mState == eClosed ? NS_BASE_STREAM_CLOSED : NS_ERROR_FAILURE;
    }
  }

  return fileMetadata->GetSize(aRetval);
}

NS_IMETHODIMP
RemoteLazyInputStream::GetLastModified(int64_t* aRetval) {
  nsCOMPtr<nsIFileMetadata> fileMetadata;
  {
    MutexAutoLock lock(mMutex);
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
            ("GetLastModified %s", Describe().get()));

    fileMetadata = do_QueryInterface(mInnerStream);
    if (!fileMetadata) {
      return mState == eClosed ? NS_BASE_STREAM_CLOSED : NS_ERROR_FAILURE;
    }
  }

  return fileMetadata->GetLastModified(aRetval);
}

NS_IMETHODIMP
RemoteLazyInputStream::GetFileDescriptor(PRFileDesc** aRetval) {
  nsCOMPtr<nsIFileMetadata> fileMetadata;
  {
    MutexAutoLock lock(mMutex);
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
            ("GetFileDescriptor %s", Describe().get()));

    fileMetadata = do_QueryInterface(mInnerStream);
    if (!fileMetadata) {
      return mState == eClosed ? NS_BASE_STREAM_CLOSED : NS_ERROR_FAILURE;
    }
  }

  return fileMetadata->GetFileDescriptor(aRetval);
}

nsresult RemoteLazyInputStream::EnsureAsyncRemoteStream() {
  // We already have an async remote stream.
  if (mAsyncInnerStream) {
    return NS_OK;
  }

  MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Debug,
          ("EnsureAsyncRemoteStream %s", Describe().get()));

  if (NS_WARN_IF(!mInnerStream)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIInputStream> stream = mInnerStream;

  // Check if the stream is blocking, if it is, we want to make it non-blocking
  // using a pipe.
  bool nonBlocking = false;
  nsresult rv = stream->IsNonBlocking(&nonBlocking);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // We don't return NS_ERROR_NOT_IMPLEMENTED from ReadSegments,
  // so it's possible that callers are expecting us to succeed in the future.
  // We need to make sure the stream we return here supports ReadSegments,
  // so wrap if in a buffered stream if necessary.
  //
  // We only need to do this if we won't be wrapping the stream in a pipe, which
  // will add buffering anyway.
  if (nonBlocking && !NS_InputStreamIsBuffered(stream)) {
    nsCOMPtr<nsIInputStream> bufferedStream;
    nsresult rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream),
                                            stream.forget(), 4096);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    stream = bufferedStream;
  }

  nsCOMPtr<nsIAsyncInputStream> asyncStream = do_QueryInterface(stream);

  // If non-blocking and non-async, let's use NonBlockingAsyncInputStream.
  if (nonBlocking && !asyncStream) {
    rv = NonBlockingAsyncInputStream::Create(stream.forget(),
                                             getter_AddRefs(asyncStream));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(asyncStream);
  }

  if (!asyncStream) {
    // Let's make the stream async using the DOMFile thread.
    nsCOMPtr<nsIAsyncInputStream> pipeIn;
    nsCOMPtr<nsIAsyncOutputStream> pipeOut;
    rv = NS_NewPipe2(getter_AddRefs(pipeIn), getter_AddRefs(pipeOut), true,
                     true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    RefPtr<RemoteLazyInputStreamThread> thread =
        RemoteLazyInputStreamThread::GetOrCreate();
    if (NS_WARN_IF(!thread)) {
      return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
    }

    rv = NS_AsyncCopy(stream, pipeOut, thread, NS_ASYNCCOPY_VIA_WRITESEGMENTS);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    asyncStream = pipeIn;
  }

  MOZ_ASSERT(asyncStream);
  mAsyncInnerStream = asyncStream;
  mInnerStream = nullptr;

  return NS_OK;
}

// nsIInputStreamLength

NS_IMETHODIMP
RemoteLazyInputStream::Length(int64_t* aLength) {
  MutexAutoLock lock(mMutex);

  if (mState == eClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  if (!mActor) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_BASE_STREAM_WOULD_BLOCK;
}

namespace {

class InputStreamLengthCallbackRunnable final : public DiscardableRunnable {
 public:
  static void Execute(nsIInputStreamLengthCallback* aCallback,
                      nsIEventTarget* aEventTarget,
                      RemoteLazyInputStream* aStream, int64_t aLength) {
    MOZ_ASSERT(aCallback);
    MOZ_ASSERT(aEventTarget);

    RefPtr<InputStreamLengthCallbackRunnable> runnable =
        new InputStreamLengthCallbackRunnable(aCallback, aStream, aLength);

    nsCOMPtr<nsIEventTarget> target = aEventTarget;
    target->Dispatch(runnable, NS_DISPATCH_NORMAL);
  }

  NS_IMETHOD
  Run() override {
    mCallback->OnInputStreamLengthReady(mStream, mLength);
    mCallback = nullptr;
    mStream = nullptr;
    return NS_OK;
  }

 private:
  InputStreamLengthCallbackRunnable(nsIInputStreamLengthCallback* aCallback,
                                    RemoteLazyInputStream* aStream,
                                    int64_t aLength)
      : DiscardableRunnable("dom::InputStreamLengthCallbackRunnable"),
        mCallback(aCallback),
        mStream(aStream),
        mLength(aLength) {
    MOZ_ASSERT(mCallback);
    MOZ_ASSERT(mStream);
  }

  nsCOMPtr<nsIInputStreamLengthCallback> mCallback;
  RefPtr<RemoteLazyInputStream> mStream;
  const int64_t mLength;
};

}  // namespace

// nsIAsyncInputStreamLength

NS_IMETHODIMP
RemoteLazyInputStream::AsyncLengthWait(nsIInputStreamLengthCallback* aCallback,
                                       nsIEventTarget* aEventTarget) {
  // If we have the callback, we must have the event target.
  if (NS_WARN_IF(!!aCallback != !!aEventTarget)) {
    return NS_ERROR_FAILURE;
  }

  {
    MutexAutoLock lock(mMutex);

    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
            ("AsyncLengthWait(%p, %p) %s", aCallback, aEventTarget,
             Describe().get()));

    if (mActor) {
      if (aCallback) {
        auto* thread = RemoteLazyInputStreamThread::GetOrCreate();
        if (NS_WARN_IF(!thread)) {
          return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
        }
        thread->Dispatch(NS_NewRunnableFunction(
            "RemoteLazyInputStream::AsyncLengthWait",
            [self = RefPtr{this}, actor = mActor,
             callback = nsCOMPtr{aCallback},
             eventTarget = nsCOMPtr{aEventTarget}] {
              actor->SendLengthNeeded(
                  [self, callback, eventTarget](int64_t aLength) {
                    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
                            ("AsyncLengthWait resolve %" PRId64, aLength));
                    int64_t length = -1;
                    if (aLength > 0) {
                      uint64_t sourceLength =
                          aLength - std::min<uint64_t>(aLength, self->mStart);
                      length = int64_t(
                          std::min<uint64_t>(sourceLength, self->mLength));
                    }
                    InputStreamLengthCallbackRunnable::Execute(
                        callback, eventTarget, self, length);
                  },
                  [self, callback,
                   eventTarget](mozilla::ipc::ResponseRejectReason) {
                    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Warning,
                            ("AsyncLengthWait reject"));
                    InputStreamLengthCallbackRunnable::Execute(
                        callback, eventTarget, self, -1);
                  });
            }));
      }

      return NS_OK;
    }
  }

  MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
          ("AsyncLengthWait immediate"));

  // If execution has reached here, it means the stream is either closed or
  // consumed, and therefore the callback can be executed immediately
  InputStreamLengthCallbackRunnable::Execute(aCallback, aEventTarget, this, -1);
  return NS_OK;
}

void RemoteLazyInputStream::IPCWrite(IPC::MessageWriter* aWriter) {
  // If we have an actor still, serialize efficiently by cloning our actor to
  // maintain a reference to the parent side.
  RefPtr<RemoteLazyInputStreamChild> actor;

  nsCOMPtr<nsIInputStream> innerStream;

  RefPtr<nsIInputStreamCallback> inputStreamCallback;
  nsCOMPtr<nsIEventTarget> inputStreamCallbackEventTarget;

  {
    MutexAutoLock lock(mMutex);

    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
            ("Serialize %s", Describe().get()));

    actor = mActor.forget();

    if (mAsyncInnerStream) {
      MOZ_ASSERT(!mInnerStream);
      innerStream = mAsyncInnerStream.forget();
    } else {
      innerStream = mInnerStream.forget();
    }

    // TODO(Bug 1737783): Notify to the mFileMetadataCallback that this
    // lazy input stream has been closed.
    mFileMetadataCallback = nullptr;
    mFileMetadataCallbackEventTarget = nullptr;

    inputStreamCallback = mInputStreamCallback.forget();
    inputStreamCallbackEventTarget = mInputStreamCallbackEventTarget.forget();

    mState = eClosed;
  }

  if (inputStreamCallback) {
    InputStreamCallbackRunnable::Execute(
        inputStreamCallback.forget(), inputStreamCallbackEventTarget.forget(),
        this);
  }

  bool closed = !actor && !innerStream;
  IPC::WriteParam(aWriter, closed);
  if (closed) {
    return;
  }

  // If we still have a connection to our remote actor, create a clone endpoint
  // for it and tell it that the stream has been consumed. The clone of the
  // connection can be transferred to another process.
  if (actor) {
    MOZ_LOG(
        gRemoteLazyStreamLog, LogLevel::Debug,
        ("Serializing as actor: %s", nsIDToCString(actor->StreamID()).get()));
    // Create a clone of the actor, and then tell it that this stream is no
    // longer referencing it.
    mozilla::ipc::Endpoint<PRemoteLazyInputStreamParent> parentEp;
    mozilla::ipc::Endpoint<PRemoteLazyInputStreamChild> childEp;
    MOZ_ALWAYS_SUCCEEDS(
        PRemoteLazyInputStream::CreateEndpoints(&parentEp, &childEp));

    auto* thread = RemoteLazyInputStreamThread::GetOrCreate();
    if (thread) {
      thread->Dispatch(NS_NewRunnableFunction(
          "RemoteLazyInputStreamChild::SendClone",
          [actor, parentEp = std::move(parentEp)]() mutable {
            bool ok = actor->SendClone(std::move(parentEp));
            MOZ_LOG(
                gRemoteLazyStreamLog, LogLevel::Verbose,
                ("SendClone for %s: %s", nsIDToCString(actor->StreamID()).get(),
                 ok ? "OK" : "ERR"));
          }));

    }  // else we are shutting down xpcom threads.

    // NOTE: Call `StreamConsumed` after dispatching the `SendClone` runnable,
    // as this method may dispatch a runnable to `RemoteLazyInputStreamThread`
    // to call `SendGoodbye`, which needs to happen after `SendClone`.
    actor->StreamConsumed();

    IPC::WriteParam(aWriter, actor->StreamID());
    IPC::WriteParam(aWriter, mStart);
    IPC::WriteParam(aWriter, mLength);
    IPC::WriteParam(aWriter, std::move(childEp));

    if (innerStream) {
      innerStream->Close();
    }
    return;
  }

  // If we have a stream and are in the parent process, create a new actor pair
  // and transfer ownership of the stream into storage.
  auto streamStorage = RemoteLazyInputStreamStorage::Get();
  if (streamStorage.isOk()) {
    MOZ_ASSERT(XRE_IsParentProcess());
    nsID id = nsID::GenerateUUID();
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Debug,
            ("Serializing as new stream: %s", nsIDToCString(id).get()));

    streamStorage.inspect()->AddStream(innerStream, id);

    mozilla::ipc::Endpoint<PRemoteLazyInputStreamParent> parentEp;
    mozilla::ipc::Endpoint<PRemoteLazyInputStreamChild> childEp;
    MOZ_ALWAYS_SUCCEEDS(
        PRemoteLazyInputStream::CreateEndpoints(&parentEp, &childEp));

    // Bind the actor on our background thread.
    streamStorage.inspect()->TaskQueue()->Dispatch(NS_NewRunnableFunction(
        "RemoteLazyInputStreamParent::Bind",
        [parentEp = std::move(parentEp), id]() mutable {
          auto stream = MakeRefPtr<RemoteLazyInputStreamParent>(id);
          parentEp.Bind(stream);
        }));

    IPC::WriteParam(aWriter, id);
    IPC::WriteParam(aWriter, 0);
    IPC::WriteParam(aWriter, UINT64_MAX);
    IPC::WriteParam(aWriter, std::move(childEp));
    return;
  }

  MOZ_CRASH("Cannot serialize new RemoteLazyInputStream from this process");
}

already_AddRefed<RemoteLazyInputStream> RemoteLazyInputStream::IPCRead(
    IPC::MessageReader* aReader) {
  MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose, ("Deserialize"));

  bool closed;
  if (NS_WARN_IF(!IPC::ReadParam(aReader, &closed))) {
    return nullptr;
  }
  if (closed) {
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
            ("Deserialize closed stream"));
    return do_AddRef(new RemoteLazyInputStream());
  }

  nsID id{};
  uint64_t start;
  uint64_t length;
  mozilla::ipc::Endpoint<PRemoteLazyInputStreamChild> endpoint;
  if (NS_WARN_IF(!IPC::ReadParam(aReader, &id)) ||
      NS_WARN_IF(!IPC::ReadParam(aReader, &start)) ||
      NS_WARN_IF(!IPC::ReadParam(aReader, &length)) ||
      NS_WARN_IF(!IPC::ReadParam(aReader, &endpoint))) {
    return nullptr;
  }

  if (!endpoint.IsValid()) {
    MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Warning,
            ("Deserialize failed due to invalid endpoint!"));
    return do_AddRef(new RemoteLazyInputStream());
  }

  RefPtr<RemoteLazyInputStreamChild> actor =
      BindChildActor(id, std::move(endpoint));

  return do_AddRef(new RemoteLazyInputStream(actor, start, length));
}

}  // namespace mozilla

void IPC::ParamTraits<mozilla::RemoteLazyInputStream*>::Write(
    IPC::MessageWriter* aWriter, mozilla::RemoteLazyInputStream* aParam) {
  bool nonNull = !!aParam;
  IPC::WriteParam(aWriter, nonNull);
  if (aParam) {
    aParam->IPCWrite(aWriter);
  }
}

bool IPC::ParamTraits<mozilla::RemoteLazyInputStream*>::Read(
    IPC::MessageReader* aReader,
    RefPtr<mozilla::RemoteLazyInputStream>* aResult) {
  bool nonNull = false;
  if (!IPC::ReadParam(aReader, &nonNull)) {
    return false;
  }
  if (!nonNull) {
    *aResult = nullptr;
    return true;
  }
  *aResult = mozilla::RemoteLazyInputStream::IPCRead(aReader);
  return *aResult;
}
