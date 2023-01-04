/* -*- mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* this source code form is subject to the terms of the mozilla public
 * license, v. 2.0. if a copy of the mpl was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioNodeEngineSSE2.h"
#include "AlignmentUtils.h"
#include <emmintrin.h>

namespace mozilla {
void AudioBufferAddWithScale_SSE(const float* aInput, float aScale,
                                 float* aOutput, uint32_t aSize) {
  __m128 vin0, vin1, vin2, vin3, vscaled0, vscaled1, vscaled2, vscaled3, vout0,
      vout1, vout2, vout3, vgain;

  ASSERT_ALIGNED16(aInput);
  ASSERT_ALIGNED16(aOutput);
  ASSERT_MULTIPLE16(aSize);

  vgain = _mm_load1_ps(&aScale);

  for (unsigned i = 0; i < aSize; i += 16) {
    vin0 = _mm_load_ps(&aInput[i]);
    vin1 = _mm_load_ps(&aInput[i + 4]);
    vin2 = _mm_load_ps(&aInput[i + 8]);
    vin3 = _mm_load_ps(&aInput[i + 12]);

    vscaled0 = _mm_mul_ps(vin0, vgain);
    vscaled1 = _mm_mul_ps(vin1, vgain);
    vscaled2 = _mm_mul_ps(vin2, vgain);
    vscaled3 = _mm_mul_ps(vin3, vgain);

    vin0 = _mm_load_ps(&aOutput[i]);
    vin1 = _mm_load_ps(&aOutput[i + 4]);
    vin2 = _mm_load_ps(&aOutput[i + 8]);
    vin3 = _mm_load_ps(&aOutput[i + 12]);

    vout0 = _mm_add_ps(vin0, vscaled0);
    vout1 = _mm_add_ps(vin1, vscaled1);
    vout2 = _mm_add_ps(vin2, vscaled2);
    vout3 = _mm_add_ps(vin3, vscaled3);

    _mm_store_ps(&aOutput[i], vout0);
    _mm_store_ps(&aOutput[i + 4], vout1);
    _mm_store_ps(&aOutput[i + 8], vout2);
    _mm_store_ps(&aOutput[i + 12], vout3);
  }
}

void AudioBlockCopyChannelWithScale_SSE(const float* aInput, float aScale,
                                        float* aOutput) {
  __m128 vin0, vin1, vin2, vin3, vout0, vout1, vout2, vout3;

  ASSERT_ALIGNED16(aInput);
  ASSERT_ALIGNED16(aOutput);

  __m128 vgain = _mm_load1_ps(&aScale);

  for (unsigned i = 0; i < WEBAUDIO_BLOCK_SIZE; i += 16) {
    vin0 = _mm_load_ps(&aInput[i]);
    vin1 = _mm_load_ps(&aInput[i + 4]);
    vin2 = _mm_load_ps(&aInput[i + 8]);
    vin3 = _mm_load_ps(&aInput[i + 12]);
    vout0 = _mm_mul_ps(vin0, vgain);
    vout1 = _mm_mul_ps(vin1, vgain);
    vout2 = _mm_mul_ps(vin2, vgain);
    vout3 = _mm_mul_ps(vin3, vgain);
    _mm_store_ps(&aOutput[i], vout0);
    _mm_store_ps(&aOutput[i + 4], vout1);
    _mm_store_ps(&aOutput[i + 8], vout2);
    _mm_store_ps(&aOutput[i + 12], vout3);
  }
}

void AudioBlockCopyChannelWithScale_SSE(const float aInput[WEBAUDIO_BLOCK_SIZE],
                                        const float aScale[WEBAUDIO_BLOCK_SIZE],
                                        float aOutput[WEBAUDIO_BLOCK_SIZE]) {
  __m128 vin0, vin1, vin2, vin3, vscaled0, vscaled1, vscaled2, vscaled3, vout0,
      vout1, vout2, vout3;

  ASSERT_ALIGNED16(aInput);
  ASSERT_ALIGNED16(aScale);
  ASSERT_ALIGNED16(aOutput);

  for (unsigned i = 0; i < WEBAUDIO_BLOCK_SIZE; i += 16) {
    vscaled0 = _mm_load_ps(&aScale[i]);
    vscaled1 = _mm_load_ps(&aScale[i + 4]);
    vscaled2 = _mm_load_ps(&aScale[i + 8]);
    vscaled3 = _mm_load_ps(&aScale[i + 12]);

    vin0 = _mm_load_ps(&aInput[i]);
    vin1 = _mm_load_ps(&aInput[i + 4]);
    vin2 = _mm_load_ps(&aInput[i + 8]);
    vin3 = _mm_load_ps(&aInput[i + 12]);

    vout0 = _mm_mul_ps(vin0, vscaled0);
    vout1 = _mm_mul_ps(vin1, vscaled1);
    vout2 = _mm_mul_ps(vin2, vscaled2);
    vout3 = _mm_mul_ps(vin3, vscaled3);

    _mm_store_ps(&aOutput[i], vout0);
    _mm_store_ps(&aOutput[i + 4], vout1);
    _mm_store_ps(&aOutput[i + 8], vout2);
    _mm_store_ps(&aOutput[i + 12], vout3);
  }
}

void AudioBufferInPlaceScale_SSE(float* aBlock, float aScale, uint32_t aSize) {
  __m128 vout0, vout1, vout2, vout3, vin0, vin1, vin2, vin3;

  ASSERT_ALIGNED16(aBlock);
  ASSERT_MULTIPLE16(aSize);

  __m128 vgain = _mm_load1_ps(&aScale);

  for (unsigned i = 0; i < aSize; i += 16) {
    vin0 = _mm_load_ps(&aBlock[i]);
    vin1 = _mm_load_ps(&aBlock[i + 4]);
    vin2 = _mm_load_ps(&aBlock[i + 8]);
    vin3 = _mm_load_ps(&aBlock[i + 12]);
    vout0 = _mm_mul_ps(vin0, vgain);
    vout1 = _mm_mul_ps(vin1, vgain);
    vout2 = _mm_mul_ps(vin2, vgain);
    vout3 = _mm_mul_ps(vin3, vgain);
    _mm_store_ps(&aBlock[i], vout0);
    _mm_store_ps(&aBlock[i + 4], vout1);
    _mm_store_ps(&aBlock[i + 8], vout2);
    _mm_store_ps(&aBlock[i + 12], vout3);
  }
}

void AudioBufferInPlaceScale_SSE(float* aBlock, float* aScale, uint32_t aSize) {
  __m128 vout0, vout1, vout2, vout3, vgain0, vgain1, vgain2, vgain3, vin0, vin1,
      vin2, vin3;

  ASSERT_ALIGNED16(aBlock);
  ASSERT_MULTIPLE16(aSize);

  for (unsigned i = 0; i < aSize; i += 16) {
    vin0 = _mm_load_ps(&aBlock[i]);
    vin1 = _mm_load_ps(&aBlock[i + 4]);
    vin2 = _mm_load_ps(&aBlock[i + 8]);
    vin3 = _mm_load_ps(&aBlock[i + 12]);
    vgain0 = _mm_load_ps(&aScale[i]);
    vgain1 = _mm_load_ps(&aScale[i + 4]);
    vgain2 = _mm_load_ps(&aScale[i + 8]);
    vgain3 = _mm_load_ps(&aScale[i + 12]);
    vout0 = _mm_mul_ps(vin0, vgain0);
    vout1 = _mm_mul_ps(vin1, vgain1);
    vout2 = _mm_mul_ps(vin2, vgain2);
    vout3 = _mm_mul_ps(vin3, vgain3);
    _mm_store_ps(&aBlock[i], vout0);
    _mm_store_ps(&aBlock[i + 4], vout1);
    _mm_store_ps(&aBlock[i + 8], vout2);
    _mm_store_ps(&aBlock[i + 12], vout3);
  }
}

void AudioBlockPanStereoToStereo_SSE(const float aInputL[WEBAUDIO_BLOCK_SIZE],
                                     const float aInputR[WEBAUDIO_BLOCK_SIZE],
                                     float aGainL, float aGainR,
                                     bool aIsOnTheLeft,
                                     float aOutputL[WEBAUDIO_BLOCK_SIZE],
                                     float aOutputR[WEBAUDIO_BLOCK_SIZE]) {
  __m128 vinl0, vinr0, vinl1, vinr1, vout0, vout1, vscaled0, vscaled1, vgainl,
      vgainr;

  ASSERT_ALIGNED16(aInputL);
  ASSERT_ALIGNED16(aInputR);
  ASSERT_ALIGNED16(aOutputL);
  ASSERT_ALIGNED16(aOutputR);

  vgainl = _mm_load1_ps(&aGainL);
  vgainr = _mm_load1_ps(&aGainR);

  if (aIsOnTheLeft) {
    for (unsigned i = 0; i < WEBAUDIO_BLOCK_SIZE; i += 8) {
      vinl0 = _mm_load_ps(&aInputL[i]);
      vinr0 = _mm_load_ps(&aInputR[i]);
      vinl1 = _mm_load_ps(&aInputL[i + 4]);
      vinr1 = _mm_load_ps(&aInputR[i + 4]);

      /* left channel : aOutputL  = aInputL + aInputR * gainL */
      vscaled0 = _mm_mul_ps(vinr0, vgainl);
      vscaled1 = _mm_mul_ps(vinr1, vgainl);
      vout0 = _mm_add_ps(vscaled0, vinl0);
      vout1 = _mm_add_ps(vscaled1, vinl1);
      _mm_store_ps(&aOutputL[i], vout0);
      _mm_store_ps(&aOutputL[i + 4], vout1);

      /* right channel : aOutputR = aInputR * gainR */
      vscaled0 = _mm_mul_ps(vinr0, vgainr);
      vscaled1 = _mm_mul_ps(vinr1, vgainr);
      _mm_store_ps(&aOutputR[i], vscaled0);
      _mm_store_ps(&aOutputR[i + 4], vscaled1);
    }
  } else {
    for (unsigned i = 0; i < WEBAUDIO_BLOCK_SIZE; i += 8) {
      vinl0 = _mm_load_ps(&aInputL[i]);
      vinr0 = _mm_load_ps(&aInputR[i]);
      vinl1 = _mm_load_ps(&aInputL[i + 4]);
      vinr1 = _mm_load_ps(&aInputR[i + 4]);

      /* left channel : aInputL * gainL */
      vscaled0 = _mm_mul_ps(vinl0, vgainl);
      vscaled1 = _mm_mul_ps(vinl1, vgainl);
      _mm_store_ps(&aOutputL[i], vscaled0);
      _mm_store_ps(&aOutputL[i + 4], vscaled1);

      /* right channel: aOutputR = aInputR + aInputL * gainR */
      vscaled0 = _mm_mul_ps(vinl0, vgainr);
      vscaled1 = _mm_mul_ps(vinl1, vgainr);
      vout0 = _mm_add_ps(vscaled0, vinr0);
      vout1 = _mm_add_ps(vscaled1, vinr1);
      _mm_store_ps(&aOutputR[i], vout0);
      _mm_store_ps(&aOutputR[i + 4], vout1);
    }
  }
}

