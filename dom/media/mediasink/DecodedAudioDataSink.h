/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(DecodedAudioDataSink_h__)
#define DecodedAudioDataSink_h__

#include "AudioSink.h"
#include "MediaEventSource.h"
#include "MediaInfo.h"
#include "mozilla/nsRefPtr.h"
#include "nsISupportsImpl.h"

#include "mozilla/dom/AudioChannelBinding.h"
#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ReentrantMonitor.h"

namespace mozilla {

class AudioStream;

namespace media {

class DecodedAudioDataSink : public AudioSink {
public:

  DecodedAudioDataSink(MediaQueue<MediaData>& aAudioQueue,
                       int64_t aStartTime,
                       const AudioInfo& aInfo,
                       dom::AudioChannel aChannel);

  // Return a promise which will be resolved when DecodedAudioDataSink
  // finishes playing, or rejected if any error.
  nsRefPtr<GenericPromise> Init() override;

  /*
   * All public functions below are thread-safe.
   */
  int64_t GetPosition() override;
  int64_t GetEndTime() const override;

  // Check whether we've pushed more frames to the audio hardware than it has
  // played.
  bool HasUnplayedFrames() override;

  // Shut down the DecodedAudioDataSink's resources.
  void Shutdown() override;

  void SetVolume(double aVolume) override;
  void SetPlaybackRate(double aPlaybackRate) override;
  void SetPreservesPitch(bool aPreservesPitch) override;
  void SetPlaying(bool aPlaying) override;

private:
  enum State {
    AUDIOSINK_STATE_INIT,
    AUDIOSINK_STATE_PLAYING,
    AUDIOSINK_STATE_COMPLETE,
    AUDIOSINK_STATE_SHUTDOWN,
    AUDIOSINK_STATE_ERROR
  };

  virtual ~DecodedAudioDataSink();

  void DispatchTask(already_AddRefed<nsIRunnable>&& event);
  void SetState(State aState);
  void ScheduleNextLoop();
  void ScheduleNextLoopCrossThread();

  void OnAudioQueueEvent();
  void ConnectListener();
  void DisconnectListener();

  // The main loop for the audio thread. Sent to the thread as
  // an nsRunnableMethod. This continually does blocking writes to
  // to audio stream to play audio data.
  void AudioLoop();

  // Allocate and initialize mAudioStream.  Returns NS_OK on success.
  nsresult InitializeAudioStream();

  void Drain();

  void Cleanup();

  bool ExpectMoreAudioData();

  // Return true if playback is not ready and the sink is not told to shut down.
  bool WaitingForAudioToPlay();

  // Check if the sink has been told to shut down, resuming mAudioStream if
  // not.  Returns true if processing should continue, false if AudioLoop
  // should shutdown.
  bool IsPlaybackContinuing();

  // Write audio samples or silence to the audio hardware.
  // Return false if any error. Called on the audio thread.
  bool PlayAudio();

  void FinishAudioLoop();

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

  // If we have already written enough frames to the AudioStream, start the
  // playback.
  void StartAudioStreamPlaybackIfNeeded();
  void WriteSilence(uint32_t aFrames);

  ReentrantMonitor& GetReentrantMonitor() const {
    return mMonitor;
  }

  void AssertCurrentThreadInMonitor() const {
    GetReentrantMonitor().AssertCurrentThreadIn();
  }

  void AssertOnAudioThread();
  void AssertNotOnAudioThread();

  mutable ReentrantMonitor mMonitor;

  // There members are accessed on the audio thread only.
  State mState;
  Maybe<State> mPendingState;
  bool mAudioLoopScheduled;

  // Thread for pushing audio onto the audio hardware.
  // The "audio push thread".
  nsCOMPtr<nsIThread> mThread;

  // The audio stream resource. Used on the state machine, and audio threads.
  // This is created and destroyed on the audio thread, while holding the
  // decoder monitor, so if this is used off the audio thread, you must
  // first acquire the decoder monitor and check that it is non-null.
  nsRefPtr<AudioStream> mAudioStream;

  // The presentation time of the first audio frame that was played in
  // microseconds. We can add this to the audio stream position to determine
  // the current audio time. Accessed on audio and state machine thread.
  // Synchronized by decoder monitor.
  const int64_t mStartTime;

  // PCM frames written to the stream so far.
  Atomic<int64_t> mWritten;

  // Keep the last good position returned from the audio stream. Used to ensure
  // position returned by GetPosition() is mono-increasing in spite of audio
  // stream error.
  int64_t mLastGoodPosition;

  const AudioInfo mInfo;

  const dom::AudioChannel mChannel;

  bool mStopAudioThread;

  bool mPlaying;

  MozPromiseHolder<GenericPromise> mEndPromise;

  MediaEventListener mPushListener;
  MediaEventListener mFinishListener;
};

} // namespace media
} // namespace mozilla

#endif
