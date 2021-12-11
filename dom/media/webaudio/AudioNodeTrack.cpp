/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioNodeTrack.h"

#include "MediaTrackGraphImpl.h"
#include "MediaTrackListener.h"
#include "AudioNodeEngine.h"
#include "ThreeDPoint.h"
#include "Tracing.h"
#include "AudioChannelFormat.h"
#include "AudioParamTimeline.h"
#include "AudioContext.h"
#include "nsMathUtils.h"
#include "AlignmentUtils.h"
#include "blink/Reverb.h"

using namespace mozilla::dom;

namespace mozilla {

/**
 * An AudioNodeTrack produces a single audio track with ID
 * AUDIO_TRACK. This track has rate AudioContext::sIdealAudioRate
 * for regular audio contexts, and the rate requested by the web content
 * for offline audio contexts.
 * Each chunk in the track is a single block of WEBAUDIO_BLOCK_SIZE samples.
 * Note: This must be a different value than MEDIA_STREAM_DEST_TRACK_ID
 */

AudioNodeTrack::AudioNodeTrack(AudioNodeEngine* aEngine, Flags aFlags,
                               TrackRate aSampleRate)
    : ProcessedMediaTrack(
          aSampleRate, MediaSegment::AUDIO,
          (aFlags & EXTERNAL_OUTPUT) ? new AudioSegment() : nullptr),
      mEngine(aEngine),
      mFlags(aFlags),
      mNumberOfInputChannels(2),
      mIsActive(aEngine->IsActive()),
      mMarkAsEndedAfterThisBlock(false),
      mAudioParamTrack(false),
      mPassThrough(false) {
  MOZ_ASSERT(NS_IsMainThread());
  mSuspendedCount = !(mIsActive || mFlags & EXTERNAL_OUTPUT);
  mChannelCountMode = ChannelCountMode::Max;
  mChannelInterpretation = ChannelInterpretation::Speakers;
  mLastChunks.SetLength(std::max(uint16_t(1), mEngine->OutputCount()));
  MOZ_COUNT_CTOR(AudioNodeTrack);
}

AudioNodeTrack::~AudioNodeTrack() {
  MOZ_ASSERT(mActiveInputCount == 0);
  MOZ_COUNT_DTOR(AudioNodeTrack);
}

void AudioNodeTrack::OnGraphThreadDone() { mEngine->OnGraphThreadDone(); }

void AudioNodeTrack::DestroyImpl() {
  // These are graph thread objects, so clean up on graph thread.
  mInputChunks.Clear();
  mLastChunks.Clear();

  ProcessedMediaTrack::DestroyImpl();
}

/* static */
already_AddRefed<AudioNodeTrack> AudioNodeTrack::Create(
    AudioContext* aCtx, AudioNodeEngine* aEngine, Flags aFlags,
    MediaTrackGraph* aGraph) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(aGraph);

  // MediaRecorders use an AudioNodeTrack, but no AudioNode
  AudioNode* node = aEngine->NodeMainThread();

  RefPtr<AudioNodeTrack> track =
      new AudioNodeTrack(aEngine, aFlags, aGraph->GraphRate());
  if (node) {
    track->SetChannelMixingParametersImpl(node->ChannelCount(),
                                          node->ChannelCountModeValue(),
                                          node->ChannelInterpretationValue());
  }
  // All realtime tracks are initially suspended.
  // ApplyAudioContextOperation() is used to start tracks so that a new track
  // will not be started before the existing tracks, which may be awaiting an
  // AudioCallbackDriver to resume.
  bool isRealtime = !aCtx->IsOffline();
  track->mSuspendedCount += isRealtime;
  aGraph->AddTrack(track);
  if (isRealtime && !aCtx->ShouldSuspendNewTrack()) {
    nsTArray<RefPtr<mozilla::MediaTrack>> tracks;
    tracks.AppendElement(track);
    aGraph->ApplyAudioContextOperation(aCtx->DestinationTrack(),
                                       std::move(tracks),
                                       AudioContextOperation::Resume);
  }
  return track.forget();
}

