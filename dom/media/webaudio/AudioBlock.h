/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef MOZILLA_AUDIOBLOCK_H_
#define MOZILLA_AUDIOBLOCK_H_

#include "AudioSegment.h"

namespace mozilla {

/**
 * Allocates, if necessary, aChannelCount buffers of WEBAUDIO_BLOCK_SIZE float
 * samples for writing to an AudioChunk.
 */
void AllocateAudioBlock(uint32_t aChannelCount, AudioChunk* aChunk);

} // namespace mozilla

#endif // MOZILLA_AUDIOBLOCK_H_
