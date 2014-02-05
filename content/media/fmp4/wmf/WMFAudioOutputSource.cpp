/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFAudioOutputSource.h"
#include "VideoUtils.h"
#include "WMFUtils.h"
#include "nsTArray.h"

#include "prlog.h"

#ifdef PR_LOGGING
PRLogModuleInfo* GetDemuxerLog();
#define LOG(...) PR_LOG(GetDemuxerLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define LOG(...)
#endif

namespace mozilla {

static void
AACAudioSpecificConfigToUserData(const uint8_t* aAudioSpecConfig,
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
  w[0] = 0x1; // Payload type ADTS
  w[1] = 0xFE; // Profile level indication, none specified.

  aOutUserData.AppendElements(heeInfo, heeInfoLen);
  aOutUserData.AppendElements(aAudioSpecConfig, aConfigLength);
}

WMFAudioOutputSource::WMFAudioOutputSource(const mp4_demuxer::AudioDecoderConfig& aConfig)
  : mAudioChannels(ChannelLayoutToChannelCount(aConfig.channel_layout()))
  , mAudioBytesPerSample(aConfig.bits_per_channel() / 8)
  , mAudioRate(aConfig.samples_per_second())
  , mAudioFrameOffset(0)
  , mAudioFrameSum(0)
  , mMustRecaptureAudioPosition(true)
{
  MOZ_COUNT_CTOR(WMFAudioOutputSource);
  AACAudioSpecificConfigToUserData(aConfig.extra_data(),
                                   aConfig.extra_data_size(),
                                   mUserData);
}

WMFAudioOutputSource::~WMFAudioOutputSource()
{
  MOZ_COUNT_DTOR(WMFAudioOutputSource);
}

TemporaryRef<MFTDecoder>
WMFAudioOutputSource::Init()
{
  RefPtr<MFTDecoder> decoder(new MFTDecoder());

  HRESULT hr = decoder->Create(CLSID_CMSAACDecMFT);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  // Setup input/output media types
  RefPtr<IMFMediaType> type;

  hr = wmf::MFCreateMediaType(byRef(type));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  hr = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  hr = type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  hr = type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, mAudioRate);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  hr = type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, mAudioChannels);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  hr = type->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0x1); // ADTS
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  hr = type->SetBlob(MF_MT_USER_DATA,
                     mUserData.Elements(),
                     mUserData.Length());
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  hr = decoder->SetMediaTypes(type, MFAudioFormat_PCM);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  mDecoder = decoder;

  return decoder.forget();
}

HRESULT
WMFAudioOutputSource::Output(int64_t aStreamOffset,
                        nsAutoPtr<MediaData>& aOutData)
{
  aOutData = nullptr;
  RefPtr<IMFSample> sample;
  HRESULT hr = mDecoder->Output(&sample);
  if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
    return MF_E_TRANSFORM_NEED_MORE_INPUT;
  }
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IMFMediaBuffer> buffer;
  hr = sample->ConvertToContiguousBuffer(byRef(buffer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  BYTE* data = nullptr; // Note: *data will be owned by the IMFMediaBuffer, we don't need to free it.
  DWORD maxLength = 0, currentLength = 0;
  hr = buffer->Lock(&data, &maxLength, &currentLength);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  int32_t numSamples = currentLength / mAudioBytesPerSample;
  int32_t numFrames = numSamples / mAudioChannels;

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
  int32_t numFramesToStrip = 0;
  sample->GetUINT32(MFSampleExtension_Discontinuity, &discontinuity);
  if (mMustRecaptureAudioPosition || discontinuity) {
    mAudioFrameSum = 0;
    LONGLONG timestampHns = 0;
    hr = sample->GetSampleTime(&timestampHns);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
    hr = HNsToFrames(timestampHns, mAudioRate, &mAudioFrameOffset);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
    if (mAudioFrameOffset < 0) {
      // First sample has a negative timestamp. Strip off the samples until
      // we reach positive territory.
      numFramesToStrip = -mAudioFrameOffset;
      mAudioFrameOffset = 0;
    }
    mMustRecaptureAudioPosition = false;
  }
  MOZ_ASSERT(numFramesToStrip >= 0);
  int32_t offset = std::min<int32_t>(numFramesToStrip, numFrames);
  numFrames -= offset;
  numSamples -= offset * mAudioChannels;
  MOZ_ASSERT(numFrames >= 0);
  MOZ_ASSERT(numSamples >= 0);
  if (numFrames == 0) {
    // All data from this chunk stripped, loop back and try to output the next
    // frame, if possible.
    return S_OK;
  }

  nsAutoArrayPtr<AudioDataValue> audioData(new AudioDataValue[numSamples]);

  // Just assume PCM output for now...
  MOZ_ASSERT(mAudioBytesPerSample == 2);
  int16_t* pcm = ((int16_t*)data) + (offset * mAudioChannels);
  MOZ_ASSERT(pcm >= (int16_t*)data);
  MOZ_ASSERT(pcm <= (int16_t*)(data + currentLength));
  MOZ_ASSERT(pcm+numSamples <= (int16_t*)(data + currentLength));
  for (int32_t i = 0; i < numSamples; ++i) {
    audioData[i] = AudioSampleToFloat(pcm[i]);
  }

  buffer->Unlock();
  int64_t timestamp;
  hr = FramesToUsecs(mAudioFrameOffset + mAudioFrameSum, mAudioRate, &timestamp);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  mAudioFrameSum += numFrames;

  int64_t duration;
  hr = FramesToUsecs(numFrames, mAudioRate, &duration);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  aOutData = new AudioData(aStreamOffset,
                           timestamp,
                           duration,
                           numFrames,
                           audioData.forget(),
                           mAudioChannels);

  #ifdef LOG_SAMPLE_DECODE
  LOG("Decoded audio sample! timestamp=%lld duration=%lld currentLength=%u",
      timestamp, duration, currentLength);
  #endif

  return S_OK;
}

} // namespace mozilla
