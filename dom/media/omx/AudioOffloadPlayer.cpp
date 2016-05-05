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

#include "AudioOffloadPlayer.h"
#include "nsComponentManagerUtils.h"
#include "nsITimer.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "VideoUtils.h"
#include "mozilla/dom/power/PowerManagerService.h"
#include "mozilla/dom/WakeLock.h"

#include <binder/IPCThreadState.h>
#include <stagefright/foundation/ADebug.h>
#include <stagefright/foundation/ALooper.h>
#include <stagefright/MediaDefs.h>
#include <stagefright/MediaErrors.h>
#include <stagefright/MediaSource.h>
#include <stagefright/MetaData.h>
#include <stagefright/Utils.h>
#include <AudioTrack.h>
#include <AudioSystem.h>
#include <AudioParameter.h>
#include <hardware/audio.h>

using namespace android;

namespace mozilla {

LazyLogModule gAudioOffloadPlayerLog("AudioOffloadPlayer");
#define AUDIO_OFFLOAD_LOG(type, msg) \
  MOZ_LOG(gAudioOffloadPlayerLog, type, msg)

// maximum time in paused state when offloading audio decompression.
// When elapsed, the GonkAudioSink is destroyed to allow the audio DSP to power down.
static const uint64_t OFFLOAD_PAUSE_MAX_MSECS = 60000ll;

AudioOffloadPlayer::AudioOffloadPlayer(MediaOmxCommonDecoder* aObserver) :
  mStarted(false),
  mPlaying(false),
  mReachedEOS(false),
  mIsElementVisible(true),
  mSampleRate(0),
  mStartPosUs(0),
  mPositionTimeMediaUs(-1),
  mInputBuffer(nullptr),
  mObserver(aObserver)
{
  MOZ_ASSERT(NS_IsMainThread());

  CHECK(aObserver);
#if ANDROID_VERSION >= 21
  mSessionId = AudioSystem::newAudioUniqueId();
  AudioSystem::acquireAudioSessionId(mSessionId, -1);
#else
  mSessionId = AudioSystem::newAudioSessionId();
  AudioSystem::acquireAudioSessionId(mSessionId);
#endif
  mAudioSink = new AudioOutput(mSessionId,
      IPCThreadState::self()->getCallingUid());
}

AudioOffloadPlayer::~AudioOffloadPlayer()
{
  Reset();
#if ANDROID_VERSION >= 21
  AudioSystem::releaseAudioSessionId(mSessionId, -1);
#else
  AudioSystem::releaseAudioSessionId(mSessionId);
#endif
}

void AudioOffloadPlayer::SetSource(const sp<MediaSource> &aSource)
{
  MOZ_ASSERT(NS_IsMainThread());
  CHECK(!mSource.get());

  mSource = aSource;
}

status_t AudioOffloadPlayer::Start(bool aSourceAlreadyStarted)
{
  MOZ_ASSERT(NS_IsMainThread());
  CHECK(!mStarted);
  CHECK(mSource.get());

  status_t err;
  CHECK(mAudioSink.get());

  if (!aSourceAlreadyStarted) {
    err = mSource->start();

    if (err != OK) {
      return err;
    }
  }

  sp<MetaData> format = mSource->getFormat();
  const char* mime;
  int avgBitRate = -1;
  int32_t channelMask;
  int32_t numChannels;
  int64_t durationUs = -1;
  audio_format_t audioFormat = AUDIO_FORMAT_PCM_16_BIT;
  uint32_t flags = AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD;
  audio_offload_info_t offloadInfo = AUDIO_INFO_INITIALIZER;

  CHECK(format->findCString(kKeyMIMEType, &mime));
  CHECK(format->findInt32(kKeySampleRate, &mSampleRate));
  CHECK(format->findInt32(kKeyChannelCount, &numChannels));
  format->findInt32(kKeyBitRate, &avgBitRate);
  format->findInt64(kKeyDuration, &durationUs);

  if(!format->findInt32(kKeyChannelMask, &channelMask)) {
    channelMask = CHANNEL_MASK_USE_CHANNEL_ORDER;
  }

  if (mapMimeToAudioFormat(audioFormat, mime) != OK) {
    AUDIO_OFFLOAD_LOG(LogLevel::Error, ("Couldn't map mime type \"%s\" to a valid "
        "AudioSystem::audio_format", mime));
    audioFormat = AUDIO_FORMAT_INVALID;
  }

  offloadInfo.duration_us = durationUs;
  offloadInfo.sample_rate = mSampleRate;
  offloadInfo.channel_mask = channelMask;
  offloadInfo.format = audioFormat;
  offloadInfo.stream_type = AUDIO_STREAM_MUSIC;
  offloadInfo.bit_rate = avgBitRate;
  offloadInfo.has_video = false;
  offloadInfo.is_streaming = false;

  AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("isOffloadSupported: SR=%u, CM=0x%x, "
      "Format=0x%x, StreamType=%d, BitRate=%u, duration=%lld us, has_video=%d",
      offloadInfo.sample_rate, offloadInfo.channel_mask, offloadInfo.format,
      offloadInfo.stream_type, offloadInfo.bit_rate, offloadInfo.duration_us,
      offloadInfo.has_video));

