/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ISOMediaBoxes_h_
#define ISOMediaBoxes_h_

#include <bitset>
#include "nsString.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "MuxerOperation.h"

#define WRITE_FULLBOX(_compositor, _size)       \
  BoxSizeChecker checker(_compositor, _size);   \
  FullBox::Write();

#define FOURCC(a, b, c, d) ( ((a) << 24) | ((b) << 16) | ((c) << 8) | (d) )

namespace mozilla {

/**
 * track type from spec 8.4.3.3
 */
#define Audio_Track 0x01
#define Video_Track 0x02

class AudioTrackMetadata;
class VideoTrackMetadata;
class ES_Descriptor;
class ISOControl;

/**
 * This is the base class for all ISO media format boxes.
 * It provides the fields of box type(four CC) and size.
 * The data members in the beginning of a Box (or its descendants)
 * are the 14496-12 defined member. Other members prefix with 'm'
 * are private control data.
 *
 * This class is for inherited only, it shouldn't be instanced directly.
 */
class Box : public MuxerOperation {
protected:
  // ISO BMFF members
  uint32_t size;     // 14496-12 4-2 'Object Structure'. Size of this box.
  nsCString boxType; // four CC name, all table names are listed in
                     // 14496-12 table 1.

public:
  // MuxerOperation methods
  nsresult Write() MOZ_OVERRIDE;
  nsresult Find(const nsACString& aType,
                nsTArray<nsRefPtr<MuxerOperation>>& aOperations) MOZ_OVERRIDE;

  // This helper class will compare the written size in Write() and the size in
  // Generate(). If their are not equal, it will assert.
  class BoxSizeChecker {
  public:
    BoxSizeChecker(ISOControl* aControl, uint32_t aSize);
    ~BoxSizeChecker();

    uint32_t ori_size;
    uint32_t box_size;
    ISOControl* mControl;
  };

protected:
  Box() MOZ_DELETE;
  Box(const nsACString& aType, ISOControl* aControl);

  ISOControl* mControl;
  nsRefPtr<AudioTrackMetadata> mAudioMeta;
  nsRefPtr<VideoTrackMetadata> mVideoMeta;
};

/**
 * FullBox (and its descendants) is the box which contains the 'real' data
 * members. It is the edge in the ISO box structure and it doesn't contain
 * any box.
 *
 * This class is for inherited only, it shouldn't be instanced directly.
 */
class FullBox : public Box {
public:
  // ISO BMFF members
  uint8_t version;       // 14496-12 4.2 'Object Structure'
  std::bitset<24> flags; //

  // MuxerOperation methods
  nsresult Write() MOZ_OVERRIDE;

protected:
  // FullBox methods
  FullBox(const nsACString& aType, uint8_t aVersion, uint32_t aFlags,
          ISOControl* aControl);
  FullBox() MOZ_DELETE;
};

/**
 * The default implementation of the container box.
 * Basically, the container box inherits this class and overrides the
 * constructor only.
 *
 * According to 14496-12 3.1.1 'container box', a container box is
 * 'box whose sole purpose is to contain and group a set of related boxes'
 *
 * This class is for inherited only, it shouldn't be instanced directly.
 */
class DefaultContainerImpl : public Box {
public:
  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;
  nsresult Find(const nsACString& aType,
                nsTArray<nsRefPtr<MuxerOperation>>& aOperations) MOZ_OVERRIDE;

protected:
  // DefaultContainerImpl methods
  DefaultContainerImpl(const nsACString& aType, ISOControl* aControl);
  DefaultContainerImpl() MOZ_DELETE;

