/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_WEBRTCVIDEOCODECFACTORY_H_
#define DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_WEBRTCVIDEOCODECFACTORY_H_

#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "MediaEventSource.h"

namespace mozilla {
class GmpPluginNotifierInterface {
  virtual MediaEventSource<uint64_t>& CreatedGmpPluginEvent() = 0;
  virtual MediaEventSource<uint64_t>& ReleasedGmpPluginEvent() = 0;
};

class GmpPluginNotifier : public GmpPluginNotifierInterface {
 public:
  explicit GmpPluginNotifier(nsCOMPtr<nsISerialEventTarget> aOwningThread)
      : mOwningThread(std::move(aOwningThread)),
        mCreatedGmpPluginEvent(
            new MediaEventForwarder<uint64_t>(mOwningThread)),
        mReleasedGmpPluginEvent(
            new MediaEventForwarder<uint64_t>(mOwningThread)) {}

  ~GmpPluginNotifier() {
    mOwningThread->Dispatch(NS_NewRunnableFunction(
        "~GmpPluginNotifier",
        [createdEvent = std::move(mCreatedGmpPluginEvent),
         releasedEvent = std::move(mReleasedGmpPluginEvent)]() mutable {
          createdEvent->DisconnectAll();
          releasedEvent->DisconnectAll();
        }));
  }

  MediaEventSource<uint64_t>& CreatedGmpPluginEvent() override {
    MOZ_ASSERT(mOwningThread->IsOnCurrentThread());
    return *mCreatedGmpPluginEvent;
  }

  MediaEventSource<uint64_t>& ReleasedGmpPluginEvent() override {
    MOZ_ASSERT(mOwningThread->IsOnCurrentThread());
    return *mReleasedGmpPluginEvent;
  }

 protected:
  const nsCOMPtr<nsISerialEventTarget> mOwningThread;
  RefPtr<MediaEventForwarder<uint64_t> > mCreatedGmpPluginEvent;
  RefPtr<MediaEventForwarder<uint64_t> > mReleasedGmpPluginEvent;
};

class WebrtcVideoDecoderFactory : public GmpPluginNotifier,
                                  public webrtc::VideoDecoderFactory {
 public:
  WebrtcVideoDecoderFactory(nsCOMPtr<nsISerialEventTarget> aOwningThread,
                            std::string aPCHandle)
      : GmpPluginNotifier(std::move(aOwningThread)),
        mPCHandle(std::move(aPCHandle)) {}

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    MOZ_CRASH("Unexpected call");
    return std::vector<webrtc::SdpVideoFormat>();
  }

  std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(
      const webrtc::SdpVideoFormat& aFormat) override;

 private:
  const std::string mPCHandle;
};

class WebrtcVideoEncoderFactory : public GmpPluginNotifierInterface,
                                  public webrtc::VideoEncoderFactory {
  class InternalFactory : public GmpPluginNotifier,
                          public webrtc::VideoEncoderFactory {
   public:
    InternalFactory(nsCOMPtr<nsISerialEventTarget> aOwningThread,
                    std::string aPCHandle)
        : GmpPluginNotifier(std::move(aOwningThread)),
          mPCHandle(std::move(aPCHandle)) {}

    std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
      MOZ_CRASH("Unexpected call");
      return std::vector<webrtc::SdpVideoFormat>();
    }

    std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
        const webrtc::SdpVideoFormat& aFormat) override;

    bool Supports(const webrtc::SdpVideoFormat& aFormat);

   private:
    const std::string mPCHandle;
  };

 public:
  explicit WebrtcVideoEncoderFactory(
      nsCOMPtr<nsISerialEventTarget> aOwningThread, std::string aPCHandle)
      : mInternalFactory(MakeUnique<InternalFactory>(std::move(aOwningThread),
                                                     std::move(aPCHandle))) {}

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    MOZ_CRASH("Unexpected call");
    return std::vector<webrtc::SdpVideoFormat>();
  }

  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
      const webrtc::SdpVideoFormat& aFormat) override;

  MediaEventSource<uint64_t>& CreatedGmpPluginEvent() override {
    return mInternalFactory->CreatedGmpPluginEvent();
  }
  MediaEventSource<uint64_t>& ReleasedGmpPluginEvent() override {
    return mInternalFactory->ReleasedGmpPluginEvent();
  }

 private:
  const UniquePtr<InternalFactory> mInternalFactory;
};
}  // namespace mozilla

#endif
