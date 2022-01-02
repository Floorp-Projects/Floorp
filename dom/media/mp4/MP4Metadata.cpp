/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/UniquePtr.h"
#include "VideoUtils.h"
#include "MoofParser.h"
#include "MP4Metadata.h"
#include "ByteStream.h"
#include "mp4parse.h"

#include <limits>
#include <stdint.h>
#include <vector>

using mozilla::media::TimeUnit;

namespace mozilla {
LazyLogModule gMP4MetadataLog("MP4Metadata");

IndiceWrapper::IndiceWrapper(Mp4parseByteData& aIndice) {
  mIndice.data = nullptr;
  mIndice.length = aIndice.length;
  mIndice.indices = aIndice.indices;
}

size_t IndiceWrapper::Length() const { return mIndice.length; }

bool IndiceWrapper::GetIndice(size_t aIndex, Index::Indice& aIndice) const {
  if (aIndex >= mIndice.length) {
    MOZ_LOG(gMP4MetadataLog, LogLevel::Error, ("Index overflow in indice"));
    return false;
  }

  const Mp4parseIndice* indice = &mIndice.indices[aIndex];
  aIndice.start_offset = indice->start_offset;
  aIndice.end_offset = indice->end_offset;
  aIndice.start_composition = indice->start_composition;
  aIndice.end_composition = indice->end_composition;
  aIndice.start_decode = indice->start_decode;
  aIndice.sync = indice->sync;
  return true;
}

static const char* TrackTypeToString(mozilla::TrackInfo::TrackType aType) {
  switch (aType) {
    case mozilla::TrackInfo::kAudioTrack:
      return "audio";
    case mozilla::TrackInfo::kVideoTrack:
      return "video";
    default:
      return "unknown";
  }
}

bool StreamAdaptor::Read(uint8_t* buffer, uintptr_t size, size_t* bytes_read) {
  if (!mOffset.isValid()) {
    MOZ_LOG(gMP4MetadataLog, LogLevel::Error,
            ("Overflow in source stream offset"));
    return false;
  }
  bool rv = mSource->ReadAt(mOffset.value(), buffer, size, bytes_read);
  if (rv) {
    mOffset += *bytes_read;
  }
  return rv;
}

// Wrapper to allow rust to call our read adaptor.
static intptr_t read_source(uint8_t* buffer, uintptr_t size, void* userdata) {
  MOZ_ASSERT(buffer);
  MOZ_ASSERT(userdata);

  auto source = reinterpret_cast<StreamAdaptor*>(userdata);
  size_t bytes_read = 0;
  bool rv = source->Read(buffer, size, &bytes_read);
  if (!rv) {
    MOZ_LOG(gMP4MetadataLog, LogLevel::Warning, ("Error reading source data"));
    return -1;
  }
  return bytes_read;
}

MP4Metadata::MP4Metadata(ByteStream* aSource)
    : mSource(aSource), mSourceAdaptor(aSource) {
  DDLINKCHILD("source", aSource);
}

MP4Metadata::~MP4Metadata() = default;

nsresult MP4Metadata::Parse() {
  Mp4parseIo io = {read_source, &mSourceAdaptor};
  Mp4parseParser* parser = nullptr;
  Mp4parseStatus status = mp4parse_new(&io, &parser);
  if (status == MP4PARSE_STATUS_OK && parser) {
    mParser.reset(parser);
    MOZ_ASSERT(mParser);
  } else {
    MOZ_ASSERT(!mParser);
    MOZ_LOG(gMP4MetadataLog, LogLevel::Debug,
            ("Parse failed, return code %d\n", status));
    return status == MP4PARSE_STATUS_OOM ? NS_ERROR_OUT_OF_MEMORY
                                         : NS_ERROR_DOM_MEDIA_METADATA_ERR;
  }

  UpdateCrypto();

  return NS_OK;
}

void MP4Metadata::UpdateCrypto() {
  Mp4parsePsshInfo info = {};
  if (mp4parse_get_pssh_info(mParser.get(), &info) != MP4PARSE_STATUS_OK) {
    return;
  }

  if (info.data.length == 0) {
    return;
  }

  mCrypto.Update(info.data.data, info.data.length);
}

bool TrackTypeEqual(TrackInfo::TrackType aLHS, Mp4parseTrackType aRHS) {
  switch (aLHS) {
    case TrackInfo::kAudioTrack:
      return aRHS == MP4PARSE_TRACK_TYPE_AUDIO;
    case TrackInfo::kVideoTrack:
      return aRHS == MP4PARSE_TRACK_TYPE_VIDEO;
    default:
      return false;
  }
}

MP4Metadata::ResultAndTrackCount MP4Metadata::GetNumberTracks(
    mozilla::TrackInfo::TrackType aType) const {
  uint32_t tracks;
  auto rv = mp4parse_get_track_count(mParser.get(), &tracks);
  if (rv != MP4PARSE_STATUS_OK) {
    MOZ_LOG(gMP4MetadataLog, LogLevel::Warning,
            ("rust parser error %d counting tracks", rv));
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("Rust parser error %d", rv)),
            MP4Metadata::NumberTracksError()};
  }

  uint32_t total = 0;
  for (uint32_t i = 0; i < tracks; ++i) {
    Mp4parseTrackInfo track_info;
    rv = mp4parse_get_track_info(mParser.get(), i, &track_info);
    if (rv != MP4PARSE_STATUS_OK) {
      continue;
    }

    if (track_info.track_type == MP4PARSE_TRACK_TYPE_AUDIO) {
      Mp4parseTrackAudioInfo audio;
      auto rv = mp4parse_get_track_audio_info(mParser.get(), i, &audio);
      if (rv != MP4PARSE_STATUS_OK) {
        MOZ_LOG(gMP4MetadataLog, LogLevel::Warning,
                ("mp4parse_get_track_audio_info returned error %d", rv));
        continue;
      }
      MOZ_DIAGNOSTIC_ASSERT(audio.sample_info_count > 0,
                            "Must have at least one audio sample info");
      if (audio.sample_info_count == 0) {
        return {
            MediaResult(
                NS_ERROR_DOM_MEDIA_METADATA_ERR,
                RESULT_DETAIL(
                    "Got 0 audio sample info while checking number tracks")),
            MP4Metadata::NumberTracksError()};
      }
      // We assume the codec of the first sample info is representative of the
      // whole track and skip it if we don't recognize the codec.
      if (audio.sample_info[0].codec_type == MP4PARSE_CODEC_UNKNOWN) {
        continue;
      }
    } else if (track_info.track_type == MP4PARSE_TRACK_TYPE_VIDEO) {
      Mp4parseTrackVideoInfo video;
      auto rv = mp4parse_get_track_video_info(mParser.get(), i, &video);
      if (rv != MP4PARSE_STATUS_OK) {
        MOZ_LOG(gMP4MetadataLog, LogLevel::Warning,
                ("mp4parse_get_track_video_info returned error %d", rv));
        continue;
      }
      MOZ_DIAGNOSTIC_ASSERT(video.sample_info_count > 0,
                            "Must have at least one video sample info");
      if (video.sample_info_count == 0) {
        return {
            MediaResult(
                NS_ERROR_DOM_MEDIA_METADATA_ERR,
                RESULT_DETAIL(
                    "Got 0 video sample info while checking number tracks")),
            MP4Metadata::NumberTracksError()};
      }
      // We assume the codec of the first sample info is representative of the
      // whole track and skip it if we don't recognize the codec.
      if (video.sample_info[0].codec_type == MP4PARSE_CODEC_UNKNOWN) {
        continue;
      }
    } else {
      // Only audio and video are supported
      continue;
    }
    if (TrackTypeEqual(aType, track_info.track_type)) {
      total += 1;
    }
  }

  MOZ_LOG(gMP4MetadataLog, LogLevel::Info,
          ("%s tracks found: %u", TrackTypeToString(aType), total));

  return {NS_OK, total};
}

