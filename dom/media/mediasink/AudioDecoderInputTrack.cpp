/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioDecoderInputTrack.h"

#include "MediaData.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_media.h"
#include "Tracing.h"

// Use abort() instead of exception in SoundTouch.
#define ST_NO_EXCEPTION_HANDLING 1
#include "soundtouch/SoundTouchFactory.h"

namespace mozilla {

extern LazyLogModule gMediaDecoderLog;

#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Debug, \
          ("AudioDecoderInputTrack=%p " msg, this, ##__VA_ARGS__))

#define LOG_M(msg, this, ...)                \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Debug, \
          ("AudioDecoderInputTrack=%p " msg, this, ##__VA_ARGS__))

/* static */
AudioDecoderInputTrack* AudioDecoderInputTrack::Create(
    MediaTrackGraph* aGraph, nsISerialEventTarget* aDecoderThread,
    const AudioInfo& aInfo, float aPlaybackRate, float aVolume,
    bool aPreservesPitch) {
  MOZ_ASSERT(aGraph);
  MOZ_ASSERT(aDecoderThread);
  AudioDecoderInputTrack* track =
      new AudioDecoderInputTrack(aDecoderThread, aGraph->GraphRate(), aInfo,
                                 aPlaybackRate, aVolume, aPreservesPitch);
  aGraph->AddTrack(track);
  return track;
}

AudioDecoderInputTrack::AudioDecoderInputTrack(
    nsISerialEventTarget* aDecoderThread, TrackRate aGraphRate,
    const AudioInfo& aInfo, float aPlaybackRate, float aVolume,
    bool aPreservesPitch)
    : ProcessedMediaTrack(aGraphRate, MediaSegment::AUDIO, new AudioSegment()),
      mDecoderThread(aDecoderThread),
      mResamplerChannelCount(0),
      mInitialInputChannels(aInfo.mChannels),
      mInputSampleRate(aInfo.mRate),
      mDelayedScheduler(mDecoderThread),
      mPlaybackRate(aPlaybackRate),
      mVolume(aVolume),
      mPreservesPitch(aPreservesPitch) {}

bool AudioDecoderInputTrack::ConvertAudioDataToSegment(
    AudioData* aAudio, AudioSegment& aSegment,
    const PrincipalHandle& aPrincipalHandle) {
  AssertOnDecoderThread();
  MOZ_ASSERT(aAudio);
  MOZ_ASSERT(aSegment.IsEmpty());
  if (!aAudio->Frames()) {
    LOG("Ignore audio with zero frame");
    return false;
  }

  aAudio->EnsureAudioBuffer();
  RefPtr<SharedBuffer> buffer = aAudio->mAudioBuffer;
  AudioDataValue* bufferData = static_cast<AudioDataValue*>(buffer->Data());
  AutoTArray<const AudioDataValue*, 2> channels;
  for (uint32_t i = 0; i < aAudio->mChannels; ++i) {
    channels.AppendElement(bufferData + i * aAudio->Frames());
  }
  aSegment.AppendFrames(buffer.forget(), channels, aAudio->Frames(),
                        aPrincipalHandle);
  const TrackRate newInputRate = static_cast<TrackRate>(aAudio->mRate);
  if (newInputRate != mInputSampleRate) {
    LOG("Input sample rate changed %u -> %u", mInputSampleRate, newInputRate);
    mInputSampleRate = newInputRate;
    mResampler.own(nullptr);
    mResamplerChannelCount = 0;
  }
  if (mInputSampleRate != GraphImpl()->GraphRate()) {
    aSegment.ResampleChunks(mResampler, &mResamplerChannelCount,
                            mInputSampleRate, GraphImpl()->GraphRate());
  }
  return aSegment.GetDuration() > 0;
}

void AudioDecoderInputTrack::AppendData(
    AudioData* aAudio, const PrincipalHandle& aPrincipalHandle) {
  AssertOnDecoderThread();
  MOZ_ASSERT(aAudio);
  nsTArray<RefPtr<AudioData>> audio;
  audio.AppendElement(aAudio);
  AppendData(audio, aPrincipalHandle);
}

void AudioDecoderInputTrack::AppendData(
    nsTArray<RefPtr<AudioData>>& aAudioArray,
    const PrincipalHandle& aPrincipalHandle) {
  AssertOnDecoderThread();
  MOZ_ASSERT(!mShutdownSPSCQueue);

  // Batching all new data together in order to push them as a single unit that
  // gives the SPSC queue more spaces.
  for (const auto& audio : aAudioArray) {
    BatchData(audio, aPrincipalHandle);
  }

  // If SPSC queue doesn't have much available capacity now, we would push
  // batched later.
  if (ShouldBatchData()) {
    return;
  }
  PushBatchedDataIfNeeded();
}

bool AudioDecoderInputTrack::ShouldBatchData() const {
  AssertOnDecoderThread();
  // If the SPSC queue has less available capacity than the threshold, then all
  // input audio data should be batched together, in order not to increase the
  // pressure of SPSC queue.
  static const int kThresholdNumerator = 3;
  static const int kThresholdDenominator = 10;
  return mSPSCQueue.AvailableWrite() <
         mSPSCQueue.Capacity() * kThresholdNumerator / kThresholdDenominator;
}

bool AudioDecoderInputTrack::HasBatchedData() const {
  AssertOnDecoderThread();
  return !mBatchedData.mSegment.IsEmpty();
}

void AudioDecoderInputTrack::BatchData(
    AudioData* aAudio, const PrincipalHandle& aPrincipalHandle) {
  AssertOnDecoderThread();
  AudioSegment segment;
  if (!ConvertAudioDataToSegment(aAudio, segment, aPrincipalHandle)) {
    return;
  }
  mBatchedData.mSegment.AppendFrom(&segment);
  if (!mBatchedData.mStartTime.IsValid()) {
    mBatchedData.mStartTime = aAudio->mTime;
  }
  mBatchedData.mEndTime = aAudio->GetEndTime();
  LOG("batched data [%" PRId64 ":%" PRId64 "] sz=%" PRId64,
      aAudio->mTime.ToMicroseconds(), aAudio->GetEndTime().ToMicroseconds(),
      mBatchedData.mSegment.GetDuration());
  DispatchPushBatchedDataIfNeeded();
}

void AudioDecoderInputTrack::DispatchPushBatchedDataIfNeeded() {
  AssertOnDecoderThread();
  MOZ_ASSERT(!mShutdownSPSCQueue);
  // The graph thread runs iteration around per 2~10ms. Doing this to ensure
  // that we can keep consuming data. If the producer stops pushing new data
  // due to MDSM stops decoding, which is because MDSM thinks the data stored
  // in the audio queue are enough. The way to remove those data from the
  // audio queue is driven by us, so we have to keep consuming data.
  // Otherwise, we would get stuck because those batched data would never be
  // consumed.
  static const uint8_t kTimeoutMS = 10;
  TimeStamp target =
      TimeStamp::Now() + TimeDuration::FromMilliseconds(kTimeoutMS);
  mDelayedScheduler.Ensure(
      target,
      [self = RefPtr<AudioDecoderInputTrack>(this), this]() {
        LOG("In the task of DispatchPushBatchedDataIfNeeded");
        mDelayedScheduler.CompleteRequest();
        MOZ_ASSERT(!mShutdownSPSCQueue);
        MOZ_ASSERT(HasBatchedData());
        // The capacity in SPSC is still not enough, so we can't push data now.
        // Retrigger another task to push batched data.
        if (ShouldBatchData()) {
          DispatchPushBatchedDataIfNeeded();
          return;
        }
        PushBatchedDataIfNeeded();
      },
      []() { MOZ_DIAGNOSTIC_ASSERT(false); });
}

void AudioDecoderInputTrack::PushBatchedDataIfNeeded() {
  AssertOnDecoderThread();
  if (!HasBatchedData()) {
    return;
  }
  LOG("Append batched data [%" PRId64 ":%" PRId64 "], available SPSC sz=%u",
      mBatchedData.mStartTime.ToMicroseconds(),
      mBatchedData.mEndTime.ToMicroseconds(), mSPSCQueue.AvailableWrite());
  SPSCData data({SPSCData::DecodedData(std::move(mBatchedData))});
  PushDataToSPSCQueue(data);
  MOZ_ASSERT(mBatchedData.mSegment.IsEmpty());
  // No batched data remains, we can cancel the pending tasks.
  mDelayedScheduler.Reset();
}

void AudioDecoderInputTrack::NotifyEndOfStream() {
  AssertOnDecoderThread();
  // Force to push all data before EOS. Otherwise, the track would be ended too
  // early without sending all data.
  PushBatchedDataIfNeeded();
  SPSCData data({SPSCData::EOS()});
  LOG("Set EOS, available SPSC sz=%u", mSPSCQueue.AvailableWrite());
  PushDataToSPSCQueue(data);
}

void AudioDecoderInputTrack::ClearFutureData() {
  AssertOnDecoderThread();
  // Clear the data hasn't been pushed to SPSC queue yet.
  mBatchedData.Clear();
  mDelayedScheduler.Reset();
  SPSCData data({SPSCData::ClearFutureData()});
  LOG("Set clear future data, available SPSC sz=%u",
      mSPSCQueue.AvailableWrite());
  PushDataToSPSCQueue(data);
}

void AudioDecoderInputTrack::PushDataToSPSCQueue(SPSCData& data) {
  AssertOnDecoderThread();
  const bool rv = mSPSCQueue.Enqueue(data);
  MOZ_DIAGNOSTIC_ASSERT(rv, "Failed to push data, SPSC queue is full!");
  Unused << rv;
}

void AudioDecoderInputTrack::SetVolume(float aVolume) {
  AssertOnDecoderThread();
  LOG("Set volume=%f", aVolume);
  GetMainThreadSerialEventTarget()->Dispatch(
      NS_NewRunnableFunction("AudioDecoderInputTrack::SetVolume",
                             [self = RefPtr<AudioDecoderInputTrack>(this),
                              aVolume] { self->SetVolumeImpl(aVolume); }));
}

void AudioDecoderInputTrack::SetVolumeImpl(float aVolume) {
  MOZ_ASSERT(NS_IsMainThread());
  class Message : public ControlMessage {
   public:
    Message(AudioDecoderInputTrack* aTrack, float aVolume)
        : ControlMessage(aTrack), mTrack(aTrack), mVolume(aVolume) {}
    void Run() override {
      TRACE_COMMENT("AudioDecoderInputTrack::SetVolume ControlMessage", "%f",
                    mVolume);
      LOG_M("Apply volume=%f", mTrack.get(), mVolume);
      mTrack->mVolume = mVolume;
    }

   protected:
    const RefPtr<AudioDecoderInputTrack> mTrack;
    const float mVolume;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aVolume));
}

void AudioDecoderInputTrack::SetPlaybackRate(float aPlaybackRate) {
  AssertOnDecoderThread();
  LOG("Set playback rate=%f", aPlaybackRate);
  GetMainThreadSerialEventTarget()->Dispatch(NS_NewRunnableFunction(
      "AudioDecoderInputTrack::SetPlaybackRate",
      [self = RefPtr<AudioDecoderInputTrack>(this), aPlaybackRate] {
        self->SetPlaybackRateImpl(aPlaybackRate);
      }));
}

void AudioDecoderInputTrack::SetPlaybackRateImpl(float aPlaybackRate) {
  MOZ_ASSERT(NS_IsMainThread());
  class Message : public ControlMessage {
   public:
    Message(AudioDecoderInputTrack* aTrack, float aPlaybackRate)
        : ControlMessage(aTrack),
          mTrack(aTrack),
          mPlaybackRate(aPlaybackRate) {}
    void Run() override {
      TRACE_COMMENT("AudioDecoderInputTrack::SetPlaybackRate ControlMessage",
                    "%f", mPlaybackRate);
      LOG_M("Apply playback rate=%f", mTrack.get(), mPlaybackRate);
      mTrack->mPlaybackRate = mPlaybackRate;
      mTrack->SetTempoAndRateForTimeStretcher();
    }

   protected:
    const RefPtr<AudioDecoderInputTrack> mTrack;
    const float mPlaybackRate;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aPlaybackRate));
}

void AudioDecoderInputTrack::SetPreservesPitch(bool aPreservesPitch) {
  AssertOnDecoderThread();
  LOG("Set preserves pitch=%d", aPreservesPitch);
  GetMainThreadSerialEventTarget()->Dispatch(NS_NewRunnableFunction(
      "AudioDecoderInputTrack::SetPreservesPitch",
      [self = RefPtr<AudioDecoderInputTrack>(this), aPreservesPitch] {
        self->SetPreservesPitchImpl(aPreservesPitch);
      }));
}

void AudioDecoderInputTrack::SetPreservesPitchImpl(bool aPreservesPitch) {
  MOZ_ASSERT(NS_IsMainThread());
  class Message : public ControlMessage {
   public:
    Message(AudioDecoderInputTrack* aTrack, bool aPreservesPitch)
        : ControlMessage(aTrack),
          mTrack(aTrack),
          mPreservesPitch(aPreservesPitch) {}
    void Run() override {
      TRACE_COMMENT("AudioDecoderInputTrack::SetPreservesPitch", "%s",
                    mPreservesPitch ? "true" : "false")
      LOG_M("Apply preserves pitch=%d", mTrack.get(), mPreservesPitch);
      mTrack->mPreservesPitch = mPreservesPitch;
      mTrack->SetTempoAndRateForTimeStretcher();
    }

   protected:
    const RefPtr<AudioDecoderInputTrack> mTrack;
    const bool mPreservesPitch;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aPreservesPitch));
}

