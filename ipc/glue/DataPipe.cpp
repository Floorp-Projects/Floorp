/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DataPipe.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/Logging.h"
#include "mozilla/MoveOnlyFunction.h"
#include "mozilla/ipc/InputStreamParams.h"
#include "nsIAsyncInputStream.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace ipc {

LazyLogModule gDataPipeLog("DataPipe");

namespace data_pipe_detail {

// Helper for queueing up actions to be run once the mutex has been unlocked.
// Actions will be run in-order.
class SCOPED_CAPABILITY DataPipeAutoLock {
 public:
  explicit DataPipeAutoLock(Mutex& aMutex) CAPABILITY_ACQUIRE(aMutex)
      : mMutex(aMutex) {
    mMutex.Lock();
  }
  DataPipeAutoLock(const DataPipeAutoLock&) = delete;
  DataPipeAutoLock& operator=(const DataPipeAutoLock&) = delete;

  template <typename F>
  void AddUnlockAction(F aAction) {
    mActions.AppendElement(std::move(aAction));
  }

  ~DataPipeAutoLock() CAPABILITY_RELEASE() {
    mMutex.Unlock();
    for (auto& action : mActions) {
      action();
    }
  }

 private:
  Mutex& mMutex;
  AutoTArray<MoveOnlyFunction<void()>, 4> mActions;
};

static void DoNotifyOnUnlock(DataPipeAutoLock& aLock,
                             already_AddRefed<nsIRunnable> aCallback,
                             already_AddRefed<nsIEventTarget> aTarget) {
  nsCOMPtr<nsIRunnable> callback{std::move(aCallback)};
  nsCOMPtr<nsIEventTarget> target{std::move(aTarget)};
  if (callback) {
    aLock.AddUnlockAction(
        [callback = std::move(callback), target = std::move(target)]() mutable {
          if (target) {
            target->Dispatch(callback.forget());
          } else {
            NS_DispatchBackgroundTask(callback.forget());
          }
        });
  }
}

class DataPipeLink : public NodeController::PortObserver {
 public:
  DataPipeLink(bool aReceiverSide, std::shared_ptr<Mutex> aMutex,
               ScopedPort aPort, SharedMemory* aShmem, uint32_t aCapacity,
               nsresult aPeerStatus, uint32_t aOffset, uint32_t aAvailable)
      : mMutex(std::move(aMutex)),
        mPort(std::move(aPort)),
        mShmem(aShmem),
        mCapacity(aCapacity),
        mReceiverSide(aReceiverSide),
        mPeerStatus(aPeerStatus),
        mOffset(aOffset),
        mAvailable(aAvailable) {}

  void Init() EXCLUDES(*mMutex) {
    {
      DataPipeAutoLock lock(*mMutex);
      if (NS_FAILED(mPeerStatus)) {
        return;
      }
      MOZ_ASSERT(mPort.IsValid());
      mPort.Controller()->SetPortObserver(mPort.Port(), this);
    }
    OnPortStatusChanged();
  }

  void OnPortStatusChanged() final EXCLUDES(*mMutex);

  // Add a task to notify the callback after `aLock` is unlocked.
  //
  // This method is safe to call multiple times, as after the first time it is
  // called, `mCallback` will be cleared.
  void NotifyOnUnlock(DataPipeAutoLock& aLock) REQUIRES(*mMutex) {
    DoNotifyOnUnlock(aLock, mCallback.forget(), mCallbackTarget.forget());
  }

  void SendBytesConsumedOnUnlock(DataPipeAutoLock& aLock, uint32_t aBytes)
      REQUIRES(*mMutex) {
    MOZ_LOG(gDataPipeLog, LogLevel::Verbose,
            ("SendOnUnlock CONSUMED(%u) %s", aBytes, Describe(aLock).get()));
    if (NS_FAILED(mPeerStatus)) {
      return;
    }

    // `mPort` may be destroyed by `SetPeerError` after the DataPipe is unlocked
    // but before we send the message. The strong controller and port references
    // will allow us to try to send the message anyway, and it will be safely
    // dropped if the port has already been closed. CONSUMED messages are safe
    // to deliver out-of-order, so we don't need to worry about ordering here.
    aLock.AddUnlockAction([controller = RefPtr{mPort.Controller()},
                           port = mPort.Port(), aBytes]() mutable {
      auto message = MakeUnique<IPC::Message>(
          MSG_ROUTING_NONE, DATA_PIPE_BYTES_CONSUMED_MESSAGE_TYPE);
      IPC::MessageWriter writer(*message);
      WriteParam(&writer, aBytes);
      controller->SendUserMessage(port, std::move(message));
    });
  }

