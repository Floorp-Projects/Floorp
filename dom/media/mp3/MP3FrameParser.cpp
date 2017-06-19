/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MP3FrameParser.h"

#include <algorithm>
#include <inttypes.h>

#include "mozilla/Assertions.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/SizePrintfMacros.h"
#include "VideoUtils.h"

extern mozilla::LazyLogModule gMediaDemuxerLog;
#define MP3LOG(msg, ...) \
  MOZ_LOG(gMediaDemuxerLog, LogLevel::Debug, ("MP3Demuxer " msg, ##__VA_ARGS__))
#define MP3LOGV(msg, ...) \
  MOZ_LOG(gMediaDemuxerLog, LogLevel::Verbose, ("MP3Demuxer " msg, ##__VA_ARGS__))

using mp4_demuxer::ByteReader;

namespace mozilla {

// FrameParser

namespace frame_header {
// FrameHeader mRaw byte offsets.
static const int SYNC1 = 0;
static const int SYNC2_VERSION_LAYER_PROTECTION = 1;
static const int BITRATE_SAMPLERATE_PADDING_PRIVATE = 2;
static const int CHANNELMODE_MODEEXT_COPY_ORIG_EMPH = 3;
} // namespace frame_header

FrameParser::FrameParser()
{
}

void
FrameParser::Reset()
{
  mID3Parser.Reset();
  mFrame.Reset();
}

void
FrameParser::ResetFrameData()
{
  mFrame.Reset();
  mFirstFrame.Reset();
  mPrevFrame.Reset();
}

void
FrameParser::EndFrameSession()
{
  if (!mID3Parser.Header().IsValid()) {
    // Reset ID3 tags only if we have not parsed a valid ID3 header yet.
    mID3Parser.Reset();
  }
  mPrevFrame = mFrame;
  mFrame.Reset();
}

const FrameParser::Frame&
FrameParser::CurrentFrame() const
{
  return mFrame;
}

const FrameParser::Frame&
FrameParser::PrevFrame() const
{
  return mPrevFrame;
}

const FrameParser::Frame&
FrameParser::FirstFrame() const
{
  return mFirstFrame;
}

const ID3Parser::ID3Header&
FrameParser::ID3Header() const
{
  return mID3Parser.Header();
}

const FrameParser::VBRHeader&
FrameParser::VBRInfo() const
{
  return mVBRHeader;
}

bool
FrameParser::Parse(ByteReader* aReader, uint32_t* aBytesToSkip)
{
  MOZ_ASSERT(aReader && aBytesToSkip);
  *aBytesToSkip = 0;

  if (!mID3Parser.Header().Size() && !mFirstFrame.Length()) {
    // No MP3 frames have been parsed yet, look for ID3v2 headers at file begin.
    // ID3v1 tags may only be at file end.
    // TODO: should we try to read ID3 tags at end of file/mid-stream, too?
    const size_t prevReaderOffset = aReader->Offset();
    const uint32_t tagSize = mID3Parser.Parse(aReader);
    if (tagSize) {
      // ID3 tag found, skip past it.
      const uint32_t skipSize = tagSize - ID3Parser::ID3Header::SIZE;

      if (skipSize > aReader->Remaining()) {
        // Skipping across the ID3v2 tag would take us past the end of the
        // buffer, therefore we return immediately and let the calling function
        // handle skipping the rest of the tag.
        MP3LOGV("ID3v2 tag detected, size=%d,"
                " needing to skip %" PRIuSIZE " bytes past the current buffer",
                tagSize, skipSize - aReader->Remaining());
        *aBytesToSkip = skipSize - aReader->Remaining();
        return false;
      }
      MP3LOGV("ID3v2 tag detected, size=%d", tagSize);
      aReader->Read(skipSize);
    } else {
      // No ID3v2 tag found, rewinding reader in order to search for a MPEG
      // frame header.
      aReader->Seek(prevReaderOffset);
    }
  }

  while (aReader->CanRead8() && !mFrame.ParseNext(aReader->ReadU8())) { }

  if (mFrame.Length()) {
    // MP3 frame found.
    if (!mFirstFrame.Length()) {
      mFirstFrame = mFrame;
    }
    // Indicate success.
    return true;
  }
  return false;
}

// FrameParser::Header

FrameParser::FrameHeader::FrameHeader()
{
  Reset();
}

uint8_t
FrameParser::FrameHeader::Sync1() const
{
  return mRaw[frame_header::SYNC1];
}

uint8_t
FrameParser::FrameHeader::Sync2() const
{
  return 0x7 & mRaw[frame_header::SYNC2_VERSION_LAYER_PROTECTION] >> 5;
}

uint8_t
FrameParser::FrameHeader::RawVersion() const
{
  return 0x3 & mRaw[frame_header::SYNC2_VERSION_LAYER_PROTECTION] >> 3;
}

uint8_t
FrameParser::FrameHeader::RawLayer() const
{
  return 0x3 & mRaw[frame_header::SYNC2_VERSION_LAYER_PROTECTION] >> 1;
}

uint8_t
FrameParser::FrameHeader::RawProtection() const
{
  return 0x1 & mRaw[frame_header::SYNC2_VERSION_LAYER_PROTECTION] >> 6;
}

uint8_t
FrameParser::FrameHeader::RawBitrate() const
{
  return 0xF & mRaw[frame_header::BITRATE_SAMPLERATE_PADDING_PRIVATE] >> 4;
}

uint8_t
FrameParser::FrameHeader::RawSampleRate() const
{
  return 0x3 & mRaw[frame_header::BITRATE_SAMPLERATE_PADDING_PRIVATE] >> 2;
}

uint8_t
FrameParser::FrameHeader::Padding() const
{
  return 0x1 & mRaw[frame_header::BITRATE_SAMPLERATE_PADDING_PRIVATE] >> 1;
}

uint8_t
FrameParser::FrameHeader::Private() const
{
  return 0x1 & mRaw[frame_header::BITRATE_SAMPLERATE_PADDING_PRIVATE];
}

uint8_t
FrameParser::FrameHeader::RawChannelMode() const
{
  return 0x3 & mRaw[frame_header::CHANNELMODE_MODEEXT_COPY_ORIG_EMPH] >> 6;
}

int32_t
FrameParser::FrameHeader::Layer() const
{
  static const uint8_t LAYERS[4] = { 0, 3, 2, 1 };

  return LAYERS[RawLayer()];
}

int32_t
FrameParser::FrameHeader::SampleRate() const
{
  // Sample rates - use [version][srate]
  static const uint16_t SAMPLE_RATE[4][4] = {
    { 11025, 12000,  8000, 0 }, // MPEG 2.5
    {     0,     0,     0, 0 }, // Reserved
    { 22050, 24000, 16000, 0 }, // MPEG 2
    { 44100, 48000, 32000, 0 }  // MPEG 1
  };

  return SAMPLE_RATE[RawVersion()][RawSampleRate()];
}

int32_t
FrameParser::FrameHeader::Channels() const
{
  // 3 is single channel (mono), any other value is some variant of dual
  // channel.
  return RawChannelMode() == 3 ? 1 : 2;
}

int32_t
FrameParser::FrameHeader::SamplesPerFrame() const
{
  // Samples per frame - use [version][layer]
  static const uint16_t FRAME_SAMPLE[4][4] = {
    // Layer     3     2     1       Version
    {      0,  576, 1152,  384 }, // 2.5
    {      0,    0,    0,    0 }, // Reserved
    {      0,  576, 1152,  384 }, // 2
    {      0, 1152, 1152,  384 }  // 1
  };

  return FRAME_SAMPLE[RawVersion()][RawLayer()];
}

int32_t
FrameParser::FrameHeader::Bitrate() const
{
  // Bitrates - use [version][layer][bitrate]
  static const uint16_t BITRATE[4][4][16] = {
    { // Version 2.5
      { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }, // Reserved
      { 0,   8,  16,  24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 }, // Layer 3
      { 0,   8,  16,  24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 }, // Layer 2
      { 0,  32,  48,  56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, 0 }  // Layer 1
    },
    { // Reserved
      { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }, // Invalid
      { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }, // Invalid
      { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }, // Invalid
      { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }  // Invalid
    },
    { // Version 2
      { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }, // Reserved
      { 0,   8,  16,  24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 }, // Layer 3
      { 0,   8,  16,  24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 }, // Layer 2
      { 0,  32,  48,  56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, 0 }  // Layer 1
    },
    { // Version 1
      { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }, // Reserved
      { 0,  32,  40,  48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 0 }, // Layer 3
      { 0,  32,  48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, 0 }, // Layer 2
      { 0,  32,  64,  96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0 }, // Layer 1
    }
  };

  return 1000 * BITRATE[RawVersion()][RawLayer()][RawBitrate()];
}

int32_t
FrameParser::FrameHeader::SlotSize() const
{
  // Slot size (MPEG unit of measurement) - use [layer]
  static const uint8_t SLOT_SIZE[4] = { 0, 1, 1, 4 }; // Rsvd, 3, 2, 1

  return SLOT_SIZE[RawLayer()];
}

bool
FrameParser::FrameHeader::ParseNext(uint8_t c)
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
FrameParser::FrameHeader::IsValid(int aPos) const
{
  if (aPos >= SIZE) {
    return true;
  }
  if (aPos == frame_header::SYNC1) {
    return Sync1() == 0xFF;
  }
  if (aPos == frame_header::SYNC2_VERSION_LAYER_PROTECTION) {
    return Sync2() == 7 &&
           RawVersion() != 1 &&
           Layer() == 3;
  }
  if (aPos == frame_header::BITRATE_SAMPLERATE_PADDING_PRIVATE) {
    return RawBitrate() != 0xF && RawBitrate() != 0 &&
           RawSampleRate() != 3;
  }
  return true;
}

bool
FrameParser::FrameHeader::IsValid() const
{
  return mPos >= SIZE;
}

void
FrameParser::FrameHeader::Reset()
{
  mPos = 0;
}

bool
FrameParser::FrameHeader::Update(uint8_t c)
{
  if (mPos < SIZE) {
    mRaw[mPos] = c;
  }
  return IsValid(mPos++);
}

// FrameParser::VBRHeader

namespace vbr_header {
static const char* TYPE_STR[3] = {"NONE", "XING", "VBRI"};
static const uint32_t TOC_SIZE = 100;
} // namespace vbr_header

FrameParser::VBRHeader::VBRHeader()
  : mType(NONE)
{
}

FrameParser::VBRHeader::VBRHeaderType
FrameParser::VBRHeader::Type() const
{
  return mType;
}

const Maybe<uint32_t>&
FrameParser::VBRHeader::NumAudioFrames() const
{
  return mNumAudioFrames;
}

const Maybe<uint32_t>&
FrameParser::VBRHeader::NumBytes() const
{
  return mNumBytes;
}

const Maybe<uint32_t>&
FrameParser::VBRHeader::Scale() const
{
  return mScale;
}

bool
FrameParser::VBRHeader::IsTOCPresent() const
{
  return mTOC.size() == vbr_header::TOC_SIZE;
}

bool
FrameParser::VBRHeader::IsValid() const
{
  return mType != NONE;
}

bool
FrameParser::VBRHeader::IsComplete() const
{
  return IsValid()
         && mNumAudioFrames.valueOr(0) > 0
         && mNumBytes.valueOr(0) > 0
         // We don't care about the scale for any computations here.
         // && mScale < 101
         && true;
}

int64_t
FrameParser::VBRHeader::Offset(float aDurationFac) const
{
  if (!IsTOCPresent()) {
    return -1;
  }

  // Constrain the duration percentage to [0, 99].
  const float durationPer =
    100.0f * std::min(0.99f, std::max(0.0f, aDurationFac));
  const size_t fullPer = durationPer;
  const float rest = durationPer - fullPer;

  MOZ_ASSERT(fullPer < mTOC.size());
  int64_t offset = mTOC.at(fullPer);

  if (rest > 0.0 && fullPer + 1 < mTOC.size()) {
    offset += rest * (mTOC.at(fullPer + 1) - offset);
  }

  return offset;
}

bool
FrameParser::VBRHeader::ParseXing(ByteReader* aReader)
{
  static const uint32_t XING_TAG = BigEndian::readUint32("Xing");
  static const uint32_t INFO_TAG = BigEndian::readUint32("Info");

  enum Flags
  {
    NUM_FRAMES = 0x01,
    NUM_BYTES = 0x02,
    TOC = 0x04,
    VBR_SCALE = 0x08
  };

  MOZ_ASSERT(aReader);
  const size_t prevReaderOffset = aReader->Offset();

  // We have to search for the Xing header as its position can change.
  while (aReader->CanRead32() &&
         aReader->PeekU32() != XING_TAG && aReader->PeekU32() != INFO_TAG) {
    aReader->Read(1);
  }

  if (aReader->CanRead32()) {
    // Skip across the VBR header ID tag.
    aReader->ReadU32();
    mType = XING;
  }
  uint32_t flags = 0;
  if (aReader->CanRead32()) {
    flags = aReader->ReadU32();
  }
  if (flags & NUM_FRAMES && aReader->CanRead32()) {
    mNumAudioFrames = Some(aReader->ReadU32());
  }
  if (flags & NUM_BYTES && aReader->CanRead32()) {
    mNumBytes = Some(aReader->ReadU32());
  }
  if (flags & TOC && aReader->Remaining() >= vbr_header::TOC_SIZE) {
    if (!mNumBytes) {
      // We don't have the stream size to calculate offsets, skip the TOC.
      aReader->Read(vbr_header::TOC_SIZE);
    } else {
      mTOC.clear();
      mTOC.reserve(vbr_header::TOC_SIZE);
      for (size_t i = 0; i < vbr_header::TOC_SIZE; ++i) {
        mTOC.push_back(1.0f / 256.0f * aReader->ReadU8() * mNumBytes.value());
      }
    }
  }
  if (flags & VBR_SCALE && aReader->CanRead32()) {
    mScale = Some(aReader->ReadU32());
  }

  aReader->Seek(prevReaderOffset);
  return mType == XING;
}

bool
FrameParser::VBRHeader::ParseVBRI(ByteReader* aReader)
{
  static const uint32_t TAG = BigEndian::readUint32("VBRI");
  static const uint32_t OFFSET = 32 + FrameParser::FrameHeader::SIZE;
  static const uint32_t FRAME_COUNT_OFFSET = OFFSET + 14;
  static const uint32_t MIN_FRAME_SIZE = OFFSET + 26;

  MOZ_ASSERT(aReader);
  // ParseVBRI assumes that the ByteReader offset points to the beginning of a
  // frame, therefore as a simple check, we look for the presence of a frame
  // sync at that position.
  MOZ_ASSERT((aReader->PeekU16() & 0xFFE0) == 0xFFE0);
  const size_t prevReaderOffset = aReader->Offset();

  // VBRI have a fixed relative position, so let's check for it there.
  if (aReader->Remaining() > MIN_FRAME_SIZE) {
    aReader->Seek(prevReaderOffset + OFFSET);
    if (aReader->ReadU32() == TAG) {
      aReader->Seek(prevReaderOffset + FRAME_COUNT_OFFSET);
      mNumAudioFrames = Some(aReader->ReadU32());
      mType = VBRI;
      aReader->Seek(prevReaderOffset);
      return true;
    }
  }
  aReader->Seek(prevReaderOffset);
  return false;
}

bool
FrameParser::VBRHeader::Parse(ByteReader* aReader)
{
  const bool rv = ParseVBRI(aReader) || ParseXing(aReader);
  if (rv) {
    MP3LOG("VBRHeader::Parse found valid VBR/CBR header: type=%s"
           " NumAudioFrames=%u NumBytes=%u Scale=%u TOC-size=%" PRIuSIZE,
           vbr_header::TYPE_STR[Type()], NumAudioFrames().valueOr(0),
           NumBytes().valueOr(0), Scale().valueOr(0), mTOC.size());
  }
  return rv;
}

// FrameParser::Frame

void
FrameParser::Frame::Reset()
{
  mHeader.Reset();
}

int32_t
FrameParser::Frame::Length() const
{
  if (!mHeader.IsValid() || !mHeader.SampleRate()) {
    return 0;
  }

  const float bitsPerSample = mHeader.SamplesPerFrame() / 8.0f;
  const int32_t frameLen = bitsPerSample * mHeader.Bitrate()
                           / mHeader.SampleRate()
                           + mHeader.Padding() * mHeader.SlotSize();
  return frameLen;
}

bool
FrameParser::Frame::ParseNext(uint8_t c)
{
  return mHeader.ParseNext(c);
}

const FrameParser::FrameHeader&
FrameParser::Frame::Header() const
{
  return mHeader;
}

bool
FrameParser::ParseVBRHeader(ByteReader* aReader)
{
  return mVBRHeader.Parse(aReader);
}

// ID3Parser

// Constants
namespace id3_header {
static const int ID_LEN = 3;
static const int VERSION_LEN = 2;
static const int FLAGS_LEN = 1;
static const int SIZE_LEN = 4;

static const int ID_END = ID_LEN;
static const int VERSION_END = ID_END + VERSION_LEN;
static const int FLAGS_END = VERSION_END + FLAGS_LEN;
static const int SIZE_END = FLAGS_END + SIZE_LEN;

static const uint8_t ID[ID_LEN] = {'I', 'D', '3'};

static const uint8_t MIN_MAJOR_VER = 2;
static const uint8_t MAX_MAJOR_VER = 4;
} // namespace id3_header

uint32_t
ID3Parser::Parse(ByteReader* aReader)
{
  MOZ_ASSERT(aReader);

  while (aReader->CanRead8() && !mHeader.ParseNext(aReader->ReadU8())) { }

  return mHeader.TotalTagSize();
}

void
ID3Parser::Reset()
{
  mHeader.Reset();
}

const ID3Parser::ID3Header&
ID3Parser::Header() const
{
  return mHeader;
}

// ID3Parser::Header

ID3Parser::ID3Header::ID3Header()
{
  Reset();
}

void
ID3Parser::ID3Header::Reset()
{
  mSize = 0;
  mPos = 0;
}

uint8_t
ID3Parser::ID3Header::MajorVersion() const
{
  return mRaw[id3_header::ID_END];
}

uint8_t
ID3Parser::ID3Header::MinorVersion() const
{
  return mRaw[id3_header::ID_END + 1];
}

uint8_t
ID3Parser::ID3Header::Flags() const
{
  return mRaw[id3_header::FLAGS_END - id3_header::FLAGS_LEN];
}

uint32_t
ID3Parser::ID3Header::Size() const
{
  if (!IsValid()) {
    return 0;
  }
  return mSize;
}

uint8_t
ID3Parser::ID3Header::FooterSize() const
{
  if (Flags() & (1 << 4)) {
    return SIZE;
  }
  return 0;
}

uint32_t
ID3Parser::ID3Header::TotalTagSize() const
{
  if (IsValid()) {
    // Header found, return total tag size.
    return ID3Header::SIZE + Size() + FooterSize();
  }
  return 0;
}

bool
ID3Parser::ID3Header::ParseNext(uint8_t c)
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
ID3Parser::ID3Header::IsValid(int aPos) const
{
  if (aPos >= SIZE) {
    return true;
  }
  const uint8_t c = mRaw[aPos];
  switch (aPos) {
    case 0: case 1: case 2:
      // Expecting "ID3".
      return id3_header::ID[aPos] == c;
    case 3:
      return MajorVersion() >= id3_header::MIN_MAJOR_VER
             && MajorVersion() <= id3_header::MAX_MAJOR_VER;
    case 4:
      return MinorVersion() < 0xFF;
    case 5:
      // Validate flags for supported versions, see bug 949036.
      return ((0xFF >> MajorVersion()) & c) == 0;
    case 6: case 7: case 8: case 9:
      return c < 0x80;
  }
  return true;
}

bool
ID3Parser::ID3Header::IsValid() const
{
  return mPos >= SIZE;
}

bool
ID3Parser::ID3Header::Update(uint8_t c)
{
  if (mPos >= id3_header::SIZE_END - id3_header::SIZE_LEN
      && mPos < id3_header::SIZE_END) {
    mSize <<= 7;
    mSize |= c;
  }
  if (mPos < SIZE) {
    mRaw[mPos] = c;
  }
  return IsValid(mPos++);
}

} // namespace mozilla
