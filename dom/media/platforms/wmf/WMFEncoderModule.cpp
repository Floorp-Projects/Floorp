/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFEncoderModule.h"

#include "MP4Decoder.h"
#include "WMFMediaDataEncoder.h"

namespace mozilla {
extern LazyLogModule sPEMLog;

static MediaDataEncoder::CodecType MimeTypeToCodecType(
    const nsACString& aMimeType) {
  if (MP4Decoder::IsH264(aMimeType)) {
    return MediaDataEncoder::CodecType::H264;
  } else {
    MOZ_ASSERT(false, "Unsupported Mimetype");
    return MediaDataEncoder::CodecType::Unknown;
  }
}

bool WMFEncoderModule::SupportsMimeType(const nsACString& aMimeType) const {
  return CanCreateWMFEncoder(MimeTypeToCodecType(aMimeType));
}

already_AddRefed<MediaDataEncoder> WMFEncoderModule::CreateVideoEncoder(
    const CreateEncoderParams& aParams) const {
  MediaDataEncoder::CodecType codec =
      MimeTypeToCodecType(aParams.mConfig.mMimeType);
  RefPtr<MediaDataEncoder> encoder;
  switch (codec) {
    case MediaDataEncoder::CodecType::H264:
      encoder = new WMFMediaDataEncoder<MediaDataEncoder::H264Config>(
          aParams.ToH264Config(), aParams.mTaskQueue);
      break;
    default:
      // Do nothing.
      break;
  }
  return encoder.forget();
}

}  // namespace mozilla
