/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioSink.h"
#include "AudioConverter.h"
#include "AudioDeviceInfo.h"
#include "MediaQueue.h"
#include "VideoUtils.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/ProfilerMarkerTypes.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsPrintfCString.h"
#include "Tracing.h"

namespace mozilla {

extern LazyLogModule gMediaDecoderLog;
#define SINK_LOG(msg, ...)                   \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Debug, \
          ("AudioSink=%p " msg, this, ##__VA_ARGS__))
#define SINK_LOG_V(msg, ...)                   \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Verbose, \
          ("AudioSink=%p " msg, this, ##__VA_ARGS__))

// The amount of audio frames that is used to fuzz rounding errors.
static const int64_t AUDIO_FUZZ_FRAMES = 1;

using media::TimeUnit;

AudioSink::AudioSink(AbstractThread* aThread,
                     MediaQueue<AudioData>& aAudioQueue,
                     const TimeUnit& aStartTime, const AudioInfo& aInfo,
                     AudioDeviceInfo* aAudioDevice)
    : mStartTime(aStartTime),
      mInfo(aInfo),
      mAudioDevice(aAudioDevice),
      mPlaying(true),
      mWritten(0),
      mErrored(false),
      mOwnerThread(aThread),
      mFramesParsed(0),
      mOutputRate(DecideAudioPlaybackSampleRate(aInfo)),
      mOutputChannels(DecideAudioPlaybackChannels(aInfo)),
      mAudibilityMonitor(
          mOutputRate,
          StaticPrefs::dom_media_silence_duration_for_audibility()),
      mIsAudioDataAudible(false),
      mProcessedQueueFinished(false),
      mAudioQueue(aAudioQueue),
      mProcessedQueueThresholdMS(
          StaticPrefs::media_audio_audiosink_threshold_ms()) {
  // Twice the limit that trigger a refill.
  float capacitySeconds = mProcessedQueueThresholdMS / 1000.f * 2;
  mProcessedSPSCQueue =
      MakeUnique<SPSCQueue<AudioDataValue>>(static_cast<uint32_t>(
          capacitySeconds * static_cast<float>(mOutputChannels * mOutputRate)));
  SINK_LOG("Ringbuffer has space for %u elements (%lf seconds)",
           mProcessedSPSCQueue->Capacity(), capacitySeconds);
}

AudioSink::~AudioSink() = default;

Result<already_AddRefed<MediaSink::EndedPromise>, nsresult> AudioSink::Start(
    const PlaybackParams& aParams) {
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());

  mAudioQueueListener = mAudioQueue.PushEvent().Connect(
      mOwnerThread, this, &AudioSink::OnAudioPushed);
  mAudioQueueFinishListener = mAudioQueue.FinishEvent().Connect(
      mOwnerThread, this, &AudioSink::NotifyAudioNeeded);
  mProcessedQueueListener =
      mAudioPopped.Connect(mOwnerThread, this, &AudioSink::OnAudioPopped);

  // To ensure at least one audio packet will be popped from AudioQueue and
  // ready to be played.
  NotifyAudioNeeded();
  nsresult rv = InitializeAudioStream(aParams);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }

  return mAudioStream->Start();
}

TimeUnit AudioSink::GetPosition() {
  int64_t tmp;
  if (mAudioStream && (tmp = mAudioStream->GetPosition()) >= 0) {
    TimeUnit pos = TimeUnit::FromMicroseconds(tmp);
    NS_ASSERTION(pos >= mLastGoodPosition,
                 "AudioStream position shouldn't go backward");
    TimeUnit tmp = mStartTime + pos;
    if (!tmp.IsValid()) {
      mErrored = true;
      return mStartTime + mLastGoodPosition;
    }
    // Update the last good position when we got a good one.
    if (pos >= mLastGoodPosition) {
      mLastGoodPosition = pos;
    }
  }

  return mStartTime + mLastGoodPosition;
}

bool AudioSink::HasUnplayedFrames() {
  // Experimentation suggests that GetPositionInFrames() is zero-indexed,
  // so we need to add 1 here before comparing it to mWritten.
  return mProcessedSPSCQueue->AvailableRead() ||
         (mAudioStream && mAudioStream->GetPositionInFrames() + 1 < mWritten);
}

void AudioSink::Shutdown() {
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());

