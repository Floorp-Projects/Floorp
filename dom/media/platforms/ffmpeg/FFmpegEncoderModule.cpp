/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegEncoderModule.h"

// This must be the last header included
#include "FFmpegLibs.h"

namespace mozilla {

template <int V>
bool FFmpegEncoderModule<V>::SupportsMimeType(
    const nsACString& aMimeType) const {
  // TODO: Check aMimeType to determine if we support or not.
  return false;
}

template <int V>
already_AddRefed<MediaDataEncoder> FFmpegEncoderModule<V>::CreateVideoEncoder(
    const CreateEncoderParams& aParams, const bool aHardwareNotAllowed) const {
  MediaDataEncoder::CodecType ct =
      CreateEncoderParams::CodecTypeForMime(aParams.mConfig.mMimeType);
  // TODO: Create a FFmpegVideoDecoder if we support the mime type specified in
  // aParams.
  return nullptr;
}

template class FFmpegEncoderModule<LIBAV_VER>;

}  // namespace mozilla