void AudioDecoderInputTrack::Close() {
  AssertOnDecoderThread();
  LOG("Close");
  mShutdownSPSCQueue = true;
  mBatchedData.Clear();
  mDelayedScheduler.Reset();
}

void AudioDecoderInputTrack::DestroyImpl() {
  LOG("DestroyImpl");
  AssertOnGraphThreadOrNotRunning();
  mBufferedData.Clear();
  if (mTimeStretcher) {
    soundtouch::destroySoundTouchObj(mTimeStretcher);
  }
  ProcessedMediaTrack::DestroyImpl();
}

AudioDecoderInputTrack::~AudioDecoderInputTrack() {
  MOZ_ASSERT(mBatchedData.mSegment.IsEmpty());
  MOZ_ASSERT(mShutdownSPSCQueue);
  mResampler.own(nullptr);
}

void AudioDecoderInputTrack::ProcessInput(GraphTime aFrom, GraphTime aTo,
                                          uint32_t aFlags) {
  AssertOnGraphThread();
  if (Ended()) {
    return;
  }

  TrackTime consumedDuration = 0;
  auto notify = MakeScopeExit([this, &consumedDuration] {
    NotifyInTheEndOfProcessInput(consumedDuration);
  });

  if (mSentAllData && (aFlags & ALLOW_END)) {
    LOG("End track");
    mEnded = true;
    return;
  }

  const TrackTime expectedDuration = aTo - aFrom;
  LOG("ProcessInput [%" PRId64 " to %" PRId64 "], duration=%" PRId64, aFrom,
      aTo, expectedDuration);

  // Drain all data from SPSC queue first, because we want that the SPSC queue
  // always has capacity of accepting data from the producer. In addition, we
  // also need to check if there is any control related data that should be
  // applied to output segment, eg. `ClearFutureData`.
  SPSCData data;
  while (mSPSCQueue.Dequeue(&data, 1) > 0) {
    HandleSPSCData(data);
  }

  consumedDuration += AppendBufferedDataToOutput(expectedDuration);
  if (HasSentAllData()) {
    LOG("Sent all data, should end track in next iteration");
    mSentAllData = true;
  }
}

