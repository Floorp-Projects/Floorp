/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaEngineAudioStream.h"

#include "MFMediaEngineUtils.h"

namespace mozilla {

#define LOGV(msg, ...)                          \
  MOZ_LOG(gMFMediaEngineLog, LogLevel::Verbose, \
          ("MFMediaStream=%p (%s), " msg, this, \
           this->GetDescriptionName().get(), ##__VA_ARGS__))

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

/* static */
MFMediaEngineAudioStream* MFMediaEngineAudioStream::Create(
    uint64_t aStreamId, const TrackInfo& aInfo, MFMediaSource* aParentSource) {
  MOZ_ASSERT(aInfo.IsAudio());
  MFMediaEngineAudioStream* stream;
  if (FAILED(MakeAndInitialize<MFMediaEngineAudioStream>(
          &stream, aStreamId, aInfo, aParentSource))) {
    return nullptr;
  }
  return stream;
}

HRESULT MFMediaEngineAudioStream::CreateMediaType(const TrackInfo& aInfo,
                                                  IMFMediaType** aMediaType) {
  const AudioInfo& info = *aInfo.GetAsAudioInfo();
  GUID subType = AudioMimeTypeToMediaFoundationSubtype(info.mMimeType);
  NS_ENSURE_TRUE(subType != GUID_NULL, MF_E_TOPO_CODEC_NOT_FOUND);

  // https://docs.microsoft.com/en-us/windows/win32/medfound/media-type-attributes
  ComPtr<IMFMediaType> mediaType;
  RETURN_IF_FAILED(wmf::MFCreateMediaType(&mediaType));
  RETURN_IF_FAILED(mediaType->SetGUID(MF_MT_SUBTYPE, subType));
  RETURN_IF_FAILED(mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
  RETURN_IF_FAILED(
      mediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, info.mChannels));
  RETURN_IF_FAILED(
      mediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, info.mRate));
  uint64_t bitDepth = info.mBitDepth != 0 ? info.mBitDepth : 16;
  RETURN_IF_FAILED(mediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bitDepth));
  if (subType == MFAudioFormat_AAC) {
    if (mAACUserData.IsEmpty()) {
      MOZ_ASSERT(info.mCodecSpecificConfig.is<AacCodecSpecificData>());
      const auto& blob = info.mCodecSpecificConfig.as<AacCodecSpecificData>()
                             .mDecoderConfigDescriptorBinaryBlob;
      AACAudioSpecificConfigToUserData(info.mExtendedProfile, blob->Elements(),
                                       blob->Length(), mAACUserData);
      LOGV("Generated AAC user data");
    }
    RETURN_IF_FAILED(
        mediaType->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0x0));  // Raw AAC packet
    RETURN_IF_FAILED(mediaType->SetBlob(
        MF_MT_USER_DATA, mAACUserData.Elements(), mAACUserData.Length()));
  }
  LOGV("Created audio type, subtype=%s, channel=%" PRIu32 ", rate=%" PRIu32
       ", bitDepth=%" PRIu64,
       GUIDToStr(subType), info.mChannels, info.mRate, bitDepth);

  *aMediaType = mediaType.Detach();
  return S_OK;
}

bool MFMediaEngineAudioStream::HasEnoughRawData() const {
  // If more than this much raw audio is queued, we'll hold off request more
  // audio.
  static const int64_t AMPLE_AUDIO_USECS = 2000000;
  return mRawDataQueue.Duration() >= AMPLE_AUDIO_USECS;
}

#undef LOGV

}  // namespace mozilla
