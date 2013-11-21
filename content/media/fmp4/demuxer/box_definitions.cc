// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mp4_demuxer/box_definitions.h"
#include "mp4_demuxer/es_descriptor.h"

#include <iostream>

namespace mp4_demuxer {

FileType::FileType() {}
FileType::~FileType() {}
FourCC FileType::BoxType() const { return FOURCC_FTYP; }

bool FileType::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFourCC(&major_brand) && reader->Read4(&minor_version));
  size_t num_brands = (reader->size() - reader->pos()) / sizeof(FourCC);
  return reader->SkipBytes(sizeof(FourCC) * num_brands);  // compatible_brands
}

ProtectionSystemSpecificHeader::ProtectionSystemSpecificHeader() {}
ProtectionSystemSpecificHeader::~ProtectionSystemSpecificHeader() {}
FourCC ProtectionSystemSpecificHeader::BoxType() const { return FOURCC_PSSH; }

bool ProtectionSystemSpecificHeader::Parse(BoxReader* reader) {
  // Validate the box's contents and hang on to the system ID.
  uint32_t size;
  RCHECK(reader->ReadFullBoxHeader() &&
         reader->ReadVec(&system_id, 16) &&
         reader->Read4(&size) &&
         reader->HasBytes(size));

  // Copy the entire box, including the header, for passing to EME as initData.
  DCHECK(raw_box.empty());
  reader->ReadVec(&raw_box, size);
  //raw_box.assign(reader->data(), reader->data() + size);
  return true;
}

SampleAuxiliaryInformationOffset::SampleAuxiliaryInformationOffset() {}
SampleAuxiliaryInformationOffset::~SampleAuxiliaryInformationOffset() {}
FourCC SampleAuxiliaryInformationOffset::BoxType() const { return FOURCC_SAIO; }

bool SampleAuxiliaryInformationOffset::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  if (reader->flags() & 1)
    RCHECK(reader->SkipBytes(8));

  uint32_t count;
  RCHECK(reader->Read4(&count) &&
         reader->HasBytes(count * (reader->version() == 1 ? 8 : 4)));
  offsets.resize(count);

  for (uint32_t i = 0; i < count; i++) {
    if (reader->version() == 1) {
      RCHECK(reader->Read8(&offsets[i]));
    } else {
      RCHECK(reader->Read4Into8(&offsets[i]));
    }
  }
  return true;
}

SampleAuxiliaryInformationSize::SampleAuxiliaryInformationSize()
  : default_sample_info_size(0), sample_count(0) {
}
SampleAuxiliaryInformationSize::~SampleAuxiliaryInformationSize() {}
FourCC SampleAuxiliaryInformationSize::BoxType() const { return FOURCC_SAIZ; }

bool SampleAuxiliaryInformationSize::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  if (reader->flags() & 1)
    RCHECK(reader->SkipBytes(8));

  RCHECK(reader->Read1(&default_sample_info_size) &&
         reader->Read4(&sample_count));
  if (default_sample_info_size == 0)
    return reader->ReadVec(&sample_info_sizes, sample_count);
  return true;
}

OriginalFormat::OriginalFormat() : format(FOURCC_NULL) {}
OriginalFormat::~OriginalFormat() {}
FourCC OriginalFormat::BoxType() const { return FOURCC_FRMA; }

bool OriginalFormat::Parse(BoxReader* reader) {
  return reader->ReadFourCC(&format);
}

SchemeType::SchemeType() : type(FOURCC_NULL), version(0) {}
SchemeType::~SchemeType() {}
FourCC SchemeType::BoxType() const { return FOURCC_SCHM; }

bool SchemeType::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader() &&
         reader->ReadFourCC(&type) &&
         reader->Read4(&version));
  return true;
}

TrackEncryption::TrackEncryption()
  : is_encrypted(false), default_iv_size(0) {
}
TrackEncryption::~TrackEncryption() {}
FourCC TrackEncryption::BoxType() const { return FOURCC_TENC; }

bool TrackEncryption::Parse(BoxReader* reader) {
  uint8_t flag;
  RCHECK(reader->ReadFullBoxHeader() &&
         reader->SkipBytes(2) &&
         reader->Read1(&flag) &&
         reader->Read1(&default_iv_size) &&
         reader->ReadVec(&default_kid, 16));
  is_encrypted = (flag != 0);
  if (is_encrypted) {
    RCHECK(default_iv_size == 8 || default_iv_size == 16);
  } else {
    RCHECK(default_iv_size == 0);
  }
  return true;
}

