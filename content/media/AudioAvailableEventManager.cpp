/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioAvailableEventManager.h"
#include "VideoUtils.h"
#include "MediaDecoder.h"
#include "nsIRunnable.h"

namespace mozilla {

static const nsTArray< nsCOMPtr<nsIRunnable> >::size_type MAX_PENDING_EVENTS = 100;

class nsAudioAvailableEventRunner : public nsRunnable
{
private:
  nsRefPtr<MediaDecoder> mDecoder;
  nsAutoArrayPtr<float> mFrameBuffer;
public:
  nsAudioAvailableEventRunner(MediaDecoder* aDecoder, float* aFrameBuffer,
                              uint32_t aFrameBufferLength, float aTime) :
    mDecoder(aDecoder),
    mFrameBuffer(aFrameBuffer),
    mFrameBufferLength(aFrameBufferLength),
    mTime(aTime)
  {
    MOZ_COUNT_CTOR(nsAudioAvailableEventRunner);
  }

  ~nsAudioAvailableEventRunner() {
    MOZ_COUNT_DTOR(nsAudioAvailableEventRunner);
  }

  NS_IMETHOD Run()
  {
    mDecoder->AudioAvailable(mFrameBuffer.forget(), mFrameBufferLength, mTime);
    return NS_OK;
  }

  const uint32_t mFrameBufferLength;

  // Start time of the buffer data (in seconds).
  const float mTime;
};


AudioAvailableEventManager::AudioAvailableEventManager(MediaDecoder* aDecoder) :
  mDecoder(aDecoder),
  mSignalBuffer(new float[mDecoder->GetFrameBufferLength()]),
  mSignalBufferLength(mDecoder->GetFrameBufferLength()),
  mNewSignalBufferLength(mSignalBufferLength),
  mSignalBufferPosition(0),
  mReentrantMonitor("media.audioavailableeventmanager"),
  mHasListener(false)
{
  MOZ_COUNT_CTOR(AudioAvailableEventManager);
}

AudioAvailableEventManager::~AudioAvailableEventManager()
{
  MOZ_COUNT_DTOR(AudioAvailableEventManager);
}

void AudioAvailableEventManager::Init(uint32_t aChannels, uint32_t aRate)
{
  NS_ASSERTION(aChannels != 0 && aRate != 0, "Audio metadata not known.");
  mSamplesPerSecond = static_cast<float>(aChannels * aRate);
}

void AudioAvailableEventManager::DispatchPendingEvents(uint64_t aCurrentTime)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (!mHasListener) {
    return;
  }

  while (mPendingEvents.Length() > 0) {
    nsAudioAvailableEventRunner* e =
      (nsAudioAvailableEventRunner*)mPendingEvents[0].get();
    if (e->mTime * USECS_PER_S > aCurrentTime) {
      break;
    }
    nsCOMPtr<nsIRunnable> event = mPendingEvents[0];
    mPendingEvents.RemoveElementAt(0);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }
}

