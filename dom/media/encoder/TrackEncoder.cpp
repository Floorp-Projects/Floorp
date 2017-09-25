/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TrackEncoder.h"

#include "AudioChannelFormat.h"
#include "GeckoProfiler.h"
#include "MediaStreamGraph.h"
#include "MediaStreamListener.h"
#include "mozilla/AbstractThread.h"
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
// 1 second threshold if the audio encoder cannot be initialized.
static const int AUDIO_INIT_FAILED_DURATION = 1;
// 30 second threshold if the video encoder cannot be initialized.
static const int VIDEO_INIT_FAILED_DURATION = 30;

TrackEncoder::TrackEncoder(TrackRate aTrackRate)
  : mEncodingComplete(false)
  , mEosSetInEncoder(false)
  , mInitialized(false)
  , mEndOfStream(false)
  , mCanceled(false)
  , mCurrentTime(0)
  , mInitCounter(0)
  , mNotInitDuration(0)
  , mSuspended(false)
  , mTrackRate(aTrackRate)
{
}

bool
TrackEncoder::IsInitialized()
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  return mInitialized;
}

bool
TrackEncoder::IsEncodingComplete()
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  return mEncodingComplete;
}

void
TrackEncoder::SetInitialized()
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());

  if (mInitialized) {
    return;
  }

  mInitialized = true;

  auto listeners(mListeners);
  for (auto& l : listeners) {
    l->Initialized(this);
  }
}

void
TrackEncoder::OnDataAvailable()
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());

  auto listeners(mListeners);
  for (auto& l : listeners) {
    l->DataAvailable(this);
  }
}

void
TrackEncoder::OnError()
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());

  Cancel();

  auto listeners(mListeners);
  for (auto& l : listeners) {
    l->Error(this);
  }
}

void
TrackEncoder::RegisterListener(TrackEncoderListener* aListener)
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mListeners.Contains(aListener));
  mListeners.AppendElement(aListener);
}

bool
TrackEncoder::UnregisterListener(TrackEncoderListener* aListener)
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  return mListeners.RemoveElement(aListener);
}

void
TrackEncoder::SetWorkerThread(AbstractThread* aWorkerThread)
{
  mWorkerThread = aWorkerThread;
}

void
AudioTrackEncoder::Suspend(TimeStamp)
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Info,
    ("[AudioTrackEncoder %p]: Suspend(), was %s",
     this, mSuspended ? "suspended" : "live"));

  if (mSuspended) {
    return;
  }

  mSuspended = true;
}

void
AudioTrackEncoder::Resume(TimeStamp)
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Info,
    ("[AudioTrackEncoder %p]: Resume(), was %s",
     this, mSuspended ? "suspended" : "live"));

  if (!mSuspended) {
    return;
  }

  mSuspended = false;
}

void
AudioTrackEncoder::AppendAudioSegment(AudioSegment&& aSegment)
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Verbose,
    ("[AudioTrackEncoder %p]: AppendAudioSegment() duration=%" PRIu64,
     this, aSegment.GetDuration()));

  if (mCanceled) {
    return;
  }

  if (mEndOfStream) {
    return;
  }

  mIncomingBuffer.AppendFrom(&aSegment);
}

void
AudioTrackEncoder::TakeTrackData(AudioSegment& aSegment)
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());

  if (mCanceled) {
    return;
  }

  aSegment.AppendFrom(&mOutgoingBuffer);
}