  nsTArray<nsRefPtr<MuxerOperation>> boxes;
};

// 14496-12 4.3 'File Type Box'
// Box type: 'ftyp'
class FileTypeBox : public Box {
public:
  // ISO BMFF members
  nsCString major_brand; // four chars
  uint32_t minor_version;
  nsTArray<nsCString> compatible_brands;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // FileTypeBox methods
  FileTypeBox(ISOControl* aControl);
  ~FileTypeBox();
};

// 14496-12 8.2.1 'Movie Box'
// Box type: 'moov'
// MovieBox contains MovieHeaderBox, TrackBox and MovieExtendsBox.
class MovieBox : public DefaultContainerImpl {
public:
  MovieBox(ISOControl* aControl);
  ~MovieBox();
};

// 14496-12 8.2.2 'Movie Header Box'
// Box type: 'mvhd'
class MovieHeaderBox : public FullBox {
public:
  // ISO BMFF members
  uint32_t creation_time;
  uint32_t modification_time;
  uint32_t timescale;
  uint32_t duration;
  uint32_t rate;
  uint16_t volume;
  uint16_t reserved16;
  uint32_t reserved32[2];
  uint32_t matrix[9];
  uint32_t pre_defined[6];
  uint32_t next_track_ID;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // MovieHeaderBox methods
  MovieHeaderBox(ISOControl* aControl);
  ~MovieHeaderBox();
  uint32_t GetTimeScale();
};

// 14496-12 8.4.2 'Media Header Box'
// Box type: 'mdhd'
class MediaHeaderBox : public FullBox {
public:
  // ISO BMFF members
  uint32_t creation_time;
  uint32_t modification_time;
  uint32_t timescale;
  uint32_t duration;
  std::bitset<1> pad;
  std::bitset<5> lang1;
  std::bitset<5> lang2;
  std::bitset<5> lang3;
  uint16_t pre_defined;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // MediaHeaderBox methods
  MediaHeaderBox(uint32_t aType, ISOControl* aControl);
  ~MediaHeaderBox();
  uint32_t GetTimeScale();

protected:
  uint32_t mTrackType;
};

// 14496-12 8.3.1 'Track Box'
// Box type: 'trak'
// TrackBox contains TrackHeaderBox and MediaBox.
class TrackBox : public DefaultContainerImpl {
public:
  TrackBox(uint32_t aTrackType, ISOControl* aControl);
  ~TrackBox();
};

// 14496-12 8.1.1 'Media Data Box'
// Box type: 'mdat'
class MediaDataBox : public Box {
public:
  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // MediaDataBox methods
  uint32_t GetAllSampleSize() { return mAllSampleSize; }
  uint32_t FirstSampleOffsetInMediaDataBox() { return mFirstSampleOffset; }
  MediaDataBox(uint32_t aTrackType, ISOControl* aControl);
  ~MediaDataBox();

protected:
  uint32_t mAllSampleSize;      // All audio and video sample size in this box.
  uint32_t mFirstSampleOffset;  // The offset of first sample in this box from
                                // the beginning of this mp4 file.
  uint32_t mTrackType;
};

// flags for TrackRunBox::flags, 14496-12 8.8.8.1.
#define flags_data_offset_present                     0x000001
#define flags_first_sample_flags_present              0x000002
#define flags_sample_duration_present                 0x000100
#define flags_sample_size_present                     0x000200
#define flags_sample_flags_present                    0x000400
#define flags_sample_composition_time_offsets_present 0x000800

// flag for TrackRunBox::tbl::sample_flags and TrackExtendsBox::default_sample_flags
// which is defined in 14496-12 8.8.3.1.
uint32_t set_sample_flags(bool aSync);

// 14496-12 8.8.8 'Track Fragment Run Box'
// Box type: 'trun'
class TrackRunBox : public FullBox {
public:
  // ISO BMFF members
  typedef struct {
    uint32_t sample_duration;
    uint32_t sample_size;
    uint32_t sample_flags;
    uint32_t sample_composition_time_offset;
  } tbl;

  uint32_t sample_count;
  // the following are optional fields
  uint32_t data_offset; // data offset exists when audio/video are present in file.
  uint32_t first_sample_flags;
  nsAutoArrayPtr<tbl> sample_info_table;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // TrackRunBox methods
  uint32_t GetAllSampleSize() { return mAllSampleSize; }
  nsresult SetDataOffset(uint32_t aOffset);

  TrackRunBox(uint32_t aType, uint32_t aFlags, ISOControl* aControl);
  ~TrackRunBox();

protected:
  uint32_t fillSampleTable();

  uint32_t mAllSampleSize;
  uint32_t mTrackType;
};

// tf_flags in TrackFragmentHeaderBox, 14496-12 8.8.7.1.
#define base_data_offset_present         0x000001
#define sample_description_index_present 0x000002
#define default_sample_duration_present  0x000008
#define default_sample_size_present      0x000010
#define default_sample_flags_present     0x000020
#define duration_is_empty                0x010000
#define default_base_is_moof             0x020000

// 14496-12 8.8.7 'Track Fragment Header Box'
// Box type: 'tfhd'
class TrackFragmentHeaderBox : public FullBox {
public:
  // ISO BMFF members
  uint32_t track_ID;
  uint64_t base_data_offset;
  uint32_t default_sample_duration;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // TrackFragmentHeaderBox methods
  nsresult UpdateBaseDataOffset(uint64_t aOffset); // The offset of the first
                                                   // sample in file.

