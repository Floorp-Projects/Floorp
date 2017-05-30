/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "TrackEncoder.h"
#include "AudioChannelFormat.h"
#include "MediaStreamGraph.h"
#include "MediaStreamListener.h"
#include "mozilla/Logging.h"
#include "VideoUtils.h"
#include "mozilla/Logging.h"

namespace mozilla {

LazyLogModule gTrackEncoderLog("TrackEncoder");
#define TRACK_LOG(type, msg) MOZ_LOG(gTrackEncoderLog, type, msg)

static const int DEFAULT_CHANNELS = 1;
static const int DEFAULT_SAMPLING_RATE = 16000;
static const int DEFAULT_FRAME_WIDTH = 640;
static const int DEFAULT_FRAME_HEIGHT = 480;
static const int DEFAULT_TRACK_RATE = USECS_PER_S;
// 30 seconds threshold if the encoder still can't not be initialized.
static const int INIT_FAILED_DURATION = 30;

TrackEncoder::TrackEncoder()
  : mReentrantMonitor("media.TrackEncoder")
  , mEncodingComplete(false)
  , mEosSetInEncoder(false)
  , mInitialized(false)
  , mEndOfStream(false)
  , mCanceled(false)
  , mInitCounter(0)
  , mNotInitDuration(0)
{
}

void TrackEncoder::NotifyEvent(MediaStreamGraph* aGraph,
                 MediaStreamGraphEvent event)
{
  if (event == MediaStreamGraphEvent::EVENT_REMOVED) {
    NotifyEndOfStream();
  }
}

void
AudioTrackEncoder::NotifyQueuedTrackChanges(MediaStreamGraph* aGraph,
                                            TrackID aID,
                                            StreamTime aTrackOffset,
                                            uint32_t aTrackEvents,
                                            const MediaSegment& aQueuedMedia)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (mCanceled) {
    return;
  }

  const AudioSegment& audio = static_cast<const AudioSegment&>(aQueuedMedia);

  // Check and initialize parameters for codec encoder.
  if (!mInitialized) {
    mInitCounter++;
    TRACK_LOG(LogLevel::Debug, ("Init the audio encoder %d times", mInitCounter));
    AudioSegment::ChunkIterator iter(const_cast<AudioSegment&>(audio));
    while (!iter.IsEnded()) {
      AudioChunk chunk = *iter;

      // The number of channels is determined by the first non-null chunk, and
      // thus the audio encoder is initialized at this time.
      if (!chunk.IsNull()) {
        nsresult rv = Init(chunk.mChannelData.Length(), aGraph->GraphRate());
        if (NS_FAILED(rv)) {
          TRACK_LOG(LogLevel::Error, ("[AudioTrackEncoder]: Fail to initialize the encoder!"));
          NotifyCancel();
        }
        break;
      }

      iter.Next();
    }

    mNotInitDuration += aQueuedMedia.GetDuration();
    if (!mInitialized &&
        (mNotInitDuration / aGraph->GraphRate() > INIT_FAILED_DURATION) &&
        mInitCounter > 1) {
      TRACK_LOG(LogLevel::Warning, ("[AudioTrackEncoder]: Initialize failed for 30s."));
      NotifyEndOfStream();
      return;
    }
  }

  // Append and consume this raw segment.
  AppendAudioSegment(audio);


  // The stream has stopped and reached the end of track.
  if (aTrackEvents == TrackEventCommand::TRACK_EVENT_ENDED) {
    TRACK_LOG(LogLevel::Info, ("[AudioTrackEncoder]: Receive TRACK_EVENT_ENDED ."));
    NotifyEndOfStream();
  }
}

void
AudioTrackEncoder::NotifyEndOfStream()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  // If source audio track is completely silent till the end of encoding,
  // initialize the encoder with default channel counts and sampling rate.
  if (!mCanceled && !mInitialized) {
    Init(DEFAULT_CHANNELS, DEFAULT_SAMPLING_RATE);
  }

  mEndOfStream = true;
  mReentrantMonitor.NotifyAll();
}