  void SetPeerError(DataPipeAutoLock& aLock, nsresult aStatus,
                    bool aSendClosed = false) REQUIRES(*mMutex) {
    MOZ_LOG(gDataPipeLog, LogLevel::Debug,
            ("SetPeerError(%s%s) %s", GetStaticErrorName(aStatus),
             aSendClosed ? ", send" : "", Describe(aLock).get()));
    // The pipe was closed or errored. Clear the observer reference back
    // to this type from the port layer, and ensure we notify waiters.
    MOZ_ASSERT(NS_SUCCEEDED(mPeerStatus));
    mPeerStatus = NS_SUCCEEDED(aStatus) ? NS_BASE_STREAM_CLOSED : aStatus;
    aLock.AddUnlockAction([port = std::move(mPort), aStatus, aSendClosed] {
      if (aSendClosed) {
        auto message = MakeUnique<IPC::Message>(MSG_ROUTING_NONE,
                                                DATA_PIPE_CLOSED_MESSAGE_TYPE);
        IPC::MessageWriter writer(*message);
        WriteParam(&writer, aStatus);
        port.Controller()->SendUserMessage(port.Port(), std::move(message));
      }
      // The `ScopedPort` being destroyed with this action will close it,
      // clearing the observer reference from the ports layer.
    });
    NotifyOnUnlock(aLock);
  }

  nsCString Describe(DataPipeAutoLock& aLock) const REQUIRES(*mMutex) {
    return nsPrintfCString(
        "[%s(%p) c=%u e=%s o=%u a=%u, cb=%s]",
        mReceiverSide ? "Receiver" : "Sender", this, mCapacity,
        GetStaticErrorName(mPeerStatus), mOffset, mAvailable,
        mCallback ? (mCallbackClosureOnly ? "clo" : "yes") : "no");
  }

  // This mutex is shared with the `DataPipeBase` which owns this
  // `DataPipeLink`.
  std::shared_ptr<Mutex> mMutex;

  ScopedPort mPort GUARDED_BY(*mMutex);
  const RefPtr<SharedMemory> mShmem;
  const uint32_t mCapacity;
  const bool mReceiverSide;

  bool mProcessingSegment GUARDED_BY(*mMutex) = false;

  nsresult mPeerStatus GUARDED_BY(*mMutex) = NS_OK;
  uint32_t mOffset GUARDED_BY(*mMutex) = 0;
  uint32_t mAvailable GUARDED_BY(*mMutex) = 0;

