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

#include "DynamicsCompressor.h"
#include "AlignmentUtils.h"
#include "AudioBlock.h"

#include <cmath>
#include "AudioNodeEngine.h"
#include "nsDebug.h"

using mozilla::WEBAUDIO_BLOCK_SIZE;
using mozilla::AudioBlockCopyChannelWithScale;

namespace WebCore {

DynamicsCompressor::DynamicsCompressor(float sampleRate, unsigned numberOfChannels)
    : m_numberOfChannels(numberOfChannels)
    , m_sampleRate(sampleRate)
    , m_compressor(sampleRate, numberOfChannels)
{
    // Uninitialized state - for parameter recalculation.
    m_lastFilterStageRatio = -1;
    m_lastAnchor = -1;
    m_lastFilterStageGain = -1;

    setNumberOfChannels(numberOfChannels);
    initializeParameters();
}

size_t DynamicsCompressor::sizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
    size_t amount = aMallocSizeOf(this);
    amount += m_preFilterPacks.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (size_t i = 0; i < m_preFilterPacks.Length(); i++) {
        if (m_preFilterPacks[i]) {
            amount += m_preFilterPacks[i]->sizeOfIncludingThis(aMallocSizeOf);
        }
    }

    amount += m_postFilterPacks.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (size_t i = 0; i < m_postFilterPacks.Length(); i++) {
        if (m_postFilterPacks[i]) {
            amount += m_postFilterPacks[i]->sizeOfIncludingThis(aMallocSizeOf);
        }
    }

    amount += aMallocSizeOf(m_sourceChannels.get());
    amount += aMallocSizeOf(m_destinationChannels.get());
    amount += m_compressor.sizeOfExcludingThis(aMallocSizeOf);
    return amount;
}

void DynamicsCompressor::setParameterValue(unsigned parameterID, float value)
{
    MOZ_ASSERT(parameterID < ParamLast);
    if (parameterID < ParamLast)
        m_parameters[parameterID] = value;
}

void DynamicsCompressor::initializeParameters()
{
    // Initializes compressor to default values.

    m_parameters[ParamThreshold] = -24; // dB
    m_parameters[ParamKnee] = 30; // dB
    m_parameters[ParamRatio] = 12; // unit-less
    m_parameters[ParamAttack] = 0.003f; // seconds
    m_parameters[ParamRelease] = 0.250f; // seconds
    m_parameters[ParamPreDelay] = 0.006f; // seconds

    // Release zone values 0 -> 1.
    m_parameters[ParamReleaseZone1] = 0.09f;
    m_parameters[ParamReleaseZone2] = 0.16f;
    m_parameters[ParamReleaseZone3] = 0.42f;
    m_parameters[ParamReleaseZone4] = 0.98f;

    m_parameters[ParamFilterStageGain] = 4.4f; // dB
    m_parameters[ParamFilterStageRatio] = 2;
    m_parameters[ParamFilterAnchor] = 15000 / nyquist();

    m_parameters[ParamPostGain] = 0; // dB
    m_parameters[ParamReduction] = 0; // dB

    // Linear crossfade (0 -> 1).
    m_parameters[ParamEffectBlend] = 1;
}

float DynamicsCompressor::parameterValue(unsigned parameterID)
{
    MOZ_ASSERT(parameterID < ParamLast);
    return m_parameters[parameterID];
}

void DynamicsCompressor::setEmphasisStageParameters(unsigned stageIndex, float gain, float normalizedFrequency /* 0 -> 1 */)
{
    float gk = 1 - gain / 20;
    float f1 = normalizedFrequency * gk;
    float f2 = normalizedFrequency / gk;
    float r1 = expf(-f1 * M_PI);
    float r2 = expf(-f2 * M_PI);

    MOZ_ASSERT(m_numberOfChannels == m_preFilterPacks.Length());

    for (unsigned i = 0; i < m_numberOfChannels; ++i) {
        // Set pre-filter zero and pole to create an emphasis filter.
        ZeroPole& preFilter = m_preFilterPacks[i]->filters[stageIndex];
        preFilter.setZero(r1);
        preFilter.setPole(r2);

        // Set post-filter with zero and pole reversed to create the de-emphasis filter.
        // If there were no compressor kernel in between, they would cancel each other out (allpass filter).
        ZeroPole& postFilter = m_postFilterPacks[i]->filters[stageIndex];
        postFilter.setZero(r2);
        postFilter.setPole(r1);
    }
}

