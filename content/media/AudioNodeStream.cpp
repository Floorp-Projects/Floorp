/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioNodeStream.h"

#include "MediaStreamGraphImpl.h"
#include "AudioNodeEngine.h"
#include "ThreeDPoint.h"

using namespace mozilla::dom;

namespace mozilla {

/**
 * An AudioNodeStream produces a single audio track with ID
 * AUDIO_NODE_STREAM_TRACK_ID. This track has rate AudioContext::sIdealAudioRate
 * for regular audio contexts, and the rate requested by the web content
 * for offline audio contexts.
 * Each chunk in the track is a single block of WEBAUDIO_BLOCK_SIZE samples.
 * Note: This must be a different value than MEDIA_STREAM_DEST_TRACK_ID
 */
static const int AUDIO_NODE_STREAM_TRACK_ID = 1;

AudioNodeStream::~AudioNodeStream()
{
  MOZ_COUNT_DTOR(AudioNodeStream);
}

void
AudioNodeStream::SetStreamTimeParameter(uint32_t aIndex, MediaStream* aRelativeToStream,
                                        double aStreamTime)
{
  class Message : public ControlMessage {
  public:
    Message(AudioNodeStream* aStream, uint32_t aIndex, MediaStream* aRelativeToStream,
            double aStreamTime)
      : ControlMessage(aStream), mStreamTime(aStreamTime),
        mRelativeToStream(aRelativeToStream), mIndex(aIndex) {}
    virtual void Run()
    {
      static_cast<AudioNodeStream*>(mStream)->
          SetStreamTimeParameterImpl(mIndex, mRelativeToStream, mStreamTime);
    }
    double mStreamTime;
    MediaStream* mRelativeToStream;
    uint32_t mIndex;
  };

  MOZ_ASSERT(this);
  GraphImpl()->AppendMessage(new Message(this, aIndex, aRelativeToStream, aStreamTime));
}

void
AudioNodeStream::SetStreamTimeParameterImpl(uint32_t aIndex, MediaStream* aRelativeToStream,
                                            double aStreamTime)
{
  TrackTicks ticks =
      WebAudioUtils::ConvertDestinationStreamTimeToSourceStreamTime(
          aStreamTime, this, aRelativeToStream);
  mEngine->SetStreamTimeParameter(aIndex, ticks);
}

void
AudioNodeStream::SetDoubleParameter(uint32_t aIndex, double aValue)
{
  class Message : public ControlMessage {
  public:
    Message(AudioNodeStream* aStream, uint32_t aIndex, double aValue)
      : ControlMessage(aStream), mValue(aValue), mIndex(aIndex) {}
    virtual void Run()
    {
      static_cast<AudioNodeStream*>(mStream)->Engine()->
          SetDoubleParameter(mIndex, mValue);
    }
    double mValue;
    uint32_t mIndex;
  };

  MOZ_ASSERT(this);
  GraphImpl()->AppendMessage(new Message(this, aIndex, aValue));
}

void
AudioNodeStream::SetInt32Parameter(uint32_t aIndex, int32_t aValue)
{
  class Message : public ControlMessage {
  public:
    Message(AudioNodeStream* aStream, uint32_t aIndex, int32_t aValue)
      : ControlMessage(aStream), mValue(aValue), mIndex(aIndex) {}
    virtual void Run()
    {
      static_cast<AudioNodeStream*>(mStream)->Engine()->
          SetInt32Parameter(mIndex, mValue);
    }
    int32_t mValue;
    uint32_t mIndex;
  };

  MOZ_ASSERT(this);
  GraphImpl()->AppendMessage(new Message(this, aIndex, aValue));
}

void
AudioNodeStream::SetTimelineParameter(uint32_t aIndex,
                                      const AudioParamTimeline& aValue)
{
  class Message : public ControlMessage {
  public:
    Message(AudioNodeStream* aStream, uint32_t aIndex,
            const AudioParamTimeline& aValue)
      : ControlMessage(aStream),
        mValue(aValue),
        mSampleRate(aStream->SampleRate()),
        mIndex(aIndex) {}
    virtual void Run()
    {
      static_cast<AudioNodeStream*>(mStream)->Engine()->
          SetTimelineParameter(mIndex, mValue, mSampleRate);
    }
    AudioParamTimeline mValue;
    TrackRate mSampleRate;
    uint32_t mIndex;
  };
  GraphImpl()->AppendMessage(new Message(this, aIndex, aValue));
}

void
AudioNodeStream::SetThreeDPointParameter(uint32_t aIndex, const ThreeDPoint& aValue)
{
  class Message : public ControlMessage {
  public:
    Message(AudioNodeStream* aStream, uint32_t aIndex, const ThreeDPoint& aValue)
      : ControlMessage(aStream), mValue(aValue), mIndex(aIndex) {}
    virtual void Run()
    {
      static_cast<AudioNodeStream*>(mStream)->Engine()->
          SetThreeDPointParameter(mIndex, mValue);
    }
    ThreeDPoint mValue;
    uint32_t mIndex;
  };

  MOZ_ASSERT(this);
  GraphImpl()->AppendMessage(new Message(this, aIndex, aValue));
}

void
AudioNodeStream::SetBuffer(already_AddRefed<ThreadSharedFloatArrayBufferList> aBuffer)
{
  class Message : public ControlMessage {
  public:
    Message(AudioNodeStream* aStream,
            already_AddRefed<ThreadSharedFloatArrayBufferList> aBuffer)
      : ControlMessage(aStream), mBuffer(aBuffer) {}
    virtual void Run()
    {
      static_cast<AudioNodeStream*>(mStream)->Engine()->
          SetBuffer(mBuffer.forget());
    }
    nsRefPtr<ThreadSharedFloatArrayBufferList> mBuffer;
  };

  MOZ_ASSERT(this);
  GraphImpl()->AppendMessage(new Message(this, aBuffer));
}

void
AudioNodeStream::SetRawArrayData(nsTArray<float>& aData)
{
  class Message : public ControlMessage {
  public:
    Message(AudioNodeStream* aStream,
            nsTArray<float>& aData)
      : ControlMessage(aStream)
    {
      mData.SwapElements(aData);
    }
    virtual void Run()
    {
      static_cast<AudioNodeStream*>(mStream)->Engine()->SetRawArrayData(mData);
    }
    nsTArray<float> mData;
  };

  MOZ_ASSERT(this);
  GraphImpl()->AppendMessage(new Message(this, aData));
}

void
AudioNodeStream::SetChannelMixingParameters(uint32_t aNumberOfChannels,
                                            ChannelCountMode aChannelCountMode,
                                            ChannelInterpretation aChannelInterpretation)
{
  class Message : public ControlMessage {
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
    virtual void Run()
    {
      static_cast<AudioNodeStream*>(mStream)->
        SetChannelMixingParametersImpl(mNumberOfChannels, mChannelCountMode,
                                       mChannelInterpretation);
    }
    uint32_t mNumberOfChannels;
    ChannelCountMode mChannelCountMode;
    ChannelInterpretation mChannelInterpretation;
  };

  MOZ_ASSERT(this);
  GraphImpl()->AppendMessage(new Message(this, aNumberOfChannels,
                                         aChannelCountMode,
                                         aChannelInterpretation));
}

void
AudioNodeStream::SetChannelMixingParametersImpl(uint32_t aNumberOfChannels,
                                                ChannelCountMode aChannelCountMode,
                                                ChannelInterpretation aChannelInterpretation)
{
  // Make sure that we're not clobbering any significant bits by fitting these
  // values in 16 bits.
  MOZ_ASSERT(int(aChannelCountMode) < INT16_MAX);
  MOZ_ASSERT(int(aChannelInterpretation) < INT16_MAX);

  mNumberOfInputChannels = aNumberOfChannels;
  mChannelCountMode = aChannelCountMode;
  mChannelInterpretation = aChannelInterpretation;
}

bool
AudioNodeStream::AllInputsFinished() const
{
  uint32_t inputCount = mInputs.Length();
  for (uint32_t i = 0; i < inputCount; ++i) {
    if (!mInputs[i]->GetSource()->IsFinishedOnGraphThread()) {
      return false;
    }
  }
  return !!inputCount;
}

void
AudioNodeStream::ObtainInputBlock(AudioChunk& aTmpChunk, uint32_t aPortIndex)
{
  uint32_t inputCount = mInputs.Length();
  uint32_t outputChannelCount = 1;
  nsAutoTArray<AudioChunk*,250> inputChunks;
  for (uint32_t i = 0; i < inputCount; ++i) {
    if (aPortIndex != mInputs[i]->InputNumber()) {
      // This input is connected to a different port
      continue;
    }
    MediaStream* s = mInputs[i]->GetSource();
    AudioNodeStream* a = static_cast<AudioNodeStream*>(s);
    MOZ_ASSERT(a == s->AsAudioNodeStream());
    if (a->IsFinishedOnGraphThread() ||
        a->IsAudioParamStream()) {
      continue;
    }
    AudioChunk* chunk = &a->mLastChunks[mInputs[i]->OutputNumber()];
    MOZ_ASSERT(chunk);
    if (chunk->IsNull()) {
      continue;
    }

    inputChunks.AppendElement(chunk);
    outputChannelCount =
      GetAudioChannelsSuperset(outputChannelCount, chunk->mChannelData.Length());
  }

  switch (mChannelCountMode) {
  case ChannelCountMode::Explicit:
    // Disregard the output channel count that we've calculated, and just use
    // mNumberOfInputChannels.
    outputChannelCount = mNumberOfInputChannels;
    break;
  case ChannelCountMode::Clamped_max:
    // Clamp the computed output channel count to mNumberOfInputChannels.
    outputChannelCount = std::min(outputChannelCount, mNumberOfInputChannels);
    break;
  case ChannelCountMode::Max:
    // Nothing to do here, just shut up the compiler warning.
    break;
  }

  uint32_t inputChunkCount = inputChunks.Length();
  if (inputChunkCount == 0 ||
      (inputChunkCount == 1 && inputChunks[0]->mChannelData.Length() == 0)) {
    aTmpChunk.SetNull(WEBAUDIO_BLOCK_SIZE);
    return;
  }

  if (inputChunkCount == 1 &&
      inputChunks[0]->mChannelData.Length() == outputChannelCount) {
    aTmpChunk = *inputChunks[0];
    return;
  }

  if (outputChannelCount == 0) {
    aTmpChunk.SetNull(WEBAUDIO_BLOCK_SIZE);
    return;
  }

  AllocateAudioBlock(outputChannelCount, &aTmpChunk);
  float silenceChannel[WEBAUDIO_BLOCK_SIZE] = {0.f};
  // The static storage here should be 1KB, so it's fine
  nsAutoTArray<float, GUESS_AUDIO_CHANNELS*WEBAUDIO_BLOCK_SIZE> downmixBuffer;

  for (uint32_t i = 0; i < inputChunkCount; ++i) {
    AudioChunk* chunk = inputChunks[i];
    nsAutoTArray<const void*,GUESS_AUDIO_CHANNELS> channels;
    channels.AppendElements(chunk->mChannelData);
    if (channels.Length() < outputChannelCount) {
      if (mChannelInterpretation == ChannelInterpretation::Speakers) {
        AudioChannelsUpMix(&channels, outputChannelCount, nullptr);
        NS_ASSERTION(outputChannelCount == channels.Length(),
                     "We called GetAudioChannelsSuperset to avoid this");
      } else {
        // Fill up the remaining channels by zeros
        for (uint32_t j = channels.Length(); j < outputChannelCount; ++j) {
          channels.AppendElement(silenceChannel);
        }
      }
    } else if (channels.Length() > outputChannelCount) {
      if (mChannelInterpretation == ChannelInterpretation::Speakers) {
        nsAutoTArray<float*,GUESS_AUDIO_CHANNELS> outputChannels;
        outputChannels.SetLength(outputChannelCount);
        downmixBuffer.SetLength(outputChannelCount * WEBAUDIO_BLOCK_SIZE);
        for (uint32_t j = 0; j < outputChannelCount; ++j) {
          outputChannels[j] = &downmixBuffer[j * WEBAUDIO_BLOCK_SIZE];
        }

        AudioChannelsDownMix(channels, outputChannels.Elements(),
                             outputChannelCount, WEBAUDIO_BLOCK_SIZE);

        channels.SetLength(outputChannelCount);
        for (uint32_t j = 0; j < channels.Length(); ++j) {
          channels[j] = outputChannels[j];
        }
      } else {
        // Drop the remaining channels
        channels.RemoveElementsAt(outputChannelCount,
                                  channels.Length() - outputChannelCount);
      }
    }

    for (uint32_t c = 0; c < channels.Length(); ++c) {
      const float* inputData = static_cast<const float*>(channels[c]);
      float* outputData = static_cast<float*>(const_cast<void*>(aTmpChunk.mChannelData[c]));
      if (inputData) {
        if (i == 0) {
          AudioBlockCopyChannelWithScale(inputData, chunk->mVolume, outputData);
        } else {
          AudioBlockAddChannelWithScale(inputData, chunk->mVolume, outputData);
        }
      } else {
        if (i == 0) {
          memset(outputData, 0, WEBAUDIO_BLOCK_SIZE*sizeof(float));
        }
      }
    }
  }
}

// The MediaStreamGraph guarantees that this is actually one block, for
// AudioNodeStreams.
void
AudioNodeStream::ProduceOutput(GraphTime aFrom, GraphTime aTo)
{
  if (mMarkAsFinishedAfterThisBlock) {
    // This stream was finished the last time that we looked at it, and all
    // of the depending streams have finished their output as well, so now
    // it's time to mark this stream as finished.
    FinishOutput();
  }

  StreamBuffer::Track* track = EnsureTrack(AUDIO_NODE_STREAM_TRACK_ID, mSampleRate);

  AudioSegment* segment = track->Get<AudioSegment>();

  uint16_t outputCount = std::max(uint16_t(1), mEngine->OutputCount());
  mLastChunks.SetLength(outputCount);

  if (mInCycle) {
    // XXX DelayNode not supported yet so just produce silence
    for (uint16_t i = 0; i < outputCount; ++i) {
      mLastChunks[i].SetNull(WEBAUDIO_BLOCK_SIZE);
    }
  } else {
    for (uint16_t i = 0; i < outputCount; ++i) {
      mLastChunks[i].SetNull(0);
    }

    // We need to generate at least one input
    uint16_t maxInputs = std::max(uint16_t(1), mEngine->InputCount());
    OutputChunks inputChunks;
    inputChunks.SetLength(maxInputs);
    for (uint16_t i = 0; i < maxInputs; ++i) {
      ObtainInputBlock(inputChunks[i], i);
    }
    bool finished = false;
    if (maxInputs <= 1 && mEngine->OutputCount() <= 1) {
      mEngine->ProduceAudioBlock(this, inputChunks[0], &mLastChunks[0], &finished);
    } else {
      mEngine->ProduceAudioBlocksOnPorts(this, inputChunks, mLastChunks, &finished);
    }
    if (finished) {
      mMarkAsFinishedAfterThisBlock = true;
    }
  }

  if (mDisabledTrackIDs.Contains(AUDIO_NODE_STREAM_TRACK_ID)) {
    for (uint32_t i = 0; i < mLastChunks.Length(); ++i) {
      mLastChunks[i].SetNull(WEBAUDIO_BLOCK_SIZE);
    }
  }

  if (mKind == MediaStreamGraph::EXTERNAL_STREAM) {
    segment->AppendAndConsumeChunk(&mLastChunks[0]);
  } else {
    segment->AppendNullData(mLastChunks[0].GetDuration());
  }

  for (uint32_t j = 0; j < mListeners.Length(); ++j) {
    MediaStreamListener* l = mListeners[j];
    AudioChunk copyChunk = mLastChunks[0];
    AudioSegment tmpSegment;
    tmpSegment.AppendAndConsumeChunk(&copyChunk);
    l->NotifyQueuedTrackChanges(Graph(), AUDIO_NODE_STREAM_TRACK_ID,
                                mSampleRate, segment->GetDuration(), 0,
                                tmpSegment);
  }
}

TrackTicks
AudioNodeStream::GetCurrentPosition()
{
  return EnsureTrack(AUDIO_NODE_STREAM_TRACK_ID, mSampleRate)->Get<AudioSegment>()->GetDuration();
}

void
AudioNodeStream::FinishOutput()
{
  if (IsFinishedOnGraphThread()) {
    return;
  }

  StreamBuffer::Track* track = EnsureTrack(AUDIO_NODE_STREAM_TRACK_ID, mSampleRate);
  track->SetEnded();
  FinishOnGraphThread();

  for (uint32_t j = 0; j < mListeners.Length(); ++j) {
    MediaStreamListener* l = mListeners[j];
    AudioSegment emptySegment;
    l->NotifyQueuedTrackChanges(Graph(), AUDIO_NODE_STREAM_TRACK_ID,
                                mSampleRate,
                                track->GetSegment()->GetDuration(),
                                MediaStreamListener::TRACK_EVENT_ENDED, emptySegment);
  }
}

}