size_t AudioNodeTrack::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t amount = 0;

  // Not reported:
  // - mEngine

  amount += ProcessedMediaTrack::SizeOfExcludingThis(aMallocSizeOf);
  amount += mLastChunks.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (size_t i = 0; i < mLastChunks.Length(); i++) {
    // NB: This is currently unshared only as there are instances of
    //     double reporting in DMD otherwise.
    amount += mLastChunks[i].SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  return amount;
}

size_t AudioNodeTrack::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

void AudioNodeTrack::SizeOfAudioNodesIncludingThis(
    MallocSizeOf aMallocSizeOf, AudioNodeSizes& aUsage) const {
  // Explicitly separate out the track memory.
  aUsage.mTrack = SizeOfIncludingThis(aMallocSizeOf);

  if (mEngine) {
    // This will fill out the rest of |aUsage|.
    mEngine->SizeOfIncludingThis(aMallocSizeOf, aUsage);
  }
}

void AudioNodeTrack::SetTrackTimeParameter(uint32_t aIndex,
                                           AudioContext* aContext,
                                           double aTrackTime) {
  class Message final : public ControlMessage {
   public:
    Message(AudioNodeTrack* aTrack, uint32_t aIndex,
            MediaTrack* aRelativeToTrack, double aTrackTime)
        : ControlMessage(aTrack),
          mTrackTime(aTrackTime),
          mRelativeToTrack(aRelativeToTrack),
          mIndex(aIndex) {}
    void Run() override {
      TRACE("AudioNodeTrack::SetTrackTimeParameterImpl");
      static_cast<AudioNodeTrack*>(mTrack)->SetTrackTimeParameterImpl(
          mIndex, mRelativeToTrack, mTrackTime);
    }
    double mTrackTime;
    MediaTrack* MOZ_UNSAFE_REF(
        "ControlMessages are processed in order.  This \
destination track is not yet destroyed.  Its (future) destroy message will be \
processed after this message.") mRelativeToTrack;
    uint32_t mIndex;
  };

  GraphImpl()->AppendMessage(MakeUnique<Message>(
      this, aIndex, aContext->DestinationTrack(), aTrackTime));
}

void AudioNodeTrack::SetTrackTimeParameterImpl(uint32_t aIndex,
                                               MediaTrack* aRelativeToTrack,
                                               double aTrackTime) {
  TrackTime ticks = aRelativeToTrack->SecondsToNearestTrackTime(aTrackTime);
  mEngine->SetTrackTimeParameter(aIndex, ticks);
}

void AudioNodeTrack::SetDoubleParameter(uint32_t aIndex, double aValue) {
  class Message final : public ControlMessage {
   public:
    Message(AudioNodeTrack* aTrack, uint32_t aIndex, double aValue)
        : ControlMessage(aTrack), mValue(aValue), mIndex(aIndex) {}
    void Run() override {
      TRACE("AudioNodeTrack::SetDoubleParameter");
      static_cast<AudioNodeTrack*>(mTrack)->Engine()->SetDoubleParameter(
          mIndex, mValue);
    }
    double mValue;
    uint32_t mIndex;
  };

  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aIndex, aValue));
}

void AudioNodeTrack::SetInt32Parameter(uint32_t aIndex, int32_t aValue) {
  class Message final : public ControlMessage {
   public:
    Message(AudioNodeTrack* aTrack, uint32_t aIndex, int32_t aValue)
        : ControlMessage(aTrack), mValue(aValue), mIndex(aIndex) {}
    void Run() override {
      TRACE("AudioNodeTrack::SetInt32Parameter");
      static_cast<AudioNodeTrack*>(mTrack)->Engine()->SetInt32Parameter(mIndex,
                                                                        mValue);
    }
    int32_t mValue;
    uint32_t mIndex;
  };

  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aIndex, aValue));
}

void AudioNodeTrack::SendTimelineEvent(uint32_t aIndex,
                                       const AudioTimelineEvent& aEvent) {
  class Message final : public ControlMessage {
   public:
    Message(AudioNodeTrack* aTrack, uint32_t aIndex,
            const AudioTimelineEvent& aEvent)
        : ControlMessage(aTrack),
          mEvent(aEvent),
          mSampleRate(aTrack->mSampleRate),
          mIndex(aIndex) {}
    void Run() override {
      TRACE("AudioNodeTrack::RecvTimelineEvent");
      static_cast<AudioNodeTrack*>(mTrack)->Engine()->RecvTimelineEvent(mIndex,
                                                                        mEvent);
    }
    AudioTimelineEvent mEvent;
    TrackRate mSampleRate;
    uint32_t mIndex;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aIndex, aEvent));
}

