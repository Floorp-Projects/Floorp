/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioDecoderInputTrack.h"

#include "MediaData.h"
#include "mozilla/ScopeExit.h"

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
  MOZ_ASSERT(!mShutdownSPSCQueue);

  // If SPSC queue doesn't have much available capacity now, we batch new data
  // together to be pushed as a single unit when the SPSC queue has more space.
  if (ShouldBatchData()) {
    BatchData(aAudio, aPrincipalHandle);
    return;
  }

  // Append new data into batched data and push them together.
  if (HasBatchedData()) {
    BatchData(aAudio, aPrincipalHandle);
    PushBatchedDataIfNeeded();
    return;
  }

  SPSCData data({SPSCData::DecodedData(aAudio->mTime, aAudio->GetEndTime())});
  if (ConvertAudioDataToSegment(aAudio, data.AsDecodedData()->mSegment,
                                aPrincipalHandle)) {
    LOG("Append data [%" PRId64 ":%" PRId64 "], available SPSC sz=%u",
        aAudio->mTime.ToMicroseconds(), aAudio->GetEndTime().ToMicroseconds(),
        mSPSCQueue.AvailableWrite());
    PushDataToSPSCQueue(data);
  }
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
      LOG_M("Apply playback rate=%f", mTrack.get(), mPlaybackRate);
      mTrack->mPlaybackRate = mPlaybackRate;
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
      LOG_M("Apply preserves pitch=%d", mTrack.get(), mPreservesPitch);
      mTrack->mPreservesPitch = mPreservesPitch;
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

  TrackTime filledDuration = 0;
  auto notify = MakeScopeExit([this, &filledDuration] {
    NotifyInTheEndOfProcessInput(filledDuration);
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

  filledDuration += AppendBufferedDataToOutput(expectedDuration);
  if (TrackTime gap = expectedDuration - filledDuration; gap > 0) {
    LOG("Audio underrun, fill silence %" PRId64, gap);
    MOZ_ASSERT(mBufferedData.IsEmpty());
    mSegment->AppendNullData(gap);
  }
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
    mReceivedEOS = true;
    return;
  }
  if (aData.IsClearFutureData()) {
    mBufferedData.Clear();
    if (!Ended()) {
      mReceivedEOS = false;
    }
    return;
  }
  MOZ_ASSERT_UNREACHABLE("unsupported SPSC data");
}

TrackTime AudioDecoderInputTrack::AppendBufferedDataToOutput(
    TrackTime aExpectedDuration) {
  AssertOnGraphThread();

  const TrackTime available =
      std::min(aExpectedDuration, mBufferedData.GetDuration());

  // Remove the necessary part from `mBufferedData` to create a new
  // segment in order to apply some operation without affecting all data.
  AudioSegment outputSegment;
  outputSegment.AppendSlice(mBufferedData, 0, available);
  MOZ_ASSERT(outputSegment.GetDuration() <= aExpectedDuration);
  mBufferedData.RemoveLeading(available);

  // Apply any necessary change on the segement which would be outputed to the
  // graph.
  outputSegment.ApplyVolume(mVolume);
  ApplyTrackDisabling(&outputSegment);
  mSegment->AppendFrom(&outputSegment);

  LOG("Appended %" PRId64 ", and remaining %" PRId64, available,
      mBufferedData.GetDuration());
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

#undef LOG
}  // namespace mozilla