  err = mAudioSink->Open(mSampleRate,
                         numChannels,
                         channelMask,
                         audioFormat,
                         &AudioOffloadPlayer::AudioSinkCallback,
                         this,
                         (audio_output_flags_t) flags,
                         &offloadInfo);
  if (err == OK) {
    // If the playback is offloaded to h/w we pass the
    // HAL some metadata information
    // We don't want to do this for PCM because it will be going
    // through the AudioFlinger mixer before reaching the hardware
    SendMetaDataToHal(mAudioSink, format);
  }
  mStarted = true;
  mPlaying = false;

  return err;
}

status_t AudioOffloadPlayer::ChangeState(MediaDecoder::PlayState aState)
{
  MOZ_ASSERT(NS_IsMainThread());
  mPlayState = aState;

  switch (mPlayState) {
    case MediaDecoder::PLAY_STATE_PLAYING: {
      status_t err = Play();
      if (err != OK) {
        return err;
      }
      StartTimeUpdate();
    } break;

    case MediaDecoder::PLAY_STATE_PAUSED:
    case MediaDecoder::PLAY_STATE_SHUTDOWN:
      // Just pause here during play state shutdown as well to stop playing
      // offload track immediately. Resources will be freed by
      // MediaOmxCommonDecoder
      Pause();
      break;

    case MediaDecoder::PLAY_STATE_ENDED:
      Pause(true);
      break;

    default:
      break;
  }
  return OK;
}

static void ResetCallback(nsITimer* aTimer, void* aClosure)
{
  AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("%s", __FUNCTION__));
  AudioOffloadPlayer* player = static_cast<AudioOffloadPlayer*>(aClosure);
  if (player) {
    player->Reset();
  }
}

void AudioOffloadPlayer::Pause(bool aPlayPendingSamples)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mStarted) {
    CHECK(mAudioSink.get());
    WakeLockCreate();

    if (aPlayPendingSamples) {
      mAudioSink->Stop();
    } else {
      mAudioSink->Pause();
    }
    mPlaying = false;
  }

  if (mResetTimer) {
    return;
  }
  mResetTimer = do_CreateInstance("@mozilla.org/timer;1");
  mResetTimer->InitWithFuncCallback(ResetCallback,
                                    this,
                                    OFFLOAD_PAUSE_MAX_MSECS,
                                    nsITimer::TYPE_ONE_SHOT);
}

status_t AudioOffloadPlayer::Play()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mResetTimer) {
    mResetTimer->Cancel();
    mResetTimer = nullptr;
    WakeLockRelease();
  }

  status_t err = OK;

  if (!mStarted) {
    // Last pause timed out and offloaded audio sink was reset. Start it again
    err = Start(false);
    if (err != OK) {
      return err;
    }
    // Seek to last play position only when there was no seek during last pause
    android::Mutex::Autolock autoLock(mLock);
    if (!mSeekTarget.IsValid()) {
      mSeekTarget = SeekTarget(mPositionTimeMediaUs,
                               SeekTarget::Accurate,
                               MediaDecoderEventVisibility::Suppressed);
      DoSeek();
    }
  }

  if (!mPlaying) {
    CHECK(mAudioSink.get());
    err = mAudioSink->Start();
    if (err == OK) {
      mPlaying = true;
    }
  }

  return err;
}

