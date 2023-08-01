/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include "mozilla/EndianUtils.h"

#include "OpusParser.h"
#include "VideoUtils.h"

#include <opus/opus.h>
extern "C" {
#include <opus/opus_multistream.h>
}

#include <cmath>

namespace mozilla {

extern LazyLogModule gMediaDecoderLog;
#define OPUS_LOG(type, msg) MOZ_LOG(gMediaDecoderLog, type, msg)

OpusParser::OpusParser()
    : mRate(0),
      mNominalRate(0),
      mChannels(0),
      mPreSkip(0),
#ifdef MOZ_SAMPLE_TYPE_FLOAT32
      mGain(1.0f),
#else
      mGain_Q16(65536),
#endif
      mChannelMapping(0),
      mStreams(0),
      mCoupledStreams(0),
      mPrevPacketGranulepos(0) {
}

bool OpusParser::DecodeHeader(unsigned char* aData, size_t aLength) {
  if (aLength < 19 || memcmp(aData, "OpusHead", 8) != 0) {
    OPUS_LOG(LogLevel::Debug, ("Invalid Opus file: unrecognized header"));
    return false;
  }

  mRate = 48000;  // The Opus decoder runs at 48 kHz regardless.

  int version = aData[8];
  // Accept file format versions 0.x.
  if ((version & 0xf0) != 0) {
    OPUS_LOG(LogLevel::Debug,
             ("Rejecting unknown Opus file version %d", version));
    return false;
  }

  mChannels = aData[9];
  if (mChannels < 1) {
    OPUS_LOG(LogLevel::Debug,
             ("Invalid Opus file: Number of channels %d", mChannels));
    return false;
  }

  mPreSkip = LittleEndian::readUint16(aData + 10);
  mNominalRate = LittleEndian::readUint32(aData + 12);
  double gain_dB = LittleEndian::readInt16(aData + 16) / 256.0;
#ifdef MOZ_SAMPLE_TYPE_FLOAT32
  mGain = static_cast<float>(pow(10, 0.05 * gain_dB));
#else
  mGain_Q16 = static_cast<int32_t>(std::min(
      65536 * pow(10, 0.05 * gain_dB) + 0.5, static_cast<double>(INT32_MAX)));
#endif
  mChannelMapping = aData[18];

  if (mChannelMapping == 0) {
    // Mapping family 0 only allows two channels
    if (mChannels > 2) {
      OPUS_LOG(LogLevel::Debug, ("Invalid Opus file: too many channels (%d) for"
                                 " mapping family 0.",
                                 mChannels));
      return false;
    }
    mStreams = 1;
    mCoupledStreams = mChannels - 1;
    mMappingTable[0] = 0;
    mMappingTable[1] = 1;
  } else if (mChannelMapping == 1 || mChannelMapping == 2 ||
             mChannelMapping == 255) {
    // Currently only up to 8 channels are defined for mapping family 1
    if (mChannelMapping == 1 && mChannels > 8) {
      OPUS_LOG(LogLevel::Debug, ("Invalid Opus file: too many channels (%d) for"
                                 " mapping family 1.",
                                 mChannels));
      return false;
    }
    if (mChannelMapping == 2) {
      if (!IsValidMapping2ChannelsCount(mChannels)) {
        return false;
      }
    }
    if (aLength > static_cast<unsigned>(20 + mChannels)) {
      mStreams = aData[19];
      mCoupledStreams = aData[20];
      int i;
      for (i = 0; i < mChannels; i++) {
        mMappingTable[i] = aData[21 + i];
      }
    } else {
      OPUS_LOG(LogLevel::Debug, ("Invalid Opus file: channel mapping %d,"
                                 " but no channel mapping table",
                                 mChannelMapping));
      return false;
    }
  } else {
    OPUS_LOG(LogLevel::Debug, ("Invalid Opus file: unsupported channel mapping "
                               "family %d",
                               mChannelMapping));
    return false;
  }
  if (mStreams < 1) {
    OPUS_LOG(LogLevel::Debug, ("Invalid Opus file: no streams"));
    return false;
  }
  if (mCoupledStreams > mStreams) {
    OPUS_LOG(LogLevel::Debug,
             ("Invalid Opus file: more coupled streams (%d) than "
              "total streams (%d)",
              mCoupledStreams, mStreams));
    return false;
  }

#ifdef DEBUG
  OPUS_LOG(LogLevel::Debug, ("Opus stream header:"));
  OPUS_LOG(LogLevel::Debug, (" channels: %d", mChannels));
  OPUS_LOG(LogLevel::Debug, ("  preskip: %d", mPreSkip));
  OPUS_LOG(LogLevel::Debug, (" original: %d Hz", mNominalRate));
  OPUS_LOG(LogLevel::Debug, ("     gain: %.2f dB", gain_dB));
  OPUS_LOG(LogLevel::Debug, ("Channel Mapping:"));
  OPUS_LOG(LogLevel::Debug, ("   family: %d", mChannelMapping));
  OPUS_LOG(LogLevel::Debug, ("  streams: %d", mStreams));
#endif
  return true;
}

bool OpusParser::DecodeTags(unsigned char* aData, size_t aLength) {
  if (aLength < 16 || memcmp(aData, "OpusTags", 8) != 0) {
    return false;
  }

  // Copy out the raw comment lines, but only do basic validation
  // checks against the string packing: too little data, too many
  // comments, or comments that are too long. Rejecting these cases
  // helps reduce the propagation of broken files.
  // We do not ensure they are valid UTF-8 here, nor do we validate
  // the required ASCII_TAG=value format of the user comments.
  const unsigned char* buf = aData + 8;
  uint32_t bytes = aLength - 8;
  uint32_t len;
  // Read the vendor string.
  len = LittleEndian::readUint32(buf);
  buf += 4;
  bytes -= 4;
  if (len > bytes) return false;
  mVendorString = nsCString(reinterpret_cast<const char*>(buf), len);
  buf += len;
  bytes -= len;
  // Read the user comments.
  if (bytes < 4) return false;
  uint32_t ncomments = LittleEndian::readUint32(buf);
  buf += 4;
  bytes -= 4;
  // If there are so many comments even their length fields
  // won't fit in the packet, stop reading now.
  if (ncomments > (bytes >> 2)) return false;
  for (uint32_t i = 0; i < ncomments; i++) {
    if (bytes < 4) return false;
    len = LittleEndian::readUint32(buf);
    buf += 4;
    bytes -= 4;
    if (len > bytes) return false;
    mTags.AppendElement(nsCString(reinterpret_cast<const char*>(buf), len));
    buf += len;
    bytes -= len;
  }

#ifdef DEBUG
  OPUS_LOG(LogLevel::Debug, ("Opus metadata header:"));
  OPUS_LOG(LogLevel::Debug, ("  vendor: %s", mVendorString.get()));
  for (uint32_t i = 0; i < mTags.Length(); i++) {
    OPUS_LOG(LogLevel::Debug, (" %s", mTags[i].get()));
  }
#endif
  return true;
}

/* static */
bool OpusParser::IsValidMapping2ChannelsCount(uint8_t aChannels) {
  // https://tools.ietf.org/html/draft-ietf-codec-ambisonics-08#page-4
  // For both channel mapping family 2 and family 3, the allowed numbers
  // of channels: (1 + n)^2 + 2j for n = 0, 1, ..., 14 and j = 0 or 1,
  // where n denotes the (highest) ambisonic order and j denotes whether
  // or not there is a separate non-diegetic stereo stream Explicitly the
  // allowed number of channels are 1, 3, 4, 6, 9, 11, 16, 18, 25, 27, 36,
  // 38, 49, 51, 64, 66, 81, 83, 100, 102, 121, 123, 144, 146, 169, 171,
  // 196, 198, 225, and 227.

  // We use the property that int(sqrt(n)) == int(sqrt(n+2)) for n != 3
  // which is handled by the test n^2 + 2 != channel
  if (aChannels < 1 || aChannels > 227) {
    return false;
  }
  double val = sqrt(aChannels);
  int32_t valInt = int32_t(val);
  return val == valInt || valInt * valInt + 2 == aChannels;
}

#undef OPUS_LOG

}  // namespace mozilla