  bool mCallbackClosureOnly GUARDED_BY(*mMutex) = false;
  nsCOMPtr<nsIRunnable> mCallback GUARDED_BY(*mMutex);
  nsCOMPtr<nsIEventTarget> mCallbackTarget GUARDED_BY(*mMutex);
};

void DataPipeLink::OnPortStatusChanged() {
  DataPipeAutoLock lock(*mMutex);

  while (NS_SUCCEEDED(mPeerStatus)) {
    UniquePtr<IPC::Message> message;
    if (!mPort.Controller()->GetMessage(mPort.Port(), &message)) {
      SetPeerError(lock, NS_BASE_STREAM_CLOSED);
      return;
    }
    if (!message) {
      return;  // no more messages
    }

    IPC::MessageReader reader(*message);
    switch (message->type()) {
      case DATA_PIPE_CLOSED_MESSAGE_TYPE: {
        nsresult status = NS_OK;
        if (!ReadParam(&reader, &status)) {
          NS_WARNING("Unable to parse nsresult error from peer");
          status = NS_ERROR_UNEXPECTED;
        }
        MOZ_LOG(gDataPipeLog, LogLevel::Debug,
                ("Got CLOSED(%s) %s", GetStaticErrorName(status),
                 Describe(lock).get()));
        SetPeerError(lock, status);
        return;
      }
      case DATA_PIPE_BYTES_CONSUMED_MESSAGE_TYPE: {
        uint32_t consumed = 0;
        if (!ReadParam(&reader, &consumed)) {
          NS_WARNING("Unable to parse bytes consumed from peer");
          SetPeerError(lock, NS_ERROR_UNEXPECTED);
          return;
        }

        MOZ_LOG(gDataPipeLog, LogLevel::Verbose,
                ("Got CONSUMED(%u) %s", consumed, Describe(lock).get()));
        auto newAvailable = CheckedUint32{mAvailable} + consumed;
        if (!newAvailable.isValid() || newAvailable.value() > mCapacity) {
          NS_WARNING("Illegal bytes consumed message received from peer");
          SetPeerError(lock, NS_ERROR_UNEXPECTED);
          return;
        }
        mAvailable = newAvailable.value();
        if (!mCallbackClosureOnly) {
          NotifyOnUnlock(lock);
        }
        break;
      }
      default: {
        NS_WARNING("Illegal message type received from peer");
        SetPeerError(lock, NS_ERROR_UNEXPECTED);
        return;
      }
    }
  }
}

DataPipeBase::DataPipeBase(bool aReceiverSide, nsresult aError)
    : mMutex(std::make_shared<Mutex>(aReceiverSide ? "DataPipeReceiver"
                                                   : "DataPipeSender")),
      mStatus(NS_SUCCEEDED(aError) ? NS_BASE_STREAM_CLOSED : aError) {}

DataPipeBase::DataPipeBase(bool aReceiverSide, ScopedPort aPort,
                           SharedMemory* aShmem, uint32_t aCapacity,
                           nsresult aPeerStatus, uint32_t aOffset,
                           uint32_t aAvailable)
    : mMutex(std::make_shared<Mutex>(aReceiverSide ? "DataPipeReceiver"
                                                   : "DataPipeSender")),
      mStatus(NS_OK),
      mLink(new DataPipeLink(aReceiverSide, mMutex, std::move(aPort), aShmem,
                             aCapacity, aPeerStatus, aOffset, aAvailable)) {
  mLink->Init();
}

DataPipeBase::~DataPipeBase() {
  DataPipeAutoLock lock(*mMutex);
  CloseInternal(lock, NS_BASE_STREAM_CLOSED);
}

void DataPipeBase::CloseInternal(DataPipeAutoLock& aLock, nsresult aStatus) {
  if (NS_FAILED(mStatus)) {
    return;
  }

  MOZ_LOG(
      gDataPipeLog, LogLevel::Debug,
      ("Closing(%s) %s", GetStaticErrorName(aStatus), Describe(aLock).get()));

  // Set our status to an errored status.
  mStatus = NS_SUCCEEDED(aStatus) ? NS_BASE_STREAM_CLOSED : aStatus;
  RefPtr<DataPipeLink> link = mLink.forget();
  AssertSameMutex(link->mMutex);
  link->NotifyOnUnlock(aLock);

  // If our peer hasn't disappeared yet, clean up our connection to it.
  if (NS_SUCCEEDED(link->mPeerStatus)) {
    link->SetPeerError(aLock, mStatus, /* aSendClosed */ true);
  }
}

nsresult DataPipeBase::ProcessSegmentsInternal(
    uint32_t aCount, ProcessSegmentFun aProcessSegment,
    uint32_t* aProcessedCount) {
  *aProcessedCount = 0;

  while (*aProcessedCount < aCount) {
    DataPipeAutoLock lock(*mMutex);
    mMutex->AssertCurrentThreadOwns();

    MOZ_LOG(gDataPipeLog, LogLevel::Verbose,
            ("ProcessSegments(%u of %u) %s", *aProcessedCount, aCount,
             Describe(lock).get()));

    nsresult status = CheckStatus(lock);
    if (NS_FAILED(status)) {
      if (*aProcessedCount > 0) {
        return NS_OK;
      }
      return status == NS_BASE_STREAM_CLOSED ? NS_OK : status;
    }

    RefPtr<DataPipeLink> link = mLink;
    AssertSameMutex(link->mMutex);
    if (!link->mAvailable) {
      MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(link->mPeerStatus),
                            "CheckStatus will have returned an error");
      return *aProcessedCount > 0 ? NS_OK : NS_BASE_STREAM_WOULD_BLOCK;
    }

    MOZ_RELEASE_ASSERT(!link->mProcessingSegment,
                       "Only one thread may be processing a segment at a time");

    // Extract an iterator over the next contiguous region of the shared memory
    // buffer which will be used .
    char* start = static_cast<char*>(link->mShmem->memory()) + link->mOffset;
    char* iter = start;
    char* end = start + std::min({aCount - *aProcessedCount, link->mAvailable,
                                  link->mCapacity - link->mOffset});

    // Record the consumed region from our segment when exiting this scope,
    // telling our peer how many bytes were consumed. Hold on to `mLink` to keep
    // the shmem mapped and make sure we can clean up even if we're closed while
    // processing the shmem region.
    link->mProcessingSegment = true;
    auto scopeExit = MakeScopeExit([&] {
      mMutex->AssertCurrentThreadOwns();  // should still be held
      AssertSameMutex(link->mMutex);

      MOZ_RELEASE_ASSERT(link->mProcessingSegment);
      link->mProcessingSegment = false;
      uint32_t totalProcessed = iter - start;
      if (totalProcessed > 0) {
        link->mOffset += totalProcessed;
        MOZ_RELEASE_ASSERT(link->mOffset <= link->mCapacity);
        if (link->mOffset == link->mCapacity) {
          link->mOffset = 0;
        }
        link->mAvailable -= totalProcessed;
        link->SendBytesConsumedOnUnlock(lock, totalProcessed);
      }
      MOZ_LOG(gDataPipeLog, LogLevel::Verbose,
              ("Processed Segment(%u of %zu) %s", totalProcessed, end - start,
               Describe(lock).get()));
    });

    {
      MutexAutoUnlock unlock(*mMutex);
      while (iter < end) {
        uint32_t processed = 0;
        Span segment{iter, end};
        nsresult rv = aProcessSegment(segment, *aProcessedCount, &processed);
        if (NS_FAILED(rv) || processed == 0) {
          return NS_OK;
        }

        MOZ_RELEASE_ASSERT(processed <= segment.Length());
        iter += processed;
        *aProcessedCount += processed;
      }
    }
  }
  MOZ_DIAGNOSTIC_ASSERT(*aProcessedCount == aCount,
                        "Must have processed exactly aCount");
  return NS_OK;
}