void AudioOffloadPlayer::Reset()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mStarted) {
    return;
  }

  CHECK(mAudioSink.get());

  AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("reset: mPlaying=%d mReachedEOS=%d",
      mPlaying, mReachedEOS));

  mAudioSink->Stop();
  // If we're closing and have reached EOS, we don't want to flush
  // the track because if it is offloaded there could be a small
  // amount of residual data in the hardware buffer which we must
  // play to give gapless playback.
  // But if we're resetting when paused or before we've reached EOS
  // we can't be doing a gapless playback and there could be a large
  // amount of data queued in the hardware if the track is offloaded,
  // so we must flush to prevent a track switch being delayed playing
  // the buffered data that we don't want now
  if (!mPlaying || !mReachedEOS) {
    mAudioSink->Flush();
  }

  mAudioSink->Close();
  // Make sure to release any buffer we hold onto so that the
  // source is able to stop().

  if (mInputBuffer) {
    AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("Releasing input buffer"));

    mInputBuffer->release();
    mInputBuffer = nullptr;
  }
  mSource->stop();

  IPCThreadState::self()->flushCommands();
  StopTimeUpdate();

  mReachedEOS = false;
  mStarted = false;
  mPlaying = false;
  mStartPosUs = 0;

  WakeLockRelease();
}

RefPtr<MediaDecoder::SeekPromise> AudioOffloadPlayer::Seek(SeekTarget aTarget)
{
  MOZ_ASSERT(NS_IsMainThread());
  android::Mutex::Autolock autoLock(mLock);

  mSeekPromise.RejectIfExists(true, __func__);
  mSeekTarget = aTarget;
  RefPtr<MediaDecoder::SeekPromise> p = mSeekPromise.Ensure(__func__);
  DoSeek();
  return p;
}

status_t AudioOffloadPlayer::DoSeek()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mSeekTarget.IsValid());
  CHECK(mAudioSink.get());

  AUDIO_OFFLOAD_LOG(LogLevel::Debug,
                    ("DoSeek ( %lld )", mSeekTarget.GetTime().ToMicroseconds()));

  mReachedEOS = false;
  mPositionTimeMediaUs = -1;
  mStartPosUs = mSeekTarget.GetTime().ToMicroseconds();

  if (!mSeekPromise.IsEmpty()) {
    nsCOMPtr<nsIRunnable> nsEvent =
      NewRunnableMethod<MediaDecoderEventVisibility>(
        mObserver,
        &MediaDecoder::SeekingStarted,
        mSeekTarget.mEventVisibility);
    NS_DispatchToCurrentThread(nsEvent);
  }

  if (mPlaying) {
    mAudioSink->Pause();
    mAudioSink->Flush();
    mAudioSink->Start();

  } else {
    if (mStarted) {
      mAudioSink->Flush();
    }

    if (!mSeekPromise.IsEmpty()) {
      AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("Fake seek complete during pause"));
      // We do not reset mSeekTarget here.
      MediaDecoder::SeekResolveValue val(mReachedEOS, mSeekTarget.mEventVisibility);
      mSeekPromise.Resolve(val, __func__);
    }
  }

  return OK;
}

int64_t AudioOffloadPlayer::GetMediaTimeUs()
{
  android::Mutex::Autolock autoLock(mLock);

  int64_t playPosition = 0;
  if (mSeekTarget.IsValid()) {
    return mSeekTarget.GetTime().ToMicroseconds();
  }
  if (!mStarted) {
    return mPositionTimeMediaUs;
  }

  playPosition = GetOutputPlayPositionUs_l();
  if (!mReachedEOS) {
    mPositionTimeMediaUs = playPosition;
  }

  return mPositionTimeMediaUs;
}

