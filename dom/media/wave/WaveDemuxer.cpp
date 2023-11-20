/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaveDemuxer.h"

#include <inttypes.h>
#include <algorithm>

#include "mozilla/Assertions.h"
#include "mozilla/Utf8.h"
#include "BufferReader.h"
#include "VideoUtils.h"
#include "TimeUnits.h"
#include "mozilla/Logging.h"

using mozilla::media::TimeIntervals;
using mozilla::media::TimeUnit;

extern mozilla::LazyLogModule gMediaDemuxerLog;

namespace mozilla {

#define LOG(msg, ...) \
  MOZ_LOG(gMediaDemuxerLog, LogLevel::Debug, msg, ##__VA_ARGS__)

WAVDemuxer::WAVDemuxer(MediaResource* aSource) : mSource(aSource) {
  DDLINKCHILD("source", aSource);
}

bool WAVDemuxer::InitInternal() {
  if (!mTrackDemuxer) {
    mTrackDemuxer = new WAVTrackDemuxer(mSource.GetResource());
    DDLINKCHILD("track demuxer", mTrackDemuxer.get());
  }
  return mTrackDemuxer->Init();
}

RefPtr<WAVDemuxer::InitPromise> WAVDemuxer::Init() {
  if (!InitInternal()) {
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                                        __func__);
  }
  return InitPromise::CreateAndResolve(NS_OK, __func__);
}

uint32_t WAVDemuxer::GetNumberTracks(TrackInfo::TrackType aType) const {
  return aType == TrackInfo::kAudioTrack ? 1u : 0u;
}

already_AddRefed<MediaTrackDemuxer> WAVDemuxer::GetTrackDemuxer(
    TrackInfo::TrackType aType, uint32_t aTrackNumber) {
  if (!mTrackDemuxer) {
    return nullptr;
  }
  return RefPtr<WAVTrackDemuxer>(mTrackDemuxer).forget();
}

bool WAVDemuxer::IsSeekable() const { return true; }

// WAVTrackDemuxer

WAVTrackDemuxer::WAVTrackDemuxer(MediaResource* aSource)
    : mSource(aSource),
      mOffset(0),
      mFirstChunkOffset(0),
      mNumParsedChunks(0),
      mChunkIndex(0),
      mDataLength(0),
      mTotalChunkLen(0),
      mSamplesPerChunk(0),
      mSamplesPerSecond(0),
      mChannels(0),
      mSampleFormat(0) {
  DDLINKCHILD("source", aSource);
  Reset();
}

bool WAVTrackDemuxer::Init() {
  Reset();
  FastSeek(TimeUnit());

  if (!mInfo) {
    mInfo = MakeUnique<AudioInfo>();
    mInfo->mCodecSpecificConfig =
        AudioCodecSpecificVariant{WaveCodecSpecificData{}};
  }

  if (!RIFFParserInit()) {
    return false;
  }

  bool hasValidFmt = false;

  while (true) {
    if (!HeaderParserInit()) {
      return false;
    }

    uint32_t chunkName = mHeaderParser.GiveHeader().ChunkName();
    uint32_t chunkSize = mHeaderParser.GiveHeader().ChunkSize();

    if (chunkName == FRMT_CODE) {
      hasValidFmt = FmtChunkParserInit();
    } else if (chunkName == LIST_CODE) {
      mHeaderParser.Reset();
      uint64_t endOfListChunk = static_cast<uint64_t>(mOffset) + chunkSize;
      if (endOfListChunk > UINT32_MAX) {
        return false;
      }
      if (!ListChunkParserInit(chunkSize)) {
        mOffset = endOfListChunk;
      }
    } else if (chunkName == DATA_CODE) {
      mDataLength = chunkSize;
      if (mFirstChunkOffset != mOffset) {
        mFirstChunkOffset = mOffset;
      }
      break;
    } else {
      mOffset += chunkSize;  // Skip other irrelevant chunks.
    }
    if (mOffset & 1) {
      // Wave files are 2-byte aligned so we need to round up
      mOffset += 1;
    }
    mHeaderParser.Reset();
  }

  if (!hasValidFmt) {
    return false;
  }

  int64_t streamLength = StreamLength();
  // If the chunk length and the resource length are not equal, use the
  // resource length as the "real" data chunk length, if it's longer than the
  // chunk size.
  if (streamLength != -1) {
    uint64_t streamLengthPositive = static_cast<uint64_t>(streamLength);
    if (streamLengthPositive > mFirstChunkOffset &&
        mDataLength > streamLengthPositive - mFirstChunkOffset) {
      mDataLength = streamLengthPositive - mFirstChunkOffset;
    }
  }

  mSamplesPerSecond = mFmtChunk.SampleRate();
  mChannels = mFmtChunk.Channels();
  if (!mSamplesPerSecond || !mChannels || !mFmtChunk.ValidBitsPerSamples()) {
    return false;
  }
  mSamplesPerChunk =
      DATA_CHUNK_SIZE * 8 / mChannels / mFmtChunk.ValidBitsPerSamples();
  mSampleFormat = mFmtChunk.ValidBitsPerSamples();

  mInfo->mRate = mSamplesPerSecond;
  mInfo->mChannels = mChannels;
  mInfo->mBitDepth = mFmtChunk.ValidBitsPerSamples();
  mInfo->mProfile = AssertedCast<uint8_t>(mFmtChunk.WaveFormat() & 0x00FF);
  mInfo->mExtendedProfile =
      AssertedCast<uint8_t>(mFmtChunk.WaveFormat() & 0xFF00 >> 8);
  mInfo->mMimeType = "audio/wave; codecs=";
  mInfo->mMimeType.AppendInt(mFmtChunk.WaveFormat());
  mInfo->mDuration = Duration();
  mInfo->mChannelMap = mFmtChunk.ChannelMap();

  if (AudioConfig::ChannelLayout::Channels(mInfo->mChannelMap) !=
      mInfo->mChannels) {
    AudioConfig::ChannelLayout::ChannelMap defaultForChannelCount =
        AudioConfig::ChannelLayout(mInfo->mChannels).Map();
    LOG(("Channel count of %" PRIu32
         " and channel layout disagree, overriding channel map from %s to %s",
         mInfo->mChannels,
         AudioConfig::ChannelLayout::ChannelMapToString(mInfo->mChannelMap)
             .get(),
         AudioConfig::ChannelLayout::ChannelMapToString(defaultForChannelCount)
             .get()));
    mInfo->mChannelMap = defaultForChannelCount;
  }
  LOG(("WavDemuxer initialized: %s", mInfo->ToString().get()));

  return mInfo->mDuration.IsPositive();
}

bool WAVTrackDemuxer::RIFFParserInit() {
  RefPtr<MediaRawData> riffHeader = GetFileHeader(FindRIFFHeader());
  if (!riffHeader) {
    return false;
  }
  BufferReader RIFFReader(riffHeader->Data(), 12);
  Unused << mRIFFParser.Parse(RIFFReader);
  return mRIFFParser.RiffHeader().IsValid(11);
}

bool WAVTrackDemuxer::HeaderParserInit() {
  RefPtr<MediaRawData> header = GetFileHeader(FindChunkHeader());
  if (!header) {
    return false;
  }
  BufferReader headerReader(header->Data(), 8);
  Unused << mHeaderParser.Parse(headerReader);
  return true;
}

bool WAVTrackDemuxer::FmtChunkParserInit() {
  RefPtr<MediaRawData> fmtChunk = GetFileHeader(FindFmtChunk());
  if (!fmtChunk || fmtChunk->Size() < 16) {
    return false;
  }
  nsTArray<uint8_t> fmtChunkData(fmtChunk->Data(), fmtChunk->Size());
  mFmtChunk.Init(std::move(fmtChunkData));
  return true;
}

bool WAVTrackDemuxer::ListChunkParserInit(uint32_t aChunkSize) {
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

    MediaByteRange mRange = {mOffset, mOffset + length};
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

    if (!IsUtf8(val)) {
      mHeaderParser.Reset();
      continue;
    }

    switch (id) {
      case 0x49415254:  // IART
        mInfo->mTags.AppendElement(MetadataTag("artist"_ns, val));
        break;
      case 0x49434d54:  // ICMT
        mInfo->mTags.AppendElement(MetadataTag("comments"_ns, val));
        break;
      case 0x49474e52:  // IGNR
        mInfo->mTags.AppendElement(MetadataTag("genre"_ns, val));
        break;
      case 0x494e414d:  // INAM
        mInfo->mTags.AppendElement(MetadataTag("name"_ns, val));
        break;
      default:
        LOG(("Metadata key %08x not handled", id));
    }

    mHeaderParser.Reset();
  }
  return true;
}

