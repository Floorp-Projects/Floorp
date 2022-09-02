/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef AudioSink_h__
#define AudioSink_h__

#include "AudioStream.h"
#include "AudibilityMonitor.h"
#include "MediaEventSource.h"
#include "MediaInfo.h"
#include "MediaQueue.h"
#include "MediaSink.h"
#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/Monitor.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"
#include "nsISupportsImpl.h"

namespace mozilla {

class AudioConverter;

class AudioSink : private AudioStream::DataSource {
 public:
  enum class InitializationType {
    // This AudioSink is being initialized for the first time
    INITIAL,
    UNMUTING
  };
  struct PlaybackParams {
    PlaybackParams(double aVolume, double aPlaybackRate, bool aPreservesPitch)
        : mVolume(aVolume),
          mPlaybackRate(aPlaybackRate),
          mPreservesPitch(aPreservesPitch) {}
    double mVolume;
    double mPlaybackRate;
    bool mPreservesPitch;
  };

  AudioSink(AbstractThread* aThread, MediaQueue<AudioData>& aAudioQueue,
            const AudioInfo& aInfo);

  ~AudioSink();

  // Allocate and initialize mAudioStream. Returns NS_OK on success.
  nsresult InitializeAudioStream(const PlaybackParams& aParams,
                                 const RefPtr<AudioDeviceInfo>& aAudioDevice,
                                 InitializationType aInitializationType);

  // Start audio playback.
  nsresult Start(const media::TimeUnit& aStartTime,
                 MozPromiseHolder<MediaSink::EndedPromise>& aEndedPromise);

  /*
   * All public functions are not thread-safe.
   * Called on the task queue of MDSM only.
   */
  media::TimeUnit GetPosition();
  media::TimeUnit GetEndTime() const;

  // Check whether we've pushed more frames to the audio stream than it
  // has played.
  bool HasUnplayedFrames();

  // The duration of the buffered frames.
  media::TimeUnit UnplayedDuration() const;

  // Shut down the AudioSink's resources. If an AudioStream existed, return the
  // ended promise it had, if it's shutting down-mid stream becaues it's muting.
  Maybe<MozPromiseHolder<MediaSink::EndedPromise>> Shutdown(
      ShutdownCause aShutdownCause = ShutdownCause::Regular);

  void SetVolume(double aVolume);
  void SetStreamName(const nsAString& aStreamName);
  void SetPlaybackRate(double aPlaybackRate);
  void SetPreservesPitch(bool aPreservesPitch);
  void SetPlaying(bool aPlaying);

  MediaEventSource<bool>& AudibleEvent() { return mAudibleEvent; }

  void GetDebugInfo(dom::MediaSinkDebugInfo& aInfo);

  // This returns true if the audio callbacks are being called, and so the
  // audio stream-based clock is moving forward.
  bool AudioStreamCallbackStarted() {
    return mAudioStream && mAudioStream->CallbackStarted();
  }

  void UpdateStartTime(const media::TimeUnit& aStartTime) {
    mStartTime = aStartTime;
  }

 private:
  // Interface of AudioStream::DataSource.
  // Called on the callback thread of cubeb. Returns the number of frames that
  // were available.
  uint32_t PopFrames(AudioDataValue* aBuffer, uint32_t aFrames,
                     bool aAudioThreadChanged) override;
  bool Ended() const override;

  // When shutting down, it's important to not lose any audio data, it might be
  // still of use, in two scenarios:
  // - If the audio is now captured to a MediaStream, whatever is enqueued in
  // the ring buffer needs to be played out now ;
  // - If the AudioSink is shutting down because the audio is muted, it's
  // important to keep the audio around in case it's quickly unmuted,
  // and in general to keep A/V sync correct when unmuted.
  void ReenqueueUnplayedAudioDataIfNeeded();

  void CheckIsAudible(const Span<AudioDataValue>& aInterleaved,
                      size_t aChannel);

  // The audio stream resource. Used on the task queue of MDSM only.
  RefPtr<AudioStream> mAudioStream;

  // The presentation time of the first audio frame that was played.
  // We can add this to the audio stream position to determine
  // the current audio time.
  media::TimeUnit mStartTime;

  // Keep the last good position returned from the audio stream. Used to ensure
  // position returned by GetPosition() is mono-increasing in spite of audio
  // stream error. Used on the task queue of MDSM only.
  media::TimeUnit mLastGoodPosition;

  // Used on the task queue of MDSM only.
  bool mPlaying;

  // PCM frames written to the stream so far. Written on the callback thread,
  // read on the MDSM thread.
  Atomic<int64_t> mWritten;

  // True if there is any error in processing audio data like overflow.
  Atomic<bool> mErrored;

  const RefPtr<AbstractThread> mOwnerThread;

  // Audio Processing objects and methods
  void OnAudioPopped();
  void OnAudioPushed(const RefPtr<AudioData>& aSample);
  void NotifyAudioNeeded();
  // Drain the converter and add the output to the processed audio queue.
  // A maximum of aMaxFrames will be added.
  uint32_t DrainConverter(uint32_t aMaxFrames = UINT32_MAX);
  already_AddRefed<AudioData> CreateAudioFromBuffer(
      AlignedAudioBuffer&& aBuffer, AudioData* aReference);
  // Add data to the processsed queue return the number of frames added.
  uint32_t PushProcessedAudio(AudioData* aData);
  uint32_t AudioQueuedInRingBufferMS() const;
  uint32_t SampleToFrame(uint32_t aSamples) const;
  UniquePtr<AudioConverter> mConverter;
  UniquePtr<SPSCQueue<AudioDataValue>> mProcessedSPSCQueue;
  MediaEventListener mAudioQueueListener;
  MediaEventListener mAudioQueueFinishListener;
  MediaEventListener mProcessedQueueListener;
  // Number of frames processed from mAudioQueue. Used to determine gaps in
  // the input stream. It indicates the time in frames since playback started
  // at the current input framerate.
  int64_t mFramesParsed;
  Maybe<RefPtr<AudioData>> mLastProcessedPacket;
  media::TimeUnit mLastEndTime;
  // Never modifed after construction.
  uint32_t mOutputRate;
  uint32_t mOutputChannels;
  AudibilityMonitor mAudibilityMonitor;
  bool mIsAudioDataAudible;
  MediaEventProducer<bool> mAudibleEvent;
  // Only signed on the real-time audio thread.
  MediaEventProducer<void> mAudioPopped;

  Atomic<bool> mProcessedQueueFinished;
  MediaQueue<AudioData>& mAudioQueue;
  const float mProcessedQueueThresholdMS;
};

}  // namespace mozilla

#endif  // AudioSink_h__
