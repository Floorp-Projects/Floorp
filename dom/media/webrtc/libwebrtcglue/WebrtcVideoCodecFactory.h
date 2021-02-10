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
class WebrtcVideoDecoderFactory : public webrtc::VideoDecoderFactory {
 public:
  explicit WebrtcVideoDecoderFactory(std::string aPCHandle)
      : mPCHandle(std::move(aPCHandle)) {}

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    MOZ_CRASH("Unexpected call");
    return std::vector<webrtc::SdpVideoFormat>();
  }

  std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(
      const webrtc::SdpVideoFormat& aFormat) override;

 private:
  const std::string mPCHandle;
};

class WebrtcVideoEncoderFactory : public webrtc::VideoEncoderFactory {
  class InternalFactory : public webrtc::VideoEncoderFactory {
   public:
    explicit InternalFactory(std::string aPCHandle)
        : mPCHandle(std::move(aPCHandle)) {}

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
  explicit WebrtcVideoEncoderFactory(std::string aPCHandle)
      : mInternalFactory(MakeUnique<InternalFactory>(std::move(aPCHandle))) {}

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    MOZ_CRASH("Unexpected call");
    return std::vector<webrtc::SdpVideoFormat>();
  }

  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
      const webrtc::SdpVideoFormat& aFormat) override;

 private:
  const UniquePtr<InternalFactory> mInternalFactory;
};
}  // namespace mozilla

#endif
