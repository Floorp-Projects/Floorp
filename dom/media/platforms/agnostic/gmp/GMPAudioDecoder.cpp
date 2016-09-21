/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPAudioDecoder.h"
#include "nsServiceManagerUtils.h"
#include "MediaInfo.h"
#include "GMPDecoderModule.h"
#include "nsPrintfCString.h"

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
    mCallback->Error(MediaResult(
      NS_ERROR_DOM_MEDIA_FATAL_ERR,
      RESULT_DETAIL(
        "Invalid rate or num channels returned on GMP audio samples")));
    return;
  }

  size_t numFrames = aPCM.Length() / aChannels;
  MOZ_ASSERT((aPCM.Length() % aChannels) == 0);
  AlignedAudioBuffer audioData(aPCM.Length());
  if (!audioData) {
    mCallback->Error(
      MediaResult(NS_ERROR_OUT_OF_MEMORY,
                  RESULT_DETAIL("Unable to allocate audio buffer")));
    return;
  }

  for (size_t i = 0; i < aPCM.Length(); ++i) {
    audioData[i] = AudioSampleToFloat(aPCM[i]);
  }

  if (mMustRecaptureAudioPosition) {
    mAudioFrameSum = 0;
    auto timestamp = UsecsToFrames(aTimeStamp, aRate);
    if (!timestamp.isValid()) {
      mCallback->Error(MediaResult(NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
                                   RESULT_DETAIL("Invalid timestamp")));
      return;
    }
    mAudioFrameOffset = timestamp.value();
    mMustRecaptureAudioPosition = false;
  }

  auto timestamp = FramesToUsecs(mAudioFrameOffset + mAudioFrameSum, aRate);
  if (!timestamp.isValid()) {
    mCallback->Error(
      MediaResult(NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
                  RESULT_DETAIL("Invalid timestamp on audio samples")));
    return;
  }
  mAudioFrameSum += numFrames;

  auto duration = FramesToUsecs(numFrames, aRate);
  if (!duration.isValid()) {
    mCallback->Error(
      MediaResult(NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
                  RESULT_DETAIL("Invalid duration on audio samples")));
    return;
  }

  RefPtr<AudioData> audio(new AudioData(mLastStreamOffset,
                                        timestamp.value(),
                                        duration.value(),
                                        numFrames,
                                        Move(audioData),
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
  mCallback->Error(MediaResult(aErr == GMPDecodeErr
                               ? NS_ERROR_DOM_MEDIA_DECODE_ERR
                               : NS_ERROR_DOM_MEDIA_FATAL_ERR,
                               RESULT_DETAIL("GMPErr:%x", aErr)));
}

void
AudioCallbackAdapter::Terminated()
{
  mCallback->Error(MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                               RESULT_DETAIL("Audio GMP decoder terminated.")));
}

GMPAudioDecoderParams::GMPAudioDecoderParams(const CreateDecoderParams& aParams)
  : mConfig(aParams.AudioConfig())
  , mTaskQueue(aParams.mTaskQueue)
  , mCallback(nullptr)
  , mAdapter(nullptr)
  , mCrashHelper(aParams.mCrashHelper)
{}

GMPAudioDecoderParams&
GMPAudioDecoderParams::WithCallback(MediaDataDecoderProxy* aWrapper)
{
  MOZ_ASSERT(aWrapper);
  MOZ_ASSERT(!mCallback); // Should only be called once per instance.
  mCallback = aWrapper->Callback();
  mAdapter = nullptr;
  return *this;
}

GMPAudioDecoderParams&
GMPAudioDecoderParams::WithAdapter(AudioCallbackAdapter* aAdapter)
{
  MOZ_ASSERT(aAdapter);
  MOZ_ASSERT(!mAdapter); // Should only be called once per instance.
  mCallback = aAdapter->Callback();
  mAdapter = aAdapter;
  return *this;
}

GMPAudioDecoder::GMPAudioDecoder(const GMPAudioDecoderParams& aParams)
  : mConfig(aParams.mConfig)
  , mCallback(aParams.mCallback)
  , mGMP(nullptr)
  , mAdapter(aParams.mAdapter)
  , mCrashHelper(aParams.mCrashHelper)
{
  MOZ_ASSERT(!mAdapter || mCallback == mAdapter->Callback());
  if (!mAdapter) {
    mAdapter = new AudioCallbackAdapter(mCallback);
  }
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
  return SHARED_GMP_DECODING_NODE_ID;
}

void
GMPAudioDecoder::GMPInitDone(GMPAudioDecoderProxy* aGMP)
{
  MOZ_ASSERT(IsOnGMPThread());

  if (!aGMP) {
    mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
    return;
  }
  if (mInitPromise.IsEmpty()) {
    // GMP must have been shutdown while we were waiting for Init operation
    // to complete.
    aGMP->Close();
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
    mInitPromise.Reject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
    return;
  }

  mGMP = aGMP;
  mInitPromise.Resolve(TrackInfo::kAudioTrack, __func__);
}

RefPtr<MediaDataDecoder::InitPromise>
GMPAudioDecoder::Init()
{
  MOZ_ASSERT(IsOnGMPThread());

  mMPS = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  MOZ_ASSERT(mMPS);

  RefPtr<InitPromise> promise(mInitPromise.Ensure(__func__));

  nsTArray<nsCString> tags;
  InitTags(tags);
  UniquePtr<GetGMPAudioDecoderCallback> callback(new GMPInitDoneCallback(this));
  if (NS_FAILED(mMPS->GetGMPAudioDecoder(mCrashHelper, &tags, GetNodeId(), Move(callback)))) {
    mInitPromise.Reject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }

  return promise;
}

void
GMPAudioDecoder::Input(MediaRawData* aSample)
{
  MOZ_ASSERT(IsOnGMPThread());

  RefPtr<MediaRawData> sample(aSample);
  if (!mGMP) {
    mCallback->Error(MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                 RESULT_DETAIL("mGMP not initialized")));
    return;
  }

  mAdapter->SetLastStreamOffset(sample->mOffset);

  gmp::GMPAudioSamplesImpl samples(sample, mConfig.mChannels, mConfig.mRate);
  nsresult rv = mGMP->Decode(samples);
  if (NS_FAILED(rv)) {
    mCallback->Error(MediaResult(rv, __func__));
  }
}

void
GMPAudioDecoder::Flush()
{
  MOZ_ASSERT(IsOnGMPThread());

  if (!mGMP || NS_FAILED(mGMP->Reset())) {
    // Abort the flush.
    mCallback->FlushComplete();
  }
}

void
GMPAudioDecoder::Drain()
{
  MOZ_ASSERT(IsOnGMPThread());

  if (!mGMP || NS_FAILED(mGMP->Drain())) {
    mCallback->DrainComplete();
  }
}

void
GMPAudioDecoder::Shutdown()
{
  mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  if (!mGMP) {
    return;
  }
  // Note this unblocks flush and drain operations waiting for callbacks.
  mGMP->Close();
  mGMP = nullptr;
}

} // namespace mozilla