int64_t AudioOffloadPlayer::GetOutputPlayPositionUs_l() const
{
  CHECK(mAudioSink.get());
  uint32_t playedSamples = 0;

  mAudioSink->GetPosition(&playedSamples);

  const int64_t playedUs = (static_cast<int64_t>(playedSamples) * 1000000 ) /
      mSampleRate;

  // HAL position is relative to the first buffer we sent at mStartPosUs
  const int64_t renderedDuration = mStartPosUs + playedUs;
  return renderedDuration;
}

void AudioOffloadPlayer::NotifyAudioEOS()
{
  android::Mutex::Autolock autoLock(mLock);
  // We do not reset mSeekTarget here.
  if (!mSeekPromise.IsEmpty()) {
    MediaDecoder::SeekResolveValue val(mReachedEOS, mSeekTarget.mEventVisibility);
    mSeekPromise.Resolve(val, __func__);
  }
  NS_DispatchToMainThread(NewRunnableMethod(mObserver,
                                            &MediaDecoder::PlaybackEnded));
}

void AudioOffloadPlayer::NotifyPositionChanged()
{
  NS_DispatchToMainThread(NewRunnableMethod(mObserver,
                                            &MediaOmxCommonDecoder::NotifyOffloadPlayerPositionChanged));
}

void AudioOffloadPlayer::NotifyAudioTearDown()
{
  // Fallback to state machine.
  // state machine's seeks will be done with
  // MediaDecoderEventVisibility::Suppressed.
  android::Mutex::Autolock autoLock(mLock);
  // We do not reset mSeekTarget here.
  if (!mSeekPromise.IsEmpty()) {
    MediaDecoder::SeekResolveValue val(mReachedEOS, mSeekTarget.mEventVisibility);
    mSeekPromise.Resolve(val, __func__);
  }
  NS_DispatchToMainThread(NewRunnableMethod(mObserver,
                                            &MediaOmxCommonDecoder::AudioOffloadTearDown));
}

// static
size_t AudioOffloadPlayer::AudioSinkCallback(GonkAudioSink* aAudioSink,
                                             void* aBuffer,
                                             size_t aSize,
                                             void* aCookie,
                                             GonkAudioSink::cb_event_t aEvent)
{
  AudioOffloadPlayer* me = (AudioOffloadPlayer*) aCookie;

  switch (aEvent) {

    case GonkAudioSink::CB_EVENT_FILL_BUFFER:
      AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("Notify Audio position changed"));
      me->NotifyPositionChanged();
      return me->FillBuffer(aBuffer, aSize);

    case GonkAudioSink::CB_EVENT_STREAM_END:
      AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("Notify Audio EOS"));
      me->mReachedEOS = true;
      me->NotifyAudioEOS();
      break;

    case GonkAudioSink::CB_EVENT_TEAR_DOWN:
      AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("Notify Tear down event"));
      me->NotifyAudioTearDown();
      break;

    default:
      AUDIO_OFFLOAD_LOG(LogLevel::Error, ("Unknown event %d from audio sink",
          aEvent));
      break;
  }
  return 0;
}

