/* -*- mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* this source code form is subject to the terms of the mozilla public
 * license, v. 2.0. if a copy of the mpl was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioNodeEngineSSE2.h"
#include <emmintrin.h>

#ifdef DEBUG
  #define ASSERT_ALIGNED(ptr)                                                  \
            MOZ_ASSERT((((uintptr_t)ptr + 15) & ~0x0F) == (uintptr_t)ptr,      \
                       #ptr " has to be aligned 16-bytes aligned.");
#else
  #define ASSERT_ALIGNED(ptr)
#endif

namespace mozilla {
void
AudioBlockAddChannelWithScale_SSE(const float aInput[WEBAUDIO_BLOCK_SIZE],
                                  float aScale,
                                  float aOutput[WEBAUDIO_BLOCK_SIZE])
{
  __m128 vin0, vin1, vin2, vin3,
         vscaled0, vscaled1, vscaled2, vscaled3,
         vout0, vout1, vout2, vout3,
         vgain;

  ASSERT_ALIGNED(aInput);
  ASSERT_ALIGNED(aOutput);

  vgain = _mm_load1_ps(&aScale);

  for (unsigned i = 0; i < WEBAUDIO_BLOCK_SIZE; i+=16) {
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

void
AudioBlockAddChannel_SSE(const float aInput[WEBAUDIO_BLOCK_SIZE],
                         float aOutput[WEBAUDIO_BLOCK_SIZE])
{
  __m128 vin0, vin1, vin2, vin3,
         vin4, vin5, vin6, vin7,
         vout0, vout1, vout2, vout3;

  ASSERT_ALIGNED(aInput);
  ASSERT_ALIGNED(aOutput);

  for (unsigned i = 0; i < WEBAUDIO_BLOCK_SIZE; i+=16) {
    vin0 = _mm_load_ps(&aInput[i]);
    vin1 = _mm_load_ps(&aInput[i + 4]);
    vin2 = _mm_load_ps(&aInput[i + 8]);
    vin3 = _mm_load_ps(&aInput[i + 12]);

    vin4 = _mm_load_ps(&aOutput[i]);
    vin5 = _mm_load_ps(&aOutput[i + 4]);
    vin6 = _mm_load_ps(&aOutput[i + 8]);
    vin7 = _mm_load_ps(&aOutput[i + 12]);

    vout0 = _mm_add_ps(vin0, vin4);
    vout1 = _mm_add_ps(vin1, vin5);
    vout2 = _mm_add_ps(vin2, vin6);
    vout3 = _mm_add_ps(vin3, vin7);

    _mm_store_ps(&aOutput[i], vout0);
    _mm_store_ps(&aOutput[i + 4], vout1);
    _mm_store_ps(&aOutput[i + 8], vout2);
    _mm_store_ps(&aOutput[i + 12], vout3);
  }
}

void
AudioBlockCopyChannelWithScale_SSE(const float* aInput,
                                   float aScale,
                                   float* aOutput)
{
  __m128 vin0, vin1, vin2, vin3,
         vout0, vout1, vout2, vout3;

  ASSERT_ALIGNED(aInput);
  ASSERT_ALIGNED(aOutput);

  __m128 vgain = _mm_load1_ps(&aScale);

  for (unsigned i = 0 ; i < WEBAUDIO_BLOCK_SIZE; i+=16) {
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

void
AudioBlockCopyChannelWithScale_SSE(const float aInput[WEBAUDIO_BLOCK_SIZE],
                                   const float aScale[WEBAUDIO_BLOCK_SIZE],
                                   float aOutput[WEBAUDIO_BLOCK_SIZE])
{
  __m128 vin0, vin1, vin2, vin3,
         vscaled0, vscaled1, vscaled2, vscaled3,
         vout0, vout1, vout2, vout3;

  ASSERT_ALIGNED(aInput);
  ASSERT_ALIGNED(aScale);
  ASSERT_ALIGNED(aOutput);

  for (unsigned i = 0 ; i < WEBAUDIO_BLOCK_SIZE; i+=16) {
    vscaled0 = _mm_load_ps(&aScale[i]);
    vscaled1 = _mm_load_ps(&aScale[i+4]);
    vscaled2 = _mm_load_ps(&aScale[i+8]);
    vscaled3 = _mm_load_ps(&aScale[i+12]);

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

void
AudioBlockInPlaceScale_SSE(float aBlock[WEBAUDIO_BLOCK_SIZE],
                           uint32_t aChannelCount,
                           float aScale)
{
  __m128 vout0, vout1, vout2, vout3,
         vin0, vin1, vin2, vin3;

  ASSERT_ALIGNED(aBlock);

  __m128 vgain = _mm_load1_ps(&aScale);

  for (unsigned i = 0; i < WEBAUDIO_BLOCK_SIZE; i+=16) {
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

void
AudioBlockPanStereoToStereo_SSE(const float aInputL[WEBAUDIO_BLOCK_SIZE],
                                const float aInputR[WEBAUDIO_BLOCK_SIZE],
                                float aGainL, float aGainR, bool aIsOnTheLeft,
                                float aOutputL[WEBAUDIO_BLOCK_SIZE],
                                float aOutputR[WEBAUDIO_BLOCK_SIZE])
{
  __m128 vinl0, vinr0, vinl1, vinr1,
         vout0, vout1,
         vscaled0, vscaled1,
         vgainl, vgainr;

  ASSERT_ALIGNED(aInputL);
  ASSERT_ALIGNED(aInputR);
  ASSERT_ALIGNED(aOutputL);
  ASSERT_ALIGNED(aOutputR);

  vgainl = _mm_load1_ps(&aGainL);
  vgainr = _mm_load1_ps(&aGainR);

  if (aIsOnTheLeft) {
    for (unsigned i = 0; i < WEBAUDIO_BLOCK_SIZE; i+=8) {
      vinl0 = _mm_load_ps(&aInputL[i]);
      vinr0 = _mm_load_ps(&aInputR[i]);
      vinl1 = _mm_load_ps(&aInputL[i+4]);
      vinr1 = _mm_load_ps(&aInputR[i+4]);

      /* left channel : aOutputL  = aInputL + aInputR * gainL */
      vscaled0 = _mm_mul_ps(vinr0, vgainl);
      vscaled1 = _mm_mul_ps(vinr1, vgainl);
      vout0 = _mm_add_ps(vscaled0, vinl0);
      vout1 = _mm_add_ps(vscaled1, vinl1);
      _mm_store_ps(&aOutputL[i], vout0);
      _mm_store_ps(&aOutputL[i+4], vout1);

      /* right channel : aOutputR = aInputR * gainR */
      vscaled0 = _mm_mul_ps(vinr0, vgainr);
      vscaled1 = _mm_mul_ps(vinr1, vgainr);
      _mm_store_ps(&aOutputR[i], vscaled0);
      _mm_store_ps(&aOutputR[i+4], vscaled1);
    }
  } else {
    for (unsigned i = 0; i < WEBAUDIO_BLOCK_SIZE; i+=8) {
      vinl0 = _mm_load_ps(&aInputL[i]);
      vinr0 = _mm_load_ps(&aInputR[i]);
      vinl1 = _mm_load_ps(&aInputL[i+4]);
      vinr1 = _mm_load_ps(&aInputR[i+4]);

      /* left channel : aInputL * gainL */
      vscaled0 = _mm_mul_ps(vinl0, vgainl);
      vscaled1 = _mm_mul_ps(vinl1, vgainl);
      _mm_store_ps(&aOutputL[i], vscaled0);
      _mm_store_ps(&aOutputL[i+4], vscaled1);

      /* right channel: aOutputR = aInputR + aInputL * gainR */
      vscaled0 = _mm_mul_ps(vinl0, vgainr);
      vscaled1 = _mm_mul_ps(vinl1, vgainr);
      vout0 = _mm_add_ps(vscaled0, vinr0);
      vout1 = _mm_add_ps(vscaled1, vinr1);
      _mm_store_ps(&aOutputR[i], vout0);
      _mm_store_ps(&aOutputR[i+4], vout1);
    }
  }
}
}
