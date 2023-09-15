/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_DataPipe_h
#define mozilla_ipc_DataPipe_h

#include "mozilla/ipc/SharedMemoryBasic.h"
#include "mozilla/ipc/NodeController.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsISupports.h"

namespace mozilla {
namespace ipc {

namespace data_pipe_detail {

class DataPipeAutoLock;
class DataPipeLink;

class DataPipeBase {
 public:
  DataPipeBase(const DataPipeBase&) = delete;
  DataPipeBase& operator=(const DataPipeBase&) = delete;

 protected:
  explicit DataPipeBase(bool aReceiverSide, nsresult aError);
  DataPipeBase(bool aReceiverSide, ScopedPort aPort,
               SharedMemoryBasic::Handle aShmemHandle, SharedMemory* aShmem,
               uint32_t aCapacity, nsresult aPeerStatus, uint32_t aOffset,
               uint32_t aAvailable);

  void CloseInternal(DataPipeAutoLock&, nsresult aStatus) MOZ_REQUIRES(*mMutex);

  void AsyncWaitInternal(already_AddRefed<nsIRunnable> aCallback,
                         already_AddRefed<nsIEventTarget> aTarget,
                         bool aClosureOnly) MOZ_EXCLUDES(*mMutex);

  // Like `nsWriteSegmentFun` or `nsReadSegmentFun`.
  using ProcessSegmentFun =
      FunctionRef<nsresult(Span<char> aSpan, uint32_t aProcessedThisCall,
                           uint32_t* aProcessedCount)>;
  nsresult ProcessSegmentsInternal(uint32_t aCount,
                                   ProcessSegmentFun aProcessSegment,
                                   uint32_t* aProcessedCount)
      MOZ_EXCLUDES(*mMutex);

  nsresult CheckStatus(DataPipeAutoLock&) MOZ_REQUIRES(*mMutex);

  nsCString Describe(DataPipeAutoLock&) MOZ_REQUIRES(*mMutex);

  // Thread safety helper to tell the analysis that `mLink->mMutex` is held when
  // `mMutex` is held.
  void AssertSameMutex(const std::shared_ptr<Mutex>& aMutex)
      MOZ_REQUIRES(*mMutex) MOZ_ASSERT_CAPABILITY(*aMutex) {
    MOZ_ASSERT(mMutex == aMutex);
  }

  virtual ~DataPipeBase();

  const std::shared_ptr<Mutex> mMutex;
  nsresult mStatus MOZ_GUARDED_BY(*mMutex) = NS_OK;
  RefPtr<DataPipeLink> mLink MOZ_GUARDED_BY(*mMutex);
};

template <typename T>
void DataPipeWrite(IPC::MessageWriter* aWriter, T* aParam);

template <typename T>
bool DataPipeRead(IPC::MessageReader* aReader, RefPtr<T>* aResult);

}  // namespace data_pipe_detail

class DataPipeSender;
class DataPipeReceiver;

#define NS_DATAPIPESENDER_IID                        \
  {                                                  \
    0x6698ed77, 0x9fff, 0x425d, {                    \
      0xb0, 0xa6, 0x1d, 0x30, 0x66, 0xee, 0xb8, 0x16 \
    }                                                \
  }

// Helper class for streaming data to another process.
class DataPipeSender final : public nsIAsyncOutputStream,
                             public data_pipe_detail::DataPipeBase {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DATAPIPESENDER_IID)
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM
  NS_DECL_NSIASYNCOUTPUTSTREAM

 private:
  friend nsresult NewDataPipe(uint32_t, DataPipeSender**, DataPipeReceiver**);
  friend void data_pipe_detail::DataPipeWrite<DataPipeSender>(
      IPC::MessageWriter* aWriter, DataPipeSender* aParam);
  friend bool data_pipe_detail::DataPipeRead<DataPipeSender>(
      IPC::MessageReader* aReader, RefPtr<DataPipeSender>* aResult);

  explicit DataPipeSender(nsresult aError)
      : data_pipe_detail::DataPipeBase(/* aReceiverSide */ false, aError) {}
  DataPipeSender(ScopedPort aPort, SharedMemoryBasic::Handle aShmemHandle,
                 SharedMemory* aShmem, uint32_t aCapacity, nsresult aPeerStatus,
                 uint32_t aOffset, uint32_t aAvailable)
      : data_pipe_detail::DataPipeBase(
            /* aReceiverSide */ false, std::move(aPort),
            std::move(aShmemHandle), aShmem, aCapacity, aPeerStatus, aOffset,
            aAvailable) {}

  ~DataPipeSender() = default;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DataPipeSender, NS_DATAPIPESENDER_IID)

#define NS_DATAPIPERECEIVER_IID                      \
  {                                                  \
    0x0a185f83, 0x499e, 0x450c, {                    \
      0x95, 0x82, 0x27, 0x67, 0xad, 0x6d, 0x64, 0xb5 \
    }                                                \
  }

// Helper class for streaming data from another process.
class DataPipeReceiver final : public nsIAsyncInputStream,
                               public nsIIPCSerializableInputStream,
                               public data_pipe_detail::DataPipeBase {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DATAPIPERECEIVER_IID)
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM

 private:
  friend nsresult NewDataPipe(uint32_t, DataPipeSender**, DataPipeReceiver**);
  friend void data_pipe_detail::DataPipeWrite<DataPipeReceiver>(
      IPC::MessageWriter* aWriter, DataPipeReceiver* aParam);
  friend bool data_pipe_detail::DataPipeRead<DataPipeReceiver>(
      IPC::MessageReader* aReader, RefPtr<DataPipeReceiver>* aResult);

  explicit DataPipeReceiver(nsresult aError)
      : data_pipe_detail::DataPipeBase(/* aReceiverSide */ true, aError) {}
  DataPipeReceiver(ScopedPort aPort, SharedMemoryBasic::Handle aShmemHandle,
                   SharedMemory* aShmem, uint32_t aCapacity,
                   nsresult aPeerStatus, uint32_t aOffset, uint32_t aAvailable)
      : data_pipe_detail::DataPipeBase(
            /* aReceiverSide */ true, std::move(aPort), std::move(aShmemHandle),
            aShmem, aCapacity, aPeerStatus, aOffset, aAvailable) {}

  ~DataPipeReceiver() = default;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DataPipeReceiver, NS_DATAPIPERECEIVER_IID)

constexpr uint32_t kDefaultDataPipeCapacity = 64 * 1024;

/**
 * Create a new DataPipe pair. The sender and receiver ends of the pipe may be
 * used to transfer data between processes. |aCapacity| is the capacity of the
 * underlying ring buffer. If `0` is passed, `kDefaultDataPipeCapacity` will be
 * used.
 */
nsresult NewDataPipe(uint32_t aCapacity, DataPipeSender** aSender,
                     DataPipeReceiver** aReceiver);

}  // namespace ipc
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::ipc::DataPipeSender*> {
  static void Write(MessageWriter* aWriter,
                    mozilla::ipc::DataPipeSender* aParam);
  static bool Read(MessageReader* aReader,
                   RefPtr<mozilla::ipc::DataPipeSender>* aResult);
};

template <>
struct ParamTraits<mozilla::ipc::DataPipeReceiver*> {
  static void Write(MessageWriter* aWriter,
                    mozilla::ipc::DataPipeReceiver* aParam);
  static bool Read(MessageReader* aReader,
                   RefPtr<mozilla::ipc::DataPipeReceiver>* aResult);
};

}  // namespace IPC

#endif  // mozilla_ipc_DataPipe_h