Maybe<uint32_t> MP4Metadata::TrackTypeToGlobalTrackIndex(
    mozilla::TrackInfo::TrackType aType, size_t aTrackNumber) const {
  uint32_t tracks;
  auto rv = mp4parse_get_track_count(mParser.get(), &tracks);
  if (rv != MP4PARSE_STATUS_OK) {
    return Nothing();
  }

  /* The MP4Metadata API uses a per-TrackType index of tracks, but mp4parse
     (and libstagefright) use a global track index.  Convert the index by
     counting the tracks of the requested type and returning the global
     track index when a match is found. */
  uint32_t perType = 0;
  for (uint32_t i = 0; i < tracks; ++i) {
    Mp4parseTrackInfo track_info;
    rv = mp4parse_get_track_info(mParser.get(), i, &track_info);
    if (rv != MP4PARSE_STATUS_OK) {
      continue;
    }
    if (TrackTypeEqual(aType, track_info.track_type)) {
      if (perType == aTrackNumber) {
        return Some(i);
      }
      perType += 1;
    }
  }

  return Nothing();
}

MP4Metadata::ResultAndTrackInfo MP4Metadata::GetTrackInfo(
    mozilla::TrackInfo::TrackType aType, size_t aTrackNumber) const {
  Maybe<uint32_t> trackIndex = TrackTypeToGlobalTrackIndex(aType, aTrackNumber);
  if (trackIndex.isNothing()) {
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("No %s tracks", TrackTypeToStr(aType))),
            nullptr};
  }

  Mp4parseTrackInfo info;
  auto rv = mp4parse_get_track_info(mParser.get(), trackIndex.value(), &info);
  if (rv != MP4PARSE_STATUS_OK) {
    MOZ_LOG(gMP4MetadataLog, LogLevel::Warning,
            ("mp4parse_get_track_info returned %d", rv));
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("Cannot find %s track #%zu",
                                      TrackTypeToStr(aType), aTrackNumber)),
            nullptr};
  }