void AudioDecoderInputTrack::HandleSPSCData(SPSCData& aData) {
  AssertOnGraphThread();
  if (aData.IsDecodedData()) {
    MOZ_ASSERT(!mReceivedEOS);
    AudioSegment& segment = aData.AsDecodedData()->mSegment;
    LOG("popped out data [%" PRId64 ":%" PRId64 "] sz=%" PRId64,
        aData.AsDecodedData()->mStartTime.ToMicroseconds(),
        aData.AsDecodedData()->mEndTime.ToMicroseconds(),
        segment.GetDuration());
    mBufferedData.AppendFrom(&segment);
    return;
  }
  if (aData.IsEOS()) {
    MOZ_ASSERT(!Ended());
    LOG("Received EOS");
    mReceivedEOS = true;
    return;
  }
  if (aData.IsClearFutureData()) {
    LOG("Clear future data");
    mBufferedData.Clear();
    if (!Ended()) {
      LOG("Clear EOS");
      mReceivedEOS = false;
    }
    return;
  }
  MOZ_ASSERT_UNREACHABLE("unsupported SPSC data");
}

TrackTime AudioDecoderInputTrack::AppendBufferedDataToOutput(
    TrackTime aExpectedDuration) {
  AssertOnGraphThread();

  // Remove the necessary part from `mBufferedData` to create a new
  // segment in order to apply some operation without affecting all data.
  AudioSegment outputSegment;
  TrackTime consumedDuration = 0;
  if (mPlaybackRate != 1.0) {
    consumedDuration =
        AppendTimeStretchedDataToSegment(aExpectedDuration, outputSegment);
  } else {
    consumedDuration =
        AppendUnstretchedDataToSegment(aExpectedDuration, outputSegment);
  }

  // Apply any necessary change on the segement which would be outputed to the
  // graph.
  const TrackTime appendedDuration = outputSegment.GetDuration();
  outputSegment.ApplyVolume(mVolume);
  ApplyTrackDisabling(&outputSegment);
  mSegment->AppendFrom(&outputSegment);

  LOG("Appended %" PRId64 ", consumed %" PRId64
      ", remaining raw buffered %" PRId64 ", remaining time-stretched %u",
      appendedDuration, consumedDuration, mBufferedData.GetDuration(),
      mTimeStretcher ? mTimeStretcher->numSamples() : 0);
  if (auto gap = aExpectedDuration - appendedDuration; gap > 0) {
    LOG("Audio underrun, fill silence %" PRId64, gap);
    MOZ_ASSERT(mBufferedData.IsEmpty());
    mSegment->AppendNullData(gap);
  }
  return consumedDuration;
}

