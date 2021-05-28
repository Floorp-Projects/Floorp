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

// Amount of audio frames we will be processing ahead of use
static const int32_t LOW_AUDIO_USECS = 300000;

using media::TimeUnit;

AudioSink::AudioSink(AbstractThread* aThread,
                     MediaQueue<AudioData>& aAudioQueue,
                     const TimeUnit& aStartTime, const AudioInfo& aInfo,
                     AudioDeviceInfo* aAudioDevice)
    : mStartTime(aStartTime),
      mInfo(aInfo),
      mAudioDevice(aAudioDevice),
      mPlaying(true),
      mMonitor("AudioSink"),
      mWritten(0),
      mErrored(false),
      mOwnerThread(aThread),
      mProcessedQueueLength(0),
      mFramesParsed(0),
      mOutputRate(DecideAudioPlaybackSampleRate(aInfo)),
      mOutputChannels(DecideAudioPlaybackChannels(aInfo)),
      mAudibilityMonitor(
          mOutputRate,
          StaticPrefs::dom_media_silence_duration_for_audibility()),
      mIsAudioDataAudible(false),
      mAudioQueue(aAudioQueue) {}

AudioSink::~AudioSink() = default;

Result<already_AddRefed<MediaSink::EndedPromise>, nsresult> AudioSink::Start(
    const PlaybackParams& aParams) {
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());

  mAudioQueueListener = mAudioQueue.PushEvent().Connect(
      mOwnerThread, this, &AudioSink::OnAudioPushed);
  mAudioQueueFinishListener = mAudioQueue.FinishEvent().Connect(
      mOwnerThread, this, &AudioSink::NotifyAudioNeeded);
  mProcessedQueueListener = mProcessedQueue.PopFrontEvent().Connect(
      mOwnerThread, this, &AudioSink::OnAudioPopped);

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
  int64_t total;
  {
    MonitorAutoLock mon(mMonitor);
    total = mWritten + (mCursor.get() ? mCursor->Available() : 0);
  }
  return mProcessedQueue.GetSize() ||
         (mAudioStream && mAudioStream->GetPositionInFrames() + 1 < total);
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
  // Shutdown audio sink doesn't mean the playback is going to stop, so if we
  // simply discard these data, then we will no longer be able to play them.
  // Eg. we change to sink to capture-based sink that will need to continue play
  // remaining data from the audio queue.
  {
    MonitorAutoLock mon(mMonitor);
    while (mProcessedQueue.GetSize() > 0) {
      RefPtr<AudioData> audio = mProcessedQueue.PopBack();
      if (audio == mCurrentData) {
        break;
      }
      mAudioQueue.PushFront(audio);
    }
    if (mCurrentData) {
      uint32_t unplayedFrames = mCursor->Available();
      // If we've consumed some partial content from the first audio data, then
      // we have to adjust its data offset and frames number in order not to
      // play the same content again.
      if (unplayedFrames > 0 && unplayedFrames < mCurrentData->Frames()) {
        const uint32_t orginalFrames = mCurrentData->Frames();
        const uint32_t offsetFrames = mCurrentData->Frames() - unplayedFrames;
        Unused << mCurrentData->SetTrimWindow(
            {mCurrentData->mTime + FramesToTimeUnit(offsetFrames, mOutputRate),
             mCurrentData->GetEndTime()});
        SINK_LOG_V("After adjustment, audio frame from %u to %u", orginalFrames,
                   mCurrentData->Frames());
      }
      mAudioQueue.PushFront(mCurrentData);
    }
    MOZ_ASSERT(mProcessedQueue.GetSize() == 0);
  }
  mProcessedQueue.Finish();
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
  mAudioStream = new AudioStream(*this);
  // When AudioQueue is empty, there is no way to know the channel layout of
  // the coming audio data, so we use the predefined channel map instead.
  AudioConfig::ChannelLayout::ChannelMap channelMap =
      mConverter ? mConverter->OutputConfig().Layout().Map()
                 : AudioConfig::ChannelLayout(mOutputChannels).Map();
  // The layout map used here is already processed by mConverter with
  // mOutputChannels into SMPTE format, so there is no need to worry if
  // StaticPrefs::accessibility_monoaudio_enable() or
  // StaticPrefs::media_forcestereo_enabled() is applied.
  nsresult rv = mAudioStream->Init(mOutputChannels, channelMap, mOutputRate,
                                   mAudioDevice);
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
  int64_t written;
  {
    MonitorAutoLock mon(mMonitor);
    written = mWritten;
  }
  TimeUnit played = FramesToTimeUnit(written, mOutputRate) + mStartTime;
  if (!played.IsValid()) {
    NS_WARNING("Int overflow calculating audio end time");
    return TimeUnit::Zero();
  }
  // As we may be resampling, rounding errors may occur. Ensure we never get
  // past the original end time.
  return std::min(mLastEndTime, played);
}

