/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP3_DEMUXER_H_
#define MP3_DEMUXER_H_

#include "mozilla/Attributes.h"
#include "MediaDataDemuxer.h"
#include "MediaResource.h"

namespace mozilla {
namespace mp3 {

class MP3TrackDemuxer;

class MP3Demuxer : public MediaDataDemuxer {
public:
  // MediaDataDemuxer interface.
  explicit MP3Demuxer(MediaResource* aSource);
  nsRefPtr<InitPromise> Init() override;
  already_AddRefed<MediaDataDemuxer> Clone() const override;
  bool HasTrackType(TrackInfo::TrackType aType) const override;
  uint32_t GetNumberTracks(TrackInfo::TrackType aType) const override;
  already_AddRefed<MediaTrackDemuxer> GetTrackDemuxer(
      TrackInfo::TrackType aType, uint32_t aTrackNumber) override;
  bool IsSeekable() const override;
  void NotifyDataArrived(uint32_t aLength, int64_t aOffset) override;
  void NotifyDataRemoved() override;

private:
  // Synchronous initialization.
  bool InitInternal();

  nsRefPtr<MediaResource> mSource;
  nsRefPtr<MP3TrackDemuxer> mTrackDemuxer;
};

// ID3 header parser state machine used by FrameParser.
// The header contains the following format (one byte per term):
// 'I' 'D' '3' MajorVersion MinorVersion Flags Size1 Size2 Size3 Size4
// For more details see http://id3.org/id3v2.3.0.
class ID3Parser {
public:
  // Holds the ID3 header and its parsing state.
  class ID3Header {
  public:
    // The header size is static, see class comment.
    static const int SIZE = 10;

    // Constructor.
    ID3Header();

    // Resets the state to allow for a new parsing session.
    void Reset();

    // The ID3 tags are versioned like this: ID3vMajorVersion.MinorVersion.
    uint8_t MajorVersion() const;
    uint8_t MinorVersion() const;

    // The ID3 flags field.
    uint8_t Flags() const;

    // The derived size based on the provided size fields.
    uint32_t Size() const;

    // Returns the size of an ID3v2.4 footer if present and zero otherwise.
    uint8_t FooterSize() const;

    // Returns whether the parsed data is a valid ID3 header up to the given
    // byte position.
    bool IsValid(int aPos) const;

    // Returns whether the parsed data is a complete and valid ID3 header.
    bool IsValid() const;

    // Parses the next provided byte.
    // Returns whether the byte creates a valid sequence up to this point.
    bool ParseNext(uint8_t c);

  private:
    // Updates the parser state machine with the provided next byte.
    // Returns whether the provided byte is a valid next byte in the sequence.
    bool Update(uint8_t c);

    // The currently parsed byte sequence.
    uint8_t mRaw[SIZE];

    // The derived size as provided by the size fields.
    // The header size fields holds a 4 byte sequence with each MSB set to 0,
    // this bits need to be ignored when deriving the actual size.
    uint32_t mSize;

    // The current byte position in the parsed sequence. Reset via Reset and
    // incremented via Update.
    int mPos;
  };

  // Returns the parsed ID3 header. Note: check for validity.
  const ID3Header& Header() const;

  // Parses the given buffer range [aBeg, aEnd) for a valid ID3 header.
  // Returns the header begin position or aEnd if no valid header was found.
  const uint8_t* Parse(const uint8_t* aBeg, const uint8_t* aEnd);

  // Resets the state to allow for a new parsing session.
  void Reset();

private:
  // The currently parsed ID3 header. Reset via Reset, updated via Parse.
  ID3Header mHeader;
};

struct FrameParserResult {
  const uint8_t* mBufferPos;
  const uint32_t mBytesToSkip;
};

// MPEG audio frame parser.
// The MPEG frame header has the following format (one bit per character):
// 11111111 111VVLLC BBBBSSPR MMEETOHH
// {   sync   } - 11 sync bits
//   VV         - MPEG audio version ID (0->2.5, 1->reserved, 2->2, 3->1)
//   LL         - Layer description (0->reserved, 1->III, 2->II, 3->I)
//   C          - CRC protection bit (0->protected, 1->not protected)
//   BBBB       - Bitrate index (see table in implementation)
//   SS         - Sampling rate index (see table in implementation)
//   P          - Padding bit (0->not padded, 1->padded by 1 slot size)
//   R          - Private bit (ignored)
//   MM         - Channel mode (0->stereo, 1->joint stereo, 2->dual channel,
//                3->single channel)
//   EE         - Mode extension for joint stereo (ignored)
//   T          - Copyright (0->disabled, 1->enabled)
//   O          - Original (0->copy, 1->original)
//   HH         - Emphasis (0->none, 1->50/15 ms, 2->reserved, 3->CCIT J.17)
class FrameParser {
public:
  // Holds the frame header and its parsing state.
  class FrameHeader {
  public:
    // The header size is static, see class comments.
    static const int SIZE = 4;

