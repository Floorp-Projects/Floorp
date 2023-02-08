/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFAudioMFTManager.h"
#include "MediaInfo.h"
#include "TimeUnits.h"
#include "VideoUtils.h"
#include "WMFUtils.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Logging.h"
#include "mozilla/Telemetry.h"
#include "nsTArray.h"

#define LOG(...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {

using media::TimeUnit;

WMFAudioMFTManager::WMFAudioMFTManager(const AudioInfo& aConfig)
    : mAudioChannels(aConfig.mChannels),
      mChannelsMap(AudioConfig::ChannelLayout::UNKNOWN_MAP),
      mAudioRate(aConfig.mRate),
      mStreamType(GetStreamTypeFromMimeType(aConfig.mMimeType)) {
  MOZ_COUNT_CTOR(WMFAudioMFTManager);

  if (mStreamType == WMFStreamType::AAC) {
    const uint8_t* audioSpecConfig;
    uint32_t configLength;
    if (aConfig.mCodecSpecificConfig.is<AacCodecSpecificData>()) {
      const AacCodecSpecificData& aacCodecSpecificData =
          aConfig.mCodecSpecificConfig.as<AacCodecSpecificData>();
      audioSpecConfig =
          aacCodecSpecificData.mDecoderConfigDescriptorBinaryBlob->Elements();
      configLength =
          aacCodecSpecificData.mDecoderConfigDescriptorBinaryBlob->Length();
    } else {
      // Gracefully handle failure to cover all codec specific cases above. Once
      // we're confident there is no fall through from these cases above, we
      // should remove this code.
      RefPtr<MediaByteBuffer> audioCodecSpecificBinaryBlob =
          GetAudioCodecSpecificBlob(aConfig.mCodecSpecificConfig);
      audioSpecConfig = audioCodecSpecificBinaryBlob->Elements();
      configLength = audioCodecSpecificBinaryBlob->Length();
    }
    AACAudioSpecificConfigToUserData(aConfig.mExtendedProfile, audioSpecConfig,
                                     configLength, mUserData);
  }
}

WMFAudioMFTManager::~WMFAudioMFTManager() {
  MOZ_COUNT_DTOR(WMFAudioMFTManager);
}

const GUID& WMFAudioMFTManager::GetMediaSubtypeGUID() {
  MOZ_ASSERT(StreamTypeIsAudio(mStreamType));
  switch (mStreamType) {
    case WMFStreamType::AAC:
      return MFAudioFormat_AAC;
    case WMFStreamType::MP3:
      return MFAudioFormat_MP3;
    default:
      return GUID_NULL;
  };
}

bool WMFAudioMFTManager::Init() {
  NS_ENSURE_TRUE(StreamTypeIsAudio(mStreamType), false);

  RefPtr<MFTDecoder> decoder(new MFTDecoder());
  // Note: MP3 MFT isn't registered as supporting Float output, but it works.
  // Find PCM output MFTs as this is the common type.
  HRESULT hr = WMFDecoderModule::CreateMFTDecoder(mStreamType, decoder);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  // Setup input/output media types
  RefPtr<IMFMediaType> inputType;

  hr = wmf::MFCreateMediaType(getter_AddRefs(inputType));
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = inputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = inputType->SetGUID(MF_MT_SUBTYPE, GetMediaSubtypeGUID());
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = inputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, mAudioRate);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = inputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, mAudioChannels);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  if (mStreamType == WMFStreamType::AAC) {
    hr = inputType->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0x0);  // Raw AAC packet
    NS_ENSURE_TRUE(SUCCEEDED(hr), false);

    hr = inputType->SetBlob(MF_MT_USER_DATA, mUserData.Elements(),
                            mUserData.Length());
    NS_ENSURE_TRUE(SUCCEEDED(hr), false);
  }

  RefPtr<IMFMediaType> outputType;
  hr = wmf::MFCreateMediaType(getter_AddRefs(outputType));
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = outputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = outputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = decoder->SetMediaTypes(inputType, outputType);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  mDecoder = decoder;

  return true;
}

HRESULT
WMFAudioMFTManager::Input(MediaRawData* aSample) {
  mLastInputTime = aSample->mTime;
  return mDecoder->Input(aSample->Data(), uint32_t(aSample->Size()),
                         aSample->mTime.ToMicroseconds(),
                         aSample->mDuration.ToMicroseconds());
}

HRESULT
WMFAudioMFTManager::UpdateOutputType() {
  HRESULT hr;

  RefPtr<IMFMediaType> type;
  hr = mDecoder->GetOutputMediaType(type);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &mAudioRate);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &mAudioChannels);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  uint32_t channelsMap;
  hr = type->GetUINT32(MF_MT_AUDIO_CHANNEL_MASK, &channelsMap);
  if (SUCCEEDED(hr)) {
    mChannelsMap = channelsMap;
  } else {
    LOG("Unable to retrieve channel layout. Ignoring");
    mChannelsMap = AudioConfig::ChannelLayout::UNKNOWN_MAP;
  }

  return S_OK;
}

