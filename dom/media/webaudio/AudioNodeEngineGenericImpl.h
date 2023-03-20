/* -*- mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* this source code form is subject to the terms of the mozilla public
 * license, v. 2.0. if a copy of the mpl was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIONODEENGINEGENERICIMPL_H_
#define MOZILLA_AUDIONODEENGINEGENERICIMPL_H_

#include "AudioNodeEngineGeneric.h"
#include "AlignmentUtils.h"

#if defined(__GNUC__) && __GNUC__ > 7
#  define MOZ_PRAGMA(tokens) _Pragma(#tokens)
#  define MOZ_UNROLL(factor) MOZ_PRAGMA(GCC unroll factor)
#elif defined(__INTEL_COMPILER) || (defined(__clang__) && __clang_major__ > 3)
#  define MOZ_PRAGMA(tokens) _Pragma(#tokens)
#  define MOZ_UNROLL(factor) MOZ_PRAGMA(unroll factor)
#else
#  define MOZ_UNROLL(_)
#endif

namespace mozilla {

template <class Arch>
static bool is_aligned(const void* ptr) {
  return (reinterpret_cast<uintptr_t>(ptr) &
          ~(static_cast<uintptr_t>(Arch::alignment()) - 1)) ==
         reinterpret_cast<uintptr_t>(ptr);
};

template <class Arch>
void Engine<Arch>::AudioBufferAddWithScale(const float* aInput, float aScale,
                                           float* aOutput, uint32_t aSize) {
  if constexpr (Arch::requires_alignment()) {
    if (aScale == 1.0f) {
      while (!is_aligned<Arch>(aInput) || !is_aligned<Arch>(aOutput)) {
        if (!aSize) return;
        *aOutput += *aInput;
        ++aOutput;
        ++aInput;
        --aSize;
      }
    } else {
      while (!is_aligned<Arch>(aInput) || !is_aligned<Arch>(aOutput)) {
        if (!aSize) return;
        *aOutput += *aInput * aScale;
        ++aOutput;
        ++aInput;
        --aSize;
      }
    }
  }
  MOZ_ASSERT(is_aligned<Arch>(aInput), "aInput is aligned");
  MOZ_ASSERT(is_aligned<Arch>(aOutput), "aOutput is aligned");

  xsimd::batch<float, Arch> vgain(aScale);

  uint32_t aVSize = aSize & ~(xsimd::batch<float, Arch>::size - 1);
  MOZ_UNROLL(4)
  for (unsigned i = 0; i < aVSize; i += xsimd::batch<float, Arch>::size) {
    auto vin1 = xsimd::batch<float, Arch>::load_aligned(&aInput[i]);
    auto vin2 = xsimd::batch<float, Arch>::load_aligned(&aOutput[i]);
    auto vout = xsimd::fma(vin1, vgain, vin2);
    vout.store_aligned(&aOutput[i]);
  }

  for (unsigned i = aVSize; i < aSize; ++i) {
    aOutput[i] += aInput[i] * aScale;
  }
}

template <class Arch>
void Engine<Arch>::AudioBlockCopyChannelWithScale(const float* aInput,
                                                  float aScale,
                                                  float* aOutput) {
  MOZ_ASSERT(is_aligned<Arch>(aInput), "aInput is aligned");
  MOZ_ASSERT(is_aligned<Arch>(aOutput), "aOutput is aligned");

  MOZ_ASSERT((WEBAUDIO_BLOCK_SIZE % xsimd::batch<float, Arch>::size == 0),
             "requires tail processing");

  xsimd::batch<float, Arch> vgain = (aScale);

  MOZ_UNROLL(4)
  for (unsigned i = 0; i < WEBAUDIO_BLOCK_SIZE;
       i += xsimd::batch<float, Arch>::size) {
    auto vin = xsimd::batch<float, Arch>::load_aligned(&aInput[i]);
    auto vout = vin * vgain;
    vout.store_aligned(&aOutput[i]);
  }
};

template <class Arch>
void Engine<Arch>::AudioBlockCopyChannelWithScale(
    const float aInput[WEBAUDIO_BLOCK_SIZE],
    const float aScale[WEBAUDIO_BLOCK_SIZE],
    float aOutput[WEBAUDIO_BLOCK_SIZE]) {
  MOZ_ASSERT(is_aligned<Arch>(aInput), "aInput is aligned");
  MOZ_ASSERT(is_aligned<Arch>(aOutput), "aOutput is aligned");
  MOZ_ASSERT(is_aligned<Arch>(aScale), "aScale is aligned");

  MOZ_ASSERT((WEBAUDIO_BLOCK_SIZE % xsimd::batch<float, Arch>::size == 0),
             "requires tail processing");

  MOZ_UNROLL(4)
  for (unsigned i = 0; i < WEBAUDIO_BLOCK_SIZE;
       i += xsimd::batch<float, Arch>::size) {
    auto vscaled = xsimd::batch<float, Arch>::load_aligned(&aScale[i]);
    auto vin = xsimd::batch<float, Arch>::load_aligned(&aInput[i]);
    auto vout = vin * vscaled;
    vout.store_aligned(&aOutput[i]);
  }
};

template <class Arch>
void Engine<Arch>::AudioBufferInPlaceScale(float* aBlock, float aScale,
                                           uint32_t aSize) {
  MOZ_ASSERT(is_aligned<Arch>(aBlock), "aBlock is aligned");

  xsimd::batch<float, Arch> vgain(aScale);

  uint32_t aVSize = aSize & ~(xsimd::batch<float, Arch>::size - 1);
  MOZ_UNROLL(4)
  for (unsigned i = 0; i < aVSize; i += xsimd::batch<float, Arch>::size) {
    auto vin = xsimd::batch<float, Arch>::load_aligned(&aBlock[i]);
    auto vout = vin * vgain;
    vout.store_aligned(&aBlock[i]);
  }
  for (unsigned i = aVSize; i < aSize; ++i) aBlock[i] *= aScale;
};

template <class Arch>
void Engine<Arch>::AudioBufferInPlaceScale(float* aBlock, float* aScale,
                                           uint32_t aSize) {
  MOZ_ASSERT(is_aligned<Arch>(aBlock), "aBlock is aligned");
  MOZ_ASSERT(is_aligned<Arch>(aScale), "aScale is aligned");

  uint32_t aVSize = aSize & ~(xsimd::batch<float, Arch>::size - 1);
  MOZ_UNROLL(4)
  for (unsigned i = 0; i < aVSize; i += xsimd::batch<float, Arch>::size) {
    auto vin = xsimd::batch<float, Arch>::load_aligned(&aBlock[i]);
    auto vgain = xsimd::batch<float, Arch>::load_aligned(&aScale[i]);
    auto vout = vin * vgain;
    vout.store_aligned(&aBlock[i]);
  }
  for (uint32_t i = aVSize; i < aSize; ++i) {
    *aBlock++ *= *aScale++;
  }
};

template <class Arch>
void Engine<Arch>::AudioBlockPanStereoToStereo(
    const float aInputL[WEBAUDIO_BLOCK_SIZE],
    const float aInputR[WEBAUDIO_BLOCK_SIZE], float aGainL, float aGainR,
    bool aIsOnTheLeft, float aOutputL[WEBAUDIO_BLOCK_SIZE],
    float aOutputR[WEBAUDIO_BLOCK_SIZE]) {
  MOZ_ASSERT(is_aligned<Arch>(aInputL), "aInputL is aligned");
  MOZ_ASSERT(is_aligned<Arch>(aInputR), "aInputR is aligned");
  MOZ_ASSERT(is_aligned<Arch>(aOutputL), "aOutputL is aligned");
  MOZ_ASSERT(is_aligned<Arch>(aOutputR), "aOutputR is aligned");

  MOZ_ASSERT((WEBAUDIO_BLOCK_SIZE % xsimd::batch<float, Arch>::size == 0),
             "requires tail processing");

  xsimd::batch<float, Arch> vgainl(aGainL);
  xsimd::batch<float, Arch> vgainr(aGainR);

  if (aIsOnTheLeft) {
    MOZ_UNROLL(2)
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
    MOZ_UNROLL(2)
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

template <class Arch>
void Engine<Arch>::BufferComplexMultiply(const float* aInput,
                                         const float* aScale, float* aOutput,
                                         uint32_t aSize) {
  MOZ_ASSERT(is_aligned<Arch>(aInput), "aInput is aligned");
  MOZ_ASSERT(is_aligned<Arch>(aOutput), "aOutput is aligned");
  MOZ_ASSERT(is_aligned<Arch>(aScale), "aScale is aligned");
  MOZ_ASSERT((aSize % xsimd::batch<float, Arch>::size == 0),
             "requires tail processing");

  MOZ_UNROLL(2)
  for (unsigned i = 0; i < aSize * 2;
       i += 2 * xsimd::batch<std::complex<float>, Arch>::size) {
    auto in1 = xsimd::batch<std::complex<float>, Arch>::load_aligned(
        reinterpret_cast<const std::complex<float>*>(&aInput[i]));
    auto in2 = xsimd::batch<std::complex<float>, Arch>::load_aligned(
        reinterpret_cast<const std::complex<float>*>(&aScale[i]));
    auto out = in1 * in2;
    out.store_aligned(reinterpret_cast<std::complex<float>*>(&aOutput[i]));
  }
};

template <class Arch>
float Engine<Arch>::AudioBufferSumOfSquares(const float* aInput,
                                            uint32_t aLength) {
  float sum = 0.f;

  if constexpr (Arch::requires_alignment()) {
    while (!is_aligned<Arch>(aInput)) {
      if (!aLength) {
        return sum;
      }
      sum += *aInput * *aInput;
      ++aInput;
      --aLength;
    }
  }

  MOZ_ASSERT(is_aligned<Arch>(aInput), "aInput is aligned");

  constexpr uint32_t unroll_factor = 4;
  xsimd::batch<float, Arch> accs[unroll_factor] = {0.f, 0.f, 0.f, 0.f};

  uint32_t vLength =
      aLength & ~(unroll_factor * xsimd::batch<float, Arch>::size - 1);

  for (uint32_t i = 0; i < vLength;
       i += unroll_factor * xsimd::batch<float, Arch>::size) {
    MOZ_UNROLL(4)
    for (uint32_t j = 0; j < unroll_factor; ++j) {
      auto in = xsimd::batch<float, Arch>::load_aligned(
          &aInput[i + xsimd::batch<float, Arch>::size * j]);
      accs[j] = xsimd::fma(in, in, accs[j]);
    }
  }

  sum += reduce_add((accs[0] + accs[1]) + (accs[2] + accs[3]));
  for (uint32_t i = vLength; i < aLength; ++i) sum += aInput[i] * aInput[i];
  return sum;
};

template <class Arch>
void Engine<Arch>::NaNToZeroInPlace(float* aSamples, size_t aCount) {
  if constexpr (Arch::requires_alignment()) {
    while (!is_aligned<Arch>(aSamples)) {
      if (!aCount) {
        return;
      }
      if (*aSamples != *aSamples) {
        *aSamples = 0.0;
      }
      ++aSamples;
      --aCount;
    }
  }

  MOZ_ASSERT(is_aligned<Arch>(aSamples), "aSamples is aligned");

  uint32_t vCount = aCount & ~(xsimd::batch<float, Arch>::size - 1);

  MOZ_UNROLL(4)
  for (uint32_t i = 0; i < vCount; i += xsimd::batch<float, Arch>::size) {
    auto vin = xsimd::batch<float, Arch>::load_aligned(&aSamples[i]);
    auto vout =
        xsimd::select(xsimd::isnan(vin), xsimd::batch<float, Arch>(0.f), vin);
    vout.store_aligned(&aSamples[i]);
  }

  for (uint32_t i = vCount; i < aCount; i++) {
    if (aSamples[i] != aSamples[i]) {
      aSamples[i] = 0.0;
    }
  }
};

template <class Arch>
void Engine<Arch>::AudioBlockPanStereoToStereo(
    const float aInputL[WEBAUDIO_BLOCK_SIZE],
    const float aInputR[WEBAUDIO_BLOCK_SIZE],
    const float aGainL[WEBAUDIO_BLOCK_SIZE],
    const float aGainR[WEBAUDIO_BLOCK_SIZE],
    const bool aIsOnTheLeft[WEBAUDIO_BLOCK_SIZE],
    float aOutputL[WEBAUDIO_BLOCK_SIZE], float aOutputR[WEBAUDIO_BLOCK_SIZE]) {
  MOZ_ASSERT(is_aligned<Arch>(aInputL), "aInputL is aligned");
  MOZ_ASSERT(is_aligned<Arch>(aInputR), "aInputR is aligned");
  MOZ_ASSERT(is_aligned<Arch>(aGainL), "aGainL is aligned");
  MOZ_ASSERT(is_aligned<Arch>(aGainR), "aGainR is aligned");
  MOZ_ASSERT(is_aligned<Arch>(aIsOnTheLeft), "aIsOnTheLeft is aligned");
  MOZ_ASSERT(is_aligned<Arch>(aOutputL), "aOutputL is aligned");
  MOZ_ASSERT(is_aligned<Arch>(aOutputR), "aOutputR is aligned");

  MOZ_ASSERT((WEBAUDIO_BLOCK_SIZE % xsimd::batch<float, Arch>::size == 0),
             "requires tail processing");

  MOZ_UNROLL(2)
  for (uint32_t i = 0; i < WEBAUDIO_BLOCK_SIZE;
       i += xsimd::batch<float, Arch>::size) {
    auto mask = xsimd::batch_bool<float, Arch>::load_aligned(&aIsOnTheLeft[i]);

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

}  // namespace mozilla

#endif