void BufferComplexMultiply_SSE(const float* aInput, const float* aScale,
                               float* aOutput, uint32_t aSize) {
  unsigned i;
  __m128 in0, in1, in2, in3, outreal0, outreal1, outreal2, outreal3, outimag0,
      outimag1, outimag2, outimag3;

  ASSERT_ALIGNED16(aInput);
  ASSERT_ALIGNED16(aScale);
  ASSERT_ALIGNED16(aOutput);
  ASSERT_MULTIPLE16(aSize);

  for (i = 0; i < aSize * 2; i += 16) {
    in0 = _mm_load_ps(&aInput[i]);
    in1 = _mm_load_ps(&aInput[i + 4]);
    in2 = _mm_load_ps(&aInput[i + 8]);
    in3 = _mm_load_ps(&aInput[i + 12]);

    outreal0 = _mm_shuffle_ps(in0, in1, _MM_SHUFFLE(2, 0, 2, 0));
    outimag0 = _mm_shuffle_ps(in0, in1, _MM_SHUFFLE(3, 1, 3, 1));
    outreal2 = _mm_shuffle_ps(in2, in3, _MM_SHUFFLE(2, 0, 2, 0));
    outimag2 = _mm_shuffle_ps(in2, in3, _MM_SHUFFLE(3, 1, 3, 1));

    in0 = _mm_load_ps(&aScale[i]);
    in1 = _mm_load_ps(&aScale[i + 4]);
    in2 = _mm_load_ps(&aScale[i + 8]);
    in3 = _mm_load_ps(&aScale[i + 12]);

    outreal1 = _mm_shuffle_ps(in0, in1, _MM_SHUFFLE(2, 0, 2, 0));
    outimag1 = _mm_shuffle_ps(in0, in1, _MM_SHUFFLE(3, 1, 3, 1));
    outreal3 = _mm_shuffle_ps(in2, in3, _MM_SHUFFLE(2, 0, 2, 0));
    outimag3 = _mm_shuffle_ps(in2, in3, _MM_SHUFFLE(3, 1, 3, 1));

    in0 = _mm_sub_ps(_mm_mul_ps(outreal0, outreal1),
                     _mm_mul_ps(outimag0, outimag1));
    in1 = _mm_add_ps(_mm_mul_ps(outreal0, outimag1),
                     _mm_mul_ps(outimag0, outreal1));
    in2 = _mm_sub_ps(_mm_mul_ps(outreal2, outreal3),
                     _mm_mul_ps(outimag2, outimag3));
    in3 = _mm_add_ps(_mm_mul_ps(outreal2, outimag3),
                     _mm_mul_ps(outimag2, outreal3));

    outreal0 = _mm_unpacklo_ps(in0, in1);
    outreal1 = _mm_unpackhi_ps(in0, in1);
    outreal2 = _mm_unpacklo_ps(in2, in3);
    outreal3 = _mm_unpackhi_ps(in2, in3);

    _mm_store_ps(&aOutput[i], outreal0);
    _mm_store_ps(&aOutput[i + 4], outreal1);
    _mm_store_ps(&aOutput[i + 8], outreal2);
    _mm_store_ps(&aOutput[i + 12], outreal3);
  }
}