  TrackFragmentHeaderBox(uint32_t aType, uint32_t aFlags, ISOControl* aControl);
  ~TrackFragmentHeaderBox();

protected:
  uint32_t mTrackType;
};

// 14496-12 8.8.6 'Track Fragment Box'
// Box type: 'traf'
// TrackFragmentBox cotains TrackFragmentHeaderBox and TrackRunBox.
class TrackFragmentBox : public DefaultContainerImpl {
public:
  TrackFragmentBox(uint32_t aType, ISOControl* aControl);
  ~TrackFragmentBox();

protected:
  uint32_t mTrackType;
};

// 14496-12 8.8.5 'Movie Fragment Header Box'
// Box type: 'mfhd'
class MovieFragmentHeaderBox : public FullBox {
public:
  // ISO BMFF members
  uint32_t sequence_number;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // MovieFragmentHeaderBox methods
  MovieFragmentHeaderBox(uint32_t aType, ISOControl* aControl);
  ~MovieFragmentHeaderBox();

protected:
  uint32_t mTrackType;
};

// 14496-12 8.8.4 'Movie Fragment Box'
// Box type: 'moof'
// MovieFragmentBox contains MovieFragmentHeaderBox and TrackFragmentBox.
class MovieFragmentBox : public DefaultContainerImpl {
public:
  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;

  // MovieFragmentBox methods
  MovieFragmentBox(uint32_t aType, ISOControl* aControl);
  ~MovieFragmentBox();

protected:
  uint32_t mTrackType;
};

// 14496-12 8.8.3 'Track Extends Box'
// Box type: 'trex'
class TrackExtendsBox : public FullBox {
public:
  // ISO BMFF members
  uint32_t track_ID;
  uint32_t default_sample_description_index;
  uint32_t default_sample_duration;
  uint32_t default_sample_size;
  uint32_t default_sample_flags;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // TrackExtendsBox methods
  TrackExtendsBox(uint32_t aType, ISOControl* aControl);
  ~TrackExtendsBox();

protected:
  uint32_t mTrackType;
};

// 14496-12 8.8.1 'Movie Extends Box'
// Box type: 'mvex'
// MovieExtendsBox contains TrackExtendsBox.
class MovieExtendsBox : public DefaultContainerImpl {
public:
  MovieExtendsBox(ISOControl* aControl);
  ~MovieExtendsBox();
};

// 14496-12 8.7.5 'Chunk Offset Box'
// Box type: 'stco'
class ChunkOffsetBox : public FullBox {
public:
  // ISO BMFF members
  typedef struct {
    uint32_t chunk_offset;
  } tbl;

  uint32_t entry_count;
  nsAutoArrayPtr<tbl> sample_tbl;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // ChunkOffsetBox methods
  ChunkOffsetBox(uint32_t aType, ISOControl* aControl);
  ~ChunkOffsetBox();

protected:
  uint32_t mTrackType;
};

// 14496-12 8.7.4 'Sample To Chunk Box'
// Box type: 'stsc'
class SampleToChunkBox : public FullBox {
public:
  // ISO BMFF members
  typedef struct {
    uint32_t first_chunk;
    uint32_t sample_per_chunk;
    uint32_t sample_description_index;
  } tbl;

  uint32_t entry_count;
  nsAutoArrayPtr<tbl> sample_tbl;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // SampleToChunkBox methods
  SampleToChunkBox(uint32_t aType, ISOControl* aControl);
  ~SampleToChunkBox();

protected:
  uint32_t mTrackType;
};

// 14496-12 8.6.1.2 'Decoding Time to Sample Box'
// Box type: 'stts'
class TimeToSampleBox : public FullBox {
public:
  // ISO BMFF members
  typedef struct {
    uint32_t sample_count;
    uint32_t sample_delta;
  } tbl;

