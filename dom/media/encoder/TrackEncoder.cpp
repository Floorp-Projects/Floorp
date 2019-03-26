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
// A maximal key frame interval allowed to set.
// Longer values will be shorten to this value.
static const int DEFAULT_KEYFRAME_INTERVAL_MS = 1000;

TrackEncoder::TrackEncoder(TrackRate aTrackRate)
    : mEncodingComplete(false),
      mEosSetInEncoder(false),
      mInitialized(false),
      mEndOfStream(false),
      mCanceled(false),
      mInitCounter(0),
      mSuspended(false),
      mTrackRate(aTrackRate) {}

bool TrackEncoder::IsInitialized() {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  return mInitialized;
}

bool TrackEncoder::IsEncodingComplete() {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  return mEncodingComplete;
}

void TrackEncoder::SetInitialized() {
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

void TrackEncoder::OnDataAvailable() {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());

  auto listeners(mListeners);
  for (auto& l : listeners) {
    l->DataAvailable(this);
  }
}

void TrackEncoder::OnError() {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());

  Cancel();

  auto listeners(mListeners);
  for (auto& l : listeners) {
    l->Error(this);
  }
}

void TrackEncoder::RegisterListener(TrackEncoderListener* aListener) {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mListeners.Contains(aListener));
  mListeners.AppendElement(aListener);
}

bool TrackEncoder::UnregisterListener(TrackEncoderListener* aListener) {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  return mListeners.RemoveElement(aListener);
}

void TrackEncoder::SetWorkerThread(AbstractThread* aWorkerThread) {
  mWorkerThread = aWorkerThread;
}

void AudioTrackEncoder::Suspend() {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Info, ("[AudioTrackEncoder %p]: Suspend(), was %s", this,
                             mSuspended ? "suspended" : "live"));

  if (mSuspended) {
    return;
  }

  mSuspended = true;
}

void AudioTrackEncoder::Resume() {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Info, ("[AudioTrackEncoder %p]: Resume(), was %s", this,
                             mSuspended ? "suspended" : "live"));

  if (!mSuspended) {
    return;
  }

  mSuspended = false;
}

void AudioTrackEncoder::AppendAudioSegment(AudioSegment&& aSegment) {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  AUTO_PROFILER_LABEL("AudioTrackEncoder::AppendAudioSegment", OTHER);
  TRACK_LOG(LogLevel::Verbose,
            ("[AudioTrackEncoder %p]: AppendAudioSegment() duration=%" PRIu64,
             this, aSegment.GetDuration()));

  if (mCanceled) {
    return;
  }

  if (mEndOfStream) {
    return;
  }

  TryInit(mOutgoingBuffer, aSegment.GetDuration());

  if (!mSuspended) {
    mOutgoingBuffer.AppendFrom(&aSegment);
  }

  if (mInitialized && mOutgoingBuffer.GetDuration() >= GetPacketDuration()) {
    OnDataAvailable();
  }
}

void AudioTrackEncoder::TakeTrackData(AudioSegment& aSegment) {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());

  if (mCanceled) {
    return;
  }

  aSegment.AppendFrom(&mOutgoingBuffer);
}

void AudioTrackEncoder::TryInit(const AudioSegment& aSegment,
                                StreamTime aDuration) {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());

  if (mInitialized) {
    return;
  }

  mInitCounter++;
  TRACK_LOG(LogLevel::Debug,
            ("[AudioTrackEncoder %p]: Inited the audio encoder %d times", this,
             mInitCounter));

  for (AudioSegment::ConstChunkIterator iter(aSegment); !iter.IsEnded();
       iter.Next()) {
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
      TRACK_LOG(
          LogLevel::Error,
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
               AUDIO_INIT_FAILED_DURATION, DEFAULT_CHANNELS));
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

void AudioTrackEncoder::Cancel() {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Info, ("[AudioTrackEncoder %p]: Cancel()", this));
  mCanceled = true;
  mOutgoingBuffer.Clear();
}

