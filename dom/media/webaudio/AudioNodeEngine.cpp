/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioNodeEngine.h"

#include "mozilla/AbstractThread.h"
#ifdef USE_NEON
#  include "mozilla/arm.h"
#  include "AudioNodeEngineNEON.h"
#endif
#ifdef USE_SSE2
#  include "mozilla/SSE.h"
#  include "AlignmentUtils.h"
#  include "AudioNodeEngineSSE2.h"
#endif
#include "AudioBlock.h"

namespace mozilla {

already_AddRefed<ThreadSharedFloatArrayBufferList>
ThreadSharedFloatArrayBufferList::Create(uint32_t aChannelCount, size_t aLength,
                                         const mozilla::fallible_t&) {
  RefPtr<ThreadSharedFloatArrayBufferList> buffer =
      new ThreadSharedFloatArrayBufferList(aChannelCount);

  for (uint32_t i = 0; i < aChannelCount; ++i) {
    float* channelData = js_pod_malloc<float>(aLength);
    if (!channelData) {
      return nullptr;
    }

    buffer->SetData(i, channelData, js_free, channelData);
  }

  return buffer.forget();
}

void WriteZeroesToAudioBlock(AudioBlock* aChunk, uint32_t aStart,
                             uint32_t aLength) {
  MOZ_ASSERT(aStart + aLength <= WEBAUDIO_BLOCK_SIZE);
  MOZ_ASSERT(!aChunk->IsNull(), "You should pass a non-null chunk");
  if (aLength == 0) {
    return;
  }

  for (uint32_t i = 0; i < aChunk->ChannelCount(); ++i) {
    PodZero(aChunk->ChannelFloatsForWrite(i) + aStart, aLength);
  }
}

void AudioBufferCopyWithScale(const float* aInput, float aScale, float* aOutput,
                              uint32_t aSize) {
  if (aScale == 1.0f) {
    PodCopy(aOutput, aInput, aSize);
  } else {
    for (uint32_t i = 0; i < aSize; ++i) {
      aOutput[i] = aInput[i] * aScale;
    }
  }
}

void AudioBufferAddWithScale(const float* aInput, float aScale, float* aOutput,
                             uint32_t aSize) {
#ifdef USE_NEON
  if (mozilla::supports_neon()) {
    AudioBufferAddWithScale_NEON(aInput, aScale, aOutput, aSize);
    return;
  }
#endif

#ifdef USE_SSE2
  if (mozilla::supports_sse2()) {
    if (aScale == 1.0f) {
      while (aSize && (!IS_ALIGNED16(aInput) || !IS_ALIGNED16(aOutput))) {
        *aOutput += *aInput;
        ++aOutput;
        ++aInput;
        --aSize;
      }
    } else {
      while (aSize && (!IS_ALIGNED16(aInput) || !IS_ALIGNED16(aOutput))) {
        *aOutput += *aInput * aScale;
        ++aOutput;
        ++aInput;
        --aSize;
      }
    }

    // we need to round aSize down to the nearest multiple of 16
    uint32_t alignedSize = aSize & ~0x0F;
    if (alignedSize > 0) {
      AudioBufferAddWithScale_SSE(aInput, aScale, aOutput, alignedSize);

      // adjust parameters for use with scalar operations below
      aInput += alignedSize;
      aOutput += alignedSize;
      aSize -= alignedSize;
    }
  }
#endif

  if (aScale == 1.0f) {
    for (uint32_t i = 0; i < aSize; ++i) {
      aOutput[i] += aInput[i];
    }
  } else {
    for (uint32_t i = 0; i < aSize; ++i) {
      aOutput[i] += aInput[i] * aScale;
    }
  }
}

void AudioBlockAddChannelWithScale(const float aInput[WEBAUDIO_BLOCK_SIZE],
                                   float aScale,
                                   float aOutput[WEBAUDIO_BLOCK_SIZE]) {
  AudioBufferAddWithScale(aInput, aScale, aOutput, WEBAUDIO_BLOCK_SIZE);
}

void AudioBlockCopyChannelWithScale(const float* aInput, float aScale,
                                    float* aOutput) {
  if (aScale == 1.0f) {
    memcpy(aOutput, aInput, WEBAUDIO_BLOCK_SIZE * sizeof(float));
  } else {
#ifdef USE_NEON
    if (mozilla::supports_neon()) {
      AudioBlockCopyChannelWithScale_NEON(aInput, aScale, aOutput);
      return;
    }
#endif

#ifdef USE_SSE2
    if (mozilla::supports_sse2()) {
      AudioBlockCopyChannelWithScale_SSE(aInput, aScale, aOutput);
      return;
    }
#endif

    for (uint32_t i = 0; i < WEBAUDIO_BLOCK_SIZE; ++i) {
      aOutput[i] = aInput[i] * aScale;
    }
  }
}

void BufferComplexMultiply(const float* aInput, const float* aScale,
                           float* aOutput, uint32_t aSize) {
#ifdef USE_SSE2
  if (mozilla::supports_sse()) {
    BufferComplexMultiply_SSE(aInput, aScale, aOutput, aSize);
    return;
  }
#endif

  for (uint32_t i = 0; i < aSize * 2; i += 2) {
    float real1 = aInput[i];
    float imag1 = aInput[i + 1];
    float real2 = aScale[i];
    float imag2 = aScale[i + 1];
    float realResult = real1 * real2 - imag1 * imag2;
    float imagResult = real1 * imag2 + imag1 * real2;
    aOutput[i] = realResult;
    aOutput[i + 1] = imagResult;
  }
}

float AudioBufferPeakValue(const float* aInput, uint32_t aSize) {
  float max = 0.0f;
  for (uint32_t i = 0; i < aSize; i++) {
    float mag = fabs(aInput[i]);
    if (mag > max) {
      max = mag;
    }
  }
  return max;
}

void AudioBlockCopyChannelWithScale(const float aInput[WEBAUDIO_BLOCK_SIZE],
                                    const float aScale[WEBAUDIO_BLOCK_SIZE],
                                    float aOutput[WEBAUDIO_BLOCK_SIZE]) {
#ifdef USE_NEON
  if (mozilla::supports_neon()) {
    AudioBlockCopyChannelWithScale_NEON(aInput, aScale, aOutput);
    return;
  }
#endif

#ifdef USE_SSE2
  if (mozilla::supports_sse2()) {
    AudioBlockCopyChannelWithScale_SSE(aInput, aScale, aOutput);
    return;
  }
#endif

  for (uint32_t i = 0; i < WEBAUDIO_BLOCK_SIZE; ++i) {
    aOutput[i] = aInput[i] * aScale[i];
  }
}

void AudioBlockInPlaceScale(float aBlock[WEBAUDIO_BLOCK_SIZE], float aScale) {
  AudioBufferInPlaceScale(aBlock, aScale, WEBAUDIO_BLOCK_SIZE);
}

void AudioBlockInPlaceScale(float aBlock[WEBAUDIO_BLOCK_SIZE],
                            float aScale[WEBAUDIO_BLOCK_SIZE]) {
  AudioBufferInPlaceScale(aBlock, aScale, WEBAUDIO_BLOCK_SIZE);
}

void AudioBufferInPlaceScale(float* aBlock, float aScale, uint32_t aSize) {
  if (aScale == 1.0f) {
    return;
  }
#ifdef USE_NEON
  if (mozilla::supports_neon()) {
    AudioBufferInPlaceScale_NEON(aBlock, aScale, aSize);
    return;
  }
#endif

#ifdef USE_SSE2
  if (mozilla::supports_sse2()) {
    AudioBufferInPlaceScale_SSE(aBlock, aScale, aSize);
    return;
  }
#endif

  for (uint32_t i = 0; i < aSize; ++i) {
    *aBlock++ *= aScale;
  }
}

void AudioBufferInPlaceScale(float* aBlock, float* aScale, uint32_t aSize) {
#ifdef USE_NEON
  if (mozilla::supports_neon()) {
    AudioBufferInPlaceScale_NEON(aBlock, aScale, aSize);
    return;
  }
#endif

#ifdef USE_SSE2
  if (mozilla::supports_sse2()) {
    AudioBufferInPlaceScale_SSE(aBlock, aScale, aSize);
    return;
  }
#endif

  for (uint32_t i = 0; i < aSize; ++i) {
    *aBlock++ *= *aScale++;
  }
}

void AudioBlockPanMonoToStereo(const float aInput[WEBAUDIO_BLOCK_SIZE],
                               float aGainL[WEBAUDIO_BLOCK_SIZE],
                               float aGainR[WEBAUDIO_BLOCK_SIZE],
                               float aOutputL[WEBAUDIO_BLOCK_SIZE],
                               float aOutputR[WEBAUDIO_BLOCK_SIZE]) {
  AudioBlockCopyChannelWithScale(aInput, aGainL, aOutputL);
  AudioBlockCopyChannelWithScale(aInput, aGainR, aOutputR);
}

void AudioBlockPanMonoToStereo(const float aInput[WEBAUDIO_BLOCK_SIZE],
                               float aGainL, float aGainR,
                               float aOutputL[WEBAUDIO_BLOCK_SIZE],
                               float aOutputR[WEBAUDIO_BLOCK_SIZE]) {
  AudioBlockCopyChannelWithScale(aInput, aGainL, aOutputL);
  AudioBlockCopyChannelWithScale(aInput, aGainR, aOutputR);
}

void AudioBlockPanStereoToStereo(const float aInputL[WEBAUDIO_BLOCK_SIZE],
                                 const float aInputR[WEBAUDIO_BLOCK_SIZE],
                                 float aGainL, float aGainR, bool aIsOnTheLeft,
                                 float aOutputL[WEBAUDIO_BLOCK_SIZE],
                                 float aOutputR[WEBAUDIO_BLOCK_SIZE]) {
#ifdef USE_NEON
  if (mozilla::supports_neon()) {
    AudioBlockPanStereoToStereo_NEON(aInputL, aInputR, aGainL, aGainR,
                                     aIsOnTheLeft, aOutputL, aOutputR);
    return;
  }
#endif

#ifdef USE_SSE2
  if (mozilla::supports_sse2()) {
    AudioBlockPanStereoToStereo_SSE(aInputL, aInputR, aGainL, aGainR,
                                    aIsOnTheLeft, aOutputL, aOutputR);
    return;
  }
#endif

  uint32_t i;

  if (aIsOnTheLeft) {
    for (i = 0; i < WEBAUDIO_BLOCK_SIZE; ++i) {
      aOutputL[i] = aInputL[i] + aInputR[i] * aGainL;
      aOutputR[i] = aInputR[i] * aGainR;
    }
  } else {
    for (i = 0; i < WEBAUDIO_BLOCK_SIZE; ++i) {
      aOutputL[i] = aInputL[i] * aGainL;
      aOutputR[i] = aInputR[i] + aInputL[i] * aGainR;
    }
  }
}

void AudioBlockPanStereoToStereo(const float aInputL[WEBAUDIO_BLOCK_SIZE],
                                 const float aInputR[WEBAUDIO_BLOCK_SIZE],
                                 const float aGainL[WEBAUDIO_BLOCK_SIZE],
                                 const float aGainR[WEBAUDIO_BLOCK_SIZE],
                                 const bool aIsOnTheLeft[WEBAUDIO_BLOCK_SIZE],
                                 float aOutputL[WEBAUDIO_BLOCK_SIZE],
                                 float aOutputR[WEBAUDIO_BLOCK_SIZE]) {
#ifdef USE_NEON
  if (mozilla::supports_neon()) {
    AudioBlockPanStereoToStereo_NEON(aInputL, aInputR, aGainL, aGainR,
                                     aIsOnTheLeft, aOutputL, aOutputR);
    return;
  }
#endif

  uint32_t i;
  for (i = 0; i < WEBAUDIO_BLOCK_SIZE; i++) {
    if (aIsOnTheLeft[i]) {
      aOutputL[i] = aInputL[i] + aInputR[i] * aGainL[i];
      aOutputR[i] = aInputR[i] * aGainR[i];
    } else {
      aOutputL[i] = aInputL[i] * aGainL[i];
      aOutputR[i] = aInputR[i] + aInputL[i] * aGainR[i];
    }
  }
}

float AudioBufferSumOfSquares(const float* aInput, uint32_t aLength) {
  float sum = 0.0f;

#ifdef USE_SSE2
  if (mozilla::supports_sse()) {
    const float* alignedInput = ALIGNED16(aInput);

    // use scalar operations for any unaligned data at the beginning
    while (aInput != alignedInput) {
      if (!aLength) {
        return sum;
      }
      sum += *aInput * *aInput;
      ++aInput;
      --aLength;
    }

    uint32_t vLength = (aLength >> 4) << 4;
    sum += AudioBufferSumOfSquares_SSE(alignedInput, vLength);

    // adjust aInput and aLength to use scalar operations for any
    // remaining values
    aInput = alignedInput + vLength;
    aLength -= vLength;
  }
#endif

  while (aLength--) {
    sum += *aInput * *aInput;
    ++aInput;
  }
  return sum;
}

void NaNToZeroInPlace(float* aSamples, size_t aCount) {
#ifdef USE_SSE2
  if (mozilla::supports_sse2()) {
    NaNToZeroInPlace_SSE(aSamples, aCount);
    return;
  }
#endif
  for (size_t i = 0; i < aCount; i++) {
    if (aSamples[i] != aSamples[i]) {
      aSamples[i] = 0.0;
    }
  }
}

AudioNodeEngine::AudioNodeEngine(dom::AudioNode* aNode)
    : mNode(aNode),
      mNodeType(aNode ? aNode->NodeType() : nullptr),
      mInputCount(aNode ? aNode->NumberOfInputs() : 1),
      mOutputCount(aNode ? aNode->NumberOfOutputs() : 0),
      mAbstractMainThread(aNode && aNode->GetAbstractMainThread()
                              ? aNode->GetAbstractMainThread()
                              : AbstractThread::MainThread()) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(AudioNodeEngine);
}

void AudioNodeEngine::ProcessBlock(AudioNodeTrack* aTrack, GraphTime aFrom,
                                   const AudioBlock& aInput,
                                   AudioBlock* aOutput, bool* aFinished) {
  MOZ_ASSERT(mInputCount <= 1 && mOutputCount <= 1);
  *aOutput = aInput;
}

void AudioNodeEngine::ProcessBlocksOnPorts(AudioNodeTrack* aTrack,
                                           GraphTime aFrom,
                                           Span<const AudioBlock> aInput,
                                           Span<AudioBlock> aOutput,
                                           bool* aFinished) {
  MOZ_ASSERT(mInputCount > 1 || mOutputCount > 1);
  // Only produce one output port, and drop all other input ports.
  aOutput[0] = aInput[0];
}

}  // namespace mozilla