void AudioNodeTrack::SetBuffer(AudioChunk&& aBuffer) {
  class Message final : public ControlMessage {
   public:
    Message(AudioNodeTrack* aTrack, AudioChunk&& aBuffer)
        : ControlMessage(aTrack), mBuffer(aBuffer) {}
    void Run() override {
      TRACE("AudioNodeTrack::SetBuffer");
      static_cast<AudioNodeTrack*>(mTrack)->Engine()->SetBuffer(
          std::move(mBuffer));
    }
    AudioChunk mBuffer;
  };

  GraphImpl()->AppendMessage(MakeUnique<Message>(this, std::move(aBuffer)));
}

void AudioNodeTrack::SetReverb(WebCore::Reverb* aReverb,
                               uint32_t aImpulseChannelCount) {
  class Message final : public ControlMessage {
   public:
    Message(AudioNodeTrack* aTrack, WebCore::Reverb* aReverb,
            uint32_t aImpulseChannelCount)
        : ControlMessage(aTrack),
          mReverb(aReverb),
          mImpulseChanelCount(aImpulseChannelCount) {}
    void Run() override {
      TRACE("AudioNodeTrack::SetReverb");
      static_cast<AudioNodeTrack*>(mTrack)->Engine()->SetReverb(
          mReverb.release(), mImpulseChanelCount);
    }
    UniquePtr<WebCore::Reverb> mReverb;
    uint32_t mImpulseChanelCount;
  };

  GraphImpl()->AppendMessage(
      MakeUnique<Message>(this, aReverb, aImpulseChannelCount));
}

void AudioNodeTrack::SetRawArrayData(nsTArray<float>&& aData) {
  class Message final : public ControlMessage {
   public:
    Message(AudioNodeTrack* aTrack, nsTArray<float>&& aData)
        : ControlMessage(aTrack), mData(std::move(aData)) {}
    void Run() override {
      TRACE("AudioNodeTrack::SetRawArrayData");
      static_cast<AudioNodeTrack*>(mTrack)->Engine()->SetRawArrayData(
          std::move(mData));
    }
    nsTArray<float> mData;
  };

  GraphImpl()->AppendMessage(MakeUnique<Message>(this, std::move(aData)));
}

void AudioNodeTrack::SetChannelMixingParameters(
    uint32_t aNumberOfChannels, ChannelCountMode aChannelCountMode,
    ChannelInterpretation aChannelInterpretation) {
  class Message final : public ControlMessage {
   public:
    Message(AudioNodeTrack* aTrack, uint32_t aNumberOfChannels,
            ChannelCountMode aChannelCountMode,
            ChannelInterpretation aChannelInterpretation)
        : ControlMessage(aTrack),
          mNumberOfChannels(aNumberOfChannels),
          mChannelCountMode(aChannelCountMode),
          mChannelInterpretation(aChannelInterpretation) {}
    void Run() override {
      TRACE("AudioNodeTrack::SetChannelMixingParameters");
      static_cast<AudioNodeTrack*>(mTrack)->SetChannelMixingParametersImpl(
          mNumberOfChannels, mChannelCountMode, mChannelInterpretation);
    }
    uint32_t mNumberOfChannels;
    ChannelCountMode mChannelCountMode;
    ChannelInterpretation mChannelInterpretation;
  };

  GraphImpl()->AppendMessage(MakeUnique<Message>(
      this, aNumberOfChannels, aChannelCountMode, aChannelInterpretation));
}

void AudioNodeTrack::SetPassThrough(bool aPassThrough) {
  class Message final : public ControlMessage {
   public:
    Message(AudioNodeTrack* aTrack, bool aPassThrough)
        : ControlMessage(aTrack), mPassThrough(aPassThrough) {}
    void Run() override {
      TRACE("AudioNodeTrack::SetPassThrough");
      static_cast<AudioNodeTrack*>(mTrack)->mPassThrough = mPassThrough;
    }
    bool mPassThrough;
  };

  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aPassThrough));
}

