/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP3_FRAME_PARSER_H_
#define MP3_FRAME_PARSER_H_

#include <vector>

#include "mozilla/Maybe.h"
#include "mp4_demuxer/ByteReader.h"

namespace mozilla {

// ID3 header parser state machine used by FrameParser.
// The header contains the following format (one byte per term):
// 'I' 'D' '3' MajorVersion MinorVersion Flags Size1 Size2 Size3 Size4
// For more details see http://id3.org/id3v2.3.0.
class ID3Parser
{
public:
  // Holds the ID3 header and its parsing state.
  class ID3Header
  {
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

    // The total size of the ID3 tag including header/footer, or zero if
    // none has been found.
    uint32_t TotalTagSize() const;

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

  // Parses contents of given ByteReader for a valid ID3v2 header.
  // Returns the total ID3v2 tag size if successful and zero otherwise.
  uint32_t Parse(mp4_demuxer::ByteReader* aReader);

  // Resets the state to allow for a new parsing session.
  void Reset();

private:
  // The currently parsed ID3 header. Reset via Reset, updated via Parse.
  ID3Header mHeader;
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
class FrameParser
{
public:
  // Holds the frame header and its parsing state.
  class FrameHeader
  {
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
  class VBRHeader
  {
  public:
    // Synchronize with vbr_header TYPE_STR on change.
    enum VBRHeaderType
    {
      NONE = 0,
      XING,
      VBRI
    };

    // Constructor.
    VBRHeader();

    // Returns the parsed VBR header type, or NONE if no valid header found.
    VBRHeaderType Type() const;

    // Returns the total number of audio frames (excluding the VBR header frame)
    // expected in the stream/file.
    const Maybe<uint32_t>& NumAudioFrames() const;

    // Returns the expected size of the stream.
    const Maybe<uint32_t>& NumBytes() const;

    // Returns the VBR scale factor (0: best quality, 100: lowest quality).
    const Maybe<uint32_t>& Scale() const;

    // Returns true iff Xing/Info TOC (table of contents) is present.
    bool IsTOCPresent() const;

    // Returns whether the header is valid (type XING or VBRI).
    bool IsValid() const;

    // Returns whether the header is valid and contains reasonable non-zero
    // field values.
    bool IsComplete() const;

    // Returns the byte offset for the given duration percentage as a factor
    // (0: begin, 1.0: end).
    int64_t Offset(float aDurationFac) const;

    // Parses contents of given ByteReader for a valid VBR header.
    // The offset of the passed ByteReader needs to point to an MPEG frame
    // begin, as a VBRI-style header is searched at a fixed offset relative to
    // frame begin. Returns whether a valid VBR header was found in the range.
    bool Parse(mp4_demuxer::ByteReader* aReader);

  private:
    // Parses contents of given ByteReader for a valid Xing header.
    // The initial ByteReader offset will be preserved.
    // Returns whether a valid Xing header was found in the range.
    bool ParseXing(mp4_demuxer::ByteReader* aReader);

    // Parses contents of given ByteReader for a valid VBRI header.
    // The initial ByteReader offset will be preserved. It also needs to point
    // to the beginning of a valid MPEG frame, as VBRI headers are searched
    // at a fixed offset relative to frame begin.
    // Returns whether a valid VBRI header was found in the range.
    bool ParseVBRI(mp4_demuxer::ByteReader* aReader);

    // The total number of frames expected as parsed from a VBR header.
    Maybe<uint32_t> mNumAudioFrames;

    // The total number of bytes expected in the stream.
    Maybe<uint32_t> mNumBytes;

    // The VBR scale factor.
    Maybe<uint32_t> mScale;

    // The TOC table mapping duration percentage to byte offset.
    std::vector<int64_t> mTOC;

    // The detected VBR header type.
    VBRHeaderType mType;
  };

  // Frame meta container used to parse and hold a frame header and side info.
  class Frame
  {
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

  // Returns the previously parsed frame. Reset via Reset.
  const Frame& PrevFrame() const;

  // Returns the first parsed frame. Reset via Reset.
  const Frame& FirstFrame() const;

  // Returns the parsed ID3 header. Note: check for validity.
  const ID3Parser::ID3Header& ID3Header() const;

  // Returns the parsed VBR header info. Note: check for validity by type.
  const VBRHeader& VBRInfo() const;

  // Resets the parser.
  void Reset();

  // Resets all frame data, but not the ID3Header.
  // Don't use between frames as first frame data is reset.
  void ResetFrameData();

  // Clear the last parsed frame to allow for next frame parsing, i.e.:
  // - sets PrevFrame to CurrentFrame
  // - resets the CurrentFrame
  // - resets ID3Header if no valid header was parsed yet
  void EndFrameSession();

  // Parses contents of given ByteReader for a valid frame header and returns
  // true if one was found. After returning, the variable passed to
  // 'aBytesToSkip' holds the amount of bytes to be skipped (if any) in order to
  // jump across a large ID3v2 tag spanning multiple buffers.
  bool Parse(mp4_demuxer::ByteReader* aReader, uint32_t* aBytesToSkip);

  // Parses contents of given ByteReader for a valid VBR header.
  // The offset of the passed ByteReader needs to point to an MPEG frame begin,
  // as a VBRI-style header is searched at a fixed offset relative to frame
  // begin. Returns whether a valid VBR header was found.
  bool ParseVBRHeader(mp4_demuxer::ByteReader* aReader);

private:
  // ID3 header parser.
  ID3Parser mID3Parser;

  // VBR header parser.
  VBRHeader mVBRHeader;

  // We keep the first parsed frame around for static info access, the
  // previously parsed frame for debugging and the currently parsed frame.
  Frame mFirstFrame;
  Frame mFrame;
  Frame mPrevFrame;
};

} // namespace mozilla

#endif