UniquePtr<AudioStream::Chunk> AudioSink::PopFrames(uint32_t aFrames) {
  class Chunk : public AudioStream::Chunk {
   public:
    Chunk(AudioData* aBuffer, uint32_t aFrames, AudioDataValue* aData)
        : mBuffer(aBuffer), mFrames(aFrames), mData(aData) {}
    Chunk() : mFrames(0), mData(nullptr) {}
    const AudioDataValue* Data() const override { return mData; }
    uint32_t Frames() const override { return mFrames; }
    uint32_t Channels() const override {
      return mBuffer ? mBuffer->mChannels : 0;
    }
    uint32_t Rate() const override { return mBuffer ? mBuffer->mRate : 0; }
    AudioDataValue* GetWritable() const override { return mData; }

   private:
    const RefPtr<AudioData> mBuffer;
    const uint32_t mFrames;
    AudioDataValue* const mData;
  };

  bool needPopping = false;
  if (!mCurrentData) {
    // No data in the queue. Return an empty chunk.
    if (!mProcessedQueue.GetSize()) {
      return MakeUnique<Chunk>();
    }

    // We need to update our values prior popping the processed queue in
    // order to prevent the pop event to fire too early (prior
    // mProcessedQueueLength being updated) or prevent HasUnplayedFrames
    // to incorrectly return true during the time interval betweeen the
    // when mProcessedQueue is read and mWritten is updated.
    needPopping = true;
    {
      MonitorAutoLock mon(mMonitor);
      mCurrentData = mProcessedQueue.PeekFront();
      mCursor = MakeUnique<AudioBufferCursor>(mCurrentData->Data(),
                                              mCurrentData->mChannels,
                                              mCurrentData->Frames());
    }
    MOZ_ASSERT(mCurrentData->Frames() > 0);
    mProcessedQueueLength -=
        FramesToUsecs(mCurrentData->Frames(), mOutputRate).value();
  }

  auto framesToPop = std::min(aFrames, mCursor->Available());

  SINK_LOG_V("playing audio at time=%" PRId64 " offset=%u length=%u",
             mCurrentData->mTime.ToMicroseconds(),
             mCurrentData->Frames() - mCursor->Available(), framesToPop);

#ifdef MOZ_GECKO_PROFILER
  mOwnerThread->Dispatch(NS_NewRunnableFunction(
      "AudioSink:AddMarker",
      [startTime = mCurrentData->mTime.ToMicroseconds(),
       endTime = mCurrentData->GetEndTime().ToMicroseconds()] {
        PROFILER_MARKER("PlayAudio", MEDIA_PLAYBACK, {}, MediaSampleMarker,
                        startTime, endTime);
      }));
#endif  // MOZ_GECKO_PROFILER

  UniquePtr<AudioStream::Chunk> chunk =
      MakeUnique<Chunk>(mCurrentData, framesToPop, mCursor->Ptr());

  {
    MonitorAutoLock mon(mMonitor);
    mWritten += framesToPop;
    mCursor->Advance(framesToPop);
    // All frames are popped. Reset mCurrentData so we can pop new elements from
    // the audio queue in next calls to PopFrames().
    if (!mCursor->Available()) {
      mCurrentData = nullptr;
    }
  }

  if (needPopping) {
    // We can now safely pop the audio packet from the processed queue.
    // This will fire the popped event, triggering a call to NotifyAudioNeeded.
    RefPtr<AudioData> releaseMe = mProcessedQueue.PopFront();
    CheckIsAudible(releaseMe);
  }

  return chunk;
}

bool AudioSink::Ended() const {
  // Return true when error encountered so AudioStream can start draining.
  return mProcessedQueue.IsFinished() || mErrored;
}

void AudioSink::CheckIsAudible(const AudioData* aData) {
  MOZ_ASSERT(aData);

  mAudibilityMonitor.Process(aData);
  bool isAudible = mAudibilityMonitor.RecentlyAudible();

  if (isAudible != mIsAudioDataAudible) {
    mIsAudioDataAudible = isAudible;
    mAudibleEvent.Notify(mIsAudioDataAudible);
  }
}

void AudioSink::OnAudioPopped(const RefPtr<AudioData>& aSample) {
  SINK_LOG_V("AudioStream has used an audio packet.");
  NotifyAudioNeeded();
}

void AudioSink::OnAudioPushed(const RefPtr<AudioData>& aSample) {
  SINK_LOG_V("One new audio packet available.");
  NotifyAudioNeeded();
}

void AudioSink::NotifyAudioNeeded() {
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn(),
             "Not called from the owner's thread");

  // Always ensure we have two processed frames pending to allow for processing
  // latency.
  while (mAudioQueue.GetSize() &&
         (mAudioQueue.IsFinished() || mProcessedQueueLength < LOW_AUDIO_USECS ||
          mProcessedQueue.GetSize() < 2)) {
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

      DrainConverter();

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
      // time.
      missingFrames = std::min<int64_t>(INT32_MAX, missingFrames.value());
      mFramesParsed += missingFrames.value();

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
    DrainConverter();
    mProcessedQueue.Finish();
  }
}

uint32_t AudioSink::PushProcessedAudio(AudioData* aData) {
  if (!aData || !aData->Frames()) {
    return 0;
  }
  mProcessedQueue.Push(aData);
  mProcessedQueueLength += FramesToUsecs(aData->Frames(), mOutputRate).value();
  return aData->Frames();
}

already_AddRefed<AudioData> AudioSink::CreateAudioFromBuffer(
    AlignedAudioBuffer&& aBuffer, AudioData* aReference) {
  uint32_t frames = aBuffer.Length() / mOutputChannels;
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

  uint32_t frames = convertedData.Length() / mOutputChannels;
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
  mProcessedQueue.Push(data);
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
