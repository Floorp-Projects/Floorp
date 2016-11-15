/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_ipc_VideoDecoderParent_h
#define include_dom_ipc_VideoDecoderParent_h

#include "mozilla/RefPtr.h"
#include "mozilla/dom/PVideoDecoderParent.h"
#include "mozilla/layers/TextureForwarder.h"
#include "VideoDecoderManagerParent.h"
#include "MediaData.h"
#include "ImageContainer.h"

namespace mozilla {
namespace dom {

class KnowsCompositorVideo;

class VideoDecoderParent final : public PVideoDecoderParent,
                                 public MediaDataDecoderCallback
{
public:
  // We refcount this class since the task queue can have runnables
  // that reference us.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoDecoderParent)

  VideoDecoderParent(VideoDecoderManagerParent* aParent,
                     TaskQueue* aManagerTaskQueue,
                     TaskQueue* aDecodeTaskQueue);

  void Destroy();

  // PVideoDecoderParent
  mozilla::ipc::IPCResult RecvInit(const VideoInfo& aVideoInfo, const layers::TextureFactoryIdentifier& aIdentifier) override;
  mozilla::ipc::IPCResult RecvInput(const MediaRawDataIPDL& aData) override;
  mozilla::ipc::IPCResult RecvFlush() override;
  mozilla::ipc::IPCResult RecvDrain() override;
  mozilla::ipc::IPCResult RecvShutdown() override;
  mozilla::ipc::IPCResult RecvSetSeekThreshold(const int64_t& aTime) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

  // MediaDataDecoderCallback
  void Output(MediaData* aData) override;
  void Error(const MediaResult& aError) override;
  void InputExhausted() override;
  void DrainComplete() override;
  bool OnReaderTaskQueue() override;

private:
  ~VideoDecoderParent();

  RefPtr<VideoDecoderManagerParent> mParent;
  RefPtr<VideoDecoderParent> mIPDLSelfRef;
  RefPtr<TaskQueue> mManagerTaskQueue;
  RefPtr<TaskQueue> mDecodeTaskQueue;
  RefPtr<MediaDataDecoder> mDecoder;
  RefPtr<KnowsCompositorVideo> mKnowsCompositor;

  // Can only be accessed from the manager thread
  bool mDestroyed;
};

} // namespace dom
} // namespace mozilla

#endif // include_dom_ipc_VideoDecoderParent_h
