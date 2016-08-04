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
#define FLAC_MAX_CHANNELS       8
#define FLAC_MIN_BLOCKSIZE     16
#define FLAC_MAX_BLOCKSIZE  65535
#define FLAC_MIN_FRAME_SIZE    11

#define BITMASK(x) ((1ULL << x)-1)

enum
{
  FLAC_CHMODE_INDEPENDENT = 0,
  FLAC_CHMODE_LEFT_SIDE,
  FLAC_CHMODE_RIGHT_SIDE,
  FLAC_CHMODE_MID_SIDE,
};

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

bool
FlacFrameParser::DecodeHeaderBlock(const uint8_t* aPacket, size_t aLength)
{
  if (!aLength || aPacket[0] == 0xff) {
    // Not a header block.
    return false;
  }
  AutoByteReader br(aPacket, aLength);

  mPacketCount++;

  // blockType is a misnomer as it could indicate here either a packet type
  // should it points to the start of a Flac in Ogg metadata, or an actual
  // block type as per the flac specification.
  uint32_t blockType = br.ReadU8() & 0x7f;

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
      if (blockDataSize != FLAC_STREAMINFO_SIZE) {
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

  if (mNumHeaders && mNumHeaders.ref() + 1 == mPacketCount) {
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
  return aLength && aPacket[0] != 0xff;
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
