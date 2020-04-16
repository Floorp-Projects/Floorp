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
  /**
   * WriterServices allows consumers of CanvasEventRingBuffer to provide
   * functions required by the write side of a CanvasEventRingBuffer without
   * introducing unnecessary dependencies on IPC code.
   */
  class WriterServices {
   public:
    virtual ~WriterServices() = default;

    /**
     * @returns true if the reader of the CanvasEventRingBuffer has permanently
     *          stopped processing, otherwise returns false.
     */
    virtual bool ReaderClosed() = 0;

    /**
     * Causes the reader to resume processing when it is in a stopped state.
     */
    virtual void ResumeReader() = 0;
  };

  /**
   * ReaderServices allows consumers of CanvasEventRingBuffer to provide
   * functions required by the read side of a CanvasEventRingBuffer without
   * introducing unnecessary dependencies on IPC code.
   */
  class ReaderServices {
   public:
    virtual ~ReaderServices() = default;

    /**
     * @returns true if the writer of the CanvasEventRingBuffer has permanently
     *          stopped processing, otherwise returns false.
     */
    virtual bool WriterClosed() = 0;
  };

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
   * @param aWriterServices provides functions required by the writer
   * @returns true if initialization succeeds
   */
  bool InitWriter(base::ProcessId aOtherPid,
                  ipc::SharedMemoryBasic::Handle* aReadHandle,
                  CrossProcessSemaphoreHandle* aReaderSem,
                  CrossProcessSemaphoreHandle* aWriterSem,
                  UniquePtr<WriterServices> aWriterServices);

  /**
   * Initialize the read side of a CanvasEventRingBuffer.
   *
   * @param aReadHandle handle to the shared memory for the buffer
   * @param aReaderSem reading blocked semaphore
   * @param aWriterSem writing blocked semaphore
   * @param aReaderServices provides functions required by the reader
   * @returns true if initialization succeeds
   */
  bool InitReader(const ipc::SharedMemoryBasic::Handle& aReadHandle,
                  const CrossProcessSemaphoreHandle& aReaderSem,
                  const CrossProcessSemaphoreHandle& aWriterSem,
                  UniquePtr<ReaderServices> aReaderServices);

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
   * Waits for data to become available. This will wait for aTimeout duration
   * aRetryCount number of times, checking to see if the other side is closed in
   * between each one.
   *
   * @param aTimeout duration to wait
   * @param aRetryCount number of times to retry
   * @returns true if data is available to read.
   */
  bool WaitForDataToRead(TimeDuration aTimeout, int32_t aRetryCount);

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
   * @returns true if the checkpoint was reached, false if the reader is closed
   *          or we timeout.
   */
  bool WaitForCheckpoint(uint32_t aCheckpoint);

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
    Atomic<uint32_t> count;
    Atomic<uint32_t> returnCount;
    Atomic<State> state;
  };

  struct WriteFooter {
    Atomic<uint32_t> count;
    Atomic<uint32_t> returnCount;
    Atomic<uint32_t> requiredDifference;
    Atomic<State> state;
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
  UniquePtr<WriterServices> mWriterServices;
  UniquePtr<ReaderServices> mReaderServices;
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
            UniquePtr<CanvasEventRingBuffer::WriterServices> aWriterServices) {
    return mOutputStream.InitWriter(aOtherPid, aHandle, aReaderSem, aWriterSem,
                                    std::move(aWriterServices));
  }

  void RecordEvent(const gfx::RecordedEvent& aEvent) final {
    if (!mOutputStream.good()) {
      return;
    }

    aEvent.RecordToStream(mOutputStream);
  }

  void RecordSourceSurfaceDestruction(gfx::SourceSurface* aSurface) final;

  void Flush() final {}

  void ReturnRead(char* aOut, size_t aSize) {
    mOutputStream.ReturnRead(aOut, aSize);
  }

  uint32_t CreateCheckpoint() { return mOutputStream.CreateCheckpoint(); }

  /**
   * Waits until the given checkpoint has been read by the translator.
   *
   * @params aCheckpoint the checkpoint to wait for
   * @returns true if the checkpoint was reached, false if the reader is closed
   *          or we timeout.
   */
  bool WaitForCheckpoint(uint32_t aCheckpoint) {
    return mOutputStream.WaitForCheckpoint(aCheckpoint);
  }

 private:
  CanvasEventRingBuffer mOutputStream;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_CanvasDrawEventRecorder_h