void AudioAvailableEventManager::QueueWrittenAudioData(AudioDataValue* aAudioData,
                                                         uint32_t aAudioDataLength,
                                                         uint64_t aEndTimeSampleOffset)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (!mHasListener) {
    return;
  }

  uint32_t currentBufferSize = mNewSignalBufferLength;
  if (currentBufferSize == 0) {
    NS_WARNING("Decoder framebuffer length not set.");
    return;
  }

  if (!mSignalBuffer ||
      (mSignalBufferPosition == 0 && mSignalBufferLength != currentBufferSize)) {
    if (!mSignalBuffer || (mSignalBufferLength < currentBufferSize)) {
      // Only resize if buffer is empty or smaller.
      mSignalBuffer = new float[currentBufferSize];
    }
    mSignalBufferLength = currentBufferSize;
  }
  AudioDataValue* audioData = aAudioData;
  uint32_t audioDataLength = aAudioDataLength;
  uint32_t signalBufferTail = mSignalBufferLength - mSignalBufferPosition;

  // Group audio samples into optimal size for event dispatch, and queue.
  while (signalBufferTail <= audioDataLength) {
    float time = 0.0;
    // Guard against unsigned number overflow during first frame time calculation.
    if (aEndTimeSampleOffset > mSignalBufferPosition + audioDataLength) {
      time = (aEndTimeSampleOffset - mSignalBufferPosition - audioDataLength) / 
             mSamplesPerSecond;
    }

    // Fill the signalBuffer.
    uint32_t i;
    float *signalBuffer = mSignalBuffer.get() + mSignalBufferPosition;
    if (audioData) {
      for (i = 0; i < signalBufferTail; ++i) {
        signalBuffer[i] = AudioSampleToFloat(audioData[i]);
      }
    } else {
      memset(signalBuffer, 0, signalBufferTail*sizeof(signalBuffer[0]));
    }
    if (audioData) {
      audioData += signalBufferTail;
    }

    NS_ASSERTION(audioDataLength >= signalBufferTail,
                 "audioDataLength about to wrap past zero to +infinity!");
    audioDataLength -= signalBufferTail;

    if (mPendingEvents.Length() > 0) {
      // Check last event timecode to make sure that all queued events
      // are in non-descending sequence.
      nsAudioAvailableEventRunner* lastPendingEvent =
        (nsAudioAvailableEventRunner*)mPendingEvents[mPendingEvents.Length() - 1].get();
      if (lastPendingEvent->mTime > time) {
        // Clear the queue to start a fresh sequence.
        mPendingEvents.Clear();
      } else if (mPendingEvents.Length() >= MAX_PENDING_EVENTS) {
        NS_WARNING("Hit audio event queue max.");
        mPendingEvents.RemoveElementsAt(0, mPendingEvents.Length() - MAX_PENDING_EVENTS + 1);
      }
    }

    // Inform the element that we've written audio data.
    nsCOMPtr<nsIRunnable> event =
      new nsAudioAvailableEventRunner(mDecoder, mSignalBuffer.forget(),
                                      mSignalBufferLength, time);
    mPendingEvents.AppendElement(event);

    // Reset the buffer
    mSignalBufferLength = currentBufferSize;
    mSignalBuffer = new float[currentBufferSize];
    mSignalBufferPosition = 0;
    signalBufferTail = currentBufferSize;
  }

  NS_ASSERTION(mSignalBufferPosition + audioDataLength < mSignalBufferLength,
               "Intermediate signal buffer must fit at least one more item.");

  if (audioDataLength > 0) {
    // Add data to the signalBuffer.
    uint32_t i;
    float *signalBuffer = mSignalBuffer.get() + mSignalBufferPosition;
    if (audioData) {
      for (i = 0; i < audioDataLength; ++i) {
        signalBuffer[i] = AudioSampleToFloat(audioData[i]);
      }
    } else {
      memset(signalBuffer, 0, audioDataLength*sizeof(signalBuffer[0]));
    }
    mSignalBufferPosition += audioDataLength;
  }
}

void AudioAvailableEventManager::Clear()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  mPendingEvents.Clear();
  mSignalBufferPosition = 0;
}

void AudioAvailableEventManager::Drain(uint64_t aEndTime)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (!mHasListener) {
    return;
  }

  // Force all pending events to go now.
  for (uint32_t i = 0; i < mPendingEvents.Length(); ++i) {
    nsCOMPtr<nsIRunnable> event = mPendingEvents[i];
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }
  mPendingEvents.Clear();

  // If there is anything left in the signal buffer, put it in an event and fire.
  if (0 == mSignalBufferPosition)
    return;

  // Zero-pad the end of the signal buffer so it's complete.
  memset(mSignalBuffer.get() + mSignalBufferPosition, 0,
         (mSignalBufferLength - mSignalBufferPosition) * sizeof(float));

  // Force this last event to go now.
  float time = (aEndTime / static_cast<float>(USECS_PER_S)) - 
               (mSignalBufferPosition / mSamplesPerSecond);
  nsCOMPtr<nsIRunnable> lastEvent =
    new nsAudioAvailableEventRunner(mDecoder, mSignalBuffer.forget(),
                                    mSignalBufferLength, time);
  NS_DispatchToMainThread(lastEvent, NS_DISPATCH_NORMAL);

  mSignalBufferPosition = 0;
}

void AudioAvailableEventManager::SetSignalBufferLength(uint32_t aLength)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  mNewSignalBufferLength = aLength;
}

void AudioAvailableEventManager::NotifyAudioAvailableListener()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  mHasListener = true;
}

} // namespace mozilla

