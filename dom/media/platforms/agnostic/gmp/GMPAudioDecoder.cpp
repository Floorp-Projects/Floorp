/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPAudioDecoder.h"
#include "nsServiceManagerUtils.h"
#include "MediaInfo.h"
#include "GMPDecoderModule.h"

namespace mozilla {

#if defined(DEBUG)
bool IsOnGMPThread()
{
  nsCOMPtr<mozIGeckoMediaPluginService> mps = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  MOZ_ASSERT(mps);

  nsCOMPtr<nsIThread> gmpThread;
  nsresult rv = mps->GetThread(getter_AddRefs(gmpThread));
  MOZ_ASSERT(NS_SUCCEEDED(rv) && gmpThread);
  return NS_GetCurrentThread() == gmpThread;
}
#endif

void
AudioCallbackAdapter::Decoded(const nsTArray<int16_t>& aPCM, uint64_t aTimeStamp, uint32_t aChannels, uint32_t aRate)
{
  MOZ_ASSERT(IsOnGMPThread());

  if (aRate == 0 || aChannels == 0) {
    NS_WARNING("Invalid rate or num channels returned on GMP audio samples");
    mCallback->Error();
    return;
  }

  size_t numFrames = aPCM.Length() / aChannels;
  MOZ_ASSERT((aPCM.Length() % aChannels) == 0);
  nsAutoArrayPtr<AudioDataValue> audioData(new AudioDataValue[aPCM.Length()]);

  for (size_t i = 0; i < aPCM.Length(); ++i) {
    audioData[i] = AudioSampleToFloat(aPCM[i]);
  }

  if (mMustRecaptureAudioPosition) {
    mAudioFrameSum = 0;
    auto timestamp = UsecsToFrames(aTimeStamp, aRate);
    if (!timestamp.isValid()) {
      NS_WARNING("Invalid timestamp");
      mCallback->Error();
      return;
    }
    mAudioFrameOffset = timestamp.value();
    mMustRecaptureAudioPosition = false;
  }

  auto timestamp = FramesToUsecs(mAudioFrameOffset + mAudioFrameSum, aRate);
  if (!timestamp.isValid()) {
    NS_WARNING("Invalid timestamp on audio samples");
    mCallback->Error();
    return;
  }
  mAudioFrameSum += numFrames;

  auto duration = FramesToUsecs(numFrames, aRate);
  if (!duration.isValid()) {
    NS_WARNING("Invalid duration on audio samples");
    mCallback->Error();
    return;
  }

  nsRefPtr<AudioData> audio(new AudioData(mLastStreamOffset,
                                          timestamp.value(),
                                          duration.value(),
                                          numFrames,
                                          audioData.forget(),
                                          aChannels,
                                          aRate));

#ifdef LOG_SAMPLE_DECODE
  LOG("Decoded audio sample! timestamp=%lld duration=%lld currentLength=%u",
      timestamp, duration, currentLength);
#endif

  mCallback->Output(audio);
}

void
AudioCallbackAdapter::InputDataExhausted()
{
  MOZ_ASSERT(IsOnGMPThread());
  mCallback->InputExhausted();
}

void
AudioCallbackAdapter::DrainComplete()
{
  MOZ_ASSERT(IsOnGMPThread());
  mCallback->DrainComplete();
}

void
AudioCallbackAdapter::ResetComplete()
{
  MOZ_ASSERT(IsOnGMPThread());
  mMustRecaptureAudioPosition = true;
  mCallback->FlushComplete();
}

void
AudioCallbackAdapter::Error(GMPErr aErr)
{
  MOZ_ASSERT(IsOnGMPThread());
  mCallback->Error();
}

void
AudioCallbackAdapter::Terminated()
{
  NS_WARNING("AAC GMP decoder terminated.");
  mCallback->Error();
}

void
GMPAudioDecoder::InitTags(nsTArray<nsCString>& aTags)
{
  aTags.AppendElement(NS_LITERAL_CSTRING("aac"));
  const Maybe<nsCString> gmp(
    GMPDecoderModule::PreferredGMP(NS_LITERAL_CSTRING("audio/mp4a-latm")));
  if (gmp.isSome()) {
    aTags.AppendElement(gmp.value());
  }
}

nsCString
GMPAudioDecoder::GetNodeId()
{
  return NS_LITERAL_CSTRING("");
}

void
GMPAudioDecoder::GMPInitDone(GMPAudioDecoderProxy* aGMP)
{
  if (!aGMP) {
    mInitPromise.Reject(MediaDataDecoder::DecoderFailureReason::INIT_ERROR, __func__);
    return;
  }
  nsTArray<uint8_t> codecSpecific;
  codecSpecific.AppendElements(mConfig.mCodecSpecificConfig->Elements(),
                               mConfig.mCodecSpecificConfig->Length());

  nsresult rv = aGMP->InitDecode(kGMPAudioCodecAAC,
                                 mConfig.mChannels,
                                 mConfig.mBitDepth,
                                 mConfig.mRate,
                                 codecSpecific,
                                 mAdapter);
  if (NS_FAILED(rv)) {
    aGMP->Close();
    mInitPromise.Reject(MediaDataDecoder::DecoderFailureReason::INIT_ERROR, __func__);
    return;
  }

  mGMP = aGMP;
  mInitPromise.Resolve(TrackInfo::kAudioTrack, __func__);
}

nsRefPtr<MediaDataDecoder::InitPromise>
GMPAudioDecoder::Init()
{
  MOZ_ASSERT(IsOnGMPThread());

  mMPS = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  MOZ_ASSERT(mMPS);

  nsRefPtr<InitPromise> promise(mInitPromise.Ensure(__func__));

  nsTArray<nsCString> tags;
  InitTags(tags);
  UniquePtr<GetGMPAudioDecoderCallback> callback(new GMPInitDoneCallback(this));
  if (NS_FAILED(mMPS->GetGMPAudioDecoder(&tags, GetNodeId(), Move(callback)))) {
    mInitPromise.Reject(MediaDataDecoder::DecoderFailureReason::INIT_ERROR, __func__);
  }

  return promise;
}

nsresult
GMPAudioDecoder::Input(MediaRawData* aSample)
{
  MOZ_ASSERT(IsOnGMPThread());

  nsRefPtr<MediaRawData> sample(aSample);
  if (!mGMP) {
    mCallback->Error();
    return NS_ERROR_FAILURE;
  }

  mAdapter->SetLastStreamOffset(sample->mOffset);

  gmp::GMPAudioSamplesImpl samples(sample, mConfig.mChannels, mConfig.mRate);
  nsresult rv = mGMP->Decode(samples);
  if (NS_FAILED(rv)) {
    mCallback->Error();
    return rv;
  }

  return NS_OK;
}

nsresult
GMPAudioDecoder::Flush()
{
  MOZ_ASSERT(IsOnGMPThread());

  if (!mGMP || NS_FAILED(mGMP->Reset())) {
    // Abort the flush.
    mCallback->FlushComplete();
  }

  return NS_OK;
}

nsresult
GMPAudioDecoder::Drain()
{
  MOZ_ASSERT(IsOnGMPThread());

  if (!mGMP || NS_FAILED(mGMP->Drain())) {
    mCallback->DrainComplete();
  }

  return NS_OK;
}

nsresult
GMPAudioDecoder::Shutdown()
{
  mInitPromise.RejectIfExists(MediaDataDecoder::DecoderFailureReason::CANCELED, __func__);
  if (!mGMP) {
    return NS_ERROR_FAILURE;
  }
  mGMP->Close();
  mGMP = nullptr;

  return NS_OK;
}

} // namespace mozilla
