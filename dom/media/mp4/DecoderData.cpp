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

// OpusDecoder header is really needed only by MP4 in rust
#include "OpusDecoder.h"
#include "mp4parse.h"

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

MediaResult MP4AudioInfo::Update(const Mp4parseTrackInfo* track,
                                 const Mp4parseTrackAudioInfo* audio) {
  auto rv = VerifyAudioOrVideoInfoAndRecordTelemetry(audio);
  NS_ENSURE_SUCCESS(rv, rv);

  Mp4parseCodec codecType = audio->sample_info[0].codec_type;
  for (uint32_t i = 0; i < audio->sample_info_count; i++) {
    if (audio->sample_info[i].protected_data.is_encrypted) {
      auto rv =
          UpdateTrackProtectedInfo(*this, audio->sample_info[i].protected_data);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    }
  }

  // We assume that the members of the first sample info are representative of
  // the entire track. This code will need to be updated should this assumption
  // ever not hold. E.g. if we need to handle different codecs in a single
  // track, or if we have different numbers or channels in a single track.
  Mp4parseByteData codecSpecificConfig =
      audio->sample_info[0].codec_specific_config;
  if (codecType == MP4PARSE_CODEC_OPUS) {
    mMimeType = "audio/opus"_ns;
    // The Opus decoder expects the container's codec delay or
    // pre-skip value, in microseconds, as a 64-bit int at the
    // start of the codec-specific config blob.
    if (codecSpecificConfig.data && codecSpecificConfig.length >= 12) {
      uint16_t preskip =
          mozilla::LittleEndian::readUint16(codecSpecificConfig.data + 10);
      mozilla::OpusDataDecoder::AppendCodecDelay(
          mCodecSpecificConfig, mozilla::FramesToUsecs(preskip, 48000).value());
    } else {
      // This file will error later as it will be rejected by the opus decoder.
      mozilla::OpusDataDecoder::AppendCodecDelay(mCodecSpecificConfig, 0);
    }
  } else if (codecType == MP4PARSE_CODEC_AAC) {
    mMimeType = "audio/mp4a-latm"_ns;
  } else if (codecType == MP4PARSE_CODEC_FLAC) {
    mMimeType = "audio/flac"_ns;
  } else if (codecType == MP4PARSE_CODEC_MP3) {
    mMimeType = "audio/mpeg"_ns;
  }

  mRate = audio->sample_info[0].sample_rate;
  mChannels = audio->sample_info[0].channels;
  mBitDepth = audio->sample_info[0].bit_depth;
  mExtendedProfile = audio->sample_info[0].extended_profile;
  mDuration = TimeUnit::FromMicroseconds(track->duration);
  mMediaTime = TimeUnit::FromMicroseconds(track->media_time);
  mTrackId = track->track_id;

  // In stagefright, mProfile is kKeyAACProfile, mExtendedProfile is kKeyAACAOT.
  if (audio->sample_info[0].profile <= 4) {
    mProfile = audio->sample_info[0].profile;
  }

  Mp4parseByteData extraData = audio->sample_info[0].extra_data;
  // If length is 0 we append nothing
  mExtraData->AppendElements(extraData.data, extraData.length);
  mCodecSpecificConfig->AppendElements(codecSpecificConfig.data,
                                       codecSpecificConfig.length);
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
  }
  mTrackId = track->track_id;
  mDuration = TimeUnit::FromMicroseconds(track->duration);
  mMediaTime = TimeUnit::FromMicroseconds(track->media_time);
  mDisplay.width = video->display_width;
  mDisplay.height = video->display_height;
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
