/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppleEncoderModule.h"

#include "AppleVTEncoder.h"
#include "MP4Decoder.h"

namespace mozilla {

bool AppleEncoderModule::SupportsMimeType(const nsACString& aMimeType) const {
  return MP4Decoder::IsH264(aMimeType);
}

already_AddRefed<MediaDataEncoder> AppleEncoderModule::CreateVideoEncoder(
    const CreateEncoderParams& aParams) const {
  RefPtr<MediaDataEncoder> encoder(
      new AppleVTEncoder(aParams.ToH264Config(), aParams.mTaskQueue));
  return encoder.forget();
}

}  // namespace mozilla
