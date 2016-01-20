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
#include "AudioOffloadPlayerBase.h"
#include "MediaDecoderOwner.h"
#include "MediaOmxCommonDecoder.h"

namespace mozilla {

namespace dom {
class WakeLock;
}

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
 * Also this class passes state changes (play/pause/seek) from
 * MediaOmxCommonDecoder to GonkAudioSink as well as provide GonkAudioSink status
 * (position changed, playback ended, seek complete, audio tear down) back to
 * MediaOmxCommonDecoder
 *
 * It acts as a bridge between MediaOmxCommonDecoder and GonkAudioSink during
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

  AudioOffloadPlayer(MediaOmxCommonDecoder* aDecoder = nullptr);

  ~AudioOffloadPlayer();

  // Caller retains ownership of "aSource".
  void SetSource(const android::sp<MediaSource> &aSource) override;

  // Start the source if it's not already started and open the GonkAudioSink to
  // create an offloaded audio track
  status_t Start(bool aSourceAlreadyStarted = false) override;

  status_t ChangeState(MediaDecoder::PlayState aState) override;

  void SetVolume(double aVolume) override;

  int64_t GetMediaTimeUs() override;

  // To update progress bar when the element is visible
  void SetElementVisibility(bool aIsVisible) override;;

  // Update ready state based on current play state. Not checking data
  // availability since offloading is currently done only when whole compressed
  // data is available
  MediaDecoderOwner::NextFrameStatus GetNextFrameStatus() override;

  RefPtr<MediaDecoder::SeekPromise> Seek(SeekTarget aTarget) override;

  void TimeUpdate();

  // Close the audio sink, stop time updates, frees the input buffers
  void Reset();

private:
  // Set when audio source is started and GonkAudioSink is initialized
  // Used only in main thread
  bool mStarted;

  // Set when audio sink is started. i.e. playback started
  // Used only in main thread
  bool mPlaying;

  // Once playback reached end of stream (last ~100ms), position provided by DSP
  // may be reset/corrupted. This bool is used to avoid that.
  // Used in main thread and offload callback thread, protected by Mutex
  // mLock
  bool mReachedEOS;

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
  // mStartPosUs + GetPosition() (i.e. absolute position) to
  // MediaOmxCommonDecoder
  // Used in main thread and offload callback thread, protected by Mutex
  // mLock
  int64_t mStartPosUs;

  // The target of current seek when there is a request to seek
  // Used in main thread and offload callback thread, protected by Mutex
  // mLock
  SeekTarget mSeekTarget;

  // MozPromise of current seek.
  // Used in main thread and offload callback thread, protected by Mutex
  // mLock
  MozPromiseHolder<MediaDecoder::SeekPromise> mSeekPromise;

  // Positions obtained from offlaoded tracks (DSP)
  // Used in main thread and offload callback thread, protected by Mutex
  // mLock
  int64_t mPositionTimeMediaUs;

  // State obtained from MediaOmxCommonDecoder. Used only in main thread
  MediaDecoder::PlayState mPlayState;

  // Protect accessing audio position related variables between main thread and
  // offload callback thread
  Mutex mLock;

  // Compressed audio source.
  // Used in main thread first and later in offload callback thread
  android::sp<MediaSource> mSource;

  // Audio sink wrapper to access offloaded audio tracks
  // Used in main thread and offload callback thread
  // Race conditions are protected in underlying Android::AudioTrack class
  android::sp<GonkAudioSink> mAudioSink;

  // Buffer used to get date from audio source. Used in offload callback thread
  MediaBuffer* mInputBuffer;

  // MediaOmxCommonDecoder object used mainly to notify the audio sink status
  MediaOmxCommonDecoder* mObserver;

  TimeStamp mLastFireUpdateTime;

  // Timer to trigger position changed events
  nsCOMPtr<nsITimer> mTimeUpdateTimer;

  // Timer to reset GonkAudioSink when audio is paused for OFFLOAD_PAUSE_MAX_USECS.
  // It is triggered in Pause() and canceled when there is a Play() within
  // OFFLOAD_PAUSE_MAX_USECS. Used only from main thread so no lock is needed.
  nsCOMPtr<nsITimer> mResetTimer;

  // To avoid device suspend when mResetTimer is going to be triggered.
  // Used only from main thread so no lock is needed.
  RefPtr<mozilla::dom::WakeLock> mWakeLock;

  // Provide the playback position in microseconds from total number of
  // frames played by audio track
  int64_t GetOutputPlayPositionUs_l() const;

  // Fill the buffer given by audio sink with data from compressed audio
  // source. Also handles the seek by seeking audio source and stop the sink in
  // case of error
  size_t FillBuffer(void *aData, size_t aSize);

  // Called by GonkAudioSink when it needs data, to notify EOS or tear down event
  static size_t AudioSinkCallback(GonkAudioSink *aAudioSink,
                                  void *aData,
                                  size_t aSize,
                                  void *aMe,
                                  GonkAudioSink::cb_event_t aEvent);

  bool IsSeeking();

  // Set mSeekTarget to the given position and restart the sink. Actual seek
  // happens in FillBuffer(). If mSeekPromise is not empty, send
  // SeekingStarted event always and SeekingStopped event when the play state is
  // paused to MediaDecoder.
  // When decoding and playing happens separately, if there is a seek during
  // pause, we can decode and keep data ready.
  // In case of offload player, no way to seek during pause. So just fake that
  // seek is done.
  status_t DoSeek();

  // Start/Resume the audio sink so that callback will start being called to get
  // compressed data
  status_t Play();

  // Stop the audio sink if we need to play till we drain the current buffer.
  // or Pause the sink in case we should stop playing immediately
  void Pause(bool aPlayPendingSamples = false);

  // When audio is offloaded, application processor wakes up less frequently
  // (>1sec) But when Player UI is visible we need to update progress bar
  // atleast once in 250ms. Start a timer when player UI becomes visible or
  // audio starts playing to send UpdateLogicalPosition events once in 250ms.
  // Stop the timer when UI goes invisible or play state is not playing.
  // Also make sure timer functions are always called from main thread
  nsresult StartTimeUpdate();
  nsresult StopTimeUpdate();

  void WakeLockCreate();
  void WakeLockRelease();

  // Notify end of stream by sending PlaybackEnded event to observer
  // (i.e.MediaDecoder)
  void NotifyAudioEOS();

  // Notify position changed event by sending UpdateLogicalPosition event to
  // observer
  void NotifyPositionChanged();

  // Offloaded audio track is invalidated due to usecase change. Notify
  // MediaDecoder to re-evaluate offloading options
  void NotifyAudioTearDown();

  // Send information from MetaData to the HAL via GonkAudioSink
  void SendMetaDataToHal(android::sp<GonkAudioSink>& aSink,
                         const android::sp<MetaData>& aMeta);

  AudioOffloadPlayer(const AudioOffloadPlayer &);
  AudioOffloadPlayer &operator=(const AudioOffloadPlayer &);
};

} // namespace mozilla

#endif  // AUDIO_OFFLOAD_PLAYER_H_
