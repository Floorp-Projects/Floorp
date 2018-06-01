/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioNodeStream.h"

#include "MediaStreamGraphImpl.h"
#include "MediaStreamListener.h"
#include "AudioNodeEngine.h"
#include "ThreeDPoint.h"
#include "AudioChannelFormat.h"
#include "AudioParamTimeline.h"
#include "AudioContext.h"
#include "nsMathUtils.h"
#include "AlignmentUtils.h"

using namespace mozilla::dom;

namespace mozilla {

/**
 * An AudioNodeStream produces a single audio track with ID
 * AUDIO_TRACK. This track has rate AudioContext::sIdealAudioRate
 * for regular audio contexts, and the rate requested by the web content
 * for offline audio contexts.
 * Each chunk in the track is a single block of WEBAUDIO_BLOCK_SIZE samples.
 * Note: This must be a different value than MEDIA_STREAM_DEST_TRACK_ID
 */

AudioNodeStream::AudioNodeStream(AudioNodeEngine* aEngine,
                                 Flags aFlags,
                                 TrackRate aSampleRate)
  : ProcessedMediaStream()
  , mEngine(aEngine)
  , mSampleRate(aSampleRate)
  , mFlags(aFlags)
  , mNumberOfInputChannels(2)
  , mIsActive(aEngine->IsActive())
  , mMarkAsFinishedAfterThisBlock(false)
  , mAudioParamStream(false)
  , mPassThrough(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  mSuspendedCount = !(mIsActive || mFlags & EXTERNAL_OUTPUT);
  mChannelCountMode = ChannelCountMode::Max;
  mChannelInterpretation = ChannelInterpretation::Speakers;
  // AudioNodes are always producing data
  mHasCurrentData = true;
  mLastChunks.SetLength(std::max(uint16_t(1), mEngine->OutputCount()));
  MOZ_COUNT_CTOR(AudioNodeStream);
}

AudioNodeStream::~AudioNodeStream()
{
  MOZ_ASSERT(mActiveInputCount == 0);
  MOZ_COUNT_DTOR(AudioNodeStream);
}

void
AudioNodeStream::DestroyImpl()
{
  // These are graph thread objects, so clean up on graph thread.
  mInputChunks.Clear();
  mLastChunks.Clear();

  ProcessedMediaStream::DestroyImpl();
}

/* static */ already_AddRefed<AudioNodeStream>
AudioNodeStream::Create(AudioContext* aCtx, AudioNodeEngine* aEngine,
                        Flags aFlags, MediaStreamGraph* aGraph)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(aGraph);

  // MediaRecorders use an AudioNodeStream, but no AudioNode
  AudioNode* node = aEngine->NodeMainThread();

  RefPtr<AudioNodeStream> stream =
    new AudioNodeStream(aEngine, aFlags, aGraph->GraphRate());
  stream->mSuspendedCount += aCtx->ShouldSuspendNewStream();
  if (node) {
    stream->SetChannelMixingParametersImpl(node->ChannelCount(),
                                           node->ChannelCountModeValue(),
                                           node->ChannelInterpretationValue());
  }
  aGraph->AddStream(stream);
  return stream.forget();
}

size_t
AudioNodeStream::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = 0;

  // Not reported:
  // - mEngine

  amount += ProcessedMediaStream::SizeOfExcludingThis(aMallocSizeOf);
  amount += mLastChunks.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (size_t i = 0; i < mLastChunks.Length(); i++) {
    // NB: This is currently unshared only as there are instances of
    //     double reporting in DMD otherwise.
    amount += mLastChunks[i].SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  return amount;
}

size_t
AudioNodeStream::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

void
AudioNodeStream::SizeOfAudioNodesIncludingThis(MallocSizeOf aMallocSizeOf,
                                               AudioNodeSizes& aUsage) const
{
  // Explicitly separate out the stream memory.
  aUsage.mStream = SizeOfIncludingThis(aMallocSizeOf);

  if (mEngine) {
    // This will fill out the rest of |aUsage|.
    mEngine->SizeOfIncludingThis(aMallocSizeOf, aUsage);
  }
}

