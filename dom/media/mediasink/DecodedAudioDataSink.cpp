/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintfCString.h"
#include "MediaQueue.h"
#include "DecodedAudioDataSink.h"
#include "VideoUtils.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"

namespace mozilla {

extern LazyLogModule gMediaDecoderLog;
#define SINK_LOG(msg, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Debug, \
    ("DecodedAudioDataSink=%p " msg, this, ##__VA_ARGS__))
#define SINK_LOG_V(msg, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Verbose, \
  ("DecodedAudioDataSink=%p " msg, this, ##__VA_ARGS__))

namespace media {

// The amount of audio frames that is used to fuzz rounding errors.
static const int64_t AUDIO_FUZZ_FRAMES = 1;

DecodedAudioDataSink::DecodedAudioDataSink(MediaQueue<MediaData>& aAudioQueue,
                                           int64_t aStartTime,
                                           const AudioInfo& aInfo,
                                           dom::AudioChannel aChannel)
  : AudioSink(aAudioQueue)
  , mStartTime(aStartTime)
  , mWritten(0)
  , mLastGoodPosition(0)
  , mInfo(aInfo)
  , mChannel(aChannel)
  , mPlaying(true)
{
}

DecodedAudioDataSink::~DecodedAudioDataSink()
{
}

RefPtr<GenericPromise>
DecodedAudioDataSink::Init(const PlaybackParams& aParams)
{
  RefPtr<GenericPromise> p = mEndPromise.Ensure(__func__);
  nsresult rv = InitializeAudioStream(aParams);
  if (NS_FAILED(rv)) {
    mEndPromise.Reject(rv, __func__);
  }
  return p;
}

int64_t
DecodedAudioDataSink::GetPosition()
{
  int64_t pos;
  if (mAudioStream &&
      (pos = mAudioStream->GetPosition()) >= 0) {
    NS_ASSERTION(pos >= mLastGoodPosition,
                 "AudioStream position shouldn't go backward");
    // Update the last good position when we got a good one.
    if (pos >= mLastGoodPosition) {
      mLastGoodPosition = pos;
    }
  }

  return mStartTime + mLastGoodPosition;
}

bool
DecodedAudioDataSink::HasUnplayedFrames()
{
  // Experimentation suggests that GetPositionInFrames() is zero-indexed,
  // so we need to add 1 here before comparing it to mWritten.
  return mAudioStream && mAudioStream->GetPositionInFrames() + 1 < mWritten;
}

void
DecodedAudioDataSink::Shutdown()
{
  if (mAudioStream) {
    mAudioStream->Shutdown();
    mAudioStream = nullptr;
  }
  mEndPromise.ResolveIfExists(true, __func__);
}

void
DecodedAudioDataSink::SetVolume(double aVolume)
{
  if (mAudioStream) {
    mAudioStream->SetVolume(aVolume);
  }
}

void
DecodedAudioDataSink::SetPlaybackRate(double aPlaybackRate)
{
  MOZ_ASSERT(aPlaybackRate != 0, "Don't set the playbackRate to 0 on AudioStream");
  if (mAudioStream) {
    mAudioStream->SetPlaybackRate(aPlaybackRate);
  }
}

void
DecodedAudioDataSink::SetPreservesPitch(bool aPreservesPitch)
{
  if (mAudioStream) {
    mAudioStream->SetPreservesPitch(aPreservesPitch);
  }
}

void
DecodedAudioDataSink::SetPlaying(bool aPlaying)
{
  if (!mAudioStream || mPlaying == aPlaying) {
    return;
  }
  // pause/resume AudioStream as necessary.
  if (!aPlaying && !mAudioStream->IsPaused()) {
    mAudioStream->Pause();
  } else if (aPlaying && mAudioStream->IsPaused()) {
    mAudioStream->Resume();
  }
  mPlaying = aPlaying;
}

nsresult
DecodedAudioDataSink::InitializeAudioStream(const PlaybackParams& aParams)
{
  mAudioStream = new AudioStream(*this);
  nsresult rv = mAudioStream->Init(mInfo.mChannels, mInfo.mRate, mChannel);
  if (NS_FAILED(rv)) {
    mAudioStream->Shutdown();
    mAudioStream = nullptr;
    return rv;
  }

  // Set playback params before calling Start() so they can take effect
  // as soon as the 1st DataCallback of the AudioStream fires.
  mAudioStream->SetVolume(aParams.mVolume);
  mAudioStream->SetPlaybackRate(aParams.mPlaybackRate);
  mAudioStream->SetPreservesPitch(aParams.mPreservesPitch);
  mAudioStream->Start();

  return NS_OK;
}

int64_t
DecodedAudioDataSink::GetEndTime() const
{
  CheckedInt64 playedUsecs = FramesToUsecs(mWritten, mInfo.mRate) + mStartTime;
  if (!playedUsecs.isValid()) {
    NS_WARNING("Int overflow calculating audio end time");
    return -1;
  }
  return playedUsecs.value();
}

UniquePtr<AudioStream::Chunk>
DecodedAudioDataSink::PopFrames(uint32_t aFrames)
{
  class Chunk : public AudioStream::Chunk {
  public:
    Chunk(AudioData* aBuffer, uint32_t aFrames, AudioDataValue* aData)
      : mBuffer(aBuffer), mFrames(aFrames), mData(aData) {}
    Chunk() : mFrames(0), mData(nullptr) {}
    const AudioDataValue* Data() const { return mData; }
    uint32_t Frames() const { return mFrames; }
    uint32_t Channels() const { return mBuffer ? mBuffer->mChannels: 0; }
    uint32_t Rate() const { return mBuffer ? mBuffer->mRate : 0; }
    AudioDataValue* GetWritable() const { return mData; }
  private:
    const RefPtr<AudioData> mBuffer;
    const uint32_t mFrames;
    AudioDataValue* const mData;
  };

  class SilentChunk : public AudioStream::Chunk {
  public:
    SilentChunk(uint32_t aFrames, uint32_t aChannels, uint32_t aRate)
      : mFrames(aFrames)
      , mChannels(aChannels)
      , mRate(aRate)
      , mData(MakeUnique<AudioDataValue[]>(aChannels * aFrames)) {
      memset(mData.get(), 0, aChannels * aFrames * sizeof(AudioDataValue));
    }
    const AudioDataValue* Data() const { return mData.get(); }
    uint32_t Frames() const { return mFrames; }
    uint32_t Channels() const { return mChannels; }
    uint32_t Rate() const { return mRate; }
    AudioDataValue* GetWritable() const { return mData.get(); }
  private:
    const uint32_t mFrames;
    const uint32_t mChannels;
    const uint32_t mRate;
    UniquePtr<AudioDataValue[]> mData;
  };

  while (!mCurrentData) {
    // No data in the queue. Return an empty chunk.
    if (AudioQueue().GetSize() == 0) {
      return MakeUnique<Chunk>();
    }

    AudioData* a = AudioQueue().PeekFront()->As<AudioData>();

    // Ignore the element with 0 frames and try next.
    if (a->mFrames == 0) {
      RefPtr<MediaData> releaseMe = AudioQueue().PopFront();
      continue;
    }

    // Ignore invalid samples.
    if (a->mRate != mInfo.mRate || a->mChannels != mInfo.mChannels) {
      NS_WARNING(nsPrintfCString(
        "mismatched sample format, data=%p rate=%u channels=%u frames=%u",
        a->mAudioData.get(), a->mRate, a->mChannels, a->mFrames).get());
      RefPtr<MediaData> releaseMe = AudioQueue().PopFront();
      continue;
    }

    // See if there's a gap in the audio. If there is, push silence into the
    // audio hardware, so we can play across the gap.
    // Calculate the timestamp of the next chunk of audio in numbers of
    // samples.
    CheckedInt64 sampleTime = UsecsToFrames(AudioQueue().PeekFront()->mTime, mInfo.mRate);
    // Calculate the number of frames that have been pushed onto the audio hardware.
    CheckedInt64 playedFrames = UsecsToFrames(mStartTime, mInfo.mRate) +
                                static_cast<int64_t>(mWritten);
    CheckedInt64 missingFrames = sampleTime - playedFrames;

    if (!missingFrames.isValid() || !sampleTime.isValid()) {
      NS_WARNING("Int overflow in DecodedAudioDataSink");
      mErrored = true;
      return MakeUnique<Chunk>();
    }

    if (missingFrames.value() > AUDIO_FUZZ_FRAMES) {
      // The next audio chunk begins some time after the end of the last chunk
      // we pushed to the audio hardware. We must push silence into the audio
      // hardware so that the next audio chunk begins playback at the correct
      // time.
      missingFrames = std::min<int64_t>(UINT32_MAX, missingFrames.value());
      auto framesToPop = std::min<uint32_t>(missingFrames.value(), aFrames);
      mWritten += framesToPop;
      return MakeUnique<SilentChunk>(framesToPop, mInfo.mChannels, mInfo.mRate);
    }

    mCurrentData = dont_AddRef(AudioQueue().PopFront().take()->As<AudioData>());
    mCursor = MakeUnique<AudioBufferCursor>(mCurrentData->mAudioData.get(),
                                            mCurrentData->mChannels,
                                            mCurrentData->mFrames);
    MOZ_ASSERT(mCurrentData->mFrames > 0);
  }

  auto framesToPop = std::min(aFrames, mCursor->Available());

  SINK_LOG_V("playing audio at time=%lld offset=%u length=%u",
             mCurrentData->mTime, mCurrentData->mFrames - mCursor->Available(), framesToPop);

  UniquePtr<AudioStream::Chunk> chunk =
    MakeUnique<Chunk>(mCurrentData, framesToPop, mCursor->Ptr());

  mWritten += framesToPop;
  mCursor->Advance(framesToPop);

  // All frames are popped. Reset mCurrentData so we can pop new elements from
  // the audio queue in next calls to PopFrames().
  if (mCursor->Available() == 0) {
    mCurrentData = nullptr;
  }

  return chunk;
}

bool
DecodedAudioDataSink::Ended() const
{
  // Return true when error encountered so AudioStream can start draining.
  return AudioQueue().IsFinished() || mErrored;
}

void
DecodedAudioDataSink::Drained()
{
  SINK_LOG("Drained");
  mEndPromise.Resolve(true, __func__);
}

} // namespace media
} // namespace mozilla
