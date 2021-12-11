/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DynamicsCompressor_h
#define DynamicsCompressor_h

#include "DynamicsCompressorKernel.h"
#include "ZeroPole.h"

#include "nsTArray.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
class AudioBlock;
}  // namespace mozilla

namespace WebCore {

using mozilla::AudioBlock;
using mozilla::UniquePtr;

// DynamicsCompressor implements a flexible audio dynamics compression effect
// such as is commonly used in musical production and game audio. It lowers the
// volume of the loudest parts of the signal and raises the volume of the
// softest parts, making the sound richer, fuller, and more controlled.

class DynamicsCompressor {
 public:
  enum {
    ParamThreshold,
    ParamKnee,
    ParamRatio,
    ParamAttack,
    ParamRelease,
    ParamPreDelay,
    ParamReleaseZone1,
    ParamReleaseZone2,
    ParamReleaseZone3,
    ParamReleaseZone4,
    ParamPostGain,
    ParamFilterStageGain,
    ParamFilterStageRatio,
    ParamFilterAnchor,
    ParamEffectBlend,
    ParamReduction,
    ParamLast
  };

  DynamicsCompressor(float sampleRate, unsigned numberOfChannels);

  void process(const AudioBlock* sourceChunk, AudioBlock* destinationChunk,
               unsigned framesToProcess);
  void reset();
  void setNumberOfChannels(unsigned);
  unsigned numberOfChannels() const { return m_numberOfChannels; }

  void setParameterValue(unsigned parameterID, float value);
  float parameterValue(unsigned parameterID);

  float sampleRate() const { return m_sampleRate; }
  float nyquist() const { return m_sampleRate / 2; }

  double tailTime() const { return 0; }
  double latencyTime() const {
    return m_compressor.latencyFrames() / static_cast<double>(sampleRate());
  }

  size_t sizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 protected:
  unsigned m_numberOfChannels;

  // m_parameters holds the tweakable compressor parameters.
  float m_parameters[ParamLast];
  void initializeParameters();

  float m_sampleRate;

  // Emphasis filter controls.
  float m_lastFilterStageRatio;
  float m_lastAnchor;
  float m_lastFilterStageGain;

  struct ZeroPoleFilterPack4 {
    ZeroPole filters[4];
    size_t sizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
      return aMallocSizeOf(this);
    }
  };

  // Per-channel emphasis filters.
  nsTArray<UniquePtr<ZeroPoleFilterPack4> > m_preFilterPacks;
  nsTArray<UniquePtr<ZeroPoleFilterPack4> > m_postFilterPacks;

  mozilla::UniquePtr<const float*[]> m_sourceChannels;
  mozilla::UniquePtr<float*[]> m_destinationChannels;

  void setEmphasisStageParameters(unsigned stageIndex, float gain,
                                  float normalizedFrequency /* 0 -> 1 */);
  void setEmphasisParameters(float gain, float anchorFreq,
                             float filterStageRatio);

  // The core compressor.
  DynamicsCompressorKernel m_compressor;
};

}  // namespace WebCore

#endif  // DynamicsCompressor_h
