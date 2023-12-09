/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlatformEncoderModule.h"
#include "nsPrintfCString.h"

namespace mozilla {

RefPtr<PlatformEncoderModule::CreateEncoderPromise>
PlatformEncoderModule::AsyncCreateEncoder(const EncoderConfig& aEncoderConfig,
                                          const RefPtr<TaskQueue>& aTaskQueue) {
  RefPtr<MediaDataEncoder> encoder;
  MediaResult result = NS_OK;
  if (aEncoderConfig.IsAudio()) {
    encoder = CreateAudioEncoder(aEncoderConfig, aTaskQueue);
  } else if (aEncoderConfig.IsVideo()) {
    encoder = CreateVideoEncoder(aEncoderConfig, aTaskQueue);
  }
  if (!encoder) {
    if (NS_FAILED(result)) {
      return CreateEncoderPromise::CreateAndReject(result, __func__);
    }
    return CreateEncoderPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    nsPrintfCString("Error creating encoder for %d",
                                    static_cast<int>(aEncoderConfig.mCodec))
                        .get()),
        __func__);
  }
  return CreateEncoderPromise::CreateAndResolve(encoder, __func__);
}

}
