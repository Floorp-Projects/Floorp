/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FlacDemuxer.h"

#include "mozilla/Maybe.h"
#include "mp4_demuxer/BitReader.h"
#include "nsAutoPtr.h"
#include "prenv.h"
#include "FlacFrameParser.h"
#include "VideoUtils.h"
#include "TimeUnits.h"

#ifdef PR_LOGGING
extern mozilla::LazyLogModule gMediaDemuxerLog;
#define LOG(msg, ...) \
  MOZ_LOG(gMediaDemuxerLog, LogLevel::Debug, ("FlacDemuxer " msg, ##__VA_ARGS__))
#define LOGV(msg, ...) \
  MOZ_LOG(gMediaDemuxerLog, LogLevel::Verbose, ("FlacDemuxer " msg, ##__VA_ARGS__))
#else
#define LOG(msg, ...)  do {} while (false)
#define LOGV(msg, ...) do {} while (false)
#endif

using namespace mozilla::media;

namespace mozilla {
namespace flac {

// flac::FrameHeader - Holds the flac frame header and its parsing
// state.

#define FLAC_MAX_CHANNELS           8
#define FLAC_MIN_BLOCKSIZE         16
#define FLAC_MAX_BLOCKSIZE      65535
#define FLAC_MIN_FRAME_SIZE        11
#define FLAC_MAX_FRAME_HEADER_SIZE 16
#define FLAC_MAX_FRAME_SIZE (FLAC_MAX_FRAME_HEADER_SIZE \
                             +FLAC_MAX_BLOCKSIZE*FLAC_MAX_CHANNELS*3)

class FrameHeader
{
public:
  const AudioInfo& Info() const { return mInfo; }

  uint32_t Size() const { return mSize; }

  bool IsValid() const { return mValid; }

  // Return the index (in samples) from the beginning of the track.
  int64_t Index() const { return mIndex; }

  // Parse the current packet and check that it made a valid flac frame header.
  // From https://xiph.org/flac/format.html#frame_header
  // A valid header is one that can be decoded without error and that has a
  // valid CRC.
  // aPacket must points to a buffer that is at least FLAC_MAX_FRAME_HEADER_SIZE
  // bytes.
  bool Parse(const uint8_t* aPacket)
  {
    mp4_demuxer::BitReader br(aPacket, FLAC_MAX_FRAME_HEADER_SIZE * 8);

    // Frame sync code.
    if ((br.ReadBits(15) & 0x7fff) != 0x7ffc) {
      return false;
    }

    // Variable block size stream code.
    mVariableBlockSize = br.ReadBit();

    // Block size and sample rate codes.
    int bs_code = br.ReadBits(4);
    int sr_code = br.ReadBits(4);

    // Channels and decorrelation.
    int ch_mode = br.ReadBits(4);
    if (ch_mode < FLAC_MAX_CHANNELS) {
      mInfo.mChannels = ch_mode + 1;
    } else if (ch_mode < FLAC_MAX_CHANNELS + FLAC_CHMODE_MID_SIDE) {
      // This is a special flac channels, we can't handle those yet. Treat it
      // as stereo.
      mInfo.mChannels = 2;
    } else {
      // invalid channel mode
      return false;
    }

    // Bits per sample.
    int bps_code = br.ReadBits(3);
    if (bps_code == 3 || bps_code == 7) {
      // Invalid sample size code.
      return false;
    }
    mInfo.mBitDepth = FlacSampleSizeTable[bps_code];

    // Reserved bit, most be 1.
    if (br.ReadBit()) {
      // Broken stream, invalid padding.
      return false;
    }

    // Sample or frame count.
    int64_t frame_or_sample_num = br.ReadUTF8();
    if (frame_or_sample_num < 0) {
      // Sample/frame number invalid.
      return false;
    }

    // Blocksize
    if (bs_code == 0) {
      // reserved blocksize code
      return false;
    } else if (bs_code == 6) {
      mBlocksize = br.ReadBits(8) + 1;
    } else if (bs_code == 7) {
      mBlocksize = br.ReadBits(16) + 1;
    } else {
      mBlocksize = FlacBlocksizeTable[bs_code];
    }

    // The sample index is either:
    // 1- coded sample number if blocksize is variable or
    // 2- coded frame number if blocksize is known.
    // A frame is made of Blocksize sample.
    mIndex = mVariableBlockSize ? frame_or_sample_num
                                : frame_or_sample_num * mBlocksize;

    // Sample rate.
    if (sr_code < 12) {
      mInfo.mRate = FlacSampleRateTable[sr_code];
    } else if (sr_code == 12) {
      mInfo.mRate = br.ReadBits(8) * 1000;
    } else if (sr_code == 13) {
      mInfo.mRate = br.ReadBits(16);
    } else if (sr_code == 14) {
      mInfo.mRate = br.ReadBits(16) * 10;
    } else {
      // Illegal sample rate code.
      return false;
    }

    // Header CRC-8 check.
    uint8_t crc = 0;
    for (uint32_t i = 0; i < br.BitCount() / 8; i++) {
      crc = CRC8Table[crc ^ aPacket[i]];
    }
    mValid = crc == br.ReadBits(8);
    mSize = br.BitCount() / 8;

    if (mValid) {
      // Set the mimetype to make it a valid AudioInfo.
      mInfo.mMimeType = "audio/flac";
    }

    return mValid;
  }

private:
  friend class Frame;
  enum
  {
    FLAC_CHMODE_INDEPENDENT = 0,
    FLAC_CHMODE_LEFT_SIDE,
    FLAC_CHMODE_RIGHT_SIDE,
    FLAC_CHMODE_MID_SIDE,
  };
  AudioInfo mInfo;
  // Index in samples from start;
  int64_t mIndex = 0;
  bool mVariableBlockSize = false;
  uint32_t mBlocksize = 0;;
  uint32_t mSize = 0;
  bool mValid = false;

  static const int FlacSampleRateTable[16];
  static const int32_t FlacBlocksizeTable[16];
  static const uint8_t FlacSampleSizeTable[8];
  static const uint8_t CRC8Table[256];
};

const int FrameHeader::FlacSampleRateTable[16] =
{
  0,
  88200, 176400, 192000,
  8000, 16000, 22050, 24000, 32000, 44100, 48000, 96000,
  0, 0, 0, 0
};

const int32_t FrameHeader::FlacBlocksizeTable[16] =
{
  0     , 192   , 576<<0, 576<<1, 576<<2, 576<<3,      0,      0,
  256<<0, 256<<1, 256<<2, 256<<3, 256<<4, 256<<5, 256<<6, 256<<7
};

const uint8_t FrameHeader::FlacSampleSizeTable[8] = { 0, 8, 12, 0, 16, 20, 24, 0 };

const uint8_t FrameHeader::CRC8Table[256] =
{
  0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
  0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
  0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
  0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
  0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
  0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
  0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
  0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
  0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
  0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
  0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
  0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
  0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
  0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
  0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
  0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
  0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
  0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
  0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
  0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
  0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
  0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
  0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
  0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
  0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
  0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
  0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
  0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
  0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
  0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
  0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
  0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

// flac::Frame - Frame meta container used to parse and hold a frame
// header and side info.
class Frame
{
public:

  // The FLAC signature is made of 14 bits set to 1; however the 15th bit is
  // mandatorily set to 0, so we need to find either of 0xfffc or 0xfffd 2-bytes
  // signature. We first use a bitmask to see if 0xfc or 0xfd is present. And if
  // so we check for the whole signature.
  // aData must be pointing to a buffer at least
  // aLength + FLAC_MAX_FRAME_HEADER_SIZE bytes.
  int64_t FindNext(const uint8_t* aData, const uint32_t aLength)
  {
    uint32_t modOffset = aLength % 4;
    uint32_t i, j;

    for (i = 0; i < modOffset; i++) {
      if ((BigEndian::readUint16(aData + i) & 0xfffe) == 0xfff8) {
        if (mHeader.Parse(aData + i)) {
          return i;
        }
      }
    }

    for (; i < aLength; i += 4) {
      uint32_t x = BigEndian::readUint32(aData + i);
      if (((x & ~(x + 0x01010101)) & 0x80808080)) {
        for (j = 0; j < 4; j++) {
          if ((BigEndian::readUint16(aData + i + j) & 0xfffe) == 0xfff8) {
            if (mHeader.Parse(aData + i + j)) {
              return i + j;
            }
          }
        }
      }
    }
    return -1;
  }

  // Find the next frame start in the current resource.
  // On exit return true, offset is set and resource points to the frame found.
  bool FindNext(MediaResourceIndex& aResource)
  {
    static const int BUFFER_SIZE = 4096;

    Reset();

    nsTArray<char> buffer;
    int64_t originalOffset = aResource.Tell();
    int64_t offset = originalOffset;
    uint32_t innerOffset = 0;

    do {
      uint32_t read = 0;
      buffer.SetLength(BUFFER_SIZE + innerOffset);
      nsresult rv =
        aResource.Read(buffer.Elements() + innerOffset, BUFFER_SIZE, &read);
      if (NS_FAILED(rv)) {
        return false;
      }

      if (read < FLAC_MAX_FRAME_HEADER_SIZE) {
        // Assume that we can't have a valid frame in such small content, we
        // must have reached EOS.
        // So we're done.
        mEOS = true;
        return false;
      }

      const size_t bufSize = read + innerOffset - FLAC_MAX_FRAME_HEADER_SIZE;
      int64_t foundOffset =
        FindNext(reinterpret_cast<uint8_t*>(buffer.Elements()), bufSize);

      if (foundOffset >= 0) {
        SetOffset(aResource, foundOffset + offset);
        return true;
      }

      // Scan the next block;
      offset += bufSize;
      buffer.RemoveElementsAt(0, bufSize);
      innerOffset = buffer.Length();
    } while (offset - originalOffset < FLAC_MAX_FRAME_SIZE);

    return false;
  }

  int64_t Offset() const { return mOffset; }

  const AudioInfo& Info() const { return Header().Info(); }

  void SetEndOffset(int64_t aOffset) { mSize = aOffset - mOffset; }

  void SetEndTime(int64_t aIndex)
  {
    if (aIndex > Header().mIndex) {
      mDuration = aIndex - Header().mIndex;
    }
  }

  uint32_t Size() const { return mSize; }

  TimeUnit Time() const
  {
    if (!IsValid()) {
      return TimeUnit::Invalid();
    }
    MOZ_ASSERT(Header().Info().mRate, "Invalid Frame. Need Header");
    return FramesToTimeUnit(Header().mIndex, Header().Info().mRate);
  }

  TimeUnit Duration() const
  {
    if (!IsValid()) {
      return TimeUnit();
    }
    MOZ_ASSERT(Header().Info().mRate, "Invalid Frame. Need Header");
    return FramesToTimeUnit(mDuration, Header().Info().mRate);
  }

  // Returns the parsed frame header.
  const FrameHeader& Header() const { return mHeader; }

  bool IsValid() const { return mHeader.IsValid(); }

  bool EOS() const { return mEOS; }

  void SetRate(uint32_t aRate) { mHeader.mInfo.mRate = aRate; };

  void SetBitDepth(uint32_t aBitDepth) { mHeader.mInfo.mBitDepth = aBitDepth; }

  void SetInvalid() { mHeader.mValid = false; }

  // Resets the frame header and data.
  void Reset() { *this = Frame(); }

private:
  void SetOffset(MediaResourceIndex& aResource, int64_t aOffset)
  {
    mOffset = aOffset;
    aResource.Seek(SEEK_SET, mOffset);
  }

  // The offset to the start of the header.
  int64_t mOffset = 0;
  uint32_t mSize = 0;
  uint32_t mDuration = 0;
  bool mEOS = false;

  // The currently parsed frame header.
  FrameHeader mHeader;

};

class FrameParser
{
public:

  // Returns the currently parsed frame. Reset via EndFrameSession.
  const Frame& CurrentFrame() const { return mFrame; }

  // Returns the first parsed frame.
  const Frame& FirstFrame() const { return mFirstFrame; }

  // Clear the last parsed frame to allow for next frame parsing
  void EndFrameSession()
  {
    mNextFrame.Reset();
    mFrame.Reset();
  }

  // Attempt to find the next frame.
  bool FindNextFrame(MediaResourceIndex& aResource)
  {
    mFrame = mNextFrame;
    if (GetNextFrame(aResource)) {
      if (!mFrame.IsValid()) {
        mFrame = mNextFrame;
        // We need two frames to be able to start playing (or have reached EOS).
        GetNextFrame(aResource);
      }
    }

    if (mFrame.IsValid()) {
      if (mNextFrame.EOS()) {
        mFrame.SetEndOffset(aResource.Tell());
      } else if (mNextFrame.IsValid()) {
        mFrame.SetEndOffset(mNextFrame.Offset());
        mFrame.SetEndTime(mNextFrame.Header().Index());
      }
    }

    if (!mFirstFrame.IsValid()) {
      mFirstFrame = mFrame;
    }
    return mFrame.IsValid();
  }

  // Convenience methods to external FlacFrameParser ones.
  bool IsHeaderBlock(const uint8_t* aPacket, size_t aLength) const
  {
    return mParser.IsHeaderBlock(aPacket, aLength);
  }

  uint32_t HeaderBlockLength(const uint8_t* aPacket) const
  {
    return mParser.HeaderBlockLength(aPacket);
  }

  bool DecodeHeaderBlock(const uint8_t* aPacket, size_t aLength)
  {
    return mParser.DecodeHeaderBlock(aPacket, aLength);
  }

  bool HasFullMetadata() const { return mParser.HasFullMetadata(); }

  AudioInfo Info() const { return mParser.mInfo; }

  // Return a hash table with tag metadata.
  MetadataTags* GetTags() const { return mParser.GetTags(); }

private:
  bool GetNextFrame(MediaResourceIndex& aResource)
  {
    while (mNextFrame.FindNext(aResource)) {
      // Move our offset slightly, so that we don't find the same frame at the
      // next FindNext call.
      aResource.Seek(SEEK_CUR, mNextFrame.Header().Size());
      if (mFrame.IsValid()
          && mNextFrame.Offset() - mFrame.Offset() < FLAC_MAX_FRAME_SIZE
          && !CheckCRC16AtOffset(mFrame.Offset(),
                                 mNextFrame.Offset(),
                                 aResource)) {
        // The frame doesn't match its CRC or would be too far, skip it..
        continue;
      }
      CheckFrameData();
      break;
    }
    return mNextFrame.IsValid();
  }

  bool CheckFrameData()
  {
    if (mNextFrame.Header().Info().mRate == 0
        || mNextFrame.Header().Info().mBitDepth == 0) {
      if (!Info().IsValid()) {
        // We can only use the STREAMINFO data if we have one.
        mNextFrame.SetInvalid();
      } else {
        if (mNextFrame.Header().Info().mRate == 0) {
          mNextFrame.SetRate(Info().mRate);
        }
        if (mNextFrame.Header().Info().mBitDepth == 0) {
          mNextFrame.SetBitDepth(Info().mBitDepth);
        }
      }
    }
    return mNextFrame.IsValid();
  }

  bool CheckCRC16AtOffset(int64_t aStart, int64_t aEnd,
                          MediaResourceIndex& aResource) const
  {
    int64_t size = aEnd - aStart;
    if (size <= 0) {
      return false;
    }
    UniquePtr<char[]> buffer(new char[size]);
    uint32_t read = 0;
    if (NS_FAILED(aResource.ReadAt(aStart, buffer.get(),
                                   size, &read))
        || read != size) {
      NS_WARNING("Couldn't read frame content");
      return false;
    }

    uint16_t crc = 0;
    uint8_t* buf = reinterpret_cast<uint8_t*>(buffer.get());
    const uint8_t *end = buf + size;
    while (buf < end) {
      crc = CRC16Table[((uint8_t)crc) ^ *buf++] ^ (crc >> 8);
    }
    return !crc;
  }

  const uint16_t CRC16Table[256] =
  {
    0x0000, 0x0580, 0x0F80, 0x0A00, 0x1B80, 0x1E00, 0x1400, 0x1180,
    0x3380, 0x3600, 0x3C00, 0x3980, 0x2800, 0x2D80, 0x2780, 0x2200,
    0x6380, 0x6600, 0x6C00, 0x6980, 0x7800, 0x7D80, 0x7780, 0x7200,
    0x5000, 0x5580, 0x5F80, 0x5A00, 0x4B80, 0x4E00, 0x4400, 0x4180,
    0xC380, 0xC600, 0xCC00, 0xC980, 0xD800, 0xDD80, 0xD780, 0xD200,
    0xF000, 0xF580, 0xFF80, 0xFA00, 0xEB80, 0xEE00, 0xE400, 0xE180,
    0xA000, 0xA580, 0xAF80, 0xAA00, 0xBB80, 0xBE00, 0xB400, 0xB180,
    0x9380, 0x9600, 0x9C00, 0x9980, 0x8800, 0x8D80, 0x8780, 0x8200,
    0x8381, 0x8601, 0x8C01, 0x8981, 0x9801, 0x9D81, 0x9781, 0x9201,
    0xB001, 0xB581, 0xBF81, 0xBA01, 0xAB81, 0xAE01, 0xA401, 0xA181,
    0xE001, 0xE581, 0xEF81, 0xEA01, 0xFB81, 0xFE01, 0xF401, 0xF181,
    0xD381, 0xD601, 0xDC01, 0xD981, 0xC801, 0xCD81, 0xC781, 0xC201,
    0x4001, 0x4581, 0x4F81, 0x4A01, 0x5B81, 0x5E01, 0x5401, 0x5181,
    0x7381, 0x7601, 0x7C01, 0x7981, 0x6801, 0x6D81, 0x6781, 0x6201,
    0x2381, 0x2601, 0x2C01, 0x2981, 0x3801, 0x3D81, 0x3781, 0x3201,
    0x1001, 0x1581, 0x1F81, 0x1A01, 0x0B81, 0x0E01, 0x0401, 0x0181,
    0x0383, 0x0603, 0x0C03, 0x0983, 0x1803, 0x1D83, 0x1783, 0x1203,
    0x3003, 0x3583, 0x3F83, 0x3A03, 0x2B83, 0x2E03, 0x2403, 0x2183,
    0x6003, 0x6583, 0x6F83, 0x6A03, 0x7B83, 0x7E03, 0x7403, 0x7183,
    0x5383, 0x5603, 0x5C03, 0x5983, 0x4803, 0x4D83, 0x4783, 0x4203,
    0xC003, 0xC583, 0xCF83, 0xCA03, 0xDB83, 0xDE03, 0xD403, 0xD183,
    0xF383, 0xF603, 0xFC03, 0xF983, 0xE803, 0xED83, 0xE783, 0xE203,
    0xA383, 0xA603, 0xAC03, 0xA983, 0xB803, 0xBD83, 0xB783, 0xB203,
    0x9003, 0x9583, 0x9F83, 0x9A03, 0x8B83, 0x8E03, 0x8403, 0x8183,
    0x8002, 0x8582, 0x8F82, 0x8A02, 0x9B82, 0x9E02, 0x9402, 0x9182,
    0xB382, 0xB602, 0xBC02, 0xB982, 0xA802, 0xAD82, 0xA782, 0xA202,
    0xE382, 0xE602, 0xEC02, 0xE982, 0xF802, 0xFD82, 0xF782, 0xF202,
    0xD002, 0xD582, 0xDF82, 0xDA02, 0xCB82, 0xCE02, 0xC402, 0xC182,
    0x4382, 0x4602, 0x4C02, 0x4982, 0x5802, 0x5D82, 0x5782, 0x5202,
    0x7002, 0x7582, 0x7F82, 0x7A02, 0x6B82, 0x6E02, 0x6402, 0x6182,
    0x2002, 0x2582, 0x2F82, 0x2A02, 0x3B82, 0x3E02, 0x3402, 0x3182,
    0x1382, 0x1602, 0x1C02, 0x1982, 0x0802, 0x0D82, 0x0782, 0x0202,
  };

  FlacFrameParser mParser;
  // We keep the first parsed frame around for static info access
  // and the currently parsed frame.
  Frame mFirstFrame;
  Frame mNextFrame;
  Frame mFrame;
};

} // namespace flac

// FlacDemuxer

FlacDemuxer::FlacDemuxer(MediaResource* aSource) : mSource(aSource) { }

bool
FlacDemuxer::InitInternal()
{
  if (!mTrackDemuxer) {
    mTrackDemuxer = new FlacTrackDemuxer(mSource);
  }
  return mTrackDemuxer->Init();
}

RefPtr<FlacDemuxer::InitPromise>
FlacDemuxer::Init()
{
  if (!InitInternal()) {
    LOG("Init() failure: waiting for data");

    return InitPromise::CreateAndReject(
      NS_ERROR_DOM_MEDIA_DEMUXER_ERR, __func__);
  }

  LOG("Init() successful");
  return InitPromise::CreateAndResolve(NS_OK, __func__);
}

bool
FlacDemuxer::HasTrackType(TrackInfo::TrackType aType) const
{
  return aType == TrackInfo::kAudioTrack;
}

uint32_t
FlacDemuxer::GetNumberTracks(TrackInfo::TrackType aType) const
{
  return (aType == TrackInfo::kAudioTrack) ? 1 : 0;
}

already_AddRefed<MediaTrackDemuxer>
FlacDemuxer::GetTrackDemuxer(TrackInfo::TrackType aType, uint32_t aTrackNumber)
{
  if (!mTrackDemuxer) {
    return nullptr;
  }

  return RefPtr<FlacTrackDemuxer>(mTrackDemuxer).forget();
}

bool
FlacDemuxer::IsSeekable() const
{
  return mTrackDemuxer && mTrackDemuxer->IsSeekable();
}

// FlacTrackDemuxer
FlacTrackDemuxer::FlacTrackDemuxer(MediaResource* aSource)
  : mSource(aSource)
  , mParser(new flac::FrameParser())
  , mTotalFrameLen(0)
{
  Reset();
}

FlacTrackDemuxer::~FlacTrackDemuxer()
{
}

bool
FlacTrackDemuxer::Init()
{
  static const int BUFFER_SIZE = 4096;

  // First check if we have a valid Flac start.
  char buffer[BUFFER_SIZE];
  const uint8_t* ubuffer = // only needed due to type constraints of ReadAt.
    reinterpret_cast<uint8_t*>(buffer);
  int64_t offset = 0;

  do {
    uint32_t read = 0;
    nsresult ret = mSource.ReadAt(offset, buffer, BUFFER_SIZE, &read);
    if (NS_FAILED(ret) || read < BUFFER_SIZE) {
      // Assume that if we can't read that many bytes while parsing the header,
      // that something is wrong.
      return false;
    }
    if (!mParser->IsHeaderBlock(ubuffer, BUFFER_SIZE)) {
      // Not a header and we haven't reached the end of the metadata blocks.
      // Will fall back to using the frames header instead.
      break;
    }
    uint32_t sizeHeader = mParser->HeaderBlockLength(ubuffer);
    RefPtr<MediaByteBuffer> block =
      mSource.MediaReadAt(offset, sizeHeader);
    if (!block || block->Length() != sizeHeader) {
      break;
    }
    if (!mParser->DecodeHeaderBlock(block->Elements(), sizeHeader)) {
      break;
    }
    offset += sizeHeader;
  } while (!mParser->HasFullMetadata());

  // First flac frame is found after the metadata.
  // Can seek there immediately to avoid reparsing it all.
  mSource.Seek(SEEK_SET, offset);

  // Find the first frame to fully initialise our parser.
  if (mParser->FindNextFrame(mSource)) {
    // Ensure that the next frame returned will be the first.
    mSource.Seek(SEEK_SET, mParser->FirstFrame().Offset());
    mParser->EndFrameSession();
  } else if (!mParser->Info().IsValid() || !mParser->FirstFrame().IsValid()) {
    // We must find at least a frame to determine the metadata.
    // We can't play this stream.
    return false;
  }

  if (!mParser->Info().IsValid() || !mParser->Info().mDuration) {
    // Check if we can look at the last frame for the end time to determine the
    // duration when we don't have any.
    TimeAtEnd();
  }

  return true;
}

UniquePtr<TrackInfo>
FlacTrackDemuxer::GetInfo() const
{
  if (mParser->Info().IsValid()) {
    // We have a proper metadata header.
    UniquePtr<TrackInfo> info = mParser->Info().Clone();
    nsAutoPtr<MetadataTags> tags(mParser->GetTags());
    if (tags) {
      for (auto iter = tags->Iter(); !iter.Done(); iter.Next()) {
        info->mTags.AppendElement(MetadataTag(iter.Key(), iter.Data()));
      }
    }
    return info;
  } else if (mParser->FirstFrame().Info().IsValid()) {
    // Use the first frame header.
    UniquePtr<TrackInfo> info = mParser->FirstFrame().Info().Clone();
    info->mDuration = Duration().ToMicroseconds();
    return info;
  }
  return nullptr;
}

bool
FlacTrackDemuxer::IsSeekable() const
{
  // For now we only allow seeking if a STREAMINFO block was found and with
  // a known number of samples (duration is set).
  return mParser->Info().IsValid() && mParser->Info().mDuration;
}

RefPtr<FlacTrackDemuxer::SeekPromise>
FlacTrackDemuxer::Seek(const TimeUnit& aTime)
{
  // Efficiently seek to the position.
  FastSeek(aTime);
  // Correct seek position by scanning the next frames.
  const TimeUnit seekTime = ScanUntil(aTime);

  return SeekPromise::CreateAndResolve(seekTime, __func__);
}

TimeUnit
FlacTrackDemuxer::FastSeek(const TimeUnit& aTime)
{
  LOG("FastSeek(%f) avgFrameLen=%f mParsedFramesDuration=%f offset=%lld",
      aTime.ToSeconds(), AverageFrameLength(),
      mParsedFramesDuration.ToSeconds(), GetResourceOffset());

  // Invalidate current frames in the parser.
  mParser->EndFrameSession();

  if (!mParser->FirstFrame().IsValid()) {
    // Something wrong, and there's nothing to seek to anyway, so we can
    // do whatever here.
    mSource.Seek(SEEK_SET, 0);
    return TimeUnit();
  }

  if (aTime <= mParser->FirstFrame().Time()) {
    // We're attempting to seek prior the first frame, return the first frame.
    mSource.Seek(SEEK_SET, mParser->FirstFrame().Offset());
    return mParser->FirstFrame().Time();
  }

  // We look for the seek position using a bisection search, starting where the
  // estimated position might be using the average frame length.
  // Typically, with flac such approximation is typically useless.

  // Estimate where the position might be.
  int64_t pivot =
    aTime.ToSeconds() * AverageFrameLength() + mParser->FirstFrame().Offset();

  // Time in seconds where we can stop seeking and will continue using
  // ScanUntil.
  static const int GAP_THRESHOLD = 5;
  int64_t first = mParser->FirstFrame().Offset();
  int64_t last = mSource.GetLength();
  Maybe<int64_t> lastFoundOffset;
  uint32_t iterations = 0;
  TimeUnit timeSeekedTo;

  do {
    iterations++;
    mSource.Seek(SEEK_SET, pivot);
    flac::Frame frame;
    if (!frame.FindNext(mSource)) {
      NS_WARNING("We should have found a point");
      break;
    }
    timeSeekedTo = frame.Time();

    LOGV("FastSeek: interation:%u found:%f @ %lld",
         iterations, timeSeekedTo.ToSeconds(), frame.Offset());

    if (lastFoundOffset && lastFoundOffset.ref() == frame.Offset()) {
      // Same frame found twice. We're done.
      break;
    }
    lastFoundOffset = Some(frame.Offset());

    if (frame.Time() == aTime) {
      break;
    }
    if (aTime > frame.Time()
        && aTime - frame.Time() <= TimeUnit::FromSeconds(GAP_THRESHOLD)) {
      // We're close enough to the target, experimentation shows that bisection
      // search doesn't help much after that.
      break;
    }
    if (frame.Time() > aTime) {
      last = pivot;
      pivot -= (pivot - first) / 2;
    } else {
      first = pivot;
      pivot += (last - pivot) / 2;
    }
  } while (true);

  if (lastFoundOffset) {
    mSource.Seek(SEEK_SET, lastFoundOffset.ref());
  }

  return timeSeekedTo;
}

TimeUnit
FlacTrackDemuxer::ScanUntil(const TimeUnit& aTime)
{
  LOG("ScanUntil(%f avgFrameLen=%f mParsedFramesDuration=%f offset=%lld",
      aTime.ToSeconds(), AverageFrameLength(),
      mParsedFramesDuration.ToSeconds(), mParser->CurrentFrame().Offset());

   if (!mParser->FirstFrame().IsValid()
       || aTime <= mParser->FirstFrame().Time()) {
     return FastSeek(aTime);
   }

  int64_t previousOffset = 0;
  TimeUnit previousTime;
  while (FindNextFrame().IsValid() && mParser->CurrentFrame().Time() < aTime) {
    previousOffset = mParser->CurrentFrame().Offset();
    previousTime = mParser->CurrentFrame().Time();
  }

  if (!mParser->CurrentFrame().IsValid()) {
    // We reached EOS.
    return Duration();
  }

  // Seek back to the last frame found prior the target.
  mParser->EndFrameSession();
  mSource.Seek(SEEK_SET, previousOffset);
  return previousTime;
}

RefPtr<FlacTrackDemuxer::SamplesPromise>
FlacTrackDemuxer::GetSamples(int32_t aNumSamples)
{
  LOGV("GetSamples(%d) Begin offset=%lld mParsedFramesDuration=%f"
       " mTotalFrameLen=%llu",
       aNumSamples, GetResourceOffset(), mParsedFramesDuration.ToSeconds(),
       mTotalFrameLen);

  if (!aNumSamples) {
    return SamplesPromise::CreateAndReject(
      NS_ERROR_DOM_MEDIA_DEMUXER_ERR, __func__);
  }

  RefPtr<SamplesHolder> frames = new SamplesHolder();

  while (aNumSamples--) {
    RefPtr<MediaRawData> frame(GetNextFrame(FindNextFrame()));
    if (!frame)
      break;

    frames->mSamples.AppendElement(frame);
  }

  LOGV("GetSamples() End mSamples.Length=%u aNumSamples=%d offset=%lld"
       " mParsedFramesDuration=%f mTotalFrameLen=%llu",
       frames->mSamples.Length(), aNumSamples, GetResourceOffset(),
       mParsedFramesDuration.ToSeconds(), mTotalFrameLen);

  if (frames->mSamples.IsEmpty()) {
    return SamplesPromise::CreateAndReject(
      NS_ERROR_DOM_MEDIA_END_OF_STREAM, __func__);
  }

  return SamplesPromise::CreateAndResolve(frames, __func__);
}

void
FlacTrackDemuxer::Reset()
{
  LOG("Reset()");
  MOZ_ASSERT(mParser);
  if (mParser->FirstFrame().IsValid()) {
    mSource.Seek(SEEK_SET, mParser->FirstFrame().Offset());
  } else {
    mSource.Seek(SEEK_SET, 0);
  }
  mParser->EndFrameSession();
}

RefPtr<FlacTrackDemuxer::SkipAccessPointPromise>
FlacTrackDemuxer::SkipToNextRandomAccessPoint(const TimeUnit& aTimeThreshold)
{
  // Will not be called for audio-only resources.
  return SkipAccessPointPromise::CreateAndReject(
    SkipFailureHolder(NS_ERROR_DOM_MEDIA_DEMUXER_ERR, 0), __func__);
}

int64_t
FlacTrackDemuxer::GetResourceOffset() const
{
  return mSource.Tell();
}

TimeIntervals
FlacTrackDemuxer::GetBuffered()
{
  TimeUnit duration = Duration();

  if (duration <= TimeUnit()) {
    return TimeIntervals();
  }

  // We could simply parse the cached data instead and read the timestamps.
  // However, for now this will do.
  AutoPinned<MediaResource> stream(mSource.GetResource());
  return GetEstimatedBufferedTimeRanges(stream, duration.ToMicroseconds());
}

const flac::Frame&
FlacTrackDemuxer::FindNextFrame()
{
  LOGV("FindNext() Begin offset=%lld mParsedFramesDuration=%f"
       " mTotalFrameLen=%llu",
       GetResourceOffset(), mParsedFramesDuration.ToSeconds(), mTotalFrameLen);

  if (mParser->FindNextFrame(mSource)) {
    // Update our current progress stats.
    mParsedFramesDuration =
      std::max(mParsedFramesDuration,
               mParser->CurrentFrame().Time() - mParser->FirstFrame().Time()
               + mParser->CurrentFrame().Duration());
    mTotalFrameLen =
      std::max<uint64_t>(mTotalFrameLen,
                         mParser->CurrentFrame().Offset()
                         - mParser->FirstFrame().Offset()
                         + mParser->CurrentFrame().Size());

    LOGV("FindNext() End time=%f offset=%lld mParsedFramesDuration=%f"
         " mTotalFrameLen=%llu",
         mParser->CurrentFrame().Time().ToSeconds(), GetResourceOffset(),
         mParsedFramesDuration.ToSeconds(), mTotalFrameLen);
  }

  return mParser->CurrentFrame();
}

already_AddRefed<MediaRawData>
FlacTrackDemuxer::GetNextFrame(const flac::Frame& aFrame)
{
  if (!aFrame.IsValid()) {
    LOG("GetNextFrame() EOS");
    return nullptr;
  }

  LOG("GetNextFrame() Begin(time=%f offset=%lld size=%u)",
      aFrame.Time().ToSeconds(), aFrame.Offset(), aFrame.Size());

  const int64_t offset = aFrame.Offset();
  const uint32_t size = aFrame.Size();

  RefPtr<MediaRawData> frame = new MediaRawData();
  frame->mOffset = offset;

  nsAutoPtr<MediaRawDataWriter> frameWriter(frame->CreateWriter());
  if (!frameWriter->SetSize(size)) {
    LOG("GetNext() Exit failed to allocated media buffer");
    return nullptr;
  }

  const uint32_t read = Read(frameWriter->Data(), offset, size);
  if (read != size) {
    LOG("GetNextFrame() Exit read=%u frame->Size=%u", read, frame->Size());
    return nullptr;
  }

  frame->mTime = aFrame.Time().ToMicroseconds();
  frame->mDuration = aFrame.Duration().ToMicroseconds();
  frame->mTimecode = frame->mTime;
  frame->mOffset = aFrame.Offset();
  frame->mKeyframe = true;

  MOZ_ASSERT(frame->mTime >= 0);
  MOZ_ASSERT(frame->mDuration >= 0);

  return frame.forget();
}

int32_t
FlacTrackDemuxer::Read(uint8_t* aBuffer, int64_t aOffset, int32_t aSize)
{
  uint32_t read = 0;
  const nsresult rv = mSource.ReadAt(aOffset, reinterpret_cast<char*>(aBuffer),
                                     static_cast<uint32_t>(aSize), &read);
  NS_ENSURE_SUCCESS(rv, 0);
  return static_cast<int32_t>(read);
}

double
FlacTrackDemuxer::AverageFrameLength() const
{
  if (mParsedFramesDuration.ToMicroseconds()) {
    return mTotalFrameLen / mParsedFramesDuration.ToSeconds();
  }

  return 0.0;
}

TimeUnit
FlacTrackDemuxer::Duration() const
{
  return std::max(mParsedFramesDuration,
                  TimeUnit::FromMicroseconds(mParser->Info().mDuration));
}

TimeUnit
FlacTrackDemuxer::TimeAtEnd()
{
  // Scan the last 128kB if available to determine the last frame.
  static const int OFFSET_FROM_END = 128 * 1024;

  // Seek to the end of the file and attempt to find the last frame.
  MediaResourceIndex source(mSource.GetResource());
  TimeUnit previousDuration;
  TimeUnit previousTime;

  const int64_t streamLen = mSource.GetLength();
  if (streamLen < 0) {
    return TimeUnit::FromInfinity();
  }

  flac::FrameParser parser;

  source.Seek(SEEK_SET, std::max<int64_t>(0LL, streamLen - OFFSET_FROM_END));
  while (parser.FindNextFrame(source)) {
    // FFmpeg flac muxer can generate a last frame with earlier than the others.
    previousTime = std::max(previousTime, parser.CurrentFrame().Time());
    if (parser.CurrentFrame().Duration() > TimeUnit()) {
      // The last frame doesn't have a duration, so only update our duration
      // if we do have one.
      previousDuration = parser.CurrentFrame().Duration();
    }
    if (source.Tell() >= streamLen) {
      // Limit the read, in case the length change half-way.
      break;
    }
  }

  // Update our current progress stats.
  mParsedFramesDuration =
    previousTime + previousDuration - mParser->FirstFrame().Time();
  mTotalFrameLen = streamLen - mParser->FirstFrame().Offset();

  return mParsedFramesDuration;
}

/* static */ bool
FlacDemuxer::FlacSniffer(const uint8_t* aData, const uint32_t aLength)
{
  if (aLength < FLAC_MAX_FRAME_HEADER_SIZE) {
    return false;
  }

  flac::Frame frame;
  return frame.FindNext(aData, aLength - FLAC_MAX_FRAME_HEADER_SIZE) >= 0;
}

} // namespace mozilla
