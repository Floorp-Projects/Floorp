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

IndiceWrapper::IndiceWrapper(Mp4parseByteData& aIndice)
{
  mIndice.data = nullptr;
  mIndice.length = aIndice.length;
  mIndice.indices = aIndice.indices;
}

size_t
IndiceWrapper::Length() const
{
  return mIndice.length;
}

bool
IndiceWrapper::GetIndice(size_t aIndex, Index::Indice& aIndice) const
{
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

static const char *
TrackTypeToString(mozilla::TrackInfo::TrackType aType)
{
  switch (aType) {
  case mozilla::TrackInfo::kAudioTrack:
    return "audio";
  case mozilla::TrackInfo::kVideoTrack:
    return "video";
  default:
    return "unknown";
  }
}

bool
StreamAdaptor::Read(uint8_t* buffer, uintptr_t size, size_t* bytes_read)
{
  if (!mOffset.isValid()) {
    MOZ_LOG(gMP4MetadataLog, LogLevel::Error, ("Overflow in source stream offset"));
    return false;
  }
  bool rv = mSource->ReadAt(mOffset.value(), buffer, size, bytes_read);
  if (rv) {
    mOffset += *bytes_read;
  }
  return rv;
}

// Wrapper to allow rust to call our read adaptor.
static intptr_t
read_source(uint8_t* buffer, uintptr_t size, void* userdata)
{
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
  : mSource(aSource)
  , mSourceAdaptor(aSource)
{
  DDLINKCHILD("source", aSource);

  Mp4parseIo io = { read_source, &mSourceAdaptor };
  mParser.reset(mp4parse_new(&io));
  MOZ_ASSERT(mParser);
}

MP4Metadata::~MP4Metadata()
{
}

nsresult
MP4Metadata::Parse()
{
  Mp4parseStatus rv = mp4parse_read(mParser.get());
  if (rv != MP4PARSE_STATUS_OK) {
    MOZ_LOG(gMP4MetadataLog, LogLevel::Debug, ("Parse failed, return code %d\n", rv));
    return rv == MP4PARSE_STATUS_OOM ? NS_ERROR_OUT_OF_MEMORY
                                     : NS_ERROR_DOM_MEDIA_METADATA_ERR;
  }

  UpdateCrypto();

  return NS_OK;
}

void
MP4Metadata::UpdateCrypto()
{
  Mp4parsePsshInfo info = {};
  if (mp4parse_get_pssh_info(mParser.get(), &info) != MP4PARSE_STATUS_OK) {
    return;
  }

  if (info.data.length == 0) {
    return;
  }

  mCrypto.Update(info.data.data, info.data.length);
}

bool
TrackTypeEqual(TrackInfo::TrackType aLHS, Mp4parseTrackType aRHS)
{
  switch (aLHS) {
  case TrackInfo::kAudioTrack:
    return aRHS == MP4PARSE_TRACK_TYPE_AUDIO;
  case TrackInfo::kVideoTrack:
    return aRHS == MP4PARSE_TRACK_TYPE_VIDEO;
  default:
    return false;
  }
}

MP4Metadata::ResultAndTrackCount
MP4Metadata::GetNumberTracks(mozilla::TrackInfo::TrackType aType) const
{
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
    if (track_info.codec == MP4PARSE_CODEC_UNKNOWN) {
      continue;
    }
    if (TrackTypeEqual(aType, track_info.track_type)) {
        total += 1;
    }
  }

  MOZ_LOG(gMP4MetadataLog, LogLevel::Info, ("%s tracks found: %u",
                                            TrackTypeToString(aType),
                                            total));

  return {NS_OK, total};
}

Maybe<uint32_t>
MP4Metadata::TrackTypeToGlobalTrackIndex(mozilla::TrackInfo::TrackType aType, size_t aTrackNumber) const
{
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

MP4Metadata::ResultAndTrackInfo
MP4Metadata::GetTrackInfo(mozilla::TrackInfo::TrackType aType,
                              size_t aTrackNumber) const
{
  Maybe<uint32_t> trackIndex = TrackTypeToGlobalTrackIndex(aType, aTrackNumber);
  if (trackIndex.isNothing()) {
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("No %s tracks",
                                      TrackTypeToStr(aType))),
            nullptr};
  }

  Mp4parseTrackInfo info;
  auto rv = mp4parse_get_track_info(mParser.get(), trackIndex.value(), &info);
  if (rv != MP4PARSE_STATUS_OK) {
    MOZ_LOG(gMP4MetadataLog, LogLevel::Warning, ("mp4parse_get_track_info returned %d", rv));
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("Cannot find %s track #%zu",
                                      TrackTypeToStr(aType),
                                      aTrackNumber)),
            nullptr};
  }