void AudioTrackEncoder::NotifyEndOfStream() {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Info,
            ("[AudioTrackEncoder %p]: NotifyEndOfStream()", this));

  if (!mCanceled && !mInitialized) {
    // If source audio track is completely silent till the end of encoding,
    // initialize the encoder with default channel counts and sampling rate.
    Init(DEFAULT_CHANNELS, DEFAULT_SAMPLING_RATE);
  }

  mEndOfStream = true;

  if (mInitialized && !mCanceled) {
    OnDataAvailable();
  }
}

/*static*/
void AudioTrackEncoder::InterleaveTrackData(AudioChunk& aChunk,
                                            int32_t aDuration,
                                            uint32_t aOutputChannels,
                                            AudioDataValue* aOutput) {
  uint32_t numChannelsToCopy = std::min(
      aOutputChannels, static_cast<uint32_t>(aChunk.mChannelData.Length()));
  switch (aChunk.mBufferFormat) {
    case AUDIO_FORMAT_S16: {
      AutoTArray<const int16_t*, 2> array;
      array.SetLength(numChannelsToCopy);
      for (uint32_t i = 0; i < array.Length(); i++) {
        array[i] = static_cast<const int16_t*>(aChunk.mChannelData[i]);
      }
      InterleaveTrackData(array, aDuration, aOutputChannels, aOutput,
                          aChunk.mVolume);
      break;
    }
    case AUDIO_FORMAT_FLOAT32: {
      AutoTArray<const float*, 2> array;
      array.SetLength(numChannelsToCopy);
      for (uint32_t i = 0; i < array.Length(); i++) {
        array[i] = static_cast<const float*>(aChunk.mChannelData[i]);
      }
      InterleaveTrackData(array, aDuration, aOutputChannels, aOutput,
                          aChunk.mVolume);
      break;
    }
    case AUDIO_FORMAT_SILENCE: {
      MOZ_ASSERT(false, "To implement.");
    }
  };
}

/*static*/
void AudioTrackEncoder::DeInterleaveTrackData(AudioDataValue* aInput,
                                              int32_t aDuration,
                                              int32_t aChannels,
                                              AudioDataValue* aOutput) {
  for (int32_t i = 0; i < aChannels; ++i) {
    for (int32_t j = 0; j < aDuration; ++j) {
      aOutput[i * aDuration + j] = aInput[i + j * aChannels];
    }
  }
}

size_t AudioTrackEncoder::SizeOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  return mOutgoingBuffer.SizeOfExcludingThis(aMallocSizeOf);
}

VideoTrackEncoder::VideoTrackEncoder(RefPtr<DriftCompensator> aDriftCompensator,
                                     TrackRate aTrackRate,
                                     FrameDroppingMode aFrameDroppingMode)
    : TrackEncoder(aTrackRate),
      mDriftCompensator(std::move(aDriftCompensator)),
      mFrameWidth(0),
      mFrameHeight(0),
      mDisplayWidth(0),
      mDisplayHeight(0),
      mEncodedTicks(0),
      mVideoBitrate(0),
      mFrameDroppingMode(aFrameDroppingMode),
      mKeyFrameInterval(DEFAULT_KEYFRAME_INTERVAL_MS),
      mEnabled(true) {
  mLastChunk.mDuration = 0;
}

void VideoTrackEncoder::Suspend(const TimeStamp& aTime) {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Info,
            ("[VideoTrackEncoder %p]: Suspend() at %.3fs, was %s", this,
             mStartTime.IsNull() ? 0.0 : (aTime - mStartTime).ToSeconds(),
             mSuspended ? "suspended" : "live"));

  if (mSuspended) {
    return;
  }

  mSuspended = true;
  mSuspendTime = aTime;
}

void VideoTrackEncoder::Resume(const TimeStamp& aTime) {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());

  if (!mSuspended) {
    return;
  }

  TRACK_LOG(
      LogLevel::Info,
      ("[VideoTrackEncoder %p]: Resume() after %.3fs, was %s", this,
       (aTime - mSuspendTime).ToSeconds(), mSuspended ? "suspended" : "live"));

  mSuspended = false;

  TimeDuration suspendDuration = aTime - mSuspendTime;
  if (!mLastChunk.mTimeStamp.IsNull()) {
    VideoChunk* nextChunk = mIncomingBuffer.FindChunkContaining(aTime);
    MOZ_ASSERT_IF(nextChunk, nextChunk->mTimeStamp <= aTime);
    if (nextChunk) {
      nextChunk->mTimeStamp = aTime;
    }
    mLastChunk.mTimeStamp += suspendDuration;
  }
  if (!mStartTime.IsNull()) {
    mStartTime += suspendDuration;
  }

  mSuspendTime = TimeStamp();
}

