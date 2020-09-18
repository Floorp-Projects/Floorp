/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteDecoderParent_h
#define include_dom_media_ipc_RemoteDecoderParent_h
#include "mozilla/PRemoteDecoderParent.h"

#include "mozilla/ShmemPool.h"

namespace mozilla {

class RemoteDecoderManagerParent;
using mozilla::ipc::IPCResult;

class RemoteDecoderParent : public PRemoteDecoderParent {
  friend class PRemoteDecoderParent;

 public:
  // We refcount this class since the task queue can have runnables
  // that reference us.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteDecoderParent)

  RemoteDecoderParent(RemoteDecoderManagerParent* aParent,
                      nsISerialEventTarget* aManagerThread,
                      TaskQueue* aDecodeTaskQueue);

  void Destroy();

  // PRemoteDecoderParent
  IPCResult RecvInit(InitResolver&& aResolver);
  IPCResult RecvDecode(nsTArray<MediaRawDataIPDL>&& aData,
                       DecodeResolver&& aResolver);
  IPCResult RecvFlush(FlushResolver&& aResolver);
  IPCResult RecvDrain(DrainResolver&& aResolver);
  IPCResult RecvShutdown(ShutdownResolver&& aResolver);
  IPCResult RecvSetSeekThreshold(const media::TimeUnit& aTime);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 protected:
  virtual ~RemoteDecoderParent();

  bool OnManagerThread();

  virtual MediaResult ProcessDecodedData(
      const MediaDataDecoder::DecodedData& aDatam,
      DecodedOutputIPDL& aDecodedData) = 0;
  ShmemBuffer AllocateBuffer(size_t aLength);

  const RefPtr<RemoteDecoderManagerParent> mParent;
  const RefPtr<TaskQueue> mDecodeTaskQueue;
  RefPtr<MediaDataDecoder> mDecoder;

 private:
  void DecodeNextSample(nsTArray<MediaRawDataIPDL>&& aData,
                        DecodedOutputIPDL&& aOutput,
                        DecodeResolver&& aResolver);
  void ReleaseBuffer(ShmemBuffer&& aBuffer);
  void ReleaseUsedShmems();
  RefPtr<RemoteDecoderParent> mIPDLSelfRef;
  const RefPtr<nsISerialEventTarget> mManagerThread;
  ShmemPool mDecodedFramePool;
  AutoTArray<ShmemBuffer, 4> mUsedShmems;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteDecoderParent_h