void AudioNodeTrack::SendRunnable(already_AddRefed<nsIRunnable> aRunnable) {
  class Message final : public ControlMessage {
   public:
    Message(MediaTrack* aTrack, already_AddRefed<nsIRunnable> aRunnable)
        : ControlMessage(aTrack), mRunnable(aRunnable) {}
    void Run() override {
      TRACE("AudioNodeTrack::SendRunnable");
      mRunnable->Run();
    }

   private:
    nsCOMPtr<nsIRunnable> mRunnable;
  };

  GraphImpl()->AppendMessage(MakeUnique<Message>(this, std::move(aRunnable)));
}

void AudioNodeTrack::SetChannelMixingParametersImpl(
    uint32_t aNumberOfChannels, ChannelCountMode aChannelCountMode,
    ChannelInterpretation aChannelInterpretation) {
  mNumberOfInputChannels = aNumberOfChannels;
  mChannelCountMode = aChannelCountMode;
  mChannelInterpretation = aChannelInterpretation;
}

uint32_t AudioNodeTrack::ComputedNumberOfChannels(uint32_t aInputChannelCount) {
  switch (mChannelCountMode) {
    case ChannelCountMode::Explicit:
      // Disregard the channel count we've calculated from inputs, and just use
      // mNumberOfInputChannels.
      return mNumberOfInputChannels;
    case ChannelCountMode::Clamped_max:
      // Clamp the computed output channel count to mNumberOfInputChannels.
      return std::min(aInputChannelCount, mNumberOfInputChannels);
    default:
    case ChannelCountMode::Max:
      // Nothing to do here, just shut up the compiler warning.
      return aInputChannelCount;
  }
}

uint32_t AudioNodeTrack::NumberOfChannels() const {
  MOZ_ASSERT(GraphImpl()->OnGraphThread());

  return mNumberOfInputChannels;
}

class AudioNodeTrack::AdvanceAndResumeMessage final : public ControlMessage {
 public:
  AdvanceAndResumeMessage(AudioNodeTrack* aTrack, TrackTime aAdvance)
      : ControlMessage(aTrack), mAdvance(aAdvance) {}
  void Run() override {
    TRACE("AudioNodeTrack::AdvanceAndResumeMessage");
    auto ns = static_cast<AudioNodeTrack*>(mTrack);
    ns->mStartTime -= mAdvance;
    ns->mSegment->AppendNullData(mAdvance);
    ns->DecrementSuspendCount();
  }

 private:
  TrackTime mAdvance;
};

void AudioNodeTrack::AdvanceAndResume(TrackTime aAdvance) {
  mMainThreadCurrentTime += aAdvance;
  GraphImpl()->AppendMessage(
      MakeUnique<AdvanceAndResumeMessage>(this, aAdvance));
}

void AudioNodeTrack::ObtainInputBlock(AudioBlock& aTmpChunk,
                                      uint32_t aPortIndex) {
  uint32_t inputCount = mInputs.Length();
  uint32_t outputChannelCount = 1;
  AutoTArray<const AudioBlock*, 250> inputChunks;
  for (uint32_t i = 0; i < inputCount; ++i) {
    if (aPortIndex != mInputs[i]->InputNumber()) {
      // This input is connected to a different port
      continue;
    }
    MediaTrack* t = mInputs[i]->GetSource();
    AudioNodeTrack* a = static_cast<AudioNodeTrack*>(t);
    MOZ_ASSERT(a == t->AsAudioNodeTrack());
    if (a->IsAudioParamTrack()) {
      continue;
    }

    const AudioBlock* chunk = &a->mLastChunks[mInputs[i]->OutputNumber()];
    MOZ_ASSERT(chunk);
    if (chunk->IsNull() || chunk->mChannelData.IsEmpty()) {
      continue;
    }

    inputChunks.AppendElement(chunk);
    outputChannelCount =
        GetAudioChannelsSuperset(outputChannelCount, chunk->ChannelCount());
  }

  outputChannelCount = ComputedNumberOfChannels(outputChannelCount);

  uint32_t inputChunkCount = inputChunks.Length();
  if (inputChunkCount == 0 ||
      (inputChunkCount == 1 && inputChunks[0]->ChannelCount() == 0)) {
    aTmpChunk.SetNull(WEBAUDIO_BLOCK_SIZE);
    return;
  }

  if (inputChunkCount == 1 &&
      inputChunks[0]->ChannelCount() == outputChannelCount) {
    aTmpChunk = *inputChunks[0];
    return;
  }

  if (outputChannelCount == 0) {
    aTmpChunk.SetNull(WEBAUDIO_BLOCK_SIZE);
    return;
  }

  aTmpChunk.AllocateChannels(outputChannelCount);
  DownmixBufferType downmixBuffer;
  ASSERT_ALIGNED16(downmixBuffer.Elements());

  for (uint32_t i = 0; i < inputChunkCount; ++i) {
    AccumulateInputChunk(i, *inputChunks[i], &aTmpChunk, &downmixBuffer);
  }
}