SchemeInfo::SchemeInfo() {}
SchemeInfo::~SchemeInfo() {}
FourCC SchemeInfo::BoxType() const { return FOURCC_SCHI; }

bool SchemeInfo::Parse(BoxReader* reader) {
  return reader->ScanChildren() && reader->ReadChild(&track_encryption);
}

ProtectionSchemeInfo::ProtectionSchemeInfo() {}
ProtectionSchemeInfo::~ProtectionSchemeInfo() {}
FourCC ProtectionSchemeInfo::BoxType() const { return FOURCC_SINF; }

bool ProtectionSchemeInfo::Parse(BoxReader* reader) {
  RCHECK(reader->ScanChildren() &&
         reader->ReadChild(&format) &&
         reader->ReadChild(&type));
  if (type.type == FOURCC_CENC)
    RCHECK(reader->ReadChild(&info));
  // Other protection schemes are silently ignored. Since the protection scheme
  // type can't be determined until this box is opened, we return 'true' for
  // non-CENC protection scheme types. It is the parent box's responsibility to
  // ensure that this scheme type is a supported one.
  return true;
}

MovieHeader::MovieHeader()
    : creation_time(0),
      modification_time(0),
      timescale(0),
      duration(0),
      rate(-1),
      volume(-1),
      next_track_id(0) {}
MovieHeader::~MovieHeader() {}
FourCC MovieHeader::BoxType() const { return FOURCC_MVHD; }

bool MovieHeader::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());

  if (reader->version() == 1) {
    RCHECK(reader->Read8(&creation_time) &&
           reader->Read8(&modification_time) &&
           reader->Read4(&timescale) &&
           reader->Read8(&duration));
  } else {
    RCHECK(reader->Read4Into8(&creation_time) &&
           reader->Read4Into8(&modification_time) &&
           reader->Read4(&timescale) &&
           reader->Read4Into8(&duration));
  }

  RCHECK(reader->Read4s(&rate) &&
         reader->Read2s(&volume) &&
         reader->SkipBytes(10) &&  // reserved
         reader->SkipBytes(36) &&  // matrix
         reader->SkipBytes(24) &&  // predefined zero
         reader->Read4(&next_track_id));
  return true;
}

TrackHeader::TrackHeader()
    : creation_time(0),
      modification_time(0),
      track_id(0),
      duration(0),
      layer(-1),
      alternate_group(-1),
      volume(-1),
      width(0),
      height(0) {}
TrackHeader::~TrackHeader() {}
FourCC TrackHeader::BoxType() const { return FOURCC_TKHD; }

bool TrackHeader::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  if (reader->version() == 1) {
    RCHECK(reader->Read8(&creation_time) &&
           reader->Read8(&modification_time) &&
           reader->Read4(&track_id) &&
           reader->SkipBytes(4) &&    // reserved
           reader->Read8(&duration));
  } else {
    RCHECK(reader->Read4Into8(&creation_time) &&
           reader->Read4Into8(&modification_time) &&
           reader->Read4(&track_id) &&
           reader->SkipBytes(4) &&   // reserved
           reader->Read4Into8(&duration));
  }

  RCHECK(reader->SkipBytes(8) &&  // reserved
         reader->Read2s(&layer) &&
         reader->Read2s(&alternate_group) &&
         reader->Read2s(&volume) &&
         reader->SkipBytes(2) &&  // reserved
         reader->SkipBytes(36) &&  // matrix
         reader->Read4(&width) &&
         reader->Read4(&height));
  width >>= 16;
  height >>= 16;
  return true;
}

SampleDescription::SampleDescription() : type(kInvalid) {}
SampleDescription::~SampleDescription() {}
FourCC SampleDescription::BoxType() const { return FOURCC_STSD; }

bool SampleDescription::Parse(BoxReader* reader) {
  uint32_t count;
  RCHECK(reader->SkipBytes(4) &&
         reader->Read4(&count));
  video_entries.clear();
  audio_entries.clear();

  // Note: this value is preset before scanning begins. See comments in the
  // Parse(Media*) function.
  if (type == kVideo) {
    RCHECK(reader->ReadAllChildren(&video_entries));
  } else if (type == kAudio) {
    RCHECK(reader->ReadAllChildren(&audio_entries));
  }
  return true;
}