void VideoTrackEncoder::Disable(const TimeStamp& aTime) {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Debug, ("[VideoTrackEncoder %p]: Disable()", this));

  if (mStartTime.IsNull()) {
    // We haven't started yet. No need to touch future frames.
    mEnabled = false;
    return;
  }

  // Advancing currentTime to process any frames in mIncomingBuffer between
  // mCurrentTime and aTime.
  AdvanceCurrentTime(aTime);
  if (!mLastChunk.mTimeStamp.IsNull()) {
    // Insert a black frame at t=aTime into mIncomingBuffer, to trigger the
    // shift to black at the right moment.
    VideoSegment tempSegment;
    tempSegment.AppendFrom(&mIncomingBuffer);
    mIncomingBuffer.AppendFrame(do_AddRef(mLastChunk.mFrame.GetImage()),
                                mLastChunk.mFrame.GetIntrinsicSize(),
                                mLastChunk.mFrame.GetPrincipalHandle(), true,
                                aTime);
    mIncomingBuffer.AppendFrom(&tempSegment);
  }
  mEnabled = false;
}

void VideoTrackEncoder::Enable(const TimeStamp& aTime) {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Debug, ("[VideoTrackEncoder %p]: Enable()", this));

  if (mStartTime.IsNull()) {
    // We haven't started yet. No need to touch future frames.
    mEnabled = true;
    return;
  }

  // Advancing currentTime to process any frames in mIncomingBuffer between
  // mCurrentTime and aTime.
  AdvanceCurrentTime(aTime);
  if (!mLastChunk.mTimeStamp.IsNull()) {
    // Insert a real frame at t=aTime into mIncomingBuffer, to trigger the
    // shift from black at the right moment.
    VideoSegment tempSegment;
    tempSegment.AppendFrom(&mIncomingBuffer);
    mIncomingBuffer.AppendFrame(do_AddRef(mLastChunk.mFrame.GetImage()),
                                mLastChunk.mFrame.GetIntrinsicSize(),
                                mLastChunk.mFrame.GetPrincipalHandle(),
                                mLastChunk.mFrame.GetForceBlack(), aTime);
    mIncomingBuffer.AppendFrom(&tempSegment);
  }
  mEnabled = true;
}

void VideoTrackEncoder::AppendVideoSegment(VideoSegment&& aSegment) {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Verbose,
            ("[VideoTrackEncoder %p]: AppendVideoSegment()", this));

  if (mCanceled) {
    return;
  }

  if (mEndOfStream) {
    return;
  }

  for (VideoSegment::ConstChunkIterator iter(aSegment); !iter.IsEnded();
       iter.Next()) {
    if (iter->IsNull()) {
      // A null image was sent. This is a signal from the source that we should
      // clear any images buffered in the future.
      mIncomingBuffer.Clear();
      continue;  // Don't append iter, as it is null.
    }
    if (VideoChunk* c = mIncomingBuffer.GetLastChunk()) {
      if (iter->mTimeStamp < c->mTimeStamp) {
        // Time went backwards. This can happen when a MediaDecoder seeks.
        // We need to handle this by removing any frames buffered in the future
        // and start over at iter->mTimeStamp.
        mIncomingBuffer.Clear();
      }
    }
    mIncomingBuffer.AppendFrame(do_AddRef(iter->mFrame.GetImage()),
                                iter->mFrame.GetIntrinsicSize(),
                                iter->mFrame.GetPrincipalHandle(),
                                iter->mFrame.GetForceBlack(), iter->mTimeStamp);
  }
  aSegment.Clear();
}