    // Constructor.
    FrameHeader();

    // Raw field access, see class comments for details.
    uint8_t Sync1() const;
    uint8_t Sync2() const;
    uint8_t RawVersion() const;
    uint8_t RawLayer() const;
    uint8_t RawProtection() const;
    uint8_t RawBitrate() const;
    uint8_t RawSampleRate() const;
    uint8_t Padding() const;
    uint8_t Private() const;
    uint8_t RawChannelMode() const;

    // Sampling rate frequency in Hz.
    int32_t SampleRate() const;

    // Number of audio channels.
    int32_t Channels() const;

    // Samples per frames, static depending on MPEG version and layer.
    int32_t SamplesPerFrame() const;

    // Slot size used for padding, static depending on MPEG layer.
    int32_t SlotSize() const;

    // Bitrate in kbps, can vary between frames.
    int32_t Bitrate() const;

    // MPEG layer (0->invalid, 1->I, 2->II, 3->III).
    int32_t Layer() const;

    // Returns whether the parsed data is a valid frame header up to the given
    // byte position.
    bool IsValid(const int aPos) const;

    // Returns whether the parsed data is a complete and valid frame header.
    bool IsValid() const;

    // Resets the state to allow for a new parsing session.
    void Reset();

    // Parses the next provided byte.
    // Returns whether the byte creates a valid sequence up to this point.
    bool ParseNext(const uint8_t c);

  private:
    // Updates the parser state machine with the provided next byte.
    // Returns whether the provided byte is a valid next byte in the sequence.
    bool Update(const uint8_t c);

    // The currently parsed byte sequence.
    uint8_t mRaw[SIZE];

    // The current byte position in the parsed sequence. Reset via Reset and
    // incremented via Update.
    int mPos;
  };

  // VBR frames may contain Xing or VBRI headers for additional info, we use
  // this class to parse them and access this info.
  class VBRHeader {
  public:
    enum VBRHeaderType {
      NONE,
      XING,
      VBRI
    };

    // Constructor.
    VBRHeader();

    // Returns the parsed VBR header type, or NONE if no valid header found.
    VBRHeaderType Type() const;

    // Returns the total number of frames expected in the stream/file.
    int64_t NumFrames() const;

    // Parses given buffer [aBeg, aEnd) for a valid VBR header.
    // Returns whether a valid VBR header was found in the range.
    bool Parse(const uint8_t* aBeg, const uint8_t* aEnd);

  private:
    // Parses given buffer [aBeg, aEnd) for a valid Xing header.
    // Returns whether a valid Xing header was found in the range.
    bool ParseXing(const uint8_t* aBeg, const uint8_t* aEnd);

    // Parses given buffer [aBeg, aEnd) for a valid VBRI header.
    // Returns whether a valid VBRI header was found in the range.
    bool ParseVBRI(const uint8_t* aBeg, const uint8_t* aEnd);

    // The total number of frames expected as parsed from a VBR header.
    int64_t mNumFrames;

    // The detected VBR header type.
    VBRHeaderType mType;
  };

  // Frame meta container used to parse and hold a frame header and side info.
  class Frame {
  public:
    // Returns the length of the frame excluding the header in bytes.
    int32_t Length() const;

    // Returns the parsed frame header.
    const FrameHeader& Header() const;

    // Resets the frame header and data.
    void Reset();

    // Parses the next provided byte.
    // Returns whether the byte creates a valid sequence up to this point.
    bool ParseNext(uint8_t c);

  private:
    // The currently parsed frame header.
    FrameHeader mHeader;
  };

  // Constructor.
  FrameParser();

  // Returns the currently parsed frame. Reset via Reset or EndFrameSession.
  const Frame& CurrentFrame() const;

#ifdef ENABLE_TESTS
  // Returns the previously parsed frame. Reset via Reset.
  const Frame& PrevFrame() const;
#endif

  // Returns the first parsed frame. Reset via Reset.
  const Frame& FirstFrame() const;

  // Returns the parsed ID3 header. Note: check for validity.
  const ID3Parser::ID3Header& ID3Header() const;

  // Returns the parsed VBR header info. Note: check for validity by type.
  const VBRHeader& VBRInfo() const;

  // Resets the parser. Don't use between frames as first frame data is reset.
  void Reset();

  // Clear the last parsed frame to allow for next frame parsing, i.e.:
  // - sets PrevFrame to CurrentFrame
  // - resets the CurrentFrame
  // - resets ID3Header if no valid header was parsed yet
  void EndFrameSession();