SampleTable::SampleTable() {}
SampleTable::~SampleTable() {}
FourCC SampleTable::BoxType() const { return FOURCC_STBL; }

bool SampleTable::Parse(BoxReader* reader) {
  return reader->ScanChildren() &&
         reader->ReadChild(&description);
}

EditList::EditList() {}
EditList::~EditList() {}
FourCC EditList::BoxType() const { return FOURCC_ELST; }

bool EditList::Parse(BoxReader* reader) {
  uint32_t count;
  RCHECK(reader->ReadFullBoxHeader() && reader->Read4(&count));

  if (reader->version() == 1) {
    RCHECK(reader->HasBytes(count * 20));
  } else {
    RCHECK(reader->HasBytes(count * 12));
  }
  edits.resize(count);

  for (std::vector<EditListEntry>::iterator edit = edits.begin();
       edit != edits.end(); ++edit) {
    if (reader->version() == 1) {
      RCHECK(reader->Read8(&edit->segment_duration) &&
             reader->Read8s(&edit->media_time));
    } else {
      RCHECK(reader->Read4Into8(&edit->segment_duration) &&
             reader->Read4sInto8s(&edit->media_time));
    }
    RCHECK(reader->Read2s(&edit->media_rate_integer) &&
           reader->Read2s(&edit->media_rate_fraction));
  }
  return true;
}

Edit::Edit() {}
Edit::~Edit() {}
FourCC Edit::BoxType() const { return FOURCC_EDTS; }

bool Edit::Parse(BoxReader* reader) {
  return reader->ScanChildren() && reader->ReadChild(&list);
}

HandlerReference::HandlerReference() : type(kInvalid) {}
HandlerReference::~HandlerReference() {}
FourCC HandlerReference::BoxType() const { return FOURCC_HDLR; }

bool HandlerReference::Parse(BoxReader* reader) {
  FourCC hdlr_type;
  RCHECK(reader->SkipBytes(8) && reader->ReadFourCC(&hdlr_type));
  // Note: remaining fields in box ignored
  if (hdlr_type == FOURCC_VIDE) {
    type = kVideo;
  } else if (hdlr_type == FOURCC_SOUN) {
    type = kAudio;
  } else {
    type = kInvalid;
  }
  return true;
}

AVCDecoderConfigurationRecord::AVCDecoderConfigurationRecord()
    : version(0),
      profile_indication(0),
      profile_compatibility(0),
      avc_level(0),
      length_size(0) {}

AVCDecoderConfigurationRecord::~AVCDecoderConfigurationRecord() {}
FourCC AVCDecoderConfigurationRecord::BoxType() const { return FOURCC_AVCC; }

bool AVCDecoderConfigurationRecord::Parse(BoxReader* reader) {
  RCHECK(reader->Read1(&version) && version == 1 &&
         reader->Read1(&profile_indication) &&
         reader->Read1(&profile_compatibility) &&
         reader->Read1(&avc_level));

  uint8_t length_size_minus_one;
  RCHECK(reader->Read1(&length_size_minus_one) &&
         (length_size_minus_one & 0xfc) == 0xfc);
  length_size = (length_size_minus_one & 0x3) + 1;

  uint8_t num_sps;
  RCHECK(reader->Read1(&num_sps) && (num_sps & 0xe0) == 0xe0);
  num_sps &= 0x1f;

  sps_list.resize(num_sps);
  for (int i = 0; i < num_sps; i++) {
    uint16_t sps_length;
    RCHECK(reader->Read2(&sps_length) &&
           reader->ReadVec(&sps_list[i], sps_length));
  }

  uint8_t num_pps;
  RCHECK(reader->Read1(&num_pps));

  pps_list.resize(num_pps);
  for (int i = 0; i < num_pps; i++) {
    uint16_t pps_length;
    RCHECK(reader->Read2(&pps_length) &&
           reader->ReadVec(&pps_list[i], pps_length));
  }

  return true;
}

PixelAspectRatioBox::PixelAspectRatioBox() : h_spacing(1), v_spacing(1) {}
PixelAspectRatioBox::~PixelAspectRatioBox() {}
FourCC PixelAspectRatioBox::BoxType() const { return FOURCC_PASP; }

bool PixelAspectRatioBox::Parse(BoxReader* reader) {
  RCHECK(reader->Read4(&h_spacing) &&
         reader->Read4(&v_spacing));
  return true;
}

