/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDataCodec.h"

#include "PDMFactory.h"
#include "WebrtcGmpVideoCodec.h"
#include "WebrtcMediaDataDecoderCodec.h"
#include "WebrtcMediaDataEncoderCodec.h"
#include "mozilla/StaticPrefs_media.h"

namespace mozilla {

/* static */
WebrtcVideoEncoder* MediaDataCodec::CreateEncoder(
    const webrtc::SdpVideoFormat& aFormat) {
  if (!StaticPrefs::media_webrtc_platformencoder()) {
    return nullptr;
  }
  if (!WebrtcMediaDataEncoder::CanCreate(
          webrtc::PayloadStringToCodecType(aFormat.name))) {
    return nullptr;
  }

  return new WebrtcVideoEncoderProxy(new WebrtcMediaDataEncoder(aFormat));
}

/* static */
WebrtcVideoDecoder* MediaDataCodec::CreateDecoder(
    webrtc::VideoCodecType aCodecType, TrackingId aTrackingId) {
  switch (aCodecType) {
    case webrtc::VideoCodecType::kVideoCodecVP8:
    case webrtc::VideoCodecType::kVideoCodecVP9:
      if (!StaticPrefs::media_navigator_mediadatadecoder_vpx_enabled()) {
        return nullptr;
      }
      break;
    case webrtc::VideoCodecType::kVideoCodecH264:
      if (!StaticPrefs::media_navigator_mediadatadecoder_h264_enabled()) {
        return nullptr;
      }
      break;
    default:
      return nullptr;
  }

  nsAutoCString codec;
  switch (aCodecType) {
    case webrtc::VideoCodecType::kVideoCodecVP8:
      codec = "video/vp8";
      break;
    case webrtc::VideoCodecType::kVideoCodecVP9:
      codec = "video/vp9";
      break;
    case webrtc::VideoCodecType::kVideoCodecH264:
      codec = "video/avc";
      break;
    default:
      return nullptr;
  }
  RefPtr<PDMFactory> pdm = new PDMFactory();
  if (pdm->SupportsMimeType(codec).isEmpty()) {
    return nullptr;
  }

  return new WebrtcMediaDataDecoder(codec, aTrackingId);
}

}  // namespace mozilla
