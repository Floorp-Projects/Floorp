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

static void AACAudioSpecificConfigToUserData(uint8_t aAACProfileLevelIndication,
                                             const uint8_t* aAudioSpecConfig,
                                             uint32_t aConfigLength,
                                             nsTArray<BYTE>& aOutUserData) {
  MOZ_ASSERT(aOutUserData.IsEmpty());

  // The MF_MT_USER_DATA for AAC is defined here:
  // http://msdn.microsoft.com/en-us/library/windows/desktop/dd742784%28v=vs.85%29.aspx
  //
  // For MFAudioFormat_AAC, MF_MT_USER_DATA contains the portion of
  // the HEAACWAVEINFO structure that appears after the WAVEFORMATEX
  // structure (that is, after the wfx member). This is followed by
  // the AudioSpecificConfig() data, as defined by ISO/IEC 14496-3.
  // [...]
  // The length of the AudioSpecificConfig() data is 2 bytes for AAC-LC
  // or HE-AAC with implicit signaling of SBR/PS. It is more than 2 bytes
  // for HE-AAC with explicit signaling of SBR/PS.
  //
  // The value of audioObjectType as defined in AudioSpecificConfig()
  // must be 2, indicating AAC-LC. The value of extensionAudioObjectType
  // must be 5 for SBR or 29 for PS.
  //
  // HEAACWAVEINFO structure:
  //    typedef struct heaacwaveinfo_tag {
  //      WAVEFORMATEX wfx;
  //      WORD         wPayloadType;
  //      WORD         wAudioProfileLevelIndication;
  //      WORD         wStructType;
  //      WORD         wReserved1;
  //      DWORD        dwReserved2;
  //    }
  const UINT32 heeInfoLen = 4 * sizeof(WORD) + sizeof(DWORD);

  // The HEAACWAVEINFO must have payload and profile set,
  // the rest can be all 0x00.
  BYTE heeInfo[heeInfoLen] = {0};
  WORD* w = (WORD*)heeInfo;
  w[0] = 0x0;  // Payload type raw AAC packet
  w[1] = aAACProfileLevelIndication;

  aOutUserData.AppendElements(heeInfo, heeInfoLen);

  if (aAACProfileLevelIndication == 2 && aConfigLength > 2) {
    // The AudioSpecificConfig is TTTTTFFF|FCCCCGGG
    // (T=ObjectType, F=Frequency, C=Channel, G=GASpecificConfig)
    // If frequency = 0xf, then the frequency is explicitly defined on 24 bits.
    int8_t frequency =
        (aAudioSpecConfig[0] & 0x7) << 1 | (aAudioSpecConfig[1] & 0x80) >> 7;
    int8_t channels = (aAudioSpecConfig[1] & 0x78) >> 3;
    int8_t gasc = aAudioSpecConfig[1] & 0x7;
    if (frequency != 0xf && channels && !gasc) {
      // We enter this condition if the AudioSpecificConfig should theorically
      // be 2 bytes long but it's not.
      // The WMF AAC decoder will error if unknown extensions are found,
      // so remove them.
      aConfigLength = 2;
    }
  }
  aOutUserData.AppendElements(aAudioSpecConfig, aConfigLength);
}

WMFAudioMFTManager::WMFAudioMFTManager(const AudioInfo& aConfig)
    : mAudioChannels(aConfig.mChannels),
      mChannelsMap(AudioConfig::ChannelLayout::UNKNOWN_MAP),
      mAudioRate(aConfig.mRate) {
  MOZ_COUNT_CTOR(WMFAudioMFTManager);

  if (aConfig.mMimeType.EqualsLiteral("audio/mpeg")) {
    mStreamType = MP3;
  } else if (aConfig.mMimeType.EqualsLiteral("audio/mp4a-latm")) {
    mStreamType = AAC;
    AACAudioSpecificConfigToUserData(
        aConfig.mExtendedProfile, aConfig.mCodecSpecificConfig->Elements(),
        aConfig.mCodecSpecificConfig->Length(), mUserData);
  } else {
    mStreamType = Unknown;
  }
}

WMFAudioMFTManager::~WMFAudioMFTManager() {
  MOZ_COUNT_DTOR(WMFAudioMFTManager);
}

const GUID& WMFAudioMFTManager::GetMFTGUID() {
  MOZ_ASSERT(mStreamType != Unknown);
  switch (mStreamType) {
    case AAC:
      return CLSID_CMSAACDecMFT;
    case MP3:
      return CLSID_CMP3DecMediaObject;
    default:
      return GUID_NULL;
  };
}

const GUID& WMFAudioMFTManager::GetMediaSubtypeGUID() {
  MOZ_ASSERT(mStreamType != Unknown);
  switch (mStreamType) {
    case AAC:
      return MFAudioFormat_AAC;
    case MP3:
      return MFAudioFormat_MP3;
    default:
      return GUID_NULL;
  };
}

bool WMFAudioMFTManager::Init() {
  NS_ENSURE_TRUE(mStreamType != Unknown, false);

  RefPtr<MFTDecoder> decoder(new MFTDecoder());

  HRESULT hr = decoder->Create(GetMFTGUID());
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

  if (mStreamType == AAC) {
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
  if (mStreamType != AAC) {
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
