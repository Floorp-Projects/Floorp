/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelFormat.h"

#include <algorithm>

namespace mozilla {

uint32_t
GetAudioChannelsSuperset(uint32_t aChannels1, uint32_t aChannels2)
{
  return std::max(aChannels1, aChannels2);
}

/**
 * UpMixMatrix represents a conversion matrix by exploiting the fact that
 * each output channel comes from at most one input channel.
 */
struct UpMixMatrix {
  uint8_t mInputDestination[CUSTOM_CHANNEL_LAYOUTS];
};

static const UpMixMatrix
gUpMixMatrices[CUSTOM_CHANNEL_LAYOUTS*(CUSTOM_CHANNEL_LAYOUTS - 1)/2] =
{
  // Upmixes from mono
  { { 0, 0 } },
  { { 0, IGNORE, IGNORE } },
  { { 0, 0, IGNORE, IGNORE } },
  { { 0, IGNORE, IGNORE, IGNORE, IGNORE } },
  { { IGNORE, IGNORE, 0, IGNORE, IGNORE, IGNORE } },
  // Upmixes from stereo
  { { 0, 1, IGNORE } },
  { { 0, 1, IGNORE, IGNORE } },
  { { 0, 1, IGNORE, IGNORE, IGNORE } },
  { { 0, 1, IGNORE, IGNORE, IGNORE, IGNORE } },
  // Upmixes from 3-channel
  { { 0, 1, 2, IGNORE } },
  { { 0, 1, 2, IGNORE, IGNORE } },
  { { 0, 1, 2, IGNORE, IGNORE, IGNORE } },
  // Upmixes from quad
  { { 0, 1, 2, 3, IGNORE } },
  { { 0, 1, IGNORE, IGNORE, 2, 3 } },
  // Upmixes from 5-channel
  { { 0, 1, 2, 3, 4, IGNORE } }
};

void
AudioChannelsUpMix(nsTArray<const void*>* aChannelArray,
                   uint32_t aOutputChannelCount,
                   const void* aZeroChannel)
{
  uint32_t inputChannelCount = aChannelArray->Length();
  uint32_t outputChannelCount =
    GetAudioChannelsSuperset(aOutputChannelCount, inputChannelCount);
  NS_ASSERTION(outputChannelCount > inputChannelCount,
               "No up-mix needed");
  MOZ_ASSERT(inputChannelCount > 0, "Bad number of channels");
  MOZ_ASSERT(outputChannelCount > 0, "Bad number of channels");

  aChannelArray->SetLength(outputChannelCount);

  if (inputChannelCount < CUSTOM_CHANNEL_LAYOUTS &&
      outputChannelCount <= CUSTOM_CHANNEL_LAYOUTS) {
    const UpMixMatrix& m = gUpMixMatrices[
      gMixingMatrixIndexByChannels[inputChannelCount - 1] +
      outputChannelCount - inputChannelCount - 1];

    const void* outputChannels[CUSTOM_CHANNEL_LAYOUTS];

    for (uint32_t i = 0; i < outputChannelCount; ++i) {
      uint8_t channelIndex = m.mInputDestination[i];
      if (channelIndex == IGNORE) {
        outputChannels[i] = aZeroChannel;
      } else {
        outputChannels[i] = aChannelArray->ElementAt(channelIndex);
      }
    }
    for (uint32_t i = 0; i < outputChannelCount; ++i) {
      aChannelArray->ElementAt(i) = outputChannels[i];
    }
    return;
  }

  for (uint32_t i = inputChannelCount; i < outputChannelCount; ++i) {
    aChannelArray->ElementAt(i) = aZeroChannel;
  }
}

} // namespace mozilla
