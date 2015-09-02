/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioNodeEngine.h"
#include "AudioNodeExternalInputStream.h"
#include "AudioChannelFormat.h"
#include "mozilla/dom/MediaStreamAudioSourceNode.h"

using namespace mozilla::dom;

namespace mozilla {

AudioNodeExternalInputStream::AudioNodeExternalInputStream(AudioNodeEngine* aEngine, TrackRate aSampleRate, uint32_t aContextId)
  : AudioNodeStream(aEngine, NO_STREAM_FLAGS, aSampleRate, aContextId)
{
  MOZ_COUNT_CTOR(AudioNodeExternalInputStream);
}

AudioNodeExternalInputStream::~AudioNodeExternalInputStream()
{
  MOZ_COUNT_DTOR(AudioNodeExternalInputStream);
}

/* static */ already_AddRefed<AudioNodeExternalInputStream>
AudioNodeExternalInputStream::Create(MediaStreamGraph* aGraph,
                                     AudioNodeEngine* aEngine)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aGraph->GraphRate() == aEngine->NodeMainThread()->Context()->SampleRate());

  nsRefPtr<AudioNodeExternalInputStream> stream =
    new AudioNodeExternalInputStream(aEngine, aGraph->GraphRate(),
                                     aEngine->NodeMainThread()->Context()->Id());
  aGraph->AddStream(stream);
  return stream.forget();
}

/**
 * Copies the data in aInput to aOffsetInBlock within aBlock.
 * aBlock must have been allocated with AllocateInputBlock and have a channel
 * count that's a superset of the channels in aInput.
 */
template <typename T>
static void
CopyChunkToBlock(AudioChunk& aInput, AudioChunk *aBlock,
                 uint32_t aOffsetInBlock)
{
  uint32_t blockChannels = aBlock->ChannelCount();
  nsAutoTArray<const T*,2> channels;
  if (aInput.IsNull()) {
    channels.SetLength(blockChannels);
    PodZero(channels.Elements(), blockChannels);
  } else {
    const nsTArray<const T*>& inputChannels = aInput.ChannelData<T>();
    channels.SetLength(inputChannels.Length());
    PodCopy(channels.Elements(), inputChannels.Elements(), channels.Length());
    if (channels.Length() != blockChannels) {
      // We only need to upmix here because aBlock's channel count has been
      // chosen to be a superset of the channel count of every chunk.
      AudioChannelsUpMix(&channels, blockChannels, static_cast<T*>(nullptr));
    }
  }

  for (uint32_t c = 0; c < blockChannels; ++c) {
    float* outputData = aBlock->ChannelFloatsForWrite(c) + aOffsetInBlock;
    if (channels[c]) {
      ConvertAudioSamplesWithScale(channels[c], outputData, aInput.GetDuration(), aInput.mVolume);
    } else {
      PodZero(outputData, aInput.GetDuration());
    }
  }
}

/**
 * Converts the data in aSegment to a single chunk aBlock. aSegment must have
 * duration WEBAUDIO_BLOCK_SIZE. aFallbackChannelCount is a superset of the
 * channels in every chunk of aSegment. aBlock must be float format or null.
 */
static void ConvertSegmentToAudioBlock(AudioSegment* aSegment,
                                       AudioChunk* aBlock,
                                       int32_t aFallbackChannelCount)
{
  NS_ASSERTION(aSegment->GetDuration() == WEBAUDIO_BLOCK_SIZE, "Bad segment duration");

  {
    AudioSegment::ChunkIterator ci(*aSegment);
    NS_ASSERTION(!ci.IsEnded(), "Should be at least one chunk!");
    if (ci->GetDuration() == WEBAUDIO_BLOCK_SIZE &&
        (ci->IsNull() || ci->mBufferFormat == AUDIO_FORMAT_FLOAT32)) {
      // Return this chunk directly to avoid copying data.
      *aBlock = *ci;
      return;
    }
  }

  AllocateAudioBlock(aFallbackChannelCount, aBlock);

  uint32_t duration = 0;
  for (AudioSegment::ChunkIterator ci(*aSegment); !ci.IsEnded(); ci.Next()) {
    switch (ci->mBufferFormat) {
      case AUDIO_FORMAT_S16: {
        CopyChunkToBlock<int16_t>(*ci, aBlock, duration);
        break;
      }
      case AUDIO_FORMAT_FLOAT32: {
        CopyChunkToBlock<float>(*ci, aBlock, duration);
        break;
      }
      case AUDIO_FORMAT_SILENCE:
        break;
    }
    duration += ci->GetDuration();
  }
}