float AudioBufferSumOfSquares_SSE(const float* aInput, uint32_t aLength) {
  unsigned i;
  __m128 in0, in1, in2, in3, acc0, acc1, acc2, acc3;
  float out[4];

  ASSERT_ALIGNED16(aInput);
  ASSERT_MULTIPLE16(aLength);

  acc0 = _mm_setzero_ps();
  acc1 = _mm_setzero_ps();
  acc2 = _mm_setzero_ps();
  acc3 = _mm_setzero_ps();

  for (i = 0; i < aLength; i += 16) {
    in0 = _mm_load_ps(&aInput[i]);
    in1 = _mm_load_ps(&aInput[i + 4]);
    in2 = _mm_load_ps(&aInput[i + 8]);
    in3 = _mm_load_ps(&aInput[i + 12]);

    in0 = _mm_mul_ps(in0, in0);
    in1 = _mm_mul_ps(in1, in1);
    in2 = _mm_mul_ps(in2, in2);
    in3 = _mm_mul_ps(in3, in3);

    acc0 = _mm_add_ps(acc0, in0);
    acc1 = _mm_add_ps(acc1, in1);
    acc2 = _mm_add_ps(acc2, in2);
    acc3 = _mm_add_ps(acc3, in3);
  }

  acc0 = _mm_add_ps(acc0, acc1);
  acc0 = _mm_add_ps(acc0, acc2);
  acc0 = _mm_add_ps(acc0, acc3);

  _mm_store_ps(out, acc0);

  return out[0] + out[1] + out[2] + out[3];
}

