/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Adts.h"
#include "AnnexB.h"
#include "BufferReader.h"
#include "DecoderData.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/Telemetry.h"
#include "VideoUtils.h"
#include "MP4Metadata.h"
#include "mozilla/Logging.h"

#include "mp4parse.h"

#define LOG(...) \
  MOZ_LOG(gMP4MetadataLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

using mozilla::media::TimeUnit;

namespace mozilla {

mozilla::Result<mozilla::Ok, nsresult> CryptoFile::DoUpdate(
    const uint8_t* aData, size_t aLength) {
  BufferReader reader(aData, aLength);
  while (reader.Remaining()) {
    PsshInfo psshInfo;
    if (!reader.ReadArray(psshInfo.uuid, 16)) {
      return mozilla::Err(NS_ERROR_FAILURE);
    }

    if (!reader.CanReadType<uint32_t>()) {
      return mozilla::Err(NS_ERROR_FAILURE);
    }
    auto length = reader.ReadType<uint32_t>();

    if (!reader.ReadArray(psshInfo.data, length)) {
      return mozilla::Err(NS_ERROR_FAILURE);
    }
    pssh.AppendElement(std::move(psshInfo));
  }
  return mozilla::Ok();
}

static MediaResult UpdateTrackProtectedInfo(mozilla::TrackInfo& aConfig,
                                            const Mp4parseSinfInfo& aSinf) {
  if (aSinf.is_encrypted != 0) {
    if (aSinf.scheme_type == MP4_PARSE_ENCRYPTION_SCHEME_TYPE_CENC) {
      aConfig.mCrypto.mCryptoScheme = CryptoScheme::Cenc;
    } else if (aSinf.scheme_type == MP4_PARSE_ENCRYPTION_SCHEME_TYPE_CBCS) {
      aConfig.mCrypto.mCryptoScheme = CryptoScheme::Cbcs;
    } else {
      // Unsupported encryption type;
      return MediaResult(
          NS_ERROR_DOM_MEDIA_METADATA_ERR,
          RESULT_DETAIL(
              "Unsupported encryption scheme encountered aSinf.scheme_type=%d",
              static_cast<int>(aSinf.scheme_type)));
    }
    aConfig.mCrypto.mIVSize = aSinf.iv_size;
    aConfig.mCrypto.mKeyId.AppendElements(aSinf.kid.data, aSinf.kid.length);
    aConfig.mCrypto.mCryptByteBlock = aSinf.crypt_byte_block;
    aConfig.mCrypto.mSkipByteBlock = aSinf.skip_byte_block;
    aConfig.mCrypto.mConstantIV.AppendElements(aSinf.constant_iv.data,
                                               aSinf.constant_iv.length);
  }
  return NS_OK;
}

// Verify various information shared by Mp4ParseTrackAudioInfo and
// Mp4ParseTrackVideoInfo and record telemetry on that info. Returns an
// appropriate MediaResult indicating if the info is valid or not.
// This verifies:
// - That we have a sample_info_count > 0 (valid tracks should have at least one
//   sample description entry)
// - That only a single codec is used across all sample infos, as we don't
//   handle multiple.
// - If more than one sample information structures contain crypto info. This
//   case is not fatal (we don't return an error), but does record telemetry
//   to help judge if we need more handling in gecko for multiple crypto.
//
// Telemetry is also recorded on the above. As of writing, the
// telemetry is recorded to give us early warning if MP4s exist that we're not
// handling. Note, if adding new checks and telemetry to this function,
// telemetry should be recorded before returning to ensure it is gathered.
template <typename Mp4ParseTrackAudioOrVideoInfo>
static MediaResult VerifyAudioOrVideoInfoAndRecordTelemetry(
    Mp4ParseTrackAudioOrVideoInfo* audioOrVideoInfo) {
  Telemetry::Accumulate(
      Telemetry::MEDIA_MP4_PARSE_NUM_SAMPLE_DESCRIPTION_ENTRIES,
      audioOrVideoInfo->sample_info_count);

  bool hasMultipleCodecs = false;
  uint32_t cryptoCount = 0;
  Mp4parseCodec codecType = audioOrVideoInfo->sample_info[0].codec_type;
  for (uint32_t i = 0; i < audioOrVideoInfo->sample_info_count; i++) {
    if (audioOrVideoInfo->sample_info[0].codec_type != codecType) {
      hasMultipleCodecs = true;
    }

    // Update our encryption info if any is present on the sample info.
    if (audioOrVideoInfo->sample_info[i].protected_data.is_encrypted) {
      cryptoCount += 1;
    }
  }

  Telemetry::Accumulate(
      Telemetry::
          MEDIA_MP4_PARSE_SAMPLE_DESCRIPTION_ENTRIES_HAVE_MULTIPLE_CODECS,
      hasMultipleCodecs);

  // Accumulate if we have multiple (2 or more) crypto entries.
  // TODO(1715283): rework this to count number of crypto entries + gather
  // richer data.
  Telemetry::Accumulate(
      Telemetry::
          MEDIA_MP4_PARSE_SAMPLE_DESCRIPTION_ENTRIES_HAVE_MULTIPLE_CRYPTO,
      cryptoCount >= 2);

  if (audioOrVideoInfo->sample_info_count == 0) {
    return MediaResult(
        NS_ERROR_DOM_MEDIA_METADATA_ERR,
        RESULT_DETAIL("Got 0 sample info while verifying track."));
  }

  if (hasMultipleCodecs) {
    // Different codecs in a single track. We don't handle this.
    return MediaResult(
        NS_ERROR_DOM_MEDIA_METADATA_ERR,
        RESULT_DETAIL("Multiple codecs encountered while verifying track."));
  }

  return NS_OK;
}

MediaResult MP4AudioInfo::Update(const Mp4parseTrackInfo* aTrack,
                                 const Mp4parseTrackAudioInfo* aAudio,
                                 const IndiceWrapper* aIndices) {
  auto rv = VerifyAudioOrVideoInfoAndRecordTelemetry(aAudio);
  NS_ENSURE_SUCCESS(rv, rv);

  Mp4parseCodec codecType = aAudio->sample_info[0].codec_type;
  for (uint32_t i = 0; i < aAudio->sample_info_count; i++) {
    if (aAudio->sample_info[i].protected_data.is_encrypted) {
      auto rv = UpdateTrackProtectedInfo(*this,
                                         aAudio->sample_info[i].protected_data);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    }
  }

  // We assume that the members of the first sample info are representative of
  // the entire track. This code will need to be updated should this assumption
  // ever not hold. E.g. if we need to handle different codecs in a single
  // track, or if we have different numbers or channels in a single track.
  Mp4parseByteData mp4ParseSampleCodecSpecific =
      aAudio->sample_info[0].codec_specific_config;
  Mp4parseByteData extraData = aAudio->sample_info[0].extra_data;
  MOZ_ASSERT(mCodecSpecificConfig.is<NoCodecSpecificData>(),
             "Should have no codec specific data yet");
  if (codecType == MP4PARSE_CODEC_OPUS) {
    mMimeType = "audio/opus"_ns;
    OpusCodecSpecificData opusCodecSpecificData{};
    // The Opus decoder expects the container's codec delay or
    // pre-skip value, in microseconds, as a 64-bit int at the
    // start of the codec-specific config blob.
    if (mp4ParseSampleCodecSpecific.data &&
        mp4ParseSampleCodecSpecific.length >= 12) {
      uint16_t preskip = mozilla::LittleEndian::readUint16(
          mp4ParseSampleCodecSpecific.data + 10);
      opusCodecSpecificData.mContainerCodecDelayFrames = preskip;
      LOG("Opus stream in MP4 container, %" PRId64
          " microseconds of encoder delay (%" PRIu16 ").",
          opusCodecSpecificData.mContainerCodecDelayFrames, preskip);
    } else {
      // This file will error later as it will be rejected by the opus decoder.
      opusCodecSpecificData.mContainerCodecDelayFrames = 0;
    }
    opusCodecSpecificData.mHeadersBinaryBlob->AppendElements(
        mp4ParseSampleCodecSpecific.data, mp4ParseSampleCodecSpecific.length);
    mCodecSpecificConfig =
        AudioCodecSpecificVariant{std::move(opusCodecSpecificData)};
  } else if (codecType == MP4PARSE_CODEC_AAC) {
    mMimeType = "audio/mp4a-latm"_ns;
    int64_t codecDelayUS = aTrack->media_time;
    double USECS_PER_S = 1e6;
    // We can't use mozilla::UsecsToFrames here because we need to round, and it
    // floors.
    uint32_t encoderDelayFrameCount = 0;
    if (codecDelayUS > 0) {
      encoderDelayFrameCount = static_cast<uint32_t>(
          std::lround(static_cast<double>(codecDelayUS) *
                      aAudio->sample_info->sample_rate / USECS_PER_S));
      LOG("AAC stream in MP4 container, %" PRIu32 " frames of encoder delay.",
          encoderDelayFrameCount);
    }

    uint64_t mediaFrameCount = 0;
    // Pass the padding number, in frames, to the AAC decoder as well.
    if (aIndices) {
      MP4SampleIndex::Indice firstIndice = {0};
      MP4SampleIndex::Indice lastIndice = {0};
      bool rv = aIndices->GetIndice(0, firstIndice);
      rv |= aIndices->GetIndice(aIndices->Length() - 1, lastIndice);
      if (rv) {
        if (firstIndice.start_composition > lastIndice.end_composition) {
          return MediaResult(
              NS_ERROR_DOM_MEDIA_METADATA_ERR,
              RESULT_DETAIL("Inconsistent start and end time in index"));
        }
        // The `end_composition` member of the very last index member is the
        // duration of the media in microseconds, excluding decoder delay and
        // padding. Convert to frames and give to the decoder so that trimming
        // can be done properly.
        mediaFrameCount =
            lastIndice.end_composition - firstIndice.start_composition;
        LOG("AAC stream in MP4 container, total media duration is %" PRIu64
            " frames",
            mediaFrameCount);
      } else {
        LOG("AAC stream in MP4 container, couldn't determine total media time");
      }
    }

    AacCodecSpecificData aacCodecSpecificData{};

    aacCodecSpecificData.mEncoderDelayFrames = encoderDelayFrameCount;
    aacCodecSpecificData.mMediaFrameCount = mediaFrameCount;

    // codec specific data is used to store the DecoderConfigDescriptor.
    aacCodecSpecificData.mDecoderConfigDescriptorBinaryBlob->AppendElements(
        mp4ParseSampleCodecSpecific.data, mp4ParseSampleCodecSpecific.length);
    // extra data stores the ES_Descriptor.
    aacCodecSpecificData.mEsDescriptorBinaryBlob->AppendElements(
        extraData.data, extraData.length);
    mCodecSpecificConfig =
        AudioCodecSpecificVariant{std::move(aacCodecSpecificData)};
  } else if (codecType == MP4PARSE_CODEC_FLAC) {
    MOZ_ASSERT(extraData.length == 0,
               "FLAC doesn't expect extra data so doesn't handle it!");
    mMimeType = "audio/flac"_ns;
    FlacCodecSpecificData flacCodecSpecificData{};
    flacCodecSpecificData.mStreamInfoBinaryBlob->AppendElements(
        mp4ParseSampleCodecSpecific.data, mp4ParseSampleCodecSpecific.length);
    mCodecSpecificConfig =
        AudioCodecSpecificVariant{std::move(flacCodecSpecificData)};
  } else if (codecType == MP4PARSE_CODEC_MP3) {
    // mp3 in mp4 can contain ES_Descriptor info (it also has a flash in mp4
    // specific box, which the rust parser recognizes). However, we don't
    // handle any such data here.
    mMimeType = "audio/mpeg"_ns;
    // TODO(bug 1705812): parse the encoder delay values from the mp4.
    mCodecSpecificConfig = AudioCodecSpecificVariant{Mp3CodecSpecificData{}};
  }

  mRate = aAudio->sample_info[0].sample_rate;
  mChannels = aAudio->sample_info[0].channels;
  mBitDepth = aAudio->sample_info[0].bit_depth;
  mExtendedProfile =
      AssertedCast<int8_t>(aAudio->sample_info[0].extended_profile);
  if (aTrack->duration > TimeUnit::MaxTicks()) {
    mDuration = TimeUnit::FromInfinity();
  } else {
    mDuration =
        TimeUnit(AssertedCast<int64_t>(aTrack->duration), aTrack->time_scale);
  }
  mMediaTime = TimeUnit(aTrack->media_time, aTrack->time_scale);
  mTrackId = aTrack->track_id;

  // In stagefright, mProfile is kKeyAACProfile, mExtendedProfile is kKeyAACAOT.
  if (aAudio->sample_info[0].profile <= 4) {
    mProfile = AssertedCast<int8_t>(aAudio->sample_info[0].profile);
  }

  if (mCodecSpecificConfig.is<NoCodecSpecificData>()) {
    // Handle codecs that are not explicitly handled above.
    MOZ_ASSERT(
        extraData.length == 0,
        "Codecs that use extra data should be explicitly handled already");
    AudioCodecSpecificBinaryBlob codecSpecificBinaryBlob;
    // No codec specific metadata set, use the generic form.
    codecSpecificBinaryBlob.mBinaryBlob->AppendElements(
        mp4ParseSampleCodecSpecific.data, mp4ParseSampleCodecSpecific.length);
    mCodecSpecificConfig =
        AudioCodecSpecificVariant{std::move(codecSpecificBinaryBlob)};
  }

  return NS_OK;
}

bool MP4AudioInfo::IsValid() const {
  return mChannels > 0 && mRate > 0 &&
         // Accept any mime type here, but if it's aac, validate the profile.
         (!mMimeType.EqualsLiteral("audio/mp4a-latm") || mProfile > 0 ||
          mExtendedProfile > 0);
}

MediaResult MP4VideoInfo::Update(const Mp4parseTrackInfo* track,
                                 const Mp4parseTrackVideoInfo* video) {
  auto rv = VerifyAudioOrVideoInfoAndRecordTelemetry(video);
  NS_ENSURE_SUCCESS(rv, rv);

  Mp4parseCodec codecType = video->sample_info[0].codec_type;
  for (uint32_t i = 0; i < video->sample_info_count; i++) {
    if (video->sample_info[i].protected_data.is_encrypted) {
      auto rv =
          UpdateTrackProtectedInfo(*this, video->sample_info[i].protected_data);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    }
  }

  // We assume that the members of the first sample info are representative of
  // the entire track. This code will need to be updated should this assumption
  // ever not hold. E.g. if we need to handle different codecs in a single
  // track, or if we have different numbers or channels in a single track.
  if (codecType == MP4PARSE_CODEC_AVC) {
    mMimeType = "video/avc"_ns;
  } else if (codecType == MP4PARSE_CODEC_VP9) {
    mMimeType = "video/vp9"_ns;
  } else if (codecType == MP4PARSE_CODEC_AV1) {
    mMimeType = "video/av1"_ns;
  } else if (codecType == MP4PARSE_CODEC_MP4V) {
    mMimeType = "video/mp4v-es"_ns;
  } else if (codecType == MP4PARSE_CODEC_HEVC) {
    mMimeType = "video/hevc"_ns;
  }
  mTrackId = track->track_id;
  if (track->duration > TimeUnit::MaxTicks()) {
    mDuration = TimeUnit::FromInfinity();
  } else {
    mDuration =
        TimeUnit(AssertedCast<int64_t>(track->duration), track->time_scale);
  }
  mMediaTime = TimeUnit(track->media_time, track->time_scale);
  mDisplay.width = AssertedCast<int32_t>(video->display_width);
  mDisplay.height = AssertedCast<int32_t>(video->display_height);
  mImage.width = video->sample_info[0].image_width;
  mImage.height = video->sample_info[0].image_height;
  mRotation = ToSupportedRotation(video->rotation);
  Mp4parseByteData extraData = video->sample_info[0].extra_data;
  // If length is 0 we append nothing
  mExtraData->AppendElements(extraData.data, extraData.length);
  return NS_OK;
}

bool MP4VideoInfo::IsValid() const {
  return (mDisplay.width > 0 && mDisplay.height > 0) ||
         (mImage.width > 0 && mImage.height > 0);
}

}  // namespace mozilla

#undef LOG
