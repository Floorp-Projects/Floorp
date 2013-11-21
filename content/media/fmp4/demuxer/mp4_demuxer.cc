// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "mp4_demuxer/mp4_demuxer.h"

#include "mp4_demuxer/Streams.h"
#include "mp4_demuxer/box_reader.h"
#include "mp4_demuxer/box_definitions.h"
#include "mp4_demuxer/basictypes.h"
#include "mp4_demuxer/es_descriptor.h"
#include "mp4_demuxer/video_util.h"
#include "mp4_demuxer/track_run_iterator.h"
#include "mp4_demuxer/audio_decoder_config.h"
#include "mp4_demuxer/video_decoder_config.h"

#include <assert.h>

using namespace std;

namespace mp4_demuxer {


MP4Sample::MP4Sample(Microseconds _decode_timestamp,
                     Microseconds _composition_timestamp,
                     Microseconds _duration,
                     int64_t _byte_offset,
                     std::vector<uint8_t>* _data,
                     TrackType _type,
                     DecryptConfig* _decrypt_config,
                     bool _is_sync_point)
  : decode_timestamp(_decode_timestamp),
    composition_timestamp(_composition_timestamp),
    duration(_duration),
    byte_offset(_byte_offset),
    data(_data),
    type(_type),
    decrypt_config(_decrypt_config),
    is_sync_point(_is_sync_point)
{
}

MP4Sample::~MP4Sample()
{
}

bool MP4Sample::is_encrypted() const {
  return decrypt_config != nullptr;
};




MP4Demuxer::MP4Demuxer(Stream* stream)
  : state_(kWaitingForInit),
    stream_(stream),
    stream_offset_(0),
    duration_(InfiniteMicroseconds),
    moof_head_(0),
    mdat_tail_(0),
    audio_track_id_(0),
    video_track_id_(0),
    audio_frameno(0),
    video_frameno(0),
    has_audio_(false),
    has_sbr_(false),
    is_audio_track_encrypted_(false),
    has_video_(false),
    is_video_track_encrypted_(false),
    can_seek_(false)
{
}

MP4Demuxer::~MP4Demuxer()
{
}

bool MP4Demuxer::Init()
{
  ChangeState(kParsingBoxes);

  // Read from the stream until the moov box is read. This will have the
  // header data that we need to initialize the decoders.
  bool ok = true;
  const int64_t length = stream_->Length();
  while (ok &&
         stream_offset_ < length &&
         !moov_ &&
         state_ == kParsingBoxes) {
    ok = ParseBox();
  }
  return state_  >= kParsingBoxes &&
         state_ < kError;
}

void MP4Demuxer::Reset() {
  moov_ = nullptr;
  runs_ = nullptr;
  stream_offset_; // TODO; Not sure if this needs to be reset?
  DMX_LOG("Warning: resetting stream_offset_\n");
  moof_head_ = 0;
  mdat_tail_ = 0;
}

// TODO(xhwang): Figure out the init data type appropriately once it's spec'ed.
static const char kMp4InitDataType[] = "video/mp4";

bool MP4Demuxer::ParseMoov(BoxReader* reader) {
  RCHECK(state_ < kError);
  moov_ = new Movie();
  RCHECK(moov_->Parse(reader));
  runs_ = new TrackRunIterator(moov_.get());

  has_audio_ = false;
  has_video_ = false;

  for (std::vector<Track>::const_iterator track = moov_->tracks.begin();
       track != moov_->tracks.end(); ++track) {
    // TODO(strobe): Only the first audio and video track present in a file are
    // used. (Track selection is better accomplished via Source IDs, though, so
    // adding support for track selection within a stream is low-priority.)
    const SampleDescription& samp_descr =
        track->media.information.sample_table.description;

    // TODO(strobe): When codec reconfigurations are supported, detect and send
    // a codec reconfiguration for fragments using a sample description index
    // different from the previous one
    size_t desc_idx = 0;
    for (size_t t = 0; t < moov_->extends.tracks.size(); t++) {
      const TrackExtends& trex = moov_->extends.tracks[t];
      if (trex.track_id == track->header.track_id) {
        desc_idx = trex.default_sample_description_index;
        break;
      }
    }
    RCHECK(desc_idx > 0);
    desc_idx -= 1;  // BMFF descriptor index is one-based

    if (track->media.handler.type == kAudio && !audio_config_.IsValidConfig()) {
      RCHECK(!samp_descr.audio_entries.empty());

      // It is not uncommon to find otherwise-valid files with incorrect sample
      // description indices, so we fail gracefully in that case.
      if (desc_idx >= samp_descr.audio_entries.size())
        desc_idx = 0;
      const AudioSampleEntry& entry = samp_descr.audio_entries[desc_idx];
      const AAC& aac = entry.esds.aac;

      if (!(entry.format == FOURCC_MP4A ||
            (entry.format == FOURCC_ENCA &&
             entry.sinf.format.format == FOURCC_MP4A))) {
        DMX_LOG("Unsupported audio format 0x%x in stsd box\n", entry.format);
        return false;
      }

      int audio_type = entry.esds.object_type;
      DMX_LOG("audio_type 0x%x\n", audio_type);

      const std::vector<uint8_t>& asc = aac.AudioSpecificConfig();
      if (asc.size() > 0) {
        DMX_LOG("audio specific config:");
        for (unsigned i=0; i<asc.size(); ++i) {
          DMX_LOG(" 0x%x", asc[i]);
        }
        DMX_LOG("\n");
      }

      // Check if it is MPEG4 AAC defined in ISO 14496 Part 3 or
      // supported MPEG2 AAC varients.
      if (audio_type != kISO_14496_3 && audio_type != kISO_13818_7_AAC_LC) {
        DMX_LOG("Unsupported audio object type 0x%x  in esds.", audio_type);
        return false;
      }

      SampleFormat sample_format;
      if (entry.samplesize == 8) {
        sample_format = kSampleFormatU8;
      } else if (entry.samplesize == 16) {
        sample_format = kSampleFormatS16;
      } else if (entry.samplesize == 32) {
        sample_format = kSampleFormatS32;
      } else {
        DMX_LOG("Unsupported sample size.\n");
        return false;
      }

      is_audio_track_encrypted_ = entry.sinf.info.track_encryption.is_encrypted;
      DMX_LOG("is_audio_track_encrypted_: %d\n", is_audio_track_encrypted_);
      // TODO(cpearce): Chromium checks the MIME type specified to see if it contains
      // the codec info that tells us it's using SBR. We should check for that
      // here too.
      audio_config_.Initialize(kCodecAAC, sample_format,
                               aac.GetChannelLayout(has_sbr_),
                               aac.GetOutputSamplesPerSecond(has_sbr_),
                               &asc.front(),
                               asc.size(),
                               is_audio_track_encrypted_);
      has_audio_ = true;
      audio_track_id_ = track->header.track_id;
    }
    if (track->media.handler.type == kVideo && !video_config_.IsValidConfig()) {
      RCHECK(!samp_descr.video_entries.empty());
      if (desc_idx >= samp_descr.video_entries.size())
        desc_idx = 0;
      const VideoSampleEntry& entry = samp_descr.video_entries[desc_idx];

      if (!(entry.format == FOURCC_AVC1 ||
            (entry.format == FOURCC_ENCV &&
             entry.sinf.format.format == FOURCC_AVC1))) {
        DMX_LOG("Unsupported video format 0x%x in stsd box.\n", entry.format);
        return false;
      }

      // TODO(strobe): Recover correct crop box
      IntSize coded_size(entry.width, entry.height);
      IntRect visible_rect(0, 0, coded_size.width(), coded_size.height());
      IntSize natural_size = GetNaturalSize(visible_rect.size(),
                                            entry.pixel_aspect.h_spacing,
                                            entry.pixel_aspect.v_spacing);
      is_video_track_encrypted_ = entry.sinf.info.track_encryption.is_encrypted;
      DMX_LOG("is_video_track_encrypted_: %d\n", is_video_track_encrypted_);
      video_config_.Initialize(kCodecH264, H264PROFILE_MAIN,  VideoFrameFormat::YV12,
                              coded_size, visible_rect, natural_size,
                              // No decoder-specific buffer needed for AVC;
                              // SPS/PPS are embedded in the video stream
                              NULL, 0, is_video_track_encrypted_, true);
      has_video_ = true;
      video_track_id_ = track->header.track_id;
    }
  }

  //RCHECK(config_cb_.Run(audio_config, video_config));

  if (moov_->extends.header.fragment_duration > 0) {
    duration_ = MicrosecondsFromRational(moov_->extends.header.fragment_duration,
                                     moov_->header.timescale);
  } else if (moov_->header.duration > 0 &&
             moov_->header.duration != kuint64max) {
    duration_ = MicrosecondsFromRational(moov_->header.duration,
                                     moov_->header.timescale);
  } else {
    duration_ = InfiniteMicroseconds;
  }

  //if (!init_cb_.is_null())
  //  base::ResetAndReturn(&init_cb_).Run(true, duration);

  return true;
}

Microseconds
MP4Demuxer::Duration() const {
  return duration_;
}

bool MP4Demuxer::ParseMoof(BoxReader* reader) {
  RCHECK(state_ < kError);
  RCHECK(moov_.get());  // Must already have initialization segment
  MovieFragment moof;
  RCHECK(moof.Parse(reader));
  RCHECK(runs_->Init(moof));
  //new_segment_cb_.Run(runs_->GetMinDecodeTimestamp());
  ChangeState(kEmittingSamples);
  return true;
}

bool MP4Demuxer::ParseBox() {
  RCHECK(state_ < kError);
  bool err = false;
  nsAutoPtr<BoxReader> reader(BoxReader::ReadTopLevelBox(stream_,
                                                         stream_offset_,
                                                         &err));
  if (!reader || err) {
    DMX_LOG("Failed to read box at offset=%lld", stream_offset_);
    return false;
  }
  string type = FourCCToString(reader->type());

  DMX_LOG("offset=%lld version=0x%x flags=0x%x size=%d",
      stream_offset_, (uint32_t)reader->version(),
      reader->flags(), reader->size());

  if (reader->type() == FOURCC_MOOV) {
    DMX_LOG("ParseMoov\n");
    if (!ParseMoov(reader.get())) {
      DMX_LOG("ParseMoov failed\n");
      return false;
    }
  } else if (reader->type() == FOURCC_MOOF) {
    DMX_LOG("MOOF encountered\n.");
    moof_head_ = stream_offset_;
    if (!ParseMoof(reader.get())) {
      DMX_LOG("ParseMoof failed\n");
      return false;
    }
    mdat_tail_ = stream_offset_ + reader->size();
  }

  stream_offset_ += reader->size();

  return true;
}

bool MP4Demuxer::EmitSample(nsAutoPtr<MP4Sample>* sample) {
  bool ok = true;
  if (!runs_->IsRunValid()) {

    // Flush any buffers we've gotten in this chunk so that buffers don't
    // cross NewSegment() calls
    //ok = SendAndFlushSamples(/*audio_buffers, video_buffers*/);
    //if (!ok)
    //  return false;

    ChangeState(kParsingBoxes);
    //end_of_segment_cb_.Run();
    return true;
  }

  if (!runs_->IsSampleValid()) {
    runs_->AdvanceRun();
    return true;
  }

  bool audio = has_audio_ && audio_track_id_ == runs_->track_id();
  bool video = has_video_ && video_track_id_ == runs_->track_id();

  // Skip this entire track if it's not one we're interested in
  if (!audio && !video)
    runs_->AdvanceRun();

  // Attempt to cache the auxiliary information first. Aux info is usually
  // placed in a contiguous block before the sample data, rather than being
  // interleaved. If we didn't cache it, this would require that we retain the
  // start of the segment buffer while reading samples. Aux info is typically
  // quite small compared to sample data, so this pattern is useful on
  // memory-constrained devices where the source buffer consumes a substantial
  // portion of the total system memory.
  if (runs_->AuxInfoNeedsToBeCached()) {
    int64_t aux_info_offset = runs_->aux_info_offset() + moof_head_;
    if (stream_->Length() - aux_info_offset < runs_->aux_info_size()) {
      return false;
    }

    return runs_->CacheAuxInfo(stream_, moof_head_);
  }

  nsAutoPtr<DecryptConfig> decrypt_config;
  std::vector<SubsampleEntry> subsamples;
  if (runs_->is_encrypted()) {
    runs_->GetDecryptConfig(decrypt_config);
    subsamples = decrypt_config->subsamples();
  }

  nsAutoPtr<vector<uint8_t>> frame_buf(new vector<uint8_t>());
  const int64_t sample_offset = runs_->sample_offset() + moof_head_;
  StreamReader reader(stream_, sample_offset, runs_->sample_size());
  reader.ReadVec(frame_buf, runs_->sample_size());

  if (video) {
    if (!PrepareAVCBuffer(runs_->video_description().avcc,
                          frame_buf, &subsamples)) {
      DMX_LOG("Failed to prepare AVC sample for decode\n");
      return false;
    }
  }

  if (audio) {
    if (!PrepareAACBuffer(runs_->audio_description().esds.aac,
                          frame_buf, &subsamples)) {
      DMX_LOG("Failed to prepare AAC sample for decode\n");
      return false;
    }
  }

  const bool is_encrypted = (audio && is_audio_track_encrypted_) ||
                            (video && is_video_track_encrypted_);
  assert(runs_->is_encrypted() == is_encrypted);
  if (decrypt_config) {
    if (!subsamples.empty()) {
      // Create a new config with the updated subsamples.
      decrypt_config = new DecryptConfig(decrypt_config->key_id(),
                                         decrypt_config->iv(),
                                         decrypt_config->data_offset(),
                                         subsamples);
    }
    // else, use the existing config.
  } else if (is_encrypted) {
    // The media pipeline requires a DecryptConfig with an empty |iv|.
    // TODO(ddorwin): Refactor so we do not need a fake key ID ("1");
    decrypt_config = new DecryptConfig("1", "", 0, std::vector<SubsampleEntry>());
  }

  assert(audio || video);
  *sample = new MP4Sample(runs_->dts(),
                          runs_->cts(),
                          runs_->duration(),
                          sample_offset,
                          frame_buf.forget(),
                          audio ? kAudio : kVideo,
                          decrypt_config.forget(),
                          runs_->is_keyframe());
  runs_->AdvanceSample();
  return true;
}

bool MP4Demuxer::PrepareAVCBuffer(
    const AVCDecoderConfigurationRecord& avc_config,
    std::vector<uint8_t>* frame_buf,
    std::vector<SubsampleEntry>* subsamples) const {
  // Convert the AVC NALU length fields to Annex B headers, as expected by
  // decoding libraries. Since this may enlarge the size of the buffer, we also
  // update the clear byte count for each subsample if encryption is used to
  // account for the difference in size between the length prefix and Annex B
  // start code.
  RCHECK(AVC::ConvertFrameToAnnexB(avc_config.length_size, frame_buf));
  if (!subsamples->empty()) {
    const int nalu_size_diff = 4 - avc_config.length_size;
    size_t expected_size = runs_->sample_size() +
        subsamples->size() * nalu_size_diff;
    RCHECK(frame_buf->size() == expected_size);
    for (size_t i = 0; i < subsamples->size(); i++)
      (*subsamples)[i].clear_bytes += nalu_size_diff;
  }

  if (runs_->is_keyframe()) {
    // If this is a keyframe, we (re-)inject SPS and PPS headers at the start of
    // a frame. If subsample info is present, we also update the clear byte
    // count for that first subsample.
    std::vector<uint8_t> param_sets;
    RCHECK(AVC::ConvertConfigToAnnexB(avc_config, &param_sets));
    frame_buf->insert(frame_buf->begin(),
                      param_sets.begin(), param_sets.end());
    if (!subsamples->empty())
      (*subsamples)[0].clear_bytes += param_sets.size();
  }
  return true;
}

bool MP4Demuxer::PrepareAACBuffer(const AAC& aac_config,
                                  std::vector<uint8_t>* frame_buf,
                                  std::vector<SubsampleEntry>* subsamples) const {
  // Append an ADTS header to every audio sample.
  RCHECK(aac_config.ConvertEsdsToADTS(frame_buf));

  // As above, adjust subsample information to account for the headers. AAC is
  // not required to use subsample encryption, so we may need to add an entry.
  if (subsamples->empty()) {
    SubsampleEntry entry;
    entry.clear_bytes = AAC::kADTSHeaderSize;
    entry.cypher_bytes = frame_buf->size() - AAC::kADTSHeaderSize;
    subsamples->push_back(entry);
  } else {
    (*subsamples)[0].clear_bytes += AAC::kADTSHeaderSize;
  }
  return true;
}

// Reads the metadata boxes.
bool MP4Demuxer::Demux(nsAutoPtr<MP4Sample>* sample,
                       bool* end_of_stream)
{
  RCHECK(state_ < kError);
  assert(state_ > kWaitingForInit);
  *end_of_stream = false;

  const int64_t length = stream_->Length();
  bool ok = true;
  while (ok) {
    if (state_ == kParsingBoxes) {
      if (stream_offset_ < length) {
        ok = ParseBox();
      } else {
        DMX_LOG("End of stream reached.\n");
        *end_of_stream = true;
        break;
      }
    } else {
      DCHECK_EQ(kEmittingSamples, state_);
      ok = EmitSample(sample);
      if (ok && *sample) {
        // Got a sample, return.
        break;
      }
    }
  }

  if (!ok) {
    DMX_LOG("Error demuxing stream\n");
    ChangeState(kError);
    return false;
  }

  return true;
}

void MP4Demuxer::ChangeState(State new_state) {
  DMX_LOG("Demuxer changing state: %d\n", new_state);
  state_ = new_state;
  if (state_ == kError) {
    Reset();
  }
}

const AudioDecoderConfig&
MP4Demuxer::AudioConfig() const
{
  return audio_config_;
}

const VideoDecoderConfig&
MP4Demuxer::VideoConfig() const
{
  return video_config_;
}

bool
MP4Demuxer::HasAudio() const
{
  return has_audio_;
}

bool
MP4Demuxer::HasVideo() const
{
  return has_video_;
}

bool
MP4Demuxer::CanSeek() const
{
  return can_seek_;
}

} // namespace mp4_demuxer
