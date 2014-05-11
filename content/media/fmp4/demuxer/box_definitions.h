// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP4_BOX_DEFINITIONS_H_
#define MEDIA_MP4_BOX_DEFINITIONS_H_

#include <string>
#include <vector>

#include "mp4_demuxer/box_reader.h"

#include "mp4_demuxer/basictypes.h"
#include "mp4_demuxer/aac.h"
#include "mp4_demuxer/avc.h"
#include "mp4_demuxer/box_reader.h"
#include "mp4_demuxer/fourccs.h"

namespace mp4_demuxer {

enum TrackType {
  kInvalid = 0,
  kVideo,
  kAudio,
  kHint
};

#define DECLARE_BOX_METHODS(T) \
  T(); \
  virtual ~T(); \
  virtual bool Parse(BoxReader* reader) OVERRIDE; \
  virtual FourCC BoxType() const OVERRIDE; \

struct FileType : Box {
  DECLARE_BOX_METHODS(FileType);

  FourCC major_brand;
  uint32_t minor_version;
};

struct ProtectionSystemSpecificHeader : Box {
  DECLARE_BOX_METHODS(ProtectionSystemSpecificHeader);

  std::vector<uint8_t> system_id;
  std::vector<uint8_t> raw_box;
};

struct SampleAuxiliaryInformationOffset : Box {
  DECLARE_BOX_METHODS(SampleAuxiliaryInformationOffset);

  std::vector<uint64_t> offsets;
};

struct SampleAuxiliaryInformationSize : Box {
  DECLARE_BOX_METHODS(SampleAuxiliaryInformationSize);

  uint8_t default_sample_info_size;
  uint32_t sample_count;
  std::vector<uint8_t> sample_info_sizes;
};

struct OriginalFormat : Box {
  DECLARE_BOX_METHODS(OriginalFormat);

  FourCC format;
};

struct SchemeType : Box {
  DECLARE_BOX_METHODS(SchemeType);

  FourCC type;
  uint32_t version;
};

struct TrackEncryption : Box {
  DECLARE_BOX_METHODS(TrackEncryption);

  // Note: this definition is specific to the CENC protection type.
  bool is_encrypted;
  uint8_t default_iv_size;
  std::vector<uint8_t> default_kid;
};

struct SchemeInfo : Box {
  DECLARE_BOX_METHODS(SchemeInfo);

  TrackEncryption track_encryption;
};

struct ProtectionSchemeInfo : Box {
  DECLARE_BOX_METHODS(ProtectionSchemeInfo);

  OriginalFormat format;
  SchemeType type;
  SchemeInfo info;
};

struct MovieHeader : Box {
  DECLARE_BOX_METHODS(MovieHeader);

  uint64_t creation_time;
  uint64_t modification_time;
  uint32_t timescale;
  uint64_t duration;
  int32_t rate;
  int16_t volume;
  uint32_t next_track_id;
};

struct TrackHeader : Box {
  DECLARE_BOX_METHODS(TrackHeader);

  uint64_t creation_time;
  uint64_t modification_time;
  uint32_t track_id;
  uint64_t duration;
  int16_t layer;
  int16_t alternate_group;
  int16_t volume;
  uint32_t width;
  uint32_t height;
};

struct EditListEntry {
  uint64_t segment_duration;
  int64_t media_time;
  int16_t media_rate_integer;
  int16_t media_rate_fraction;
};

struct EditList : Box {
  DECLARE_BOX_METHODS(EditList);

  std::vector<EditListEntry> edits;
};

struct Edit : Box {
  DECLARE_BOX_METHODS(Edit);

  EditList list;
};

struct HandlerReference : Box {
  DECLARE_BOX_METHODS(HandlerReference);

  TrackType type;
};

struct AVCDecoderConfigurationRecord : Box {
  DECLARE_BOX_METHODS(AVCDecoderConfigurationRecord);

  uint8_t version;
  uint8_t profile_indication;
  uint8_t profile_compatibility;
  uint8_t avc_level;
  uint8_t length_size;

  typedef std::vector<uint8_t> SPS;
  typedef std::vector<uint8_t> PPS;

  std::vector<SPS> sps_list;
  std::vector<PPS> pps_list;
};

struct PixelAspectRatioBox : Box {
  DECLARE_BOX_METHODS(PixelAspectRatioBox);

  uint32_t h_spacing;
  uint32_t v_spacing;
};

struct VideoSampleEntry : Box {
  DECLARE_BOX_METHODS(VideoSampleEntry);

  FourCC format;
  uint16_t data_reference_index;
  uint16_t width;
  uint16_t height;

  PixelAspectRatioBox pixel_aspect;
  ProtectionSchemeInfo sinf;

