/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFAudioMFTManager.h"
#include "MediaInfo.h"
#include "VideoUtils.h"
#include "WMFUtils.h"
#include "nsTArray.h"
#include "TimeUnits.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Logging.h"

#define LOG(...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {

static void
AACAudioSpecificConfigToUserData(uint8_t aAACProfileLevelIndication,
                                 const uint8_t* aAudioSpecConfig,
                                 uint32_t aConfigLength,
                                 nsTArray<BYTE>& aOutUserData)
{
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
  w[0] = 0x0; // Payload type raw AAC packet
  w[1] = aAACProfileLevelIndication;

  aOutUserData.AppendElements(heeInfo, heeInfoLen);

  if (aAACProfileLevelIndication == 2 && aConfigLength > 2) {
    // The AudioSpecificConfig is TTTTTFFF|FCCCCGGG
    // (T=ObjectType, F=Frequency, C=Channel, G=GASpecificConfig)
    // If frequency = 0xf, then the frequency is explicitly defined on 24 bits.
    int8_t profile = (aAudioSpecConfig[0] & 0xF8) >> 3;
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

WMFAudioMFTManager::WMFAudioMFTManager(
  const AudioInfo& aConfig)
  : mAudioChannels(aConfig.mChannels)
  , mAudioRate(aConfig.mRate)
  , mAudioFrameSum(0)
  , mMustRecaptureAudioPosition(true)
{
  MOZ_COUNT_CTOR(WMFAudioMFTManager);

  if (aConfig.mMimeType.EqualsLiteral("audio/mpeg")) {
    mStreamType = MP3;
  } else if (aConfig.mMimeType.EqualsLiteral("audio/mp4a-latm")) {
    mStreamType = AAC;
    AACAudioSpecificConfigToUserData(aConfig.mExtendedProfile,
                                     aConfig.mCodecSpecificConfig->Elements(),
                                     aConfig.mCodecSpecificConfig->Length(),
                                     mUserData);
  } else {
    mStreamType = Unknown;
  }
}

WMFAudioMFTManager::~WMFAudioMFTManager()
{
  MOZ_COUNT_DTOR(WMFAudioMFTManager);
}

const GUID&
WMFAudioMFTManager::GetMFTGUID()
{
  MOZ_ASSERT(mStreamType != Unknown);
  switch (mStreamType) {
    case AAC: return CLSID_CMSAACDecMFT;
    case MP3: return CLSID_CMP3DecMediaObject;
    default: return GUID_NULL;
  };
}

const GUID&
WMFAudioMFTManager::GetMediaSubtypeGUID()
{
  MOZ_ASSERT(mStreamType != Unknown);
  switch (mStreamType) {
    case AAC: return MFAudioFormat_AAC;
    case MP3: return MFAudioFormat_MP3;
    default: return GUID_NULL;
  };
}

bool
WMFAudioMFTManager::Init()
{
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
    hr = inputType->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0x0); // Raw AAC packet
    NS_ENSURE_TRUE(SUCCEEDED(hr), false);

    hr = inputType->SetBlob(MF_MT_USER_DATA,
                            mUserData.Elements(),
                            mUserData.Length());
    NS_ENSURE_TRUE(SUCCEEDED(hr), false);
  }

  RefPtr<IMFMediaType> outputType;
  hr = wmf::MFCreateMediaType(getter_AddRefs(outputType));
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = outputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = outputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = decoder->SetMediaTypes(inputType, outputType);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  mDecoder = decoder;

  return true;
}

HRESULT
WMFAudioMFTManager::Input(MediaRawData* aSample)
{
  return mDecoder->Input(aSample->Data(),
                         uint32_t(aSample->Size()),
                         aSample->mTime);
}

HRESULT
WMFAudioMFTManager::UpdateOutputType()
{
  HRESULT hr;

  RefPtr<IMFMediaType> type;
  hr = mDecoder->GetOutputMediaType(type);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &mAudioRate);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &mAudioChannels);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  AudioConfig::ChannelLayout layout(mAudioChannels);
  if (!layout.IsValid()) {
    return E_FAIL;
  }

  return S_OK;
}