  mAudioQueueListener.Disconnect();
  mAudioQueueFinishListener.Disconnect();
  mProcessedQueueListener.Disconnect();

  if (mAudioStream) {
    mAudioStream->Shutdown();
    mAudioStream = nullptr;
  }
  mProcessedQueueFinished = true;
}

void AudioSink::SetVolume(double aVolume) {
  if (mAudioStream) {
    mAudioStream->SetVolume(aVolume);
  }
}

void AudioSink::SetStreamName(const nsAString& aStreamName) {
  if (mAudioStream) {
    mAudioStream->SetStreamName(aStreamName);
  }
}

void AudioSink::SetPlaybackRate(double aPlaybackRate) {
  MOZ_ASSERT(aPlaybackRate != 0,
             "Don't set the playbackRate to 0 on AudioStream");
  if (mAudioStream) {
    mAudioStream->SetPlaybackRate(aPlaybackRate);
  }
}

void AudioSink::SetPreservesPitch(bool aPreservesPitch) {
  if (mAudioStream) {
    mAudioStream->SetPreservesPitch(aPreservesPitch);
  }
}

void AudioSink::SetPlaying(bool aPlaying) {
  if (!mAudioStream || mAudioStream->IsPlaybackCompleted() ||
      mPlaying == aPlaying) {
    return;
  }
  // pause/resume AudioStream as necessary.
  if (!aPlaying) {
    mAudioStream->Pause();
  } else if (aPlaying) {
    mAudioStream->Resume();
  }
  mPlaying = aPlaying;
}

nsresult AudioSink::InitializeAudioStream(const PlaybackParams& aParams) {
  // When AudioQueue is empty, there is no way to know the channel layout of
  // the coming audio data, so we use the predefined channel map instead.
  AudioConfig::ChannelLayout::ChannelMap channelMap =
      mConverter ? mConverter->OutputConfig().Layout().Map()
                 : AudioConfig::ChannelLayout(mOutputChannels).Map();
  // The layout map used here is already processed by mConverter with
  // mOutputChannels into SMPTE format, so there is no need to worry if
  // StaticPrefs::accessibility_monoaudio_enable() or
  // StaticPrefs::media_forcestereo_enabled() is applied.
  mAudioStream =
      new AudioStream(*this, mOutputRate, mOutputChannels, channelMap);
  nsresult rv = mAudioStream->Init(mAudioDevice);
  if (NS_FAILED(rv)) {
    mAudioStream->Shutdown();
    mAudioStream = nullptr;
    return rv;
  }

  // Set playback params before calling Start() so they can take effect
  // as soon as the 1st DataCallback of the AudioStream fires.
  mAudioStream->SetVolume(aParams.mVolume);
  mAudioStream->SetPlaybackRate(aParams.mPlaybackRate);
  mAudioStream->SetPreservesPitch(aParams.mPreservesPitch);
  return NS_OK;
}

TimeUnit AudioSink::GetEndTime() const {
  TimeUnit played = FramesToTimeUnit(mWritten, mOutputRate) + mStartTime;
  if (!played.IsValid()) {
    NS_WARNING("Int overflow calculating audio end time");
    return TimeUnit::Zero();
  }
  // As we may be resampling, rounding errors may occur. Ensure we never get
  // past the original end time.
  return std::min(mLastEndTime, played);
}

