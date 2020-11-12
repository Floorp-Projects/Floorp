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
      mRate(aParams.mRate) {}

CreateDecoderParamsForAsync::CreateDecoderParamsForAsync(
    CreateDecoderParamsForAsync&& aParams) = default;

RefPtr<PlatformDecoderModule::CreateDecoderPromise>
PlatformDecoderModule::AsyncCreateDecoder(const CreateDecoderParams& aParams) {
  RefPtr<MediaDataDecoder> decoder;
  if (aParams.mError) {
    *aParams.mError = NS_OK;
  }
  if (aParams.mConfig.IsAudio()) {
    decoder = CreateAudioDecoder(aParams);
  } else if (aParams.mConfig.IsVideo()) {
    decoder = CreateAudioDecoder(aParams);
  }
  if (!decoder) {
    if (aParams.mError && NS_FAILED(*aParams.mError)) {
      return CreateDecoderPromise::CreateAndReject(*aParams.mError, __func__);
    }
    return CreateDecoderPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    nsPrintfCString("Error creating decoder for %s",
                                    aParams.mConfig.mMimeType.get())
                        .get()),
        __func__);
  }
  RefPtr<CreateDecoderPromise> p = decoder->Init()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [decoder](MediaDataDecoder::InitPromise::ResolveOrRejectValue&& aValue) {
        RefPtr<CreateDecoderPromise> p;
        if (aValue.IsReject()) {
          p = decoder->Shutdown()->Then(
              GetCurrentSerialEventTarget(), __func__,
              [reject = aValue.RejectValue()]() {
                return CreateDecoderPromise::CreateAndReject(reject, __func__);
              });
        } else {
          p = CreateDecoderPromise::CreateAndResolve(decoder, __func__);
        }
        return p;
      });
  return p;
}

}  // namespace mozilla
