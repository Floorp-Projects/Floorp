// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP4_MP4DEMUXER_H
#define MEDIA_MP4_MP4DEMUXER_H

#include "mp4_demuxer/audio_decoder_config.h"
#include "mp4_demuxer/video_decoder_config.h"
#include "mp4_demuxer/decrypt_config.h"
#include "mp4_demuxer/box_definitions.h"


#include "nsAutoPtr.h"
#include <memory>

namespace mp4_demuxer {

class Stream;
class BoxReader;
struct Movie;
class TrackRunIterator;
struct AVCDecoderConfigurationRecord;
class AAC;

// Constructs an MP4 Sample. Note this assumes ownership of the |data| vector
// passed in.
struct MP4Sample {
  MP4Sample(Microseconds decode_timestamp,
            Microseconds composition_timestamp,
            Microseconds duration,
            int64_t byte_offset,
            std::vector<uint8_t>* data,
            TrackType type,
            DecryptConfig* decrypt_config,
            bool is_sync_point);
  ~MP4Sample();

  const Microseconds decode_timestamp;

  const Microseconds composition_timestamp;

  const Microseconds duration;

  // Offset of sample in byte stream.
  const int64_t byte_offset;

  // Raw demuxed data.
  const nsAutoPtr<std::vector<uint8_t>> data;

  // Is this an audio or video sample?
  const TrackType type;

  const nsAutoPtr<DecryptConfig> decrypt_config;

  // Whether this is a keyframe or not.
  const bool is_sync_point;

  bool is_encrypted() const;
};

class MP4Demuxer {
public:
  MP4Demuxer(Stream* stream);
  ~MP4Demuxer();

  bool Init();

  // Reads the metadata boxes, up to the first fragment.
  bool Demux(nsAutoPtr<MP4Sample>* sample,
             bool* end_of_stream);

  bool HasAudio() const;
  const AudioDecoderConfig& AudioConfig() const;

  bool HasVideo() const;
  const VideoDecoderConfig& VideoConfig() const;

  Microseconds Duration() const;

  bool CanSeek() const;

private:

  enum State {
    kWaitingForInit,
    kParsingBoxes,
    kEmittingSamples,
    kError
  };

  // Parses the bitstream. Returns false on error.
  bool Parse(nsAutoPtr<MP4Sample>* sample,
             bool& end_of_stream);

  void ChangeState(State new_state);

  // Return true on success, false on failure.
  bool ParseBox();
  bool ParseMoov(BoxReader* reader);
  bool ParseMoof(BoxReader* reader);

  void Reset();

  bool EmitSample(nsAutoPtr<MP4Sample>* sample);

  bool PrepareAACBuffer(const AAC& aac_config,
                        std::vector<uint8_t>* frame_buf,
                        std::vector<SubsampleEntry>* subsamples) const;

  bool PrepareAVCBuffer(const AVCDecoderConfigurationRecord& avc_config,
                        std::vector<uint8_t>* frame_buf,
                        std::vector<SubsampleEntry>* subsamples) const;

  State state_;
  
  // Stream abstraction that we read from. It is the responsibility of the
  // owner of the demuxer to ensure that it stays alive for the lifetime
  // of the demuxer.
  Stream* stream_;
  int64_t stream_offset_;

  Microseconds duration_;

  // These two parameters are only valid in the |kEmittingSegments| state.
  //
  // |moof_head_| is the offset of the start of the most recently parsed moof
  // block. All byte offsets in sample information are relative to this offset,
  // as mandated by the Media Source spec.
  int64_t moof_head_;
  // |mdat_tail_| is the stream offset of the end of the current 'mdat' box.
  // Valid iff it is greater than the head of the queue.
  int64_t mdat_tail_;

  nsAutoPtr<Movie> moov_;
  nsAutoPtr<TrackRunIterator> runs_;

  uint32_t audio_track_id_;
  uint32_t video_track_id_;

  uint32_t audio_frameno;
  uint32_t video_frameno;

  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;

  bool has_audio_;
  bool has_sbr_; // NOTE: This is not initialized!
  bool is_audio_track_encrypted_;

  bool has_video_;
  bool is_video_track_encrypted_;

  bool can_seek_;
};

} // mp4_demuxer

#endif // MEDIA_MP4_MP4DEMUXER_H