void NaNToZeroInPlace_SSE(float* aSamples, size_t aCount) {
  __m128 vin0, vin1, vin2, vin3;
  __m128 vmask0, vmask1, vmask2, vmask3;
  __m128 vout0, vout1, vout2, vout3;

  float* samplesAligned16 = ALIGNED16(aSamples);
  size_t leadingElementsScalar =
      std::min(static_cast<size_t>(samplesAligned16 - aSamples), aCount);
  size_t remainingElements = aCount - leadingElementsScalar;
  size_t vectoredEnd = aCount - remainingElements % 16;

  MOZ_ASSERT(!((vectoredEnd - leadingElementsScalar) % 16));

  size_t i = 0;
  for (; i < leadingElementsScalar; i++) {
    if (aSamples[i] != aSamples[i]) {
      aSamples[i] = 0.0;
    }
  }

  ASSERT_ALIGNED16(&aSamples[i]);

  for (; i < vectoredEnd; i += 16) {
    vin0 = _mm_load_ps(&aSamples[i + 0]);
    vin1 = _mm_load_ps(&aSamples[i + 4]);
    vin2 = _mm_load_ps(&aSamples[i + 8]);
    vin3 = _mm_load_ps(&aSamples[i + 12]);

    vmask0 = _mm_cmpord_ps(vin0, vin0);
    vmask1 = _mm_cmpord_ps(vin1, vin1);
    vmask2 = _mm_cmpord_ps(vin2, vin2);
    vmask3 = _mm_cmpord_ps(vin3, vin3);

    vout0 = _mm_and_ps(vin0, vmask0);
    vout1 = _mm_and_ps(vin1, vmask1);
    vout2 = _mm_and_ps(vin2, vmask2);
    vout3 = _mm_and_ps(vin3, vmask3);

    _mm_store_ps(&aSamples[i + 0], vout0);
    _mm_store_ps(&aSamples[i + 4], vout1);
    _mm_store_ps(&aSamples[i + 8], vout2);
    _mm_store_ps(&aSamples[i + 12], vout3);
  }
  for (; i < aCount; i++) {
    if (aSamples[i] != aSamples[i]) {
      aSamples[i] = 0.0;
    }
  }
}

}  // namespace mozilla