nsresult
AudioTrackEncoder::AppendAudioSegment(const AudioSegment& aSegment)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  AudioSegment::ChunkIterator iter(const_cast<AudioSegment&>(aSegment));
  while (!iter.IsEnded()) {
    AudioChunk chunk = *iter;
    // Append and consume both non-null and null chunks.
    mRawSegment.AppendAndConsumeChunk(&chunk);
    iter.Next();
  }

  if (mRawSegment.GetDuration() >= GetPacketDuration()) {
    mReentrantMonitor.NotifyAll();
  }

  return NS_OK;
}

/*static*/
void
AudioTrackEncoder::InterleaveTrackData(AudioChunk& aChunk,
                                       int32_t aDuration,
                                       uint32_t aOutputChannels,
                                       AudioDataValue* aOutput)
{
  uint32_t numChannelsToCopy = std::min(aOutputChannels,
                                        aChunk.mChannelData.Length());
  switch(aChunk.mBufferFormat) {
    case AUDIO_FORMAT_S16: {
      AutoTArray<const int16_t*, 2> array;
      array.SetLength(numChannelsToCopy);
      for (uint32_t i = 0; i < array.Length(); i++) {
        array[i] = static_cast<const int16_t*>(aChunk.mChannelData[i]);
      }
      InterleaveTrackData(array, aDuration, aOutputChannels, aOutput, aChunk.mVolume);
      break;
    }
    case AUDIO_FORMAT_FLOAT32: {
      AutoTArray<const float*, 2> array;
      array.SetLength(numChannelsToCopy);
      for (uint32_t i = 0; i < array.Length(); i++) {
        array[i] = static_cast<const float*>(aChunk.mChannelData[i]);
      }
      InterleaveTrackData(array, aDuration, aOutputChannels, aOutput, aChunk.mVolume);
      break;
   }
   case AUDIO_FORMAT_SILENCE: {
      MOZ_ASSERT(false, "To implement.");
    }
  };
}

/*static*/
void
AudioTrackEncoder::DeInterleaveTrackData(AudioDataValue* aInput,
                                         int32_t aDuration,
                                         int32_t aChannels,
                                         AudioDataValue* aOutput)
{
  for (int32_t i = 0; i < aChannels; ++i) {
    for(int32_t j = 0; j < aDuration; ++j) {
      aOutput[i * aDuration + j] = aInput[i + j * aChannels];
    }
  }
}

size_t
AudioTrackEncoder::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return mRawSegment.SizeOfExcludingThis(aMallocSizeOf);
}

void
VideoTrackEncoder::Init(const VideoSegment& aSegment)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (mInitialized) {
    return;
  }

  mInitCounter++;
  TRACK_LOG(LogLevel::Debug, ("Init the video encoder %d times", mInitCounter));
  VideoSegment::ConstChunkIterator iter(aSegment);
  while (!iter.IsEnded()) {
   VideoChunk chunk = *iter;
   if (!chunk.IsNull()) {
     gfx::IntSize imgsize = chunk.mFrame.GetImage()->GetSize();
     gfx::IntSize intrinsicSize = chunk.mFrame.GetIntrinsicSize();
     nsresult rv = Init(imgsize.width, imgsize.height,
                        intrinsicSize.width, intrinsicSize.height);

     if (NS_FAILED(rv)) {
       TRACK_LOG(LogLevel::Error, ("[VideoTrackEncoder]: Fail to initialize the encoder!"));
       NotifyCancel();
     }
     break;
   }

   iter.Next();
  }

  mNotInitDuration += aSegment.GetDuration();
  if ((mNotInitDuration / mTrackRate > INIT_FAILED_DURATION) &&
      mInitCounter > 1) {
    TRACK_LOG(LogLevel::Debug, ("[VideoTrackEncoder]: Initialize failed for %ds.", INIT_FAILED_DURATION));
    NotifyEndOfStream();
    return;
  }
}