void
AudioNodeStream::SetStreamTimeParameter(uint32_t aIndex, AudioContext* aContext,
                                        double aStreamTime)
{
  class Message final : public ControlMessage
  {
  public:
    Message(AudioNodeStream* aStream, uint32_t aIndex, MediaStream* aRelativeToStream,
            double aStreamTime)
      : ControlMessage(aStream), mStreamTime(aStreamTime),
        mRelativeToStream(aRelativeToStream), mIndex(aIndex)
    {}
    void Run() override
    {
      static_cast<AudioNodeStream*>(mStream)->
          SetStreamTimeParameterImpl(mIndex, mRelativeToStream, mStreamTime);
    }
    double mStreamTime;
    MediaStream* MOZ_UNSAFE_REF("ControlMessages are processed in order.  This \
destination stream is not yet destroyed.  Its (future) destroy message will be \
processed after this message.") mRelativeToStream;
    uint32_t mIndex;
  };

  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aIndex,
                                                 aContext->DestinationStream(),
                                                 aStreamTime));
}

void
AudioNodeStream::SetStreamTimeParameterImpl(uint32_t aIndex, MediaStream* aRelativeToStream,
                                            double aStreamTime)
{
  StreamTime ticks = aRelativeToStream->SecondsToNearestStreamTime(aStreamTime);
  mEngine->SetStreamTimeParameter(aIndex, ticks);
}

void
AudioNodeStream::SetDoubleParameter(uint32_t aIndex, double aValue)
{
  class Message final : public ControlMessage
  {
  public:
    Message(AudioNodeStream* aStream, uint32_t aIndex, double aValue)
      : ControlMessage(aStream), mValue(aValue), mIndex(aIndex)
    {}
    void Run() override
    {
      static_cast<AudioNodeStream*>(mStream)->Engine()->
          SetDoubleParameter(mIndex, mValue);
    }
    double mValue;
    uint32_t mIndex;
  };

  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aIndex, aValue));
}

void
AudioNodeStream::SetInt32Parameter(uint32_t aIndex, int32_t aValue)
{
  class Message final : public ControlMessage
  {
  public:
    Message(AudioNodeStream* aStream, uint32_t aIndex, int32_t aValue)
      : ControlMessage(aStream), mValue(aValue), mIndex(aIndex)
    {}
    void Run() override
    {
      static_cast<AudioNodeStream*>(mStream)->Engine()->
          SetInt32Parameter(mIndex, mValue);
    }
    int32_t mValue;
    uint32_t mIndex;
  };

  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aIndex, aValue));
}

void
AudioNodeStream::SendTimelineEvent(uint32_t aIndex,
                                   const AudioTimelineEvent& aEvent)
{
  class Message final : public ControlMessage
  {
  public:
    Message(AudioNodeStream* aStream, uint32_t aIndex,
            const AudioTimelineEvent& aEvent)
      : ControlMessage(aStream),
        mEvent(aEvent),
        mSampleRate(aStream->SampleRate()),
        mIndex(aIndex)
    {}
    void Run() override
    {
      static_cast<AudioNodeStream*>(mStream)->Engine()->
          RecvTimelineEvent(mIndex, mEvent);
    }
    AudioTimelineEvent mEvent;
    TrackRate mSampleRate;
    uint32_t mIndex;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aIndex, aEvent));
}

void
AudioNodeStream::SetThreeDPointParameter(uint32_t aIndex, const ThreeDPoint& aValue)
{
  class Message final : public ControlMessage
  {
  public:
    Message(AudioNodeStream* aStream, uint32_t aIndex, const ThreeDPoint& aValue)
      : ControlMessage(aStream), mValue(aValue), mIndex(aIndex)
    {}
    void Run() override
    {
      static_cast<AudioNodeStream*>(mStream)->Engine()->
          SetThreeDPointParameter(mIndex, mValue);
    }
    ThreeDPoint mValue;
    uint32_t mIndex;
  };

  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aIndex, aValue));
}

void
AudioNodeStream::SetBuffer(AudioChunk&& aBuffer)
{
  class Message final : public ControlMessage
  {
  public:
    Message(AudioNodeStream* aStream, AudioChunk&& aBuffer)
      : ControlMessage(aStream), mBuffer(aBuffer)
    {}
    void Run() override
    {
      static_cast<AudioNodeStream*>(mStream)->Engine()->
        SetBuffer(std::move(mBuffer));
    }
    AudioChunk mBuffer;
  };

  GraphImpl()->AppendMessage(MakeUnique<Message>(this, std::move(aBuffer)));
}

void
AudioNodeStream::SetRawArrayData(nsTArray<float>& aData)
{
  class Message final : public ControlMessage
  {
  public:
    Message(AudioNodeStream* aStream,
            nsTArray<float>& aData)
      : ControlMessage(aStream)
    {
      mData.SwapElements(aData);
    }
    void Run() override
    {
      static_cast<AudioNodeStream*>(mStream)->Engine()->SetRawArrayData(mData);
    }
    nsTArray<float> mData;
  };

  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aData));
}

