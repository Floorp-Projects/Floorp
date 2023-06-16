/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP3_FRAME_PARSER_H_
#define MP3_FRAME_PARSER_H_

#include <vector>

#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "BufferReader.h"

namespace mozilla {

// ID3 header parser state machine used by FrameParser.
// The header contains the following format (one byte per term):
// 'I' 'D' '3' MajorVersion MinorVersion Flags Size1 Size2 Size3 Size4
// For more details see https://id3.org/id3v2.4.0-structure
class ID3Parser {
 public:
  // Holds the ID3 header and its parsing state.
  class ID3Header {
   public:
    // The header size is static, see class comment.
    static const int SIZE = 10;
    static const int ID3v1_SIZE = 128;

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

    // To see whether we have parsed the value of the size from header.
    bool HasSizeBeenSet() const;

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
    uint8_t mRaw[SIZE] = {};

    // The derived size as provided by the size fields.
    // The header size fields holds a 4 byte sequence with each MSB set to 0,
    // this bits need to be ignored when deriving the actual size.
    Maybe<uint32_t> mSize;

    // The current byte position in the parsed sequence. Reset via Reset and
    // incremented via Update.
    int mPos = 0;
  };

  // Check if the buffer is starting with ID3v2 tag.
  static bool IsBufferStartingWithID3Tag(BufferReader* aReader);
  // Similarly, if the buffer is starting with ID3v1 tag.
  static bool IsBufferStartingWithID3v1Tag(BufferReader* aReader);

  // Returns the parsed ID3 header. Note: check for validity.
  const ID3Header& Header() const;

  // Returns the size of all parsed ID3 headers.
  uint32_t TotalHeadersSize() const;

  // Parses contents of given BufferReader for a valid ID3v2 header.
  // Returns the parsed ID3v2 tag size if successful and zero otherwise.
  uint32_t Parse(BufferReader* aReader);

  // Resets the state to allow for a new parsing session.
  void Reset();

 private:
  uint32_t ParseInternal(BufferReader* aReader);

  // The currently parsed ID3 header. Reset via Reset, updated via Parse.
  ID3Header mHeader;
  // If a file contains multiple ID3 headers, then we would only select the
  // latest one, but keep the size of former abandoned in order to return the
  // correct size offset.
  uint32_t mFormerID3Size = 0;
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
    uint32_t SampleRate() const;

    // Number of audio channels.
    uint32_t Channels() const;

    // Samples per frames, static depending on MPEG version and layer.
    uint32_t SamplesPerFrame() const;

    // Slot size used for padding, static depending on MPEG layer.
    uint32_t SlotSize() const;

    // Bitrate in kbps, can vary between frames.
    uint32_t Bitrate() const;

    // MPEG layer (0->invalid, 1->I, 2->II, 3->III).
    uint32_t Layer() const;

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
    uint8_t mRaw[SIZE] = {};

    // The current byte position in the parsed sequence. Reset via Reset and
    // incremented via Update.
    int mPos = 0;
  };

  // VBR frames may contain Xing or VBRI headers for additional info, we use
  // this class to parse them and access this info.
  class VBRHeader {
   public:
    // Synchronize with vbr_header TYPE_STR on change.
    enum VBRHeaderType { NONE = 0, XING, VBRI };

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
    int64_t Offset(media::TimeUnit aTime, media::TimeUnit aDuration) const;

    // Parses contents of given ByteReader for a valid VBR header.
    // The offset of the passed ByteReader needs to point to an MPEG frame
    // begin, as a VBRI-style header is searched at a fixed offset relative to
    // frame begin. Returns whether a valid VBR header was found in the range.
    bool Parse(BufferReader* aReader, size_t aFrameSize);

    uint32_t EncoderDelay() const { return mEncoderDelay; }
    uint32_t EncoderPadding() const { return mEncoderPadding; }

   private:
    // Parses contents of given ByteReader for a valid Xing header.
    // The initial ByteReader offset will be preserved.
    // Returns whether a valid Xing header was found in the range.
    Result<bool, nsresult> ParseXing(BufferReader* aReader, size_t aFrameSize);

    // Parses contents of given ByteReader for a valid VBRI header.
    // The initial ByteReader offset will be preserved. It also needs to point
    // to the beginning of a valid MPEG frame, as VBRI headers are searched
    // at a fixed offset relative to frame begin.
    // Returns whether a valid VBRI header was found in the range.
    Result<bool, nsresult> ParseVBRI(BufferReader* aReader);

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

    uint16_t mVBRISeekOffsetsFramesPerEntry = 0;

    // Delay and padding values found in the LAME header. The encoder delay is a
    // number of frames that has to be skipped at the beginning of the stream,
    // encoder padding is a number of frames that needs to be ignored in the
    // last packet.
    uint16_t mEncoderDelay = 0;
    uint16_t mEncoderPadding = 0;
  };

  // Frame meta container used to parse and hold a frame header and side info.
  class Frame {
   public:
    // Returns the length of the frame excluding the header in bytes.
    uint32_t Length() const;

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

  // Returns whether ID3 metadata have been found, at the end of the file.
  bool ID3v1MetadataFound() const;

  // Returns the size of all parsed ID3 headers.
  uint32_t TotalID3HeaderSize() const;

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

  // Parses contents of given BufferReader for a valid frame header and returns
  // true if one was found. After returning, the variable passed to
  // 'aBytesToSkip' holds the amount of bytes to be skipped (if any) in order to
  // jump across a large ID3v2 tag spanning multiple buffers.
  Result<bool, nsresult> Parse(BufferReader* aReader, uint32_t* aBytesToSkip);

  // Parses contents of given BufferReader for a valid VBR header.
  // The offset of the passed BufferReader needs to point to an MPEG frame
  // begin, as a VBRI-style header is searched at a fixed offset relative to
  // frame begin. Returns whether a valid VBR header was found.
  bool ParseVBRHeader(BufferReader* aReader);

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
  // If this is true, ID3v1 metadata have been found at the end of the file, and
  // must be sustracted from the stream size in order to compute the stream
  // duration, when computing the duration of a CBR file based on its length in
  // bytes. This means that the duration can change at the moment we reach the
  // end of the file.
  bool mID3v1MetadataFound = false;
};

}  // namespace mozilla

#endif