void
AudioNodeExternalInputStream::ProcessInput(GraphTime aFrom, GraphTime aTo,
                                           uint32_t aFlags)
{
  // According to spec, number of outputs is always 1.
  MOZ_ASSERT(mLastChunks.Length() == 1);

  // GC stuff can result in our input stream being destroyed before this stream.
  // Handle that.
  if (!IsEnabled() || mInputs.IsEmpty() || mPassThrough) {
    mLastChunks[0].SetNull(WEBAUDIO_BLOCK_SIZE);
    AdvanceOutputSegment();
    return;
  }

  MOZ_ASSERT(mInputs.Length() == 1);

  MediaStream* source = mInputs[0]->GetSource();
  nsAutoTArray<AudioSegment,1> audioSegments;
  uint32_t inputChannels = 0;
  for (StreamBuffer::TrackIter tracks(source->mBuffer, MediaSegment::AUDIO);
       !tracks.IsEnded(); tracks.Next()) {
    const StreamBuffer::Track& inputTrack = *tracks;
    const AudioSegment& inputSegment =
        *static_cast<AudioSegment*>(inputTrack.GetSegment());
    if (inputSegment.IsNull()) {
      continue;
    }

    AudioSegment& segment = *audioSegments.AppendElement();
    GraphTime next;
    for (GraphTime t = aFrom; t < aTo; t = next) {
      MediaInputPort::InputInterval interval = mInputs[0]->GetNextInputInterval(t);
      interval.mEnd = std::min(interval.mEnd, aTo);
      if (interval.mStart >= interval.mEnd)
        break;
      next = interval.mEnd;

      StreamTime outputStart = GraphTimeToStreamTime(interval.mStart);
      StreamTime outputEnd = GraphTimeToStreamTime(interval.mEnd);
      StreamTime ticks = outputEnd - outputStart;

      if (interval.mInputIsBlocked) {
        segment.AppendNullData(ticks);
      } else {
        StreamTime inputStart =
          std::min(inputSegment.GetDuration(),
                   source->GraphTimeToStreamTime(interval.mStart));
        StreamTime inputEnd =
          std::min(inputSegment.GetDuration(),
                   source->GraphTimeToStreamTime(interval.mEnd));

        segment.AppendSlice(inputSegment, inputStart, inputEnd);
        // Pad if we're looking past the end of the track
        segment.AppendNullData(ticks - (inputEnd - inputStart));
      }
    }

    for (AudioSegment::ChunkIterator iter(segment); !iter.IsEnded(); iter.Next()) {
      inputChannels = GetAudioChannelsSuperset(inputChannels, iter->ChannelCount());
    }
  }

  uint32_t accumulateIndex = 0;
  if (inputChannels) {
    nsAutoTArray<float,GUESS_AUDIO_CHANNELS*WEBAUDIO_BLOCK_SIZE> downmixBuffer;
    for (uint32_t i = 0; i < audioSegments.Length(); ++i) {
      AudioChunk tmpChunk;
      ConvertSegmentToAudioBlock(&audioSegments[i], &tmpChunk, inputChannels);
      if (!tmpChunk.IsNull()) {
        if (accumulateIndex == 0) {
          AllocateAudioBlock(inputChannels, &mLastChunks[0]);
        }
        AccumulateInputChunk(accumulateIndex, tmpChunk, &mLastChunks[0], &downmixBuffer);
        accumulateIndex++;
      }
    }
  }
  if (accumulateIndex == 0) {
    mLastChunks[0].SetNull(WEBAUDIO_BLOCK_SIZE);
  }

  // Using AudioNodeStream's AdvanceOutputSegment to push the media stream graph along with null data.
  AdvanceOutputSegment();
}

bool
AudioNodeExternalInputStream::IsEnabled()
{
  return ((MediaStreamAudioSourceNodeEngine*)Engine())->IsEnabled();
}

} // namespace mozilla