uint32_t AudioSink::PopFrames(AudioDataValue* aBuffer, uint32_t aFrames,
                              bool aAudioThreadChanged) {
  // This is safe, because we have the guarantee, by the OS, that audio
  // callbacks are never called concurrently. Audio thread changes can only
  // happen when not using cubeb remoting, and often when changing audio device
  // at the system level.
  if (aAudioThreadChanged) {
    mProcessedSPSCQueue->ResetThreadIds();
  }

  TRACE_COMMENT("AudioSink::PopFrames", "%u frames (ringbuffer: %u/%u)",
                aFrames, SampleToFrame(mProcessedSPSCQueue->AvailableRead()),
                SampleToFrame(mProcessedSPSCQueue->Capacity()));

  const int samplesToPop = static_cast<int>(aFrames * mOutputChannels);
  const int samplesRead = mProcessedSPSCQueue->Dequeue(aBuffer, samplesToPop);
  MOZ_ASSERT(samplesRead % mOutputChannels == 0);
  mWritten += SampleToFrame(samplesRead);
  if (samplesRead != samplesToPop) {
    if (Ended()) {
      SINK_LOG("Last PopFrames -- Source ended.");
    } else {
      NS_WARNING("Underrun when popping samples from audiosink ring buffer.");
      TRACE_COMMENT("AudioSink::PopFrames", "Underrun %u frames missing",
                    SampleToFrame(samplesToPop - samplesRead));
    }
    // silence the rest
    PodZero(aBuffer + samplesRead, samplesToPop - samplesRead);
  }

  mAudioPopped.Notify();

  SINK_LOG_V("Popping %u frames. Remaining in ringbuffer %u / %u\n", aFrames,
             SampleToFrame(mProcessedSPSCQueue->AvailableRead()),
             SampleToFrame(mProcessedSPSCQueue->Capacity()));

  // Don't consider the silence added because of an underrun.
  CheckIsAudible(Span(aBuffer, samplesRead), mOutputChannels);

  return SampleToFrame(samplesRead);
}

bool AudioSink::Ended() const {
  // Return true when error encountered so AudioStream can start draining.
  return mProcessedQueueFinished || mErrored;
}

void AudioSink::CheckIsAudible(const Span<AudioDataValue>& aInterleaved,
                               size_t aChannel) {
  mAudibilityMonitor.ProcessInterleaved(aInterleaved, aChannel);
  bool isAudible = mAudibilityMonitor.RecentlyAudible();

  if (isAudible != mIsAudioDataAudible) {
    mIsAudioDataAudible = isAudible;
    mAudibleEvent.Notify(mIsAudioDataAudible);
  }
}

void AudioSink::OnAudioPopped() {
  SINK_LOG_V("AudioStream has used an audio packet.");
  NotifyAudioNeeded();
}

void AudioSink::OnAudioPushed(const RefPtr<AudioData>& aSample) {
  SINK_LOG_V("One new audio packet available.");
  NotifyAudioNeeded();
}

uint32_t AudioSink::AudioQueuedInRingBufferMS() const {
  return static_cast<uint32_t>(
      1000 * SampleToFrame(mProcessedSPSCQueue->AvailableRead()) / mOutputRate);
}

uint32_t AudioSink::SampleToFrame(uint32_t aSamples) const {
  return aSamples / mOutputChannels;
}