TrackTime AudioDecoderInputTrack::AppendTimeStretchedDataToSegment(
    TrackTime aExpectedDuration, AudioSegment& aOutput) {
  AssertOnGraphThread();
  EnsureTimeStretcher();

  MOZ_ASSERT(mPlaybackRate != 1.0f);
  MOZ_ASSERT(aExpectedDuration >= 0);
  MOZ_ASSERT(mTimeStretcher);
  MOZ_ASSERT(aOutput.IsEmpty());

  // If we don't have enough data that have been time-stretched, fill raw data
  // into the time stretcher until the amount of samples that time stretcher
  // finishes processed reaches or exceeds the expected duration.
  TrackTime consumedDuration = 0;
  if (mTimeStretcher->numSamples() < aExpectedDuration) {
    consumedDuration = FillDataToTimeStretcher(aExpectedDuration);
  }
  MOZ_ASSERT(consumedDuration >= 0);
  Unused << GetDataFromTimeStretcher(aExpectedDuration, aOutput);
  return consumedDuration;
}

TrackTime AudioDecoderInputTrack::FillDataToTimeStretcher(
    TrackTime aExpectedDuration) {
  AssertOnGraphThread();
  MOZ_ASSERT(mPlaybackRate != 1.0f);
  MOZ_ASSERT(aExpectedDuration >= 0);
  MOZ_ASSERT(mTimeStretcher);

  TrackTime consumedDuration = 0;
  const uint32_t channels = GetChannelCountForTimeStretcher();
  mBufferedData.IterateOnChunks([&](AudioChunk* aChunk) {
    MOZ_ASSERT(aChunk);
    if (aChunk->IsNull() && aChunk->GetDuration() == 0) {
      // Skip this chunk and wait for next one.
      return false;
    }
    const uint32_t bufferLength = channels * aChunk->GetDuration();
    if (bufferLength > mInterleavedBuffer.Capacity()) {
      mInterleavedBuffer.SetCapacity(bufferLength);
    }
    mInterleavedBuffer.SetLengthAndRetainStorage(bufferLength);
    if (aChunk->IsNull()) {
      MOZ_ASSERT(aChunk->GetDuration(), "chunk with only silence");
      memset(mInterleavedBuffer.Elements(), 0, mInterleavedBuffer.Length());
    } else {
      // Do the up-mix/down-mix first if necessary that forces to change the
      // data's channel count to the time stretcher's channel count. Then
      // perform a transformation from planar to interleaved.
      switch (aChunk->mBufferFormat) {
        case AUDIO_FORMAT_S16:
          WriteChunk<int16_t>(*aChunk, channels, 1.0f,
                              mInterleavedBuffer.Elements());
          break;
        case AUDIO_FORMAT_FLOAT32:
          WriteChunk<float>(*aChunk, channels, 1.0f,
                            mInterleavedBuffer.Elements());
          break;
        default:
          MOZ_ASSERT_UNREACHABLE("Not expected format");
      }
    }
    mTimeStretcher->putSamples(mInterleavedBuffer.Elements(),
                               aChunk->GetDuration());
    consumedDuration += aChunk->GetDuration();
    return mTimeStretcher->numSamples() >= aExpectedDuration;
  });
  mBufferedData.RemoveLeading(consumedDuration);
  return consumedDuration;
}

