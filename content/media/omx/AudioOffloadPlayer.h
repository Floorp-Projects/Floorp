/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AUDIO_OFFLOAD_PLAYER_H_
#define AUDIO_OFFLOAD_PLAYER_H_

#include <stagefright/MediaBuffer.h>
#include <stagefright/MediaSource.h>
#include <stagefright/TimeSource.h>
#include <utils/threads.h>
#include <utils/RefBase.h>

#include "AudioOutput.h"

#include "MediaDecoderOwner.h"
#include "MediaOmxDecoder.h"

namespace mozilla {

class MediaOmxDecoder;

/**
 * AudioOffloadPlayer adds support for audio tunneling to a digital signal
 * processor (DSP) in the device chipset. With tunneling, audio decoding is
 * off-loaded to the DSP, waking the application processor less often and using
 * less battery
 *
 * This depends on offloading capability provided by Android KK AudioTrack class
 *
 * Audio playback is based on pull mechanism, whenever audio sink needs
 * data, FillBuffer() will read data from compressed audio source and provide
 * it to the sink
 *
 * Also this class passes state changes (play/pause/seek) from MediaOmxDecoder
 * to AudioSink as well as provide AudioSink status (position changed,
 * playback ended, seek complete, audio tear down) back to MediaOmxDecoder
 *
 * It acts as a bridge between MediaOmxDecoder and AudioSink during
 * offload playback
 */

class AudioOffloadPlayer : public AudioOffloadPlayerBase
{
  typedef android::Mutex Mutex;
  typedef android::MetaData MetaData;
  typedef android::status_t status_t;
  typedef android::AudioTrack AudioTrack;
  typedef android::MediaBuffer MediaBuffer;
  typedef android::MediaSource MediaSource;

public:
  enum {
    REACHED_EOS,
    SEEK_COMPLETE
  };

  AudioOffloadPlayer(MediaOmxDecoder* aDecoder = nullptr);

  ~AudioOffloadPlayer();

  // Caller retains ownership of "aSource".
  void SetSource(const android::sp<MediaSource> &aSource);

  // Start the source if it's not already started and open the AudioSink to
  // create an offloaded audio track
  status_t Start(bool aSourceAlreadyStarted = false);

  double GetMediaTimeSecs();

  // To update progress bar when the element is visible
  void SetElementVisibility(bool aIsVisible);

  void ChangeState(MediaDecoder::PlayState aState);

  void SetVolume(double aVolume);

  // Update ready state based on current play state. Not checking data
  // availability since offloading is currently done only when whole compressed
  // data is available
  MediaDecoderOwner::NextFrameStatus GetNextFrameStatus();

  void TimeUpdate();

private:
  // Set when audio source is started and audioSink is initialized
  // Used only in main thread
  bool mStarted;

  // Set when audio sink is started. i.e. playback started
  // Used only in main thread
  bool mPlaying;

  // Set when playstate is seeking and reset when FillBUffer() acknowledged
  // seeking by seeking audio source. Used in main thread and offload
  // callback thread, protected by Mutex mLock
  bool mSeeking;

  // Once playback reached end of stream (last ~100ms), position provided by DSP
  // may be reset/corrupted. This bool is used to avoid that.
  // Used in main thread and offload callback thread, protected by Mutex
  // mLock
  bool mReachedEOS;

  // Set when there is a seek request during pause.
  // Used in main thread and offload callback thread, protected by Mutex
  // mLock
  bool mSeekDuringPause;

  // Set when the HTML Audio Element is visible to the user.
  // Used only in main thread
  bool mIsElementVisible;

  // Session id given by Android::AudioSystem and used while creating audio sink
  // Used only in main thread
  int mSessionId;

  // Sample rate of current audio track. Used only in main thread
  int mSampleRate;

  // After seeking, positions returned by offlaoded tracks (DSP) will be
  // relative to the seeked position. And seeked position may be slightly
  // different than given mSeekTimeUs, if audio source cannot find a frame at
  // that position. Store seeked position in mStartPosUs and provide
  // mStartPosUs + GetPosition() (i.e. absolute position) to MediaOmxDecoder
  // Used in main thread and offload callback thread, protected by Mutex
  // mLock
  int64_t mStartPosUs;