  uint32_t entry_count;
  nsAutoArrayPtr<tbl> sample_tbl;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // TimeToSampleBox methods
  TimeToSampleBox(uint32_t aType, ISOControl* aControl);
  ~TimeToSampleBox();

protected:
  uint32_t mTrackType;
};

/**
 * 14496-12 8.5.2 'Sample Description Box'
 * This is the base class for VisualSampleEntry and AudioSampleEntry.
 *
 * This class is for inherited only, it shouldn't be instanced directly.
 *
 * The inhertied tree of a codec box should be:
 *
 *                                            +--> AVCSampleEntry
 *                  +--> VisualSampleEntryBox +
 *                  |                         +--> ...
 *   SampleEntryBox +
 *                  |                         +--> MP4AudioSampleEntry
 *                  +--> AudioSampleEntryBox  +
 *                                            +--> AMRSampleEntry
 *                                            +
 *                                            +--> ...
 *
 */
class SampleEntryBox : public Box {
public:
  // ISO BMFF members
  uint8_t reserved[6];
  uint16_t data_reference_index;

  // sampleentrybox methods
  SampleEntryBox(const nsACString& aFormat, ISOControl* aControl);

  // MuxerOperation methods
  nsresult Write() MOZ_OVERRIDE;

protected:
  SampleEntryBox() MOZ_DELETE;
};

// 14496-12 8.5.2 'Sample Description Box'
// Box type: 'stsd'
class SampleDescriptionBox : public FullBox {
public:
  // ISO BMFF members
  uint32_t entry_count;
  nsRefPtr<SampleEntryBox> sample_entry_box;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // SampleDescriptionBox methods
  SampleDescriptionBox(uint32_t aType, ISOControl* aControl);
  ~SampleDescriptionBox();

protected:
  nsresult CreateAudioSampleEntry(nsRefPtr<SampleEntryBox>& aSampleEntry);
  nsresult CreateVideoSampleEntry(nsRefPtr<SampleEntryBox>& aSampleEntry);

  uint32_t mTrackType;
};

// 14496-12 8.5.2.2
// The base class for audio codec box.
// This class is for inherited only, it shouldn't be instanced directly.
class AudioSampleEntry : public SampleEntryBox {
public:
  // ISO BMFF members
  uint16_t sound_version;
  uint8_t reserved2[6];
  uint16_t channels;
  uint16_t sample_size;
  uint16_t compressionId;
  uint16_t packet_size;
  uint32_t timeScale;  // (sample rate of media) <<16

  // MuxerOperation methods
  nsresult Write() MOZ_OVERRIDE;

  ~AudioSampleEntry();

protected:
  AudioSampleEntry(const nsACString& aFormat, ISOControl* aControl);
};

// 14496-12 8.5.2.2
// The base class for video codec box.
// This class is for inherited only, it shouldn't be instanced directly.
class VisualSampleEntry : public SampleEntryBox {
public:
  // ISO BMFF members
  uint8_t reserved[16];
  uint16_t width;
  uint16_t height;

  uint32_t horizresolution; // 72 dpi
  uint32_t vertresolution;  // 72 dpi
  uint32_t reserved2;
  uint16_t frame_count;     // 1, defined in 14496-12 8.5.2.2

  uint8_t compressorName[32];
  uint16_t depth;       // 0x0018, defined in 14496-12 8.5.2.2;
  uint16_t pre_defined; // -1, defined in 14496-12 8.5.2.2;

  // MuxerOperation methods
  nsresult Write() MOZ_OVERRIDE;

  // VisualSampleEntry methods
  ~VisualSampleEntry();

protected:
  VisualSampleEntry(const nsACString& aFormat, ISOControl* aControl);
};

// 14496-12 8.7.3.2 'Sample Size Box'
// Box type: 'stsz'
class SampleSizeBox : public FullBox {
public:
  // ISO BMFF members
  uint32_t sample_size;
  uint32_t sample_count;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // SampleSizeBox methods
  SampleSizeBox(ISOControl* aControl);
  ~SampleSizeBox();
};

// 14496-12 8.5.1 'Sample Table Box'
// Box type: 'stbl'
//
// SampleTableBox contains SampleDescriptionBox,
//                         TimeToSampleBox,
//                         SampleToChunkBox,
//                         SampleSizeBox and
//                         ChunkOffsetBox.
class SampleTableBox : public DefaultContainerImpl {
public:
  SampleTableBox(uint32_t aType, ISOControl* aControl);
  ~SampleTableBox();
};

// 14496-12 8.7.2 'Data Reference Box'
// Box type: 'url '
class DataEntryUrlBox : public FullBox {
public:
  // ISO BMFF members
  // flags in DataEntryUrlBox::flags
  const static uint16_t flags_media_at_the_same_file = 0x0001;

