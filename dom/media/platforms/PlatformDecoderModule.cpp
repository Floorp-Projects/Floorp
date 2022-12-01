/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlatformDecoderModule.h"

#include "ImageContainer.h"
#include "nsPrintfCString.h"

namespace mozilla {

CreateDecoderParamsForAsync::CreateDecoderParamsForAsync(
    const CreateDecoderParams& aParams)
    : mConfig(aParams.mConfig.Clone()),
      mImageContainer(aParams.mImageContainer),
      mKnowsCompositor(aParams.mKnowsCompositor),
      mCrashHelper(aParams.mCrashHelper),
      mUseNullDecoder(aParams.mUseNullDecoder),
      mNoWrapper(aParams.mNoWrapper),
      mType(aParams.mType),
      mOnWaitingForKeyEvent(aParams.mOnWaitingForKeyEvent),
      mOptions(aParams.mOptions),
      mRate(aParams.mRate),
      mMediaEngineId(aParams.mMediaEngineId),
      mTrackingId(aParams.mTrackingId) {}

CreateDecoderParamsForAsync::CreateDecoderParamsForAsync(
    CreateDecoderParamsForAsync&& aParams) = default;

RefPtr<PlatformDecoderModule::CreateDecoderPromise>
PlatformDecoderModule::AsyncCreateDecoder(const CreateDecoderParams& aParams) {
  RefPtr<MediaDataDecoder> decoder;
  MediaResult result = NS_OK;
  if (aParams.mConfig.IsAudio()) {
    decoder = CreateAudioDecoder(CreateDecoderParams{aParams, &result});
  } else if (aParams.mConfig.IsVideo()) {
    decoder = CreateVideoDecoder(CreateDecoderParams{aParams, &result});
  }
  if (!decoder) {
    if (NS_FAILED(result)) {
      return CreateDecoderPromise::CreateAndReject(result, __func__);
    }
    return CreateDecoderPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    nsPrintfCString("Error creating decoder for %s",
                                    aParams.mConfig.mMimeType.get())
                        .get()),
        __func__);
  }
  return CreateDecoderPromise::CreateAndResolve(decoder, __func__);
}

}  // namespace mozilla
