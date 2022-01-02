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
            const media::TimeUnit& aStartTime, const AudioInfo& aInfo,
            AudioDeviceInfo* aAudioDevice);

  ~AudioSink();

  // Start audio playback and return a promise which will be resolved when the
  // playback finishes, or return an error result if any error occurs.
  Result<already_AddRefed<MediaSink::EndedPromise>, nsresult> Start(
      const PlaybackParams& aParams);

  /*
   * All public functions are not thread-safe.
   * Called on the task queue of MDSM only.
   */
  media::TimeUnit GetPosition();
  media::TimeUnit GetEndTime() const;

  // Check whether we've pushed more frames to the audio hardware than it has
  // played.
  bool HasUnplayedFrames();

  // Shut down the AudioSink's resources.
  void Shutdown();

  void SetVolume(double aVolume);
  void SetStreamName(const nsAString& aStreamName);
  void SetPlaybackRate(double aPlaybackRate);
  void SetPreservesPitch(bool aPreservesPitch);
  void SetPlaying(bool aPlaying);

  MediaEventSource<bool>& AudibleEvent() { return mAudibleEvent; }

  void GetDebugInfo(dom::MediaSinkDebugInfo& aInfo);

  const RefPtr<AudioDeviceInfo>& AudioDevice() { return mAudioDevice; }

 private:
  // Allocate and initialize mAudioStream. Returns NS_OK on success.
  nsresult InitializeAudioStream(const PlaybackParams& aParams);

  // Interface of AudioStream::DataSource.
  // Called on the callback thread of cubeb.
  UniquePtr<AudioStream::Chunk> PopFrames(uint32_t aFrames) override;
  bool Ended() const override;

  void CheckIsAudible(const AudioData* aData);

  // The audio stream resource. Used on the task queue of MDSM only.
  RefPtr<AudioStream> mAudioStream;

  // The presentation time of the first audio frame that was played.
  // We can add this to the audio stream position to determine
  // the current audio time.
  const media::TimeUnit mStartTime;

  // Keep the last good position returned from the audio stream. Used to ensure
  // position returned by GetPosition() is mono-increasing in spite of audio
  // stream error. Used on the task queue of MDSM only.
  media::TimeUnit mLastGoodPosition;

  const AudioInfo mInfo;

  // The output device this AudioSink is playing data to. The system's default
  // device is used if this is null.
  const RefPtr<AudioDeviceInfo> mAudioDevice;

  // Used on the task queue of MDSM only.
  bool mPlaying;

  /*
   * Members to implement AudioStream::DataSource.
   * Used on the callback thread of cubeb.
   */
  // The AudioData at which AudioStream::DataSource is reading.
  RefPtr<AudioData> mCurrentData;

  // Monitor protecting access to mCursor, mWritten and mCurrentData.
  // mCursor is created/destroyed on the cubeb thread, while we must also
  // ensure that mWritten and mCursor::Available() get modified simultaneously.
  // (written on cubeb thread, and read on MDSM task queue).
  mutable Monitor mMonitor;
  // Keep track of the read position of mCurrentData.
  UniquePtr<AudioBufferCursor> mCursor;

  // PCM frames written to the stream so far.
  int64_t mWritten;

  // True if there is any error in processing audio data like overflow.
  Atomic<bool> mErrored;

  const RefPtr<AbstractThread> mOwnerThread;

  // Audio Processing objects and methods
  void OnAudioPopped(const RefPtr<AudioData>& aSample);
  void OnAudioPushed(const RefPtr<AudioData>& aSample);
  void NotifyAudioNeeded();
  // Drain the converter and add the output to the processed audio queue.
  // A maximum of aMaxFrames will be added.
  uint32_t DrainConverter(uint32_t aMaxFrames = UINT32_MAX);
  already_AddRefed<AudioData> CreateAudioFromBuffer(
      AlignedAudioBuffer&& aBuffer, AudioData* aReference);
  // Add data to the processsed queue, update mProcessedQueueLength and
  // return the number of frames added.
  uint32_t PushProcessedAudio(AudioData* aData);
  UniquePtr<AudioConverter> mConverter;
  MediaQueue<AudioData> mProcessedQueue;
  // Length in microseconds of the ProcessedQueue
  Atomic<uint64_t> mProcessedQueueLength;
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

  MediaQueue<AudioData>& mAudioQueue;
};

}  // namespace mozilla

#endif  // AudioSink_h__