#ifdef DEBUG
  bool haveSampleInfo = false;
  const char* codecString = "unrecognized";
  Mp4parseCodec codecType = MP4PARSE_CODEC_UNKNOWN;
  if (info.track_type == MP4PARSE_TRACK_TYPE_AUDIO) {
    Mp4parseTrackAudioInfo audio;
    auto rv = mp4parse_get_track_audio_info(mParser.get(), trackIndex.value(),
                                            &audio);
    if (rv == MP4PARSE_STATUS_OK && audio.sample_info_count > 0) {
      codecType = audio.sample_info[0].codec_type;
      haveSampleInfo = true;
    }
  } else if (info.track_type == MP4PARSE_TRACK_TYPE_VIDEO) {
    Mp4parseTrackVideoInfo video;
    auto rv = mp4parse_get_track_video_info(mParser.get(), trackIndex.value(),
                                            &video);
    if (rv == MP4PARSE_STATUS_OK && video.sample_info_count > 0) {
      codecType = video.sample_info[0].codec_type;
      haveSampleInfo = true;
    }
  }
  if (haveSampleInfo) {
    switch (codecType) {
      case MP4PARSE_CODEC_UNKNOWN:
        codecString = "unknown";
        break;
      case MP4PARSE_CODEC_AAC:
        codecString = "aac";
        break;
      case MP4PARSE_CODEC_OPUS:
        codecString = "opus";
        break;
      case MP4PARSE_CODEC_FLAC:
        codecString = "flac";
        break;
      case MP4PARSE_CODEC_ALAC:
        codecString = "alac";
        break;
      case MP4PARSE_CODEC_H263:
        codecString = "h.263";
        break;
      case MP4PARSE_CODEC_AVC:
        codecString = "h.264";
        break;
      case MP4PARSE_CODEC_VP9:
        codecString = "vp9";
        break;
      case MP4PARSE_CODEC_AV1:
        codecString = "av1";
        break;
      case MP4PARSE_CODEC_MP3:
        codecString = "mp3";
        break;
      case MP4PARSE_CODEC_MP4V:
        codecString = "mp4v";
        break;
      case MP4PARSE_CODEC_JPEG:
        codecString = "jpeg";
        break;
      case MP4PARSE_CODEC_AC3:
        codecString = "ac-3";
        break;
      case MP4PARSE_CODEC_EC3:
        codecString = "ec-3";
        break;
    }
  }
  MOZ_LOG(gMP4MetadataLog, LogLevel::Debug,
          ("track codec %s (%u)\n", codecString, codecType));