void
AudioTrackEncoder::TryInit(const AudioSegment& aSegment, StreamTime aDuration)
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());

  if (mInitialized) {
    return;
  }

  mInitCounter++;
  TRACK_LOG(LogLevel::Debug,
    ("[AudioTrackEncoder %p]: Inited the audio encoder %d times",
     this, mInitCounter));

  for (AudioSegment::ConstChunkIterator iter(aSegment); !iter.IsEnded(); iter.Next()) {
    // The number of channels is determined by the first non-null chunk, and
    // thus the audio encoder is initialized at this time.
    if (iter->IsNull()) {
      continue;
    }

    nsresult rv = Init(iter->mChannelData.Length(), mTrackRate);

    if (NS_SUCCEEDED(rv)) {
      TRACK_LOG(LogLevel::Info,
        ("[AudioTrackEncoder %p]: Successfully initialized!", this));
      return;
    } else {
      TRACK_LOG(LogLevel::Error,
        ("[AudioTrackEncoder %p]: Failed to initialize the encoder!", this));
      OnError();
      return;
    }
    break;
  }

  mNotInitDuration += aDuration;
  if (!mInitialized &&
      (mNotInitDuration / mTrackRate > AUDIO_INIT_FAILED_DURATION) &&
      mInitCounter > 1) {
    // Perform a best effort initialization since we haven't gotten any
    // data yet. Motivated by issues like Bug 1336367
    TRACK_LOG(LogLevel::Warning,
              ("[AudioTrackEncoder]: Initialize failed "
               "for %ds. Attempting to init with %d "
               "(default) channels!",
               AUDIO_INIT_FAILED_DURATION,
               DEFAULT_CHANNELS));
    nsresult rv = Init(DEFAULT_CHANNELS, mTrackRate);
    Telemetry::Accumulate(
      Telemetry::MEDIA_RECORDER_TRACK_ENCODER_INIT_TIMEOUT_TYPE, 0);
    if (NS_FAILED(rv)) {
      TRACK_LOG(LogLevel::Error,
                ("[AudioTrackEncoder %p]: Default-channel-init failed.", this));
      OnError();
      return;
    }
  }
}

void
AudioTrackEncoder::Cancel()
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Info,
    ("[AudioTrackEncoder %p]: Cancel(), currentTime=%" PRIu64,
     this, mCurrentTime));
  mCanceled = true;
  mIncomingBuffer.Clear();
  mOutgoingBuffer.Clear();
}

void
AudioTrackEncoder::NotifyEndOfStream()
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Info,
    ("[AudioTrackEncoder %p]: NotifyEndOfStream(), currentTime=%" PRIu64,
     this, mCurrentTime));

  if (!mCanceled && !mInitialized) {
    // If source audio track is completely silent till the end of encoding,
    // initialize the encoder with default channel counts and sampling rate.
    Init(DEFAULT_CHANNELS, DEFAULT_SAMPLING_RATE);
  }

  mEndOfStream = true;

  mIncomingBuffer.Clear();

  if (mInitialized && !mCanceled) {
    OnDataAvailable();
  }
}

void
AudioTrackEncoder::SetStartOffset(StreamTime aStartOffset)
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  MOZ_ASSERT(mCurrentTime == 0);
  TRACK_LOG(LogLevel::Info,
    ("[AudioTrackEncoder %p]: SetStartOffset(), aStartOffset=%" PRIu64,
     this, aStartOffset));
  mIncomingBuffer.InsertNullDataAtStart(aStartOffset);
  mCurrentTime = aStartOffset;
}

void
AudioTrackEncoder::AdvanceBlockedInput(StreamTime aDuration)
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Verbose,
    ("[AudioTrackEncoder %p]: AdvanceBlockedInput(), aDuration=%" PRIu64,
     this, aDuration));

  // We call Init here so it can account for aDuration towards the Init timeout
  TryInit(mOutgoingBuffer, aDuration);

  mIncomingBuffer.InsertNullDataAtStart(aDuration);
  mCurrentTime += aDuration;
}

