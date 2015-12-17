/* -*- mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* this source code form is subject to the terms of the mozilla public
 * license, v. 2.0. if a copy of the mpl was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioNodeEngineNEON.h"
#include <arm_neon.h>

//#ifdef DEBUG
#if 0 // see bug 921099
  #define ASSERT_ALIGNED(ptr)                                                  \
            MOZ_ASSERT((((uintptr_t)ptr + 15) & ~0x0F) == (uintptr_t)ptr,      \
                       #ptr " has to be aligned 16-bytes aligned.");
#else
  #define ASSERT_ALIGNED(ptr)
#endif

#define ADDRESS_OF(array, index) ((float32_t*)&array[index])

namespace mozilla {
void AudioBufferAddWithScale_NEON(const float* aInput,
                                  float aScale,
                                  float* aOutput,
                                  uint32_t aSize)
{
  ASSERT_ALIGNED(aInput);
  ASSERT_ALIGNED(aOutput);

  float32x4_t vin0, vin1, vin2, vin3;
  float32x4_t vout0, vout1, vout2, vout3;
  float32x4_t vscale = vmovq_n_f32(aScale);

  uint32_t dif = aSize % 16;
  aSize -= dif;
  unsigned i = 0;
  for (; i < aSize; i+=16) {
    vin0 = vld1q_f32(ADDRESS_OF(aInput, i));
    vin1 = vld1q_f32(ADDRESS_OF(aInput, i+4));
    vin2 = vld1q_f32(ADDRESS_OF(aInput, i+8));
    vin3 = vld1q_f32(ADDRESS_OF(aInput, i+12));

    vout0 = vld1q_f32(ADDRESS_OF(aOutput, i));
    vout1 = vld1q_f32(ADDRESS_OF(aOutput, i+4));
    vout2 = vld1q_f32(ADDRESS_OF(aOutput, i+8));
    vout3 = vld1q_f32(ADDRESS_OF(aOutput, i+12));

    vout0 = vmlaq_f32(vout0, vin0, vscale);
    vout1 = vmlaq_f32(vout1, vin1, vscale);
    vout2 = vmlaq_f32(vout2, vin2, vscale);
    vout3 = vmlaq_f32(vout3, vin3, vscale);

    vst1q_f32(ADDRESS_OF(aOutput, i), vout0);
    vst1q_f32(ADDRESS_OF(aOutput, i+4), vout1);
    vst1q_f32(ADDRESS_OF(aOutput, i+8), vout2);
    vst1q_f32(ADDRESS_OF(aOutput, i+12), vout3);
  }

  for (unsigned j = 0; j < dif; ++i, ++j) {
    aOutput[i] += aInput[i]*aScale;
  }
}
void
AudioBlockCopyChannelWithScale_NEON(const float* aInput,
                                    float aScale,
                                    float* aOutput)
{
  ASSERT_ALIGNED(aInput);
  ASSERT_ALIGNED(aOutput);

  float32x4_t vin0, vin1, vin2, vin3;
  float32x4_t vout0, vout1, vout2, vout3;
  float32x4_t vscale = vmovq_n_f32(aScale);

  for (uint32_t i = 0; i < WEBAUDIO_BLOCK_SIZE; i+=16) {
    vin0 = vld1q_f32(ADDRESS_OF(aInput, i));
    vin1 = vld1q_f32(ADDRESS_OF(aInput, i+4));
    vin2 = vld1q_f32(ADDRESS_OF(aInput, i+8));
    vin3 = vld1q_f32(ADDRESS_OF(aInput, i+12));

    vout0 = vmulq_f32(vin0, vscale);
    vout1 = vmulq_f32(vin1, vscale);
    vout2 = vmulq_f32(vin2, vscale);
    vout3 = vmulq_f32(vin3, vscale);

    vst1q_f32(ADDRESS_OF(aOutput, i), vout0);
    vst1q_f32(ADDRESS_OF(aOutput, i+4), vout1);
    vst1q_f32(ADDRESS_OF(aOutput, i+8), vout2);
    vst1q_f32(ADDRESS_OF(aOutput, i+12), vout3);
  }
}

void
AudioBlockCopyChannelWithScale_NEON(const float aInput[WEBAUDIO_BLOCK_SIZE],
                                    const float aScale[WEBAUDIO_BLOCK_SIZE],
                                    float aOutput[WEBAUDIO_BLOCK_SIZE])
{
  ASSERT_ALIGNED(aInput);
  ASSERT_ALIGNED(aScale);
  ASSERT_ALIGNED(aOutput);

  float32x4_t vin0, vin1, vin2, vin3;
  float32x4_t vout0, vout1, vout2, vout3;
  float32x4_t vscale0, vscale1, vscale2, vscale3;

  for (uint32_t i = 0; i < WEBAUDIO_BLOCK_SIZE; i+=16) {
    vin0 = vld1q_f32(ADDRESS_OF(aInput, i));
    vin1 = vld1q_f32(ADDRESS_OF(aInput, i+4));
    vin2 = vld1q_f32(ADDRESS_OF(aInput, i+8));
    vin3 = vld1q_f32(ADDRESS_OF(aInput, i+12));

    vscale0 = vld1q_f32(ADDRESS_OF(aScale, i));
    vscale1 = vld1q_f32(ADDRESS_OF(aScale, i+4));
    vscale2 = vld1q_f32(ADDRESS_OF(aScale, i+8));
    vscale3 = vld1q_f32(ADDRESS_OF(aScale, i+12));

    vout0 = vmulq_f32(vin0, vscale0);
    vout1 = vmulq_f32(vin1, vscale1);
    vout2 = vmulq_f32(vin2, vscale2);
    vout3 = vmulq_f32(vin3, vscale3);

    vst1q_f32(ADDRESS_OF(aOutput, i), vout0);
    vst1q_f32(ADDRESS_OF(aOutput, i+4), vout1);
    vst1q_f32(ADDRESS_OF(aOutput, i+8), vout2);
    vst1q_f32(ADDRESS_OF(aOutput, i+12), vout3);
  }
}

void
AudioBufferInPlaceScale_NEON(float* aBlock,
                             float aScale,
                             uint32_t aSize)
{
  ASSERT_ALIGNED(aBlock);

  float32x4_t vin0, vin1, vin2, vin3;
  float32x4_t vout0, vout1, vout2, vout3;
  float32x4_t vscale = vmovq_n_f32(aScale);

  uint32_t dif = aSize % 16;
  uint32_t vectorSize = aSize - dif;
  uint32_t i = 0;
  for (; i < vectorSize; i+=16) {
    vin0 = vld1q_f32(ADDRESS_OF(aBlock, i));
    vin1 = vld1q_f32(ADDRESS_OF(aBlock, i+4));
    vin2 = vld1q_f32(ADDRESS_OF(aBlock, i+8));
    vin3 = vld1q_f32(ADDRESS_OF(aBlock, i+12));

    vout0 = vmulq_f32(vin0, vscale);
    vout1 = vmulq_f32(vin1, vscale);
    vout2 = vmulq_f32(vin2, vscale);
    vout3 = vmulq_f32(vin3, vscale);

    vst1q_f32(ADDRESS_OF(aBlock, i), vout0);
    vst1q_f32(ADDRESS_OF(aBlock, i+4), vout1);
    vst1q_f32(ADDRESS_OF(aBlock, i+8), vout2);
    vst1q_f32(ADDRESS_OF(aBlock, i+12), vout3);
  }

  for (unsigned j = 0; j < dif; ++i, ++j) {
    aBlock[i] *= aScale;
  }
}

void
AudioBlockPanStereoToStereo_NEON(const float aInputL[WEBAUDIO_BLOCK_SIZE],
                                 const float aInputR[WEBAUDIO_BLOCK_SIZE],
                                 float aGainL, float aGainR, bool aIsOnTheLeft,
                                 float aOutputL[WEBAUDIO_BLOCK_SIZE],
                                 float aOutputR[WEBAUDIO_BLOCK_SIZE])
{
  ASSERT_ALIGNED(aInputL);
  ASSERT_ALIGNED(aInputR);
  ASSERT_ALIGNED(aOutputL);
  ASSERT_ALIGNED(aOutputR);

  float32x4_t vinL0, vinL1;
  float32x4_t vinR0, vinR1;
  float32x4_t voutL0, voutL1;
  float32x4_t voutR0, voutR1;
  float32x4_t vscaleL = vmovq_n_f32(aGainL);
  float32x4_t vscaleR = vmovq_n_f32(aGainR);

  if (aIsOnTheLeft) {
    for (uint32_t i = 0; i < WEBAUDIO_BLOCK_SIZE; i+=8) {
      vinL0 = vld1q_f32(ADDRESS_OF(aInputL, i));
      vinL1 = vld1q_f32(ADDRESS_OF(aInputL, i+4));

      vinR0 = vld1q_f32(ADDRESS_OF(aInputR, i));
      vinR1 = vld1q_f32(ADDRESS_OF(aInputR, i+4));

      voutL0 = vmlaq_f32(vinL0, vinR0, vscaleL);
      voutL1 = vmlaq_f32(vinL1, vinR1, vscaleL);

      vst1q_f32(ADDRESS_OF(aOutputL, i), voutL0);
      vst1q_f32(ADDRESS_OF(aOutputL, i+4), voutL1);

      voutR0 = vmulq_f32(vinR0, vscaleR);
      voutR1 = vmulq_f32(vinR1, vscaleR);

      vst1q_f32(ADDRESS_OF(aOutputR, i), voutR0);
      vst1q_f32(ADDRESS_OF(aOutputR, i+4), voutR1);
    }
  } else {
    for (uint32_t i = 0; i < WEBAUDIO_BLOCK_SIZE; i+=8) {
      vinL0 = vld1q_f32(ADDRESS_OF(aInputL, i));
      vinL1 = vld1q_f32(ADDRESS_OF(aInputL, i+4));

      vinR0 = vld1q_f32(ADDRESS_OF(aInputR, i));
      vinR1 = vld1q_f32(ADDRESS_OF(aInputR, i+4));

      voutL0 = vmulq_f32(vinL0, vscaleL);
      voutL1 = vmulq_f32(vinL1, vscaleL);

      vst1q_f32(ADDRESS_OF(aOutputL, i), voutL0);
      vst1q_f32(ADDRESS_OF(aOutputL, i+4), voutL1);

      voutR0 = vmlaq_f32(vinR0, vinL0, vscaleR);
      voutR1 = vmlaq_f32(vinR1, vinL1, vscaleR);

      vst1q_f32(ADDRESS_OF(aOutputR, i), voutR0);
      vst1q_f32(ADDRESS_OF(aOutputR, i+4), voutR1);
    }
  }
}

void
AudioBlockPanStereoToStereo_NEON(const float aInputL[WEBAUDIO_BLOCK_SIZE],
                                 const float aInputR[WEBAUDIO_BLOCK_SIZE],
                                 float aGainL[WEBAUDIO_BLOCK_SIZE],
                                 float aGainR[WEBAUDIO_BLOCK_SIZE],
                                 const bool aIsOnTheLeft[WEBAUDIO_BLOCK_SIZE],
                                 float aOutputL[WEBAUDIO_BLOCK_SIZE],
                                 float aOutputR[WEBAUDIO_BLOCK_SIZE])
{
  ASSERT_ALIGNED(aInputL);
  ASSERT_ALIGNED(aInputR);
  ASSERT_ALIGNED(aGainL);
  ASSERT_ALIGNED(aGainR);
  ASSERT_ALIGNED(aIsOnTheLeft);
  ASSERT_ALIGNED(aOutputL);
  ASSERT_ALIGNED(aOutputR);

  float32x4_t vinL0, vinL1;
  float32x4_t vinR0, vinR1;
  float32x4_t voutL0, voutL1;
  float32x4_t voutR0, voutR1;
  float32x4_t vscaleL0, vscaleL1;
  float32x4_t vscaleR0, vscaleR1;
  float32x4_t onleft0, onleft1, notonleft0, notonleft1;

  float32x4_t zero = {0, 0, 0, 0};
  uint8x8_t isOnTheLeft;

  for (uint32_t i = 0; i < WEBAUDIO_BLOCK_SIZE; i+=8) {
    vinL0 = vld1q_f32(ADDRESS_OF(aInputL, i));
    vinL1 = vld1q_f32(ADDRESS_OF(aInputL, i+4));

    vinR0 = vld1q_f32(ADDRESS_OF(aInputR, i));
    vinR1 = vld1q_f32(ADDRESS_OF(aInputR, i+4));

    vscaleL0 = vld1q_f32(ADDRESS_OF(aGainL, i));
    vscaleL1 = vld1q_f32(ADDRESS_OF(aGainL, i+4));

    vscaleR0 = vld1q_f32(ADDRESS_OF(aGainR, i));
    vscaleR1 = vld1q_f32(ADDRESS_OF(aGainR, i+4));

    // Load output with boolean "on the left" values. This assumes that
    // bools are stored as a single byte.
    isOnTheLeft = vld1_u8((uint8_t *)&aIsOnTheLeft[i]);
    voutL0 = vsetq_lane_f32(vget_lane_u8(isOnTheLeft, 0), voutL0, 0);
    voutL0 = vsetq_lane_f32(vget_lane_u8(isOnTheLeft, 1), voutL0, 1);
    voutL0 = vsetq_lane_f32(vget_lane_u8(isOnTheLeft, 2), voutL0, 2);
    voutL0 = vsetq_lane_f32(vget_lane_u8(isOnTheLeft, 3), voutL0, 3);
    voutL1 = vsetq_lane_f32(vget_lane_u8(isOnTheLeft, 4), voutL1, 0);
    voutL1 = vsetq_lane_f32(vget_lane_u8(isOnTheLeft, 5), voutL1, 1);
    voutL1 = vsetq_lane_f32(vget_lane_u8(isOnTheLeft, 6), voutL1, 2);
    voutL1 = vsetq_lane_f32(vget_lane_u8(isOnTheLeft, 7), voutL1, 3);

    // Convert the boolean values into masks by setting all bits to 1
    // if true.
    voutL0 = (float32x4_t)vcgtq_f32(voutL0, zero);
    voutL1 = (float32x4_t)vcgtq_f32(voutL1, zero);

    // The right output masks are the same as the left masks
    voutR0 = voutL0;
    voutR1 = voutL1;

    // Calculate left channel assuming isOnTheLeft
    onleft0 = vmlaq_f32(vinL0, vinR0, vscaleL0);
    onleft1 = vmlaq_f32(vinL1, vinR1, vscaleL0);

    // Calculate left channel assuming not isOnTheLeft
    notonleft0 = vmulq_f32(vinL0, vscaleL0);
    notonleft1 = vmulq_f32(vinL1, vscaleL1);

    // Write results using previously stored masks
    voutL0 = vbslq_f32((uint32x4_t)voutL0, onleft0, notonleft0);
    voutL1 = vbslq_f32((uint32x4_t)voutL1, onleft1, notonleft1);

    // Calculate right channel assuming isOnTheLeft
    onleft0 = vmulq_f32(vinR0, vscaleR0);
    onleft1 = vmulq_f32(vinR1, vscaleR1);

    // Calculate right channel assuming not isOnTheLeft
    notonleft0 = vmlaq_f32(vinR0, vinL0, vscaleR0);
    notonleft1 = vmlaq_f32(vinR1, vinL1, vscaleR1);

    // Write results using previously stored masks
    voutR0 = vbslq_f32((uint32x4_t)voutR0, onleft0, notonleft0);
    voutR1 = vbslq_f32((uint32x4_t)voutR1, onleft1, notonleft1);

    vst1q_f32(ADDRESS_OF(aOutputL, i), voutL0);
    vst1q_f32(ADDRESS_OF(aOutputL, i+4), voutL1);
    vst1q_f32(ADDRESS_OF(aOutputR, i), voutR0);
    vst1q_f32(ADDRESS_OF(aOutputR, i+4), voutR1);
  }
}
}