#endif

  // This specialization interface is wild.
  UniquePtr<mozilla::TrackInfo> e;
  switch (aType) {
    case TrackInfo::TrackType::kAudioTrack: {
      Mp4parseTrackAudioInfo audio;
      auto rv = mp4parse_get_track_audio_info(mParser.get(), trackIndex.value(),
                                              &audio);
      if (rv != MP4PARSE_STATUS_OK) {
        MOZ_LOG(gMP4MetadataLog, LogLevel::Warning,
                ("mp4parse_get_track_audio_info returned error %d", rv));
        return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                            RESULT_DETAIL("Cannot parse %s track #%zu",
                                          TrackTypeToStr(aType), aTrackNumber)),
                nullptr};
      }
      auto track = mozilla::MakeUnique<MP4AudioInfo>();
      MediaResult updateStatus = track->Update(&info, &audio);
      if (NS_FAILED(updateStatus)) {
        MOZ_LOG(gMP4MetadataLog, LogLevel::Warning,
                ("Updating audio track failed with %s",
                 updateStatus.Message().get()));
        return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                            RESULT_DETAIL(
                                "Failed to update %s track #%zu with error: %s",
                                TrackTypeToStr(aType), aTrackNumber,
                                updateStatus.Message().get())),
                nullptr};
      }
      e = std::move(track);
    } break;
    case TrackInfo::TrackType::kVideoTrack: {
      Mp4parseTrackVideoInfo video;
      auto rv = mp4parse_get_track_video_info(mParser.get(), trackIndex.value(),
                                              &video);
      if (rv != MP4PARSE_STATUS_OK) {
        MOZ_LOG(gMP4MetadataLog, LogLevel::Warning,
                ("mp4parse_get_track_video_info returned error %d", rv));
        return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                            RESULT_DETAIL("Cannot parse %s track #%zu",
                                          TrackTypeToStr(aType), aTrackNumber)),
                nullptr};
      }
      auto track = mozilla::MakeUnique<MP4VideoInfo>();
      MediaResult updateStatus = track->Update(&info, &video);
      if (NS_FAILED(updateStatus)) {
        MOZ_LOG(gMP4MetadataLog, LogLevel::Warning,
                ("Updating video track failed with %s",
                 updateStatus.Message().get()));
        return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                            RESULT_DETAIL(
                                "Failed to update %s track #%zu with error: %s",
                                TrackTypeToStr(aType), aTrackNumber,
                                updateStatus.Message().get())),
                nullptr};
      }
      e = std::move(track);
    } break;
    default:
      MOZ_LOG(gMP4MetadataLog, LogLevel::Warning,
              ("unhandled track type %d", aType));
      return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                          RESULT_DETAIL("Cannot handle %s track #%zu",
                                        TrackTypeToStr(aType), aTrackNumber)),
              nullptr};
  }

  // No duration in track, use fragment_duration.
  if (e && !e->mDuration.IsPositive()) {
    Mp4parseFragmentInfo info;
    auto rv = mp4parse_get_fragment_info(mParser.get(), &info);
    if (rv == MP4PARSE_STATUS_OK) {
      e->mDuration = TimeUnit::FromMicroseconds(info.fragment_duration);
    }
  }

  if (e && e->IsValid()) {
    return {NS_OK, std::move(e)};
  }
  MOZ_LOG(gMP4MetadataLog, LogLevel::Debug, ("TrackInfo didn't validate"));

  return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                      RESULT_DETAIL("Invalid %s track #%zu",
                                    TrackTypeToStr(aType), aTrackNumber)),
          nullptr};
}

bool MP4Metadata::CanSeek() const { return true; }

MP4Metadata::ResultAndCryptoFile MP4Metadata::Crypto() const {
  return {NS_OK, &mCrypto};
}

MP4Metadata::ResultAndIndice MP4Metadata::GetTrackIndice(uint32_t aTrackId) {
  Mp4parseByteData indiceRawData = {};

  uint8_t fragmented = false;
  auto rv = mp4parse_is_fragmented(mParser.get(), aTrackId, &fragmented);
  if (rv != MP4PARSE_STATUS_OK) {
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("Cannot parse whether track id %u is "
                                      "fragmented, mp4parse_error=%d",
                                      aTrackId, int(rv))),
            nullptr};
  }

  if (!fragmented) {
    rv = mp4parse_get_indice_table(mParser.get(), aTrackId, &indiceRawData);
    if (rv != MP4PARSE_STATUS_OK) {
      return {
          MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                      RESULT_DETAIL("Cannot parse index table in track id %u, "
                                    "mp4parse_error=%d",
                                    aTrackId, int(rv))),
          nullptr};
    }
  }

  UniquePtr<IndiceWrapper> indice;
  indice = mozilla::MakeUnique<IndiceWrapper>(indiceRawData);

  return {NS_OK, std::move(indice)};
}

/*static*/ MP4Metadata::ResultAndByteBuffer MP4Metadata::Metadata(
    ByteStream* aSource) {
  auto parser = mozilla::MakeUnique<MoofParser>(
      aSource, AsVariant(ParseAllTracks{}), false);
  RefPtr<mozilla::MediaByteBuffer> buffer = parser->Metadata();
  if (!buffer) {
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("Cannot parse metadata")),
            nullptr};
  }
  return {NS_OK, std::move(buffer)};
}

}  // namespace mozilla