TrackTime AudioDecoderInputTrack::AppendUnstretchedDataToSegment(
    TrackTime aExpectedDuration, AudioSegment& aOutput) {
  AssertOnGraphThread();
  MOZ_ASSERT(mPlaybackRate == 1.0f);
  MOZ_ASSERT(aExpectedDuration >= 0);
  MOZ_ASSERT(aOutput.IsEmpty());

  const TrackTime drained =
      DrainStretchedDataIfNeeded(aExpectedDuration, aOutput);
  const TrackTime available =
      std::min(aExpectedDuration - drained, mBufferedData.GetDuration());
  aOutput.AppendSlice(mBufferedData, 0, available);
  MOZ_ASSERT(aOutput.GetDuration() <= aExpectedDuration);
  mBufferedData.RemoveLeading(available);
  return available;
}

TrackTime AudioDecoderInputTrack::DrainStretchedDataIfNeeded(
    TrackTime aExpectedDuration, AudioSegment& aOutput) {
  AssertOnGraphThread();
  MOZ_ASSERT(mPlaybackRate == 1.0f);
  MOZ_ASSERT(aExpectedDuration >= 0);

  if (!mTimeStretcher) {
    return 0;
  }
  if (mTimeStretcher->numSamples() == 0) {
    return 0;
  }
  return GetDataFromTimeStretcher(aExpectedDuration, aOutput);
}