HRESULT
WMFAudioMFTManager::Output(int64_t aStreamOffset, RefPtr<MediaData>& aOutData) {
  aOutData = nullptr;
  RefPtr<IMFSample> sample;
  HRESULT hr;
  int typeChangeCount = 0;
  const auto oldAudioRate = mAudioRate;
  while (true) {
    hr = mDecoder->Output(&sample);
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      return hr;
    }
    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
      hr = mDecoder->FindDecoderOutputType();
      NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
      hr = UpdateOutputType();
      NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
      // Catch infinite loops, but some decoders perform at least 2 stream
      // changes on consecutive calls, so be permissive.
      // 100 is arbitrarily > 2.
      NS_ENSURE_TRUE(typeChangeCount < 100, MF_E_TRANSFORM_STREAM_CHANGE);
      ++typeChangeCount;
      continue;
    }
    break;
  }

  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  if (!sample) {
    LOG("Audio MFTDecoder returned success but null output.");
    return E_FAIL;
  }

  UINT32 discontinuity = false;
  sample->GetUINT32(MFSampleExtension_Discontinuity, &discontinuity);
  if (mFirstFrame || discontinuity) {
    // Update the output type, in case this segment has a different
    // rate. This also triggers on the first sample, which can have a
    // different rate than is advertised in the container, and sometimes we
    // don't get a MF_E_TRANSFORM_STREAM_CHANGE when the rate changes.
    hr = UpdateOutputType();
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
    mFirstFrame = false;
  }

  TimeUnit pts = GetSampleTime(sample);
  NS_ENSURE_TRUE(pts.IsValid(), E_FAIL);

  RefPtr<IMFMediaBuffer> buffer;
  hr = sample->ConvertToContiguousBuffer(getter_AddRefs(buffer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  BYTE* data = nullptr;  // Note: *data will be owned by the IMFMediaBuffer, we
                         // don't need to free it.
  DWORD maxLength = 0, currentLength = 0;
  hr = buffer->Lock(&data, &maxLength, &currentLength);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Output is made of floats.
  int32_t numSamples = currentLength / sizeof(float);
  int32_t numFrames = numSamples / mAudioChannels;
  MOZ_ASSERT(numFrames >= 0);
  MOZ_ASSERT(numSamples >= 0);
  if (numFrames == 0) {
    // All data from this chunk stripped, loop back and try to output the next
    // frame, if possible.
    return S_OK;
  }

  if (oldAudioRate != mAudioRate) {
    LOG("Audio rate changed from %" PRIu32 " to %" PRIu32, oldAudioRate,
        mAudioRate);
  }

  AlignedAudioBuffer audioData(numSamples);
  if (!audioData) {
    return E_OUTOFMEMORY;
  }

  PodCopy(audioData.Data(), reinterpret_cast<float*>(data), numSamples);

  buffer->Unlock();

  TimeUnit duration = FramesToTimeUnit(numFrames, mAudioRate);
  NS_ENSURE_TRUE(duration.IsValid(), E_FAIL);

  const bool isAudioRateChangedToHigher = oldAudioRate < mAudioRate;
  if (IsPartialOutput(duration, isAudioRateChangedToHigher)) {
    LOG("Encounter a partial frame?! duration shrinks from %" PRId64
        " to %" PRId64,
        mLastOutputDuration.ToMicroseconds(), duration.ToMicroseconds());
    return MF_E_TRANSFORM_NEED_MORE_INPUT;
  }

  aOutData = new AudioData(aStreamOffset, pts, std::move(audioData),
                           mAudioChannels, mAudioRate, mChannelsMap);
  MOZ_DIAGNOSTIC_ASSERT(duration == aOutData->mDuration, "must be equal");
  mLastOutputDuration = aOutData->mDuration;

#ifdef LOG_SAMPLE_DECODE
  LOG("Decoded audio sample! timestamp=%lld duration=%lld currentLength=%u",
      pts.ToMicroseconds(), duration.ToMicroseconds(), currentLength);
#endif

  return S_OK;
}

bool WMFAudioMFTManager::IsPartialOutput(
    const media::TimeUnit& aNewOutputDuration,
    const bool aIsRateChangedToHigher) const {
  // This issue was found in Windows11, where AAC MFT decoder would incorrectly
  // output partial output samples to us, even if MS's documentation said it
  // won't happen [1]. More details are described in bug 1731430 comment 26.
  // If the audio rate isn't changed to higher, which would result in shorter
  // duration, but the new output duration is still shorter than the last one,
  // then new output is possible an incorrect partial output.
  // [1]
  // https://docs.microsoft.com/en-us/windows/win32/medfound/mft-message-command-drain
  if (mStreamType != WMFStreamType::AAC) {
    return false;
  }
  if (mLastOutputDuration > aNewOutputDuration && !aIsRateChangedToHigher) {
    return true;
  }
  return false;
}

void WMFAudioMFTManager::Shutdown() { mDecoder = nullptr; }

}  // namespace mozilla

#undef LOG