media::TimeUnit WAVTrackDemuxer::SeekPosition() const {
  TimeUnit pos = Duration(mChunkIndex);
  if (Duration() > TimeUnit()) {
    pos = std::min(Duration(), pos);
  }
  return pos;
}

RefPtr<MediaRawData> WAVTrackDemuxer::DemuxSample() {
  return GetNextChunk(FindNextChunk());
}

UniquePtr<TrackInfo> WAVTrackDemuxer::GetInfo() const { return mInfo->Clone(); }

RefPtr<WAVTrackDemuxer::SeekPromise> WAVTrackDemuxer::Seek(
    const TimeUnit& aTime) {
  FastSeek(aTime);
  const TimeUnit seekTime = ScanUntil(aTime);
  return SeekPromise::CreateAndResolve(seekTime, __func__);
}

TimeUnit WAVTrackDemuxer::FastSeek(const TimeUnit& aTime) {
  if (aTime.ToMicroseconds()) {
    mChunkIndex = ChunkIndexFromTime(aTime);
  } else {
    mChunkIndex = 0;
  }

  mOffset = OffsetFromChunkIndex(mChunkIndex);

  if (mOffset > mFirstChunkOffset && StreamLength() > 0) {
    mOffset = std::min(static_cast<uint64_t>(StreamLength() - 1), mOffset);
  }

  return Duration(mChunkIndex);
}

