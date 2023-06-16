/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaEngineAudioStream.h"

#include <mferror.h>
#include <mfapi.h>

#include "MFMediaEngineUtils.h"
#include "WMFUtils.h"
#include "mozilla/StaticPrefs_media.h"

namespace mozilla {

#define LOG(msg, ...)                           \
  MOZ_LOG(gMFMediaEngineLog, LogLevel::Debug,   \
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
  mAudioInfo = info;
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
      MOZ_ASSERT(info.mCodecSpecificConfig.is<AacCodecSpecificData>() ||
                 info.mCodecSpecificConfig.is<AudioCodecSpecificBinaryBlob>());
      RefPtr<MediaByteBuffer> blob;
      if (info.mCodecSpecificConfig.is<AacCodecSpecificData>()) {
        blob = info.mCodecSpecificConfig.as<AacCodecSpecificData>()
                   .mDecoderConfigDescriptorBinaryBlob;
      } else {
        blob = info.mCodecSpecificConfig.as<AudioCodecSpecificBinaryBlob>()
                   .mBinaryBlob;
      }
      AACAudioSpecificConfigToUserData(info.mExtendedProfile, blob->Elements(),
                                       blob->Length(), mAACUserData);
      LOG("Generated AAC user data");
    }
    RETURN_IF_FAILED(
        mediaType->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0x0));  // Raw AAC packet
    RETURN_IF_FAILED(mediaType->SetBlob(
        MF_MT_USER_DATA, mAACUserData.Elements(), mAACUserData.Length()));
  }
  LOG("Created audio type, subtype=%s, channel=%" PRIu32 ", rate=%" PRIu32
      ", bitDepth=%" PRIu64 ", encrypted=%d",
      GUIDToStr(subType), info.mChannels, info.mRate, bitDepth,
      mAudioInfo.mCrypto.IsEncrypted());

  if (IsEncrypted()) {
    ComPtr<IMFMediaType> protectedMediaType;
    RETURN_IF_FAILED(wmf::MFWrapMediaType(mediaType.Get(),
                                          MFMediaType_Protected, subType,
                                          protectedMediaType.GetAddressOf()));
    LOG("Wrap MFMediaType_Audio into MFMediaType_Protected");
    *aMediaType = protectedMediaType.Detach();
  } else {
    *aMediaType = mediaType.Detach();
  }
  return S_OK;
}

bool MFMediaEngineAudioStream::HasEnoughRawData() const {
  // If more than this much raw audio is queued, we'll hold off request more
  // audio.
  return mRawDataQueueForFeedingEngine.Duration() >=
         StaticPrefs::media_wmf_media_engine_raw_data_threshold_audio();
}

already_AddRefed<MediaData> MFMediaEngineAudioStream::OutputDataInternal() {
  AssertOnTaskQueue();
  if (mRawDataQueueForGeneratingOutput.GetSize() == 0) {
    return nullptr;
  }
  // The media engine doesn't provide a way to allow us to access decoded audio
  // frames, and the audio playback will be handled internally inside the media
  // engine. So we simply return fake audio data.
  RefPtr<MediaRawData> input = mRawDataQueueForGeneratingOutput.PopFront();
  RefPtr<MediaData> output =
      new AudioData(input->mOffset, input->mTime, AlignedAudioBuffer{},
                    mAudioInfo.mChannels, mAudioInfo.mRate);
  return output.forget();
}

nsCString MFMediaEngineAudioStream::GetCodecName() const {
  WMFStreamType type = GetStreamTypeFromMimeType(mAudioInfo.mMimeType);
  switch (type) {
    case WMFStreamType::MP3:
      return "mp3"_ns;
    case WMFStreamType::AAC:
      return "aac"_ns;
    case WMFStreamType::OPUS:
      return "opus"_ns;
    case WMFStreamType::VORBIS:
      return "vorbis"_ns;
    default:
      return "unknown"_ns;
  }
}

bool MFMediaEngineAudioStream::IsEncrypted() const {
  return mAudioInfo.mCrypto.IsEncrypted();
}

#undef LOG

}  // namespace mozilla