void DataPipeBase::AsyncWaitInternal(already_AddRefed<nsIRunnable> aCallback,
                                     already_AddRefed<nsIEventTarget> aTarget,
                                     bool aClosureOnly) {
  RefPtr<nsIRunnable> callback = std::move(aCallback);
  RefPtr<nsIEventTarget> target = std::move(aTarget);

  DataPipeAutoLock lock(*mMutex);
  MOZ_LOG(gDataPipeLog, LogLevel::Debug,
          ("AsyncWait %s %p %s", aClosureOnly ? "(closure)" : "(ready)",
           callback.get(), Describe(lock).get()));

  if (NS_FAILED(CheckStatus(lock))) {
#ifdef DEBUG
    if (mLink) {
      AssertSameMutex(mLink->mMutex);
      MOZ_ASSERT(!mLink->mCallback);
    }
#endif
    DoNotifyOnUnlock(lock, callback.forget(), target.forget());
    return;
  }

  AssertSameMutex(mLink->mMutex);

  // NOTE: After this point, `mLink` may have previously had a callback which is
  // now being cancelled, make sure we clear `mCallback` even if we're going to
  // call `aCallback` immediately.
  mLink->mCallback = callback.forget();
  mLink->mCallbackTarget = target.forget();
  mLink->mCallbackClosureOnly = aClosureOnly;
  if (!aClosureOnly && mLink->mAvailable) {
    mLink->NotifyOnUnlock(lock);
  }
}

nsresult DataPipeBase::CheckStatus(DataPipeAutoLock& aLock) {
  // If our peer has closed or errored, we may need to close our local side to
  // reflect the error code our peer provided. If we're a sender, we want to
  // become closed immediately, whereas if we're a receiver we want to wait
  // until our available buffer has been exhausted.
  //
  // NOTE: There may still be 2-stage writes/reads ongoing at this point, which
  // will continue due to `mLink` being kept alive by the
  // `ProcessSegmentsInternal` function.
  if (NS_FAILED(mStatus)) {
    return mStatus;
  }
  AssertSameMutex(mLink->mMutex);
  if (NS_FAILED(mLink->mPeerStatus) &&
      (!mLink->mReceiverSide || !mLink->mAvailable)) {
    CloseInternal(aLock, mLink->mPeerStatus);
  }
  return mStatus;
}