void VideoTrackEncoder::TakeTrackData(VideoSegment& aSegment) {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());

  if (mCanceled) {
    return;
  }

  aSegment.AppendFrom(&mOutgoingBuffer);
  mOutgoingBuffer.Clear();
}

void VideoTrackEncoder::Init(const VideoSegment& aSegment,
                             const TimeStamp& aTime) {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());

  if (mInitialized) {
    return;
  }

  mInitCounter++;
  TRACK_LOG(LogLevel::Debug,
            ("[VideoTrackEncoder %p]: Init the video encoder %d times", this,
             mInitCounter));

  for (VideoSegment::ConstChunkIterator iter(aSegment); !iter.IsEnded();
       iter.Next()) {
    if (iter->IsNull()) {
      continue;
    }

    gfx::IntSize imgsize = iter->mFrame.GetImage()->GetSize();
    gfx::IntSize intrinsicSize = iter->mFrame.GetIntrinsicSize();
    nsresult rv = Init(imgsize.width, imgsize.height, intrinsicSize.width,
                       intrinsicSize.height);

    if (NS_SUCCEEDED(rv)) {
      TRACK_LOG(LogLevel::Info,
                ("[VideoTrackEncoder %p]: Successfully initialized!", this));
      return;
    } else {
      TRACK_LOG(
          LogLevel::Error,
          ("[VideoTrackEncoder %p]: Failed to initialize the encoder!", this));
      OnError();
    }
    break;
  }

  if (((aTime - mStartTime).ToSeconds() > VIDEO_INIT_FAILED_DURATION) &&
      mInitCounter > 1) {
    TRACK_LOG(LogLevel::Warning,
              ("[VideoTrackEncoder %p]: No successful init for %ds.", this,
               VIDEO_INIT_FAILED_DURATION));
    Telemetry::Accumulate(
        Telemetry::MEDIA_RECORDER_TRACK_ENCODER_INIT_TIMEOUT_TYPE, 1);
    OnError();
    return;
  }
}

void VideoTrackEncoder::Cancel() {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  TRACK_LOG(LogLevel::Info, ("[VideoTrackEncoder %p]: Cancel()", this));
  mCanceled = true;
  mIncomingBuffer.Clear();
  mOutgoingBuffer.Clear();
  mLastChunk.SetNull(0);
}

void VideoTrackEncoder::NotifyEndOfStream() {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());

  if (!mCanceled && !mInitialized) {
    // If source video track is muted till the end of encoding, initialize the
    // encoder with default frame width, frame height, and track rate.
    Init(DEFAULT_FRAME_WIDTH, DEFAULT_FRAME_HEIGHT, DEFAULT_FRAME_WIDTH,
         DEFAULT_FRAME_HEIGHT);
  }

  if (mEndOfStream) {
    // We have already been notified.
    return;
  }

  mEndOfStream = true;
  TRACK_LOG(LogLevel::Info,
            ("[VideoTrackEncoder %p]: NotifyEndOfStream()", this));

  if (!mLastChunk.IsNull()) {
    RefPtr<layers::Image> lastImage = mLastChunk.mFrame.GetImage();
    const TimeStamp now = TimeStamp::Now();
    TimeStamp currentTime = mSuspended ? mSuspendTime : mCurrentTime;
    currentTime = mDriftCompensator->GetVideoTime(now, currentTime);
    TimeDuration absoluteEndTime = currentTime - mStartTime;
    CheckedInt64 duration =
        UsecsToFrames(absoluteEndTime.ToMicroseconds(), mTrackRate) -
        mEncodedTicks;
    if (duration.isValid() && duration.value() > 0) {
      mEncodedTicks += duration.value();
      TRACK_LOG(LogLevel::Debug,
                ("[VideoTrackEncoder %p]: Appending last video frame %p at pos "
                 "%.3fs, "
                 "track-end=%.3fs",
                 this, lastImage.get(),
                 (mLastChunk.mTimeStamp - mStartTime).ToSeconds(),
                 absoluteEndTime.ToSeconds()));
      mOutgoingBuffer.AppendFrame(
          lastImage.forget(), mLastChunk.mFrame.GetIntrinsicSize(),
          PRINCIPAL_HANDLE_NONE, mLastChunk.mFrame.GetForceBlack() || !mEnabled,
          mLastChunk.mTimeStamp);
      mOutgoingBuffer.ExtendLastFrameBy(duration.value());
    }
  }

  mIncomingBuffer.Clear();
  mLastChunk.SetNull(0);

  if (mInitialized && !mCanceled) {
    OnDataAvailable();
  }
}

