/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MP3FrameParser.h"

#include <algorithm>
#include <inttypes.h>

#include "TimeUnits.h"
#include "mozilla/Assertions.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Try.h"
#include "VideoUtils.h"

extern mozilla::LazyLogModule gMediaDemuxerLog;
#define MP3LOG(msg, ...) \
  MOZ_LOG(gMediaDemuxerLog, LogLevel::Debug, ("MP3Demuxer " msg, ##__VA_ARGS__))
#define MP3LOGV(msg, ...)                      \
  MOZ_LOG(gMediaDemuxerLog, LogLevel::Verbose, \
          ("MP3Demuxer " msg, ##__VA_ARGS__))

namespace mozilla {

// FrameParser

namespace frame_header {
// FrameHeader mRaw byte offsets.
static const int SYNC1 = 0;
static const int SYNC2_VERSION_LAYER_PROTECTION = 1;
static const int BITRATE_SAMPLERATE_PADDING_PRIVATE = 2;
static const int CHANNELMODE_MODEEXT_COPY_ORIG_EMPH = 3;
}  // namespace frame_header

FrameParser::FrameParser() = default;

void FrameParser::Reset() {
  mID3Parser.Reset();
  mFrame.Reset();
}

void FrameParser::ResetFrameData() {
  mFrame.Reset();
  mFirstFrame.Reset();
  mPrevFrame.Reset();
}

void FrameParser::EndFrameSession() {
  if (!mID3Parser.Header().IsValid()) {
    // Reset ID3 tags only if we have not parsed a valid ID3 header yet.
    mID3Parser.Reset();
  }
  mPrevFrame = mFrame;
  mFrame.Reset();
}

const FrameParser::Frame& FrameParser::CurrentFrame() const { return mFrame; }

const FrameParser::Frame& FrameParser::PrevFrame() const { return mPrevFrame; }

const FrameParser::Frame& FrameParser::FirstFrame() const {
  return mFirstFrame;
}

const ID3Parser::ID3Header& FrameParser::ID3Header() const {
  return mID3Parser.Header();
}

uint32_t FrameParser::TotalID3HeaderSize() const {
  uint32_t ID3v1Size = 0;
  if (mID3v1MetadataFound) {
    ID3v1Size = 128;
  }
  return ID3v1Size + mID3Parser.TotalHeadersSize();
}

const FrameParser::VBRHeader& FrameParser::VBRInfo() const {
  return mVBRHeader;
}

Result<bool, nsresult> FrameParser::Parse(BufferReader* aReader,
                                          uint32_t* aBytesToSkip) {
  MOZ_ASSERT(aReader && aBytesToSkip);
  *aBytesToSkip = 0;

  if (ID3Parser::IsBufferStartingWithID3v1Tag(aReader)) {
    // This is usually at the end of the file, and is always 128 bytes, that
    // can simply be skipped.
    aReader->Read(128);
    *aBytesToSkip = 128;
    mID3v1MetadataFound = true;
    MP3LOGV("ID3v1 tag detected, skipping 128 bytes past the current buffer");
    return false;
  }

  if (ID3Parser::IsBufferStartingWithID3Tag(aReader) && !mFirstFrame.Length()) {
    // No MP3 frames have been parsed yet, look for ID3v2 headers at file begin.
    // ID3v1 tags may only be at file end.
    const size_t prevReaderOffset = aReader->Offset();
    const uint32_t tagSize = mID3Parser.Parse(aReader);
    if (!!tagSize) {
      // ID3 tag found, skip past it.
      const uint32_t skipSize = tagSize - ID3Parser::ID3Header::SIZE;

      if (skipSize > aReader->Remaining()) {
        // Skipping across the ID3v2 tag would take us past the end of the
        // buffer, therefore we return immediately and let the calling function
        // handle skipping the rest of the tag.
        MP3LOGV(
            "ID3v2 tag detected, size=%d,"
            " needing to skip %zu bytes past the current buffer",
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

  for (auto res = aReader->ReadU8();
       res.isOk() && !mFrame.ParseNext(res.unwrap()); res = aReader->ReadU8()) {
  }

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

FrameParser::FrameHeader::FrameHeader() { Reset(); }

uint8_t FrameParser::FrameHeader::Sync1() const {
  return mRaw[frame_header::SYNC1];
}

uint8_t FrameParser::FrameHeader::Sync2() const {
  return 0x7 & mRaw[frame_header::SYNC2_VERSION_LAYER_PROTECTION] >> 5;
}

uint8_t FrameParser::FrameHeader::RawVersion() const {
  return 0x3 & mRaw[frame_header::SYNC2_VERSION_LAYER_PROTECTION] >> 3;
}

uint8_t FrameParser::FrameHeader::RawLayer() const {
  return 0x3 & mRaw[frame_header::SYNC2_VERSION_LAYER_PROTECTION] >> 1;
}

uint8_t FrameParser::FrameHeader::RawProtection() const {
  return 0x1 & mRaw[frame_header::SYNC2_VERSION_LAYER_PROTECTION] >> 6;
}

uint8_t FrameParser::FrameHeader::RawBitrate() const {
  return 0xF & mRaw[frame_header::BITRATE_SAMPLERATE_PADDING_PRIVATE] >> 4;
}

uint8_t FrameParser::FrameHeader::RawSampleRate() const {
  return 0x3 & mRaw[frame_header::BITRATE_SAMPLERATE_PADDING_PRIVATE] >> 2;
}

uint8_t FrameParser::FrameHeader::Padding() const {
  return 0x1 & mRaw[frame_header::BITRATE_SAMPLERATE_PADDING_PRIVATE] >> 1;
}

uint8_t FrameParser::FrameHeader::Private() const {
  return 0x1 & mRaw[frame_header::BITRATE_SAMPLERATE_PADDING_PRIVATE];
}

uint8_t FrameParser::FrameHeader::RawChannelMode() const {
  return 0x3 & mRaw[frame_header::CHANNELMODE_MODEEXT_COPY_ORIG_EMPH] >> 6;
}

uint32_t FrameParser::FrameHeader::Layer() const {
  static const uint8_t LAYERS[4] = {0, 3, 2, 1};

  return LAYERS[RawLayer()];
}

uint32_t FrameParser::FrameHeader::SampleRate() const {
  // Sample rates - use [version][srate]
  static const uint16_t SAMPLE_RATE[4][4] = {
      // clang-format off
    { 11025, 12000,  8000, 0 }, // MPEG 2.5
    {     0,     0,     0, 0 }, // Reserved
    { 22050, 24000, 16000, 0 }, // MPEG 2
    { 44100, 48000, 32000, 0 }  // MPEG 1
      // clang-format on
  };

  return SAMPLE_RATE[RawVersion()][RawSampleRate()];
}

uint32_t FrameParser::FrameHeader::Channels() const {
  // 3 is single channel (mono), any other value is some variant of dual
  // channel.
  return RawChannelMode() == 3 ? 1 : 2;
}

uint32_t FrameParser::FrameHeader::SamplesPerFrame() const {
  // Samples per frame - use [version][layer]
  static const uint16_t FRAME_SAMPLE[4][4] = {
      // clang-format off
    // Layer     3     2     1       Version
    {      0,  576, 1152,  384 }, // 2.5
    {      0,    0,    0,    0 }, // Reserved
    {      0,  576, 1152,  384 }, // 2
    {      0, 1152, 1152,  384 }  // 1
      // clang-format on
  };

  return FRAME_SAMPLE[RawVersion()][RawLayer()];
}

uint32_t FrameParser::FrameHeader::Bitrate() const {
  // Bitrates - use [version][layer][bitrate]
  static const uint16_t BITRATE[4][4][16] = {
      // clang-format off
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
      // clang-format on
  };

  return 1000 * BITRATE[RawVersion()][RawLayer()][RawBitrate()];
}

uint32_t FrameParser::FrameHeader::SlotSize() const {
  // Slot size (MPEG unit of measurement) - use [layer]
  static const uint8_t SLOT_SIZE[4] = {0, 1, 1, 4};  // Rsvd, 3, 2, 1

  return SLOT_SIZE[RawLayer()];
}

bool FrameParser::FrameHeader::ParseNext(uint8_t c) {
  if (!Update(c)) {
    Reset();
    if (!Update(c)) {
      Reset();
    }
  }
  return IsValid();
}

bool FrameParser::ID3v1MetadataFound() const { return mID3v1MetadataFound; }

bool FrameParser::FrameHeader::IsValid(int aPos) const {
  if (aPos >= SIZE) {
    return true;
  }
  if (aPos == frame_header::SYNC1) {
    return Sync1() == 0xFF;
  }
  if (aPos == frame_header::SYNC2_VERSION_LAYER_PROTECTION) {
    return Sync2() == 7 && RawVersion() != 1 && Layer() == 3;
  }
  if (aPos == frame_header::BITRATE_SAMPLERATE_PADDING_PRIVATE) {
    return RawBitrate() != 0xF && RawBitrate() != 0 && RawSampleRate() != 3;
  }
  return true;
}

bool FrameParser::FrameHeader::IsValid() const { return mPos >= SIZE; }

void FrameParser::FrameHeader::Reset() { mPos = 0; }

bool FrameParser::FrameHeader::Update(uint8_t c) {
  if (mPos < SIZE) {
    mRaw[mPos] = c;
  }
  return IsValid(mPos++);
}

// FrameParser::VBRHeader

namespace vbr_header {
static const char* TYPE_STR[3] = {"NONE", "XING", "VBRI"};
static const uint32_t TOC_SIZE = 100;
}  // namespace vbr_header

FrameParser::VBRHeader::VBRHeader() : mType(NONE) {}

FrameParser::VBRHeader::VBRHeaderType FrameParser::VBRHeader::Type() const {
  return mType;
}

const Maybe<uint32_t>& FrameParser::VBRHeader::NumAudioFrames() const {
  return mNumAudioFrames;
}

const Maybe<uint32_t>& FrameParser::VBRHeader::NumBytes() const {
  return mNumBytes;
}

const Maybe<uint32_t>& FrameParser::VBRHeader::Scale() const { return mScale; }

bool FrameParser::VBRHeader::IsTOCPresent() const {
  // This doesn't use VBRI TOC
  return !mTOC.empty() && mType != VBRI;
}

bool FrameParser::VBRHeader::IsValid() const { return mType != NONE; }

bool FrameParser::VBRHeader::IsComplete() const {
  return IsValid() && mNumAudioFrames.valueOr(0) > 0 && mNumBytes.valueOr(0) > 0
      // We don't care about the scale for any computations here.
      // && mScale < 101
      ;
}

int64_t FrameParser::VBRHeader::Offset(media::TimeUnit aTime,
                                       media::TimeUnit aDuration) const {
  if (!IsTOCPresent()) {
    return -1;
  }

  int64_t offset = -1;
  if (mType == XING) {
    // Constrain the duration percentage to [0, 99].
    double percent = 100. * aTime.ToSeconds() / aDuration.ToSeconds();
    const double durationPer = std::clamp(percent, 0., 99.);
    double integer;
    const double fractional = modf(durationPer, &integer);
    size_t integerPer = AssertedCast<size_t>(integer);

    MOZ_ASSERT(integerPer < mTOC.size());
    offset = mTOC.at(integerPer);
    if (fractional > 0.0 && integerPer + 1 < mTOC.size()) {
      offset += AssertedCast<int64_t>(fractional) *
                (mTOC.at(integerPer + 1) - offset);
    }
  }
  // TODO: VBRI TOC seeking
  MP3LOG("VBRHeader::Offset (%s): %f is at byte %" PRId64 "",
         mType == XING ? "XING" : "VBRI", aTime.ToSeconds(), offset);

  return offset;
}

Result<bool, nsresult> FrameParser::VBRHeader::ParseXing(BufferReader* aReader,
                                                         size_t aFrameSize) {
  static const uint32_t XING_TAG = BigEndian::readUint32("Xing");
  static const uint32_t INFO_TAG = BigEndian::readUint32("Info");
  static const uint32_t LAME_TAG = BigEndian::readUint32("LAME");
  static const uint32_t LAVC_TAG = BigEndian::readUint32("Lavc");

  enum Flags {
    NUM_FRAMES = 0x01,
    NUM_BYTES = 0x02,
    TOC = 0x04,
    VBR_SCALE = 0x08
  };

  MOZ_ASSERT(aReader);

  // Seek backward to the original position before leaving this scope.
  const size_t prevReaderOffset = aReader->Offset();
  auto scopeExit = MakeScopeExit([&] { aReader->Seek(prevReaderOffset); });

  // We have to search for the Xing header as its position can change.
  for (auto res = aReader->PeekU32();
       res.isOk() && res.unwrap() != XING_TAG && res.unwrap() != INFO_TAG;) {
    aReader->Read(1);
    res = aReader->PeekU32();
  }

  // Skip across the VBR header ID tag.
  MOZ_TRY(aReader->ReadU32());
  mType = XING;

  uint32_t flags;
  MOZ_TRY_VAR(flags, aReader->ReadU32());

  if (flags & NUM_FRAMES) {
    uint32_t frames;
    MOZ_TRY_VAR(frames, aReader->ReadU32());
    mNumAudioFrames = Some(frames);
  }
  if (flags & NUM_BYTES) {
    uint32_t bytes;
    MOZ_TRY_VAR(bytes, aReader->ReadU32());
    mNumBytes = Some(bytes);
  }
  if (flags & TOC && aReader->Remaining() >= vbr_header::TOC_SIZE) {
    if (!mNumBytes) {
      // We don't have the stream size to calculate offsets, skip the TOC.
      aReader->Read(vbr_header::TOC_SIZE);
    } else {
      mTOC.clear();
      mTOC.reserve(vbr_header::TOC_SIZE);
      uint8_t data;
      for (size_t i = 0; i < vbr_header::TOC_SIZE; ++i) {
        MOZ_TRY_VAR(data, aReader->ReadU8());
        mTOC.push_back(
            AssertedCast<uint32_t>(1.0f / 256.0f * data * mNumBytes.value()));
      }
    }
  }
  if (flags & VBR_SCALE) {
    uint32_t scale;
    MOZ_TRY_VAR(scale, aReader->ReadU32());
    mScale = Some(scale);
  }

  uint32_t lameOrLavcTag;
  MOZ_TRY_VAR(lameOrLavcTag, aReader->ReadU32());

  if (lameOrLavcTag == LAME_TAG || lameOrLavcTag == LAVC_TAG) {
    // Skip 17 bytes after the LAME tag:
    // - http://gabriel.mp3-tech.org/mp3infotag.html
    // - 5 bytes for the encoder short version string
    // - 1 byte for the info tag revision + VBR method
    // - 1 byte for the lowpass filter value
    // - 8 bytes for the ReplayGain information
    // - 1 byte for the encoding flags + ATH Type
    // - 1 byte for the specified bitrate if ABR, else the minimal bitrate
    if (!aReader->Read(17)) {
      return mozilla::Err(NS_ERROR_FAILURE);
    }

    // The encoder delay is three bytes, for two 12-bits integers are the
    // encoder delay and the padding.
    const uint8_t* delayPadding = aReader->Read(3);
    if (!delayPadding) {
      return mozilla::Err(NS_ERROR_FAILURE);
    }
    mEncoderDelay =
        uint32_t(delayPadding[0]) << 4 | (delayPadding[1] & 0xf0) >> 4;
    mEncoderPadding = uint32_t(delayPadding[1] & 0x0f) << 8 | delayPadding[2];

    constexpr uint16_t DEFAULT_DECODER_DELAY = 529;
    mEncoderDelay += DEFAULT_DECODER_DELAY + aFrameSize;  // ignore first frame.
    mEncoderPadding -= std::min(mEncoderPadding, DEFAULT_DECODER_DELAY);

    MP3LOG("VBRHeader::ParseXing: LAME encoder delay section: delay: %" PRIu16
           " frames, padding: %" PRIu16 " frames",
           mEncoderDelay, mEncoderPadding);
  }

  return mType == XING;
}

template <typename T>
int readAndConvertToInt(BufferReader* aReader) {
  int value = AssertedCast<int>(aReader->ReadType<T>());
  return value;
}

Result<bool, nsresult> FrameParser::VBRHeader::ParseVBRI(
    BufferReader* aReader) {
  static const uint32_t TAG = BigEndian::readUint32("VBRI");
  static const uint32_t OFFSET = 32 + FrameParser::FrameHeader::SIZE;
  static const uint32_t MIN_FRAME_SIZE = OFFSET + 26;

  MOZ_ASSERT(aReader);
  // ParseVBRI assumes that the ByteReader offset points to the beginning of a
  // frame, therefore as a simple check, we look for the presence of a frame
  // sync at that position.
  auto sync = aReader->PeekU16();
  if (sync.isOk()) {  // To avoid compiler complains 'set but unused'.
    MOZ_ASSERT((sync.unwrap() & 0xFFE0) == 0xFFE0);
  }

  // Seek backward to the original position before leaving this scope.
  const size_t prevReaderOffset = aReader->Offset();
  auto scopeExit = MakeScopeExit([&] { aReader->Seek(prevReaderOffset); });

  // VBRI have a fixed relative position, so let's check for it there.
  if (aReader->Remaining() > MIN_FRAME_SIZE) {
    aReader->Seek(prevReaderOffset + OFFSET);
    uint32_t tag;
    MOZ_TRY_VAR(tag, aReader->ReadU32());
    if (tag == TAG) {
      uint16_t vbriEncoderVersion, vbriEncoderDelay, vbriQuality;
      uint32_t vbriBytes, vbriFrames;
      uint16_t vbriSeekOffsetsTableSize, vbriSeekOffsetsScaleFactor,
          vbriSeekOffsetsBytesPerEntry, vbriSeekOffsetsFramesPerEntry;
      MOZ_TRY_VAR(vbriEncoderVersion, aReader->ReadU16());
      MOZ_TRY_VAR(vbriEncoderDelay, aReader->ReadU16());
      MOZ_TRY_VAR(vbriQuality, aReader->ReadU16());
      MOZ_TRY_VAR(vbriBytes, aReader->ReadU32());
      MOZ_TRY_VAR(vbriFrames, aReader->ReadU32());
      MOZ_TRY_VAR(vbriSeekOffsetsTableSize, aReader->ReadU16());
      MOZ_TRY_VAR(vbriSeekOffsetsScaleFactor, aReader->ReadU32());
      MOZ_TRY_VAR(vbriSeekOffsetsBytesPerEntry, aReader->ReadU16());
      MOZ_TRY_VAR(vbriSeekOffsetsFramesPerEntry, aReader->ReadU16());

      mTOC.reserve(vbriSeekOffsetsTableSize + 1);

      int (*readFunc)(BufferReader*) = nullptr;
      switch (vbriSeekOffsetsBytesPerEntry) {
        case 1:
          readFunc = &readAndConvertToInt<uint8_t>;
          break;
        case 2:
          readFunc = &readAndConvertToInt<int16_t>;
          break;
        case 4:
          readFunc = &readAndConvertToInt<int32_t>;
          break;
        case 8:
          readFunc = &readAndConvertToInt<int64_t>;
          break;
        default:
          MP3LOG("Unhandled vbriSeekOffsetsBytesPerEntry size of %hd",
                 vbriSeekOffsetsBytesPerEntry);
          break;
      }
      for (uint32_t i = 0; readFunc && i < vbriSeekOffsetsTableSize; i++) {
        int entry = readFunc(aReader);
        mTOC.push_back(entry * vbriSeekOffsetsScaleFactor);
      }
      MP3LOG(
          "Header::Parse found valid  header: EncoderVersion=%hu "
          "EncoderDelay=%hu "
          "Quality=%hu "
          "Bytes=%u "
          "Frames=%u "
          "SeekOffsetsTableSize=%u "
          "SeekOffsetsScaleFactor=%hu "
          "SeekOffsetsBytesPerEntry=%hu "
          "SeekOffsetsFramesPerEntry=%hu",
          vbriEncoderVersion, vbriEncoderDelay, vbriQuality, vbriBytes,
          vbriFrames, vbriSeekOffsetsTableSize, vbriSeekOffsetsScaleFactor,
          vbriSeekOffsetsBytesPerEntry, vbriSeekOffsetsFramesPerEntry);
      // Adjust the number of frames so it's counted the same way as in the XING
      // header
      if (vbriFrames < 1) {
        return false;
      }
      mNumAudioFrames = Some(vbriFrames - 1);
      mNumBytes = Some(vbriBytes);
      mEncoderDelay = vbriEncoderDelay;
      mVBRISeekOffsetsFramesPerEntry = vbriSeekOffsetsFramesPerEntry;
      MP3LOG("TOC:");
      for (auto entry : mTOC) {
        MP3LOG("%" PRId64, entry);
      }

      mType = VBRI;
      return true;
    }
  }
  return false;
}

bool FrameParser::VBRHeader::Parse(BufferReader* aReader, size_t aFrameSize) {
  auto res = std::make_pair(ParseVBRI(aReader), ParseXing(aReader, aFrameSize));
  const bool rv = (res.first.isOk() && res.first.unwrap()) ||
                  (res.second.isOk() && res.second.unwrap());
  if (rv) {
    MP3LOG(
        "VBRHeader::Parse found valid VBR/CBR header: type=%s"
        " NumAudioFrames=%u NumBytes=%u Scale=%u TOC-size=%zu Delay=%u",
        vbr_header::TYPE_STR[Type()], NumAudioFrames().valueOr(0),
        NumBytes().valueOr(0), Scale().valueOr(0), mTOC.size(), mEncoderDelay);
  }
  return rv;
}

// FrameParser::Frame

void FrameParser::Frame::Reset() { mHeader.Reset(); }

uint32_t FrameParser::Frame::Length() const {
  if (!mHeader.IsValid() || !mHeader.SampleRate()) {
    return 0;
  }

  const uint32_t bitsPerSample = mHeader.SamplesPerFrame() / 8;
  const uint32_t frameLen =
      bitsPerSample * mHeader.Bitrate() / mHeader.SampleRate() +
      mHeader.Padding() * mHeader.SlotSize();
  return frameLen;
}

bool FrameParser::Frame::ParseNext(uint8_t c) { return mHeader.ParseNext(c); }

const FrameParser::FrameHeader& FrameParser::Frame::Header() const {
  return mHeader;
}

bool FrameParser::ParseVBRHeader(BufferReader* aReader) {
  return mVBRHeader.Parse(aReader, CurrentFrame().Header().SamplesPerFrame());
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
static const uint8_t IDv1[ID_LEN] = {'T', 'A', 'G'};

static const uint8_t MIN_MAJOR_VER = 2;
static const uint8_t MAX_MAJOR_VER = 4;
}  // namespace id3_header

bool ID3Parser::IsBufferStartingWithID3v1Tag(BufferReader* aReader) {
  mozilla::Result<uint32_t, nsresult> res = aReader->PeekU24();
  if (res.isErr()) {
    return false;
  }
  // If buffer starts with ID3v1 tag, `rv` would be reverse and its content
  // should be '3' 'D' 'I' from the lowest bit.
  uint32_t rv = res.unwrap();
  for (int idx = id3_header::ID_LEN - 1; idx >= 0; idx--) {
    if ((rv & 0xff) != id3_header::IDv1[idx]) {
      return false;
    }
    rv = rv >> 8;
  }
  return true;
}

/* static */
bool ID3Parser::IsBufferStartingWithID3Tag(BufferReader* aReader) {
  mozilla::Result<uint32_t, nsresult> res = aReader->PeekU24();
  if (res.isErr()) {
    return false;
  }
  // If buffer starts with ID3v2 tag, `rv` would be reverse and its content
  // should be '3' 'D' 'I' from the lowest bit.
  uint32_t rv = res.unwrap();
  for (int idx = id3_header::ID_LEN - 1; idx >= 0; idx--) {
    if ((rv & 0xff) != id3_header::ID[idx]) {
      return false;
    }
    rv = rv >> 8;
  }
  return true;
}

uint32_t ID3Parser::Parse(BufferReader* aReader) {
  MOZ_ASSERT(aReader);
  MOZ_ASSERT(ID3Parser::IsBufferStartingWithID3Tag(aReader));

  if (!mHeader.HasSizeBeenSet()) {
    return ParseInternal(aReader);
  }

  // Encounter another possible ID3 header, if that is valid then we would use
  // it and save the size of previous one in order to report the size of all ID3
  // headers together in `TotalHeadersSize()`.
  ID3Header prevHeader = mHeader;
  mHeader.Reset();
  uint32_t size = ParseInternal(aReader);
  if (!size) {
    // next ID3 is invalid, so revert the header.
    mHeader = prevHeader;
    return size;
  }

  mFormerID3Size += prevHeader.TotalTagSize();
  return size;
}

uint32_t ID3Parser::ParseInternal(BufferReader* aReader) {
  for (auto res = aReader->ReadU8();
       res.isOk() && !mHeader.ParseNext(res.unwrap());
       res = aReader->ReadU8()) {
  }
  return mHeader.TotalTagSize();
}

void ID3Parser::Reset() {
  mHeader.Reset();
  mFormerID3Size = 0;
}

uint32_t ID3Parser::TotalHeadersSize() const {
  return mHeader.TotalTagSize() + mFormerID3Size;
}

const ID3Parser::ID3Header& ID3Parser::Header() const { return mHeader; }

// ID3Parser::Header

ID3Parser::ID3Header::ID3Header() { Reset(); }

void ID3Parser::ID3Header::Reset() {
  mSize.reset();
  mPos = 0;
}

uint8_t ID3Parser::ID3Header::MajorVersion() const {
  return mRaw[id3_header::ID_END];
}

uint8_t ID3Parser::ID3Header::MinorVersion() const {
  return mRaw[id3_header::ID_END + 1];
}

uint8_t ID3Parser::ID3Header::Flags() const {
  return mRaw[id3_header::FLAGS_END - id3_header::FLAGS_LEN];
}

uint32_t ID3Parser::ID3Header::Size() const {
  if (!IsValid() || !mSize) {
    return 0;
  }
  return *mSize;
}

bool ID3Parser::ID3Header::HasSizeBeenSet() const { return !!mSize; }

uint8_t ID3Parser::ID3Header::FooterSize() const {
  if (Flags() & (1 << 4)) {
    return SIZE;
  }
  return 0;
}

uint32_t ID3Parser::ID3Header::TotalTagSize() const {
  if (IsValid()) {
    // Header found, return total tag size.
    return ID3Header::SIZE + Size() + FooterSize();
  }
  return 0;
}

bool ID3Parser::ID3Header::ParseNext(uint8_t c) {
  if (!Update(c)) {
    Reset();
    if (!Update(c)) {
      Reset();
    }
  }
  return IsValid();
}

bool ID3Parser::ID3Header::IsValid(int aPos) const {
  if (aPos >= SIZE) {
    return true;
  }
  const uint8_t c = mRaw[aPos];
  switch (aPos) {
    case 0:
    case 1:
    case 2:
      // Expecting "ID3".
      return id3_header::ID[aPos] == c;
    case 3:
      return MajorVersion() >= id3_header::MIN_MAJOR_VER &&
             MajorVersion() <= id3_header::MAX_MAJOR_VER;
    case 4:
      return MinorVersion() < 0xFF;
    case 5:
      // Validate flags for supported versions, see bug 949036.
      return ((0xFF >> MajorVersion()) & c) == 0;
    case 6:
    case 7:
    case 8:
    case 9:
      return c < 0x80;
  }
  return true;
}

bool ID3Parser::ID3Header::IsValid() const { return mPos >= SIZE; }

bool ID3Parser::ID3Header::Update(uint8_t c) {
  if (mPos >= id3_header::SIZE_END - id3_header::SIZE_LEN &&
      mPos < id3_header::SIZE_END) {
    uint32_t tmp = mSize.valueOr(0) << 7;
    mSize = Some(tmp | c);
  }
  if (mPos < SIZE) {
    mRaw[mPos] = c;
  }
  return IsValid(mPos++);
}

}  // namespace mozilla
