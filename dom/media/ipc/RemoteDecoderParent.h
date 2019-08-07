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
                      TaskQueue* aManagerTaskQueue,
                      TaskQueue* aDecodeTaskQueue);

  void Destroy();

  // PRemoteDecoderParent
  IPCResult RecvInit();
  IPCResult RecvInput(const MediaRawDataIPDL& aData);
  IPCResult RecvFlush();
  IPCResult RecvDrain();
  IPCResult RecvShutdown();
  IPCResult RecvSetSeekThreshold(const media::TimeUnit& aTime);
  IPCResult RecvDoneWithOutput(Shmem&& aOutputShmem);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 protected:
  virtual ~RemoteDecoderParent();

  bool OnManagerThread();
  void Error(const MediaResult& aError);

  virtual MediaResult ProcessDecodedData(
      const MediaDataDecoder::DecodedData& aData) = 0;

  RefPtr<RemoteDecoderManagerParent> mParent;
  RefPtr<RemoteDecoderParent> mIPDLSelfRef;
  RefPtr<TaskQueue> mManagerTaskQueue;
  RefPtr<TaskQueue> mDecodeTaskQueue;
  RefPtr<MediaDataDecoder> mDecoder;

  // Can only be accessed from the manager thread
  bool mDestroyed = false;
  ShmemPool mDecodedFramePool;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteDecoderParent_h
