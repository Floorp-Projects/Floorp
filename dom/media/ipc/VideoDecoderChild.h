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
namespace dom {

class RemoteVideoDecoder;
class RemoteDecoderModule;

class VideoDecoderChild final : public PVideoDecoderChild
{
public:
  explicit VideoDecoderChild();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoDecoderChild)

  // PVideoDecoderChild
  bool RecvOutput(const VideoDataIPDL& aData) override;
  bool RecvInputExhausted() override;
  bool RecvDrainComplete() override;
  bool RecvError(const nsresult& aError) override;
  bool RecvInitComplete() override;
  bool RecvInitFailed(const nsresult& aReason) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

  RefPtr<MediaDataDecoder::InitPromise> Init();
  void Input(MediaRawData* aSample);
  void Flush();
  void Drain();
  void Shutdown();

  void InitIPDL(MediaDataDecoderCallback* aCallback,
                const VideoInfo& aVideoInfo,
                layers::LayersBackend aLayersBackend);
  void DestroyIPDL();

  // Called from IPDL when our actor has been destroyed
  void IPDLActorDestroyed();

private:
  ~VideoDecoderChild();

  void AssertOnManagerThread();

  RefPtr<VideoDecoderChild> mIPDLSelfRef;
  RefPtr<nsIThread> mThread;

  MediaDataDecoderCallback* mCallback;

  MozPromiseHolder<MediaDataDecoder::InitPromise> mInitPromise;

  VideoInfo mVideoInfo;
  layers::LayersBackend mLayersBackend;
  bool mCanSend;
};

} // namespace dom
} // namespace mozilla

#endif // include_dom_ipc_VideoDecoderChild_h