void
VideoTrackEncoder::SetCurrentFrames(const VideoSegment& aSegment)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (mCanceled) {
    return;
  }

  Init(aSegment);
  AppendVideoSegment(aSegment);
}

void
VideoTrackEncoder::NotifyQueuedTrackChanges(MediaStreamGraph* aGraph,
                                            TrackID aID,
                                            StreamTime aTrackOffset,
                                            uint32_t aTrackEvents,
                                            const MediaSegment& aQueuedMedia)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (mCanceled) {
    return;
  }

  if (!(aTrackEvents == TRACK_EVENT_CREATED ||
       aTrackEvents == TRACK_EVENT_ENDED)) {
    return;
  }

  const VideoSegment& video = static_cast<const VideoSegment&>(aQueuedMedia);

   // Check and initialize parameters for codec encoder.
  Init(video);

  AppendVideoSegment(video);

  // The stream has stopped and reached the end of track.
  if (aTrackEvents == TrackEventCommand::TRACK_EVENT_ENDED) {
    TRACK_LOG(LogLevel::Info, ("[VideoTrackEncoder]: Receive TRACK_EVENT_ENDED ."));
    NotifyEndOfStream();
  }

}

nsresult
VideoTrackEncoder::AppendVideoSegment(const VideoSegment& aSegment)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (mEndOfStream) {
    MOZ_ASSERT(false);
    return NS_OK;
  }

  // Append all video segments from MediaStreamGraph, including null an
  // non-null frames.
  VideoSegment::ConstChunkIterator iter(aSegment);
  for (; !iter.IsEnded(); iter.Next()) {
    VideoChunk chunk = *iter;

    if (mLastChunk.mTimeStamp.IsNull()) {
      if (chunk.IsNull()) {
        // The start of this track is frameless. We need to track the time
        // it takes to get the first frame.
        mLastChunk.mDuration += chunk.mDuration;
        continue;
      }

      // This is the first real chunk in the track. Use its timestamp as the
      // starting point for this track.
      MOZ_ASSERT(!chunk.mTimeStamp.IsNull());
      const StreamTime nullDuration = mLastChunk.mDuration;
      mLastChunk = chunk;
      chunk.mDuration = 0;

      TRACK_LOG(LogLevel::Verbose,
                ("[VideoTrackEncoder]: Got first video chunk after %" PRId64 " ticks.",
                 nullDuration));
      // Adapt to the time before the first frame. This extends the first frame
      // from [start, end] to [0, end], but it'll do for now.
      auto diff = FramesToTimeUnit(nullDuration, mTrackRate);
      if (!diff.IsValid()) {
        NS_ERROR("null duration overflow");
        return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
      }

      mLastChunk.mTimeStamp -= diff.ToTimeDuration();
      mLastChunk.mDuration += nullDuration;
    }

    MOZ_ASSERT(!mLastChunk.IsNull());
    if (mLastChunk.CanCombineWithFollowing(chunk) || chunk.IsNull()) {
      TRACK_LOG(LogLevel::Verbose,
                ("[VideoTrackEncoder]: Got dupe or null chunk."));
      // This is the same frame as before (or null). We extend the last chunk
      // with its duration.
      mLastChunk.mDuration += chunk.mDuration;

      if (mLastChunk.mDuration < mTrackRate) {
        TRACK_LOG(LogLevel::Verbose,
                  ("[VideoTrackEncoder]: Ignoring dupe/null chunk of duration %" PRId64,
                   chunk.mDuration));
        continue;
      }

      TRACK_LOG(LogLevel::Verbose,
                ("[VideoTrackEncoder]: Chunk >1 second. duration=%" PRId64 ", "
                 "trackRate=%" PRId32, mLastChunk.mDuration, mTrackRate));

      // If we have gotten dupes for over a second, we force send one
      // to the encoder to make sure there is some output.
      chunk.mTimeStamp = mLastChunk.mTimeStamp + TimeDuration::FromSeconds(1);

      // chunk's duration has already been accounted for.
      chunk.mDuration = 0;

      if (chunk.IsNull()) {
        // Ensure that we don't pass null to the encoder by making mLastChunk
        // null later on.
        chunk.mFrame = mLastChunk.mFrame;
      }
    }

    if (mStartOffset.IsNull()) {
      mStartOffset = mLastChunk.mTimeStamp;
    }

    TimeDuration relativeTime = chunk.mTimeStamp - mStartOffset;
    RefPtr<layers::Image> lastImage = mLastChunk.mFrame.GetImage();
    TRACK_LOG(LogLevel::Verbose,
              ("[VideoTrackEncoder]: Appending video frame %p, at pos %.5fs",
               lastImage.get(), relativeTime.ToSeconds()));
    CheckedInt64 totalDuration =
      UsecsToFrames(relativeTime.ToMicroseconds(), mTrackRate);
    if (!totalDuration.isValid()) {
      NS_ERROR("Duration overflow");
      return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
    }

    CheckedInt64 duration = totalDuration - mEncodedTicks;
    if (!duration.isValid()) {
      NS_ERROR("Duration overflow");
      return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
    }

    if (duration.isValid()) {
      if (duration.value() <= 0) {
        // The timestamp for mLastChunk is newer than for chunk.
        // This means the durations reported from MediaStreamGraph for
        // mLastChunk were larger than the timestamp diff - and durations were
        // used to trigger the 1-second frame above. This could happen due to
        // drift or underruns in the graph.
        TRACK_LOG(LogLevel::Warning,
                  ("[VideoTrackEncoder]: Underrun detected. Diff=%" PRId64,
                   duration.value()));
        chunk.mTimeStamp = mLastChunk.mTimeStamp;
      } else {
        mEncodedTicks += duration.value();
        mRawSegment.AppendFrame(lastImage.forget(),
                                duration.value(),
                                mLastChunk.mFrame.GetIntrinsicSize(),
                                PRINCIPAL_HANDLE_NONE,
                                mLastChunk.mFrame.GetForceBlack(),
                                mLastChunk.mTimeStamp);
      }
    }

    mLastChunk = chunk;
  }

  if (mRawSegment.GetDuration() > 0) {
    mReentrantMonitor.NotifyAll();
  }

  return NS_OK;
}

