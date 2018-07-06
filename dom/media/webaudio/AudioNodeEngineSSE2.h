/* -*- mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* this source code form is subject to the terms of the mozilla public
 * license, v. 2.0. if a copy of the mpl was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioNodeEngine.h"

namespace mozilla {
void
AudioBufferAddWithScale_SSE(const float* aInput,
                            float aScale,
                            float* aOutput,
                            uint32_t aSize);

void
AudioBlockCopyChannelWithScale_SSE(const float* aInput,
                                   float aScale,
                                   float* aOutput);

void
AudioBlockCopyChannelWithScale_SSE(const float aInput[WEBAUDIO_BLOCK_SIZE],
                                   const float aScale[WEBAUDIO_BLOCK_SIZE],
                                   float aOutput[WEBAUDIO_BLOCK_SIZE]);

void
AudioBufferInPlaceScale_SSE(float* aBlock,
                            float aScale,
                            uint32_t aSize);
void
AudioBufferInPlaceScale_SSE(float* aBlock,
                            float* aScale,
                            uint32_t aSize);

void
AudioBlockPanStereoToStereo_SSE(const float aInputL[WEBAUDIO_BLOCK_SIZE],
                                const float aInputR[WEBAUDIO_BLOCK_SIZE],
                                float aGainL, float aGainR, bool aIsOnTheLeft,
                                float aOutputL[WEBAUDIO_BLOCK_SIZE],
                                float aOutputR[WEBAUDIO_BLOCK_SIZE]);

float
AudioBufferSumOfSquares_SSE(const float* aInput, uint32_t aLength);

void
BufferComplexMultiply_SSE(const float* aInput,
                          const float* aScale,
                          float* aOutput,
                          uint32_t aSize);
}
