/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppleEncoderModule.h"

#include "nsMimeTypes.h"

#include "AppleVTEncoder.h"

namespace mozilla {

bool AppleEncoderModule::SupportsMimeType(const nsACString& aMimeType) const {
  return aMimeType.EqualsLiteral(VIDEO_MP4) ||
         aMimeType.EqualsLiteral("video/avc");
}

already_AddRefed<MediaDataEncoder> AppleEncoderModule::CreateVideoEncoder(
    const CreateEncoderParams& aParams) const {
  const VideoInfo* info = aParams.mConfig.GetAsVideoInfo();
  MOZ_ASSERT(info);

  using Config = AppleVTEncoder::Config;
  Config config =
      Config(MediaDataEncoder::CodecType::H264, aParams.mUsage, info->mImage,
             aParams.mPixelFormat, aParams.mFramerate, aParams.mBitrate);
  if (aParams.mCodecSpecific) {
    config.SetCodecSpecific(aParams.mCodecSpecific.ref().mH264);
  }

  RefPtr<MediaDataEncoder> encoder(
      new AppleVTEncoder(std::forward<Config>(config), aParams.mTaskQueue));
  return encoder.forget();
}

}  // namespace mozilla
