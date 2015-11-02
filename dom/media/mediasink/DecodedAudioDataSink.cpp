/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioStream.h"
#include "MediaQueue.h"
#include "DecodedAudioDataSink.h"
#include "VideoUtils.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"

namespace mozilla {

extern PRLogModuleInfo* gMediaDecoderLog;
#define SINK_LOG(msg, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Debug, \
    ("DecodedAudioDataSink=%p " msg, this, ##__VA_ARGS__))
#define SINK_LOG_V(msg, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Verbose, \
  ("DecodedAudioDataSink=%p " msg, this, ##__VA_ARGS__))

namespace media {

// The amount of audio frames that is used to fuzz rounding errors.
static const int64_t AUDIO_FUZZ_FRAMES = 1;

DecodedAudioDataSink::DecodedAudioDataSink(MediaQueue<MediaData>& aAudioQueue,
                                           int64_t aStartTime,
                                           const AudioInfo& aInfo,
                                           dom::AudioChannel aChannel)
  : AudioSink(aAudioQueue)
  , mMonitor("DecodedAudioDataSink::mMonitor")
  , mState(AUDIOSINK_STATE_INIT)
  , mAudioLoopScheduled(false)
  , mStartTime(aStartTime)
  , mWritten(0)
  , mLastGoodPosition(0)
  , mInfo(aInfo)
  , mChannel(aChannel)
  , mStopAudioThread(false)
  , mPlaying(true)
{
}

DecodedAudioDataSink::~DecodedAudioDataSink()
{
}

void
DecodedAudioDataSink::SetState(State aState)
{
  AssertOnAudioThread();
  mPendingState = Some(aState);
}

void
DecodedAudioDataSink::DispatchTask(already_AddRefed<nsIRunnable>&& event)
{
  DebugOnly<nsresult> rv = mThread->Dispatch(Move(event), NS_DISPATCH_NORMAL);
  // There isn't much we can do if Dispatch() fails.
  // Just assert it to keep things simple.
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

void
DecodedAudioDataSink::OnAudioQueueEvent()
{
  AssertOnAudioThread();
  if (!mAudioLoopScheduled) {
    AudioLoop();
  }
}

void
DecodedAudioDataSink::ConnectListener()
{
  AssertOnAudioThread();
  mPushListener = AudioQueue().PushEvent().Connect(
    mThread, this, &DecodedAudioDataSink::OnAudioQueueEvent);
  mFinishListener = AudioQueue().FinishEvent().Connect(
    mThread, this, &DecodedAudioDataSink::OnAudioQueueEvent);
}

void
DecodedAudioDataSink::DisconnectListener()
{
  AssertOnAudioThread();
  mPushListener.Disconnect();
  mFinishListener.Disconnect();
}

void
DecodedAudioDataSink::ScheduleNextLoop()
{
  AssertOnAudioThread();
  if (mAudioLoopScheduled) {
    return;
  }
  mAudioLoopScheduled = true;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(this, &DecodedAudioDataSink::AudioLoop);
  DispatchTask(r.forget());
}

void
DecodedAudioDataSink::ScheduleNextLoopCrossThread()
{
  AssertNotOnAudioThread();
  RefPtr<DecodedAudioDataSink> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([self] () {
    // Do nothing if there is already a pending task waiting for its turn.
    if (!self->mAudioLoopScheduled) {
      self->AudioLoop();
    }
  });
  DispatchTask(r.forget());
}

RefPtr<GenericPromise>
DecodedAudioDataSink::Init()
{
  RefPtr<GenericPromise> p = mEndPromise.Ensure(__func__);
  nsresult rv = NS_NewNamedThread("Media Audio",
                                  getter_AddRefs(mThread),
                                  nullptr,
                                  SharedThreadPool::kStackSize);
  if (NS_FAILED(rv)) {
    mEndPromise.Reject(rv, __func__);
    return p;
  }

  ScheduleNextLoopCrossThread();
  return p;
}

int64_t
DecodedAudioDataSink::GetPosition()
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

  int64_t pos;
  if (mAudioStream &&
      (pos = mAudioStream->GetPosition()) >= 0) {
    NS_ASSERTION(pos >= mLastGoodPosition,
                 "AudioStream position shouldn't go backward");
    // Update the last good position when we got a good one.
    if (pos >= mLastGoodPosition) {
      mLastGoodPosition = pos;
    }
  }

  return mStartTime + mLastGoodPosition;
}

bool
DecodedAudioDataSink::HasUnplayedFrames()
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  // Experimentation suggests that GetPositionInFrames() is zero-indexed,
  // so we need to add 1 here before comparing it to mWritten.
  return mAudioStream && mAudioStream->GetPositionInFrames() + 1 < mWritten;
}

void
DecodedAudioDataSink::Shutdown()
{
  {
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
    if (mAudioStream) {
      mAudioStream->Cancel();
    }
  }
  RefPtr<DecodedAudioDataSink> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([=] () {
    self->mStopAudioThread = true;
    if (!self->mAudioLoopScheduled) {
      self->AudioLoop();
    }
  });
  DispatchTask(r.forget());

  mThread->Shutdown();
  mThread = nullptr;
  if (mAudioStream) {
    mAudioStream->Shutdown();
    mAudioStream = nullptr;
  }

  // Should've reached the final state after shutdown.
  MOZ_ASSERT(mState == AUDIOSINK_STATE_SHUTDOWN ||
             mState == AUDIOSINK_STATE_ERROR);
  // Should have no pending state change.
  MOZ_ASSERT(mPendingState.isNothing());
}

void
DecodedAudioDataSink::SetVolume(double aVolume)
{
  AssertNotOnAudioThread();
  RefPtr<DecodedAudioDataSink> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([=] () {
    if (self->mState == AUDIOSINK_STATE_PLAYING) {
      self->mAudioStream->SetVolume(aVolume);
    }
  });
  DispatchTask(r.forget());
}

void
DecodedAudioDataSink::SetPlaybackRate(double aPlaybackRate)
{
  AssertNotOnAudioThread();
  MOZ_ASSERT(aPlaybackRate != 0, "Don't set the playbackRate to 0 on AudioStream");
  RefPtr<DecodedAudioDataSink> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([=] () {
    if (self->mState == AUDIOSINK_STATE_PLAYING) {
      self->mAudioStream->SetPlaybackRate(aPlaybackRate);
    }
  });
  DispatchTask(r.forget());
}

void
DecodedAudioDataSink::SetPreservesPitch(bool aPreservesPitch)
{
  AssertNotOnAudioThread();
  RefPtr<DecodedAudioDataSink> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([=] () {
    if (self->mState == AUDIOSINK_STATE_PLAYING) {
      self->mAudioStream->SetPreservesPitch(aPreservesPitch);
    }
  });
  DispatchTask(r.forget());
}

void
DecodedAudioDataSink::SetPlaying(bool aPlaying)
{
  AssertNotOnAudioThread();
  RefPtr<DecodedAudioDataSink> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([=] () {
    if (self->mState != AUDIOSINK_STATE_PLAYING ||
        self->mPlaying == aPlaying) {
      return;
    }
    self->mPlaying = aPlaying;
    // pause/resume AudioStream as necessary.
    if (!aPlaying && !self->mAudioStream->IsPaused()) {
      self->mAudioStream->Pause();
    } else if (aPlaying && self->mAudioStream->IsPaused()) {
      self->mAudioStream->Resume();
    }
    // Wake up the audio loop to play next sample.
    if (aPlaying && !self->mAudioLoopScheduled) {
      self->AudioLoop();
    }
  });
  DispatchTask(r.forget());
}

nsresult
DecodedAudioDataSink::InitializeAudioStream()
{
  // AudioStream initialization can block for extended periods in unusual
  // circumstances, so we take care to drop the decoder monitor while
  // initializing.
  RefPtr<AudioStream> audioStream(new AudioStream());
  nsresult rv = audioStream->Init(mInfo.mChannels, mInfo.mRate, mChannel);
  if (NS_FAILED(rv)) {
    audioStream->Shutdown();
    return rv;
  }

  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  mAudioStream = audioStream;

  return NS_OK;
}

void
DecodedAudioDataSink::Drain()
{
  AssertOnAudioThread();
  MOZ_ASSERT(mPlaying && !mAudioStream->IsPaused());
  // If the media was too short to trigger the start of the audio stream,
  // start it now.
  mAudioStream->Start();
  mAudioStream->Drain();
}

void
DecodedAudioDataSink::Cleanup()
{
  AssertOnAudioThread();
  mEndPromise.Resolve(true, __func__);
  // Since the promise if resolved asynchronously, we don't shutdown
  // AudioStream here so MDSM::ResyncAudioClock can get the correct
  // audio position.
}

bool
DecodedAudioDataSink::ExpectMoreAudioData()
{
  return AudioQueue().GetSize() == 0 && !AudioQueue().IsFinished();
}

bool
DecodedAudioDataSink::WaitingForAudioToPlay()
{
  AssertOnAudioThread();
  // Return true if we're not playing, and we're not shutting down, or we're
  // playing and we've got no audio to play.
  if (!mStopAudioThread && (!mPlaying || ExpectMoreAudioData())) {
    return true;
  }
  return false;
}

bool
DecodedAudioDataSink::IsPlaybackContinuing()
{
  AssertOnAudioThread();
  // If we're shutting down, captured, or at EOS, break out and exit the audio
  // thread.
  if (mStopAudioThread || AudioQueue().AtEndOfStream()) {
    return false;
  }

  return true;
}

void
DecodedAudioDataSink::AudioLoop()
{
  AssertOnAudioThread();
  mAudioLoopScheduled = false;

  switch (mState) {
    case AUDIOSINK_STATE_INIT: {
      SINK_LOG("AudioLoop started");
      nsresult rv = InitializeAudioStream();
      if (NS_FAILED(rv)) {
        NS_WARNING("Initializing AudioStream failed.");
        mEndPromise.Reject(rv, __func__);
        SetState(AUDIOSINK_STATE_ERROR);
        break;
      }
      SetState(AUDIOSINK_STATE_PLAYING);
      ConnectListener();
      break;
    }

    case AUDIOSINK_STATE_PLAYING: {
      if (WaitingForAudioToPlay()) {
        // OnAudioQueueEvent() will schedule next loop.
        break;
      }
      if (!IsPlaybackContinuing()) {
        SetState(AUDIOSINK_STATE_COMPLETE);
        break;
      }
      if (!PlayAudio()) {
        SetState(AUDIOSINK_STATE_COMPLETE);
        break;
      }
      // Schedule next loop to play next sample.
      ScheduleNextLoop();
      break;
    }

    case AUDIOSINK_STATE_COMPLETE: {
      DisconnectListener();
      FinishAudioLoop();
      SetState(AUDIOSINK_STATE_SHUTDOWN);
      break;
    }

    case AUDIOSINK_STATE_SHUTDOWN:
      break;

    case AUDIOSINK_STATE_ERROR:
      break;
  } // end of switch

  // We want mState to stay stable during AudioLoop to keep things simple.
  // Therefore, we only do state transition at the end of AudioLoop.
  if (mPendingState.isSome()) {
    MOZ_ASSERT(mState != mPendingState.ref());
    SINK_LOG("change mState, %d -> %d", mState, mPendingState.ref());
    mState = mPendingState.ref();
    mPendingState.reset();
    // Schedule next loop when state changes.
    ScheduleNextLoop();
  }
}

bool
DecodedAudioDataSink::PlayAudio()
{
  // See if there's a gap in the audio. If there is, push silence into the
  // audio hardware, so we can play across the gap.
  // Calculate the timestamp of the next chunk of audio in numbers of
  // samples.
  NS_ASSERTION(AudioQueue().GetSize() > 0, "Should have data to play");
  CheckedInt64 sampleTime = UsecsToFrames(AudioQueue().PeekFront()->mTime, mInfo.mRate);

  // Calculate the number of frames that have been pushed onto the audio hardware.
  CheckedInt64 playedFrames = UsecsToFrames(mStartTime, mInfo.mRate) +
                              static_cast<int64_t>(mWritten);

  CheckedInt64 missingFrames = sampleTime - playedFrames;
  if (!missingFrames.isValid() || !sampleTime.isValid()) {
    NS_WARNING("Int overflow adding in AudioLoop");
    return false;
  }

  if (missingFrames.value() > AUDIO_FUZZ_FRAMES) {
    // The next audio chunk begins some time after the end of the last chunk
    // we pushed to the audio hardware. We must push silence into the audio
    // hardware so that the next audio chunk begins playback at the correct
    // time.
    missingFrames = std::min<int64_t>(UINT32_MAX, missingFrames.value());
    mWritten += PlaySilence(static_cast<uint32_t>(missingFrames.value()));
  } else {
    mWritten += PlayFromAudioQueue();
  }

  return true;
}

void
DecodedAudioDataSink::FinishAudioLoop()
{
  AssertOnAudioThread();
  MOZ_ASSERT(mStopAudioThread || AudioQueue().AtEndOfStream());
  if (!mStopAudioThread && mPlaying) {
    Drain();
  }
  SINK_LOG("AudioLoop complete");
  Cleanup();
  SINK_LOG("AudioLoop exit");
}

uint32_t
DecodedAudioDataSink::PlaySilence(uint32_t aFrames)
{
  // Maximum number of bytes we'll allocate and write at once to the audio
  // hardware when the audio stream contains missing frames and we're
  // writing silence in order to fill the gap. We limit our silence-writes
  // to 32KB in order to avoid allocating an impossibly large chunk of
  // memory if we encounter a large chunk of silence.
  const uint32_t SILENCE_BYTES_CHUNK = 32 * 1024;

  AssertOnAudioThread();
  NS_ASSERTION(!mAudioStream->IsPaused(), "Don't play when paused");
  uint32_t maxFrames = SILENCE_BYTES_CHUNK / mInfo.mChannels / sizeof(AudioDataValue);
  uint32_t frames = std::min(aFrames, maxFrames);
  SINK_LOG_V("playing %u frames of silence", aFrames);
  WriteSilence(frames);
  return frames;
}

uint32_t
DecodedAudioDataSink::PlayFromAudioQueue()
{
  AssertOnAudioThread();
  NS_ASSERTION(!mAudioStream->IsPaused(), "Don't play when paused");
  RefPtr<AudioData> audio =
    dont_AddRef(AudioQueue().PopFront().take()->As<AudioData>());

  SINK_LOG_V("playing %u frames of audio at time %lld",
             audio->mFrames, audio->mTime);
  if (audio->mRate == mInfo.mRate && audio->mChannels == mInfo.mChannels) {
    mAudioStream->Write(audio->mAudioData, audio->mFrames);
  } else {
    SINK_LOG_V("mismatched sample format mInfo=[%uHz/%u channels] audio=[%uHz/%u channels]",
               mInfo.mRate, mInfo.mChannels, audio->mRate, audio->mChannels);
    PlaySilence(audio->mFrames);
  }

  StartAudioStreamPlaybackIfNeeded();

  return audio->mFrames;
}

void
DecodedAudioDataSink::StartAudioStreamPlaybackIfNeeded()
{
  // This value has been chosen empirically.
  const uint32_t MIN_WRITE_BEFORE_START_USECS = 200000;

  // We want to have enough data in the buffer to start the stream.
  if (static_cast<double>(mAudioStream->GetWritten()) / mAudioStream->GetRate() >=
      static_cast<double>(MIN_WRITE_BEFORE_START_USECS) / USECS_PER_S) {
    mAudioStream->Start();
  }
}

void
DecodedAudioDataSink::WriteSilence(uint32_t aFrames)
{
  uint32_t numSamples = aFrames * mInfo.mChannels;
  nsAutoTArray<AudioDataValue, 1000> buf;
  buf.SetLength(numSamples);
  memset(buf.Elements(), 0, numSamples * sizeof(AudioDataValue));
  mAudioStream->Write(buf.Elements(), aFrames);

  StartAudioStreamPlaybackIfNeeded();
}

int64_t
DecodedAudioDataSink::GetEndTime() const
{
  CheckedInt64 playedUsecs = FramesToUsecs(mWritten, mInfo.mRate) + mStartTime;
  if (!playedUsecs.isValid()) {
    NS_WARNING("Int overflow calculating audio end time");
    return -1;
  }
  return playedUsecs.value();
}

void
DecodedAudioDataSink::AssertOnAudioThread()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mThread);
}

void
DecodedAudioDataSink::AssertNotOnAudioThread()
{
  MOZ_ASSERT(NS_GetCurrentThread() != mThread);
}

} // namespace media
} // namespace mozilla
