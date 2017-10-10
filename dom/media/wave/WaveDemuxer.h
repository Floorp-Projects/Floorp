/* This Source Code Form is subject to the terms of the Mozilla Public
 * Licence, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WAV_DEMUXER_H_
#define WAV_DEMUXER_H_

#include "MediaDataDemuxer.h"
#include "MediaResource.h"

namespace mozilla {
class BufferReader;

static const uint32_t FRMT_CODE = 0x666d7420;
static const uint32_t DATA_CODE = 0x64617461;
static const uint32_t LIST_CODE = 0x4c495354;
static const uint32_t INFO_CODE = 0x494e464f;

static const uint8_t RIFF[4] = {'R', 'I', 'F', 'F'};
static const uint8_t WAVE[4] = {'W', 'A', 'V', 'E'};

static const uint16_t RIFF_CHUNK_SIZE = 12;
static const uint16_t CHUNK_HEAD_SIZE = 8;
static const uint16_t FMT_CHUNK_MIN_SIZE = 16;
static const uint16_t DATA_CHUNK_SIZE = 768;

class WAVTrackDemuxer;

DDLoggedTypeDeclNameAndBase(WAVDemuxer, MediaDataDemuxer);
DDLoggedTypeNameAndBase(WAVTrackDemuxer, MediaTrackDemuxer);

class WAVDemuxer
  : public MediaDataDemuxer
  , public DecoderDoctorLifeLogger<WAVDemuxer>
{
public:
  // MediaDataDemuxer interface.
  explicit WAVDemuxer(MediaResource* aSource);
  RefPtr<InitPromise> Init() override;
  uint32_t GetNumberTracks(TrackInfo::TrackType aType) const override;
  already_AddRefed<MediaTrackDemuxer> GetTrackDemuxer(
    TrackInfo::TrackType aType, uint32_t aTrackNumber) override;
  bool IsSeekable() const override;

private:
  // Synchronous Initialization.
  bool InitInternal();

  MediaResourceIndex mSource;
  RefPtr<WAVTrackDemuxer> mTrackDemuxer;
};

class RIFFParser
{
private:
  class RIFFHeader;
public:
  const RIFFHeader& RiffHeader() const;

  Result<uint32_t, nsresult> Parse(BufferReader& aReader);

  void Reset();

private:
  class RIFFHeader
  {
  public:
    RIFFHeader();
    void Reset();

    bool IsValid() const;
    bool IsValid(int aPos) const;

    bool ParseNext(uint8_t c);

  private:
    bool Update(uint8_t c);

    uint8_t mRaw[RIFF_CHUNK_SIZE];

    int mPos;
  };

  RIFFHeader mRiffHeader;
};

class HeaderParser
{
private:
  class ChunkHeader;
public:
  const ChunkHeader& GiveHeader() const;

  Result<uint32_t, nsresult> Parse(BufferReader& aReader);

  void Reset();

private:
  class ChunkHeader
  {
  public:
    ChunkHeader();
    void Reset();

    bool IsValid() const;

    uint32_t ChunkName() const;
    uint32_t ChunkSize() const;

    bool ParseNext(uint8_t c);

  private:
    void Update(uint8_t c);

    uint8_t mRaw[CHUNK_HEAD_SIZE];

    int mPos;
  };

  ChunkHeader mHeader;
};

class FormatParser
{
private:
  class FormatChunk;
public:
  const FormatChunk& FmtChunk() const;

  Result<uint32_t, nsresult> Parse(BufferReader& aReader);

  void Reset();

private:
  class FormatChunk
  {
  public:
    FormatChunk();
    void Reset();

    uint16_t WaveFormat() const;
    uint16_t Channels() const;
    uint32_t SampleRate() const;
    uint16_t FrameSize() const;
    uint16_t SampleFormat() const;

    bool IsValid() const;
    bool ParseNext(uint8_t c);

  private:
    void Update(uint8_t c);

    uint8_t mRaw[FMT_CHUNK_MIN_SIZE];

    int mPos;
  };

  FormatChunk mFmtChunk;
};

class DataParser
{
private:
  class DataChunk;
public:
  DataParser();

  const DataChunk& CurrentChunk() const;

  void Reset();

private:
  class DataChunk
  {
  public:
    void Reset();
  private:
    int mPos; // To Check Alignment
  };

  DataChunk mChunk;
};

class WAVTrackDemuxer
  : public MediaTrackDemuxer
  , public DecoderDoctorLifeLogger<WAVTrackDemuxer>
{
public:
  explicit WAVTrackDemuxer(MediaResource* aSource);

  bool Init();

  int64_t StreamLength() const;

  media::TimeUnit Duration() const;
  media::TimeUnit Duration(int64_t aNumDataChunks) const;
  media::TimeUnit DurationFromBytes(uint32_t aNumBytes) const;

  media::TimeUnit SeekPosition() const;

  RefPtr<MediaRawData> DemuxSample();

  // MediaTrackDemuxer interface.
  UniquePtr<TrackInfo> GetInfo() const override;
  RefPtr<SeekPromise> Seek(const media::TimeUnit& aTime) override;
  RefPtr<SamplesPromise> GetSamples(int32_t aNumSamples) override;
  void Reset() override;
  RefPtr<SkipAccessPointPromise> SkipToNextRandomAccessPoint(
    const media::TimeUnit& aTimeThreshold) override;
  int64_t GetResourceOffset() const override;
  media::TimeIntervals GetBuffered() override;

private:
  ~WAVTrackDemuxer() { }

  media::TimeUnit FastSeek(const media::TimeUnit& aTime);
  media::TimeUnit ScanUntil(const media::TimeUnit& aTime);

  MediaByteRange FindNextChunk();

  MediaByteRange FindChunkHeader();
  MediaByteRange FindRIFFHeader();
  MediaByteRange FindFmtChunk();
  MediaByteRange FindListChunk();
  MediaByteRange FindInfoTag();

  bool RIFFParserInit();
  bool HeaderParserInit();
  bool FmtChunkParserInit();
  bool ListChunkParserInit(uint32_t aChunkSize);

  bool SkipNextChunk(const MediaByteRange& aRange);

  already_AddRefed<MediaRawData> GetNextChunk(const MediaByteRange& aRange);
  already_AddRefed<MediaRawData> GetFileHeader(const MediaByteRange& aRange);

  void UpdateState(const MediaByteRange& aRange);

  int64_t OffsetFromChunkIndex(int64_t aChunkIndex) const;
  int64_t ChunkIndexFromOffset(int64_t aOffet) const;
  int64_t ChunkIndexFromTime(const media::TimeUnit& aTime) const;

  int32_t Read(uint8_t* aBuffer, int64_t aOffset, int32_t aSize);

  MediaResourceIndex mSource;

  DataParser mParser;
  RIFFParser mRIFFParser;
  HeaderParser mHeaderParser;

  FormatParser mFmtParser;
  // ListChunkParser mListChunkParser;

  int64_t mOffset;
  int64_t mFirstChunkOffset;

  uint32_t mNumParsedChunks;
  int32_t mChunkIndex;

  uint32_t mDataLength;
  uint64_t mTotalChunkLen;

  int32_t mSamplesPerChunk;
  int32_t mSamplesPerSecond;

  int32_t mChannels;
  int32_t mSampleFormat;

  UniquePtr<AudioInfo> mInfo;
};

} // namespace mozilla

#endif