TimeUnit WAVTrackDemuxer::ScanUntil(const TimeUnit& aTime) {
  if (!aTime.ToMicroseconds()) {
    return FastSeek(aTime);
  }

  if (Duration(mChunkIndex) > aTime) {
    FastSeek(aTime);
  }

  return SeekPosition();
}

RefPtr<WAVTrackDemuxer::SamplesPromise> WAVTrackDemuxer::GetSamples(
    int32_t aNumSamples) {
  MOZ_ASSERT(aNumSamples);

  RefPtr<SamplesHolder> datachunks = new SamplesHolder();

  while (aNumSamples--) {
    RefPtr<MediaRawData> datachunk = GetNextChunk(FindNextChunk());
    if (!datachunk) {
      break;
    }
    if (!datachunk->HasValidTime()) {
      return SamplesPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_DEMUXER_ERR,
                                             __func__);
    }
    datachunks->AppendSample(datachunk);
  }

  if (datachunks->GetSamples().IsEmpty()) {
    return SamplesPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_END_OF_STREAM,
                                           __func__);
  }

  return SamplesPromise::CreateAndResolve(datachunks, __func__);
}

void WAVTrackDemuxer::Reset() {
  FastSeek(TimeUnit());
  mParser.Reset();
  mHeaderParser.Reset();
  mRIFFParser.Reset();
}

RefPtr<WAVTrackDemuxer::SkipAccessPointPromise>
WAVTrackDemuxer::SkipToNextRandomAccessPoint(const TimeUnit& aTimeThreshold) {
  return SkipAccessPointPromise::CreateAndReject(
      SkipFailureHolder(NS_ERROR_DOM_MEDIA_DEMUXER_ERR, 0), __func__);
}

int64_t WAVTrackDemuxer::GetResourceOffset() const {
  return AssertedCast<int64_t>(mOffset);
}

TimeIntervals WAVTrackDemuxer::GetBuffered() {
  TimeUnit duration = Duration();

  if (duration <= TimeUnit()) {
    return TimeIntervals();
  }

  AutoPinned<MediaResource> stream(mSource.GetResource());
  return GetEstimatedBufferedTimeRanges(stream, duration.ToMicroseconds());
}

int64_t WAVTrackDemuxer::StreamLength() const { return mSource.GetLength(); }

