/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <climits>
#include "TrackMetadataBase.h"
#include "ISOMediaBoxes.h"
#include "ISOControl.h"
#include "ISOMediaWriter.h"
#include "EncodedFrameContainer.h"
#include "ISOTrackMetadata.h"
#include "MP4ESDS.h"
#include "AMRBox.h"
#include "AVCBox.h"
#include "VideoUtils.h"

namespace mozilla {

// 14496-12 6.2.2 'Data Types and fields'
const uint32_t iso_matrix[] = { 0x00010000, 0,          0,
                                0,          0x00010000, 0,
                                0,          0,          0x40000000 };

uint32_t
set_sample_flags(bool aSync)
{
  std::bitset<32> flags;
  flags.set(16, !aSync);
  return flags.to_ulong();
}

Box::BoxSizeChecker::BoxSizeChecker(ISOControl* aControl, uint32_t aSize)
{
  mControl = aControl;
  ori_size = mControl->GetBufPos();
  box_size = aSize;
  MOZ_COUNT_CTOR(BoxSizeChecker);
}

Box::BoxSizeChecker::~BoxSizeChecker()
{
  uint32_t cur_size = mControl->GetBufPos();
  if ((cur_size - ori_size) != box_size) {
    MOZ_ASSERT(false);
  }

  MOZ_COUNT_DTOR(BoxSizeChecker);
}

nsresult
MediaDataBox::Generate(uint32_t* aBoxSize)
{
  mFirstSampleOffset = size;
  mAllSampleSize = 0;

  if (mTrackType & Audio_Track) {
    FragmentBuffer* frag = mControl->GetFragment(Audio_Track);
    mAllSampleSize += frag->GetFirstFragmentSampleSize();
  }
  if (mTrackType & Video_Track) {
    FragmentBuffer* frag = mControl->GetFragment(Video_Track);
    mAllSampleSize += frag->GetFirstFragmentSampleSize();
  }

  size += mAllSampleSize;
  *aBoxSize = size;
  return NS_OK;
}

nsresult
MediaDataBox::Write()
{
  nsresult rv;
  BoxSizeChecker checker(mControl, size);
  Box::Write();
  nsTArray<uint32_t> types;
  types.AppendElement(Audio_Track);
  types.AppendElement(Video_Track);

  for (uint32_t l = 0; l < types.Length(); l++) {
    if (mTrackType & types[l]) {
      FragmentBuffer* frag = mControl->GetFragment(types[l]);
      nsTArray<nsRefPtr<EncodedFrame>> frames;

      // Here is the last time we get fragment frames, flush it!
      rv = frag->GetFirstFragment(frames, true);
      NS_ENSURE_SUCCESS(rv, rv);

      uint32_t len = frames.Length();
      for (uint32_t i = 0; i < len; i++) {
        nsTArray<uint8_t> frame_buffer;
        frames.ElementAt(i)->SwapOutFrameData(frame_buffer);
        mControl->WriteAVData(frame_buffer);
      }
    }
  }

  return NS_OK;
}

MediaDataBox::MediaDataBox(uint32_t aTrackType, ISOControl* aControl)
  : Box(NS_LITERAL_CSTRING("mdat"), aControl)
  , mAllSampleSize(0)
  , mFirstSampleOffset(0)
  , mTrackType(aTrackType)
{
  MOZ_COUNT_CTOR(MediaDataBox);
}

MediaDataBox::~MediaDataBox()
{
  MOZ_COUNT_DTOR(MediaDataBox);
}

uint32_t
TrackRunBox::fillSampleTable()
{
  uint32_t table_size = 0;
  nsresult rv;
  nsTArray<nsRefPtr<EncodedFrame>> frames;
  FragmentBuffer* frag = mControl->GetFragment(mTrackType);

  rv = frag->GetFirstFragment(frames);
  if (NS_FAILED(rv)) {
    return 0;
  }
  uint32_t len = frames.Length();
  sample_info_table = new tbl[len];
  // Create sample table according to 14496-12 8.8.8.2.
  for (uint32_t i = 0; i < len; i++) {
    // Sample size.
    sample_info_table[i].sample_size = 0;
    if (flags.to_ulong() & flags_sample_size_present) {
      sample_info_table[i].sample_size = frames.ElementAt(i)->GetFrameData().Length();
      mAllSampleSize += sample_info_table[i].sample_size;
      table_size += sizeof(uint32_t);
    }

    // Sample flags.
    sample_info_table[i].sample_flags = 0;
    if (flags.to_ulong() & flags_sample_flags_present) {
      sample_info_table[i].sample_flags =
        set_sample_flags(
          (frames.ElementAt(i)->GetFrameType() == EncodedFrame::AVC_I_FRAME));
      table_size += sizeof(uint32_t);
    }

    // Sample duration.
    sample_info_table[i].sample_duration = 0;
    if (flags.to_ulong() & flags_sample_duration_present) {
      // Calculate each frame's duration, it is decided by "current frame
      // timestamp - last frame timestamp".
      uint64_t frame_time = 0;
      if (i == 0) {
        frame_time = frames.ElementAt(i)->GetTimeStamp() -
                     frag->GetLastFragmentLastFrameTime();
      } else {
        frame_time = frames.ElementAt(i)->GetTimeStamp() -
                     frames.ElementAt(i - 1)->GetTimeStamp();
        // Keep the last frame time of current fagment, it will be used to calculate
        // the first frame duration of next fragment.
        if ((len - 1) == i) {
          frag->SetLastFragmentLastFrameTime(frames.ElementAt(i)->GetTimeStamp());
        }
      }

      // In TrackRunBox, there should be exactly one type, either audio or video.
      MOZ_ASSERT((mTrackType & Video_Track) ^ (mTrackType & Audio_Track));
      sample_info_table[i].sample_duration = (mTrackType & Video_Track ?
        frame_time * mVideoMeta->GetVideoClockRate() / USECS_PER_S :
        frame_time * mAudioMeta->GetAudioSampleRate() / USECS_PER_S);

      table_size += sizeof(uint32_t);
    }

    sample_info_table[i].sample_composition_time_offset = 0;
  }
  return table_size;
}

nsresult
TrackRunBox::Generate(uint32_t* aBoxSize)
{
  FragmentBuffer* frag = mControl->GetFragment(mTrackType);
  sample_count = frag->GetFirstFragmentSampleNumber();
  size += sizeof(sample_count);

  // data_offset needs to be updated if there is other
  // TrackRunBox before this one.
  if (flags.to_ulong() & flags_data_offset_present) {
    data_offset = 0;
    size += sizeof(data_offset);
  }
  size += fillSampleTable();

  *aBoxSize = size;

  return NS_OK;
}

nsresult
TrackRunBox::SetDataOffset(uint32_t aOffset)
{
  data_offset = aOffset;
  return NS_OK;
}

nsresult
TrackRunBox::Write()
{
  WRITE_FULLBOX(mControl, size)
  mControl->Write(sample_count);
  if (flags.to_ulong() & flags_data_offset_present) {
    mControl->Write(data_offset);
  }
  for (uint32_t i = 0; i < sample_count; i++) {
    if (flags.to_ulong() & flags_sample_duration_present) {
      mControl->Write(sample_info_table[i].sample_duration);
    }
    if (flags.to_ulong() & flags_sample_size_present) {
      mControl->Write(sample_info_table[i].sample_size);
    }
    if (flags.to_ulong() & flags_sample_flags_present) {
      mControl->Write(sample_info_table[i].sample_flags);
    }
  }

  return NS_OK;
}

TrackRunBox::TrackRunBox(uint32_t aType, uint32_t aFlags, ISOControl* aControl)
  : FullBox(NS_LITERAL_CSTRING("trun"), 0, aFlags, aControl)
  , sample_count(0)
  , data_offset(0)
  , first_sample_flags(0)
  , mAllSampleSize(0)
  , mTrackType(aType)
{
  MOZ_COUNT_CTOR(TrackRunBox);
}

TrackRunBox::~TrackRunBox()
{
  MOZ_COUNT_DTOR(TrackRunBox);
}

nsresult
TrackFragmentHeaderBox::UpdateBaseDataOffset(uint64_t aOffset)
{
  base_data_offset = aOffset;
  return NS_OK;
}

nsresult
TrackFragmentHeaderBox::Generate(uint32_t* aBoxSize)
{
  track_ID = (mTrackType == Audio_Track ?
                mControl->GetTrackID(mAudioMeta->GetKind()) :
                mControl->GetTrackID(mVideoMeta->GetKind()));
  size += sizeof(track_ID);

  if (flags.to_ulong() & base_data_offset_present) {
    // base_data_offset needs to add size of 'trun', 'tfhd' and
    // header of 'mdat' later.
    base_data_offset = 0;
    size += sizeof(base_data_offset);
  }
  if (flags.to_ulong() & default_sample_duration_present) {
    if (mTrackType == Video_Track) {
      if (!mVideoMeta->GetVideoFrameRate()) {
        // 0 means frame rate is variant, so it is wrong to write
        // default_sample_duration.
        MOZ_ASSERT(0);
        default_sample_duration = 0;
      } else {
        default_sample_duration = mVideoMeta->GetVideoClockRate() / mVideoMeta->GetVideoFrameRate();
      }
    } else if (mTrackType == Audio_Track) {
      default_sample_duration = mAudioMeta->GetAudioFrameDuration();
    } else {
      MOZ_ASSERT(0);
      return NS_ERROR_FAILURE;
    }
    size += sizeof(default_sample_duration);
  }
  *aBoxSize = size;
  return NS_OK;
}

nsresult
TrackFragmentHeaderBox::Write()
{
  WRITE_FULLBOX(mControl, size)
  mControl->Write(track_ID);
  if (flags.to_ulong() & base_data_offset_present) {
    mControl->Write(base_data_offset);
  }
  if (flags.to_ulong() & default_sample_duration_present) {
    mControl->Write(default_sample_duration);
  }
  return NS_OK;
}

TrackFragmentHeaderBox::TrackFragmentHeaderBox(uint32_t aType,
                                               uint32_t aFlags,
                                               ISOControl* aControl)
  : FullBox(NS_LITERAL_CSTRING("tfhd"), 0, aFlags, aControl)
  , track_ID(0)
  , base_data_offset(0)
  , default_sample_duration(0)
{
  mTrackType = aType;
  MOZ_COUNT_CTOR(TrackFragmentHeaderBox);
}

TrackFragmentHeaderBox::~TrackFragmentHeaderBox()
{
  MOZ_COUNT_DTOR(TrackFragmentHeaderBox);
}

TrackFragmentBox::TrackFragmentBox(uint32_t aType, ISOControl* aControl)
  : DefaultContainerImpl(NS_LITERAL_CSTRING("traf"), aControl)
  , mTrackType(aType)
{
  // Flags in TrackFragmentHeaderBox.
  uint32_t tf_flags = base_data_offset_present;

  // Ideally, audio encoder generates audio frame in const rate. However, some
  // audio encoders don't do it so the audio frame duration needs to be checked
  // here.
  if ((mTrackType & Audio_Track) && mAudioMeta->GetAudioFrameDuration()) {
    tf_flags |= default_sample_duration_present;
  }

  boxes.AppendElement(new TrackFragmentHeaderBox(aType, tf_flags, aControl));

  // Always adds flags_data_offset_present in each TrackRunBox, Android
  // parser requires this flag to calculate the correct bitstream offset.
  uint32_t tr_flags = flags_sample_size_present | flags_data_offset_present;

  // Flags in TrackRunBox.
  // If there is no default sample duration exists, each frame duration needs to
  // be recored in the TrackRunBox.
  tr_flags |= (tf_flags & default_sample_duration_present ? 0 : flags_sample_duration_present);

  // For video, add sample_flags to record I frame.
  tr_flags |= (mTrackType & Video_Track ? flags_sample_flags_present : 0);

  boxes.AppendElement(new TrackRunBox(mTrackType, tr_flags, aControl));
  MOZ_COUNT_CTOR(TrackFragmentBox);
}

TrackFragmentBox::~TrackFragmentBox()
{
  MOZ_COUNT_DTOR(TrackFragmentBox);
}

nsresult
MovieFragmentHeaderBox::Generate(uint32_t* aBoxSize)
{
  sequence_number = mControl->GetCurFragmentNumber();
  size += sizeof(sequence_number);
  *aBoxSize = size;
  return NS_OK;
}

nsresult
MovieFragmentHeaderBox::Write()
{
  WRITE_FULLBOX(mControl, size)
  mControl->Write(sequence_number);
  return NS_OK;
}

MovieFragmentHeaderBox::MovieFragmentHeaderBox(uint32_t aTrackType,
                                               ISOControl* aControl)
  : FullBox(NS_LITERAL_CSTRING("mfhd"), 0, 0, aControl)
  , sequence_number(0)
  , mTrackType(aTrackType)
{
  MOZ_COUNT_CTOR(MovieFragmentHeaderBox);
}

MovieFragmentHeaderBox::~MovieFragmentHeaderBox()
{
  MOZ_COUNT_DTOR(MovieFragmentHeaderBox);
}

MovieFragmentBox::MovieFragmentBox(uint32_t aType, ISOControl* aControl)
  : DefaultContainerImpl(NS_LITERAL_CSTRING("moof"), aControl)
  , mTrackType(aType)
{
  boxes.AppendElement(new MovieFragmentHeaderBox(mTrackType, aControl));

  if (mTrackType & Audio_Track) {
    boxes.AppendElement(
      new TrackFragmentBox(Audio_Track, aControl));
  }
  if (mTrackType & Video_Track) {
    boxes.AppendElement(
      new TrackFragmentBox(Video_Track, aControl));
  }
  MOZ_COUNT_CTOR(MovieFragmentBox);
}

MovieFragmentBox::~MovieFragmentBox()
{
  MOZ_COUNT_DTOR(MovieFragmentBox);
}

nsresult
MovieFragmentBox::Generate(uint32_t* aBoxSize)
{
  nsresult rv = DefaultContainerImpl::Generate(aBoxSize);
  NS_ENSURE_SUCCESS(rv, rv);

  // Correct data_offset if there are both audio and video track in
  // this fragment. This offset means the offset in the MediaDataBox.
  if (mTrackType & (Audio_Track | Video_Track)) {
    nsTArray<nsRefPtr<MuxerOperation>> truns;
    rv = Find(NS_LITERAL_CSTRING("trun"), truns);
    NS_ENSURE_SUCCESS(rv, rv);
    uint32_t len = truns.Length();
    uint32_t data_offset = 0;
    for (uint32_t i = 0; i < len; i++) {
      TrackRunBox* trun = (TrackRunBox*) truns.ElementAt(i).get();
      rv = trun->SetDataOffset(data_offset);
      NS_ENSURE_SUCCESS(rv, rv);
      data_offset += trun->GetAllSampleSize();
    }
  }

  return NS_OK;
}

nsresult
TrackExtendsBox::Generate(uint32_t* aBoxSize)
{
  track_ID = (mTrackType == Audio_Track ?
                mControl->GetTrackID(mAudioMeta->GetKind()) :
                mControl->GetTrackID(mVideoMeta->GetKind()));

  if (mTrackType == Audio_Track) {
    default_sample_description_index = 1;
    default_sample_duration = mAudioMeta->GetAudioFrameDuration();
    default_sample_size = mAudioMeta->GetAudioFrameSize();
    default_sample_flags = set_sample_flags(1);
  } else if (mTrackType == Video_Track) {
    default_sample_description_index = 1;
    // Video meta data has assigned framerate, it implies that this video's
    // frame rate should be fixed.
    if (mVideoMeta->GetVideoFrameRate()) {
      default_sample_duration =
        mVideoMeta->GetVideoClockRate() / mVideoMeta->GetVideoFrameRate();
    }
    default_sample_size = 0;
    default_sample_flags = set_sample_flags(0);
  } else {
    MOZ_ASSERT(0);
    return NS_ERROR_FAILURE;
  }

  size += sizeof(track_ID) +
          sizeof(default_sample_description_index) +
          sizeof(default_sample_duration) +
          sizeof(default_sample_size) +
          sizeof(default_sample_flags);

  *aBoxSize = size;

  return NS_OK;
}

nsresult
TrackExtendsBox::Write()
{
  WRITE_FULLBOX(mControl, size)
  mControl->Write(track_ID);
  mControl->Write(default_sample_description_index);
  mControl->Write(default_sample_duration);
  mControl->Write(default_sample_size);
  mControl->Write(default_sample_flags);

  return NS_OK;
}

TrackExtendsBox::TrackExtendsBox(uint32_t aType, ISOControl* aControl)
  : FullBox(NS_LITERAL_CSTRING("trex"), 0, 0, aControl)
  , track_ID(0)
  , default_sample_description_index(0)
  , default_sample_duration(0)
  , default_sample_size(0)
  , default_sample_flags(0)
  , mTrackType(aType)
{
  MOZ_COUNT_CTOR(TrackExtendsBox);
}

TrackExtendsBox::~TrackExtendsBox()
{
  MOZ_COUNT_DTOR(TrackExtendsBox);
}

MovieExtendsBox::MovieExtendsBox(ISOControl* aControl)
  : DefaultContainerImpl(NS_LITERAL_CSTRING("mvex"), aControl)
{
  if (mAudioMeta) {
    boxes.AppendElement(new TrackExtendsBox(Audio_Track, aControl));
  }
  if (mVideoMeta) {
    boxes.AppendElement(new TrackExtendsBox(Video_Track, aControl));
  }
  MOZ_COUNT_CTOR(MovieExtendsBox);
}

MovieExtendsBox::~MovieExtendsBox()
{
  MOZ_COUNT_DTOR(MovieExtendsBox);
}

nsresult
ChunkOffsetBox::Generate(uint32_t* aBoxSize)
{
  // We don't need time to sample table in fragmented mp4.
  entry_count = 0;
  size += sizeof(entry_count);
  *aBoxSize = size;
  return NS_OK;
}

nsresult
ChunkOffsetBox::Write()
{
  WRITE_FULLBOX(mControl, size)
  mControl->Write(entry_count);
  return NS_OK;
}

ChunkOffsetBox::ChunkOffsetBox(uint32_t aType, ISOControl* aControl)
  : FullBox(NS_LITERAL_CSTRING("stco"), 0, 0, aControl)
  , entry_count(0)
{
  MOZ_COUNT_CTOR(ChunkOffsetBox);
}

ChunkOffsetBox::~ChunkOffsetBox()
{
  MOZ_COUNT_DTOR(ChunkOffsetBox);
}

nsresult
SampleToChunkBox::Generate(uint32_t* aBoxSize)
{
  // We don't need time to sample table in fragmented mp4
  entry_count = 0;
  size += sizeof(entry_count);
  *aBoxSize = size;
  return NS_OK;
}

nsresult
SampleToChunkBox::Write()
{
  WRITE_FULLBOX(mControl, size)
  mControl->Write(entry_count);
  return NS_OK;
}

SampleToChunkBox::SampleToChunkBox(uint32_t aType, ISOControl* aControl)
  : FullBox(NS_LITERAL_CSTRING("stsc"), 0, 0, aControl)
  , entry_count(0)
{
  MOZ_COUNT_CTOR(SampleToChunkBox);
}

SampleToChunkBox::~SampleToChunkBox()
{
  MOZ_COUNT_DTOR(SampleToChunkBox);
}

nsresult
TimeToSampleBox::Generate(uint32_t* aBoxSize)
{
  // We don't need time to sample table in fragmented mp4.
  entry_count = 0;
  size += sizeof(entry_count);
  *aBoxSize = size;
  return NS_OK;
}

nsresult
TimeToSampleBox::Write()
{
  WRITE_FULLBOX(mControl, size)
  mControl->Write(entry_count);
  return NS_OK;
}

TimeToSampleBox::TimeToSampleBox(uint32_t aType, ISOControl* aControl)
  : FullBox(NS_LITERAL_CSTRING("stts"), 0, 0, aControl)
  , entry_count(0)
{
  MOZ_COUNT_CTOR(TimeToSampleBox);
}

TimeToSampleBox::~TimeToSampleBox()
{
  MOZ_COUNT_DTOR(TimeToSampleBox);
}

nsresult
SampleDescriptionBox::Generate(uint32_t* aBoxSize)
{
  entry_count = 1;
  size += sizeof(entry_count);

  nsresult rv;
  uint32_t box_size;
  rv = sample_entry_box->Generate(&box_size);
  NS_ENSURE_SUCCESS(rv, rv);
  size += box_size;
  *aBoxSize = size;

  return NS_OK;
}

nsresult
SampleDescriptionBox::Write()
{
  WRITE_FULLBOX(mControl, size)
  nsresult rv;
  mControl->Write(entry_count);
  rv = sample_entry_box->Write();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

SampleDescriptionBox::SampleDescriptionBox(uint32_t aType, ISOControl* aControl)
  : FullBox(NS_LITERAL_CSTRING("stsd"), 0, 0, aControl)
  , entry_count(0)
{
  mTrackType = aType;

  switch (mTrackType) {
  case Audio_Track:
    {
      CreateAudioSampleEntry(sample_entry_box);
    }
    break;
  case Video_Track:
    {
      CreateVideoSampleEntry(sample_entry_box);
    }
    break;
  }
  MOZ_ASSERT(sample_entry_box);
  MOZ_COUNT_CTOR(SampleDescriptionBox);
}

nsresult
SampleDescriptionBox::CreateAudioSampleEntry(nsRefPtr<SampleEntryBox>& aSampleEntry)
{
  if (mAudioMeta->GetKind() == TrackMetadataBase::METADATA_AMR) {
    aSampleEntry = new AMRSampleEntry(mControl);
  } else if (mAudioMeta->GetKind() == TrackMetadataBase::METADATA_AAC) {
    aSampleEntry = new MP4AudioSampleEntry(mControl);
  } else {
    MOZ_ASSERT(0);
  }
  return NS_OK;
}

nsresult
SampleDescriptionBox::CreateVideoSampleEntry(nsRefPtr<SampleEntryBox>& aSampleEntry)
{
  if (mVideoMeta->GetKind() == TrackMetadataBase::METADATA_AVC) {
    aSampleEntry = new AVCSampleEntry(mControl);
  } else {
    MOZ_ASSERT(0);
  }
  return NS_OK;
}

SampleDescriptionBox::~SampleDescriptionBox()
{
  MOZ_COUNT_DTOR(SampleDescriptionBox);
}

nsresult
SampleSizeBox::Generate(uint32_t* aBoxSize)
{
  size += sizeof(sample_size) +
          sizeof(sample_count);
  *aBoxSize = size;
  return NS_OK;
}

nsresult
SampleSizeBox::Write()
{
  WRITE_FULLBOX(mControl, size)
  mControl->Write(sample_size);
  mControl->Write(sample_count);
  return NS_OK;
}

SampleSizeBox::SampleSizeBox(ISOControl* aControl)
  : FullBox(NS_LITERAL_CSTRING("stsz"), 0, 0, aControl)
  , sample_size(0)
  , sample_count(0)
{
  MOZ_COUNT_CTOR(SampleSizeBox);
}

SampleSizeBox::~SampleSizeBox()
{
  MOZ_COUNT_DTOR(SampleSizeBox);
}

SampleTableBox::SampleTableBox(uint32_t aType, ISOControl* aControl)
  : DefaultContainerImpl(NS_LITERAL_CSTRING("stbl"), aControl)
{
  boxes.AppendElement(new SampleDescriptionBox(aType, aControl));
  boxes.AppendElement(new TimeToSampleBox(aType, aControl));
  boxes.AppendElement(new SampleToChunkBox(aType, aControl));
  boxes.AppendElement(new SampleSizeBox(aControl));
  boxes.AppendElement(new ChunkOffsetBox(aType, aControl));
  MOZ_COUNT_CTOR(SampleTableBox);
}

SampleTableBox::~SampleTableBox()
{
  MOZ_COUNT_DTOR(SampleTableBox);
}

nsresult
DataEntryUrlBox::Generate(uint32_t* aBoxSize)
{
  // location is null here, do nothing
  size += location.Length();
  *aBoxSize = size;

  return NS_OK;
}

nsresult
DataEntryUrlBox::Write()
{
  WRITE_FULLBOX(mControl, size)
  return NS_OK;
}

DataEntryUrlBox::DataEntryUrlBox()
  : FullBox(NS_LITERAL_CSTRING("url "), 0, 0, (ISOControl*) nullptr)
{
  MOZ_COUNT_CTOR(DataEntryUrlBox);
}

DataEntryUrlBox::DataEntryUrlBox(ISOControl* aControl)
  : FullBox(NS_LITERAL_CSTRING("url "), 0, flags_media_at_the_same_file, aControl)
{
  MOZ_COUNT_CTOR(DataEntryUrlBox);
}

DataEntryUrlBox::DataEntryUrlBox(const DataEntryUrlBox& aBox)
  : FullBox(aBox.boxType, aBox.version, aBox.flags.to_ulong(), aBox.mControl)
{
  location = aBox.location;
  MOZ_COUNT_CTOR(DataEntryUrlBox);
}

DataEntryUrlBox::~DataEntryUrlBox()
{
  MOZ_COUNT_DTOR(DataEntryUrlBox);
}

nsresult DataReferenceBox::Generate(uint32_t* aBoxSize)
{
  entry_count = 1;  // only allow on entry here
  size += sizeof(uint32_t);

  for (uint32_t i = 0; i < entry_count; i++) {
    uint32_t box_size = 0;
    DataEntryUrlBox* url = new DataEntryUrlBox(mControl);
    url->Generate(&box_size);
    size += box_size;
    urls.AppendElement(url);
  }

  *aBoxSize = size;

  return NS_OK;
}

nsresult DataReferenceBox::Write()
{
  WRITE_FULLBOX(mControl, size)
  mControl->Write(entry_count);

  for (uint32_t i = 0; i < entry_count; i++) {
    urls[i]->Write();
  }

  return NS_OK;
}

DataReferenceBox::DataReferenceBox(ISOControl* aControl)
  : FullBox(NS_LITERAL_CSTRING("dref"), 0, 0, aControl)
  , entry_count(0)
{
  MOZ_COUNT_CTOR(DataReferenceBox);
}

DataReferenceBox::~DataReferenceBox()
{
  MOZ_COUNT_DTOR(DataReferenceBox);
}

DataInformationBox::DataInformationBox(ISOControl* aControl)
  : DefaultContainerImpl(NS_LITERAL_CSTRING("dinf"), aControl)
{
  boxes.AppendElement(new DataReferenceBox(aControl));
  MOZ_COUNT_CTOR(DataInformationBox);
}

DataInformationBox::~DataInformationBox()
{
  MOZ_COUNT_DTOR(DataInformationBox);
}

nsresult
VideoMediaHeaderBox::Generate(uint32_t* aBoxSize)
{
  size += sizeof(graphicsmode) +
          sizeof(opcolor);

  *aBoxSize = size;

  return NS_OK;
}

nsresult
VideoMediaHeaderBox::Write()
{
  WRITE_FULLBOX(mControl, size)
  mControl->Write(graphicsmode);
  mControl->WriteArray(opcolor, 3);
  return NS_OK;
}

VideoMediaHeaderBox::VideoMediaHeaderBox(ISOControl* aControl)
  : FullBox(NS_LITERAL_CSTRING("vmhd"), 0, 1, aControl)
  , graphicsmode(0)
{
  memset(opcolor, 0 , sizeof(opcolor));
  MOZ_COUNT_CTOR(VideoMediaHeaderBox);
}

VideoMediaHeaderBox::~VideoMediaHeaderBox()
{
  MOZ_COUNT_DTOR(VideoMediaHeaderBox);
}

nsresult
SoundMediaHeaderBox::Generate(uint32_t* aBoxSize)
{
  balance = 0;
  reserved = 0;
  size += sizeof(balance) +
          sizeof(reserved);

  *aBoxSize = size;

  return NS_OK;
}

nsresult
SoundMediaHeaderBox::Write()
{
  WRITE_FULLBOX(mControl, size)
  mControl->Write(balance);
  mControl->Write(reserved);

  return NS_OK;
}

SoundMediaHeaderBox::SoundMediaHeaderBox(ISOControl* aControl)
  : FullBox(NS_LITERAL_CSTRING("smhd"), 0, 0, aControl)
{
  MOZ_COUNT_CTOR(SoundMediaHeaderBox);
}

SoundMediaHeaderBox::~SoundMediaHeaderBox()
{
  MOZ_COUNT_DTOR(SoundMediaHeaderBox);
}

MediaInformationBox::MediaInformationBox(uint32_t aType, ISOControl* aControl)
  : DefaultContainerImpl(NS_LITERAL_CSTRING("minf"), aControl)
{
  mTrackType = aType;

  if (mTrackType == Audio_Track) {
    boxes.AppendElement(new SoundMediaHeaderBox(aControl));
  } else if (mTrackType == Video_Track) {
    boxes.AppendElement(new VideoMediaHeaderBox(aControl));
  } else {
    MOZ_ASSERT(0);
  }

  boxes.AppendElement(new DataInformationBox(aControl));
  boxes.AppendElement(new SampleTableBox(aType, aControl));
  MOZ_COUNT_CTOR(MediaInformationBox);
}

MediaInformationBox::~MediaInformationBox()
{
  MOZ_COUNT_DTOR(MediaInformationBox);
}

nsresult
HandlerBox::Generate(uint32_t* aBoxSize)
{
  pre_defined = 0;
  if (mTrackType == Audio_Track) {
    handler_type = FOURCC('s', 'o', 'u', 'n');
  } else if (mTrackType == Video_Track) {
    handler_type = FOURCC('v', 'i', 'd', 'e');
  }

  size += sizeof(pre_defined) +
          sizeof(handler_type) +
          sizeof(reserved);

  *aBoxSize = size;

  return NS_OK;
}

nsresult
HandlerBox::Write()
{
  WRITE_FULLBOX(mControl, size)
  mControl->Write(pre_defined);
  mControl->Write(handler_type);
  mControl->WriteArray(reserved, 3);

  return NS_OK;
}

HandlerBox::HandlerBox(uint32_t aType, ISOControl* aControl)
  : FullBox(NS_LITERAL_CSTRING("hdlr"), 0, 0, aControl)
  , pre_defined(0)
  , handler_type(0)
{
  mTrackType = aType;
  memset(reserved, 0 , sizeof(reserved));
  MOZ_COUNT_CTOR(HandlerBox);
}

HandlerBox::~HandlerBox()
{
  MOZ_COUNT_DTOR(HandlerBox);
}

MediaHeaderBox::MediaHeaderBox(uint32_t aType, ISOControl* aControl)
  : FullBox(NS_LITERAL_CSTRING("mdhd"), 0, 0, aControl)
  , creation_time(0)
  , modification_time(0)
  , timescale(0)
  , duration(0)
  , pad(0)
  , lang1(0)
  , lang2(0)
  , lang3(0)
  , pre_defined(0)
{
  mTrackType = aType;
  MOZ_COUNT_CTOR(MediaHeaderBox);
}

MediaHeaderBox::~MediaHeaderBox()
{
  MOZ_COUNT_DTOR(MediaHeaderBox);
}

uint32_t
MediaHeaderBox::GetTimeScale()
{
  if (mTrackType == Audio_Track) {
    return mAudioMeta->GetAudioSampleRate();
  }

  return mVideoMeta->GetVideoClockRate();
}

nsresult
MediaHeaderBox::Generate(uint32_t* aBoxSize)
{
  creation_time = mControl->GetTime();
  modification_time = mControl->GetTime();
  timescale = GetTimeScale();
  duration = 0; // fragmented mp4

  pad = 0;
  lang1 = 'u' - 0x60; // "und" underdetermined language
  lang2 = 'n' - 0x60;
  lang3 = 'd' - 0x60;
  size += (pad.size() + lang1.size() + lang2.size() + lang3.size()) / CHAR_BIT;

  pre_defined = 0;
  size += sizeof(creation_time) +
          sizeof(modification_time) +
          sizeof(timescale) +
          sizeof(duration) +
          sizeof(pre_defined);

  *aBoxSize = size;

  return NS_OK;
}

nsresult
MediaHeaderBox::Write()
{
  WRITE_FULLBOX(mControl, size)
  mControl->Write(creation_time);
  mControl->Write(modification_time);
  mControl->Write(timescale);
  mControl->Write(duration);
  mControl->WriteBits(pad.to_ulong(), pad.size());
  mControl->WriteBits(lang1.to_ulong(), lang1.size());
  mControl->WriteBits(lang2.to_ulong(), lang2.size());
  mControl->WriteBits(lang3.to_ulong(), lang3.size());
  mControl->Write(pre_defined);

  return NS_OK;
}

MovieBox::MovieBox(ISOControl* aControl)
  : DefaultContainerImpl(NS_LITERAL_CSTRING("moov"), aControl)
{
  boxes.AppendElement(new MovieHeaderBox(aControl));
  if (aControl->HasAudioTrack()) {
    boxes.AppendElement(new TrackBox(Audio_Track, aControl));
  }
  if (aControl->HasVideoTrack()) {
    boxes.AppendElement(new TrackBox(Video_Track, aControl));
  }
  boxes.AppendElement(new MovieExtendsBox(aControl));
  MOZ_COUNT_CTOR(MovieBox);
}

MovieBox::~MovieBox()
{
  MOZ_COUNT_DTOR(MovieBox);
}

nsresult
MovieHeaderBox::Generate(uint32_t* aBoxSize)
{
  creation_time = mControl->GetTime();
  modification_time = mControl->GetTime();
  timescale = GetTimeScale();
  duration = 0;     // The duration is always 0 in fragmented mp4.
  next_track_ID = mControl->GetNextTrackID();

  size += sizeof(next_track_ID) +
          sizeof(creation_time) +
          sizeof(modification_time) +
          sizeof(timescale) +
          sizeof(duration) +
          sizeof(rate) +
          sizeof(volume) +
          sizeof(reserved16) +
          sizeof(reserved32) +
          sizeof(matrix) +
          sizeof(pre_defined);

  *aBoxSize = size;

  return NS_OK;
}

nsresult
MovieHeaderBox::Write()
{
  WRITE_FULLBOX(mControl, size)
  mControl->Write(creation_time);
  mControl->Write(modification_time);
  mControl->Write(timescale);
  mControl->Write(duration);
  mControl->Write(rate);
  mControl->Write(volume);
  mControl->Write(reserved16);
  mControl->WriteArray(reserved32, 2);
  mControl->WriteArray(matrix, 9);
  mControl->WriteArray(pre_defined, 6);
  mControl->Write(next_track_ID);

  return NS_OK;
}

uint32_t
MovieHeaderBox::GetTimeScale()
{
  // Only audio track in container.
  if (mAudioMeta && !mVideoMeta) {
    return mAudioMeta->GetAudioSampleRate();
  }

  // return video rate
  return mVideoMeta->GetVideoClockRate();
}

MovieHeaderBox::~MovieHeaderBox()
{
  MOZ_COUNT_DTOR(MovieHeaderBox);
}

MovieHeaderBox::MovieHeaderBox(ISOControl* aControl)
  : FullBox(NS_LITERAL_CSTRING("mvhd"), 0, 0, aControl)
  , creation_time(0)
  , modification_time(0)
  , timescale(90000)
  , duration(0)
  , rate(0x00010000)
  , volume(0x0100)
  , reserved16(0)
  , next_track_ID(1)
{
  memcpy(matrix, iso_matrix, sizeof(matrix));
  memset(reserved32, 0, sizeof(reserved32));
  memset(pre_defined, 0, sizeof(pre_defined));
  MOZ_COUNT_CTOR(MovieHeaderBox);
}

TrackHeaderBox::TrackHeaderBox(uint32_t aType, ISOControl* aControl)
  : FullBox(NS_LITERAL_CSTRING("tkhd"), 0,
            flags_track_enabled | flags_track_in_movie | flags_track_in_preview,
            aControl)
  , creation_time(0)
  , modification_time(0)
  , track_ID(0)
  , reserved(0)
  , duration(0)
  , layer(0)
  , alternate_group(0)
  , volume(0)
  , reserved3(0)
  , width(0)
  , height(0)
{
  mTrackType = aType;
  memcpy(matrix, iso_matrix, sizeof(matrix));
  memset(reserved2, 0, sizeof(reserved2));
  MOZ_COUNT_CTOR(TrackHeaderBox);
}

TrackHeaderBox::~TrackHeaderBox()
{
  MOZ_COUNT_DTOR(TrackHeaderBox);
}

nsresult
TrackHeaderBox::Generate(uint32_t* aBoxSize)
{
  creation_time = mControl->GetTime();
  modification_time = mControl->GetTime();
  track_ID = (mTrackType == Audio_Track ?
                mControl->GetTrackID(mAudioMeta->GetKind()) :
                mControl->GetTrackID(mVideoMeta->GetKind()));
  // fragmented mp4
  duration = 0;

  // volume, audiotrack is always 0x0100 in 14496-12 8.3.2.2
  volume = (mTrackType == Audio_Track ? 0x0100 : 0);

  if (mTrackType == Video_Track) {
    width = mVideoMeta->GetVideoDisplayWidth() << 16;
    height = mVideoMeta->GetVideoDisplayHeight() << 16;
    // Check display size, using the pixel size if any of them is invalid.
    if (!width || !height) {
      width = mVideoMeta->GetVideoWidth() << 16;
      height = mVideoMeta->GetVideoHeight() << 16;
    }
  }

  size += sizeof(creation_time) +
          sizeof(modification_time) +
          sizeof(track_ID) +
          sizeof(reserved) +
          sizeof(duration) +
          sizeof(reserved2) +
          sizeof(layer) +
          sizeof(alternate_group) +
          sizeof(volume) +
          sizeof(reserved3) +
          sizeof(matrix) +
          sizeof(width) +
          sizeof(height);

  *aBoxSize = size;

  return NS_OK;
}

nsresult
TrackHeaderBox::Write()
{
  WRITE_FULLBOX(mControl, size)
  mControl->Write(creation_time);
  mControl->Write(modification_time);
  mControl->Write(track_ID);
  mControl->Write(reserved);
  mControl->Write(duration);
  mControl->WriteArray(reserved2, 2);
  mControl->Write(layer);
  mControl->Write(alternate_group);
  mControl->Write(volume);
  mControl->Write(reserved3);
  mControl->WriteArray(matrix, 9);
  mControl->Write(width);
  mControl->Write(height);

  return NS_OK;
}

nsresult
FileTypeBox::Generate(uint32_t* aBoxSize)
{
  minor_version = 0;

  if (mControl->GetMuxingType() == ISOMediaWriter::TYPE_FRAG_MP4) {
    if (!mControl->HasVideoTrack() && mControl->HasAudioTrack()) {
      major_brand = "M4A ";
    } else {
      major_brand = "MP42";
    }
    compatible_brands.AppendElement("mp42");
    compatible_brands.AppendElement("isom");
  } else if (mControl->GetMuxingType() == ISOMediaWriter::TYPE_FRAG_3GP) {
    major_brand = "3gp9";
    // According to 3GPP TS 26.244 V12.2.0, section 5.3.4, it's recommended to
    // list all compatible brands here. 3GP spec supports fragment from '3gp6'.
    compatible_brands.AppendElement("3gp9");
    compatible_brands.AppendElement("3gp8");
    compatible_brands.AppendElement("3gp7");
    compatible_brands.AppendElement("3gp6");
    compatible_brands.AppendElement("isom");
  } else {
    MOZ_ASSERT(0);
  }

  size += major_brand.Length() +
          sizeof(minor_version) +
          compatible_brands.Length() * 4;

  *aBoxSize = size;

  return NS_OK;
}

nsresult
FileTypeBox::Write()
{
  BoxSizeChecker checker(mControl, size);
  Box::Write();
  mControl->WriteFourCC(major_brand.get());
  mControl->Write(minor_version);
  uint32_t len = compatible_brands.Length();
  for (uint32_t i = 0; i < len; i++) {
    mControl->WriteFourCC(compatible_brands[i].get());
  }

  return NS_OK;
}

FileTypeBox::FileTypeBox(ISOControl* aControl)
  : Box(NS_LITERAL_CSTRING("ftyp"), aControl)
  , minor_version(0)
{
  MOZ_COUNT_CTOR(FileTypeBox);
}

FileTypeBox::~FileTypeBox()
{
  MOZ_COUNT_DTOR(FileTypeBox);
}

MediaBox::MediaBox(uint32_t aType, ISOControl* aControl)
  : DefaultContainerImpl(NS_LITERAL_CSTRING("mdia"), aControl)
{
  mTrackType = aType;
  boxes.AppendElement(new MediaHeaderBox(aType, aControl));
  boxes.AppendElement(new HandlerBox(aType, aControl));
  boxes.AppendElement(new MediaInformationBox(aType, aControl));
  MOZ_COUNT_CTOR(MediaBox);
}

MediaBox::~MediaBox()
{
  MOZ_COUNT_DTOR(MediaBox);
}

nsresult
DefaultContainerImpl::Generate(uint32_t* aBoxSize)
{
  nsresult rv;
  uint32_t box_size;
  uint32_t len = boxes.Length();
  for (uint32_t i = 0; i < len; i++) {
    rv = boxes.ElementAt(i)->Generate(&box_size);
    NS_ENSURE_SUCCESS(rv, rv);
    size += box_size;
  }
  *aBoxSize = size;
  return NS_OK;
}

nsresult
DefaultContainerImpl::Find(const nsACString& aType,
                           nsTArray<nsRefPtr<MuxerOperation>>& aOperations)
{
  nsresult rv = Box::Find(aType, aOperations);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t len = boxes.Length();
  for (uint32_t i = 0; i < len; i++) {
    rv = boxes.ElementAt(i)->Find(aType, aOperations);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult
DefaultContainerImpl::Write()
{
  BoxSizeChecker checker(mControl, size);
  Box::Write();

  nsresult rv;
  uint32_t len = boxes.Length();
  for (uint32_t i = 0; i < len; i++) {
    rv = boxes.ElementAt(i)->Write();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

DefaultContainerImpl::DefaultContainerImpl(const nsACString& aType,
                                           ISOControl* aControl)
  : Box(aType, aControl)
{
}

nsresult
Box::Write()
{
  mControl->Write(size);
  mControl->WriteFourCC(boxType.get());
  return NS_OK;
}

nsresult
Box::Find(const nsACString& aType, nsTArray<nsRefPtr<MuxerOperation>>& aOperations)
{
  if (boxType == aType) {
    aOperations.AppendElement(this);
  }
  return NS_OK;
}

Box::Box(const nsACString& aType, ISOControl* aControl)
  : size(8), mControl(aControl)
{
  MOZ_ASSERT(aType.Length() == 4);
  boxType = aType;
  aControl->GetAudioMetadata(mAudioMeta);
  aControl->GetVideoMetadata(mVideoMeta);
}

FullBox::FullBox(const nsACString& aType, uint8_t aVersion, uint32_t aFlags,
                 ISOControl* aControl)
  : Box(aType, aControl)
{
  // Cast to uint64_t due to VC2010  bug.
  std::bitset<24> tmp_flags((uint64_t)aFlags);
  version = aVersion;
  flags = tmp_flags;
  size += sizeof(version) + flags.size() / CHAR_BIT;
}

nsresult
FullBox::Write()
{
  Box::Write();
  mControl->Write(version);
  mControl->WriteBits(flags.to_ulong(), flags.size());
  return NS_OK;
}

TrackBox::TrackBox(uint32_t aTrackType, ISOControl* aControl)
  : DefaultContainerImpl(NS_LITERAL_CSTRING("trak"), aControl)
{
  boxes.AppendElement(new TrackHeaderBox(aTrackType, aControl));
  boxes.AppendElement(new MediaBox(aTrackType, aControl));
  MOZ_COUNT_CTOR(TrackBox);
}

TrackBox::~TrackBox()
{
  MOZ_COUNT_DTOR(TrackBox);
}

SampleEntryBox::SampleEntryBox(const nsACString& aFormat, ISOControl* aControl)
  : Box(aFormat, aControl)
  , data_reference_index(0)
{
  data_reference_index = 1; // There is only one data reference in each track.
  size += sizeof(reserved) +
          sizeof(data_reference_index);
  memset(reserved, 0, sizeof(reserved));
}

nsresult
SampleEntryBox::Write()
{
  Box::Write();
  mControl->Write(reserved, sizeof(reserved));
  mControl->Write(data_reference_index);
  return NS_OK;
}

nsresult
AudioSampleEntry::Write()
{
  SampleEntryBox::Write();
  mControl->Write(sound_version);
  mControl->Write(reserved2, sizeof(reserved2));
  mControl->Write(channels);
  mControl->Write(sample_size);
  mControl->Write(compressionId);
  mControl->Write(packet_size);
  mControl->Write(timeScale);
  return NS_OK;
}

AudioSampleEntry::AudioSampleEntry(const nsACString& aFormat, ISOControl* aControl)
  : SampleEntryBox(aFormat, aControl)
  , sound_version(0)
  , channels(2)
  , sample_size(16)
  , compressionId(0)
  , packet_size(0)
  , timeScale(0)
{
  memset(reserved2, 0 , sizeof(reserved2));
  channels = mAudioMeta->GetAudioChannels();
  timeScale = mAudioMeta->GetAudioSampleRate() << 16;

  size += sizeof(sound_version) +
          sizeof(reserved2) +
          sizeof(sample_size) +
          sizeof(channels) +
          sizeof(packet_size) +
          sizeof(compressionId) +
          sizeof(timeScale);

  MOZ_COUNT_CTOR(AudioSampleEntry);
}

AudioSampleEntry::~AudioSampleEntry()
{
  MOZ_COUNT_DTOR(AudioSampleEntry);
}

nsresult
VisualSampleEntry::Write()
{
  SampleEntryBox::Write();

  mControl->Write(reserved, sizeof(reserved));
  mControl->Write(width);
  mControl->Write(height);
  mControl->Write(horizresolution);
  mControl->Write(vertresolution);
  mControl->Write(reserved2);
  mControl->Write(frame_count);
  mControl->Write(compressorName, sizeof(compressorName));
  mControl->Write(depth);
  mControl->Write(pre_defined);

  return NS_OK;
}

VisualSampleEntry::VisualSampleEntry(const nsACString& aFormat, ISOControl* aControl)
  : SampleEntryBox(aFormat, aControl)
  , width(0)
  , height(0)
  , horizresolution(resolution_72_dpi)
  , vertresolution(resolution_72_dpi)
  , reserved2(0)
  , frame_count(1)
  , depth(video_depth)
  , pre_defined(-1)
{
  memset(reserved, 0 , sizeof(reserved));
  memset(compressorName, 0 , sizeof(compressorName));

  // both fields occupy 16 bits defined in 14496-2 6.2.3.
  width = mVideoMeta->GetVideoWidth();
  height = mVideoMeta->GetVideoHeight();

  size += sizeof(reserved) +
          sizeof(width) +
          sizeof(height) +
          sizeof(horizresolution) +
          sizeof(vertresolution) +
          sizeof(reserved2) +
          sizeof(frame_count) +
          sizeof(compressorName) +
          sizeof(depth) +
          sizeof(pre_defined);

  MOZ_COUNT_CTOR(VisualSampleEntry);
}

VisualSampleEntry::~VisualSampleEntry()
{
  MOZ_COUNT_DTOR(VisualSampleEntry);
}

}