VideoSampleEntry::VideoSampleEntry()
    : format(FOURCC_NULL),
      data_reference_index(0),
      width(0),
      height(0) {}

VideoSampleEntry::~VideoSampleEntry() {}
FourCC VideoSampleEntry::BoxType() const {
  DMX_LOG("VideoSampleEntry should be parsed according to the "
      "handler type recovered in its Media ancestor.\n");
  return FOURCC_NULL;
}

bool VideoSampleEntry::Parse(BoxReader* reader) {
  format = reader->type();
  RCHECK(reader->SkipBytes(6) &&
         reader->Read2(&data_reference_index) &&
         reader->SkipBytes(16) &&
         reader->Read2(&width) &&
         reader->Read2(&height) &&
         reader->SkipBytes(50));

  RCHECK(reader->ScanChildren() &&
         reader->MaybeReadChild(&pixel_aspect));

  if (format == FOURCC_ENCV) {
    // Continue scanning until a recognized protection scheme is found, or until
    // we run out of protection schemes.
    while (sinf.type.type != FOURCC_CENC) {
      if (!reader->ReadChild(&sinf))
        return false;
    }
  }

  if (format == FOURCC_AVC1 ||
      (format == FOURCC_ENCV && sinf.format.format == FOURCC_AVC1)) {
    RCHECK(reader->ReadChild(&avcc));
  }
  return true;
}

ElementaryStreamDescriptor::ElementaryStreamDescriptor()
    : object_type(kForbidden) {}

ElementaryStreamDescriptor::~ElementaryStreamDescriptor() {}

FourCC ElementaryStreamDescriptor::BoxType() const {
  return FOURCC_ESDS;
}

bool ElementaryStreamDescriptor::Parse(BoxReader* reader) {
  std::vector<uint8_t> data;
  ESDescriptor es_desc;

  RCHECK(reader->ReadFullBoxHeader());
  RCHECK(reader->ReadVec(&data, reader->size() - reader->pos()));
  RCHECK(es_desc.Parse(data));

  object_type = es_desc.object_type();

  RCHECK(aac.Parse(es_desc.decoder_specific_info()));

  return true;
}

AudioSampleEntry::AudioSampleEntry()
    : format(FOURCC_NULL),
      data_reference_index(0),
      channelcount(0),
      samplesize(0),
      samplerate(0) {}

AudioSampleEntry::~AudioSampleEntry() {}

FourCC AudioSampleEntry::BoxType() const {
  DMX_LOG("AudioSampleEntry should be parsed according to the "
      "handler type recovered in its Media ancestor.\n");
  return FOURCC_NULL;
}

bool AudioSampleEntry::Parse(BoxReader* reader) {
  format = reader->type();
  RCHECK(reader->SkipBytes(6) &&
         reader->Read2(&data_reference_index) &&
         reader->SkipBytes(8) &&
         reader->Read2(&channelcount) &&
         reader->Read2(&samplesize) &&
         reader->SkipBytes(4) &&
         reader->Read4(&samplerate));
  // Convert from 16.16 fixed point to integer
  samplerate >>= 16;

  RCHECK(reader->ScanChildren());
  if (format == FOURCC_ENCA) {
    // Continue scanning until a recognized protection scheme is found, or until
    // we run out of protection schemes.
    while (sinf.type.type != FOURCC_CENC) {
      if (!reader->ReadChild(&sinf))
        return false;
    }
  }

  RCHECK(reader->ReadChild(&esds));
  return true;
}

MediaHeader::MediaHeader()
    : creation_time(0),
      modification_time(0),
      timescale(0),
      duration(0) {}
MediaHeader::~MediaHeader() {}
FourCC MediaHeader::BoxType() const { return FOURCC_MDHD; }

bool MediaHeader::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());

  if (reader->version() == 1) {
    RCHECK(reader->Read8(&creation_time) &&
           reader->Read8(&modification_time) &&
           reader->Read4(&timescale) &&
           reader->Read8(&duration));
  } else {
    RCHECK(reader->Read4Into8(&creation_time) &&
           reader->Read4Into8(&modification_time) &&
           reader->Read4(&timescale) &&
           reader->Read4Into8(&duration));
  }
  // Skip language information
  return reader->SkipBytes(4);
}

MediaInformation::MediaInformation() {}
MediaInformation::~MediaInformation() {}
FourCC MediaInformation::BoxType() const { return FOURCC_MINF; }