TimeUnit WAVTrackDemuxer::Duration() const {
  if (!mDataLength || !mChannels || !mSampleFormat) {
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

TimeUnit WAVTrackDemuxer::Duration(int64_t aNumDataChunks) const {
  if (!mSamplesPerSecond || !mSamplesPerChunk) {
    return TimeUnit();
  }
  const int64_t frames = mSamplesPerChunk * aNumDataChunks;
  return TimeUnit(frames, mSamplesPerSecond);
}

TimeUnit WAVTrackDemuxer::DurationFromBytes(uint32_t aNumBytes) const {
  if (!mSamplesPerSecond || !mChannels || !mSampleFormat) {
    return TimeUnit();
  }

  uint64_t numSamples = aNumBytes * 8 / mChannels / mSampleFormat;

  return TimeUnit(numSamples, mSamplesPerSecond);
}

MediaByteRange WAVTrackDemuxer::FindNextChunk() {
  if (mOffset + DATA_CHUNK_SIZE < mFirstChunkOffset + mDataLength) {
    return {mOffset, mOffset + DATA_CHUNK_SIZE};
  }
  return {mOffset, mFirstChunkOffset + mDataLength};
}

MediaByteRange WAVTrackDemuxer::FindChunkHeader() {
  return {mOffset, mOffset + CHUNK_HEAD_SIZE};
}

MediaByteRange WAVTrackDemuxer::FindRIFFHeader() {
  return {mOffset, mOffset + RIFF_CHUNK_SIZE};
}

MediaByteRange WAVTrackDemuxer::FindFmtChunk() {
  return {mOffset, mOffset + mHeaderParser.GiveHeader().ChunkSize()};
}

MediaByteRange WAVTrackDemuxer::FindListChunk() {
  return {mOffset, mOffset + mHeaderParser.GiveHeader().ChunkSize()};
}

MediaByteRange WAVTrackDemuxer::FindInfoTag() { return {mOffset, mOffset + 4}; }

bool WAVTrackDemuxer::SkipNextChunk(const MediaByteRange& aRange) {
  UpdateState(aRange);
  return true;
}

already_AddRefed<MediaRawData> WAVTrackDemuxer::GetNextChunk(
    const MediaByteRange& aRange) {
  if (!aRange.Length()) {
    return nullptr;
  }

  RefPtr<MediaRawData> datachunk = new MediaRawData();
  datachunk->mOffset = aRange.mStart;

  UniquePtr<MediaRawDataWriter> chunkWriter(datachunk->CreateWriter());
  if (!chunkWriter->SetSize(static_cast<uint32_t>(aRange.Length()))) {
    return nullptr;
  }

  const uint32_t read = Read(chunkWriter->Data(), datachunk->mOffset,
                             AssertedCast<int64_t>(datachunk->Size()));

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
    uint32_t mBytesRemaining = mDataLength - mChunkIndex * DATA_CHUNK_SIZE;
    datachunk->mDuration = DurationFromBytes(mBytesRemaining);
  }
  datachunk->mTimecode = datachunk->mTime;
  datachunk->mKeyframe = true;

  MOZ_ASSERT(!datachunk->mTime.IsNegative());
  MOZ_ASSERT(!datachunk->mDuration.IsNegative());

  return datachunk.forget();
}

already_AddRefed<MediaRawData> WAVTrackDemuxer::GetFileHeader(
    const MediaByteRange& aRange) {
  if (!aRange.Length()) {
    return nullptr;
  }

  RefPtr<MediaRawData> fileHeader = new MediaRawData();
  fileHeader->mOffset = aRange.mStart;

  UniquePtr<MediaRawDataWriter> headerWriter(fileHeader->CreateWriter());
  if (!headerWriter->SetSize(static_cast<uint32_t>(aRange.Length()))) {
    return nullptr;
  }

  const uint32_t read = Read(headerWriter->Data(), fileHeader->mOffset,
                             AssertedCast<int64_t>(fileHeader->Size()));

  if (read != aRange.Length()) {
    return nullptr;
  }

  UpdateState(aRange);

  return fileHeader.forget();
}

uint64_t WAVTrackDemuxer::OffsetFromChunkIndex(uint32_t aChunkIndex) const {
  return mFirstChunkOffset + aChunkIndex * DATA_CHUNK_SIZE;
}

uint64_t WAVTrackDemuxer::ChunkIndexFromTime(
    const media::TimeUnit& aTime) const {
  if (!mSamplesPerChunk || !mSamplesPerSecond) {
    return 0;
  }
  double chunkDurationS =
      mSamplesPerChunk / static_cast<double>(mSamplesPerSecond);
  int64_t chunkIndex = std::floor(aTime.ToSeconds() / chunkDurationS);
  return chunkIndex;
}

void WAVTrackDemuxer::UpdateState(const MediaByteRange& aRange) {
  // Full chunk parsed, move offset to its end.
  mOffset = static_cast<uint32_t>(aRange.mEnd);
  mTotalChunkLen += static_cast<uint64_t>(aRange.Length());
}

int64_t WAVTrackDemuxer::Read(uint8_t* aBuffer, int64_t aOffset,
                              int64_t aSize) {
  const int64_t streamLen = StreamLength();
  if (mInfo && streamLen > 0) {
    int64_t max = streamLen > aOffset ? streamLen - aOffset : 0;
    aSize = std::min(aSize, max);
  }
  uint32_t read = 0;
  const nsresult rv = mSource.ReadAt(aOffset, reinterpret_cast<char*>(aBuffer),
                                     static_cast<uint32_t>(aSize), &read);
  NS_ENSURE_SUCCESS(rv, 0);
  return read;
}

// RIFFParser

Result<uint32_t, nsresult> RIFFParser::Parse(BufferReader& aReader) {
  for (auto res = aReader.ReadU8();
       res.isOk() && !mRiffHeader.ParseNext(res.unwrap());
       res = aReader.ReadU8()) {
  }

  if (mRiffHeader.IsValid()) {
    return RIFF_CHUNK_SIZE;
  }

  return 0;
}

void RIFFParser::Reset() { mRiffHeader.Reset(); }

const RIFFParser::RIFFHeader& RIFFParser::RiffHeader() const {
  return mRiffHeader;
}

// RIFFParser::RIFFHeader

RIFFParser::RIFFHeader::RIFFHeader() { Reset(); }

void RIFFParser::RIFFHeader::Reset() {
  memset(mRaw, 0, sizeof(mRaw));
  mPos = 0;
}

bool RIFFParser::RIFFHeader::ParseNext(uint8_t c) {
  if (!Update(c)) {
    Reset();
    if (!Update(c)) {
      Reset();
    }
  }
  return IsValid();
}

bool RIFFParser::RIFFHeader::IsValid(int aPos) const {
  if (aPos > -1 && aPos < 4) {
    return RIFF[aPos] == mRaw[aPos];
  }
  if (aPos > 7 && aPos < 12) {
    return WAVE[aPos - 8] == mRaw[aPos];
  }
  return true;
}

bool RIFFParser::RIFFHeader::IsValid() const { return mPos >= RIFF_CHUNK_SIZE; }

bool RIFFParser::RIFFHeader::Update(uint8_t c) {
  if (mPos < RIFF_CHUNK_SIZE) {
    mRaw[mPos] = c;
  }
  return IsValid(mPos++);
}

// HeaderParser

Result<uint32_t, nsresult> HeaderParser::Parse(BufferReader& aReader) {
  for (auto res = aReader.ReadU8();
       res.isOk() && !mHeader.ParseNext(res.unwrap()); res = aReader.ReadU8()) {
  }

  if (mHeader.IsValid()) {
    return CHUNK_HEAD_SIZE;
  }

  return 0;
}

void HeaderParser::Reset() { mHeader.Reset(); }

const HeaderParser::ChunkHeader& HeaderParser::GiveHeader() const {
  return mHeader;
}

// HeaderParser::ChunkHeader

HeaderParser::ChunkHeader::ChunkHeader() { Reset(); }

void HeaderParser::ChunkHeader::Reset() {
  memset(mRaw, 0, sizeof(mRaw));
  mPos = 0;
}

bool HeaderParser::ChunkHeader::ParseNext(uint8_t c) {
  Update(c);
  return IsValid();
}

bool HeaderParser::ChunkHeader::IsValid() const {
  return mPos >= CHUNK_HEAD_SIZE;
}

uint32_t HeaderParser::ChunkHeader::ChunkName() const {
  return static_cast<uint32_t>(
      ((mRaw[0] << 24) | (mRaw[1] << 16) | (mRaw[2] << 8) | (mRaw[3])));
}

uint32_t HeaderParser::ChunkHeader::ChunkSize() const {
  return static_cast<uint32_t>(
      ((mRaw[7] << 24) | (mRaw[6] << 16) | (mRaw[5] << 8) | (mRaw[4])));
}

void HeaderParser::ChunkHeader::Update(uint8_t c) {
  if (mPos < CHUNK_HEAD_SIZE) {
    mRaw[mPos++] = c;
  }
}

// FormatChunk

void FormatChunk::Init(nsTArray<uint8_t>&& aData) { mRaw = std::move(aData); }

uint16_t FormatChunk::WaveFormat() const { return (mRaw[1] << 8) | (mRaw[0]); }

uint16_t FormatChunk::Channels() const { return (mRaw[3] << 8) | (mRaw[2]); }

uint32_t FormatChunk::SampleRate() const {
  return static_cast<uint32_t>((mRaw[7] << 24) | (mRaw[6] << 16) |
                               (mRaw[5] << 8) | (mRaw[4]));
}

uint16_t FormatChunk::AverageBytesPerSec() const {
  return static_cast<uint16_t>((mRaw[11] << 24) | (mRaw[10] << 16) |
                               (mRaw[9] << 8) | (mRaw[8]));
}

uint16_t FormatChunk::BlockAlign() const {
  return static_cast<uint16_t>(mRaw[13] << 8) | (mRaw[12]);
}

uint16_t FormatChunk::ValidBitsPerSamples() const {
  return (mRaw[15] << 8) | (mRaw[14]);
}

uint16_t FormatChunk::ExtraFormatInfoSize() const {
  uint16_t value = static_cast<uint16_t>(mRaw[17] << 8) | (mRaw[16]);
  if (WaveFormat() != 0xFFFE && value != 0) {
    NS_WARNING(
        "Found non-zero extra format info length and the wave format"
        " isn't WAVEFORMATEXTENSIBLE.");
    return 0;
  }
  if (WaveFormat() == 0xFFFE && value < 22) {
    NS_WARNING(
        "Wave format is WAVEFORMATEXTENSIBLE and extra data size isn't at"
        " least 22 bytes");
    return 0;
  }
  return value;
}

AudioConfig::ChannelLayout::ChannelMap FormatChunk::ChannelMap() const {
  // Regular mapping if file doesn't have channel mapping info. Alternatively,
  // if the chunk size doesn't have the field for the size of the extension
  // data, return a regular mapping.
  constexpr size_t SIZE_WAVEFORMATEX = 18;
  constexpr size_t MIN_SIZE_WAVEFORMATEXTENSIBLE = 22;
  constexpr size_t OFFSET_CHANNEL_MAP = 20;
  if (WaveFormat() != 0xFFFE || mRaw.Length() <= SIZE_WAVEFORMATEX) {
    return AudioConfig::ChannelLayout(Channels()).Map();
  }
  // The length of this chunk is at least 18, check if it's long enough to
  // hold the WAVE_FORMAT_EXTENSIBLE struct, that is 22 more than the
  // WAVEFORMATEX. If not, fall back to a common mapping. The channel
  // mapping is four bytes, starting at offset 20.
  if (ExtraFormatInfoSize() < MIN_SIZE_WAVEFORMATEXTENSIBLE ||
      mRaw.Length() < SIZE_WAVEFORMATEX + MIN_SIZE_WAVEFORMATEXTENSIBLE) {
    return AudioConfig::ChannelLayout(Channels()).Map();
  }
  // ChannelLayout::ChannelMap is by design bit-per-bit compatible with
  // WAVEFORMATEXTENSIBLE's dwChannelMask attribute.
  BufferReader reader(mRaw.Elements() + OFFSET_CHANNEL_MAP, sizeof(uint32_t));
  auto channelMap = reader.ReadLEU32();
  return channelMap.unwrapOr(AudioConfig::ChannelLayout::UNKNOWN_MAP);
}

// DataParser

DataParser::DataParser() = default;

void DataParser::Reset() { mChunk.Reset(); }

const DataParser::DataChunk& DataParser::CurrentChunk() const { return mChunk; }

// DataParser::DataChunk

void DataParser::DataChunk::Reset() { mPos = 0; }

}  // namespace mozilla