  nsCString location;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // DataEntryUrlBox methods
  DataEntryUrlBox();
  DataEntryUrlBox(ISOControl* aControl);
  DataEntryUrlBox(const DataEntryUrlBox& aBox);
  ~DataEntryUrlBox();
};

// 14496-12 8.7.2 'Data Reference Box'
// Box type: 'dref'
class DataReferenceBox : public FullBox {
public:
  // ISO BMFF members
  uint32_t entry_count;
  nsTArray<nsAutoPtr<DataEntryUrlBox>> urls;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // DataReferenceBox methods
  DataReferenceBox(ISOControl* aControl);
  ~DataReferenceBox();
};

// 14496-12 8.7.1 'Data Information Box'
// Box type: 'dinf'
// DataInformationBox contains DataReferenceBox.
class DataInformationBox : public DefaultContainerImpl {
public:
  DataInformationBox(ISOControl* aControl);
  ~DataInformationBox();
};

// 14496-12 8.4.5.2 'Video Media Header Box'
// Box type: 'vmhd'
class VideoMediaHeaderBox : public FullBox {
public:
  // ISO BMFF members
  uint16_t graphicsmode;
  uint16_t opcolor[3];

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // VideoMediaHeaderBox methods
  VideoMediaHeaderBox(ISOControl* aControl);
  ~VideoMediaHeaderBox();
};

// 14496-12 8.4.5.3 'Sound Media Header Box'
// Box type: 'smhd'
class SoundMediaHeaderBox : public FullBox {
public:
  // ISO BMFF members
  uint16_t balance;
  uint16_t reserved;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // SoundMediaHeaderBox methods
  SoundMediaHeaderBox(ISOControl* aControl);
  ~SoundMediaHeaderBox();
};

// 14496-12 8.4.4 'Media Information Box'
// Box type: 'minf'
// MediaInformationBox contains SoundMediaHeaderBox, DataInformationBox and
// SampleTableBox.
class MediaInformationBox : public DefaultContainerImpl {
public:
  MediaInformationBox(uint32_t aType, ISOControl* aControl);
  ~MediaInformationBox();

protected:
  uint32_t mTrackType;
};

// flags for TrackHeaderBox::flags.
#define flags_track_enabled    0x000001
#define flags_track_in_movie   0x000002
#define flags_track_in_preview 0x000004

// 14496-12 8.3.2 'Track Header Box'
// Box type: 'tkhd'
class TrackHeaderBox : public FullBox {
public:
  // ISO BMFF members
  // version = 0
  uint32_t creation_time;
  uint32_t modification_time;
  uint32_t track_ID;
  uint32_t reserved;
  uint32_t duration;

  uint32_t reserved2[2];
  uint16_t layer;
  uint16_t alternate_group;
  uint16_t volume;
  uint16_t reserved3;
  uint32_t matrix[9];
  uint32_t width;
  uint32_t height;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // TrackHeaderBox methods
  TrackHeaderBox(uint32_t aType, ISOControl* aControl);
  ~TrackHeaderBox();

protected:
  uint32_t mTrackType;
};

// 14496-12 8.4.3 'Handler Reference Box'
// Box type: 'hdlr'
class HandlerBox : public FullBox {
public:
  // ISO BMFF members
  uint32_t pre_defined;
  uint32_t handler_type;
  uint32_t reserved[3];
  nsCString name;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // HandlerBox methods
  HandlerBox(uint32_t aType, ISOControl* aControl);
  ~HandlerBox();

protected:
  uint32_t mTrackType;
};

// 14496-12 8.4.1 'Media Box'
// Box type: 'mdia'
// MediaBox contains MediaHeaderBox, HandlerBox, and MediaInformationBox.
class MediaBox : public DefaultContainerImpl {
public:
  MediaBox(uint32_t aType, ISOControl* aControl);
  ~MediaBox();

protected:
  uint32_t mTrackType;
};

}
#endif // ISOMediaBoxes_h_
