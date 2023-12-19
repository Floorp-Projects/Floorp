/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasDrawEventRecorder.h"

#include <string.h>

#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/layers/CanvasChild.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/TextureRecorded.h"
#include "mozilla/layers/SharedSurfacesChild.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "RecordedCanvasEventImpl.h"

namespace mozilla {
namespace layers {

struct ShmemAndHandle {
  RefPtr<ipc::SharedMemoryBasic> shmem;
  Handle handle;
};

static Maybe<ShmemAndHandle> CreateAndMapShmem(size_t aSize) {
  auto shmem = MakeRefPtr<ipc::SharedMemoryBasic>();
  if (!shmem->Create(aSize) || !shmem->Map(aSize)) {
    return Nothing();
  }

  auto shmemHandle = shmem->TakeHandle();
  if (!shmemHandle) {
    return Nothing();
  }

  return Some(ShmemAndHandle{shmem.forget(), std::move(shmemHandle)});
}

CanvasDrawEventRecorder::CanvasDrawEventRecorder(
    dom::ThreadSafeWorkerRef* aWorkerRef)
    : mWorkerRef(aWorkerRef), mIsOnWorker(!!aWorkerRef) {
  mDefaultBufferSize = ipc::SharedMemory::PageAlignedSize(
      StaticPrefs::gfx_canvas_remote_default_buffer_size());
  mMaxSpinCount = StaticPrefs::gfx_canvas_remote_max_spin_count();
  mDropBufferLimit = StaticPrefs::gfx_canvas_remote_drop_buffer_limit();
  mDropBufferOnZero = mDropBufferLimit;
}

CanvasDrawEventRecorder::~CanvasDrawEventRecorder() {
  MOZ_ASSERT(!mWorkerRef);
  MOZ_ASSERT(mRecordedTextures.IsEmpty());
}

bool CanvasDrawEventRecorder::Init(TextureType aTextureType,
                                   gfx::BackendType aBackendType,
                                   UniquePtr<Helpers> aHelpers) {
  NS_ASSERT_OWNINGTHREAD(CanvasDrawEventRecorder);

  mHelpers = std::move(aHelpers);

  MOZ_ASSERT(mTextureType == TextureType::Unknown);
  auto header = CreateAndMapShmem(sizeof(Header));
  if (NS_WARN_IF(header.isNothing())) {
    return false;
  }

  mHeader = static_cast<Header*>(header->shmem->memory());
  mHeader->eventCount = 0;
  mHeader->writerWaitCount = 0;
  mHeader->writerState = State::Processing;
  mHeader->processedCount = 0;
  mHeader->readerState = State::Processing;

  // We always keep at least two buffers. This means that when we
  // have to add a new buffer, there is at least a full buffer that requires
  // translating while the handle is sent over.
  AutoTArray<Handle, 2> bufferHandles;
  auto buffer = CreateAndMapShmem(mDefaultBufferSize);
  if (NS_WARN_IF(buffer.isNothing())) {
    return false;
  }
  mCurrentBuffer = CanvasBuffer(std::move(buffer->shmem));
  bufferHandles.AppendElement(std::move(buffer->handle));

  buffer = CreateAndMapShmem(mDefaultBufferSize);
  if (NS_WARN_IF(buffer.isNothing())) {
    return false;
  }
  mRecycledBuffers.emplace(buffer->shmem.forget(), 0);
  bufferHandles.AppendElement(std::move(buffer->handle));

  mWriterSemaphore.reset(CrossProcessSemaphore::Create("CanvasRecorder", 0));
  auto writerSem = mWriterSemaphore->CloneHandle();
  mWriterSemaphore->CloseHandle();
  if (NS_WARN_IF(!IsHandleValid(writerSem))) {
    return false;
  }

  mReaderSemaphore.reset(CrossProcessSemaphore::Create("CanvasTranslator", 0));
  auto readerSem = mReaderSemaphore->CloneHandle();
  mReaderSemaphore->CloseHandle();
  if (NS_WARN_IF(!IsHandleValid(readerSem))) {
    return false;
  }

  if (NS_WARN_IF(!mHelpers->InitTranslator(
          aTextureType, aBackendType, std::move(header->handle),
          std::move(bufferHandles), mDefaultBufferSize, std::move(readerSem),
          std::move(writerSem),
          /* aUseIPDLThread */ false))) {
    return false;
  }

  mTextureType = aTextureType;
  mHeaderShmem = header->shmem;
  return true;
}

void CanvasDrawEventRecorder::RecordEvent(const gfx::RecordedEvent& aEvent) {
  NS_ASSERT_OWNINGTHREAD(CanvasDrawEventRecorder);
  aEvent.RecordToStream(*this);
}

int64_t CanvasDrawEventRecorder::CreateCheckpoint() {
  NS_ASSERT_OWNINGTHREAD(CanvasDrawEventRecorder);
  int64_t checkpoint = mHeader->eventCount;
  RecordEvent(RecordedCheckpoint());
  return checkpoint;
}

bool CanvasDrawEventRecorder::WaitForCheckpoint(int64_t aCheckpoint) {
  NS_ASSERT_OWNINGTHREAD(CanvasDrawEventRecorder);

  uint32_t spinCount = mMaxSpinCount;
  do {
    if (mHeader->processedCount >= aCheckpoint) {
      return true;
    }
  } while (--spinCount != 0);

  mHeader->writerState = State::AboutToWait;
  if (mHeader->processedCount >= aCheckpoint) {
    mHeader->writerState = State::Processing;
    return true;
  }

  mHeader->writerWaitCount = aCheckpoint;
  mHeader->writerState = State::Waiting;

  // Wait unless we detect the reading side has closed.
  while (!mHelpers->ReaderClosed() && mHeader->readerState != State::Failed) {
    if (mWriterSemaphore->Wait(Some(TimeDuration::FromMilliseconds(100)))) {
      MOZ_ASSERT(mHeader->processedCount >= aCheckpoint);
      return true;
    }
  }

  // Either the reader has failed or we're stopping writing for some other
  // reason (e.g. shutdown), so mark us as failed so the reader is aware.
  mHeader->writerState = State::Failed;
  return false;
}

void CanvasDrawEventRecorder::WriteInternalEvent(EventType aEventType) {
  MOZ_ASSERT(mCurrentBuffer.SizeRemaining() > 0);

  WriteElement(mCurrentBuffer.Writer(), aEventType);
  IncrementEventCount();
}

gfx::ContiguousBuffer& CanvasDrawEventRecorder::GetContiguousBuffer(
    size_t aSize) {
  if (!mCurrentBuffer.IsValid()) {
    // If the current buffer is invalid then we've already failed previously.
    MOZ_ASSERT(mHeader->writerState == State::Failed);
    return mCurrentBuffer;
  }

  // We make sure that our buffer can hold aSize + 1 to ensure we always have
  // room for the end of buffer event.

  // Check if there is enough room is our current buffer.
  if (mCurrentBuffer.SizeRemaining() > aSize) {
    return mCurrentBuffer;
  }

  // If the next recycled buffer is big enough and free use that.
  if (mRecycledBuffers.front().Capacity() > aSize &&
      mRecycledBuffers.front().eventCount <= mHeader->processedCount) {
    // Only queue default size buffers for recycling.
    if (mCurrentBuffer.Capacity() == mDefaultBufferSize) {
      WriteInternalEvent(RECYCLE_BUFFER);
      mRecycledBuffers.emplace(std::move(mCurrentBuffer.shmem),
                               mHeader->eventCount);
    } else {
      WriteInternalEvent(DROP_BUFFER);
    }

    mCurrentBuffer = CanvasBuffer(std::move(mRecycledBuffers.front().shmem));
    mRecycledBuffers.pop();

    // If we have more than one recycled buffers free a configured number of
    // times in a row then drop one.
    if (mRecycledBuffers.size() > 1 &&
        mRecycledBuffers.front().eventCount < mHeader->processedCount) {
      if (--mDropBufferOnZero == 0) {
        WriteInternalEvent(DROP_BUFFER);
        mCurrentBuffer =
            CanvasBuffer(std::move(mRecycledBuffers.front().shmem));
        mRecycledBuffers.pop();
        mDropBufferOnZero = 1;
      }
    } else {
      mDropBufferOnZero = mDropBufferLimit;
    }

    return mCurrentBuffer;
  }

  // We don't have a buffer free or it is not big enough, so create a new one.
  WriteInternalEvent(PAUSE_TRANSLATION);

  // Only queue default size buffers for recycling.
  if (mCurrentBuffer.Capacity() == mDefaultBufferSize) {
    mRecycledBuffers.emplace(std::move(mCurrentBuffer.shmem),
                             mHeader->eventCount);
  }

  size_t bufferSize = std::max(mDefaultBufferSize,
                               ipc::SharedMemory::PageAlignedSize(aSize + 1));
  auto newBuffer = CreateAndMapShmem(bufferSize);
  if (NS_WARN_IF(newBuffer.isNothing())) {
    mHeader->writerState = State::Failed;
    mCurrentBuffer = CanvasBuffer();
    return mCurrentBuffer;
  }

  if (!mHelpers->AddBuffer(std::move(newBuffer->handle), bufferSize)) {
    mHeader->writerState = State::Failed;
    mCurrentBuffer = CanvasBuffer();
    return mCurrentBuffer;
  }

  mCurrentBuffer = CanvasBuffer(std::move(newBuffer->shmem));
  return mCurrentBuffer;
}

void CanvasDrawEventRecorder::DropFreeBuffers() {
  while (mRecycledBuffers.size() > 1 &&
         mRecycledBuffers.front().eventCount < mHeader->processedCount) {
    // If we encountered an error, we may have invalidated mCurrentBuffer in
    // GetContiguousBuffer. No need to write the DROP_BUFFER event.
    if (mCurrentBuffer.IsValid()) {
      WriteInternalEvent(DROP_BUFFER);
    }
    mCurrentBuffer = CanvasBuffer(std::move(mRecycledBuffers.front().shmem));
    mRecycledBuffers.pop();
  }
}

void CanvasDrawEventRecorder::IncrementEventCount() {
  mHeader->eventCount++;
  CheckAndSignalReader();
}

void CanvasDrawEventRecorder::CheckAndSignalReader() {
  do {
    switch (mHeader->readerState) {
      case State::Processing:
      case State::Paused:
      case State::Failed:
        return;
      case State::AboutToWait:
        // The reader is making a decision about whether to wait. So, we must
        // wait until it has decided to avoid races. Check if the reader is
        // closed to avoid hangs.
        if (mHelpers->ReaderClosed()) {
          return;
        }
        continue;
      case State::Waiting:
        if (mHeader->processedCount < mHeader->eventCount) {
          // We have to use compareExchange here because the reader can change
          // from Waiting to Stopped.
          if (mHeader->readerState.compareExchange(State::Waiting,
                                                   State::Processing)) {
            mReaderSemaphore->Signal();
            return;
          }

          MOZ_ASSERT(mHeader->readerState == State::Stopped);
          continue;
        }
        return;
      case State::Stopped:
        if (mHeader->processedCount < mHeader->eventCount) {
          mHeader->readerState = State::Processing;
          if (!mHelpers->RestartReader()) {
            mHeader->writerState = State::Failed;
          }
        }
        return;
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid waiting state.");
        return;
    }
  } while (true);
}

void CanvasDrawEventRecorder::DetachResources() {
  NS_ASSERT_OWNINGTHREAD(CanvasDrawEventRecorder);

  nsTHashSet<RecordedTextureData*> recordedTextures =
      std::move(mRecordedTextures);
  for (const auto& texture : recordedTextures) {
    texture->DestroyOnOwningThread();
  }

  nsTHashMap<void*, ThreadSafeWeakPtr<SourceSurfaceCanvasRecording>>
      recordedSurfaces = std::move(mRecordedSurfaces);
  for (const auto& entry : recordedSurfaces) {
    RefPtr<SourceSurfaceCanvasRecording> surface(entry.GetData());
    if (surface) {
      surface->DestroyOnOwningThread();
    }
  }

  // There may be pending deletions waiting on the ImageBridgeChild thread for
  // this recorder. Let's make sure we handle any outstanding events before
  // destroying our worker reference.
  if (mIsOnWorker) {
    if (RefPtr<ImageBridgeChild> imageBridge =
            ImageBridgeChild::GetSingleton()) {
      imageBridge->FlushEvents();
    }
  }

  DrawEventRecorderPrivate::DetachResources();

  {
    auto lockedPendingDeletions = mPendingDeletions.Lock();
    mWorkerRef = nullptr;
  }

  if (mHelpers) {
    mHelpers->Destroy();
  }
}

bool CanvasDrawEventRecorder::IsOnOwningThread() {
  auto lockedPendingDeletions = mPendingDeletions.Lock();

  if (mWorkerRef) {
    return mWorkerRef->Private()->IsOnCurrentThread();
  }

  MOZ_RELEASE_ASSERT(!mIsOnWorker, "Worker already shutdown!");
  return NS_IsMainThread();
}

void CanvasDrawEventRecorder::QueueProcessPendingDeletionsLocked(
    RefPtr<CanvasDrawEventRecorder>&& aRecorder) {
  if (!mWorkerRef) {
    MOZ_RELEASE_ASSERT(
        !mIsOnWorker,
        "QueueProcessPendingDeletionsLocked called after worker shutdown!");

    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "CanvasDrawEventRecorder::QueueProcessPendingDeletionsLocked",
        [self = std::move(aRecorder)]() { self->ProcessPendingDeletions(); }));
    return;
  }

  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "CanvasDrawEventRecorder::QueueProcessPendingDeletionsLocked",
        [self = std::move(aRecorder)]() mutable {
          self->QueueProcessPendingDeletions(std::move(self));
        }));
    return;
  }

  class ProcessPendingRunnable final : public dom::WorkerRunnable {
   public:
    ProcessPendingRunnable(dom::WorkerPrivate* aWorkerPrivate,
                           RefPtr<CanvasDrawEventRecorder>&& aRecorder)
        : dom::WorkerRunnable(aWorkerPrivate),
          mRecorder(std::move(aRecorder)) {}

    bool WorkerRun(JSContext*, dom::WorkerPrivate*) override {
      RefPtr<CanvasDrawEventRecorder> recorder = std::move(mRecorder);
      recorder->ProcessPendingDeletions();
      return true;
    }

   private:
    RefPtr<CanvasDrawEventRecorder> mRecorder;
  };

  auto task = MakeRefPtr<ProcessPendingRunnable>(mWorkerRef->Private(),
                                                 std::move(aRecorder));
  if (NS_WARN_IF(!task->Dispatch())) {
    MOZ_CRASH("ProcessPendingRunnable leaked!");
  }
}