TrackTime AudioDecoderInputTrack::GetDataFromTimeStretcher(
    TrackTime aExpectedDuration, AudioSegment& aOutput) {
  AssertOnGraphThread();
  MOZ_ASSERT(mTimeStretcher);
  MOZ_ASSERT(aExpectedDuration >= 0);

  if (HasSentAllData() && mTimeStretcher->numUnprocessedSamples()) {
    mTimeStretcher->flush();
    LOG("Flush %u frames from the time stretcher",
        mTimeStretcher->numSamples());
  }

  const TrackTime available =
      std::min((TrackTime)mTimeStretcher->numSamples(), aExpectedDuration);
  if (available == 0) {
    // Either running out of stretched data, or the raw data we filled into
    // the time stretcher were not enough for producing stretched data.
    return 0;
  }

  // Retrieve interleaved data from the time stretcher.
  const uint32_t channelCount = GetChannelCountForTimeStretcher();
  const uint32_t bufferLength = channelCount * available;
  if (bufferLength > mInterleavedBuffer.Capacity()) {
    mInterleavedBuffer.SetCapacity(bufferLength);
  }
  mInterleavedBuffer.SetLengthAndRetainStorage(bufferLength);
  mTimeStretcher->receiveSamples(mInterleavedBuffer.Elements(), available);

  // Perform a transformation from interleaved to planar.
  CheckedInt<size_t> bufferSize(sizeof(AudioDataValue));
  bufferSize *= bufferLength;
  RefPtr<SharedBuffer> buffer = SharedBuffer::Create(bufferSize);
  AudioDataValue* bufferData = static_cast<AudioDataValue*>(buffer->Data());
  AutoTArray<AudioDataValue*, 2> planarBuffer;
  planarBuffer.SetLength(channelCount);
  for (size_t idx = 0; idx < channelCount; idx++) {
    planarBuffer[idx] = bufferData + idx * available;
  }
  DeinterleaveAndConvertBuffer(mInterleavedBuffer.Elements(), available,
                               channelCount, planarBuffer.Elements());
  AutoTArray<const AudioDataValue*, 2> outputChannels;
  outputChannels.AppendElements(planarBuffer);
  aOutput.AppendFrames(buffer.forget(), outputChannels,
                       static_cast<int32_t>(available),
                       mBufferedData.GetOldestPrinciple());
  return available;
}

