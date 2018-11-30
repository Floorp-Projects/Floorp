/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteVideoDecoderParent_h
#define include_dom_media_ipc_RemoteVideoDecoderParent_h
#include "mozilla/PRemoteVideoDecoderParent.h"

namespace mozilla {

class RemoteDecoderManagerParent;

class RemoteVideoDecoderParent final : public PRemoteVideoDecoderParent {
 public:
  // We refcount this class since the task queue can have runnables
  // that reference us.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteVideoDecoderParent)

  RemoteVideoDecoderParent(RemoteDecoderManagerParent* aParent,
                           const VideoInfo& aVideoInfo, float aFramerate,
                           const CreateDecoderParams::OptionSet& aOptions,
                           TaskQueue* aManagerTaskQueue,
                           TaskQueue* aDecodeTaskQueue, bool* aSuccess,
                           nsCString* aErrorDescription);

  void Destroy();

  // PRemoteVideoDecoderParent
  mozilla::ipc::IPCResult RecvInit() override;
  mozilla::ipc::IPCResult RecvInput(const MediaRawDataIPDL& aData) override;
  mozilla::ipc::IPCResult RecvFlush() override;
  mozilla::ipc::IPCResult RecvDrain() override;
  mozilla::ipc::IPCResult RecvShutdown() override;
  mozilla::ipc::IPCResult RecvSetSeekThreshold(const int64_t& aTime) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  bool OnManagerThread();
  void Error(const MediaResult& aError);

  ~RemoteVideoDecoderParent();
  void ProcessDecodedData(const MediaDataDecoder::DecodedData& aData);

  RefPtr<RemoteDecoderManagerParent> mParent;
  RefPtr<RemoteVideoDecoderParent> mIPDLSelfRef;
  RefPtr<TaskQueue> mManagerTaskQueue;
  RefPtr<TaskQueue> mDecodeTaskQueue;
  RefPtr<MediaDataDecoder> mDecoder;

  // Can only be accessed from the manager thread
  bool mDestroyed;
  VideoInfo mVideoInfo;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteVideoDecoderParent_h