  // Given seek time when there is a request to seek
  // Used in main thread and offload callback thread, protected by Mutex
  // mLock
  int64_t mSeekTimeUs;

  // Positions obtained from offlaoded tracks (DSP)
  // Used in main thread and offload callback thread, protected by Mutex
  // mLock
  int64_t mPositionTimeMediaUs;

  // State obtained from MediaOmxDecoder. Used only in main thread
  MediaDecoder::PlayState mPlayState;

  // Protect accessing audio position related variables between main thread and
  // offload callback thread
  Mutex mLock;

  // Compressed audio source.
  // Used in main thread first and later in offload callback thread
  android::sp<MediaSource> mSource;

  // Audio sink wrapper to access offloaded audio tracks
  // Used in main thread and offload callback thread, access is protected by
  // Race conditions are protected in underlying Android::AudioTrack class
  android::sp<AudioSink> mAudioSink;

  // Buffer used to get date from audio source. Used in offload callback thread
  MediaBuffer* mInputBuffer;

  // MediaOmxDecoder object used mainly to notify the audio sink status
  MediaOmxDecoder* mObserver;

  TimeStamp mLastFireUpdateTime;
  // Timer to trigger position changed events
  nsCOMPtr<nsITimer> mTimeUpdateTimer;

  int64_t GetMediaTimeUs();
  // Provide the playback position in microseconds from total number of
  // frames played by audio track
  int64_t GetOutputPlayPositionUs_l() const;

  // Fill the buffer given by audio sink with data from compressed audio
  // source. Also handles the seek by seeking audio source and stop the sink in
  // case of error
  size_t FillBuffer(void *aData, size_t aSize);

  // Called by AudioSink when it needs data, to notify EOS or tear down event
  static size_t AudioSinkCallback(AudioSink *aAudioSink,
                                  void *aData,
                                  size_t aSize,
                                  void *aMe,
                                  AudioSink::cb_event_t aEvent);

  bool IsSeeking();

  // Set mSeekTime to the given position and restart the sink. Actual seek
  // happens in FillBuffer(). To MediaDecoder, send SeekingStarted event always
  // and SeekingStopped event when the play state is paused.
  // When decoding and playing happens separately, if there is a seek during
  // pause, we can decode and keep data ready.
  // In case of offload player, no way to seek during pause. So just fake that
  // seek is done.
  status_t SeekTo(int64_t aTimeUs);

  // Close the audio sink, stop time updates, frees the input buffers
  void Reset();

  // Start/Resume the audio sink so that callback will start being called to get
  // compressed data
  status_t Play();

  // Stop the audio sink if we need to play till we drain the current buffer.
  // or Pause the sink in case we should stop playing immediately
  void Pause(bool aPlayPendingSamples = false);

  // When audio is offloaded, application processor wakes up less frequently
  // (>1sec) But when Player UI is visible we need to update progress bar
  // atleast once in 250ms. Start a timer when player UI becomes visible or
  // audio starts playing to send PlaybackPositionChanged events once in 250ms.
  // Stop the timer when UI goes invisible or play state is not playing.
  // Also make sure timer functions are always called from main thread
  nsresult StartTimeUpdate();
  nsresult StopTimeUpdate();

  // Notify end of stream by sending PlaybackEnded event to observer
  // (i.e.MediaDecoder)
  void NotifyAudioEOS();

  // Notify position changed event by sending PlaybackPositionChanged event to
  // observer
  void NotifyPositionChanged();

  // Offloaded audio track is invalidated due to usecase change. Notify
  // MediaDecoder to re-evaluate offloading options
  void NotifyAudioTearDown();

  // Send information from MetaData to the HAL via AudioSink
  void SendMetaDataToHal(android::sp<AudioSink>& aSink,
                         const android::sp<MetaData>& aMeta);

  AudioOffloadPlayer(const AudioOffloadPlayer &);
  AudioOffloadPlayer &operator=(const AudioOffloadPlayer &);
};

} // namespace mozilla

#endif  // AUDIO_OFFLOAD_PLAYER_H_
