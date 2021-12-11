/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "common/browser_logging/CSFLog.h"
#include "nspr.h"
#include "mozilla/StaticPrefs_media.h"

#include "WebrtcMediaCodecVP8VideoCodec.h"
#include "MediaCodecVideoCodec.h"

namespace mozilla {

static const char* mcvcLogTag = "MediaCodecVideoCodec";
#ifdef LOGTAG
#  undef LOGTAG
#endif
#define LOGTAG mcvcLogTag

WebrtcVideoEncoder* MediaCodecVideoCodec::CreateEncoder(CodecType aCodecType) {
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);
  if (aCodecType == CODEC_VP8) {
    if (StaticPrefs::
            media_navigator_hardware_vp8_encode_acceleration_remote_enabled()) {
      return new WebrtcMediaCodecVP8VideoRemoteEncoder();
    } else {
      return new WebrtcMediaCodecVP8VideoEncoder();
    }
  }
  return nullptr;
}

WebrtcVideoDecoder* MediaCodecVideoCodec::CreateDecoder(CodecType aCodecType) {
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);
  if (aCodecType == CODEC_VP8) {
    return new WebrtcMediaCodecVP8VideoDecoder();
  }
  return nullptr;
}

}  // namespace mozilla
