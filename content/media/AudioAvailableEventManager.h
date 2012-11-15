/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioAvailableEventManager_h__
#define AudioAvailableEventManager_h__

#include "nsCOMPtr.h"
#include "nsIRunnable.h"
#include "MediaDecoder.h"
#include "MediaDecoderReader.h"

namespace mozilla {

class AudioAvailableEventManager
{
public:
  AudioAvailableEventManager(MediaDecoder* aDecoder);
  ~AudioAvailableEventManager();

  // Initialize the event manager with audio metadata.  Called before
  // audio begins to get queued or events are dispatched.
  void Init(uint32_t aChannels, uint32_t aRate);

  // Dispatch pending MozAudioAvailable events in the queue.  Called
  // from the state machine thread.
  void DispatchPendingEvents(uint64_t aCurrentTime);

  // Queues audio sample data and re-packages it into equal sized
  // framebuffers.  Called from the audio thread.
  void QueueWrittenAudioData(AudioDataValue* aAudioData,
                             uint32_t aAudioDataLength,
                             uint64_t aEndTimeSampleOffset);

  // Clears the queue of any existing events.  Called from both the state
  // machine and audio threads.
  void Clear();

  // Fires one last event for any extra samples that didn't fit in a whole
  // framebuffer. This is meant to be called only once when the audio finishes.
  // Called from the state machine thread.
  void Drain(uint64_t aTime);

  // Sets the size of the signal buffer.
  // Called from the main and the state machine thread.
  void SetSignalBufferLength(uint32_t aLength);

  // Called by the media element to notify the manager that there is a
  // listener on the "MozAudioAvailable" event, and that we need to dispatch
  // such events. Called from the main thread.
  void NotifyAudioAvailableListener();

private:
  // The decoder associated with the event manager.  The event manager shares
  // the same lifetime as the decoder (the decoder holds a reference to the
  // manager).
  MediaDecoder* mDecoder;

  // The number of samples per second.
  float mSamplesPerSecond;

  // A buffer for audio data to be dispatched in DOM events.
  nsAutoArrayPtr<float> mSignalBuffer;

  // The current size of the signal buffer, may change due to DOM calls.
  uint32_t mSignalBufferLength;

  // The size of the new signal buffer, may change due to DOM calls.
  uint32_t mNewSignalBufferLength;

  // The position of the first available item in mSignalBuffer
  uint32_t mSignalBufferPosition;

  // The MozAudioAvailable events to be dispatched.  This queue is shared
  // between the state machine and audio threads.
  nsTArray< nsCOMPtr<nsIRunnable> > mPendingEvents;

  // ReentrantMonitor for shared access to mPendingEvents queue or
  // buffer length.
  ReentrantMonitor mReentrantMonitor;

  // True if something in the owning document has a listener on the
  // "MozAudioAvailable" event. If not, we don't need to bother copying played
  // audio data and dispatching the event. Synchronized by mReentrantMonitor.
  bool mHasListener;
};

} // namespace mozilla

#endif
