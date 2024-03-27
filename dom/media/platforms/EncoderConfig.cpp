/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EncoderConfig.h"
#include "MP4Decoder.h"
#include "VPXDecoder.h"

namespace mozilla {

CodecType EncoderConfig::CodecTypeForMime(const nsACString& aMimeType) {
  if (MP4Decoder::IsH264(aMimeType)) {
    return CodecType::H264;
  }
  if (VPXDecoder::IsVPX(aMimeType, VPXDecoder::VP8)) {
    return CodecType::VP8;
  }
  if (VPXDecoder::IsVPX(aMimeType, VPXDecoder::VP9)) {
    return CodecType::VP9;
  }
  MOZ_ASSERT_UNREACHABLE("Unsupported Mimetype");
  return CodecType::Unknown;
}

}  // namespace mozilla
