/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaveDemuxer.h"

#include <inttypes.h>
#include <algorithm>

#include "mozilla/Assertions.h"
#include "mozilla/EndianUtils.h"
#include "BufferReader.h"
#include "VideoUtils.h"
#include "TimeUnits.h"

using mozilla::media::TimeUnit;
using mozilla::media::TimeIntervals;

namespace mozilla {

// WAVDemuxer

WAVDemuxer::WAVDemuxer(MediaResource* aSource)
  : mSource(aSource)
{
  DDLINKCHILD("source", aSource);
}

bool
WAVDemuxer::InitInternal()
{
  if (!mTrackDemuxer) {
    mTrackDemuxer = new WAVTrackDemuxer(mSource.GetResource());
    DDLINKCHILD("track demuxer", mTrackDemuxer.get());
  }
  return mTrackDemuxer->Init();
}

RefPtr<WAVDemuxer::InitPromise>
WAVDemuxer::Init()
{
  if (!InitInternal()) {
    return InitPromise::CreateAndReject(
      NS_ERROR_DOM_MEDIA_METADATA_ERR, __func__);
  }
  return InitPromise::CreateAndResolve(NS_OK, __func__);
}

uint32_t
WAVDemuxer::GetNumberTracks(TrackInfo::TrackType aType) const
{
  return aType == TrackInfo::kAudioTrack ? 1u : 0u;
}

already_AddRefed<MediaTrackDemuxer>
WAVDemuxer::GetTrackDemuxer(TrackInfo::TrackType aType, uint32_t aTrackNumber)
{
  if (!mTrackDemuxer) {
    return nullptr;
  }
  return RefPtr<WAVTrackDemuxer>(mTrackDemuxer).forget();
}

bool
WAVDemuxer::IsSeekable() const
{
  return true;
}

// WAVTrackDemuxer

WAVTrackDemuxer::WAVTrackDemuxer(MediaResource* aSource)
  : mSource(aSource)
  , mOffset(0)
  , mFirstChunkOffset(0)
  , mNumParsedChunks(0)
  , mChunkIndex(0)
  , mTotalChunkLen(0)
  , mSamplesPerChunk(0)
  , mSamplesPerSecond(0)
  , mChannels(0)
{
  DDLINKCHILD("source", aSource);
  Reset();
}

bool
WAVTrackDemuxer::Init()
{
  Reset();
  FastSeek(TimeUnit());

  if (!mInfo) {
    mInfo = MakeUnique<AudioInfo>();
  }

  if (!RIFFParserInit()) {
    return false;
  }

  while (true) {
    if (!HeaderParserInit()) {
      return false;
    }

    uint32_t aChunkName = mHeaderParser.GiveHeader().ChunkName();
    uint32_t aChunkSize = mHeaderParser.GiveHeader().ChunkSize();

    if (aChunkName == FRMT_CODE) {
      if (!FmtChunkParserInit()) {
        return false;
      }
    } else if (aChunkName == LIST_CODE) {
      mHeaderParser.Reset();
      uint64_t endOfListChunk = static_cast<uint64_t>(mOffset) + aChunkSize;
      if (endOfListChunk > UINT32_MAX) {
        return false;
      }
      if (!ListChunkParserInit(aChunkSize)) {
        mOffset = endOfListChunk;
      }
    } else if (aChunkName == DATA_CODE) {
      mDataLength = aChunkSize;
      if (mFirstChunkOffset != mOffset) {
        mFirstChunkOffset = mOffset;
      }
      break;
    } else {
      mOffset += aChunkSize; // Skip other irrelevant chunks.
    }
    if (mOffset & 1) {
      // Wave files are 2-byte aligned so we need to round up
      mOffset += 1;
    }
    mHeaderParser.Reset();
  }

  if (mDataLength > StreamLength() - mFirstChunkOffset) {
    mDataLength = StreamLength() - mFirstChunkOffset;
  }

  mSamplesPerSecond = mFmtParser.FmtChunk().SampleRate();
  mChannels = mFmtParser.FmtChunk().Channels();
  mSampleFormat = mFmtParser.FmtChunk().SampleFormat();
  if (!mSamplesPerSecond || !mChannels || !mSampleFormat) {
    return false;
  }
  mSamplesPerChunk = DATA_CHUNK_SIZE * 8 / mChannels / mSampleFormat;

  mInfo->mRate = mSamplesPerSecond;
  mInfo->mChannels = mChannels;
  mInfo->mBitDepth = mSampleFormat;
  mInfo->mProfile = mFmtParser.FmtChunk().WaveFormat() & 0x00FF;
  mInfo->mExtendedProfile = (mFmtParser.FmtChunk().WaveFormat() & 0xFF00) >> 8;
  mInfo->mMimeType = "audio/wave; codecs=";
  mInfo->mMimeType.AppendInt(mFmtParser.FmtChunk().WaveFormat());
  mInfo->mDuration = Duration();

  return mInfo->mDuration.IsPositive();
}

bool
WAVTrackDemuxer::RIFFParserInit()
{
  RefPtr<MediaRawData> riffHeader = GetFileHeader(FindRIFFHeader());
  if (!riffHeader) {
    return false;
  }
  BufferReader RIFFReader(riffHeader->Data(), 12);
  Unused << mRIFFParser.Parse(RIFFReader);
  return mRIFFParser.RiffHeader().IsValid(11);
}

bool
WAVTrackDemuxer::HeaderParserInit()
{
  RefPtr<MediaRawData> header = GetFileHeader(FindChunkHeader());
  if (!header) {
    return false;
  }
  BufferReader HeaderReader(header->Data(), 8);
  Unused << mHeaderParser.Parse(HeaderReader);
  return true;
}

bool
WAVTrackDemuxer::FmtChunkParserInit()
{
  RefPtr<MediaRawData> fmtChunk = GetFileHeader(FindFmtChunk());
  if (!fmtChunk) {
    return false;
  }
  BufferReader fmtReader(fmtChunk->Data(),
                         mHeaderParser.GiveHeader().ChunkSize());
  Unused << mFmtParser.Parse(fmtReader);
  return true;
}

bool
WAVTrackDemuxer::ListChunkParserInit(uint32_t aChunkSize)
{
  uint32_t bytesRead = 0;

  RefPtr<MediaRawData> infoTag = GetFileHeader(FindInfoTag());
  if (!infoTag) {
    return false;
  }

  BufferReader infoTagReader(infoTag->Data(), 4);
  auto res = infoTagReader.ReadU32();
  if (res.isErr() || (res.isOk() && res.unwrap() != INFO_CODE)) {
    return false;
  }

  bytesRead += 4;

  while (bytesRead < aChunkSize) {
    if (!HeaderParserInit()) {
      return false;
    }

    bytesRead += 8;

    uint32_t id = mHeaderParser.GiveHeader().ChunkName();
    uint32_t length = mHeaderParser.GiveHeader().ChunkSize();

    // SubChunk Length Cannot Exceed List Chunk length.
    if (length > aChunkSize - bytesRead) {
      length = aChunkSize - bytesRead;
    }

    MediaByteRange mRange = { mOffset, mOffset + length };
    RefPtr<MediaRawData> mChunkData = GetFileHeader(mRange);
    if (!mChunkData) {
      return false;
    }

    const char* rawData = reinterpret_cast<const char*>(mChunkData->Data());

    nsCString val(rawData, length);
    if (length > 0 && val[length - 1] == '\0') {
      val.SetLength(length - 1);
    }

    if (length % 2) {
      mOffset += 1;
      length += length % 2;
    }

    bytesRead += length;

    if (!IsUTF8(val)) {
      mHeaderParser.Reset();
      continue;
    }

    switch (id) {
      case 0x49415254:                // IART
        mInfo->mTags.AppendElement(
          MetadataTag(NS_LITERAL_CSTRING("artist"), val));
        break;
      case 0x49434d54:                // ICMT
        mInfo->mTags.AppendElement(
          MetadataTag(NS_LITERAL_CSTRING("comments"), val));
        break;
      case 0x49474e52:                // IGNR
        mInfo->mTags.AppendElement(
          MetadataTag(NS_LITERAL_CSTRING("genre"), val));
        break;
      case 0x494e414d:                // INAM
        mInfo->mTags.AppendElement(
          MetadataTag(NS_LITERAL_CSTRING("name"), val));
        break;
    }

    mHeaderParser.Reset();
  }
  return true;
}

media::TimeUnit
WAVTrackDemuxer::SeekPosition() const
{
  TimeUnit pos = Duration(mChunkIndex);
  if (Duration() > TimeUnit()) {
    pos = std::min(Duration(), pos);
  }
  return pos;
}

RefPtr<MediaRawData>
WAVTrackDemuxer::DemuxSample()
{
  return GetNextChunk(FindNextChunk());
}

UniquePtr<TrackInfo>
WAVTrackDemuxer::GetInfo() const
{
  return mInfo->Clone();
}

RefPtr<WAVTrackDemuxer::SeekPromise>
WAVTrackDemuxer::Seek(const TimeUnit& aTime)
{
  FastSeek(aTime);
  const TimeUnit seekTime = ScanUntil(aTime);
  return SeekPromise::CreateAndResolve(seekTime, __func__);
}

TimeUnit
WAVTrackDemuxer::FastSeek(const TimeUnit& aTime)
{
  if (aTime.ToMicroseconds()) {
    mChunkIndex = ChunkIndexFromTime(aTime);
  } else {
    mChunkIndex = 0;
  }

  mOffset = OffsetFromChunkIndex(mChunkIndex);

  if (mOffset > mFirstChunkOffset && StreamLength() > 0) {
    mOffset = std::min(StreamLength() - 1, mOffset);
  }

  return Duration(mChunkIndex);
}

TimeUnit
WAVTrackDemuxer::ScanUntil(const TimeUnit& aTime)
{
  if (!aTime.ToMicroseconds()) {
    return FastSeek(aTime);
  }

  if (Duration(mChunkIndex) > aTime) {
    FastSeek(aTime);
  }

  return SeekPosition();
}

RefPtr<WAVTrackDemuxer::SamplesPromise>
WAVTrackDemuxer::GetSamples(int32_t aNumSamples)
{
  MOZ_ASSERT(aNumSamples);

  RefPtr<SamplesHolder> datachunks = new SamplesHolder();

  while (aNumSamples--) {
    RefPtr<MediaRawData> datachunk = GetNextChunk(FindNextChunk());
    if (!datachunk) {
      break;
    }
    datachunks->mSamples.AppendElement(datachunk);
  }

  if (datachunks->mSamples.IsEmpty()) {
    return SamplesPromise::CreateAndReject(
      NS_ERROR_DOM_MEDIA_END_OF_STREAM, __func__);
  }

  return SamplesPromise::CreateAndResolve(datachunks, __func__);
}

void
WAVTrackDemuxer::Reset()
{
  FastSeek(TimeUnit());
  mParser.Reset();
  mHeaderParser.Reset();
  mRIFFParser.Reset();
  mFmtParser.Reset();
}

RefPtr<WAVTrackDemuxer::SkipAccessPointPromise>
WAVTrackDemuxer::SkipToNextRandomAccessPoint(const TimeUnit& aTimeThreshold)
{
  return SkipAccessPointPromise::CreateAndReject(
    SkipFailureHolder(NS_ERROR_DOM_MEDIA_DEMUXER_ERR, 0), __func__);
}

int64_t
WAVTrackDemuxer::GetResourceOffset() const
{
  return mOffset;
}

TimeIntervals
WAVTrackDemuxer::GetBuffered()
{
  TimeUnit duration = Duration();

  if (duration <= TimeUnit()) {
    return TimeIntervals();
  }

  AutoPinned<MediaResource> stream(mSource.GetResource());
  return GetEstimatedBufferedTimeRanges(stream, duration.ToMicroseconds());
}

int64_t
WAVTrackDemuxer::StreamLength() const
{
  return mSource.GetLength();
}

TimeUnit
WAVTrackDemuxer::Duration() const
{
  if (!mDataLength ||!mChannels || !mSampleFormat) {
    return TimeUnit();
  }

  int64_t numSamples =
    static_cast<int64_t>(mDataLength) * 8 / mChannels / mSampleFormat;

  int64_t numUSeconds = USECS_PER_S * numSamples / mSamplesPerSecond;

  if (USECS_PER_S * numSamples % mSamplesPerSecond > mSamplesPerSecond / 2) {
    numUSeconds++;
  }

  return TimeUnit::FromMicroseconds(numUSeconds);
}

TimeUnit
WAVTrackDemuxer::Duration(int64_t aNumDataChunks) const
{
  if (!mSamplesPerSecond || !mSamplesPerChunk) {
    return TimeUnit();
  }
  const double usPerDataChunk =
    USECS_PER_S * static_cast<double>(mSamplesPerChunk) / mSamplesPerSecond;
  return TimeUnit::FromMicroseconds(aNumDataChunks * usPerDataChunk);
}

TimeUnit
WAVTrackDemuxer::DurationFromBytes(uint32_t aNumBytes) const
{
  if (!mSamplesPerSecond || !mChannels || !mSampleFormat) {
    return TimeUnit();
  }

  int64_t numSamples = aNumBytes * 8 / mChannels / mSampleFormat;

  int64_t numUSeconds = USECS_PER_S * numSamples / mSamplesPerSecond;

  if (USECS_PER_S * numSamples % mSamplesPerSecond > mSamplesPerSecond / 2) {
    numUSeconds++;
  }

  return TimeUnit::FromMicroseconds(numUSeconds);
}

MediaByteRange
WAVTrackDemuxer::FindNextChunk()
{
  if (mOffset + DATA_CHUNK_SIZE < mFirstChunkOffset + mDataLength) {
    return { mOffset, mOffset + DATA_CHUNK_SIZE };
  } else {
    return { mOffset, mFirstChunkOffset + mDataLength };
  }
}

MediaByteRange
WAVTrackDemuxer::FindChunkHeader()
{
  return { mOffset, mOffset + CHUNK_HEAD_SIZE };
}

MediaByteRange
WAVTrackDemuxer::FindRIFFHeader()
{
  return { mOffset, mOffset + RIFF_CHUNK_SIZE };
}

MediaByteRange
WAVTrackDemuxer::FindFmtChunk()
{
  return { mOffset, mOffset + mHeaderParser.GiveHeader().ChunkSize() };
}

MediaByteRange
WAVTrackDemuxer::FindListChunk()
{
  return { mOffset, mOffset + mHeaderParser.GiveHeader().ChunkSize() };
}

MediaByteRange
WAVTrackDemuxer::FindInfoTag()
{
  return { mOffset, mOffset + 4 };
}

bool
WAVTrackDemuxer::SkipNextChunk(const MediaByteRange& aRange)
{
  UpdateState(aRange);
  return true;
}

already_AddRefed<MediaRawData>
WAVTrackDemuxer::GetNextChunk(const MediaByteRange& aRange)
{
  if (!aRange.Length()) {
    return nullptr;
  }

  RefPtr<MediaRawData> datachunk = new MediaRawData();
  datachunk->mOffset = aRange.mStart;

  UniquePtr<MediaRawDataWriter> chunkWriter(datachunk->CreateWriter());
  if (!chunkWriter->SetSize(aRange.Length())) {
    return nullptr;
  }

  const uint32_t read =
    Read(chunkWriter->Data(), datachunk->mOffset, datachunk->Size());

  if (read != aRange.Length()) {
    return nullptr;
  }

  UpdateState(aRange);
  ++mNumParsedChunks;
  ++mChunkIndex;

  datachunk->mTime = Duration(mChunkIndex - 1);

  if (static_cast<uint32_t>(mChunkIndex) * DATA_CHUNK_SIZE < mDataLength) {
    datachunk->mDuration = Duration(1);
  } else {
    uint32_t mBytesRemaining =
      mDataLength - mChunkIndex * DATA_CHUNK_SIZE;
    datachunk->mDuration = DurationFromBytes(mBytesRemaining);
  }
  datachunk->mTimecode = datachunk->mTime;
  datachunk->mKeyframe = true;

  MOZ_ASSERT(!datachunk->mTime.IsNegative());
  MOZ_ASSERT(!datachunk->mDuration.IsNegative());

  return datachunk.forget();
}

already_AddRefed<MediaRawData>
WAVTrackDemuxer::GetFileHeader(const MediaByteRange& aRange)
{
  if (!aRange.Length()) {
    return nullptr;
  }

  RefPtr<MediaRawData> fileHeader = new MediaRawData();
  fileHeader->mOffset = aRange.mStart;

  UniquePtr<MediaRawDataWriter> headerWriter(fileHeader->CreateWriter());
  if (!headerWriter->SetSize(aRange.Length())) {
    return nullptr;
  }

  const uint32_t read =
    Read(headerWriter->Data(), fileHeader->mOffset, fileHeader->Size());

  if (read != aRange.Length()) {
    return nullptr;
  }

  UpdateState(aRange);

  return fileHeader.forget();
}

int64_t
WAVTrackDemuxer::OffsetFromChunkIndex(int64_t aChunkIndex) const
{
  return mFirstChunkOffset + aChunkIndex * DATA_CHUNK_SIZE;
}

int64_t
WAVTrackDemuxer::ChunkIndexFromOffset(int64_t aOffset) const
{
  int64_t chunkIndex = (aOffset - mFirstChunkOffset) / DATA_CHUNK_SIZE;
  return std::max<int64_t>(0, chunkIndex);
}

int64_t
WAVTrackDemuxer::ChunkIndexFromTime(const media::TimeUnit& aTime) const
{
  if (!mSamplesPerChunk || !mSamplesPerSecond) {
    return 0;
  }
  int64_t chunkIndex =
    (aTime.ToSeconds() * mSamplesPerSecond / mSamplesPerChunk) - 1;
  return chunkIndex;
}

void
WAVTrackDemuxer::UpdateState(const MediaByteRange& aRange)
{
  // Full chunk parsed, move offset to its end.
  mOffset = aRange.mEnd;
  mTotalChunkLen += aRange.Length();
}

int32_t
WAVTrackDemuxer::Read(uint8_t* aBuffer, int64_t aOffset, int32_t aSize)
{
  const int64_t streamLen = StreamLength();
  if (mInfo && streamLen > 0) {
    aSize = std::min<int64_t>(aSize, streamLen - aOffset);
  }
  uint32_t read = 0;
  const nsresult rv = mSource.ReadAt(aOffset,
                                     reinterpret_cast<char*>(aBuffer),
                                     static_cast<uint32_t>(aSize),
                                     &read);
  NS_ENSURE_SUCCESS(rv, 0);
  return static_cast<int32_t>(read);
}

// RIFFParser

Result<uint32_t, nsresult>
RIFFParser::Parse(BufferReader& aReader)
{
  for (auto res = aReader.ReadU8();
       res.isOk() && !mRiffHeader.ParseNext(res.unwrap()); res = aReader.ReadU8())
  {}

  if (mRiffHeader.IsValid()) {
    return RIFF_CHUNK_SIZE;
  }

  return 0;
}

void
RIFFParser::Reset()
{
  mRiffHeader.Reset();
}

const RIFFParser::RIFFHeader&
RIFFParser::RiffHeader() const
{
  return mRiffHeader;
}

// RIFFParser::RIFFHeader

RIFFParser::RIFFHeader::RIFFHeader()
{
  Reset();
}

void
RIFFParser::RIFFHeader::Reset()
{
  memset(mRaw, 0, sizeof(mRaw));
  mPos = 0;
}

bool
RIFFParser::RIFFHeader::ParseNext(uint8_t c)
{
  if (!Update(c)) {
    Reset();
    if (!Update(c)) {
      Reset();
    }
  }
  return IsValid();
}

bool
RIFFParser::RIFFHeader::IsValid(int aPos) const
{
  if (aPos > -1 && aPos < 4) {
    return RIFF[aPos] == mRaw[aPos];
  } else if (aPos > 7 && aPos < 12) {
    return WAVE[aPos - 8] == mRaw[aPos];
  } else {
    return true;
  }
}

bool
RIFFParser::RIFFHeader::IsValid() const
{
  return mPos >= RIFF_CHUNK_SIZE;
}

bool
RIFFParser::RIFFHeader::Update(uint8_t c)
{
  if (mPos < RIFF_CHUNK_SIZE) {
    mRaw[mPos] = c;
  }
  return IsValid(mPos++);
}

// HeaderParser

Result<uint32_t, nsresult>
HeaderParser::Parse(BufferReader& aReader)
{
  for (auto res = aReader.ReadU8();
       res.isOk() && !mHeader.ParseNext(res.unwrap()); res = aReader.ReadU8())
  {}

  if (mHeader.IsValid()) {
    return CHUNK_HEAD_SIZE;
  }

  return 0;
}

void
HeaderParser::Reset()
{
  mHeader.Reset();
}

const HeaderParser::ChunkHeader&
HeaderParser::GiveHeader() const
{
  return mHeader;
}

// HeaderParser::ChunkHeader

HeaderParser::ChunkHeader::ChunkHeader()
{
  Reset();
}

void
HeaderParser::ChunkHeader::Reset()
{
  memset(mRaw, 0, sizeof(mRaw));
  mPos = 0;
}

bool
HeaderParser::ChunkHeader::ParseNext(uint8_t c)
{
  Update(c);
  return IsValid();
}

bool
HeaderParser::ChunkHeader::IsValid() const
{
  return mPos >= CHUNK_HEAD_SIZE;
}

uint32_t
HeaderParser::ChunkHeader::ChunkName() const
{
  return ((mRaw[0] << 24)
          | (mRaw[1] << 16)
          | (mRaw[2] << 8 )
          | (mRaw[3]));
}

uint32_t
HeaderParser::ChunkHeader::ChunkSize() const
{
  return ((mRaw[7] << 24)
          | (mRaw[6] << 16)
          | (mRaw[5] << 8 )
          | (mRaw[4]));
}

void
HeaderParser::ChunkHeader::Update(uint8_t c)
{
  if (mPos < CHUNK_HEAD_SIZE) {
    mRaw[mPos++] = c;
  }
}

// FormatParser

Result<uint32_t, nsresult>
FormatParser::Parse(BufferReader& aReader)
{
  for (auto res = aReader.ReadU8();
       res.isOk() && !mFmtChunk.ParseNext(res.unwrap()); res = aReader.ReadU8())
  {}

  if (mFmtChunk.IsValid()) {
    return FMT_CHUNK_MIN_SIZE;
  }

  return 0;
}

void
FormatParser::Reset()
{
  mFmtChunk.Reset();
}

const FormatParser::FormatChunk&
FormatParser::FmtChunk() const
{
  return mFmtChunk;
}

// FormatParser::FormatChunk

FormatParser::FormatChunk::FormatChunk()
{
  Reset();
}

void
FormatParser::FormatChunk::Reset()
{
  memset(mRaw, 0, sizeof(mRaw));
  mPos = 0;
}

uint16_t
FormatParser::FormatChunk::WaveFormat() const
{
  return (mRaw[1] << 8) | (mRaw[0]);
}

uint16_t
FormatParser::FormatChunk::Channels() const
{
  return (mRaw[3] << 8) | (mRaw[2]);
}

uint32_t
FormatParser::FormatChunk::SampleRate() const
{
  return (mRaw[7] << 24)
         | (mRaw[6] << 16)
         | (mRaw[5] << 8)
         | (mRaw[4]);
}

uint16_t
FormatParser::FormatChunk::FrameSize() const
{
  return (mRaw[13] << 8) | (mRaw[12]);
}

uint16_t
FormatParser::FormatChunk::SampleFormat() const
{
  return (mRaw[15] << 8) | (mRaw[14]);
}

bool
FormatParser::FormatChunk::ParseNext(uint8_t c)
{
  Update(c);
  return IsValid();
}

bool
FormatParser::FormatChunk::IsValid() const
{
  return (FrameSize() == SampleRate() * Channels() / 8) &&
         (mPos >= FMT_CHUNK_MIN_SIZE);
}

void
FormatParser::FormatChunk::Update(uint8_t c)
{
  if (mPos < FMT_CHUNK_MIN_SIZE) {
    mRaw[mPos++] = c;
  }
}

// DataParser

DataParser::DataParser()
{
}

void
DataParser::Reset()
{
  mChunk.Reset();
}

const DataParser::DataChunk&
DataParser::CurrentChunk() const
{
  return mChunk;
}

// DataParser::DataChunk

void
DataParser::DataChunk::Reset()
{
  mPos = 0;
}

} // namespace mozilla
