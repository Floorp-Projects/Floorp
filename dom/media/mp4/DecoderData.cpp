/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Adts.h"
#include "AnnexB.h"
#include "BufferReader.h"
#include "DecoderData.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/EndianUtils.h"
#include "VideoUtils.h"

// OpusDecoder header is really needed only by MP4 in rust
#include "OpusDecoder.h"
#include "mp4parse.h"

using mozilla::media::TimeUnit;

namespace mozilla
{

mozilla::Result<mozilla::Ok, nsresult>
CryptoFile::DoUpdate(const uint8_t* aData, size_t aLength)
{
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
    pssh.AppendElement(psshInfo);
  }
  return mozilla::Ok();
}

bool
MP4AudioInfo::IsValid() const
{
  return mChannels > 0 && mRate > 0 &&
         // Accept any mime type here, but if it's aac, validate the profile.
         (!mMimeType.EqualsLiteral("audio/mp4a-latm") ||
          mProfile > 0 || mExtendedProfile > 0);
}

static void
UpdateTrackProtectedInfo(mozilla::TrackInfo& aConfig,
                         const mp4parse_sinf_info& aSinf)
{
  if (aSinf.is_encrypted != 0) {
    aConfig.mCrypto.mValid = true;
    aConfig.mCrypto.mMode = aSinf.is_encrypted;
    aConfig.mCrypto.mIVSize = aSinf.iv_size;
    aConfig.mCrypto.mKeyId.AppendElements(aSinf.kid.data, aSinf.kid.length);
  }
}

void
MP4AudioInfo::Update(const mp4parse_track_info* track,
                     const mp4parse_track_audio_info* audio)
{
  UpdateTrackProtectedInfo(*this, audio->protected_data);

  if (track->codec == mp4parse_codec_OPUS) {
    mMimeType = NS_LITERAL_CSTRING("audio/opus");
    // The Opus decoder expects the container's codec delay or
    // pre-skip value, in microseconds, as a 64-bit int at the
    // start of the codec-specific config blob.
    MOZ_ASSERT(audio->extra_data.data);
    MOZ_ASSERT(audio->extra_data.length >= 12);
    uint16_t preskip =
      mozilla::LittleEndian::readUint16(audio->extra_data.data + 10);
    mozilla::OpusDataDecoder::AppendCodecDelay(mCodecSpecificConfig,
        mozilla::FramesToUsecs(preskip, 48000).value());
  } else if (track->codec == mp4parse_codec_AAC) {
    mMimeType = NS_LITERAL_CSTRING("audio/mp4a-latm");
  } else if (track->codec == mp4parse_codec_FLAC) {
    mMimeType = NS_LITERAL_CSTRING("audio/flac");
  } else if (track->codec == mp4parse_codec_MP3) {
    mMimeType = NS_LITERAL_CSTRING("audio/mpeg");
  }

  mRate = audio->sample_rate;
  mChannels = audio->channels;
  mBitDepth = audio->bit_depth;
  mExtendedProfile = audio->profile;
  mDuration = TimeUnit::FromMicroseconds(track->duration);
  mMediaTime = TimeUnit::FromMicroseconds(track->media_time);
  mTrackId = track->track_id;

  // In stagefright, mProfile is kKeyAACProfile, mExtendedProfile is kKeyAACAOT.
  // Both are from audioObjectType in AudioSpecificConfig.
  if (audio->profile <= 4) {
    mProfile = audio->profile;
  }

  if (audio->extra_data.length > 0) {
    mExtraData->AppendElements(audio->extra_data.data,
                               audio->extra_data.length);
  }

  if (audio->codec_specific_config.length > 0) {
    mCodecSpecificConfig->AppendElements(audio->codec_specific_config.data,
                                         audio->codec_specific_config.length);
  }
}

void
MP4VideoInfo::Update(const mp4parse_track_info* track,
                     const mp4parse_track_video_info* video)
{
  UpdateTrackProtectedInfo(*this, video->protected_data);
  if (track->codec == mp4parse_codec_AVC) {
    mMimeType = NS_LITERAL_CSTRING("video/avc");
  } else if (track->codec == mp4parse_codec_VP9) {
    mMimeType = NS_LITERAL_CSTRING("video/vp9");
  } else if (track->codec == mp4parse_codec_MP4V) {
    mMimeType = NS_LITERAL_CSTRING("video/mp4v-es");
  }
  mTrackId = track->track_id;
  mDuration = TimeUnit::FromMicroseconds(track->duration);
  mMediaTime = TimeUnit::FromMicroseconds(track->media_time);
  mDisplay.width = video->display_width;
  mDisplay.height = video->display_height;
  mImage.width = video->image_width;
  mImage.height = video->image_height;
  mRotation = ToSupportedRotation(video->rotation);
  if (video->extra_data.data) {
    mExtraData->AppendElements(video->extra_data.data, video->extra_data.length);
  }
}

bool
MP4VideoInfo::IsValid() const
{
  return (mDisplay.width > 0 && mDisplay.height > 0) ||
    (mImage.width > 0 && mImage.height > 0);
}

}
