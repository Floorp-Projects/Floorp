/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_ipc_VideoDecoderParent_h
#define include_ipc_VideoDecoderParent_h

#include "ImageContainer.h"
#include "MediaData.h"
#include "PlatformDecoderModule.h"
#include "VideoDecoderManagerParent.h"
#include "mozilla/MozPromise.h"
#include "mozilla/PVideoDecoderParent.h"
#include "mozilla/layers/TextureForwarder.h"

namespace mozilla {

class KnowsCompositorVideo;
using mozilla::ipc::IPCResult;

class VideoDecoderParent final : public PVideoDecoderParent {
  friend class PVideoDecoderParent;

 public:
  // We refcount this class since the task queue can have runnables
  // that reference us.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoDecoderParent)

  VideoDecoderParent(VideoDecoderManagerParent* aParent,
                     const VideoInfo& aVideoInfo, float aFramerate,
                     const CreateDecoderParams::OptionSet& aOptions,
                     const layers::TextureFactoryIdentifier& aIdentifier,
                     TaskQueue* aManagerTaskQueue, TaskQueue* aDecodeTaskQueue,
                     bool* aSuccess, nsCString* aErrorDescription);

  void Destroy();

  // PVideoDecoderParent
  IPCResult RecvInit();
  IPCResult RecvInput(const MediaRawDataIPDL& aData);
  IPCResult RecvFlush();
  IPCResult RecvDrain();
  IPCResult RecvShutdown();
  IPCResult RecvSetSeekThreshold(const int64_t& aTime);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  bool OnManagerThread();
  void Error(const MediaResult& aError);

  ~VideoDecoderParent();
  void ProcessDecodedData(MediaDataDecoder::DecodedData&& aData);

  RefPtr<VideoDecoderManagerParent> mParent;
  RefPtr<VideoDecoderParent> mIPDLSelfRef;
  RefPtr<TaskQueue> mManagerTaskQueue;
  RefPtr<TaskQueue> mDecodeTaskQueue;
  RefPtr<MediaDataDecoder> mDecoder;
  RefPtr<KnowsCompositorVideo> mKnowsCompositor;

  // Can only be accessed from the manager thread
  bool mDestroyed;
};

}  // namespace mozilla

#endif  // include_ipc_VideoDecoderParent_h