void
AudioNodeStream::SetChannelMixingParameters(uint32_t aNumberOfChannels,
                                            ChannelCountMode aChannelCountMode,
                                            ChannelInterpretation aChannelInterpretation)
{
  class Message final : public ControlMessage
  {
  public:
    Message(AudioNodeStream* aStream,
            uint32_t aNumberOfChannels,
            ChannelCountMode aChannelCountMode,
            ChannelInterpretation aChannelInterpretation)
      : ControlMessage(aStream),
        mNumberOfChannels(aNumberOfChannels),
        mChannelCountMode(aChannelCountMode),
        mChannelInterpretation(aChannelInterpretation)
    {}
    void Run() override
    {
      static_cast<AudioNodeStream*>(mStream)->
        SetChannelMixingParametersImpl(mNumberOfChannels, mChannelCountMode,
                                       mChannelInterpretation);
    }
    uint32_t mNumberOfChannels;
    ChannelCountMode mChannelCountMode;
    ChannelInterpretation mChannelInterpretation;
  };

  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aNumberOfChannels,
                                                 aChannelCountMode,
                                                 aChannelInterpretation));
}

void
AudioNodeStream::SetPassThrough(bool aPassThrough)
{
  class Message final : public ControlMessage
  {
  public:
    Message(AudioNodeStream* aStream, bool aPassThrough)
      : ControlMessage(aStream), mPassThrough(aPassThrough)
    {}
    void Run() override
    {
      static_cast<AudioNodeStream*>(mStream)->mPassThrough = mPassThrough;
    }
    bool mPassThrough;
  };

  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aPassThrough));
}

void
AudioNodeStream::SetChannelMixingParametersImpl(uint32_t aNumberOfChannels,
                                                ChannelCountMode aChannelCountMode,
                                                ChannelInterpretation aChannelInterpretation)
{
  mNumberOfInputChannels = aNumberOfChannels;
  mChannelCountMode = aChannelCountMode;
  mChannelInterpretation = aChannelInterpretation;
}