  // Parses given buffer [aBeg, aEnd) for a valid frame header and returns a FrameParserResult.
  // FrameParserResult.mBufferPos points to begin of frame header if a frame header was found
  // or to aEnd otherwise. FrameParserResult.mBytesToSkip indicates whether additional bytes need to
  // be skipped in order to jump across an ID3 tag that stretches beyond the given buffer.
  FrameParserResult Parse(const uint8_t* aBeg, const uint8_t* aEnd);

  // Parses given buffer [aBeg, aEnd) for a valid VBR header.
  // Returns whether a valid VBR header was found.
  bool ParseVBRHeader(const uint8_t* aBeg, const uint8_t* aEnd);

private:
  // ID3 header parser.
  ID3Parser mID3Parser;

  // VBR header parser.
  VBRHeader mVBRHeader;

  // We keep the first parsed frame around for static info access, the
  // previously parsed frame for debugging and the currently parsed frame.
  Frame mFirstFrame;
  Frame mFrame;
#ifdef ENABLE_TESTS
  Frame mPrevFrame;
#endif
};

// The MP3 demuxer used to extract MPEG frames and side information out of
// MPEG streams.
class MP3TrackDemuxer : public MediaTrackDemuxer {
public:
  // Constructor, expecing a valid media resource.
  explicit MP3TrackDemuxer(MediaResource* aSource);

  // Initializes the track demuxer by reading the first frame for meta data.
  // Returns initialization success state.
  bool Init();

  // Returns the total stream length if known, -1 otherwise.
  int64_t StreamLength() const;

  // Returns the estimated stream duration, or a 0-duration if unknown.
  media::TimeUnit Duration() const;

  // Returns the estimated duration up to the given frame number,
  // or a 0-duration if unknown.
  media::TimeUnit Duration(int64_t aNumFrames) const;

#ifdef ENABLE_TESTS
  const FrameParser::Frame& LastFrame() const;
  nsRefPtr<MediaRawData> DemuxSample();
  media::TimeUnit SeekPosition() const;
#endif

  const ID3Parser::ID3Header& ID3Header() const;
  const FrameParser::VBRHeader& VBRInfo() const;

  // MediaTrackDemuxer interface.
  UniquePtr<TrackInfo> GetInfo() const override;
  nsRefPtr<SeekPromise> Seek(media::TimeUnit aTime) override;
  nsRefPtr<SamplesPromise> GetSamples(int32_t aNumSamples = 1) override;
  void Reset() override;
  nsRefPtr<SkipAccessPointPromise> SkipToNextRandomAccessPoint(
    media::TimeUnit aTimeThreshold) override;
  int64_t GetResourceOffset() const override;
  media::TimeIntervals GetBuffered() override;

private:
  // Destructor.
  ~MP3TrackDemuxer() {}

  // Fast approximate seeking to given time.
  media::TimeUnit FastSeek(media::TimeUnit aTime);

  // Seeks by scanning the stream up to the given time for more accurate results.
  media::TimeUnit ScanUntil(media::TimeUnit aTime);

  // Finds the next valid frame and returns its byte range.
  MediaByteRange FindNextFrame();

  // Skips the next frame given the provided byte range.
  bool SkipNextFrame(const MediaByteRange& aRange);

  // Returns the next MPEG frame, if available.
  already_AddRefed<MediaRawData> GetNextFrame(const MediaByteRange& aRange);

  // Updates post-read meta data.
  void UpdateState(const MediaByteRange& aRange);

  // Reads aSize bytes into aBuffer from the source starting at aOffset.
  // Returns the actual size read.
  int32_t Read(uint8_t* aBuffer, int64_t aOffset, int32_t aSize);

  // Returns the average frame length derived from the previously parsed frames.
  double AverageFrameLength() const;

  // The (hopefully) MPEG resource.
  MediaResourceIndex mSource;

  // MPEG frame parser used to detect frames and extract side info.
  FrameParser mParser;

  // Current byte offset in the source stream.
  int64_t mOffset;

  // Byte offset of the begin of the first frame, or 0 if none parsed yet.
  int64_t mFirstFrameOffset;

  // Total parsed frames.
  uint64_t mNumParsedFrames;

  // Current frame index.
  int64_t mFrameIndex;

  // Sum of parsed frames' lengths in bytes.
  uint64_t mTotalFrameLen;

  // Samples per frame metric derived from frame headers or 0 if none available.
  int32_t mSamplesPerFrame;

  // Samples per second metric derived from frame headers or 0 if none available.
  int32_t mSamplesPerSecond;

  // Channel count derived from frame headers or 0 if none available.
  int32_t mChannels;

  // Audio track config info.
  UniquePtr<AudioInfo> mInfo;
};

} // namespace mp3
} // namespace mozilla

#endif
