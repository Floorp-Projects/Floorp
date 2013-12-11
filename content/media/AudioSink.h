/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(AudioSink_h__)
#define AudioSink_h__

#include "nsISupportsImpl.h"
#include "MediaDecoderReader.h"
#include "mozilla/dom/AudioChannelBinding.h"

namespace mozilla {

class AudioAvailableEventManager;
class AudioStream;
class MediaDecoderStateMachine;

class AudioSink {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AudioSink)

  AudioSink(MediaDecoderStateMachine* aStateMachine,
            int64_t aStartTime, AudioInfo aInfo, dom::AudioChannel aChannel);

  nsresult Init();

  int64_t GetPosition();

  // Tell the AudioSink to stop processing and initiate shutdown.  Must be
  // called with the decoder monitor held.
  void PrepareToShutdown();

  // Shut down the AudioSink's resources.  The decoder monitor must not be
  // held during this call, as it may block processing thread event queues.
  void Shutdown();

  void SetVolume(double aVolume);
  void SetPlaybackRate(double aPlaybackRate);
  void SetPreservesPitch(bool aPreservesPitch);

  void StartPlayback();
  void StopPlayback();

private:
  ~AudioSink() {}

  // The main loop for the audio thread. Sent to the thread as
  // an nsRunnableMethod. This continually does blocking writes to
  // to audio stream to play audio data.
  void AudioLoop();

  // Allocate and initialize mAudioStream.  Returns NS_OK on success.
  nsresult InitializeAudioStream();

  void Drain();

  void Cleanup();

  bool ExpectMoreAudioData();

  // Wait on the decoder monitor until playback is ready or the sink is told to shut down.
  void WaitForAudioToPlay();

  // Check if the sink has been told to shut down, resuming mAudioStream if
  // not.  Returns true if processing should continue, false if AudioLoop
  // should shutdown.
  bool IsPlaybackContinuing();

  // Write aFrames of audio frames of silence to the audio hardware. Returns
  // the number of frames actually written. The write size is capped at
  // SILENCE_BYTES_CHUNK (32kB), so must be called in a loop to write the
  // desired number of frames. This ensures that the playback position
  // advances smoothly, and guarantees that we don't try to allocate an
  // impossibly large chunk of memory in order to play back silence. Called
  // on the audio thread.
  uint32_t PlaySilence(uint32_t aFrames);

  // Pops an audio chunk from the front of the audio queue, and pushes its
  // audio data to the audio hardware.  Called on the audio thread.
  uint32_t PlayFromAudioQueue();

  void UpdateStreamSettings();

  // If we have already written enough frames to the AudioStream, start the
  // playback.
  void StartAudioStreamPlaybackIfNeeded();
  void WriteSilence(uint32_t aFrames);

  int64_t GetEndTime();

  MediaQueue<AudioData>& AudioQueue();

  ReentrantMonitor& GetReentrantMonitor();
  void AssertCurrentThreadInMonitor();
  void AssertOnAudioThread();

  nsRefPtr<MediaDecoderStateMachine> mStateMachine;

  // Thread for pushing audio onto the audio hardware.
  // The "audio push thread".
  nsCOMPtr<nsIThread> mThread;

  // The audio stream resource. Used on the state machine, and audio threads.
  // This is created and destroyed on the audio thread, while holding the
  // decoder monitor, so if this is used off the audio thread, you must
  // first acquire the decoder monitor and check that it is non-null.
  RefPtr<AudioStream> mAudioStream;

  // The presentation time of the first audio frame that was played in
  // microseconds. We can add this to the audio stream position to determine
  // the current audio time. Accessed on audio and state machine thread.
  // Synchronized by decoder monitor.
  int64_t mStartTime;

  // PCM frames written to the stream so far.
  int64_t mWritten;

  AudioInfo mInfo;

  dom::AudioChannel mChannel;

  double mVolume;
  double mPlaybackRate;
  bool mPreservesPitch;

  bool mStopAudioThread;

  bool mSetVolume;
  bool mSetPlaybackRate;
  bool mSetPreservesPitch;

  bool mPlaying;
};

} // namespace mozilla

#endif