void AudioNodeTrack::AccumulateInputChunk(uint32_t aInputIndex,
                                          const AudioBlock& aChunk,
                                          AudioBlock* aBlock,
                                          DownmixBufferType* aDownmixBuffer) {
  AutoTArray<const float*, GUESS_AUDIO_CHANNELS> channels;
  UpMixDownMixChunk(&aChunk, aBlock->ChannelCount(), channels, *aDownmixBuffer);

  for (uint32_t c = 0; c < channels.Length(); ++c) {
    const float* inputData = static_cast<const float*>(channels[c]);
    float* outputData = aBlock->ChannelFloatsForWrite(c);
    if (inputData) {
      if (aInputIndex == 0) {
        AudioBlockCopyChannelWithScale(inputData, aChunk.mVolume, outputData);
      } else {
        AudioBlockAddChannelWithScale(inputData, aChunk.mVolume, outputData);
      }
    } else {
      if (aInputIndex == 0) {
        PodZero(outputData, WEBAUDIO_BLOCK_SIZE);
      }
    }
  }
}

void AudioNodeTrack::UpMixDownMixChunk(const AudioBlock* aChunk,
                                       uint32_t aOutputChannelCount,
                                       nsTArray<const float*>& aOutputChannels,
                                       DownmixBufferType& aDownmixBuffer) {
  for (uint32_t i = 0; i < aChunk->ChannelCount(); i++) {
    aOutputChannels.AppendElement(
        static_cast<const float*>(aChunk->mChannelData[i]));
  }
  if (aOutputChannels.Length() < aOutputChannelCount) {
    if (mChannelInterpretation == ChannelInterpretation::Speakers) {
      AudioChannelsUpMix<float>(&aOutputChannels, aOutputChannelCount, nullptr);
      NS_ASSERTION(aOutputChannelCount == aOutputChannels.Length(),
                   "We called GetAudioChannelsSuperset to avoid this");
    } else {
      // Fill up the remaining aOutputChannels by zeros
      for (uint32_t j = aOutputChannels.Length(); j < aOutputChannelCount;
           ++j) {
        aOutputChannels.AppendElement(nullptr);
      }
    }
  } else if (aOutputChannels.Length() > aOutputChannelCount) {
    if (mChannelInterpretation == ChannelInterpretation::Speakers) {
      AutoTArray<float*, GUESS_AUDIO_CHANNELS> outputChannels;
      outputChannels.SetLength(aOutputChannelCount);
      aDownmixBuffer.SetLength(aOutputChannelCount * WEBAUDIO_BLOCK_SIZE);
      for (uint32_t j = 0; j < aOutputChannelCount; ++j) {
        outputChannels[j] = &aDownmixBuffer[j * WEBAUDIO_BLOCK_SIZE];
      }

      AudioChannelsDownMix(aOutputChannels, outputChannels.Elements(),
                           aOutputChannelCount, WEBAUDIO_BLOCK_SIZE);

      aOutputChannels.SetLength(aOutputChannelCount);
      for (uint32_t j = 0; j < aOutputChannels.Length(); ++j) {
        aOutputChannels[j] = outputChannels[j];
      }
    } else {
      // Drop the remaining aOutputChannels
      aOutputChannels.RemoveLastElements(aOutputChannels.Length() -
                                         aOutputChannelCount);
    }
  }
}

