/* -*- mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* this source code form is subject to the terms of the mozilla public
 * license, v. 2.0. if a copy of the mpl was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIONODEENGINEGENERIC_H_
#define MOZILLA_AUDIONODEENGINEGENERIC_H_

#include "AudioNodeEngine.h"
#include "AlignmentUtils.h"

#include "xsimd/xsimd.hpp"

namespace mozilla {

template <class Arch>
struct Engine {
  static void AudioBufferAddWithScale(const float* aInput, float aScale,
                                      float* aOutput, uint32_t aSize) {
    ASSERT_ALIGNED16(aInput);
    ASSERT_ALIGNED16(aOutput);
    ASSERT_MULTIPLE16(aSize);

    xsimd::batch<float, Arch> vgain(aScale);

#pragma GCC unroll(4)
    for (unsigned i = 0; i < aSize; i += 4 * xsimd::batch<float, Arch>::size) {
      auto vin1 = xsimd::batch<float, Arch>::load_aligned(&aInput[i]);
      auto vin2 = xsimd::batch<float, Arch>::load_aligned(&aOutput[i]);
      auto vout = xsimd::fma(vin1, vgain, vin2);
      vout.store_aligned(&aOutput[i]);
    }
  };

  static void AudioBlockCopyChannelWithScale(const float* aInput, float aScale,
                                             float* aOutput) {
    ASSERT_ALIGNED16(aInput);
    ASSERT_ALIGNED16(aOutput);

    xsimd::batch<float, Arch> vgain = (aScale);

#pragma GCC unroll(4)
    for (unsigned i = 0; i < WEBAUDIO_BLOCK_SIZE;
         i += xsimd::batch<float, Arch>::size) {
      auto vin = xsimd::batch<float, Arch>::load_aligned(&aInput[i]);
      auto vout = vin * vgain;
      vout.store_aligned(&aOutput[i]);
    }
  };

  static void AudioBlockCopyChannelWithScale(
      const float aInput[WEBAUDIO_BLOCK_SIZE],
      const float aScale[WEBAUDIO_BLOCK_SIZE],
      float aOutput[WEBAUDIO_BLOCK_SIZE]) {
    ASSERT_ALIGNED16(aInput);
    ASSERT_ALIGNED16(aScale);
    ASSERT_ALIGNED16(aOutput);

#pragma GCC unroll(4)
    for (unsigned i = 0; i < WEBAUDIO_BLOCK_SIZE;
         i += xsimd::batch<float, Arch>::size) {
      auto vscaled = xsimd::batch<float, Arch>::load_aligned(&aScale[i]);
      auto vin = xsimd::batch<float, Arch>::load_aligned(&aInput[i]);
      auto vout = vin * vscaled;
      vout.store_aligned(&aOutput[i]);
    }
  };

  static void AudioBufferInPlaceScale(float* aBlock, float aScale,
                                      uint32_t aSize) {
    ASSERT_ALIGNED16(aBlock);
    ASSERT_MULTIPLE16(aSize);

    xsimd::batch<float, Arch> vgain(aScale);

#pragma GCC unroll(4)
    for (unsigned i = 0; i < aSize; i += xsimd::batch<float, Arch>::size) {
      auto vin = xsimd::batch<float, Arch>::load_aligned(&aBlock[i]);
      auto vout = vin * vgain;
      vout.store_aligned(&aBlock[i]);
    }
  };

  static void AudioBufferInPlaceScale(float* aBlock, float* aScale,
                                      uint32_t aSize) {
    ASSERT_ALIGNED16(aBlock);
    ASSERT_MULTIPLE16(aSize);

#pragma GCC unroll(4)
    for (unsigned i = 0; i < aSize; i += xsimd::batch<float, Arch>::size) {
      auto vin = xsimd::batch<float, Arch>::load_aligned(&aBlock[i]);
      auto vgain = xsimd::batch<float, Arch>::load_aligned(&aScale[i]);
      auto vout = vin * vgain;
      vout.store_aligned(&aBlock[i]);
    }
  };

  static void AudioBlockPanStereoToStereo(
      const float aInputL[WEBAUDIO_BLOCK_SIZE],
      const float aInputR[WEBAUDIO_BLOCK_SIZE], float aGainL, float aGainR,
      bool aIsOnTheLeft, float aOutputL[WEBAUDIO_BLOCK_SIZE],
      float aOutputR[WEBAUDIO_BLOCK_SIZE]) {
    ASSERT_ALIGNED16(aInputL);
    ASSERT_ALIGNED16(aInputR);
    ASSERT_ALIGNED16(aOutputL);
    ASSERT_ALIGNED16(aOutputR);

    xsimd::batch<float, Arch> vgainl(aGainL);
    xsimd::batch<float, Arch> vgainr(aGainR);

    if (aIsOnTheLeft) {
#pragma GCC unroll(2)
      for (unsigned i = 0; i < WEBAUDIO_BLOCK_SIZE;
           i += xsimd::batch<float, Arch>::size) {
        auto vinl = xsimd::batch<float, Arch>::load_aligned(&aInputL[i]);
        auto vinr = xsimd::batch<float, Arch>::load_aligned(&aInputR[i]);

        /* left channel : aOutputL  = aInputL + aInputR * gainL */
        auto vout = xsimd::fma(vinr, vgainl, vinl);
        vout.store_aligned(&aOutputL[i]);

        /* right channel : aOutputR = aInputR * gainR */
        auto vscaled = vinr * vgainr;
        vscaled.store_aligned(&aOutputR[i]);
      }
    } else {
#pragma GCC unroll(2)
      for (unsigned i = 0; i < WEBAUDIO_BLOCK_SIZE;
           i += xsimd::batch<float, Arch>::size) {
        auto vinl = xsimd::batch<float, Arch>::load_aligned(&aInputL[i]);
        auto vinr = xsimd::batch<float, Arch>::load_aligned(&aInputR[i]);

        /* left channel : aInputL * gainL */
        auto vscaled = vinl * vgainl;
        vscaled.store_aligned(&aOutputL[i]);

        /* right channel: aOutputR = aInputR + aInputL * gainR */
        auto vout = xsimd::fma(vinl, vgainr, vinr);
        vout.store_aligned(&aOutputR[i]);
      }
    }
  };

  static void BufferComplexMultiply(const float* aInput, const float* aScale,
                                    float* aOutput, uint32_t aSize) {
    ASSERT_ALIGNED16(aInput);
    ASSERT_ALIGNED16(aScale);
    ASSERT_ALIGNED16(aOutput);
    ASSERT_MULTIPLE16(aSize);

#pragma GCC unroll(2)
    for (unsigned i = 0; i < aSize * 2;
         i += 2 * xsimd::batch<std::complex<float>>::size) {
      auto in1 = xsimd::batch<std::complex<float>>::load_aligned(
          reinterpret_cast<const std::complex<float>*>(&aInput[i]));
      auto in2 = xsimd::batch<std::complex<float>>::load_aligned(
          reinterpret_cast<const std::complex<float>*>(&aScale[i]));
      auto out = in1 * in2;
      out.store_aligned(reinterpret_cast<std::complex<float>*>(&aOutput[i]));
    }
  };

  static float AudioBufferSumOfSquares(const float* aInput, uint32_t aLength) {
    ASSERT_ALIGNED16(aInput);
    ASSERT_MULTIPLE16(aLength);

    constexpr uint32_t unroll_factor = 4;
    xsimd::batch<float, Arch> accs[unroll_factor] = {0.f, 0.f, 0.f, 0.f};

    for (uint32_t i = 0; i < aLength;
         i += unroll_factor * xsimd::batch<float, Arch>::size) {
#pragma GCC unroll
      for (uint32_t j = 0; j < unroll_factor; ++j) {
        auto in = xsimd::batch<float, Arch>::load_aligned(
            &aInput[i + xsimd::batch<float, Arch>::size * j]);
        accs[j] = xsimd::fma(in, in, accs[j]);
      }
    }

    return reduce_add((accs[0] + accs[1]) + (accs[2] + accs[3]));
  };

  static void NaNToZeroInPlace(float* aSamples, size_t aCount) {
    float* samplesAligned16 = ALIGNED16(aSamples);
    size_t leadingElementsScalar =
        std::min(static_cast<size_t>(samplesAligned16 - aSamples), aCount);
    size_t remainingElements = aCount - leadingElementsScalar;
    size_t vectoredEnd =
        aCount - remainingElements % (4 * xsimd::batch<float, Arch>::size);

    MOZ_ASSERT(!((vectoredEnd - leadingElementsScalar) %
                 (4 * xsimd::batch<float, Arch>::size)));

    size_t i = 0;
    for (; i < leadingElementsScalar; i++) {
      if (aSamples[i] != aSamples[i]) {
        aSamples[i] = 0.0;
      }
    }

    ASSERT_ALIGNED16(&aSamples[i]);

#pragma GCC unroll(4)
    for (; i < vectoredEnd; i += xsimd::batch<float, Arch>::size) {
      auto vin = xsimd::batch<float, Arch>::load_aligned(&aSamples[i]);
      auto vout =
          xsimd::select(xsimd::isnan(vin), xsimd::batch<float, Arch>(0.f), vin);
      vout.store_aligned(&aSamples[i]);
    }
    for (; i < aCount; i++) {
      if (aSamples[i] != aSamples[i]) {
        aSamples[i] = 0.0;
      }
    }
  };

  static void AudioBlockPanStereoToStereo(
      const float aInputL[WEBAUDIO_BLOCK_SIZE],
      const float aInputR[WEBAUDIO_BLOCK_SIZE],
      const float aGainL[WEBAUDIO_BLOCK_SIZE],
      const float aGainR[WEBAUDIO_BLOCK_SIZE],
      const bool aIsOnTheLeft[WEBAUDIO_BLOCK_SIZE],
      float aOutputL[WEBAUDIO_BLOCK_SIZE],
      float aOutputR[WEBAUDIO_BLOCK_SIZE]) {
    ASSERT_ALIGNED16(aInputL);
    ASSERT_ALIGNED16(aInputR);
    ASSERT_ALIGNED16(aGainL);
    ASSERT_ALIGNED16(aGainR);
    ASSERT_ALIGNED16(aIsOnTheLeft);
    ASSERT_ALIGNED16(aOutputL);
    ASSERT_ALIGNED16(aOutputR);

#pragma GCC unroll(2)
    for (uint32_t i = 0; i < WEBAUDIO_BLOCK_SIZE;
         i += xsimd::batch<float, Arch>::size) {
      auto mask =
          xsimd::batch_bool<float, Arch>::load_aligned(&aIsOnTheLeft[i]);

      auto inputL = xsimd::batch<float, Arch>::load_aligned(&aInputL[i]);
      auto inputR = xsimd::batch<float, Arch>::load_aligned(&aInputR[i]);
      auto gainL = xsimd::batch<float, Arch>::load_aligned(&aGainL[i]);
      auto gainR = xsimd::batch<float, Arch>::load_aligned(&aGainR[i]);

      auto outL_true = xsimd::fma(inputR, gainL, inputL);
      auto outR_true = inputR * gainR;

      auto outL_false = inputL * gainL;
      auto outR_false = xsimd::fma(inputL, gainR, inputR);

      auto outL = xsimd::select(mask, outL_true, outL_false);
      auto outR = xsimd::select(mask, outR_true, outR_false);

      outL.store_aligned(&aOutputL[i]);
      outR.store_aligned(&aOutputR[i]);
    }
  }
};

}  // namespace mozilla

#endif