void CanvasDrawEventRecorder::QueueProcessPendingDeletions(
    RefPtr<CanvasDrawEventRecorder>&& aRecorder) {
  auto lockedPendingDeletions = mPendingDeletions.Lock();
  if (lockedPendingDeletions->empty()) {
    // We raced to handle the deletions, and something got there first.
    return;
  }

  QueueProcessPendingDeletionsLocked(std::move(aRecorder));
}

void CanvasDrawEventRecorder::AddPendingDeletion(
    std::function<void()>&& aPendingDeletion) {
  PendingDeletionsVector pendingDeletions;

  {
    auto lockedPendingDeletions = mPendingDeletions.Lock();
    bool wasEmpty = lockedPendingDeletions->empty();
    lockedPendingDeletions->emplace_back(std::move(aPendingDeletion));

    MOZ_RELEASE_ASSERT(!mIsOnWorker || mWorkerRef,
                       "AddPendingDeletion called after worker shutdown!");

    // If we are not on the owning thread, we must queue an event to run the
    // deletions, if we transitioned from empty to non-empty.
    if ((mWorkerRef && !mWorkerRef->Private()->IsOnCurrentThread()) ||
        (!mWorkerRef && !NS_IsMainThread())) {
      if (wasEmpty) {
        RefPtr<CanvasDrawEventRecorder> self(this);
        QueueProcessPendingDeletionsLocked(std::move(self));
      }
      return;
    }

    // Otherwise, we can just run all of them right now.
    pendingDeletions.swap(*lockedPendingDeletions);
  }

  for (const auto& pendingDeletion : pendingDeletions) {
    pendingDeletion();
  }
}

void CanvasDrawEventRecorder::StoreSourceSurfaceRecording(
    gfx::SourceSurface* aSurface, const char* aReason) {
  NS_ASSERT_OWNINGTHREAD(CanvasDrawEventRecorder);

  if (NS_IsMainThread()) {
    wr::ExternalImageId extId{};
    nsresult rv = layers::SharedSurfacesChild::Share(aSurface, extId);
    if (NS_SUCCEEDED(rv)) {
      StoreExternalSurfaceRecording(aSurface, wr::AsUint64(extId));
      return;
    }
  }

  DrawEventRecorderPrivate::StoreSourceSurfaceRecording(aSurface, aReason);
}

}  // namespace layers
}  // namespace mozilla