void VideoTrackEncoder::SetStartOffset(const TimeStamp& aStartOffset) {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  MOZ_ASSERT(mCurrentTime.IsNull());
  TRACK_LOG(LogLevel::Info, ("[VideoTrackEncoder %p]: SetStartOffset()", this));
  mStartTime = aStartOffset;
  mCurrentTime = aStartOffset;
}

void VideoTrackEncoder::AdvanceCurrentTime(const TimeStamp& aTime) {
  AUTO_PROFILER_LABEL("VideoTrackEncoder::AdvanceCurrentTime", OTHER);

  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mStartTime.IsNull());
  MOZ_ASSERT(!mCurrentTime.IsNull());

  if (mCanceled) {
    return;
  }

  if (mEndOfStream) {
    return;
  }

  if (mSuspended) {
    TRACK_LOG(
        LogLevel::Verbose,
        ("[VideoTrackEncoder %p]: AdvanceCurrentTime() suspended at %.3fs",
         this, (mCurrentTime - mStartTime).ToSeconds()));
    mCurrentTime = aTime;
    mIncomingBuffer.ForgetUpToTime(mCurrentTime);
    return;
  }

  TRACK_LOG(LogLevel::Verbose,
            ("[VideoTrackEncoder %p]: AdvanceCurrentTime() to %.3fs", this,
             (aTime - mStartTime).ToSeconds()));

  // Grab frames within the currentTime range from the incoming buffer.
  VideoSegment tempSegment;
  {
    VideoChunk* previousChunk = &mLastChunk;
    auto appendDupes = [&](const TimeStamp& aUpTo) {
      while ((aUpTo - previousChunk->mTimeStamp).ToSeconds() > 1.0) {
        // We encode at least one frame per second, even if there are none
        // flowing.
        previousChunk->mTimeStamp += TimeDuration::FromSeconds(1.0);
        tempSegment.AppendFrame(
            do_AddRef(previousChunk->mFrame.GetImage()),
            previousChunk->mFrame.GetIntrinsicSize(),
            previousChunk->mFrame.GetPrincipalHandle(),
            previousChunk->mFrame.GetForceBlack() || !mEnabled,
            previousChunk->mTimeStamp);
        TRACK_LOG(
            LogLevel::Verbose,
            ("[VideoTrackEncoder %p]: Duplicating video frame (%p) at pos %.3f",
             this, previousChunk->mFrame.GetImage(),
             (previousChunk->mTimeStamp - mStartTime).ToSeconds()));
      }
    };
    for (VideoSegment::ChunkIterator iter(mIncomingBuffer); !iter.IsEnded();
         iter.Next()) {
      MOZ_ASSERT(!iter->IsNull());
      if (!previousChunk->IsNull() &&
          iter->mTimeStamp <= previousChunk->mTimeStamp) {
        // This frame starts earlier than previousChunk. Skip.
        continue;
      }
      if (iter->mTimeStamp >= aTime) {
        // This frame starts in the future. Stop.
        break;
      }
      if (!previousChunk->IsNull()) {
        appendDupes(iter->mTimeStamp);
      }
      tempSegment.AppendFrame(
          do_AddRef(iter->mFrame.GetImage()), iter->mFrame.GetIntrinsicSize(),
          iter->mFrame.GetPrincipalHandle(),
          iter->mFrame.GetForceBlack() || !mEnabled, iter->mTimeStamp);
      TRACK_LOG(LogLevel::Verbose,
                ("[VideoTrackEncoder %p]: Taking video frame (%p) at pos %.3f",
                 this, iter->mFrame.GetImage(),
                 (iter->mTimeStamp - mStartTime).ToSeconds()));
      previousChunk = &*iter;
    }
    if (!previousChunk->IsNull()) {
      appendDupes(aTime);
    }
  }
  mCurrentTime = aTime;
  mIncomingBuffer.ForgetUpToTime(mCurrentTime);

  // Convert tempSegment timestamps to durations and add chunks with known
  // duration to mOutgoingBuffer.
  const TimeStamp now = TimeStamp::Now();
  bool chunkAppended = false;
  for (VideoSegment::ConstChunkIterator iter(tempSegment); !iter.IsEnded();
       iter.Next()) {
    VideoChunk chunk = *iter;

    if (mLastChunk.mTimeStamp.IsNull()) {
      // This is the first real chunk in the track. Make it start at the
      // beginning of the track.
      MOZ_ASSERT(!iter->mTimeStamp.IsNull());

      TRACK_LOG(
          LogLevel::Verbose,
          ("[VideoTrackEncoder %p]: Got the first video frame (%p) at pos %.3f "
           "(moving it to beginning)",
           this, iter->mFrame.GetImage(),
           (iter->mTimeStamp - mStartTime).ToSeconds()));

      mLastChunk = *iter;
      mLastChunk.mTimeStamp = mStartTime;
      continue;
    }

    MOZ_ASSERT(!mLastChunk.IsNull());
    MOZ_ASSERT(!chunk.IsNull());

    TimeDuration absoluteEndTime =
        mDriftCompensator->GetVideoTime(now, chunk.mTimeStamp) - mStartTime;
    TRACK_LOG(LogLevel::Verbose,
              ("[VideoTrackEncoder %p]: Appending video frame %p, at pos %.3fs "
               "until %.3fs",
               this, mLastChunk.mFrame.GetImage(),
               (mDriftCompensator->GetVideoTime(now, mLastChunk.mTimeStamp) -
                mStartTime)
                   .ToSeconds(),
               absoluteEndTime.ToSeconds()));
    CheckedInt64 duration =
        UsecsToFrames(absoluteEndTime.ToMicroseconds(), mTrackRate) -
        mEncodedTicks;
    if (!duration.isValid()) {
      NS_ERROR("Duration overflow");
      return;
    }

    if (duration.value() <= 0) {
      // A frame either started before the last frame (can happen when
      // multiple frames are added before SetStartOffset), or
      // two frames were so close together that they ended up at the same
      // position. We handle both cases by ignoring the previous frame.

      TRACK_LOG(LogLevel::Verbose,
                ("[VideoTrackEncoder %p]: Duration from frame %p to frame %p "
                 "is %" PRId64 ". Ignoring %p",
                 this, mLastChunk.mFrame.GetImage(), iter->mFrame.GetImage(),
                 duration.value(), mLastChunk.mFrame.GetImage()));

      TimeStamp t = mLastChunk.mTimeStamp;
      mLastChunk = *iter;
      mLastChunk.mTimeStamp = t;
      continue;
    }

    mEncodedTicks += duration.value();
    mOutgoingBuffer.AppendFrame(
        do_AddRef(mLastChunk.mFrame.GetImage()),
        mLastChunk.mFrame.GetIntrinsicSize(), PRINCIPAL_HANDLE_NONE,
        mLastChunk.mFrame.GetForceBlack() || !mEnabled, mLastChunk.mTimeStamp);
    mOutgoingBuffer.ExtendLastFrameBy(duration.value());
    chunkAppended = true;
    mLastChunk = chunk;
  }

  if (chunkAppended) {
    Init(mOutgoingBuffer, mCurrentTime);
    if (mInitialized) {
      OnDataAvailable();
    }
  }
}

size_t VideoTrackEncoder::SizeOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  return mIncomingBuffer.SizeOfExcludingThis(aMallocSizeOf) +
         mOutgoingBuffer.SizeOfExcludingThis(aMallocSizeOf);
}

void VideoTrackEncoder::SetKeyFrameInterval(int32_t aKeyFrameInterval) {
  MOZ_ASSERT(!mWorkerThread || mWorkerThread->IsCurrentThreadIn());
  mKeyFrameInterval = std::min(aKeyFrameInterval, DEFAULT_KEYFRAME_INTERVAL_MS);
}

}  // namespace mozilla
