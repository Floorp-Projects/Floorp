/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "AudioInputSource.h"

#include "CallbackThreadRegistry.h"
#include "GraphDriver.h"
#include "Tracing.h"

namespace mozilla {

extern mozilla::LazyLogModule gMediaTrackGraphLog;

#ifdef LOG_INTERNAL
#  undef LOG_INTERNAL
#endif  // LOG_INTERNAL
#define LOG_INTERNAL(level, msg, ...) \
  MOZ_LOG(gMediaTrackGraphLog, LogLevel::level, (msg, ##__VA_ARGS__))

#ifdef LOG
#  undef LOG
#endif  // LOG
#define LOG(msg, ...) LOG_INTERNAL(Debug, msg, ##__VA_ARGS__)

#ifdef LOGW
#  undef LOGW
#endif  // LOGW
#define LOGW(msg, ...) LOG_INTERNAL(Warning, msg, ##__VA_ARGS__)

#ifdef LOGE
#  undef LOGE
#endif  // LOGE
#define LOGE(msg, ...) LOG_INTERNAL(Error, msg, ##__VA_ARGS__)

#ifdef LOGV
#  undef LOGV
#endif  // LOGV
#define LOGV(msg, ...) LOG_INTERNAL(Verbose, msg, ##__VA_ARGS__)

AudioInputSource::AudioInputSource(RefPtr<EventListener>&& aListener,
                                   Id aSourceId,
                                   CubebUtils::AudioDeviceID aDeviceId,
                                   uint32_t aChannelCount, bool aIsVoice,
                                   const PrincipalHandle& aPrincipalHandle,
                                   TrackRate aSourceRate, TrackRate aTargetRate)
    : mId(aSourceId),
      mDeviceId(aDeviceId),
      mChannelCount(aChannelCount),
      mRate(aSourceRate),
      mIsVoice(aIsVoice),
      mPrincipalHandle(aPrincipalHandle),
      mSandboxed(CubebUtils::SandboxEnabled()),
      mAudioThreadId(ProfilerThreadId{}),
      mEventListener(std::move(aListener)),
      mTaskThread(CUBEB_TASK_THREAD),
      mDriftCorrector(static_cast<uint32_t>(aSourceRate),
                      static_cast<uint32_t>(aTargetRate), aPrincipalHandle) {
  MOZ_ASSERT(mChannelCount > 0);
  MOZ_ASSERT(mEventListener);
}

void AudioInputSource::Start() {
  // This is called on MediaTrackGraph's graph thread, which can be the cubeb
  // stream's callback thread. Running cubeb operations within cubeb stream
  // callback thread can cause the deadlock on Linux, so we dispatch those
  // operations to the task thread.
  MOZ_ASSERT(mTaskThread);

  LOG("AudioInputSource %p, start", this);
  MOZ_ALWAYS_SUCCEEDS(mTaskThread->Dispatch(
      NS_NewRunnableFunction(__func__, [self = RefPtr(this)]() mutable {
        self->mStream = CubebInputStream::Create(
            self->mDeviceId, self->mChannelCount,
            static_cast<uint32_t>(self->mRate), self->mIsVoice, self.get());
        if (!self->mStream) {
          LOGE("AudioInputSource %p, cannot create an audio input stream!",
               self.get());
          return;
        }

        if (uint32_t latency = 0;
            self->mStream->Latency(&latency) == CUBEB_OK) {
          Data data(LatencyChangeData{media::TimeUnit(latency, self->mRate)});
          if (self->mSPSCQueue.Enqueue(data) == 0) {
            LOGE("AudioInputSource %p, failed to enqueue latency change",
                 self.get());
          }
        }
        if (int r = self->mStream->Start(); r != CUBEB_OK) {
          LOGE(
              "AudioInputSource %p, cannot start its audio input stream! The "
              "stream is destroyed directly!",
              self.get());
          self->mStream = nullptr;
        }
      })));
}

void AudioInputSource::Stop() {
  // This is called on MediaTrackGraph's graph thread, which can be the cubeb
  // stream's callback thread. Running cubeb operations within cubeb stream
  // callback thread can cause the deadlock on Linux, so we dispatch those
  // operations to the task thread.
  MOZ_ASSERT(mTaskThread);

  LOG("AudioInputSource %p, stop", this);
  MOZ_ALWAYS_SUCCEEDS(mTaskThread->Dispatch(
      NS_NewRunnableFunction(__func__, [self = RefPtr(this)]() mutable {
        if (!self->mStream) {
          LOGE("AudioInputSource %p, has no audio input stream to stop!",
               self.get());
          return;
        }
        if (int r = self->mStream->Stop(); r != CUBEB_OK) {
          LOGE(
              "AudioInputSource %p, cannot stop its audio input stream! The "
              "stream is going to be destroyed forcefully",
              self.get());
        }
        self->mStream = nullptr;
      })));
}

AudioSegment AudioInputSource::GetAudioSegment(TrackTime aDuration,
                                               Consumer aConsumer) {
  if (aConsumer == Consumer::Changed) {
    // Reset queue's consumer thread to acquire its mReadIndex on the new
    // thread.
    mSPSCQueue.ResetConsumerThreadId();
  }

  AudioSegment raw;
  Maybe<media::TimeUnit> latency;
  while (mSPSCQueue.AvailableRead()) {
    Data data;
    DebugOnly<int> reads = mSPSCQueue.Dequeue(&data, 1);
    MOZ_ASSERT(reads);
    MOZ_ASSERT(!data.is<Empty>());
    if (data.is<AudioChunk>()) {
      raw.AppendAndConsumeChunk(std::move(data.as<AudioChunk>()));
    } else if (data.is<LatencyChangeData>()) {
      latency = Some(data.as<LatencyChangeData>().mLatency);
    }
  }

  if (latency) {
    mDriftCorrector.SetSourceLatency(*latency);
  }
  return mDriftCorrector.RequestFrames(raw, static_cast<uint32_t>(aDuration));
}

long AudioInputSource::DataCallback(const void* aBuffer, long aFrames) {
  TRACE_AUDIO_CALLBACK_BUDGET("AudioInputSource real-time budget", aFrames,
                              mRate);
  TRACE("AudioInputSource::DataCallback");

  const AudioDataValue* source =
      reinterpret_cast<const AudioDataValue*>(aBuffer);

  AudioChunk c = AudioChunk::FromInterleavedBuffer(
      source, static_cast<size_t>(aFrames), mChannelCount, mPrincipalHandle);

  // Reset queue's producer to avoid hitting the assertion for checking the
  // consistency of mSPSCQueue's mProducerId in Enqueue. This can happen when:
  // 1) cubeb stream is reinitialized behind the scenes for the device changed
  // events, e.g., users plug/unplug a TRRS mic into/from the built-in jack port
  // of some old macbooks.
  // 2) After Start() to Stop() cycle finishes, user call Start() again.
  if (CheckThreadIdChanged()) {
    mSPSCQueue.ResetProducerThreadId();
    if (!mSandboxed) {
      CallbackThreadRegistry::Get()->Register(mAudioThreadId,
                                              "NativeAudioCallback");
    }
  }

  Data data(c);
  int writes = mSPSCQueue.Enqueue(data);
  if (writes == 0) {
    LOGW("AudioInputSource %p, buffer is full. Dropping %ld frames", this,
         aFrames);
  } else {
    LOGV("AudioInputSource %p, enqueue %ld frames (%d AudioChunks)", this,
         aFrames, writes);
  }
  return aFrames;
}

void AudioInputSource::StateCallback(cubeb_state aState) {
  EventListener::State state;
  if (aState == CUBEB_STATE_STARTED) {
    LOG("AudioInputSource %p, stream started", this);
    state = EventListener::State::Started;
  } else if (aState == CUBEB_STATE_STOPPED) {
    LOG("AudioInputSource %p, stream stopped", this);
    state = EventListener::State::Stopped;
  } else if (aState == CUBEB_STATE_DRAINED) {
    LOG("AudioInputSource %p, stream is drained", this);
    state = EventListener::State::Drained;
  } else {
    MOZ_ASSERT(aState == CUBEB_STATE_ERROR);
    LOG("AudioInputSource %p, error happend", this);
    state = EventListener::State::Error;
  }
  // This can be called on any thread, so we forward the event to main thread
  // first.
  NS_DispatchToMainThread(
      NS_NewRunnableFunction(__func__, [self = RefPtr(this), s = state] {
        self->mEventListener->AudioStateCallback(self->mId, s);
      }));
}

void AudioInputSource::DeviceChangedCallback() {
  LOG("AudioInputSource %p, device changed", this);
  // This can be called on any thread, so we forward the event to main thread
  // first.
  NS_DispatchToMainThread(
      NS_NewRunnableFunction(__func__, [self = RefPtr(this)] {
        self->mEventListener->AudioDeviceChanged(self->mId);
      }));
}

bool AudioInputSource::CheckThreadIdChanged() {
  ProfilerThreadId id = profiler_current_thread_id();
  if (id != mAudioThreadId) {
    mAudioThreadId = id;
    return true;
  }
  return false;
}

#undef LOG_INTERNAL
#undef LOG
#undef LOGW
#undef LOGE
#undef LOGV

}  // namespace mozilla