// The MediaTrackGraph guarantees that this is actually one block, for
// AudioNodeTracks.
void AudioNodeTrack::ProcessInput(GraphTime aFrom, GraphTime aTo,
                                  uint32_t aFlags) {
  uint16_t outputCount = mLastChunks.Length();
  MOZ_ASSERT(outputCount == std::max(uint16_t(1), mEngine->OutputCount()));

  if (!mIsActive) {
    // mLastChunks are already null.
#ifdef DEBUG
    for (const auto& chunk : mLastChunks) {
      MOZ_ASSERT(chunk.IsNull());
    }
#endif
  } else if (InMutedCycle()) {
    mInputChunks.Clear();
    for (uint16_t i = 0; i < outputCount; ++i) {
      mLastChunks[i].SetNull(WEBAUDIO_BLOCK_SIZE);
    }
  } else {
    // We need to generate at least one input
    uint16_t maxInputs = std::max(uint16_t(1), mEngine->InputCount());
    mInputChunks.SetLength(maxInputs);
    for (uint16_t i = 0; i < maxInputs; ++i) {
      ObtainInputBlock(mInputChunks[i], i);
    }
    bool finished = false;
    if (mPassThrough) {
      MOZ_ASSERT(outputCount == 1,
                 "For now, we only support nodes that have one output port");
      mLastChunks[0] = mInputChunks[0];
    } else {
      if (maxInputs <= 1 && outputCount <= 1) {
        mEngine->ProcessBlock(this, aFrom, mInputChunks[0], &mLastChunks[0],
                              &finished);
      } else {
        mEngine->ProcessBlocksOnPorts(
            this, aFrom, Span(mInputChunks.Elements(), mEngine->InputCount()),
            Span(mLastChunks.Elements(), mEngine->OutputCount()), &finished);
      }
    }
    for (uint16_t i = 0; i < outputCount; ++i) {
      NS_ASSERTION(mLastChunks[i].GetDuration() == WEBAUDIO_BLOCK_SIZE,
                   "Invalid WebAudio chunk size");
    }
    if (finished) {
      mMarkAsEndedAfterThisBlock = true;
      if (mIsActive) {
        ScheduleCheckForInactive();
      }
    }

    if (mDisabledMode != DisabledTrackMode::ENABLED) {
      for (uint32_t i = 0; i < outputCount; ++i) {
        mLastChunks[i].SetNull(WEBAUDIO_BLOCK_SIZE);
      }
    }
  }

  if (!mEnded) {
    // Don't output anything while finished
    if (mFlags & EXTERNAL_OUTPUT) {
      AdvanceOutputSegment();
    }
    if (mMarkAsEndedAfterThisBlock && (aFlags & ALLOW_END)) {
      // This track was ended the last time that we looked at it, and all
      // of the depending tracks have ended their output as well, so now
      // it's time to mark this track as ended.
      mEnded = true;
    }
  }
}

void AudioNodeTrack::ProduceOutputBeforeInput(GraphTime aFrom) {
  MOZ_ASSERT(mEngine->AsDelayNodeEngine());
  MOZ_ASSERT(mEngine->OutputCount() == 1,
             "DelayNodeEngine output count should be 1");
  MOZ_ASSERT(!InMutedCycle(), "DelayNodes should break cycles");
  MOZ_ASSERT(mLastChunks.Length() == 1);

  if (!mIsActive) {
    mLastChunks[0].SetNull(WEBAUDIO_BLOCK_SIZE);
  } else {
    mEngine->ProduceBlockBeforeInput(this, aFrom, &mLastChunks[0]);
    NS_ASSERTION(mLastChunks[0].GetDuration() == WEBAUDIO_BLOCK_SIZE,
                 "Invalid WebAudio chunk size");
    if (mDisabledMode != DisabledTrackMode::ENABLED) {
      mLastChunks[0].SetNull(WEBAUDIO_BLOCK_SIZE);
    }
  }
}