nsCString DataPipeBase::Describe(DataPipeAutoLock& aLock) {
  if (mLink) {
    AssertSameMutex(mLink->mMutex);
    return mLink->Describe(aLock);
  }
  return nsPrintfCString("[status=%s]", GetStaticErrorName(mStatus));
}

template <typename T>
void DataPipeWrite(IPC::MessageWriter* aWriter, T* aParam) {
  DataPipeAutoLock lock(*aParam->mMutex);
  MOZ_LOG(gDataPipeLog, LogLevel::Debug,
          ("IPC Write: %s", aParam->Describe(lock).get()));

  WriteParam(aWriter, aParam->mStatus);
  if (NS_FAILED(aParam->mStatus)) {
    return;
  }

  aParam->AssertSameMutex(aParam->mLink->mMutex);
  MOZ_RELEASE_ASSERT(!aParam->mLink->mProcessingSegment,
                     "cannot transfer while processing a segment");

  // Serialize relevant parameters to our peer.
  WriteParam(aWriter, std::move(aParam->mLink->mPort));
  if (!aParam->mLink->mShmem->WriteHandle(aWriter)) {
    aWriter->FatalError("failed to write DataPipe shmem handle");
    MOZ_CRASH("failed to write DataPipe shmem handle");
  }
  WriteParam(aWriter, aParam->mLink->mCapacity);
  WriteParam(aWriter, aParam->mLink->mPeerStatus);
  WriteParam(aWriter, aParam->mLink->mOffset);
  WriteParam(aWriter, aParam->mLink->mAvailable);

  // Mark our peer as closed so we don't try to send to it when closing.
  aParam->mLink->mPeerStatus = NS_ERROR_NOT_INITIALIZED;
  aParam->CloseInternal(lock, NS_ERROR_NOT_INITIALIZED);
}

template <typename T>
bool DataPipeRead(IPC::MessageReader* aReader, RefPtr<T>* aResult) {
  nsresult rv = NS_OK;
  if (!ReadParam(aReader, &rv)) {
    aReader->FatalError("failed to read DataPipe status");
    return false;
  }
  if (NS_FAILED(rv)) {
    *aResult = new T(rv);
    MOZ_LOG(gDataPipeLog, LogLevel::Debug,
            ("IPC Read: [status=%s]", GetStaticErrorName(rv)));
    return true;
  }

  ScopedPort port;
  if (!ReadParam(aReader, &port)) {
    aReader->FatalError("failed to read DataPipe port");
    return false;
  }
  RefPtr shmem = new SharedMemoryBasic();
  if (!shmem->ReadHandle(aReader)) {
    aReader->FatalError("failed to read DataPipe shmem");
    return false;
  }
  uint32_t capacity = 0;
  nsresult peerStatus = NS_OK;
  uint32_t offset = 0;
  uint32_t available = 0;
  if (!ReadParam(aReader, &capacity) || !ReadParam(aReader, &peerStatus) ||
      !ReadParam(aReader, &offset) || !ReadParam(aReader, &available)) {
    aReader->FatalError("failed to read DataPipe fields");
    return false;
  }
  if (!capacity || offset >= capacity || available > capacity) {
    aReader->FatalError("received DataPipe state values are inconsistent");
    return false;
  }
  if (!shmem->Map(SharedMemory::PageAlignedSize(capacity))) {
    aReader->FatalError("failed to map DataPipe shared memory region");
    return false;
  }

  *aResult =
      new T(std::move(port), shmem, capacity, peerStatus, offset, available);
  if (MOZ_LOG_TEST(gDataPipeLog, LogLevel::Debug)) {
    DataPipeAutoLock lock(*(*aResult)->mMutex);
    MOZ_LOG(gDataPipeLog, LogLevel::Debug,
            ("IPC Read: %s", (*aResult)->Describe(lock).get()));
  }
  return true;
}

}  // namespace data_pipe_detail

//-----------------------------------------------------------------------------
// DataPipeSender
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(DataPipeSender, nsIOutputStream, nsIAsyncOutputStream,
                  DataPipeSender)