void DynamicsCompressor::setEmphasisParameters(float gain, float anchorFreq, float filterStageRatio)
{
    setEmphasisStageParameters(0, gain, anchorFreq);
    setEmphasisStageParameters(1, gain, anchorFreq / filterStageRatio);
    setEmphasisStageParameters(2, gain, anchorFreq / (filterStageRatio * filterStageRatio));
    setEmphasisStageParameters(3, gain, anchorFreq / (filterStageRatio * filterStageRatio * filterStageRatio));
}

void DynamicsCompressor::process(const AudioBlock* sourceChunk, AudioBlock* destinationChunk, unsigned framesToProcess)
{
    // Though numberOfChannels is retrived from destinationBus, we still name it numberOfChannels instead of numberOfDestinationChannels.
    // It's because we internally match sourceChannels's size to destinationBus by channel up/down mix. Thus we need numberOfChannels
    // to do the loop work for both m_sourceChannels and m_destinationChannels.

    unsigned numberOfChannels = destinationChunk->ChannelCount();
    unsigned numberOfSourceChannels = sourceChunk->ChannelCount();

    MOZ_ASSERT(numberOfChannels == m_numberOfChannels && numberOfSourceChannels);

    if (numberOfChannels != m_numberOfChannels || !numberOfSourceChannels) {
        destinationChunk->SetNull(WEBAUDIO_BLOCK_SIZE);
        return;
    }

    switch (numberOfChannels) {
    case 2: // stereo
        m_sourceChannels[0] = static_cast<const float*>(sourceChunk->mChannelData[0]);

        if (numberOfSourceChannels > 1)
            m_sourceChannels[1] = static_cast<const float*>(sourceChunk->mChannelData[1]);
        else
            // Simply duplicate mono channel input data to right channel for stereo processing.
            m_sourceChannels[1] = m_sourceChannels[0];

        break;
    default:
        // FIXME : support other number of channels.
        NS_WARNING("Support other number of channels");
        destinationChunk->SetNull(WEBAUDIO_BLOCK_SIZE);
        return;
    }

    for (unsigned i = 0; i < numberOfChannels; ++i)
        m_destinationChannels[i] = const_cast<float*>(static_cast<const float*>(
            destinationChunk->mChannelData[i]));

    float filterStageGain = parameterValue(ParamFilterStageGain);
    float filterStageRatio = parameterValue(ParamFilterStageRatio);
    float anchor = parameterValue(ParamFilterAnchor);

    if (filterStageGain != m_lastFilterStageGain || filterStageRatio != m_lastFilterStageRatio || anchor != m_lastAnchor) {
        m_lastFilterStageGain = filterStageGain;
        m_lastFilterStageRatio = filterStageRatio;
        m_lastAnchor = anchor;

        setEmphasisParameters(filterStageGain, anchor, filterStageRatio);
    }

    float sourceWithVolume[WEBAUDIO_BLOCK_SIZE + 4];
    float* alignedSourceWithVolume = ALIGNED16(sourceWithVolume);
    ASSERT_ALIGNED16(alignedSourceWithVolume);

    // Apply pre-emphasis filter.
    // Note that the final three stages are computed in-place in the destination buffer.
    for (unsigned i = 0; i < numberOfChannels; ++i) {
        const float* sourceData;
        if (sourceChunk->mVolume == 1.0f) {
          // Fast path, the volume scale doesn't need to get taken into account
          sourceData = m_sourceChannels[i];
        } else {
          AudioBlockCopyChannelWithScale(m_sourceChannels[i],
                                         sourceChunk->mVolume,
                                         alignedSourceWithVolume);
          sourceData = alignedSourceWithVolume;
        }

        float* destinationData = m_destinationChannels[i];
        ZeroPole* preFilters = m_preFilterPacks[i]->filters;

        preFilters[0].process(sourceData, destinationData, framesToProcess);
        preFilters[1].process(destinationData, destinationData, framesToProcess);
        preFilters[2].process(destinationData, destinationData, framesToProcess);
        preFilters[3].process(destinationData, destinationData, framesToProcess);
    }

    float dbThreshold = parameterValue(ParamThreshold);
    float dbKnee = parameterValue(ParamKnee);
    float ratio = parameterValue(ParamRatio);
    float attackTime = parameterValue(ParamAttack);
    float releaseTime = parameterValue(ParamRelease);
    float preDelayTime = parameterValue(ParamPreDelay);

    // This is effectively a master volume on the compressed signal (pre-blending).
    float dbPostGain = parameterValue(ParamPostGain);

    // Linear blending value from dry to completely processed (0 -> 1)
    // 0 means the signal is completely unprocessed.
    // 1 mixes in only the compressed signal.
    float effectBlend = parameterValue(ParamEffectBlend);

    float releaseZone1 = parameterValue(ParamReleaseZone1);
    float releaseZone2 = parameterValue(ParamReleaseZone2);
    float releaseZone3 = parameterValue(ParamReleaseZone3);
    float releaseZone4 = parameterValue(ParamReleaseZone4);

    // Apply compression to the pre-filtered signal.
    // The processing is performed in place.
    m_compressor.process(m_destinationChannels.get(),
                         m_destinationChannels.get(),
                         numberOfChannels,
                         framesToProcess,

                         dbThreshold,
                         dbKnee,
                         ratio,
                         attackTime,
                         releaseTime,
                         preDelayTime,
                         dbPostGain,
                         effectBlend,

                         releaseZone1,
                         releaseZone2,
                         releaseZone3,
                         releaseZone4
                         );

    // Update the compression amount.
    setParameterValue(ParamReduction, m_compressor.meteringGain());

    // Apply de-emphasis filter.
    for (unsigned i = 0; i < numberOfChannels; ++i) {
        float* destinationData = m_destinationChannels[i];
        ZeroPole* postFilters = m_postFilterPacks[i]->filters;

        postFilters[0].process(destinationData, destinationData, framesToProcess);
        postFilters[1].process(destinationData, destinationData, framesToProcess);
        postFilters[2].process(destinationData, destinationData, framesToProcess);
        postFilters[3].process(destinationData, destinationData, framesToProcess);
    }
}