void
VideoTrackEncoder::NotifyEndOfStream()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  // If source video track is muted till the end of encoding, initialize the
  // encoder with default frame width, frame height, and track rate.
  if (!mCanceled && !mInitialized) {
    Init(DEFAULT_FRAME_WIDTH, DEFAULT_FRAME_HEIGHT,
         DEFAULT_FRAME_WIDTH, DEFAULT_FRAME_HEIGHT);
  }

  if (mEndOfStream) {
    // We have already been notified.
    return;
  }

  mEndOfStream = true;
  TRACK_LOG(LogLevel::Info, ("[VideoTrackEncoder]: Reached end of stream"));

  if (!mLastChunk.IsNull() && mLastChunk.mDuration > 0) {
    RefPtr<layers::Image> lastImage = mLastChunk.mFrame.GetImage();
    TRACK_LOG(LogLevel::Debug,
              ("[VideoTrackEncoder]: Appending last video frame %p, "
               "duration=%.5f", lastImage.get(),
               FramesToTimeUnit(mLastChunk.mDuration, mTrackRate).ToSeconds()));
    mRawSegment.AppendFrame(lastImage.forget(),
                            mLastChunk.mDuration,
                            mLastChunk.mFrame.GetIntrinsicSize(),
                            PRINCIPAL_HANDLE_NONE,
                            mLastChunk.mFrame.GetForceBlack(),
                            mLastChunk.mTimeStamp);
  }
  mReentrantMonitor.NotifyAll();
}

size_t
VideoTrackEncoder::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return mRawSegment.SizeOfExcludingThis(aMallocSizeOf);
}

} // namespace mozilla
