/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintfCString.h"
#include "MediaQueue.h"
#include "DecodedAudioDataSink.h"
#include "VideoUtils.h"
#include "AudioConverter.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"
#include "gfxPrefs.h"

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

DecodedAudioDataSink::DecodedAudioDataSink(AbstractThread* aThread,
                                           MediaQueue<MediaData>& aAudioQueue,
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
  , mErrored(false)
  , mPlaybackComplete(false)
  , mProcessingThread(aThread)
  , mShutdown(false)
  , mFramesParsed(0)
  , mLastEndTime(0)
{
  bool resampling = gfxPrefs::AudioSinkResampling();

  if (resampling) {
    mOutputRate = gfxPrefs::AudioSinkResampleRate();
  } else if (mInfo.mRate == 44100 || mInfo.mRate == 48000) {
    // The original rate is of good quality and we want to minimize unecessary
    // resampling. The common scenario being that the sampling rate is one or
    // the other, this allows to minimize audio quality regression and hoping
    // content provider want change from those rates mid-stream.
    mOutputRate = mInfo.mRate;
  } else {
    // We will resample all data to match cubeb's preferred sampling rate.
    mOutputRate = AudioStream::GetPreferredRate();
  }

  bool monoAudioEnabled = gfxPrefs::MonoAudio();

  mOutputChannels = monoAudioEnabled
    ? 1 : (gfxPrefs::AudioSinkForceStereo() ? 2 : mInfo.mChannels);

  mAudioQueueListener = aAudioQueue.PushEvent().Connect(
    mProcessingThread, this, &DecodedAudioDataSink::OnAudioPushed);
  mProcessedQueueListener = mProcessedQueue.PopEvent().Connect(
    mProcessingThread, this, &DecodedAudioDataSink::OnAudioPopped);

  // To ensure at least one audio packet will be popped from AudioQueue and
  // ready to be played.
  NotifyAudioNeeded();
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
  mAudioQueueListener.Disconnect();
  mProcessedQueueListener.Disconnect();
  if (mAudioStream) {
    mAudioStream->Shutdown();
    mAudioStream = nullptr;
  }
  mProcessedQueue.Reset();
  mEndPromise.ResolveIfExists(true, __func__);
  mShutdown = true;
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
  if (!mAudioStream || mPlaying == aPlaying || mPlaybackComplete) {
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
  nsresult rv = mAudioStream->Init(mOutputChannels, mOutputRate, mChannel);
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
  CheckedInt64 playedUsecs = FramesToUsecs(mWritten, mOutputRate) + mStartTime;
  if (!playedUsecs.isValid()) {
    NS_WARNING("Int overflow calculating audio end time");
    return -1;
  }
  // As we may be resampling, rounding errors may occur. Ensure we never get
  // past the original end time.
  return std::min<int64_t>(mLastEndTime, playedUsecs.value());
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

  if (!mCurrentData) {
    // No data in the queue. Return an empty chunk.
    if (!mProcessedQueue.GetSize()) {
      return MakeUnique<Chunk>();
    }

    mCurrentData = dont_AddRef(mProcessedQueue.PopFront().take());
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
  if (!mCursor->Available()) {
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
  mPlaybackComplete = true;
  mEndPromise.ResolveIfExists(true, __func__);
}

void
DecodedAudioDataSink::OnAudioPopped(const RefPtr<MediaData>& aSample)
{
  SINK_LOG_V("AudioStream has used an audio packet.");
  NotifyAudioNeeded();
}

void
DecodedAudioDataSink::OnAudioPushed(const RefPtr<MediaData>& aSample)
{
  SINK_LOG_V("One new audio packet available.");
  NotifyAudioNeeded();
}

void
DecodedAudioDataSink::NotifyAudioNeeded()
{
  if (mShutdown) {
    return;
  }
  if (!mProcessingThread->IsCurrentThreadIn()) {
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(
      this, &DecodedAudioDataSink::NotifyAudioNeeded);
    mProcessingThread->Dispatch(r.forget());
    return;
  }

  if (AudioQueue().IsFinished() && !AudioQueue().GetSize()) {
    // We have reached the end of the data, drain the resampler.
    DrainConverter();
    return;
  }

  // Always ensure we have two processed frames pending to allow for processing
  // latency.
  while (AudioQueue().GetSize() && mProcessedQueue.GetSize() < 2) {
    RefPtr<AudioData> data =
      dont_AddRef(AudioQueue().PopFront().take()->As<AudioData>());
    // Ignore the element with 0 frames and try next.
    if (!data->mFrames) {
      continue;
    }

    if (!mConverter ||
        (data->mRate != mConverter->InputConfig().Rate() ||
         data->mChannels != mConverter->InputConfig().Channels())) {
      SINK_LOG_V("Audio format changed from %u@%uHz to %u@%uHz",
                 mConverter? mConverter->InputConfig().Channels() : 0,
                 mConverter ? mConverter->InputConfig().Rate() : 0,
                 data->mChannels, data->mRate);

      DrainConverter();

      // mFramesParsed indicates the current playtime in frames at the current
      // input sampling rate. Recalculate it per the new sampling rate.
      if (mFramesParsed) {
        // We minimize overflow.
        uint32_t oldRate = mConverter->InputConfig().Rate();
        uint32_t newRate = data->mRate;
        int64_t major = mFramesParsed / oldRate;
        int64_t remainder = mFramesParsed % oldRate;
        CheckedInt64 result =
          CheckedInt64(remainder) * newRate / oldRate + major * oldRate;
        if (!result.isValid()) {
          NS_WARNING("Int overflow in DecodedAudioDataSink");
          mErrored = true;
          return;
        }
        mFramesParsed = result.value();
      }

      mConverter =
        MakeUnique<AudioConverter>(
          AudioConfig(data->mChannels, data->mRate),
          AudioConfig(mOutputChannels, mOutputRate));
    }

    // See if there's a gap in the audio. If there is, push silence into the
    // audio hardware, so we can play across the gap.
    // Calculate the timestamp of the next chunk of audio in numbers of
    // samples.
    CheckedInt64 sampleTime = UsecsToFrames(data->mTime - mStartTime,
                                            data->mRate);
    // Calculate the number of frames that have been pushed onto the audio hardware.
    CheckedInt64 missingFrames = sampleTime - mFramesParsed;

    if (!missingFrames.isValid()) {
      NS_WARNING("Int overflow in DecodedAudioDataSink");
      mErrored = true;
      return;
    }

    if (missingFrames.value() > AUDIO_FUZZ_FRAMES) {
      // The next audio packet begins some time after the end of the last packet
      // we pushed to the audio hardware. We must push silence into the audio
      // hardware so that the next audio packet begins playback at the correct
      // time.
      missingFrames = std::min<int64_t>(INT32_MAX, missingFrames.value());
      mFramesParsed += missingFrames.value();
      // We need to insert silence, first use drained frames if any.
      missingFrames -= DrainConverter(missingFrames.value());
      // Insert silence is still needed.
      if (missingFrames.value()) {
        AlignedAudioBuffer silenceData(missingFrames.value() * mOutputChannels);
        RefPtr<AudioData> silence = CreateAudioFromBuffer(Move(silenceData), data);
        if (silence) {
          mProcessedQueue.Push(silence);
        }
      }
    }

    mLastEndTime = data->GetEndTime();
    mFramesParsed += data->mFrames;

    if (mConverter->InputConfig() != mConverter->OutputConfig()) {
      AlignedAudioBuffer convertedData =
        mConverter->Process(AudioSampleBuffer(Move(data->mAudioData))).Forget();
      data = CreateAudioFromBuffer(Move(convertedData), data);
      if (!data) {
        continue;
      }
    }
    mProcessedQueue.Push(data);
    mLastProcessedPacket = Some(data);
  }
}

already_AddRefed<AudioData>
DecodedAudioDataSink::CreateAudioFromBuffer(AlignedAudioBuffer&& aBuffer,
                                            AudioData* aReference)
{
  uint32_t frames = aBuffer.Length() / mOutputChannels;
  if (!frames) {
    return nullptr;
  }
  CheckedInt64 duration = FramesToUsecs(frames, mOutputRate);
  if (!duration.isValid()) {
    NS_WARNING("Int overflow in DecodedAudioDataSink");
    mErrored = true;
    return nullptr;
  }
  RefPtr<AudioData> data =
    new AudioData(aReference->mOffset,
                  aReference->mTime,
                  duration.value(),
                  frames,
                  Move(aBuffer),
                  mOutputChannels,
                  mOutputRate);
  return data.forget();
}

uint32_t
DecodedAudioDataSink::DrainConverter(uint32_t aMaxFrames)
{
  MOZ_ASSERT(mProcessingThread->IsCurrentThreadIn());

  if (!mConverter || !mLastProcessedPacket) {
    // nothing to drain.
    return 0;
  }

  // To drain we simply provide an empty packet to the audio converter.
  AlignedAudioBuffer convertedData =
    mConverter->Process(AudioSampleBuffer(AlignedAudioBuffer())).Forget();

  uint32_t frames = convertedData.Length() / mOutputChannels;
  convertedData.SetLength(std::min(frames, aMaxFrames) * mOutputChannels);

  // We assume the start time of the drained data is just before the end of the
  // previous packet. Ultimately, the start time doesn't really matter, however
  // we do not want to trigger the gap detection in PopFrames.
  RefPtr<AudioData> data = CreateAudioFromBuffer(Move(convertedData),
                                                 mLastProcessedPacket.ref());
  mLastProcessedPacket.reset();

  if (!data) {
    return 0;
  }
  mProcessedQueue.Push(data);
  return data->mFrames;
}

} // namespace media
} // namespace mozilla