void AudioNodeTrack::AdvanceOutputSegment() {
  AudioSegment* segment = GetData<AudioSegment>();

  AudioChunk copyChunk = *mLastChunks[0].AsMutableChunk();
  AudioSegment tmpSegment;
  tmpSegment.AppendAndConsumeChunk(std::move(copyChunk));

  for (const auto& l : mTrackListeners) {
    // Notify MediaTrackListeners.
    l->NotifyQueuedChanges(Graph(), segment->GetDuration(), tmpSegment);
  }

  if (mLastChunks[0].IsNull()) {
    segment->AppendNullData(tmpSegment.GetDuration());
  } else {
    segment->AppendFrom(&tmpSegment);
  }
}

void AudioNodeTrack::AddInput(MediaInputPort* aPort) {
  ProcessedMediaTrack::AddInput(aPort);
  AudioNodeTrack* ns = aPort->GetSource()->AsAudioNodeTrack();
  // Tracks that are not AudioNodeTracks are considered active.
  if (!ns || (ns->mIsActive && !ns->IsAudioParamTrack())) {
    IncrementActiveInputCount();
  }
}
void AudioNodeTrack::RemoveInput(MediaInputPort* aPort) {
  ProcessedMediaTrack::RemoveInput(aPort);
  AudioNodeTrack* ns = aPort->GetSource()->AsAudioNodeTrack();
  // Tracks that are not AudioNodeTracks are considered active.
  if (!ns || (ns->mIsActive && !ns->IsAudioParamTrack())) {
    DecrementActiveInputCount();
  }
}

void AudioNodeTrack::SetActive() {
  if (mIsActive || mMarkAsEndedAfterThisBlock) {
    return;
  }

  mIsActive = true;
  if (!(mFlags & EXTERNAL_OUTPUT)) {
    DecrementSuspendCount();
  }
  if (IsAudioParamTrack()) {
    // Consumers merely influence track order.
    // They do not read from the track.
    return;
  }

  for (const auto& consumer : mConsumers) {
    AudioNodeTrack* ns = consumer->GetDestination()->AsAudioNodeTrack();
    if (ns) {
      ns->IncrementActiveInputCount();
    }
  }
}

class AudioNodeTrack::CheckForInactiveMessage final : public ControlMessage {
 public:
  explicit CheckForInactiveMessage(AudioNodeTrack* aTrack)
      : ControlMessage(aTrack) {}
  void Run() override {
    TRACE("AudioNodeTrack::CheckForInactive");
    auto ns = static_cast<AudioNodeTrack*>(mTrack);
    ns->CheckForInactive();
  }
};

void AudioNodeTrack::ScheduleCheckForInactive() {
  if (mActiveInputCount > 0 && !mMarkAsEndedAfterThisBlock) {
    return;
  }

  auto message = MakeUnique<CheckForInactiveMessage>(this);
  GraphImpl()->RunMessageAfterProcessing(std::move(message));
}

void AudioNodeTrack::CheckForInactive() {
  if (((mActiveInputCount > 0 || mEngine->IsActive()) &&
       !mMarkAsEndedAfterThisBlock) ||
      !mIsActive) {
    return;
  }

  mIsActive = false;
  mInputChunks.Clear();  // not required for foreseeable future
  for (auto& chunk : mLastChunks) {
    chunk.SetNull(WEBAUDIO_BLOCK_SIZE);
  }
  if (!(mFlags & EXTERNAL_OUTPUT)) {
    IncrementSuspendCount();
  }
  if (IsAudioParamTrack()) {
    return;
  }

  for (const auto& consumer : mConsumers) {
    AudioNodeTrack* ns = consumer->GetDestination()->AsAudioNodeTrack();
    if (ns) {
      ns->DecrementActiveInputCount();
    }
  }
}

void AudioNodeTrack::IncrementActiveInputCount() {
  ++mActiveInputCount;
  SetActive();
}

void AudioNodeTrack::DecrementActiveInputCount() {
  MOZ_ASSERT(mActiveInputCount > 0);
  --mActiveInputCount;
  CheckForInactive();
}

}  // namespace mozilla