void
AudioTrackEncoder::AdvanceCurrentTime(StreamTime aDuration)
{
  AUTO_PROFILER_LABEL("AudioTrackEncoder::AdvanceCurrentTime", OTHER);

  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());

  if (mCanceled) {
    return;
  }

  if (mEndOfStream) {
    return;
  }

  TRACK_LOG(LogLevel::Verbose,
    ("[AudioTrackEncoder %p]: AdvanceCurrentTime() %" PRIu64,
     this, aDuration));

  StreamTime currentTime = mCurrentTime + aDuration;

  if (mSuspended) {
    mCurrentTime = currentTime;
    mIncomingBuffer.ForgetUpTo(mCurrentTime);
    return;
  }

  if (currentTime <= mIncomingBuffer.GetDuration()) {
    mOutgoingBuffer.AppendSlice(mIncomingBuffer, mCurrentTime, currentTime);

    TryInit(mOutgoingBuffer, aDuration);
    if (mInitialized && mOutgoingBuffer.GetDuration() >= GetPacketDuration()) {
      OnDataAvailable();
    }
  } else {
    NS_ASSERTION(false, "AudioTrackEncoder::AdvanceCurrentTime Not enough data");
    TRACK_LOG(LogLevel::Error,
      ("[AudioTrackEncoder %p]: AdvanceCurrentTime() Not enough data. "
       "In incoming=%" PRIu64 ", aDuration=%" PRIu64 ", currentTime=%" PRIu64,
       this, mIncomingBuffer.GetDuration(), aDuration, currentTime));
    OnError();
  }

  mCurrentTime = currentTime;
  mIncomingBuffer.ForgetUpTo(mCurrentTime);
}

/*static*/
void
AudioTrackEncoder::InterleaveTrackData(AudioChunk& aChunk,
                                       int32_t aDuration,
                                       uint32_t aOutputChannels,
                                       AudioDataValue* aOutput)
{
  uint32_t numChannelsToCopy = std::min(aOutputChannels,
                                        static_cast<uint32_t>(aChunk.mChannelData.Length()));
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
AudioTrackEncoder::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  return mIncomingBuffer.SizeOfExcludingThis(aMallocSizeOf) +
         mOutgoingBuffer.SizeOfExcludingThis(aMallocSizeOf);
}

void
VideoTrackEncoder::Suspend(TimeStamp aTime)
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Info,
    ("[VideoTrackEncoder %p]: Suspend(), was %s",
     this, mSuspended ? "suspended" : "live"));

  if (mSuspended) {
    return;
  }

  mSuspended = true;
  mSuspendTime = aTime;
}

void
VideoTrackEncoder::Resume(TimeStamp aTime)
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Info,
    ("[VideoTrackEncoder %p]: Resume(), was %s",
     this, mSuspended ? "suspended" : "live"));

  if (!mSuspended) {
    return;
  }

  mSuspended = false;

  TimeDuration suspendDuration = aTime - mSuspendTime;
  if (!mLastChunk.mTimeStamp.IsNull()) {
    VideoChunk* nextChunk = mIncomingBuffer.FindChunkContaining(mCurrentTime);
    if (nextChunk && nextChunk->mTimeStamp < aTime) {
      nextChunk->mTimeStamp = aTime;
    }
    mLastChunk.mTimeStamp += suspendDuration;
  }
  if (!mStartTime.IsNull()) {
    mStartTime += suspendDuration;
  }

  mSuspendTime = TimeStamp();
}

void
VideoTrackEncoder::AppendVideoSegment(VideoSegment&& aSegment)
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Verbose,
    ("[VideoTrackEncoder %p]: AppendVideoSegment() duration=%" PRIu64,
     this, aSegment.GetDuration()));

  if (mCanceled) {
    return;
  }

  if (mEndOfStream) {
    return;
  }

  mIncomingBuffer.AppendFrom(&aSegment);
}

void
VideoTrackEncoder::TakeTrackData(VideoSegment& aSegment)
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());

  if (mCanceled) {
    return;
  }

  aSegment.AppendFrom(&mOutgoingBuffer);
  mOutgoingBuffer.Clear();
}

