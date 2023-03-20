/* -*- mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* this source code form is subject to the terms of the mozilla public
 * license, v. 2.0. if a copy of the mpl was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIONODEENGINEGENERIC_H_
#define MOZILLA_AUDIONODEENGINEGENERIC_H_

#include "AudioNodeEngine.h"

#include "xsimd/xsimd.hpp"

namespace mozilla {

template <class Arch>
struct Engine {
  static void AudioBufferAddWithScale(const float* aInput, float aScale,
                                      float* aOutput, uint32_t aSize);

  static void AudioBlockCopyChannelWithScale(const float* aInput, float aScale,
                                             float* aOutput);

  static void AudioBlockCopyChannelWithScale(
      const float aInput[WEBAUDIO_BLOCK_SIZE],
      const float aScale[WEBAUDIO_BLOCK_SIZE],
      float aOutput[WEBAUDIO_BLOCK_SIZE]);

  static void AudioBufferInPlaceScale(float* aBlock, float aScale,
                                      uint32_t aSize);

  static void AudioBufferInPlaceScale(float* aBlock, float* aScale,
                                      uint32_t aSize);

  static void AudioBlockPanStereoToStereo(
      const float aInputL[WEBAUDIO_BLOCK_SIZE],
      const float aInputR[WEBAUDIO_BLOCK_SIZE], float aGainL, float aGainR,
      bool aIsOnTheLeft, float aOutputL[WEBAUDIO_BLOCK_SIZE],
      float aOutputR[WEBAUDIO_BLOCK_SIZE]);

  static void BufferComplexMultiply(const float* aInput, const float* aScale,
                                    float* aOutput, uint32_t aSize);

  static float AudioBufferSumOfSquares(const float* aInput, uint32_t aLength);

  static void NaNToZeroInPlace(float* aSamples, size_t aCount);

  static void AudioBlockPanStereoToStereo(
      const float aInputL[WEBAUDIO_BLOCK_SIZE],
      const float aInputR[WEBAUDIO_BLOCK_SIZE],
      const float aGainL[WEBAUDIO_BLOCK_SIZE],
      const float aGainR[WEBAUDIO_BLOCK_SIZE],
      const bool aIsOnTheLeft[WEBAUDIO_BLOCK_SIZE],
      float aOutputL[WEBAUDIO_BLOCK_SIZE], float aOutputR[WEBAUDIO_BLOCK_SIZE]);
};

}  // namespace mozilla

#endif
