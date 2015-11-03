/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "ReverbConvolver.h"
#include "ReverbConvolverStage.h"

using namespace mozilla;

template<>
struct RunnableMethodTraits<WebCore::ReverbConvolver>
{
  static void RetainCallee(WebCore::ReverbConvolver* obj) {}
  static void ReleaseCallee(WebCore::ReverbConvolver* obj) {}
};

namespace WebCore {

const int InputBufferSize = 8 * 16384;

// We only process the leading portion of the impulse response in the real-time thread.  We don't exceed this length.
// It turns out then, that the background thread has about 278msec of scheduling slop.
// Empirically, this has been found to be a good compromise between giving enough time for scheduling slop,
// while still minimizing the amount of processing done in the primary (high-priority) thread.
// This was found to be a good value on Mac OS X, and may work well on other platforms as well, assuming
// the very rough scheduling latencies are similar on these time-scales.  Of course, this code may need to be
// tuned for individual platforms if this assumption is found to be incorrect.
const size_t RealtimeFrameLimit = 8192  + 4096; // ~278msec @ 44.1KHz

const size_t MinFFTSize = 128;
const size_t MaxRealtimeFFTSize = 2048;

ReverbConvolver::ReverbConvolver(const float* impulseResponseData, size_t impulseResponseLength, size_t renderSliceSize, size_t maxFFTSize, size_t convolverRenderPhase, bool useBackgroundThreads)
    : m_impulseResponseLength(impulseResponseLength)
    , m_accumulationBuffer(impulseResponseLength + renderSliceSize)
    , m_inputBuffer(InputBufferSize)
    , m_minFFTSize(MinFFTSize) // First stage will have this size - successive stages will double in size each time
    , m_maxFFTSize(maxFFTSize) // until we hit m_maxFFTSize
    , m_backgroundThread("ConvolverWorker")
    , m_backgroundThreadCondition(&m_backgroundThreadLock)
    , m_useBackgroundThreads(useBackgroundThreads)
    , m_wantsToExit(false)
    , m_moreInputBuffered(false)
{
    // If we are using background threads then don't exceed this FFT size for the
    // stages which run in the real-time thread.  This avoids having only one or two
    // large stages (size 16384 or so) at the end which take a lot of time every several
    // processing slices.  This way we amortize the cost over more processing slices.
    m_maxRealtimeFFTSize = MaxRealtimeFFTSize;

    // For the moment, a good way to know if we have real-time constraint is to check if we're using background threads.
    // Otherwise, assume we're being run from a command-line tool.
    bool hasRealtimeConstraint = useBackgroundThreads;

    const float* response = impulseResponseData;
    size_t totalResponseLength = impulseResponseLength;

    // The total latency is zero because the direct-convolution is used in the leading portion.
    size_t reverbTotalLatency = 0;

    size_t stageOffset = 0;
    int i = 0;
    size_t fftSize = m_minFFTSize;
    while (stageOffset < totalResponseLength) {
        size_t stageSize = fftSize / 2;

        // For the last stage, it's possible that stageOffset is such that we're straddling the end
        // of the impulse response buffer (if we use stageSize), so reduce the last stage's length...
        if (stageSize + stageOffset > totalResponseLength)
            stageSize = totalResponseLength - stageOffset;

        // This "staggers" the time when each FFT happens so they don't all happen at the same time
        int renderPhase = convolverRenderPhase + i * renderSliceSize;

        bool useDirectConvolver = !stageOffset;

        nsAutoPtr<ReverbConvolverStage> stage(new ReverbConvolverStage(response, totalResponseLength, reverbTotalLatency, stageOffset, stageSize, fftSize, renderPhase, renderSliceSize, &m_accumulationBuffer, useDirectConvolver));

        bool isBackgroundStage = false;

        if (this->useBackgroundThreads() && stageOffset > RealtimeFrameLimit) {
            m_backgroundStages.AppendElement(stage.forget());
            isBackgroundStage = true;
        } else
            m_stages.AppendElement(stage.forget());

        stageOffset += stageSize;
        ++i;

        if (!useDirectConvolver) {
            // Figure out next FFT size
            fftSize *= 2;
        }

        if (hasRealtimeConstraint && !isBackgroundStage && fftSize > m_maxRealtimeFFTSize)
            fftSize = m_maxRealtimeFFTSize;
        if (fftSize > m_maxFFTSize)
            fftSize = m_maxFFTSize;
    }

    // Start up background thread
    // FIXME: would be better to up the thread priority here.  It doesn't need to be real-time, but higher than the default...
    if (this->useBackgroundThreads() && m_backgroundStages.Length() > 0) {
        if (!m_backgroundThread.Start()) {
          NS_WARNING("Cannot start convolver thread.");
          return;
        }
        CancelableTask* task = NewRunnableMethod(this, &ReverbConvolver::backgroundThreadEntry);
        m_backgroundThread.message_loop()->PostTask(FROM_HERE, task);
    }
}

ReverbConvolver::~ReverbConvolver()
{
    // Wait for background thread to stop
    if (useBackgroundThreads() && m_backgroundThread.IsRunning()) {
        m_wantsToExit = true;

        // Wake up thread so it can return
        {
            AutoLock locker(m_backgroundThreadLock);
            m_moreInputBuffered = true;
            m_backgroundThreadCondition.Signal();
        }

        m_backgroundThread.Stop();
    }
}

size_t ReverbConvolver::sizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
    size_t amount = aMallocSizeOf(this);
    amount += m_stages.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (size_t i = 0; i < m_stages.Length(); i++) {
        if (m_stages[i]) {
            amount += m_stages[i]->sizeOfIncludingThis(aMallocSizeOf);
        }
    }

