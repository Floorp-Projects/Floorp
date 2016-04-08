/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioConverter.h"
#include <string.h>

/*
 *  Parts derived from MythTV AudioConvert Class
 *  Created by Jean-Yves Avenard.
 *
 *  Copyright (C) Bubblestuff Pty Ltd 2013
 *  Copyright (C) foobum@gmail.com 2010
 */

namespace mozilla {

AudioConverter::AudioConverter(const AudioConfig& aIn, const AudioConfig& aOut)
  : mIn(aIn)
  , mOut(aOut)
{
  MOZ_DIAGNOSTIC_ASSERT(aIn.Channels() == aOut.Channels() &&
                        aIn.Rate() == aOut.Rate() &&
                        aIn.Format() == aOut.Format() &&
                        aIn.Interleaved() == aOut.Interleaved(),
                        "Only channel reordering is supported at this stage");
  MOZ_DIAGNOSTIC_ASSERT(aOut.Interleaved(), "planar audio format not supported");
  InitChannelMap();
}

bool
AudioConverter::InitChannelMap()
{
  if (!CanReorderAudio()) {
    return false;
  }
  for (uint32_t i = 0; i < mIn.Layout().Count(); i++) {
    for (uint32_t j = 0; j < mIn.Layout().Count(); j++) {
      if (mOut.Layout()[j] == mIn.Layout()[i]) {
        mChannelOrderMap[j] = i;
        break;
      }
    }
  }
  return true;
}

bool
AudioConverter::CanWorkInPlace() const
{
  return mIn.Channels() * mIn.Rate() * AudioConfig::SampleSize(mIn.Format()) >=
    mOut.Channels() * mOut.Rate() * AudioConfig::SampleSize(mOut.Format());
}

size_t
AudioConverter::Process(void* aOut, const void* aIn, size_t aBytes)
{
  if (!CanWorkInPlace()) {
    return 0;
  }
  if (mIn.Layout() != mOut.Layout() &&
      CanReorderAudio()) {
    ReOrderInterleavedChannels(aOut, aIn, aBytes);
  }
  return aBytes;
}

// Reorder interleaved channels.
// Can work in place (e.g aOut == aIn).
template <class AudioDataType>
void
_ReOrderInterleavedChannels(AudioDataType* aOut, const AudioDataType* aIn,
                            uint32_t aFrames, uint32_t aChannels,
                            const uint32_t* aChannelOrderMap)
{
  MOZ_DIAGNOSTIC_ASSERT(aChannels <= MAX_AUDIO_CHANNELS);
  AudioDataType val[MAX_AUDIO_CHANNELS];
  for (uint32_t i = 0; i < aFrames; i++) {
    for (uint32_t j = 0; j < aChannels; j++) {
      val[j] = aIn[aChannelOrderMap[j]];
    }
    for (uint32_t j = 0; j < aChannels; j++) {
      aOut[j] = val[j];
    }
    aOut += aChannels;
    aIn += aChannels;
  }
}

void
AudioConverter::ReOrderInterleavedChannels(void* aOut, const void* aIn,
                                           size_t aDataSize) const
{
  MOZ_DIAGNOSTIC_ASSERT(mIn.Channels() == mOut.Channels());

  if (mOut.Layout() == mIn.Layout()) {
    return;
  }
  if (mOut.Channels() == 1) {
    // If channel count is 1, planar and non-planar formats are the same and
    // there's nothing to reorder.
    if (aOut != aIn) {
      memmove(aOut, aIn, aDataSize);
    }
    return;
  }

  uint32_t bits = AudioConfig::FormatToBits(mOut.Format());
  switch (bits) {
    case 8:
      _ReOrderInterleavedChannels((uint8_t*)aOut, (const uint8_t*)aIn,
                                  aDataSize/sizeof(uint8_t)/mIn.Channels(),
                                  mIn.Channels(), mChannelOrderMap);
      break;
    case 16:
      _ReOrderInterleavedChannels((int16_t*)aOut,(const int16_t*)aIn,
                                  aDataSize/sizeof(int16_t)/mIn.Channels(),
                                  mIn.Channels(), mChannelOrderMap);
      break;
    default:
      MOZ_DIAGNOSTIC_ASSERT(AudioConfig::SampleSize(mOut.Format()) == 4);
      _ReOrderInterleavedChannels((int32_t*)aOut,(const int32_t*)aIn,
                                  aDataSize/sizeof(int32_t)/mIn.Channels(),
                                  mIn.Channels(), mChannelOrderMap);
      break;
  }
}

} // namespace mozilla