bool MediaInformation::Parse(BoxReader* reader) {
  return reader->ScanChildren() &&
         reader->ReadChild(&sample_table);
}

Media::Media() {}
Media::~Media() {}
FourCC Media::BoxType() const { return FOURCC_MDIA; }

bool Media::Parse(BoxReader* reader) {
  RCHECK(reader->ScanChildren() &&
         reader->ReadChild(&header) &&
         reader->ReadChild(&handler));

  // Maddeningly, the HandlerReference box specifies how to parse the
  // SampleDescription box, making the latter the only box (of those that we
  // support) which cannot be parsed correctly on its own (or even with
  // information from its strict ancestor tree). We thus copy the handler type
  // to the sample description box *before* parsing it to provide this
  // information while parsing.
  information.sample_table.description.type = handler.type;
  RCHECK(reader->ReadChild(&information));
  return true;
}

Track::Track() {}
Track::~Track() {}
FourCC Track::BoxType() const { return FOURCC_TRAK; }

bool Track::Parse(BoxReader* reader) {
  RCHECK(reader->ScanChildren() &&
         reader->ReadChild(&header) &&
         reader->ReadChild(&media) &&
         reader->MaybeReadChild(&edit));
  return true;
}

MovieExtendsHeader::MovieExtendsHeader() : fragment_duration(0) {}
MovieExtendsHeader::~MovieExtendsHeader() {}
FourCC MovieExtendsHeader::BoxType() const { return FOURCC_MEHD; }

bool MovieExtendsHeader::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  if (reader->version() == 1) {
    RCHECK(reader->Read8(&fragment_duration));
  } else {
    RCHECK(reader->Read4Into8(&fragment_duration));
  }
  return true;
}

TrackExtends::TrackExtends()
    : track_id(0),
      default_sample_description_index(0),
      default_sample_duration(0),
      default_sample_size(0),
      default_sample_flags(0) {}
TrackExtends::~TrackExtends() {}
FourCC TrackExtends::BoxType() const { return FOURCC_TREX; }

bool TrackExtends::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader() &&
         reader->Read4(&track_id) &&
         reader->Read4(&default_sample_description_index) &&
         reader->Read4(&default_sample_duration) &&
         reader->Read4(&default_sample_size) &&
         reader->Read4(&default_sample_flags));
  return true;
}

MovieExtends::MovieExtends() {}
MovieExtends::~MovieExtends() {}
FourCC MovieExtends::BoxType() const { return FOURCC_MVEX; }

bool MovieExtends::Parse(BoxReader* reader) {
  header.fragment_duration = 0;
  return reader->ScanChildren() &&
         reader->MaybeReadChild(&header) &&
         reader->ReadChildren(&tracks);
}

Movie::Movie() : fragmented(false) {}
Movie::~Movie() {}
FourCC Movie::BoxType() const { return FOURCC_MOOV; }

bool Movie::Parse(BoxReader* reader) {
  return reader->ScanChildren() &&
         reader->ReadChild(&header) &&
         reader->ReadChildren(&tracks) &&
         // Media Source specific: 'mvex' required
         reader->ReadChild(&extends) &&
         reader->MaybeReadChildren(&pssh);
}

TrackFragmentDecodeTime::TrackFragmentDecodeTime() : decode_time(0) {}
TrackFragmentDecodeTime::~TrackFragmentDecodeTime() {}
FourCC TrackFragmentDecodeTime::BoxType() const { return FOURCC_TFDT; }

bool TrackFragmentDecodeTime::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  if (reader->version() == 1)
    return reader->Read8(&decode_time);
  else
    return reader->Read4Into8(&decode_time);
}

MovieFragmentHeader::MovieFragmentHeader() : sequence_number(0) {}
MovieFragmentHeader::~MovieFragmentHeader() {}
FourCC MovieFragmentHeader::BoxType() const { return FOURCC_MFHD; }

bool MovieFragmentHeader::Parse(BoxReader* reader) {
  return reader->SkipBytes(4) && reader->Read4(&sequence_number);
}

TrackFragmentHeader::TrackFragmentHeader()
    : track_id(0),
      sample_description_index(0),
      default_sample_duration(0),
      default_sample_size(0),
      default_sample_flags(0),
      has_default_sample_flags(false) {}

TrackFragmentHeader::~TrackFragmentHeader() {}
FourCC TrackFragmentHeader::BoxType() const { return FOURCC_TFHD; }

