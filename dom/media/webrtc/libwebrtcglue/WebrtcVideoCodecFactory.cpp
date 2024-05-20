/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcVideoCodecFactory.h"

#include "GmpVideoCodec.h"
#include "MediaDataCodec.h"
#include "VideoConduit.h"
#include "mozilla/StaticPrefs_media.h"

// libwebrtc includes
#include "api/rtp_headers.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder_software_fallback_wrapper.h"
#include "media/engine/simulcast_encoder_adapter.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"

namespace mozilla {

std::unique_ptr<webrtc::VideoDecoder> WebrtcVideoDecoderFactory::Create(
    const webrtc::Environment& aEnv, const webrtc::SdpVideoFormat& aFormat) {
  std::unique_ptr<webrtc::VideoDecoder> decoder;
  auto type = webrtc::PayloadStringToCodecType(aFormat.name);

  // Attempt to create a decoder using MediaDataDecoder.
  decoder.reset(MediaDataCodec::CreateDecoder(type, mTrackingId));
  if (decoder) {
    return decoder;
  }

  switch (type) {
    case webrtc::VideoCodecType::kVideoCodecH264: {
      // Get an external decoder
      auto gmpDecoder =
          WrapUnique(GmpVideoCodec::CreateDecoder(mPCHandle, mTrackingId));
      mCreatedGmpPluginEvent.Forward(*gmpDecoder->InitPluginEvent());
      mReleasedGmpPluginEvent.Forward(*gmpDecoder->ReleasePluginEvent());
      decoder.reset(gmpDecoder.release());
      break;
    }

    // Use libvpx decoders as fallbacks.
    case webrtc::VideoCodecType::kVideoCodecVP8:
      if (!decoder) {
        decoder = webrtc::CreateVp8Decoder(aEnv);
      }
      break;
    case webrtc::VideoCodecType::kVideoCodecVP9:
      decoder = webrtc::VP9Decoder::Create();
      break;

    default:
      break;
  }

  return decoder;
}

std::unique_ptr<webrtc::VideoEncoder> WebrtcVideoEncoderFactory::Create(
    const webrtc::Environment& aEnv, const webrtc::SdpVideoFormat& aFormat) {
  if (!mInternalFactory->Supports(aFormat)) {
    return nullptr;
  }
  auto type = webrtc::PayloadStringToCodecType(aFormat.name);
  switch (type) {
    case webrtc::VideoCodecType::kVideoCodecVP8:
      // XXX We might be able to use the simulcast proxy for more codecs, but
      // that requires testing.
      return std::make_unique<webrtc::SimulcastEncoderAdapter>(
          aEnv, mInternalFactory.get(), nullptr, aFormat);
    default:
      return mInternalFactory->Create(aEnv, aFormat);
  }
}

bool WebrtcVideoEncoderFactory::InternalFactory::Supports(
    const webrtc::SdpVideoFormat& aFormat) {
  switch (webrtc::PayloadStringToCodecType(aFormat.name)) {
    case webrtc::VideoCodecType::kVideoCodecVP8:
    case webrtc::VideoCodecType::kVideoCodecVP9:
    case webrtc::VideoCodecType::kVideoCodecH264:
      return true;
    default:
      return false;
  }
}

std::unique_ptr<webrtc::VideoEncoder>
WebrtcVideoEncoderFactory::InternalFactory::Create(
    const webrtc::Environment& aEnv, const webrtc::SdpVideoFormat& aFormat) {
  MOZ_ASSERT(Supports(aFormat));

  std::unique_ptr<webrtc::VideoEncoder> platformEncoder;

  auto createPlatformEncoder = [&]() -> std::unique_ptr<webrtc::VideoEncoder> {
    std::unique_ptr<webrtc::VideoEncoder> platformEncoder;
    platformEncoder.reset(MediaDataCodec::CreateEncoder(aFormat));
    return platformEncoder;
  };

  auto createWebRTCEncoder =
      [this, &aEnv, &aFormat]() -> std::unique_ptr<webrtc::VideoEncoder> {
    std::unique_ptr<webrtc::VideoEncoder> encoder;
    switch (webrtc::PayloadStringToCodecType(aFormat.name)) {
      case webrtc::VideoCodecType::kVideoCodecH264: {
        // get an external encoder
        auto gmpEncoder =
            WrapUnique(GmpVideoCodec::CreateEncoder(aFormat, mPCHandle));
        mCreatedGmpPluginEvent.Forward(*gmpEncoder->InitPluginEvent());
        mReleasedGmpPluginEvent.Forward(*gmpEncoder->ReleasePluginEvent());
        encoder.reset(gmpEncoder.release());
        break;
      }
      // libvpx fallbacks.
      case webrtc::VideoCodecType::kVideoCodecVP8:
        encoder = webrtc::CreateVp8Encoder(aEnv);
        break;
      case webrtc::VideoCodecType::kVideoCodecVP9:
        encoder = webrtc::CreateVp9Encoder(aEnv);
        break;

      default:
        break;
    }
    return encoder;
  };

  // This is to be synced with the doc for the pref in StaticPrefs.yaml
  enum EncoderCreationStrategy {
    PreferWebRTCEncoder = 0,
    PreferPlatformEncoder = 1
  };

  std::unique_ptr<webrtc::VideoEncoder> encoder = nullptr;
  EncoderCreationStrategy strategy = static_cast<EncoderCreationStrategy>(
      StaticPrefs::media_webrtc_encoder_creation_strategy());
  switch (strategy) {
    case EncoderCreationStrategy::PreferWebRTCEncoder: {
      encoder = createWebRTCEncoder();
      // In a single case this happens: H264 is requested and OpenH264 isn't
      // available yet (e.g. first run). Attempt to use a platform encoder in
      // this case. They are not entirely ready yet but it's better than
      // erroring out.
      if (!encoder) {
        NS_WARNING(
            "Failed creating libwebrtc video encoder, falling back on platform "
            "encoder");
        return createPlatformEncoder();
      }
      return encoder;
    }
    case EncoderCreationStrategy::PreferPlatformEncoder:
      platformEncoder = createPlatformEncoder();
      encoder = createWebRTCEncoder();
      if (encoder && platformEncoder) {
        return webrtc::CreateVideoEncoderSoftwareFallbackWrapper(
            std::move(encoder), std::move(platformEncoder), false);
      }
      if (platformEncoder) {
        NS_WARNING(nsPrintfCString("No WebRTC encoder to fall back to for "
                                   "codec %s, only using platform encoder",
                                   aFormat.name.c_str())
                       .get());
        return platformEncoder;
      }
      return encoder;
  };

  MOZ_ASSERT_UNREACHABLE("Bad enum value");

  return nullptr;
}

}  // namespace mozilla