size_t AudioOffloadPlayer::FillBuffer(void* aData, size_t aSize)
{
  CHECK(mAudioSink.get());

  if (mReachedEOS) {
    return 0;
  }

  size_t sizeDone = 0;
  size_t sizeRemaining = aSize;
  int64_t seekTimeUs = -1;
  while (sizeRemaining > 0) {
    MediaSource::ReadOptions options;
    bool refreshSeekTime = false;
    {
      android::Mutex::Autolock autoLock(mLock);

      if (mSeekTarget.IsValid()) {
        seekTimeUs = mSeekTarget.GetTime().ToMicroseconds();
        options.setSeekTo(seekTimeUs);
        refreshSeekTime = true;

        if (mInputBuffer) {
          mInputBuffer->release();
          mInputBuffer = nullptr;
        }
      }
    }

    if (!mInputBuffer) {
      status_t err;
      err = mSource->read(&mInputBuffer, &options);

      CHECK((!err && mInputBuffer) || (err && !mInputBuffer));

      android::Mutex::Autolock autoLock(mLock);

      if (err != OK) {
        if (mSeekTarget.IsValid()) {
          mSeekTarget.Reset();
        }
        AUDIO_OFFLOAD_LOG(LogLevel::Error, ("Error while reading media source %d "
            "Ok to receive EOS error at end", err));
        if (!mReachedEOS) {
          // After seek there is a possible race condition if
          // OffloadThread is observing state_stopping_1 before
          // framesReady() > 0. Ensure sink stop is called
          // after last buffer is released. This ensures the
          // partial buffer is written to the driver before
          // stopping one is observed.The drawback is that
          // there will be an unnecessary call to the parser
          // after parser signalled EOS.
          if (sizeDone > 0) {
            AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("send Partial buffer down"));
            AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("skip calling stop till next"
                " fillBuffer"));
            break;
          }
          // no more buffers to push - stop() and wait for STREAM_END
          // don't set mReachedEOS until stream end received
          mAudioSink->Stop();
        }
        break;
      }

      if(mInputBuffer->range_length() != 0) {
        CHECK(mInputBuffer->meta_data()->findInt64(
            kKeyTime, &mPositionTimeMediaUs));
      }

      if (mSeekTarget.IsValid() &&
          seekTimeUs == mSeekTarget.GetTime().ToMicroseconds()) {
        MOZ_ASSERT(mSeekTarget.IsValid());
        mSeekTarget.Reset();
        if (!mSeekPromise.IsEmpty()) {
          AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("FillBuffer posting SEEK_COMPLETE"));
          MediaDecoder::SeekResolveValue val(mReachedEOS, mSeekTarget.mEventVisibility);
          mSeekPromise.Resolve(val, __func__);
        }
      } else if (mSeekTarget.IsValid()) {
        AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("seek is updated during unlocking mLock"));
      }

      if (refreshSeekTime) {
        NotifyPositionChanged();

        // need to adjust the mStartPosUs for offload decoding since parser
        // might not be able to get the exact seek time requested.
        mStartPosUs = mPositionTimeMediaUs;
        AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("Adjust seek time to: %.2f",
            mStartPosUs / 1E6));
      }
    }

    if (mInputBuffer->range_length() == 0) {
      mInputBuffer->release();
      mInputBuffer = nullptr;
      continue;
    }

    size_t copy = sizeRemaining;
    if (copy > mInputBuffer->range_length()) {
      copy = mInputBuffer->range_length();
    }

    memcpy((char *)aData + sizeDone,
        (const char *)mInputBuffer->data() + mInputBuffer->range_offset(),
        copy);

    mInputBuffer->set_range(mInputBuffer->range_offset() + copy,
        mInputBuffer->range_length() - copy);

    sizeDone += copy;
    sizeRemaining -= copy;
  }
  return sizeDone;
}

void AudioOffloadPlayer::SetElementVisibility(bool aIsVisible)
{
  MOZ_ASSERT(NS_IsMainThread());
  mIsElementVisible = aIsVisible;
  if (mIsElementVisible) {
    AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("Element is visible. Start time update"));
    StartTimeUpdate();
  }
}

static void TimeUpdateCallback(nsITimer* aTimer, void* aClosure)
{
  AudioOffloadPlayer* player = static_cast<AudioOffloadPlayer*>(aClosure);
  player->TimeUpdate();
}

void AudioOffloadPlayer::TimeUpdate()
{
  MOZ_ASSERT(NS_IsMainThread());
  TimeStamp now = TimeStamp::Now();

  // If TIMEUPDATE_MS has passed since the last fire update event fired, fire
  // another timeupdate event.
  if ((mLastFireUpdateTime.IsNull() ||
      now - mLastFireUpdateTime >=
          TimeDuration::FromMilliseconds(TIMEUPDATE_MS))) {
    mLastFireUpdateTime = now;
    NotifyPositionChanged();
  }

  if (mPlayState != MediaDecoder::PLAY_STATE_PLAYING || !mIsElementVisible) {
    StopTimeUpdate();
  }
}

