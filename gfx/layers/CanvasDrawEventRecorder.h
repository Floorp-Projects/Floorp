/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CanvasDrawEventRecorder_h
#define mozilla_layers_CanvasDrawEventRecorder_h

#include <queue>

#include "mozilla/Atomics.h"
#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/ipc/CrossProcessSemaphore.h"
#include "mozilla/ipc/SharedMemoryBasic.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

using EventType = gfx::RecordedEvent::EventType;

namespace layers {

typedef mozilla::ipc::SharedMemoryBasic::Handle Handle;
typedef mozilla::CrossProcessSemaphoreHandle CrossProcessSemaphoreHandle;

class CanvasDrawEventRecorder final : public gfx::DrawEventRecorderPrivate,
                                      public gfx::ContiguousBufferStream {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(CanvasDrawEventRecorder, final)

  CanvasDrawEventRecorder();

  enum class State : uint32_t {
    Processing,

    /**
     * This is the important state to make sure the other side signals or starts
     * us as soon as data or space is available. We set AboutToWait first and
     * then re-check the condition. If we went straight to Waiting or Stopped
     * then in between the last check and setting the state, the other side
     * could have used all available data or space and never have signaled us
     * because it didn't know we were about to wait, causing a deadlock.
     * While we are in this state, the other side must wait until we resolve the
     * AboutToWait state to one of the other states and then signal or start us
     * if it needs to.
     */
    AboutToWait,
    Waiting,
    Paused,
    Stopped,
    Failed,
  };

  struct Header {
    Atomic<int64_t> eventCount;
    Atomic<int64_t> writerWaitCount;
    Atomic<State> writerState;
    uint8_t padding1[44];
    Atomic<int64_t> processedCount;
    Atomic<State> readerState;
  };

  class Helpers {
   public:
    virtual ~Helpers() = default;

    virtual bool InitTranslator(
        TextureType aTextureType, gfx::BackendType aBackendType,
        Handle&& aReadHandle, nsTArray<Handle>&& aBufferHandles,
        uint64_t aBufferSize, CrossProcessSemaphoreHandle&& aReaderSem,
        CrossProcessSemaphoreHandle&& aWriterSem, bool aUseIPDLThread) = 0;

    virtual bool AddBuffer(Handle&& aBufferHandle, uint64_t aBufferSize) = 0;

    /**
     * @returns true if the reader of the CanvasEventRingBuffer has permanently
     *          stopped processing, otherwise returns false.
     */
    virtual bool ReaderClosed() = 0;

    /**
     * Causes the reader to resume processing when it is in a stopped state.
     */
    virtual bool RestartReader() = 0;
  };

  bool Init(TextureType aTextureType, gfx::BackendType aBackendType,
            UniquePtr<Helpers> aHelpers);

  /**
   * Record an event for processing by the CanvasParent's CanvasTranslator.
   * @param aEvent the event to record
   */
  void RecordEvent(const gfx::RecordedEvent& aEvent) final;

  void StoreSourceSurfaceRecording(gfx::SourceSurface* aSurface,
                                   const char* aReason) final;

  gfx::RecorderType GetRecorderType() const override {
    return gfx::RecorderType::CANVAS;
  }

  void Flush() final { NS_ASSERT_OWNINGTHREAD(CanvasDrawEventRecorder); }

  int64_t CreateCheckpoint();

  /**
   * Waits until the given checkpoint has been read by the translator.
   *
   * @params aCheckpoint the checkpoint to wait for
   * @returns true if the checkpoint was reached, false if the reader is closed
   *          or we timeout.
   */
  bool WaitForCheckpoint(int64_t aCheckpoint);

  TextureType GetTextureType() { return mTextureType; }

  void DropFreeBuffers();

  void ClearProcessedExternalSurfaces();

 protected:
  gfx::ContiguousBuffer& GetContiguousBuffer(size_t aSize) final;

  void IncrementEventCount() final;

 private:
  void WriteInternalEvent(EventType aEventType);

  void CheckAndSignalReader();

  size_t mDefaultBufferSize;
  size_t mMaxDefaultBuffers;
  uint32_t mMaxSpinCount;
  uint32_t mDropBufferLimit;
  uint32_t mDropBufferOnZero;

  UniquePtr<Helpers> mHelpers;

  TextureType mTextureType = TextureType::Unknown;
  RefPtr<ipc::SharedMemoryBasic> mHeaderShmem;
  Header* mHeader = nullptr;

  struct CanvasBuffer : public gfx::ContiguousBuffer {
    RefPtr<ipc::SharedMemoryBasic> shmem;

    CanvasBuffer() : ContiguousBuffer(nullptr) {}

    explicit CanvasBuffer(RefPtr<ipc::SharedMemoryBasic>&& aShmem)
        : ContiguousBuffer(static_cast<char*>(aShmem->memory()),
                           aShmem->Size()),
          shmem(std::move(aShmem)) {}

    size_t Capacity() { return shmem ? shmem->Size() : 0; }
  };

  struct RecycledBuffer {
    RefPtr<ipc::SharedMemoryBasic> shmem;
    int64_t eventCount = 0;
    explicit RecycledBuffer(RefPtr<ipc::SharedMemoryBasic>&& aShmem,
                            int64_t aEventCount)
        : shmem(std::move(aShmem)), eventCount(aEventCount) {}
    size_t Capacity() { return shmem->Size(); }
  };

  CanvasBuffer mCurrentBuffer;
  std::queue<RecycledBuffer> mRecycledBuffers;

  UniquePtr<CrossProcessSemaphore> mWriterSemaphore;
  UniquePtr<CrossProcessSemaphore> mReaderSemaphore;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_CanvasDrawEventRecorder_h
