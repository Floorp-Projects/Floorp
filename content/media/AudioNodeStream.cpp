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
 * AUDIO_NODE_STREAM_TRACK_ID. This track has rate IdealAudioRate().
 * Each chunk in the track is a single block of WEBAUDIO_BLOCK_SIZE samples.
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
  StreamTime streamTime = std::max<MediaTime>(0, SecondsToMediaTime(aStreamTime));
  GraphTime graphTime = aRelativeToStream->StreamTimeToGraphTime(streamTime);
  StreamTime thisStreamTime = GraphTimeToStreamTimeOptimistic(graphTime);
  TrackTicks ticks = TimeToTicksRoundDown(IdealAudioRate(), thisStreamTime);
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
                                      const AudioEventTimeline<ErrorResult>& aValue)
{
  class Message : public ControlMessage {
  public:
    Message(AudioNodeStream* aStream, uint32_t aIndex,
            const AudioEventTimeline<ErrorResult>& aValue)
      : ControlMessage(aStream), mValue(aValue), mIndex(aIndex) {}
    virtual void Run()
    {
      static_cast<AudioNodeStream*>(mStream)->Engine()->
          SetTimelineParameter(mIndex, mValue);
    }
    AudioEventTimeline<ErrorResult> mValue;
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

StreamBuffer::Track*
AudioNodeStream::EnsureTrack()
{
  StreamBuffer::Track* track = mBuffer.FindTrack(AUDIO_NODE_STREAM_TRACK_ID);
  if (!track) {
    nsAutoPtr<MediaSegment> segment(new AudioSegment());
    for (uint32_t j = 0; j < mListeners.Length(); ++j) {
      MediaStreamListener* l = mListeners[j];
      l->NotifyQueuedTrackChanges(Graph(), AUDIO_NODE_STREAM_TRACK_ID, IdealAudioRate(), 0,
                                  MediaStreamListener::TRACK_EVENT_CREATED,
                                  *segment);
    }
    track = &mBuffer.AddTrack(AUDIO_NODE_STREAM_TRACK_ID, IdealAudioRate(), 0, segment.forget());
  }
  return track;
}

AudioChunk*
AudioNodeStream::ObtainInputBlock(AudioChunk* aTmpChunk)
{
  uint32_t inputCount = mInputs.Length();
  uint32_t outputChannelCount = 0;
  nsAutoTArray<AudioChunk*,250> inputChunks;
  for (uint32_t i = 0; i < inputCount; ++i) {
    MediaStream* s = mInputs[i]->GetSource();
    AudioNodeStream* a = static_cast<AudioNodeStream*>(s);
    MOZ_ASSERT(a == s->AsAudioNodeStream());
    if (a->IsFinishedOnGraphThread()) {
      continue;
    }
    AudioChunk* chunk = &a->mLastChunk;
    MOZ_ASSERT(chunk);
    if (chunk->IsNull()) {
      continue;
    }

    inputChunks.AppendElement(chunk);
    outputChannelCount =
      GetAudioChannelsSuperset(outputChannelCount, chunk->mChannelData.Length());
  }

  uint32_t inputChunkCount = inputChunks.Length();
  if (inputChunkCount == 0) {
    aTmpChunk->SetNull(WEBAUDIO_BLOCK_SIZE);
    return aTmpChunk;
  }

  if (inputChunkCount == 1) {
    return inputChunks[0];
  }

  AllocateAudioBlock(outputChannelCount, aTmpChunk);

  for (uint32_t i = 0; i < inputChunkCount; ++i) {
    AudioChunk* chunk = inputChunks[i];
    nsAutoTArray<const void*,GUESS_AUDIO_CHANNELS> channels;
    channels.AppendElements(chunk->mChannelData);
    if (channels.Length() < outputChannelCount) {
      AudioChannelsUpMix(&channels, outputChannelCount, nullptr);
      NS_ASSERTION(outputChannelCount == channels.Length(),
                   "We called GetAudioChannelsSuperset to avoid this");
    }

    for (uint32_t c = 0; c < channels.Length(); ++c) {
      const float* inputData = static_cast<const float*>(channels[c]);
      float* outputData = static_cast<float*>(const_cast<void*>(aTmpChunk->mChannelData[c]));
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

  return aTmpChunk;
}

// The MediaStreamGraph guarantees that this is actually one block, for
// AudioNodeStreams.
void
AudioNodeStream::ProduceOutput(GraphTime aFrom, GraphTime aTo)
{
  StreamBuffer::Track* track = EnsureTrack();

  AudioChunk outputChunk;
  AudioSegment* segment = track->Get<AudioSegment>();

  outputChunk.SetNull(0);

  if (mInCycle) {
    // XXX DelayNode not supported yet so just produce silence
    outputChunk.SetNull(WEBAUDIO_BLOCK_SIZE);
  } else {
    AudioChunk tmpChunk;
    AudioChunk* inputChunk = ObtainInputBlock(&tmpChunk);
    bool finished = false;
    mEngine->ProduceAudioBlock(this, *inputChunk, &outputChunk, &finished);
    if (finished) {
      FinishOutput();
    }
  }

  mLastChunk = outputChunk;
  if (mKind == MediaStreamGraph::EXTERNAL_STREAM) {
    segment->AppendAndConsumeChunk(&outputChunk);
  } else {
    segment->AppendNullData(outputChunk.GetDuration());
  }

  for (uint32_t j = 0; j < mListeners.Length(); ++j) {
    MediaStreamListener* l = mListeners[j];
    AudioChunk copyChunk = outputChunk;
    AudioSegment tmpSegment;
    tmpSegment.AppendAndConsumeChunk(&copyChunk);
    l->NotifyQueuedTrackChanges(Graph(), AUDIO_NODE_STREAM_TRACK_ID,
                                IdealAudioRate(), segment->GetDuration(), 0,
                                tmpSegment);
  }
}

TrackTicks
AudioNodeStream::GetCurrentPosition()
{
  return EnsureTrack()->Get<AudioSegment>()->GetDuration();
}

void
AudioNodeStream::FinishOutput()
{
  if (IsFinishedOnGraphThread()) {
    return;
  }

  StreamBuffer::Track* track = EnsureTrack();
  track->SetEnded();
  FinishOnGraphThread();

  for (uint32_t j = 0; j < mListeners.Length(); ++j) {
    MediaStreamListener* l = mListeners[j];
    AudioSegment emptySegment;
    l->NotifyQueuedTrackChanges(Graph(), AUDIO_NODE_STREAM_TRACK_ID,
                                IdealAudioRate(),
                                track->GetSegment()->GetDuration(),
                                MediaStreamListener::TRACK_EVENT_ENDED, emptySegment);
  }
}

}
