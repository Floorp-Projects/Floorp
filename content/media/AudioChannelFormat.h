/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef MOZILLA_AUDIOCHANNELFORMAT_H_
#define MOZILLA_AUDIOCHANNELFORMAT_H_

#include <stdint.h>

#include "nsTArrayForwardDeclare.h"

namespace mozilla {

/*
 * This file provides utilities for upmixing and downmixing channels.
 *
 * The channel layouts, upmixing and downmixing are consistent with the
 * Web Audio spec.
 *
 * Channel layouts for up to 6 channels:
 *   mono   { M }
 *   stereo { L, R }
 *          { L, R, C }
 *   quad   { L, R, SL, SR }
 *          { L, R, C, SL, SR }
 *   5.1    { L, R, C, LFE, SL, SR }
 *
 * Only 1, 2, 4 and 6 are currently defined in Web Audio.
 */

/**
 * Return a channel count whose channel layout includes all the channels from
 * aChannels1 and aChannels2.
 */
uint32_t
GetAudioChannelsSuperset(uint32_t aChannels1, uint32_t aChannels2);

/**
 * Given an array of input channel data, and an output channel count,
 * replaces the array with an array of upmixed channels.
 * This shuffles the array and may set some channel buffers to aZeroChannel.
 * Don't call this with input count >= output count.
 * This may return *more* channels than requested. In that case, downmixing
 * is required to to get to aOutputChannelCount. (This is how we handle
 * odd cases like 3 -> 4 upmixing.)
 * If aChannelArray.Length() was the input to one of a series of
 * GetAudioChannelsSuperset calls resulting in aOutputChannelCount,
 * no downmixing will be required.
 */
void
AudioChannelsUpMix(nsTArray<const void*>* aChannelArray,
                   uint32_t aOutputChannelCount,
                   const void* aZeroChannel);

/**
 * Given an array of input channels (which must be float format!),
 * downmix to aOutputChannelCount, and copy the results to the
 * channel buffers in aOutputChannels.
 * Don't call this with input count <= output count.
 */
void
AudioChannelsDownMix(const nsTArray<const void*>& aChannelArray,
                     float** aOutputChannels,
                     uint32_t aOutputChannelCount,
                     uint32_t aDuration);

// A version of AudioChannelsDownMix that downmixes int16_ts may be required.

} // namespace mozilla

#endif /* MOZILLA_AUDIOCHANNELFORMAT_H_ */
