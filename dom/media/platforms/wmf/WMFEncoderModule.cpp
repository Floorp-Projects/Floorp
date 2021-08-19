/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFEncoderModule.h"

#include "WMFMediaDataEncoder.h"

namespace mozilla {
extern LazyLogModule sPEMLog;

bool WMFEncoderModule::SupportsMimeType(const nsACString& aMimeType) const {
  return CanCreateWMFEncoder(CreateEncoderParams::CodecTypeForMime(aMimeType));
}

already_AddRefed<MediaDataEncoder> WMFEncoderModule::CreateVideoEncoder(
    const CreateEncoderParams& aParams) const {
  MediaDataEncoder::CodecType codec =
      CreateEncoderParams::CodecTypeForMime(aParams.mConfig.mMimeType);
  RefPtr<MediaDataEncoder> encoder;
  switch (codec) {
    case MediaDataEncoder::CodecType::H264:
      encoder = new WMFMediaDataEncoder<MediaDataEncoder::H264Config>(
          aParams.ToH264Config(), aParams.mTaskQueue);
      break;
    case MediaDataEncoder::CodecType::VP8:
      encoder = new WMFMediaDataEncoder<MediaDataEncoder::VP8Config>(
          aParams.ToVP8Config(), aParams.mTaskQueue);
    case MediaDataEncoder::CodecType::VP9:
      encoder = new WMFMediaDataEncoder<MediaDataEncoder::VP9Config>(
          aParams.ToVP9Config(), aParams.mTaskQueue);
      break;
    default:
      // Do nothing.
      break;
  }
  return encoder.forget();
}

}  // namespace mozilla