void AudioDecoderInputTrack::NotifyInTheEndOfProcessInput(
    TrackTime aFillDuration) {
  AssertOnGraphThread();
  mWrittenFrames += aFillDuration;
  LOG("Notify, fill=%" PRId64 ", total written=%" PRId64 ", ended=%d",
      aFillDuration, mWrittenFrames, Ended());
  if (aFillDuration > 0) {
    mOnOutput.Notify(mWrittenFrames);
  }
  if (Ended()) {
    mOnEnd.Notify();
  }
}

bool AudioDecoderInputTrack::HasSentAllData() const {
  AssertOnGraphThread();
  return mReceivedEOS && mSPSCQueue.AvailableRead() == 0 &&
         mBufferedData.IsEmpty();
}

uint32_t AudioDecoderInputTrack::NumberOfChannels() const {
  AssertOnGraphThread();
  const uint32_t maxChannelCount = GetData<AudioSegment>()->MaxChannelCount();
  return maxChannelCount ? maxChannelCount : mInitialInputChannels;
}

void AudioDecoderInputTrack::EnsureTimeStretcher() {
  AssertOnGraphThread();
  if (!mTimeStretcher) {
    mTimeStretcher = soundtouch::createSoundTouchObj();
    mTimeStretcher->setSampleRate(GraphImpl()->GraphRate());
    mTimeStretcher->setChannels(GetChannelCountForTimeStretcher());
    mTimeStretcher->setPitch(1.0);

    // SoundTouch v2.1.2 uses automatic time-stretch settings with the following
    // values:
    // Tempo 0.5: 90ms sequence, 20ms seekwindow, 8ms overlap
    // Tempo 2.0: 40ms sequence, 15ms seekwindow, 8ms overlap
    // We are going to use a smaller 10ms sequence size to improve speech
    // clarity, giving more resolution at high tempo and less reverb at low
    // tempo. Maintain 15ms seekwindow and 8ms overlap for smoothness.
    mTimeStretcher->setSetting(
        SETTING_SEQUENCE_MS,
        StaticPrefs::media_audio_playbackrate_soundtouch_sequence_ms());
    mTimeStretcher->setSetting(
        SETTING_SEEKWINDOW_MS,
        StaticPrefs::media_audio_playbackrate_soundtouch_seekwindow_ms());
    mTimeStretcher->setSetting(
        SETTING_OVERLAP_MS,
        StaticPrefs::media_audio_playbackrate_soundtouch_overlap_ms());
    SetTempoAndRateForTimeStretcher();
    LOG("Create TimeStretcher (channel=%d, playbackRate=%f, preservePitch=%d)",
        GetChannelCountForTimeStretcher(), mPlaybackRate, mPreservesPitch);
  }
}

void AudioDecoderInputTrack::SetTempoAndRateForTimeStretcher() {
  AssertOnGraphThread();
  if (!mTimeStretcher) {
    return;
  }
  if (mPreservesPitch) {
    mTimeStretcher->setTempo(mPlaybackRate);
    mTimeStretcher->setRate(1.0f);
  } else {
    mTimeStretcher->setTempo(1.0f);
    mTimeStretcher->setRate(mPlaybackRate);
  }
}

uint32_t AudioDecoderInputTrack::GetChannelCountForTimeStretcher() const {
  // The time stretcher MUST be initialized with a fixed channel count, but the
  // channel count in audio chunks might vary. Therefore, we always use the
  // initial input channel count to initialize the time stretcher and perform a
  // real-time down-mix/up-mix for audio chunks which have different channel
  // count than the initial input channel count.
  return mInitialInputChannels;
}

#undef LOG
}  // namespace mozilla
