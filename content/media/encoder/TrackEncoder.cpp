/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "TrackEncoder.h"
#include "MediaStreamGraph.h"
#include "AudioChannelFormat.h"

#undef LOG
#ifdef MOZ_WIDGET_GONK
#include <android/log.h>
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "MediakEncoder", ## args);
#else
#define LOG(args, ...)
#endif

namespace mozilla {

static const int  DEFAULT_CHANNELS = 1;
static const int  DEFAULT_SAMPLING_RATE = 16000;

void
AudioTrackEncoder::NotifyQueuedTrackChanges(MediaStreamGraph* aGraph,
                                            TrackID aID,
                                            TrackRate aTrackRate,
                                            TrackTicks aTrackOffset,
                                            uint32_t aTrackEvents,
                                            const MediaSegment& aQueuedMedia)
{
  if (mCanceled) {
    return;
  }

  AudioSegment* audio = const_cast<AudioSegment*>
                        (static_cast<const AudioSegment*>(&aQueuedMedia));

  // Check and initialize parameters for codec encoder.
  if (!mInitialized) {
    AudioSegment::ChunkIterator iter(*audio);
    while (!iter.IsEnded()) {
      AudioChunk chunk = *iter;

      // The number of channels is determined by the first non-null chunk, and
      // thus the audio encoder is initialized at this time.
      if (!chunk.IsNull()) {
        nsresult rv = Init(chunk.mChannelData.Length(), aTrackRate);
        if (NS_FAILED(rv)) {
          LOG("[AudioTrackEncoder]: Fail to initialize the encoder!");
          NotifyCancel();
        }
        break;
      } else {
        mSilentDuration += chunk.mDuration;
      }
      iter.Next();
    }
  }

  // Append and consume this raw segment.
  if (mInitialized) {
    AppendAudioSegment(audio);
  }

  // The stream has stopped and reached the end of track.
  if (aTrackEvents == MediaStreamListener::TRACK_EVENT_ENDED) {
    LOG("[AudioTrackEncoder]: Receive TRACK_EVENT_ENDED .");
    NotifyEndOfStream();
  }
}

void
AudioTrackEncoder::NotifyRemoved(MediaStreamGraph* aGraph)
{
  // In case that MediaEncoder does not receive a TRACK_EVENT_ENDED event.
  LOG("[AudioTrackEncoder]: NotifyRemoved.");
  NotifyEndOfStream();
}

void
AudioTrackEncoder::NotifyEndOfStream()
{
  // If source audio chunks are completely silent till the end of encoding,
  // initialize the encoder with default channel counts and sampling rate, and
  // append this many null data to the segment of track encoder.
  if (!mCanceled && !mInitialized && mSilentDuration > 0) {
    Init(DEFAULT_CHANNELS, DEFAULT_SAMPLING_RATE);
    mRawSegment->AppendNullData(mSilentDuration);
    mSilentDuration = 0;
  }

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  mEndOfStream = true;
  mReentrantMonitor.NotifyAll();
}

nsresult
AudioTrackEncoder::AppendAudioSegment(MediaSegment* aSegment)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  AudioSegment* audio = static_cast<AudioSegment*>(aSegment);
  AudioSegment::ChunkIterator iter(*audio);

  // Append this many null data to our queued segment if there is a complete
  // silence before the audio track encoder has initialized.
  if (mSilentDuration > 0) {
    mRawSegment->AppendNullData(mSilentDuration);
    mSilentDuration = 0;
  }

  while (!iter.IsEnded()) {
    AudioChunk chunk = *iter;
    // Append and consume both non-null and null chunks.
    mRawSegment->AppendAndConsumeChunk(&chunk);
    iter.Next();
  }
  if (mRawSegment->GetDuration() >= GetPacketDuration()) {
    mReentrantMonitor.NotifyAll();
  }

  return NS_OK;
}

static const int AUDIO_PROCESSING_FRAMES = 640; /* > 10ms of 48KHz audio */
static const uint8_t gZeroChannel[MAX_AUDIO_SAMPLE_SIZE*AUDIO_PROCESSING_FRAMES] = {0};

void
AudioTrackEncoder::InterleaveTrackData(AudioChunk& aChunk,
                                       int32_t aDuration,
                                       uint32_t aOutputChannels,
                                       AudioDataValue* aOutput)
{
  if (aChunk.mChannelData.Length() < aOutputChannels) {
    // Up-mix. This might make the mChannelData have more than aChannels.
    AudioChannelsUpMix(&aChunk.mChannelData, aOutputChannels, gZeroChannel);
  }

  if (aChunk.mChannelData.Length() > aOutputChannels) {
    DownmixAndInterleave(aChunk.mChannelData, aChunk.mBufferFormat, aDuration,
                         aChunk.mVolume, mChannels, aOutput);
  } else {
    InterleaveAndConvertBuffer(aChunk.mChannelData.Elements(),
                               aChunk.mBufferFormat, aDuration, aChunk.mVolume,
                               mChannels, aOutput);
  }
}

}