void DynamicsCompressor::reset()
{
    m_lastFilterStageRatio = -1; // for recalc
    m_lastAnchor = -1;
    m_lastFilterStageGain = -1;

    for (unsigned channel = 0; channel < m_numberOfChannels; ++channel) {
        for (unsigned stageIndex = 0; stageIndex < 4; ++stageIndex) {
            m_preFilterPacks[channel]->filters[stageIndex].reset();
            m_postFilterPacks[channel]->filters[stageIndex].reset();
        }
    }

    m_compressor.reset();
}

void DynamicsCompressor::setNumberOfChannels(unsigned numberOfChannels)
{
    if (m_preFilterPacks.Length() == numberOfChannels)
        return;

    m_preFilterPacks.Clear();
    m_postFilterPacks.Clear();
    for (unsigned i = 0; i < numberOfChannels; ++i) {
        m_preFilterPacks.AppendElement(new ZeroPoleFilterPack4());
        m_postFilterPacks.AppendElement(new ZeroPoleFilterPack4());
    }

    m_sourceChannels = mozilla::MakeUnique<const float* []>(numberOfChannels);
    m_destinationChannels = mozilla::MakeUnique<float* []>(numberOfChannels);

    m_compressor.setNumberOfChannels(numberOfChannels);
    m_numberOfChannels = numberOfChannels;
}

} // namespace WebCore