void
VideoTrackEncoder::Init(const VideoSegment& aSegment, StreamTime aDuration)
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());

  if (mInitialized) {
    return;
  }

  mInitCounter++;
  TRACK_LOG(LogLevel::Debug,
    ("[VideoTrackEncoder %p]: Init the video encoder %d times",
     this, mInitCounter));

  for (VideoSegment::ConstChunkIterator iter(aSegment); !iter.IsEnded(); iter.Next()) {
    if (iter->IsNull()) {
      continue;
    }

    gfx::IntSize imgsize = iter->mFrame.GetImage()->GetSize();
    gfx::IntSize intrinsicSize = iter->mFrame.GetIntrinsicSize();
    nsresult rv = Init(imgsize.width, imgsize.height,
                       intrinsicSize.width, intrinsicSize.height);

    if (NS_SUCCEEDED(rv)) {
      TRACK_LOG(LogLevel::Info,
        ("[VideoTrackEncoder %p]: Successfully initialized!", this));
      return;
    } else {
      TRACK_LOG(LogLevel::Error,
        ("[VideoTrackEncoder %p]: Failed to initialize the encoder!", this));
      OnError();
    }
    break;
  }

  mNotInitDuration += aDuration;
  if ((mNotInitDuration / mTrackRate > VIDEO_INIT_FAILED_DURATION) &&
      mInitCounter > 1) {
    TRACK_LOG(LogLevel::Warning,
      ("[VideoTrackEncoder %p]: No successful init for %ds.",
       this, VIDEO_INIT_FAILED_DURATION));
    Telemetry::Accumulate(
      Telemetry::MEDIA_RECORDER_TRACK_ENCODER_INIT_TIMEOUT_TYPE, 1);
    OnError();
    return;
  }
}

void
VideoTrackEncoder::Cancel()
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Info,
    ("[VideoTrackEncoder %p]: Cancel(), currentTime=%" PRIu64,
     this, mCurrentTime));
  mCanceled = true;
  mIncomingBuffer.Clear();
  mOutgoingBuffer.Clear();
  mLastChunk.SetNull(0);
}

void
VideoTrackEncoder::NotifyEndOfStream()
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());

  if (!mCanceled && !mInitialized) {
    // If source video track is muted till the end of encoding, initialize the
    // encoder with default frame width, frame height, and track rate.
    Init(DEFAULT_FRAME_WIDTH, DEFAULT_FRAME_HEIGHT,
         DEFAULT_FRAME_WIDTH, DEFAULT_FRAME_HEIGHT);
  }

  if (mEndOfStream) {
    // We have already been notified.
    return;
  }

  mEndOfStream = true;
  TRACK_LOG(LogLevel::Info,
    ("[VideoTrackEncoder %p]: NotifyEndOfStream(), currentTime=%" PRIu64,
     this, mCurrentTime));

  if (!mLastChunk.IsNull() && mLastChunk.mDuration > 0) {
    RefPtr<layers::Image> lastImage = mLastChunk.mFrame.GetImage();
    TRACK_LOG(LogLevel::Debug,
              ("[VideoTrackEncoder]: Appending last video frame %p, "
               "duration=%.5f", lastImage.get(),
               FramesToTimeUnit(mLastChunk.mDuration, mTrackRate).ToSeconds()));
    mOutgoingBuffer.AppendFrame(lastImage.forget(),
                                mLastChunk.mDuration,
                                mLastChunk.mFrame.GetIntrinsicSize(),
                                PRINCIPAL_HANDLE_NONE,
                                mLastChunk.mFrame.GetForceBlack(),
                                mLastChunk.mTimeStamp);
  }

  mIncomingBuffer.Clear();
  mLastChunk.SetNull(0);

  if (mInitialized && !mCanceled) {
    OnDataAvailable();
  }
}

void
VideoTrackEncoder::SetStartOffset(StreamTime aStartOffset)
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  MOZ_ASSERT(mCurrentTime == 0);
  TRACK_LOG(LogLevel::Info,
    ("[VideoTrackEncoder %p]: SetStartOffset(), aStartOffset=%" PRIu64,
     this, aStartOffset));
  mIncomingBuffer.InsertNullDataAtStart(aStartOffset);
  mCurrentTime = aStartOffset;
}

