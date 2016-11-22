/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_ipc_VideoDecoderChild_h
#define include_dom_ipc_VideoDecoderChild_h

#include "mozilla/RefPtr.h"
#include "mozilla/dom/PVideoDecoderChild.h"
#include "MediaData.h"
#include "PlatformDecoderModule.h"

namespace mozilla {
namespace layers {
class SynchronousTask;
}
namespace dom {

class RemoteVideoDecoder;
class RemoteDecoderModule;
class VideoDecoderManagerChild;

class VideoDecoderChild final : public PVideoDecoderChild
{
public:
  explicit VideoDecoderChild();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoDecoderChild)

  // PVideoDecoderChild
  mozilla::ipc::IPCResult RecvOutput(const VideoDataIPDL& aData) override;
  mozilla::ipc::IPCResult RecvInputExhausted() override;
  mozilla::ipc::IPCResult RecvDrainComplete() override;
  mozilla::ipc::IPCResult RecvError(const nsresult& aError) override;
  mozilla::ipc::IPCResult RecvInitComplete(const bool& aHardware, const nsCString& aHardwareReason) override;
  mozilla::ipc::IPCResult RecvInitFailed(const nsresult& aReason) override;
  mozilla::ipc::IPCResult RecvFlushComplete() override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

  RefPtr<MediaDataDecoder::InitPromise> Init();
  void Input(MediaRawData* aSample);
  void Flush(layers::SynchronousTask* Task);
  void Drain();
  void Shutdown();
  bool IsHardwareAccelerated(nsACString& aFailureReason) const;
  void SetSeekThreshold(const media::TimeUnit& aTime);

  MOZ_IS_CLASS_INIT
  bool InitIPDL(MediaDataDecoderCallback* aCallback,
                const VideoInfo& aVideoInfo,
                const layers::TextureFactoryIdentifier& aIdentifier);
  void DestroyIPDL();

  // Called from IPDL when our actor has been destroyed
  void IPDLActorDestroyed();

  VideoDecoderManagerChild* GetManager();

private:
  ~VideoDecoderChild();

  void AssertOnManagerThread();

  RefPtr<VideoDecoderChild> mIPDLSelfRef;
  RefPtr<nsIThread> mThread;

  MediaDataDecoderCallback* mCallback;

  MozPromiseHolder<MediaDataDecoder::InitPromise> mInitPromise;

  layers::SynchronousTask* mFlushTask;

  nsCString mHardwareAcceleratedReason;
  bool mCanSend;
  bool mInitialized;
  bool mIsHardwareAccelerated;
};

} // namespace dom
} // namespace mozilla

#endif // include_dom_ipc_VideoDecoderChild_h