// nsIOutputStream

NS_IMETHODIMP DataPipeSender::Close() {
  return CloseWithStatus(NS_BASE_STREAM_CLOSED);
}

NS_IMETHODIMP DataPipeSender::Flush() { return NS_OK; }

NS_IMETHODIMP DataPipeSender::Write(const char* aBuf, uint32_t aCount,
                                    uint32_t* aWriteCount) {
  return WriteSegments(NS_CopyBufferToSegment, (void*)aBuf, aCount,
                       aWriteCount);
}

NS_IMETHODIMP DataPipeSender::WriteFrom(nsIInputStream* aFromStream,
                                        uint32_t aCount,
                                        uint32_t* aWriteCount) {
  return WriteSegments(NS_CopyStreamToSegment, aFromStream, aCount,
                       aWriteCount);
}

NS_IMETHODIMP DataPipeSender::WriteSegments(nsReadSegmentFun aReader,
                                            void* aClosure, uint32_t aCount,
                                            uint32_t* aWriteCount) {
  auto processSegment = [&](Span<char> aSpan, uint32_t aToOffset,
                            uint32_t* aReadCount) -> nsresult {
    return aReader(this, aClosure, aSpan.data(), aToOffset, aSpan.Length(),
                   aReadCount);
  };
  return ProcessSegmentsInternal(aCount, processSegment, aWriteCount);
}

NS_IMETHODIMP DataPipeSender::IsNonBlocking(bool* _retval) {
  *_retval = true;
  return NS_OK;
}

// nsIAsyncOutputStream

NS_IMETHODIMP DataPipeSender::CloseWithStatus(nsresult reason) {
  data_pipe_detail::DataPipeAutoLock lock(*mMutex);
  CloseInternal(lock, reason);
  return NS_OK;
}

NS_IMETHODIMP DataPipeSender::AsyncWait(nsIOutputStreamCallback* aCallback,
                                        uint32_t aFlags,
                                        uint32_t aRequestedCount,
                                        nsIEventTarget* aTarget) {
  AsyncWaitInternal(
      aCallback ? NS_NewCancelableRunnableFunction(
                      "DataPipeReceiver::AsyncWait",
                      [self = RefPtr{this}, callback = RefPtr{aCallback}] {
                        MOZ_LOG(gDataPipeLog, LogLevel::Debug,
                                ("Calling OnOutputStreamReady(%p, %p)",
                                 callback.get(), self.get()));
                        callback->OnOutputStreamReady(self);
                      })
                : nullptr,
      do_AddRef(aTarget), aFlags & WAIT_CLOSURE_ONLY);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// DataPipeReceiver
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(DataPipeReceiver, nsIInputStream, nsIAsyncInputStream,
                  nsIIPCSerializableInputStream, DataPipeReceiver)

// nsIInputStream

NS_IMETHODIMP DataPipeReceiver::Close() {
  return CloseWithStatus(NS_BASE_STREAM_CLOSED);
}

NS_IMETHODIMP DataPipeReceiver::Available(uint64_t* _retval) {
  data_pipe_detail::DataPipeAutoLock lock(*mMutex);
  nsresult rv = CheckStatus(lock);
  if (NS_FAILED(rv)) {
    return rv;
  }
  AssertSameMutex(mLink->mMutex);
  *_retval = mLink->mAvailable;
  return NS_OK;
}

NS_IMETHODIMP DataPipeReceiver::Read(char* aBuf, uint32_t aCount,
                                     uint32_t* aReadCount) {
  return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, aReadCount);
}

NS_IMETHODIMP DataPipeReceiver::ReadSegments(nsWriteSegmentFun aWriter,
                                             void* aClosure, uint32_t aCount,
                                             uint32_t* aReadCount) {
  auto processSegment = [&](Span<char> aSpan, uint32_t aToOffset,
                            uint32_t* aWriteCount) -> nsresult {
    return aWriter(this, aClosure, aSpan.data(), aToOffset, aSpan.Length(),
                   aWriteCount);
  };
  return ProcessSegmentsInternal(aCount, processSegment, aReadCount);
}

NS_IMETHODIMP DataPipeReceiver::IsNonBlocking(bool* _retval) {
  *_retval = true;
  return NS_OK;
}

// nsIAsyncInputStream