void
VideoTrackEncoder::AdvanceBlockedInput(StreamTime aDuration)
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Verbose,
    ("[VideoTrackEncoder %p]: AdvanceBlockedInput(), aDuration=%" PRIu64,
     this, aDuration));

  // We call Init here so it can account for aDuration towards the Init timeout
  Init(mOutgoingBuffer, aDuration);

  mIncomingBuffer.InsertNullDataAtStart(aDuration);
  mCurrentTime += aDuration;
}

void
VideoTrackEncoder::AdvanceCurrentTime(StreamTime aDuration)
{
  AUTO_PROFILER_LABEL("VideoTrackEncoder::AdvanceCurrentTime", OTHER);

  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());

  if (mCanceled) {
    return;
  }

  if (mEndOfStream) {
    return;
  }

  TRACK_LOG(LogLevel::Verbose,
    ("[VideoTrackEncoder %p]: AdvanceCurrentTime() %" PRIu64,
     this, aDuration));

  StreamTime currentTime = mCurrentTime + aDuration;

  if (mSuspended) {
    mCurrentTime = currentTime;
    mIncomingBuffer.ForgetUpTo(mCurrentTime);
    return;
  }

  VideoSegment tempSegment;
  if (currentTime <= mIncomingBuffer.GetDuration()) {
    tempSegment.AppendSlice(mIncomingBuffer, mCurrentTime, currentTime);
  } else {
    NS_ASSERTION(false, "VideoTrackEncoder::AdvanceCurrentTime Not enough data");
    TRACK_LOG(LogLevel::Error,
      ("[VideoTrackEncoder %p]: AdvanceCurrentTime() Not enough data. "
       "In incoming=%" PRIu64 ", aDuration=%" PRIu64 ", currentTime=%" PRIu64,
       this, mIncomingBuffer.GetDuration(), aDuration, currentTime));
    OnError();
  }

  mCurrentTime = currentTime;
  mIncomingBuffer.ForgetUpTo(mCurrentTime);

  bool chunkAppended = false;

  // Convert tempSegment timestamps to durations and add it to mOutgoingBuffer.
  VideoSegment::ConstChunkIterator iter(tempSegment);
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
        return;
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
      chunk.mDuration = mLastChunk.mDuration - mTrackRate;
      mLastChunk.mDuration = mTrackRate;

      if (chunk.IsNull()) {
        // Ensure that we don't pass null to the encoder by making mLastChunk
        // null later on.
        chunk.mFrame = mLastChunk.mFrame;
      }
    }

    if (mStartTime.IsNull()) {
      mStartTime = mLastChunk.mTimeStamp;
    }

    TimeDuration relativeTime = chunk.mTimeStamp - mStartTime;
    RefPtr<layers::Image> lastImage = mLastChunk.mFrame.GetImage();
    TRACK_LOG(LogLevel::Verbose,
              ("[VideoTrackEncoder]: Appending video frame %p, at pos %.5fs",
               lastImage.get(), relativeTime.ToSeconds()));
    CheckedInt64 duration = UsecsToFrames(relativeTime.ToMicroseconds(),
                                          mTrackRate)
                            - mEncodedTicks;
    if (!duration.isValid()) {
      NS_ERROR("Duration overflow");
      return;
    }

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
      mOutgoingBuffer.AppendFrame(lastImage.forget(),
                                  duration.value(),
                                  mLastChunk.mFrame.GetIntrinsicSize(),
                                  PRINCIPAL_HANDLE_NONE,
                                  mLastChunk.mFrame.GetForceBlack(),
                                  mLastChunk.mTimeStamp);
      chunkAppended = true;
    }

    mLastChunk = chunk;
  }

  if (chunkAppended) {
    Init(mOutgoingBuffer, aDuration);
    if (mInitialized) {
      OnDataAvailable();
    }
  }
}

size_t
VideoTrackEncoder::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  return mIncomingBuffer.SizeOfExcludingThis(aMallocSizeOf) +
         mOutgoingBuffer.SizeOfExcludingThis(aMallocSizeOf);
}

} // namespace mozilla