  // Currently expected to be present regardless of format.
  AVCDecoderConfigurationRecord avcc;
};

struct ElementaryStreamDescriptor : Box {
  DECLARE_BOX_METHODS(ElementaryStreamDescriptor);

  uint8_t object_type;
  AAC aac;
};

struct AudioSampleEntry : Box {
  DECLARE_BOX_METHODS(AudioSampleEntry);

  FourCC format;
  uint16_t data_reference_index;
  uint16_t channelcount;
  uint16_t samplesize;
  uint32_t samplerate;

  ProtectionSchemeInfo sinf;
  ElementaryStreamDescriptor esds;
};

struct SampleDescription : Box {
  DECLARE_BOX_METHODS(SampleDescription);

  TrackType type;
  std::vector<VideoSampleEntry> video_entries;
  std::vector<AudioSampleEntry> audio_entries;
};

struct SampleTable : Box {
  DECLARE_BOX_METHODS(SampleTable);

  // Media Source specific: we ignore many of the sub-boxes in this box,
  // including some that are required to be present in the BMFF spec. This
  // includes the 'stts', 'stsc', and 'stco' boxes, which must contain no
  // samples in order to be compliant files.
  SampleDescription description;
};

struct MediaHeader : Box {
  DECLARE_BOX_METHODS(MediaHeader);

  uint64_t creation_time;
  uint64_t modification_time;
  uint32_t timescale;
  uint64_t duration;
};

struct MediaInformation : Box {
  DECLARE_BOX_METHODS(MediaInformation);

  SampleTable sample_table;
};

struct Media : Box {
  DECLARE_BOX_METHODS(Media);

  MediaHeader header;
  HandlerReference handler;
  MediaInformation information;
};

struct Track : Box {
  DECLARE_BOX_METHODS(Track);

  TrackHeader header;
  Media media;
  Edit edit;
};

struct MovieExtendsHeader : Box {
  DECLARE_BOX_METHODS(MovieExtendsHeader);

  uint64_t fragment_duration;
};

struct TrackExtends : Box {
  DECLARE_BOX_METHODS(TrackExtends);

  uint32_t track_id;
  uint32_t default_sample_description_index;
  uint32_t default_sample_duration;
  uint32_t default_sample_size;
  uint32_t default_sample_flags;
};

struct MovieExtends : Box {
  DECLARE_BOX_METHODS(MovieExtends);

  MovieExtendsHeader header;
  std::vector<TrackExtends> tracks;
};

struct Movie : Box {
  DECLARE_BOX_METHODS(Movie);

  bool fragmented;
  MovieHeader header;
  MovieExtends extends;
  std::vector<Track> tracks;
  std::vector<ProtectionSystemSpecificHeader> pssh;
};

struct TrackFragmentDecodeTime : Box {
  DECLARE_BOX_METHODS(TrackFragmentDecodeTime);

  uint64_t decode_time;
};

struct MovieFragmentHeader : Box {
  DECLARE_BOX_METHODS(MovieFragmentHeader);

  uint32_t sequence_number;
};

struct TrackFragmentHeader : Box {
  DECLARE_BOX_METHODS(TrackFragmentHeader);

  uint32_t track_id;

  uint32_t sample_description_index;
  uint32_t default_sample_duration;
  uint32_t default_sample_size;
  uint32_t default_sample_flags;

  // As 'flags' might be all zero, we cannot use zeroness alone to identify
  // when default_sample_flags wasn't specified, unlike the other values.
  bool has_default_sample_flags;
};

struct TrackFragmentRun : Box {
  DECLARE_BOX_METHODS(TrackFragmentRun);

  uint32_t sample_count;
  uint32_t data_offset;
  std::vector<uint32_t> sample_flags;
  std::vector<uint32_t> sample_sizes;
  std::vector<uint32_t> sample_durations;
  std::vector<int32_t> sample_composition_time_offsets;
};

struct TrackFragment : Box {
  DECLARE_BOX_METHODS(TrackFragment);

  TrackFragmentHeader header;
  std::vector<TrackFragmentRun> runs;
  TrackFragmentDecodeTime decode_time;
  SampleAuxiliaryInformationOffset auxiliary_offset;
  SampleAuxiliaryInformationSize auxiliary_size;
};

struct MovieFragment : Box {
  DECLARE_BOX_METHODS(MovieFragment);

  MovieFragmentHeader header;
  std::vector<TrackFragment> tracks;
  std::vector<ProtectionSystemSpecificHeader> pssh;
};

#undef DECLARE_BOX

}  // namespace mp4_demuxer

#endif  // MEDIA_MP4_BOX_DEFINITIONS_H_
