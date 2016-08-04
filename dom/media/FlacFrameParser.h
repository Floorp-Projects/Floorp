/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FLAC_FRAME_PARSER_H_
#define FLAC_FRAME_PARSER_H_

#include "mozilla/Maybe.h"
#include "nsAutoPtr.h"
#include "MediaDecoder.h" // For MetadataTags
#include "MediaInfo.h"

namespace mozilla
{

class OpusParser;

// Decode a Flac Metadata block contained in either a ogg packet
// (https://xiph.org/flac/ogg_mapping.html) or in flac container
// (https://xiph.org/flac/format.html#frame_header)

class FlacFrameParser
{
public:
  FlacFrameParser();

  bool IsHeaderBlock(const uint8_t* aPacket, size_t aLength) const;
  bool DecodeHeaderBlock(const uint8_t* aPacket, size_t aLength);
  bool HasFullMetadata() const { return mFullMetadata; }
  // Return the duration in frames found in the block. -1 if error
  // such as invalid packet.
  int64_t BlockDuration(const uint8_t* aPacket, size_t aLength) const;

  // Return a hash table with tag metadata.
  MetadataTags* GetTags() const;

  AudioInfo mInfo;

private:
  bool ReconstructFlacGranulepos(void);
  Maybe<uint32_t> mNumHeaders;
  uint32_t mMinBlockSize;
  uint32_t mMaxBlockSize;
  uint32_t mMinFrameSize;
  uint32_t mMaxFrameSize;
  uint64_t mNumFrames;
  bool mFullMetadata;
  uint32_t mPacketCount;

  // Used to decode the vorbis comment metadata.
  nsAutoPtr<OpusParser> mParser;

};

}

#endif // FLAC_FRAME_PARSER_H_