HRESULT
WMFAudioMFTManager::Output(int64_t aStreamOffset,
                           RefPtr<MediaData>& aOutData)
{
  aOutData = nullptr;
  RefPtr<IMFSample> sample;
  HRESULT hr;
  int typeChangeCount = 0;
  while (true) {
    hr = mDecoder->Output(&sample);
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      return hr;
    }
    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
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
    nsCOMPtr<nsIRunnable> task = NS_NewRunnableFunction([]() -> void {
      LOG("Reporting telemetry AUDIO_MFT_OUTPUT_NULL_SAMPLES");
      Telemetry::Accumulate(Telemetry::ID::AUDIO_MFT_OUTPUT_NULL_SAMPLES, 1);
    });
    AbstractThread::MainThread()->Dispatch(task.forget());
    return E_FAIL;
  }

  RefPtr<IMFMediaBuffer> buffer;
  hr = sample->ConvertToContiguousBuffer(getter_AddRefs(buffer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  BYTE* data = nullptr; // Note: *data will be owned by the IMFMediaBuffer, we don't need to free it.
  DWORD maxLength = 0, currentLength = 0;
  hr = buffer->Lock(&data, &maxLength, &currentLength);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Sometimes when starting decoding, the AAC decoder gives us samples
  // with a negative timestamp. AAC does usually have preroll (or encoder
  // delay) encoded into its bitstream, but the amount encoded to the stream
  // is variable, and it not signalled in-bitstream. There is sometimes
  // signalling in the MP4 container what the preroll amount, but it's
  // inconsistent. It looks like WMF's AAC encoder may take this into
  // account, so strip off samples with a negative timestamp to get us
  // to a 0-timestamp start. This seems to maintain A/V sync, so we can run
  // with this until someone complains...

  // We calculate the timestamp and the duration based on the number of audio
  // frames we've already played. We don't trust the timestamp stored on the
  // IMFSample, as sometimes it's wrong, possibly due to buggy encoders?

  // If this sample block comes after a discontinuity (i.e. a gap or seek)
  // reset the frame counters, and capture the timestamp. Future timestamps
  // will be offset from this block's timestamp.
  UINT32 discontinuity = false;
  sample->GetUINT32(MFSampleExtension_Discontinuity, &discontinuity);
  if (mMustRecaptureAudioPosition || discontinuity) {
    // Update the output type, in case this segment has a different
    // rate. This also triggers on the first sample, which can have a
    // different rate than is advertised in the container, and sometimes we
    // don't get a MF_E_TRANSFORM_STREAM_CHANGE when the rate changes.
    hr = UpdateOutputType();
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

    mAudioFrameSum = 0;
    LONGLONG timestampHns = 0;
    hr = sample->GetSampleTime(&timestampHns);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
    mAudioTimeOffset = media::TimeUnit::FromMicroseconds(timestampHns / 10);
    mMustRecaptureAudioPosition = false;
  }
  // We can assume PCM 16 output.
  int32_t numSamples = currentLength / 2;
  int32_t numFrames = numSamples / mAudioChannels;
  MOZ_ASSERT(numFrames >= 0);
  MOZ_ASSERT(numSamples >= 0);
  if (numFrames == 0) {
    // All data from this chunk stripped, loop back and try to output the next
    // frame, if possible.
    return S_OK;
  }

  AlignedAudioBuffer audioData(numSamples);
  if (!audioData) {
    return E_OUTOFMEMORY;
  }

  int16_t* pcm = (int16_t*)data;
  for (int32_t i = 0; i < numSamples; ++i) {
    audioData[i] = AudioSampleToFloat(pcm[i]);
  }

  buffer->Unlock();

  media::TimeUnit timestamp =
    mAudioTimeOffset + FramesToTimeUnit(mAudioFrameSum, mAudioRate);
  NS_ENSURE_TRUE(timestamp.IsValid(), E_FAIL);

  mAudioFrameSum += numFrames;

  media::TimeUnit duration = FramesToTimeUnit(numFrames, mAudioRate);
  NS_ENSURE_TRUE(duration.IsValid(), E_FAIL);

  aOutData = new AudioData(aStreamOffset,
                           timestamp.ToMicroseconds(),
                           duration.ToMicroseconds(),
                           numFrames,
                           Move(audioData),
                           mAudioChannels,
                           mAudioRate);

  #ifdef LOG_SAMPLE_DECODE
  LOG("Decoded audio sample! timestamp=%lld duration=%lld currentLength=%u",
      timestamp.ToMicroseconds(), duration.ToMicroseconds(), currentLength);
  #endif

  return S_OK;
}

void
WMFAudioMFTManager::Shutdown()
{
  mDecoder = nullptr;
}

} // namespace mozilla
