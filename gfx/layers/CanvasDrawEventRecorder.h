/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CanvasDrawEventRecorder_h
#define mozilla_layers_CanvasDrawEventRecorder_h

#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/ipc/CrossProcessSemaphore.h"
#include "mozilla/ipc/SharedMemoryBasic.h"

namespace mozilla {
namespace layers {

class CanvasEventRingBuffer final : public gfx::EventRingBuffer {
 public:
  CanvasEventRingBuffer() {}

  /**
   * Initialize the write side of a CanvasEventRingBuffer returning handles to
   * the shared memory for the buffer and the two semaphores for waiting in the
   * reader and the writer.
   *
   * @param aOtherPid process ID to share the handles to
   * @param aReadHandle handle to the shared memory for the buffer
   * @param aReaderSem reading blocked semaphore
   * @param aWriterSem writing blocked semaphore
   * @param aResumeReaderCallback callback to start the reader when it is has
   *                             stopped
   * @returns true if initialization succeeds
   */
  bool InitWriter(base::ProcessId aOtherPid,
                  ipc::SharedMemoryBasic::Handle* aReadHandle,
                  CrossProcessSemaphoreHandle* aReaderSem,
                  CrossProcessSemaphoreHandle* aWriterSem,
                  const std::function<void()>& aResumeReaderCallback);

  /**
   * Initialize the read side of a CanvasEventRingBuffer.
   *
   * @param aReadHandle handle to the shared memory for the buffer
   * @param aReaderSem reading blocked semaphore
   * @param aWriterSem writing blocked semaphore
   * @returns true if initialization succeeds
   */
  bool InitReader(const ipc::SharedMemoryBasic::Handle& aReadHandle,
                  const CrossProcessSemaphoreHandle& aReaderSem,
                  const CrossProcessSemaphoreHandle& aWriterSem);

  bool good() const final { return mGood; }

  void SetIsBad() final { mGood = false; }

  void write(const char* const aData, const size_t aSize) final;

  bool HasDataToRead();

  /*
   * This will put the reader into a stopped state if there is no more data to
   * read. If this returns false the caller is responsible for continuing
   * translation at a later point. If it returns false the writer will start the
   * translation again when more data is written.
   *
   * @returns true if stopped
   */
  bool StopIfEmpty();

  /*
   * Waits for the given TimeDuration for data to become available.
   *
   * @returns true if data is available to read.
   */
  bool WaitForDataToRead(TimeDuration aTimeout);

  int32_t ReadNextEvent();

  void read(char* const aOut, const size_t aSize) final;

  /**
   * Writes a checkpoint event to the buffer.
   *
   * @returns the write count after the checkpoint has been written
   */
  uint32_t CreateCheckpoint();

  /**
   * Waits until the given checkpoint has been read from the buffer.
   *
   * @params aCheckpoint the checkpoint to wait for
   * @params aTimeout duration to wait while reader is not active
   * @returns true if the checkpoint was reached, false if the read count has
   *          not changed in the aTimeout duration.
   */
  bool WaitForCheckpoint(uint32_t aCheckpoint, TimeDuration aTimeout);

  /**
   * Used to send data back to the writer. This is done through the same shared
   * memory so the writer must wait and read the response after it has submitted
   * the event that uses this.
   *
   * @param aData the data to be written back to the writer
   * @param aSize the number of chars to write
   */
  void ReturnWrite(const char* aData, size_t aSize);

  /**
   * Used to read data sent back from the reader via ReturnWrite. This is done
   * through the same shared memory so the writer must wait until all expected
   * data is read before writing new events to the buffer.
   *
   * @param aOut the pointer to read into
   * @param aSize the number of chars to read
   */
  void ReturnRead(char* aOut, size_t aSize);

 protected:
  bool WaitForAndRecalculateAvailableSpace() final;
  void UpdateWriteTotalsBy(uint32_t aCount) final;

 private:
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
    Stopped
  };

  struct ReadFooter {
    Atomic<uint32_t, ReleaseAcquire> count;
    Atomic<uint32_t, ReleaseAcquire> returnCount;
    Atomic<State, ReleaseAcquire> state;
  };

  struct WriteFooter {
    Atomic<uint32_t, ReleaseAcquire> count;
    Atomic<uint32_t, ReleaseAcquire> returnCount;
    Atomic<uint32_t, ReleaseAcquire> requiredDifference;
    Atomic<State, ReleaseAcquire> state;
  };

  CanvasEventRingBuffer(const CanvasEventRingBuffer&) = delete;
  void operator=(const CanvasEventRingBuffer&) = delete;

  void IncrementWriteCountBy(uint32_t aCount);

  bool WaitForReadCount(uint32_t aReadCount, TimeDuration aTimeout);

  bool WaitForAndRecalculateAvailableData();

  void UpdateReadTotalsBy(uint32_t aCount);
  void IncrementReadCountBy(uint32_t aCount);

  void CheckAndSignalReader();

  void CheckAndSignalWriter();

  uint32_t WaitForBytesToWrite();

  uint32_t WaitForBytesToRead();

  RefPtr<ipc::SharedMemoryBasic> mSharedMemory;
  UniquePtr<CrossProcessSemaphore> mReaderSemaphore;
  UniquePtr<CrossProcessSemaphore> mWriterSemaphore;
  std::function<void()> mResumeReaderCallback;
  char* mBuf = nullptr;
  uint32_t mOurCount = 0;
  WriteFooter* mWrite = nullptr;
  ReadFooter* mRead = nullptr;
  bool mGood = false;
};

class CanvasDrawEventRecorder final : public gfx::DrawEventRecorderPrivate {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(CanvasDrawEventRecorder, final)
  explicit CanvasDrawEventRecorder(){};

  bool Init(base::ProcessId aOtherPid, ipc::SharedMemoryBasic::Handle* aHandle,
            CrossProcessSemaphoreHandle* aReaderSem,
            CrossProcessSemaphoreHandle* aWriterSem,
            const std::function<void()>& aResumeReaderCallback) {
    return mOutputStream.InitWriter(aOtherPid, aHandle, aReaderSem, aWriterSem,
                                    aResumeReaderCallback);
  }

  void RecordEvent(const gfx::RecordedEvent& aEvent) final {
    if (!mOutputStream.good()) {
      return;
    }

    aEvent.RecordToStream(mOutputStream);
  }

  void Flush() final {}

  void ReturnRead(char* aOut, size_t aSize) {
    mOutputStream.ReturnRead(aOut, aSize);
  }

  uint32_t CreateCheckpoint() { return mOutputStream.CreateCheckpoint(); }

  /**
   * Waits until the given checkpoint has been read by the translator.
   *
   * @params aCheckpoint the checkpoint to wait for
   * @params aTimeout duration to wait while translator is not actively reading
   * @returns true if the checkpoint was reached, false if the translator has
   *          not been active in the aTimeout duration.
   */
  bool WaitForCheckpoint(uint32_t aCheckpoint, TimeDuration aTimeout) {
    return mOutputStream.WaitForCheckpoint(aCheckpoint, aTimeout);
  }

 private:
  CanvasEventRingBuffer mOutputStream;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_CanvasDrawEventRecorder_h
