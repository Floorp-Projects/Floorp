/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(OpusParser_h_)
#  define OpusParser_h_

#  include "nsTArray.h"
#  include "nsString.h"

namespace mozilla {

class OpusParser {
 public:
  OpusParser();

  bool DecodeHeader(unsigned char* aData, size_t aLength);
  bool DecodeTags(unsigned char* aData, size_t aLength);
  static bool IsValidMapping2ChannelsCount(uint8_t aChannels);

  // Various fields from the Ogg Opus header.
  int mRate;              // Sample rate the decoder uses (always 48 kHz).
  uint32_t mNominalRate;  // Original sample rate of the data (informational).
  int mChannels;          // Number of channels the stream encodes.
  uint16_t mPreSkip;      // Number of samples to strip after decoder reset.
#  ifdef MOZ_SAMPLE_TYPE_FLOAT32
  float mGain;  // Gain to apply to decoder output.
#  else
  int32_t mGain_Q16;  // Gain to apply to the decoder output.
#  endif
  int mChannelMapping;  // Channel mapping family.
  int mStreams;         // Number of packed streams in each packet.
  int mCoupledStreams;  // Number of packed coupled streams in each packet.
  unsigned char mMappingTable[255] = {};  // Channel mapping table.

  // Granule position (end sample) of the last decoded Opus packet. This is
  // used to calculate the amount we should trim from the last packet.
  int64_t mPrevPacketGranulepos;

  nsTArray<nsCString> mTags;  // Unparsed comment strings from the header.

  nsCString mVendorString;  // Encoder vendor string from the header.
};

}  // namespace mozilla

#endif