    amount += m_backgroundStages.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (size_t i = 0; i < m_backgroundStages.Length(); i++) {
        if (m_backgroundStages[i]) {
            amount += m_backgroundStages[i]->sizeOfIncludingThis(aMallocSizeOf);
        }
    }

    // NB: The buffer sizes are static, so even though they might be accessed
    //     in another thread it's safe to measure them.
    amount += m_accumulationBuffer.sizeOfExcludingThis(aMallocSizeOf);
    amount += m_inputBuffer.sizeOfExcludingThis(aMallocSizeOf);

    // Possible future measurements:
    // - m_backgroundThread
    // - m_backgroundThreadLock
    // - m_backgroundThreadCondition
    return amount;
}

void ReverbConvolver::backgroundThreadEntry()
{
    while (!m_wantsToExit) {
        // Wait for realtime thread to give us more input
        m_moreInputBuffered = false;
        {
            AutoLock locker(m_backgroundThreadLock);
            while (!m_moreInputBuffered && !m_wantsToExit)
                m_backgroundThreadCondition.Wait();
        }

        // Process all of the stages until their read indices reach the input buffer's write index
        int writeIndex = m_inputBuffer.writeIndex();

        // Even though it doesn't seem like every stage needs to maintain its own version of readIndex 
        // we do this in case we want to run in more than one background thread.
        int readIndex;

        while ((readIndex = m_backgroundStages[0]->inputReadIndex()) != writeIndex) { // FIXME: do better to detect buffer overrun...
            // The ReverbConvolverStages need to process in amounts which evenly divide half the FFT size
            const int SliceSize = MinFFTSize / 2;

            // Accumulate contributions from each stage
            for (size_t i = 0; i < m_backgroundStages.Length(); ++i)
                m_backgroundStages[i]->processInBackground(this, SliceSize);
        }
    }
}

void ReverbConvolver::process(const float* sourceChannelData, size_t sourceChannelLength,
                              float* destinationChannelData, size_t destinationChannelLength,
                              size_t framesToProcess)
{
    bool isSafe = sourceChannelData && destinationChannelData && sourceChannelLength >= framesToProcess && destinationChannelLength >= framesToProcess;
    MOZ_ASSERT(isSafe);
    if (!isSafe)
        return;

    const float* source = sourceChannelData;
    float* destination = destinationChannelData;
    bool isDataSafe = source && destination;
    MOZ_ASSERT(isDataSafe);
    if (!isDataSafe)
        return;

    // Feed input buffer (read by all threads)
    m_inputBuffer.write(source, framesToProcess);

    // Accumulate contributions from each stage
    for (size_t i = 0; i < m_stages.Length(); ++i)
        m_stages[i]->process(source, framesToProcess);

    // Finally read from accumulation buffer
    m_accumulationBuffer.readAndClear(destination, framesToProcess);

    // Now that we've buffered more input, wake up our background thread.

    // Not using a MutexLocker looks strange, but we use a tryLock() instead because this is run on the real-time
    // thread where it is a disaster for the lock to be contended (causes audio glitching).  It's OK if we fail to
    // signal from time to time, since we'll get to it the next time we're called.  We're called repeatedly
    // and frequently (around every 3ms).  The background thread is processing well into the future and has a considerable amount of 
    // leeway here...
    if (m_backgroundThreadLock.Try()) {
        m_moreInputBuffered = true;
        m_backgroundThreadCondition.Signal();
        m_backgroundThreadLock.Release();
    }
}

} // namespace WebCore