NS_IMETHODIMP DataPipeReceiver::CloseWithStatus(nsresult aStatus) {
  data_pipe_detail::DataPipeAutoLock lock(*mMutex);
  CloseInternal(lock, aStatus);
  return NS_OK;
}

NS_IMETHODIMP DataPipeReceiver::AsyncWait(nsIInputStreamCallback* aCallback,
                                          uint32_t aFlags,
                                          uint32_t aRequestedCount,
                                          nsIEventTarget* aTarget) {
  AsyncWaitInternal(
      aCallback ? NS_NewCancelableRunnableFunction(
                      "DataPipeReceiver::AsyncWait",
                      [self = RefPtr{this}, callback = RefPtr{aCallback}] {
                        MOZ_LOG(gDataPipeLog, LogLevel::Debug,
                                ("Calling OnInputStreamReady(%p, %p)",
                                 callback.get(), self.get()));
                        callback->OnInputStreamReady(self);
                      })
                : nullptr,
      do_AddRef(aTarget), aFlags & WAIT_CLOSURE_ONLY);
  return NS_OK;
}

// nsIIPCSerializableInputStream

void DataPipeReceiver::SerializedComplexity(uint32_t aMaxSize,
                                            uint32_t* aSizeUsed,
                                            uint32_t* aPipes,
                                            uint32_t* aTransferables) {
  // We report DataPipeReceiver as taking one transferrable to serialize, rather
  // than one pipe, as we aren't starting a new pipe for this purpose, and are
  // instead transferring an existing pipe.
  *aTransferables = 1;
}

void DataPipeReceiver::Serialize(InputStreamParams& aParams, uint32_t aMaxSize,
                                 uint32_t* aSizeUsed) {
  *aSizeUsed = 0;
  aParams = DataPipeReceiverStreamParams(this);
}

bool DataPipeReceiver::Deserialize(const InputStreamParams& aParams) {
  MOZ_CRASH("Handled directly in `DeserializeInputStream`");
}

//-----------------------------------------------------------------------------
// NewDataPipe
//-----------------------------------------------------------------------------

nsresult NewDataPipe(uint32_t aCapacity, DataPipeSender** aSender,
                     DataPipeReceiver** aReceiver) {
  if (!aCapacity) {
    aCapacity = kDefaultDataPipeCapacity;
  }

  RefPtr<NodeController> controller = NodeController::GetSingleton();
  if (!controller) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

  auto [senderPort, receiverPort] = controller->CreatePortPair();
  auto shmem = MakeRefPtr<SharedMemoryBasic>();
  size_t alignedCapacity = SharedMemory::PageAlignedSize(aCapacity);
  if (!shmem->Create(alignedCapacity) || !shmem->Map(alignedCapacity)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  RefPtr sender = new DataPipeSender(std::move(senderPort), shmem, aCapacity,
                                     NS_OK, 0, aCapacity);
  RefPtr receiver = new DataPipeReceiver(std::move(receiverPort), shmem,
                                         aCapacity, NS_OK, 0, 0);
  sender.forget(aSender);
  receiver.forget(aReceiver);
  return NS_OK;
}

}  // namespace ipc
}  // namespace mozilla

void IPC::ParamTraits<mozilla::ipc::DataPipeSender*>::Write(
    MessageWriter* aWriter, mozilla::ipc::DataPipeSender* aParam) {
  mozilla::ipc::data_pipe_detail::DataPipeWrite(aWriter, aParam);
}

bool IPC::ParamTraits<mozilla::ipc::DataPipeSender*>::Read(
    MessageReader* aReader, RefPtr<mozilla::ipc::DataPipeSender>* aResult) {
  return mozilla::ipc::data_pipe_detail::DataPipeRead(aReader, aResult);
}

void IPC::ParamTraits<mozilla::ipc::DataPipeReceiver*>::Write(
    MessageWriter* aWriter, mozilla::ipc::DataPipeReceiver* aParam) {
  mozilla::ipc::data_pipe_detail::DataPipeWrite(aWriter, aParam);
}

bool IPC::ParamTraits<mozilla::ipc::DataPipeReceiver*>::Read(
    MessageReader* aReader, RefPtr<mozilla::ipc::DataPipeReceiver>* aResult) {
  return mozilla::ipc::data_pipe_detail::DataPipeRead(aReader, aResult);
}