nsresult AudioOffloadPlayer::StartTimeUpdate()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mTimeUpdateTimer) {
    return NS_OK;
  }

  mTimeUpdateTimer = do_CreateInstance("@mozilla.org/timer;1");
  return mTimeUpdateTimer->InitWithFuncCallback(TimeUpdateCallback,
      this,
      TIMEUPDATE_MS,
      nsITimer::TYPE_REPEATING_SLACK);
}

nsresult AudioOffloadPlayer::StopTimeUpdate()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mTimeUpdateTimer) {
    return NS_OK;
  }

  nsresult rv = mTimeUpdateTimer->Cancel();
  mTimeUpdateTimer = nullptr;
  return rv;
}

MediaDecoderOwner::NextFrameStatus AudioOffloadPlayer::GetNextFrameStatus()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mSeekTarget.IsValid()) {
    return MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_SEEKING;
  } else if (mPlayState == MediaDecoder::PLAY_STATE_ENDED) {
    return MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE;
  } else {
    return MediaDecoderOwner::NEXT_FRAME_AVAILABLE;
  }
}

void AudioOffloadPlayer::SendMetaDataToHal(sp<GonkAudioSink>& aSink,
                                           const sp<MetaData>& aMeta)
{
  int32_t sampleRate = 0;
  int32_t bitRate = 0;
  int32_t channelMask = 0;
  int32_t delaySamples = 0;
  int32_t paddingSamples = 0;
  CHECK(aSink.get());

  AudioParameter param = AudioParameter();

  if (aMeta->findInt32(kKeySampleRate, &sampleRate)) {
    param.addInt(String8(AUDIO_OFFLOAD_CODEC_SAMPLE_RATE), sampleRate);
  }
  if (aMeta->findInt32(kKeyChannelMask, &channelMask)) {
    param.addInt(String8(AUDIO_OFFLOAD_CODEC_NUM_CHANNEL), channelMask);
  }
  if (aMeta->findInt32(kKeyBitRate, &bitRate)) {
    param.addInt(String8(AUDIO_OFFLOAD_CODEC_AVG_BIT_RATE), bitRate);
  }
  if (aMeta->findInt32(kKeyEncoderDelay, &delaySamples)) {
    param.addInt(String8(AUDIO_OFFLOAD_CODEC_DELAY_SAMPLES), delaySamples);
  }
  if (aMeta->findInt32(kKeyEncoderPadding, &paddingSamples)) {
    param.addInt(String8(AUDIO_OFFLOAD_CODEC_PADDING_SAMPLES), paddingSamples);
  }

  AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("SendMetaDataToHal: bitRate %d,"
      " sampleRate %d, chanMask %d, delaySample %d, paddingSample %d", bitRate,
      sampleRate, channelMask, delaySamples, paddingSamples));

  aSink->SetParameters(param.toString());
  return;
}

void AudioOffloadPlayer::SetVolume(double aVolume)
{
  MOZ_ASSERT(NS_IsMainThread());
  CHECK(mAudioSink.get());
  mAudioSink->SetVolume((float) aVolume);
}

void AudioOffloadPlayer::WakeLockCreate()
{
  MOZ_ASSERT(NS_IsMainThread());
  AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("%s", __FUNCTION__));
  if (!mWakeLock) {
    RefPtr<dom::power::PowerManagerService> pmService =
      dom::power::PowerManagerService::GetInstance();
    NS_ENSURE_TRUE_VOID(pmService);

    ErrorResult rv;
    mWakeLock = pmService->NewWakeLock(NS_LITERAL_STRING("cpu"), nullptr, rv);
  }
}

void AudioOffloadPlayer::WakeLockRelease()
{
  MOZ_ASSERT(NS_IsMainThread());
  AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("%s", __FUNCTION__));
  if (mWakeLock) {
    ErrorResult rv;
    mWakeLock->Unlock(rv);
    mWakeLock = nullptr;
  }
}

} // namespace mozilla