void AudioSink::NotifyAudioNeeded() {
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn(),
             "Not called from the owner's thread");

  while (mAudioQueue.GetSize() &&
         AudioQueuedInRingBufferMS() < mProcessedQueueThresholdMS) {
    // Check if there's room in our ring buffer.
    if (mAudioQueue.PeekFront()->Frames() >
        SampleToFrame(mProcessedSPSCQueue->AvailableWrite())) {
      SINK_LOG_V("Can't push %u frames. In ringbuffer %u / %u\n",
                 mAudioQueue.PeekFront()->Frames(),
                 SampleToFrame(mProcessedSPSCQueue->AvailableRead()),
                 SampleToFrame(mProcessedSPSCQueue->Capacity()));
      return;
    }
    SINK_LOG_V("Pushing %u frames. In ringbuffer %u / %u\n",
               mAudioQueue.PeekFront()->Frames(),
               SampleToFrame(mProcessedSPSCQueue->AvailableRead()),
               SampleToFrame(mProcessedSPSCQueue->Capacity()));
    RefPtr<AudioData> data = mAudioQueue.PopFront();

    // Ignore the element with 0 frames and try next.
    if (!data->Frames()) {
      continue;
    }

    if (!mConverter ||
        (data->mRate != mConverter->InputConfig().Rate() ||
         data->mChannels != mConverter->InputConfig().Channels())) {
      SINK_LOG_V("Audio format changed from %u@%uHz to %u@%uHz",
                 mConverter ? mConverter->InputConfig().Channels() : 0,
                 mConverter ? mConverter->InputConfig().Rate() : 0,
                 data->mChannels, data->mRate);

      DrainConverter(SampleToFrame(mProcessedSPSCQueue->AvailableWrite()));

      // mFramesParsed indicates the current playtime in frames at the current
      // input sampling rate. Recalculate it per the new sampling rate.
      if (mFramesParsed) {
        // We minimize overflow.
        uint32_t oldRate = mConverter->InputConfig().Rate();
        uint32_t newRate = data->mRate;
        CheckedInt64 result = SaferMultDiv(mFramesParsed, newRate, oldRate);
        if (!result.isValid()) {
          NS_WARNING("Int overflow in AudioSink");
          mErrored = true;
          return;
        }
        mFramesParsed = result.value();
      }

      const AudioConfig::ChannelLayout inputLayout =
          data->mChannelMap
              ? AudioConfig::ChannelLayout::SMPTEDefault(data->mChannelMap)
              : AudioConfig::ChannelLayout(data->mChannels);
      const AudioConfig::ChannelLayout outputLayout =
          mOutputChannels == data->mChannels
              ? inputLayout
              : AudioConfig::ChannelLayout(mOutputChannels);
      AudioConfig inConfig =
          AudioConfig(inputLayout, data->mChannels, data->mRate);
      AudioConfig outConfig =
          AudioConfig(outputLayout, mOutputChannels, mOutputRate);
      if (!AudioConverter::CanConvert(inConfig, outConfig)) {
        mErrored = true;
        return;
      }
      mConverter = MakeUnique<AudioConverter>(inConfig, outConfig);
    }

    // See if there's a gap in the audio. If there is, push silence into the
    // audio hardware, so we can play across the gap.
    // Calculate the timestamp of the next chunk of audio in numbers of
    // samples.
    CheckedInt64 sampleTime =
        TimeUnitToFrames(data->mTime - mStartTime, data->mRate);
    // Calculate the number of frames that have been pushed onto the audio
    // hardware.
    CheckedInt64 missingFrames = sampleTime - mFramesParsed;

    if (!missingFrames.isValid() || !sampleTime.isValid()) {
      NS_WARNING("Int overflow in AudioSink");
      mErrored = true;
      return;
    }

    if (missingFrames.value() > AUDIO_FUZZ_FRAMES) {
      // The next audio packet begins some time after the end of the last packet
      // we pushed to the audio hardware. We must push silence into the audio
      // hardware so that the next audio packet begins playback at the correct
      // time. But don't push more than the ring buffer can receive.
      missingFrames = std::min<int64_t>(
          std::min<int64_t>(INT32_MAX, missingFrames.value()),
          SampleToFrame(mProcessedSPSCQueue->AvailableWrite()));
      mFramesParsed += missingFrames.value();

      SINK_LOG("Gap in the audio input, push %" PRId64 " frames of silence",
               missingFrames.value());

      RefPtr<AudioData> silenceData;
      AlignedAudioBuffer silenceBuffer(missingFrames.value() * data->mChannels);
      if (!silenceBuffer) {
        NS_WARNING("OOM in AudioSink");
        mErrored = true;
        return;
      }
      if (mConverter->InputConfig() != mConverter->OutputConfig()) {
        AlignedAudioBuffer convertedData =
            mConverter->Process(AudioSampleBuffer(std::move(silenceBuffer)))
                .Forget();
        silenceData = CreateAudioFromBuffer(std::move(convertedData), data);
      } else {
        silenceData = CreateAudioFromBuffer(std::move(silenceBuffer), data);
      }
      TRACE("Pushing silence");
      PushProcessedAudio(silenceData);
    }

    mLastEndTime = data->GetEndTime();
    mFramesParsed += data->Frames();

    if (mConverter->InputConfig() != mConverter->OutputConfig()) {
      AlignedAudioBuffer buffer(data->MoveableData());
      AlignedAudioBuffer convertedData =
          mConverter->Process(AudioSampleBuffer(std::move(buffer))).Forget();
      data = CreateAudioFromBuffer(std::move(convertedData), data);
    }
    if (PushProcessedAudio(data)) {
      mLastProcessedPacket = Some(data);
    }
  }

  if (mAudioQueue.IsFinished()) {
    // We have reached the end of the data, drain the resampler.
    DrainConverter(SampleToFrame(mProcessedSPSCQueue->AvailableWrite()));
    mProcessedQueueFinished = true;
  }
}