#ifdef DEBUG
  const char* codec_string = "unrecognized";
  switch (info.codec) {
    case MP4PARSE_CODEC_UNKNOWN: codec_string = "unknown"; break;
    case MP4PARSE_CODEC_AAC: codec_string = "aac"; break;
    case MP4PARSE_CODEC_OPUS: codec_string = "opus"; break;
    case MP4PARSE_CODEC_FLAC: codec_string = "flac"; break;
    case MP4PARSE_CODEC_ALAC: codec_string = "alac"; break;
    case MP4PARSE_CODEC_AVC: codec_string = "h.264"; break;
    case MP4PARSE_CODEC_VP9: codec_string = "vp9"; break;
    case MP4PARSE_CODEC_MP3: codec_string = "mp3"; break;
    case MP4PARSE_CODEC_MP4V: codec_string = "mp4v"; break;
    case MP4PARSE_CODEC_JPEG: codec_string = "jpeg"; break;
    case MP4PARSE_CODEC_AC3: codec_string = "ac-3"; break;
    case MP4PARSE_CODEC_EC3: codec_string = "ec-3"; break;
  }
  MOZ_LOG(gMP4MetadataLog, LogLevel::Debug,
    ("track codec %s (%u)\n", codec_string, info.codec));
#endif

  // This specialization interface is crazy.
  UniquePtr<mozilla::TrackInfo> e;
  switch (aType) {
    case TrackInfo::TrackType::kAudioTrack: {
      Mp4parseTrackAudioInfo audio;
      auto rv = mp4parse_get_track_audio_info(mParser.get(), trackIndex.value(), &audio);
      if (rv != MP4PARSE_STATUS_OK) {
        MOZ_LOG(gMP4MetadataLog, LogLevel::Warning, ("mp4parse_get_track_audio_info returned error %d", rv));
        return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                            RESULT_DETAIL("Cannot parse %s track #%zu",
                                          TrackTypeToStr(aType),
                                          aTrackNumber)),
                nullptr};
      }
      auto track = mozilla::MakeUnique<MP4AudioInfo>();
      track->Update(&info, &audio);
      e = std::move(track);
    }
    break;
    case TrackInfo::TrackType::kVideoTrack: {
      Mp4parseTrackVideoInfo video;
      auto rv = mp4parse_get_track_video_info(mParser.get(), trackIndex.value(), &video);
      if (rv != MP4PARSE_STATUS_OK) {
        MOZ_LOG(gMP4MetadataLog, LogLevel::Warning, ("mp4parse_get_track_video_info returned error %d", rv));
        return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                            RESULT_DETAIL("Cannot parse %s track #%zu",
                                          TrackTypeToStr(aType),
                                          aTrackNumber)),
                nullptr};
      }
      auto track = mozilla::MakeUnique<MP4VideoInfo>();
      track->Update(&info, &video);
      e = std::move(track);
    }
    break;
    default:
      MOZ_LOG(gMP4MetadataLog, LogLevel::Warning, ("unhandled track type %d", aType));
      return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                          RESULT_DETAIL("Cannot handle %s track #%zu",
                                        TrackTypeToStr(aType),
                                        aTrackNumber)),
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
                                    TrackTypeToStr(aType),
                                    aTrackNumber)),
          nullptr};
}

bool
MP4Metadata::CanSeek() const
{
  return true;
}

MP4Metadata::ResultAndCryptoFile
MP4Metadata::Crypto() const
{
  return {NS_OK, &mCrypto};
}

MP4Metadata::ResultAndIndice
MP4Metadata::GetTrackIndice(mozilla::TrackID aTrackID)
{
  Mp4parseByteData indiceRawData = {};

  uint8_t fragmented = false;
  auto rv = mp4parse_is_fragmented(mParser.get(), aTrackID, &fragmented);
  if (rv != MP4PARSE_STATUS_OK) {
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("Cannot parse whether track id %d is "
                                      "fragmented, mp4parse_error=%d",
                                      int(aTrackID), int(rv))),
            nullptr};
  }

  if (!fragmented) {
    rv = mp4parse_get_indice_table(mParser.get(), aTrackID, &indiceRawData);
    if (rv != MP4PARSE_STATUS_OK) {
      return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                          RESULT_DETAIL("Cannot parse index table in track id %d, "
                                        "mp4parse_error=%d",
                                        int(aTrackID), int(rv))),
              nullptr};
    }
  }

  UniquePtr<IndiceWrapper> indice;
  indice = mozilla::MakeUnique<IndiceWrapper>(indiceRawData);

  return {NS_OK, std::move(indice)};
}

/*static*/ MP4Metadata::ResultAndByteBuffer
MP4Metadata::Metadata(ByteStream* aSource)
{
  auto parser = mozilla::MakeUnique<MoofParser>(aSource, 0, false);
  RefPtr<mozilla::MediaByteBuffer> buffer = parser->Metadata();
  if (!buffer) {
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("Cannot parse metadata")),
            nullptr};
  }
  return {NS_OK, std::move(buffer)};
}

} // namespace mozilla