uint32_t
AudioNodeStream::ComputedNumberOfChannels(uint32_t aInputChannelCount)
{
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

class AudioNodeStream::AdvanceAndResumeMessage final : public ControlMessage {
public:
  AdvanceAndResumeMessage(AudioNodeStream* aStream, StreamTime aAdvance) :
    ControlMessage(aStream), mAdvance(aAdvance) {}
  void Run() override
  {
    auto ns = static_cast<AudioNodeStream*>(mStream);
    ns->mTracksStartTime -= mAdvance;

    StreamTracks::Track* track = ns->EnsureTrack(AUDIO_TRACK);
    track->Get<AudioSegment>()->AppendNullData(mAdvance);

    ns->GraphImpl()->DecrementSuspendCount(mStream);
  }
private:
  StreamTime mAdvance;
};

void
AudioNodeStream::AdvanceAndResume(StreamTime aAdvance)
{
  mMainThreadCurrentTime += aAdvance;
  GraphImpl()->AppendMessage(MakeUnique<AdvanceAndResumeMessage>(this, aAdvance));
}

void
AudioNodeStream::ObtainInputBlock(AudioBlock& aTmpChunk,
                                  uint32_t aPortIndex)
{
  uint32_t inputCount = mInputs.Length();
  uint32_t outputChannelCount = 1;
  AutoTArray<const AudioBlock*,250> inputChunks;
  for (uint32_t i = 0; i < inputCount; ++i) {
    if (aPortIndex != mInputs[i]->InputNumber()) {
      // This input is connected to a different port
      continue;
    }
    MediaStream* s = mInputs[i]->GetSource();
    AudioNodeStream* a = static_cast<AudioNodeStream*>(s);
    MOZ_ASSERT(a == s->AsAudioNodeStream());
    if (a->IsAudioParamStream()) {
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

void
AudioNodeStream::AccumulateInputChunk(uint32_t aInputIndex,
                                      const AudioBlock& aChunk,
                                      AudioBlock* aBlock,
                                      DownmixBufferType* aDownmixBuffer)
{
  AutoTArray<const float*,GUESS_AUDIO_CHANNELS> channels;
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

void
AudioNodeStream::UpMixDownMixChunk(const AudioBlock* aChunk,
                                   uint32_t aOutputChannelCount,
                                   nsTArray<const float*>& aOutputChannels,
                                   DownmixBufferType& aDownmixBuffer)
{
  for (uint32_t i = 0; i < aChunk->ChannelCount(); i++) {
    aOutputChannels.AppendElement(static_cast<const float*>(aChunk->mChannelData[i]));
  }
  if (aOutputChannels.Length() < aOutputChannelCount) {
    if (mChannelInterpretation == ChannelInterpretation::Speakers) {
      AudioChannelsUpMix<float>(&aOutputChannels, aOutputChannelCount, nullptr);
      NS_ASSERTION(aOutputChannelCount == aOutputChannels.Length(),
                   "We called GetAudioChannelsSuperset to avoid this");
    } else {
      // Fill up the remaining aOutputChannels by zeros
      for (uint32_t j = aOutputChannels.Length(); j < aOutputChannelCount; ++j) {
        aOutputChannels.AppendElement(nullptr);
      }
    }
  } else if (aOutputChannels.Length() > aOutputChannelCount) {
    if (mChannelInterpretation == ChannelInterpretation::Speakers) {
      AutoTArray<float*,GUESS_AUDIO_CHANNELS> outputChannels;
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
      aOutputChannels.RemoveElementsAt(aOutputChannelCount,
        aOutputChannels.Length() - aOutputChannelCount);
    }
  }
}

// The MediaStreamGraph guarantees that this is actually one block, for
// AudioNodeStreams.
void
AudioNodeStream::ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags)
{
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
      MOZ_ASSERT(outputCount == 1, "For now, we only support nodes that have one output port");
      mLastChunks[0] = mInputChunks[0];
    } else {
      if (maxInputs <= 1 && outputCount <= 1) {
        mEngine->ProcessBlock(this, aFrom,
                              mInputChunks[0], &mLastChunks[0], &finished);
      } else {
        mEngine->ProcessBlocksOnPorts(this, mInputChunks, mLastChunks, &finished);
      }
    }
    for (uint16_t i = 0; i < outputCount; ++i) {
      NS_ASSERTION(mLastChunks[i].GetDuration() == WEBAUDIO_BLOCK_SIZE,
                   "Invalid WebAudio chunk size");
    }
    if (finished) {
      mMarkAsFinishedAfterThisBlock = true;
      if (mIsActive) {
        ScheduleCheckForInactive();
      }
    }

    if (GetDisabledTrackMode(static_cast<TrackID>(AUDIO_TRACK)) != DisabledTrackMode::ENABLED) {
      for (uint32_t i = 0; i < outputCount; ++i) {
        mLastChunks[i].SetNull(WEBAUDIO_BLOCK_SIZE);
      }
    }
  }

  if (!mFinished) {
    // Don't output anything while finished
    if (mFlags & EXTERNAL_OUTPUT) {
      AdvanceOutputSegment();
    }
    if (mMarkAsFinishedAfterThisBlock && (aFlags & ALLOW_FINISH)) {
      // This stream was finished the last time that we looked at it, and all
      // of the depending streams have finished their output as well, so now
      // it's time to mark this stream as finished.
      if (mFlags & EXTERNAL_OUTPUT) {
        FinishOutput();
      }
      FinishOnGraphThread();
    }
  }
}

void
AudioNodeStream::ProduceOutputBeforeInput(GraphTime aFrom)
{
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
    if (GetDisabledTrackMode(static_cast<TrackID>(AUDIO_TRACK)) != DisabledTrackMode::ENABLED) {
      mLastChunks[0].SetNull(WEBAUDIO_BLOCK_SIZE);
    }
  }
}

void
AudioNodeStream::AdvanceOutputSegment()
{
  StreamTracks::Track* track = EnsureTrack(AUDIO_TRACK);
  // No more tracks will be coming
  mTracks.AdvanceKnownTracksTime(STREAM_TIME_MAX);

  AudioSegment* segment = track->Get<AudioSegment>();

  AudioChunk copyChunk = *mLastChunks[0].AsMutableChunk();
  AudioSegment tmpSegment;
  tmpSegment.AppendAndConsumeChunk(&copyChunk);

  for (uint32_t j = 0; j < mListeners.Length(); ++j) {
    MediaStreamListener* l = mListeners[j];
    // Notify MediaStreamListeners.
    l->NotifyQueuedTrackChanges(Graph(), AUDIO_TRACK,
                                segment->GetDuration(), TrackEventCommand::TRACK_EVENT_NONE, tmpSegment);
  }
  for (TrackBound<MediaStreamTrackListener>& b : mTrackListeners) {
    // Notify MediaStreamTrackListeners.
    if (b.mTrackID != AUDIO_TRACK) {
      continue;
    }
    b.mListener->NotifyQueuedChanges(Graph(), segment->GetDuration(), tmpSegment);
  }

  if (mLastChunks[0].IsNull()) {
    segment->AppendNullData(tmpSegment.GetDuration());
  } else {
    segment->AppendFrom(&tmpSegment);
  }
}

void
AudioNodeStream::FinishOutput()
{
  StreamTracks::Track* track = EnsureTrack(AUDIO_TRACK);
  track->SetEnded();

  for (uint32_t j = 0; j < mListeners.Length(); ++j) {
    MediaStreamListener* l = mListeners[j];
    AudioSegment emptySegment;
    l->NotifyQueuedTrackChanges(Graph(), AUDIO_TRACK,
                                track->GetSegment()->GetDuration(),
                                TrackEventCommand::TRACK_EVENT_ENDED, emptySegment);
  }
  for (TrackBound<MediaStreamTrackListener>& b : mTrackListeners) {
    // Notify MediaStreamTrackListeners.
    if (b.mTrackID != AUDIO_TRACK) {
      continue;
    }
    b.mListener->NotifyEnded();
  }
}

void
AudioNodeStream::AddInput(MediaInputPort* aPort)
{
  ProcessedMediaStream::AddInput(aPort);
  AudioNodeStream* ns = aPort->GetSource()->AsAudioNodeStream();
  // Streams that are not AudioNodeStreams are considered active.
  if (!ns || (ns->mIsActive && !ns->IsAudioParamStream())) {
    IncrementActiveInputCount();
  }
}
void
AudioNodeStream::RemoveInput(MediaInputPort* aPort)
{
  ProcessedMediaStream::RemoveInput(aPort);
  AudioNodeStream* ns = aPort->GetSource()->AsAudioNodeStream();
  // Streams that are not AudioNodeStreams are considered active.
  if (!ns || (ns->mIsActive && !ns->IsAudioParamStream())) {
    DecrementActiveInputCount();
  }
}

void
AudioNodeStream::SetActive()
{
  if (mIsActive || mMarkAsFinishedAfterThisBlock) {
    return;
  }

  mIsActive = true;
  if (!(mFlags & EXTERNAL_OUTPUT)) {
    GraphImpl()->DecrementSuspendCount(this);
  }
  if (IsAudioParamStream()) {
    // Consumers merely influence stream order.
    // They do not read from the stream.
    return;
  }

  for (const auto& consumer : mConsumers) {
    AudioNodeStream* ns = consumer->GetDestination()->AsAudioNodeStream();
    if (ns) {
      ns->IncrementActiveInputCount();
    }
  }
}

class AudioNodeStream::CheckForInactiveMessage final : public ControlMessage
{
public:
  explicit CheckForInactiveMessage(AudioNodeStream* aStream) :
    ControlMessage(aStream) {}
  void Run() override
  {
    auto ns = static_cast<AudioNodeStream*>(mStream);
    ns->CheckForInactive();
  }
};

void
AudioNodeStream::ScheduleCheckForInactive()
{
  if (mActiveInputCount > 0 && !mMarkAsFinishedAfterThisBlock) {
    return;
  }

  auto message = MakeUnique<CheckForInactiveMessage>(this);
  GraphImpl()->RunMessageAfterProcessing(std::move(message));
}

void
AudioNodeStream::CheckForInactive()
{
  if (((mActiveInputCount > 0 || mEngine->IsActive()) &&
       !mMarkAsFinishedAfterThisBlock) ||
      !mIsActive) {
    return;
  }

  mIsActive = false;
  mInputChunks.Clear(); // not required for foreseeable future
  for (auto& chunk : mLastChunks) {
    chunk.SetNull(WEBAUDIO_BLOCK_SIZE);
  }
  if (!(mFlags & EXTERNAL_OUTPUT)) {
    GraphImpl()->IncrementSuspendCount(this);
  }
  if (IsAudioParamStream()) {
    return;
  }

  for (const auto& consumer : mConsumers) {
    AudioNodeStream* ns = consumer->GetDestination()->AsAudioNodeStream();
    if (ns) {
      ns->DecrementActiveInputCount();
    }
  }
}

void
AudioNodeStream::IncrementActiveInputCount()
{
  ++mActiveInputCount;
  SetActive();
}

void
AudioNodeStream::DecrementActiveInputCount()
{
  MOZ_ASSERT(mActiveInputCount > 0);
  --mActiveInputCount;
  CheckForInactive();
}

} // namespace mozilla
