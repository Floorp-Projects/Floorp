/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AlignedTArray.h"
#include "AlignmentUtils.h"
#include "AudioNodeEngine.h"
#include "AudioNodeExternalInputStream.h"
#include "AudioChannelFormat.h"
#include "mozilla/dom/MediaStreamAudioSourceNode.h"

using namespace mozilla::dom;

namespace mozilla {

AudioNodeExternalInputStream::AudioNodeExternalInputStream(AudioNodeEngine* aEngine, TrackRate aSampleRate)
  : AudioNodeStream(aEngine, NO_STREAM_FLAGS, aSampleRate)
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
  AudioContext* ctx = aEngine->NodeMainThread()->Context();
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aGraph->GraphRate() == ctx->SampleRate());

  RefPtr<AudioNodeExternalInputStream> stream =
    new AudioNodeExternalInputStream(aEngine, aGraph->GraphRate());
  stream->mSuspendedCount += ctx->ShouldSuspendNewStream();
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
CopyChunkToBlock(AudioChunk& aInput, AudioBlock *aBlock,
                 uint32_t aOffsetInBlock)
{
  uint32_t blockChannels = aBlock->ChannelCount();
  AutoTArray<const T*,2> channels;
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
                                       AudioBlock* aBlock,
                                       int32_t aFallbackChannelCount)
{
  NS_ASSERTION(aSegment->GetDuration() == WEBAUDIO_BLOCK_SIZE, "Bad segment duration");

  {
    AudioSegment::ChunkIterator ci(*aSegment);
    NS_ASSERTION(!ci.IsEnded(), "Should be at least one chunk!");
    if (ci->GetDuration() == WEBAUDIO_BLOCK_SIZE &&
        (ci->IsNull() || ci->mBufferFormat == AUDIO_FORMAT_FLOAT32)) {

      bool aligned = true;
      for (size_t i = 0; i < ci->mChannelData.Length(); ++i) {
        if (!IS_ALIGNED16(ci->mChannelData[i])) {
            aligned = false;
            break;
        }
      }

      // Return this chunk directly to avoid copying data.
      if (aligned) {
        *aBlock = *ci;
        return;
      }
    }
  }

  aBlock->AllocateChannels(aFallbackChannelCount);

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
      case AUDIO_FORMAT_SILENCE: {
        // The actual type of the sample does not matter here, but we still need
        // to send some audio to the graph.
        CopyChunkToBlock<float>(*ci, aBlock, duration);
        break;
      }
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
    return;
  }

  MOZ_ASSERT(mInputs.Length() == 1);

  MediaStream* source = mInputs[0]->GetSource();
  AutoTArray<AudioSegment,1> audioSegments;
  uint32_t inputChannels = 0;
  for (StreamTracks::TrackIter tracks(source->mTracks);
       !tracks.IsEnded(); tracks.Next()) {
    const StreamTracks::Track& inputTrack = *tracks;
    if (!mInputs[0]->PassTrackThrough(tracks->GetID())) {
      continue;
    }

    if (inputTrack.GetSegment()->GetType() == MediaSegment::VIDEO) {
      MOZ_ASSERT(false, "AudioNodeExternalInputStream shouldn't have video tracks");
      continue;
    }

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

      // We know this stream does not block during the processing interval ---
      // we're not finished, we don't underrun, and we're not suspended.
      StreamTime outputStart = GraphTimeToStreamTime(interval.mStart);
      StreamTime outputEnd = GraphTimeToStreamTime(interval.mEnd);
      StreamTime ticks = outputEnd - outputStart;

      if (interval.mInputIsBlocked) {
        segment.AppendNullData(ticks);
      } else {
        // The input stream is not blocked in this interval, so no need to call
        // GraphTimeToStreamTimeWithBlocking.
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
    DownmixBufferType downmixBuffer;
    ASSERT_ALIGNED16(downmixBuffer.Elements());
    for (uint32_t i = 0; i < audioSegments.Length(); ++i) {
      AudioBlock tmpChunk;
      ConvertSegmentToAudioBlock(&audioSegments[i], &tmpChunk, inputChannels);
      if (!tmpChunk.IsNull()) {
        if (accumulateIndex == 0) {
          mLastChunks[0].AllocateChannels(inputChannels);
        }
        AccumulateInputChunk(accumulateIndex, tmpChunk, &mLastChunks[0], &downmixBuffer);
        accumulateIndex++;
      }
    }
  }
  if (accumulateIndex == 0) {
    mLastChunks[0].SetNull(WEBAUDIO_BLOCK_SIZE);
  }
}

bool
AudioNodeExternalInputStream::IsEnabled()
{
  return ((MediaStreamAudioSourceNodeEngine*)Engine())->IsEnabled();
}

} // namespace mozilla
