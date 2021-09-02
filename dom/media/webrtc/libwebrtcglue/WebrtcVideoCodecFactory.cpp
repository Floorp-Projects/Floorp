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
#include "media/engine/encoder_simulcast_proxy.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"

namespace mozilla {

std::unique_ptr<webrtc::VideoDecoder>
WebrtcVideoDecoderFactory::CreateVideoDecoder(
    const webrtc::SdpVideoFormat& aFormat) {
  std::unique_ptr<webrtc::VideoDecoder> decoder;
  auto type = webrtc::PayloadStringToCodecType(aFormat.name);

  // Attempt to create a decoder using MediaDataDecoder.
  decoder.reset(MediaDataCodec::CreateDecoder(type));
  if (decoder) {
    return decoder;
  }

  switch (type) {
    case webrtc::VideoCodecType::kVideoCodecH264: {
      // Get an external decoder
      auto gmpDecoder = WrapUnique(GmpVideoCodec::CreateDecoder(mPCHandle));
      mCreatedGmpPluginEvent->Forward(*gmpDecoder->InitPluginEvent());
      mReleasedGmpPluginEvent->Forward(*gmpDecoder->ReleasePluginEvent());
      decoder.reset(gmpDecoder.release());
      break;
    }

    case webrtc::VideoCodecType::kVideoCodecVP8:
#ifdef MOZ_WEBRTC_MEDIACODEC
      // Attempt to get a MediaCodec decoder
      if (StaticPrefs::
              media_navigator_hardware_vp8_decode_acceleration_enabled()) {
        nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
        if (gfxInfo) {
          int32_t status;
          nsCString discardFailureId;

          if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(
                  nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION_DECODE,
                  discardFailureId, &status))) {
            if (status != nsIGfxInfo::FEATURE_STATUS_OK) {
              NS_WARNING("VP8 decoder hardware is blocked: disabling.\n");
            } else {
              decoder = MediaCodecVideoCodec::CreateDecoder(
                  MediaCodecVideoCodec::CodecType::CODEC_VP8);
            }
          }
        }
      }
#endif
      // Use a software VP8 decoder as a fallback.
      if (!decoder) {
        decoder = webrtc::VP8Decoder::Create();
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

std::unique_ptr<webrtc::VideoEncoder>
WebrtcVideoEncoderFactory::CreateVideoEncoder(
    const webrtc::SdpVideoFormat& aFormat) {
  if (!mInternalFactory->Supports(aFormat)) {
    return nullptr;
  }
  auto type = webrtc::PayloadStringToCodecType(aFormat.name);
  switch (type) {
    case webrtc::VideoCodecType::kVideoCodecVP8:
      // XXX We might be able to use the simulcast proxy for more codecs, but
      // that requires testing.
      return std::make_unique<webrtc::EncoderSimulcastProxy>(
          mInternalFactory.get(), aFormat);
    default:
      return mInternalFactory->CreateVideoEncoder(aFormat);
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
WebrtcVideoEncoderFactory::InternalFactory::CreateVideoEncoder(
    const webrtc::SdpVideoFormat& aFormat) {
  MOZ_ASSERT(Supports(aFormat));
  std::unique_ptr<webrtc::VideoEncoder> encoder;
  auto type = webrtc::PayloadStringToCodecType(aFormat.name);

  if (StaticPrefs::media_webrtc_platformencoder()) {
    encoder.reset(MediaDataCodec::CreateEncoder(type));
    if (encoder) {
      return encoder;
    }
  }

  switch (type) {
    case webrtc::VideoCodecType::kVideoCodecH264: {
      // get an external encoder
      auto gmpEncoder = WrapUnique(GmpVideoCodec::CreateEncoder(mPCHandle));
      mCreatedGmpPluginEvent->Forward(*gmpEncoder->InitPluginEvent());
      mReleasedGmpPluginEvent->Forward(*gmpEncoder->ReleasePluginEvent());
      encoder.reset(gmpEncoder.release());
      break;
    }

    case webrtc::VideoCodecType::kVideoCodecVP8:
#ifdef MOZ_WEBRTC_MEDIACODEC
      // Attempt to get a MediaCodec encoder
      if (StaticPrefs::
              media_navigator_hardware_vp8_encode_acceleration_enabled()) {
        nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
        if (gfxInfo) {
          int32_t status;
          nsCString discardFailureId;

          if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(
                  nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION_ENCODE,
                  discardFailureId, &status))) {
            if (status != nsIGfxInfo::FEATURE_STATUS_OK) {
              NS_WARNING("VP8 encoder hardware is blocked: disabling.\n");
            } else {
              encoder = MediaCodecVideoCodec::CreateEncoder(
                  MediaCodecVideoCodec::CodecType::CODEC_VP8);
            }
          }
        }
      }
#endif
      if (!encoder) {
        // Software fallback
        encoder = webrtc::VP8Encoder::Create();
      }
      break;

    case webrtc::VideoCodecType::kVideoCodecVP9:
      encoder = webrtc::VP9Encoder::Create();
      break;

    default:
      break;
  }
  return encoder;
}

}  // namespace mozilla