uint32_t AudioSink::PushProcessedAudio(AudioData* aData) {
  if (!aData || !aData->Frames()) {
    return 0;
  }
  int framesToEnqueue = static_cast<int>(aData->Frames() * aData->mChannels);
  TRACE_COMMENT("AudioSink::PushProcessedAudio", "%u frames (%u/%u)",
                framesToEnqueue,
                SampleToFrame(mProcessedSPSCQueue->AvailableWrite()),
                SampleToFrame(mProcessedSPSCQueue->Capacity()));
  DebugOnly<int> rv =
      mProcessedSPSCQueue->Enqueue(aData->Data().Elements(), framesToEnqueue);
  NS_WARNING_ASSERTION(
      rv == static_cast<int>(aData->Frames() * aData->mChannels),
      "AudioSink ring buffer over-run, can't push new data");
  return aData->Frames();
}

already_AddRefed<AudioData> AudioSink::CreateAudioFromBuffer(
    AlignedAudioBuffer&& aBuffer, AudioData* aReference) {
  uint32_t frames = SampleToFrame(aBuffer.Length());
  if (!frames) {
    return nullptr;
  }
  auto duration = FramesToTimeUnit(frames, mOutputRate);
  if (!duration.IsValid()) {
    NS_WARNING("Int overflow in AudioSink");
    mErrored = true;
    return nullptr;
  }
  RefPtr<AudioData> data =
      new AudioData(aReference->mOffset, aReference->mTime, std::move(aBuffer),
                    mOutputChannels, mOutputRate);
  MOZ_DIAGNOSTIC_ASSERT(duration == data->mDuration, "must be equal");
  return data.forget();
}

uint32_t AudioSink::DrainConverter(uint32_t aMaxFrames) {
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());

  if (!mConverter || !mLastProcessedPacket || !aMaxFrames) {
    // nothing to drain.
    return 0;
  }

  RefPtr<AudioData> lastPacket = mLastProcessedPacket.ref();
  mLastProcessedPacket.reset();

  // To drain we simply provide an empty packet to the audio converter.
  AlignedAudioBuffer convertedData =
      mConverter->Process(AudioSampleBuffer(AlignedAudioBuffer())).Forget();

  uint32_t frames = SampleToFrame(convertedData.Length());
  if (!convertedData.SetLength(std::min(frames, aMaxFrames) *
                               mOutputChannels)) {
    // This can never happen as we were reducing the length of convertData.
    mErrored = true;
    return 0;
  }

  RefPtr<AudioData> data =
      CreateAudioFromBuffer(std::move(convertedData), lastPacket);
  if (!data) {
    return 0;
  }
  int framesToEnqueue = static_cast<int>(data->mChannels * data->Frames());
  DebugOnly<int> rv =
      mProcessedSPSCQueue->Enqueue(data->Data().Elements(), framesToEnqueue);
  NS_WARNING_ASSERTION(
      rv == static_cast<int>(data->Frames() * data->mChannels),
      "AudioSink ring buffer over-run when draining, can't push new data");
  return data->Frames();
}

void AudioSink::GetDebugInfo(dom::MediaSinkDebugInfo& aInfo) {
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  aInfo.mAudioSinkWrapper.mAudioSink.mStartTime = mStartTime.ToMicroseconds();
  aInfo.mAudioSinkWrapper.mAudioSink.mLastGoodPosition =
      mLastGoodPosition.ToMicroseconds();
  aInfo.mAudioSinkWrapper.mAudioSink.mIsPlaying = mPlaying;
  aInfo.mAudioSinkWrapper.mAudioSink.mOutputRate = mOutputRate;
  aInfo.mAudioSinkWrapper.mAudioSink.mWritten = mWritten;
  aInfo.mAudioSinkWrapper.mAudioSink.mHasErrored = bool(mErrored);
  aInfo.mAudioSinkWrapper.mAudioSink.mPlaybackComplete =
      mAudioStream ? mAudioStream->IsPlaybackCompleted() : false;
}

}  // namespace mozilla