bool TrackFragmentHeader::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader() && reader->Read4(&track_id));

  // Media Source specific: reject tracks that set 'base-data-offset-present'.
  // Although the Media Source requires that 'default-base-is-moof' (14496-12
  // Amendment 2) be set, we omit this check as many otherwise-valid files in
  // the wild don't set it.
  //
  //  RCHECK((flags & 0x020000) && !(flags & 0x1));
  RCHECK(!(reader->flags() & 0x1));

  if (reader->flags() & 0x2) {
    RCHECK(reader->Read4(&sample_description_index));
  } else {
    sample_description_index = 0;
  }

  if (reader->flags() & 0x8) {
    RCHECK(reader->Read4(&default_sample_duration));
  } else {
    default_sample_duration = 0;
  }

  if (reader->flags() & 0x10) {
    RCHECK(reader->Read4(&default_sample_size));
  } else {
    default_sample_size = 0;
  }

  if (reader->flags() & 0x20) {
    RCHECK(reader->Read4(&default_sample_flags));
    has_default_sample_flags = true;
  } else {
    has_default_sample_flags = false;
  }

  return true;
}

TrackFragmentRun::TrackFragmentRun()
    : sample_count(0), data_offset(0) {}
TrackFragmentRun::~TrackFragmentRun() {}
FourCC TrackFragmentRun::BoxType() const { return FOURCC_TRUN; }

bool TrackFragmentRun::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader() &&
         reader->Read4(&sample_count));
  const uint32_t flags = reader->flags();

  bool data_offset_present = (flags & 0x1) != 0;
  bool first_sample_flags_present = (flags & 0x4) != 0;
  bool sample_duration_present = (flags & 0x100) != 0;
  bool sample_size_present = (flags & 0x200) != 0;
  bool sample_flags_present = (flags & 0x400) != 0;
  bool sample_composition_time_offsets_present = (flags & 0x800) != 0;

  if (data_offset_present) {
    RCHECK(reader->Read4(&data_offset));
  } else {
    data_offset = 0;
  }

  uint32_t first_sample_flags;
  if (first_sample_flags_present)
    RCHECK(reader->Read4(&first_sample_flags));

  int fields = sample_duration_present + sample_size_present +
      sample_flags_present + sample_composition_time_offsets_present;
  RCHECK(reader->HasBytes(fields * sample_count));

  if (sample_duration_present)
    sample_durations.resize(sample_count);
  if (sample_size_present)
    sample_sizes.resize(sample_count);
  if (sample_flags_present)
    sample_flags.resize(sample_count);
  if (sample_composition_time_offsets_present)
    sample_composition_time_offsets.resize(sample_count);

  for (uint32_t i = 0; i < sample_count; ++i) {
    if (sample_duration_present)
      RCHECK(reader->Read4(&sample_durations[i]));
    if (sample_size_present)
      RCHECK(reader->Read4(&sample_sizes[i]));
    if (sample_flags_present)
      RCHECK(reader->Read4(&sample_flags[i]));
    if (sample_composition_time_offsets_present)
      RCHECK(reader->Read4s(&sample_composition_time_offsets[i]));
  }

  if (first_sample_flags_present) {
    if (sample_flags.size() == 0) {
      sample_flags.push_back(first_sample_flags);
    } else {
      sample_flags[0] = first_sample_flags;
    }
  }
  return true;
}

TrackFragment::TrackFragment() {}
TrackFragment::~TrackFragment() {}
FourCC TrackFragment::BoxType() const { return FOURCC_TRAF; }

bool TrackFragment::Parse(BoxReader* reader) {
  return reader->ScanChildren() &&
         reader->ReadChild(&header) &&
         // Media Source specific: 'tfdt' required
         reader->ReadChild(&decode_time) &&
         reader->MaybeReadChildren(&runs) &&
         reader->MaybeReadChild(&auxiliary_offset) &&
         reader->MaybeReadChild(&auxiliary_size);
}

MovieFragment::MovieFragment() {}
MovieFragment::~MovieFragment() {}
FourCC MovieFragment::BoxType() const { return FOURCC_MOOF; }

bool MovieFragment::Parse(BoxReader* reader) {
  RCHECK(reader->ScanChildren() &&
         reader->ReadChild(&header) &&
         reader->ReadChildren(&tracks) &&
         reader->MaybeReadChildren(&pssh));
  return true;
}

}  // namespace mp4_demuxer
