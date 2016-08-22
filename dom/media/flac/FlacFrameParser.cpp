/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FlacFrameParser.h"
#include "mp4_demuxer/ByteReader.h"
#include "nsTArray.h"
#include "OggCodecState.h"
#include "VideoUtils.h"

namespace mozilla
{

class AutoByteReader : public mp4_demuxer::ByteReader
{
public:
  AutoByteReader(const uint8_t* aData, size_t aSize)
    : mp4_demuxer::ByteReader(aData, aSize)
  {

  }
  virtual ~AutoByteReader()
  {
    DiscardRemaining();
  }
};

#define OGG_FLAC_METADATA_TYPE_STREAMINFO 0x7F
#define FLAC_STREAMINFO_SIZE   34

#define BITMASK(x) ((1ULL << x)-1)

enum
{
  FLAC_METADATA_TYPE_STREAMINFO = 0,
  FLAC_METADATA_TYPE_PADDING,
  FLAC_METADATA_TYPE_APPLICATION,
  FLAC_METADATA_TYPE_SEEKTABLE,
  FLAC_METADATA_TYPE_VORBIS_COMMENT,
  FLAC_METADATA_TYPE_CUESHEET,
  FLAC_METADATA_TYPE_PICTURE,
  FLAC_METADATA_TYPE_INVALID = 127
};

FlacFrameParser::FlacFrameParser()
  : mMinBlockSize(0)
  , mMaxBlockSize(0)
  , mMinFrameSize(0)
  , mMaxFrameSize(0)
  , mNumFrames(0)
  , mFullMetadata(false)
  , mPacketCount(0)
{
}

uint32_t
FlacFrameParser::HeaderBlockLength(const uint8_t* aPacket) const
{
  uint32_t extra = 4;
  if (aPacket[0] == 'f') {
    // This must be the first block read, which contains the fLaC signature.
    aPacket += 4;
    extra += 4;
  }
  return (BigEndian::readUint32(aPacket) & BITMASK(24)) + extra;
}

bool
FlacFrameParser::DecodeHeaderBlock(const uint8_t* aPacket, size_t aLength)
{
  if (aLength < 4 || aPacket[0] == 0xff) {
    // Not a header block.
    return false;
  }
  AutoByteReader br(aPacket, aLength);

  mPacketCount++;

  if (aPacket[0] == 'f') {
    if (mPacketCount != 1 || memcmp(br.Read(4), "fLaC", 4) ||
        br.Remaining() != FLAC_STREAMINFO_SIZE + 4) {
      return false;
    }
    aPacket += 4;
    aLength -= 4;
  }
  uint8_t blockHeader = br.ReadU8();
  // blockType is a misnomer as it could indicate here either a packet type
  // should it points to the start of a Flac in Ogg metadata, or an actual
  // block type as per the flac specification.
  uint32_t blockType = blockHeader & 0x7f;
  bool lastBlock = blockHeader & 0x80;

  if (blockType == OGG_FLAC_METADATA_TYPE_STREAMINFO) {
    if (mPacketCount != 1 || memcmp(br.Read(4), "FLAC", 4) ||
        br.Remaining() != FLAC_STREAMINFO_SIZE + 12) {
      return false;
    }
    uint32_t major = br.ReadU8();
    if (major != 1) {
      // unsupported version;
      return false;
    }
    br.ReadU8(); // minor version
    mNumHeaders = Some(uint32_t(br.ReadU16()));
    br.Read(4); // fLaC
    blockType = br.ReadU8() & BITMASK(7);
    // First METADATA_BLOCK_STREAMINFO
    if (blockType != FLAC_METADATA_TYPE_STREAMINFO) {
      // First block must be a stream info.
      return false;
    }
  }

  uint32_t blockDataSize = br.ReadU24();
  const uint8_t* blockDataStart = br.Peek(blockDataSize);
  if (!blockDataStart) {
    // Incomplete block.
    return false;
  }

  switch (blockType) {
    case FLAC_METADATA_TYPE_STREAMINFO:
    {
      if (mPacketCount != 1 || blockDataSize != FLAC_STREAMINFO_SIZE) {
        // STREAMINFO must be the first metadata block found, and its size
        // is constant.
        return false;
      }

      mMinBlockSize = br.ReadU16();
      mMaxBlockSize = br.ReadU16();
      mMinFrameSize = br.ReadU24();
      mMaxFrameSize = br.ReadU24();

      uint64_t blob = br.ReadU64();
      uint32_t sampleRate = (blob >> 44) & BITMASK(20);
      if (!sampleRate) {
        return false;
      }
      uint32_t numChannels = ((blob >> 41) & BITMASK(3)) + 1;
      if (numChannels > FLAC_MAX_CHANNELS) {
        return false;
      }
      uint32_t bps = ((blob >> 38) & BITMASK(5)) + 1;
      if (bps > 24) {
        return false;
      }
      mNumFrames = blob & BITMASK(36);

      mInfo.mMimeType = "audio/flac";
      mInfo.mRate = sampleRate;
      mInfo.mChannels = numChannels;
      mInfo.mBitDepth = bps;
      mInfo.mCodecSpecificConfig->AppendElements(blockDataStart, blockDataSize);
      CheckedInt64 duration =
        SaferMultDiv(mNumFrames, USECS_PER_S, sampleRate);
      mInfo.mDuration = duration.isValid() ? duration.value() : 0;
      mParser = new OpusParser;
      break;
    }
    case FLAC_METADATA_TYPE_VORBIS_COMMENT:
    {
      if (!mParser) {
        // We must have seen a valid streaminfo first.
        return false;
      }
      nsTArray<uint8_t> comments(blockDataSize + 8);
      comments.AppendElements("OpusTags", 8);
      comments.AppendElements(blockDataStart, blockDataSize);
      if (!mParser->DecodeTags(comments.Elements(), comments.Length())) {
        return false;
      }
      break;
    }
    default:
      break;
  }

  if (mNumHeaders && mPacketCount > mNumHeaders.ref() + 1) {
    // Received too many header block. assuming invalid.
    return false;
  }

  if (lastBlock || (mNumHeaders && mNumHeaders.ref() + 1 == mPacketCount)) {
    mFullMetadata = true;
  }

  return true;
}

int64_t
FlacFrameParser::BlockDuration(const uint8_t* aPacket, size_t aLength) const
{
  if (!mInfo.IsValid()) {
    return -1;
  }
  if (mMinBlockSize == mMaxBlockSize) {
    // block size is fixed, use this instead of looking at the frame header.
    return mMinBlockSize;
  }
  // TODO
  return 0;
}

bool
FlacFrameParser::IsHeaderBlock(const uint8_t* aPacket, size_t aLength) const
{
  // Ogg Flac header
  // The one-byte packet type 0x7F
  // The four-byte ASCII signature "FLAC", i.e. 0x46, 0x4C, 0x41, 0x43

  // Flac header:
  // "fLaC", the FLAC stream marker in ASCII, meaning byte 0 of the stream is 0x66, followed by 0x4C 0x61 0x43

  // If we detect either a ogg or plain flac header, then it must be valid.
  if (aLength < 4 || aPacket[0] == 0xff) {
    // A header is at least 4 bytes.
    return false;
  }
  if (aPacket[0] == 0x7f) {
    // Ogg packet
    AutoByteReader br(aPacket + 1, aLength - 1);
    const uint8_t* signature = br.Read(4);
    return signature && !memcmp(signature, "FLAC", 4);
  }
  AutoByteReader br(aPacket, aLength - 1);
  const uint8_t* signature = br.Read(4);
  if (signature && !memcmp(signature, "fLaC", 4)) {
    // Flac start header, must have STREAMINFO as first metadata block;
    if (!br.CanRead8()) {
      return false;
    }
    uint32_t blockType = br.ReadU8() & 0x7f;
    return blockType == FLAC_METADATA_TYPE_STREAMINFO;
  }
  char type = aPacket[0] & 0x7f;
  return type >= 1 && type <= 6;
}

MetadataTags*
FlacFrameParser::GetTags() const
{
  MetadataTags* tags;

  tags = new MetadataTags;
  for (uint32_t i = 0; i < mParser->mTags.Length(); i++) {
    OggCodecState::AddVorbisComment(tags,
                                    mParser->mTags[i].Data(),
                                    mParser->mTags[i].Length());
  }

  return tags;
}

} // namespace mozilla
