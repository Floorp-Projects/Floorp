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
#include "MediaPrefs.h"

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

// Amount of audio frames we will be processing ahead of use
static const int32_t LOW_AUDIO_USECS = 300000;

DecodedAudioDataSink::DecodedAudioDataSink(AbstractThread* aThread,
                                           MediaQueue<MediaData>& aAudioQueue,
                                           int64_t aStartTime,
                                           const AudioInfo& aInfo,
                                           dom::AudioChannel aChannel)
  : AudioSink(aAudioQueue)
  , mStartTime(aStartTime)
  , mLastGoodPosition(0)
  , mInfo(aInfo)
  , mChannel(aChannel)
  , mPlaying(true)
  , mMonitor("DecodedAudioDataSink")
  , mWritten(0)
  , mErrored(false)
  , mPlaybackComplete(false)
  , mOwnerThread(aThread)
  , mProcessedQueueLength(0)
  , mFramesParsed(0)
  , mLastEndTime(0)
  , mIsAudioDataAudible(false)
{
  bool resampling = MediaPrefs::AudioSinkResampling();

  if (resampling) {
    mOutputRate = MediaPrefs::AudioSinkResampleRate();
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
  MOZ_DIAGNOSTIC_ASSERT(mOutputRate, "output rate can't be 0.");

  bool monoAudioEnabled = MediaPrefs::MonoAudio();

  mOutputChannels = monoAudioEnabled
    ? 1 : (MediaPrefs::AudioSinkForceStereo() ? 2 : mInfo.mChannels);
}

DecodedAudioDataSink::~DecodedAudioDataSink()
{
}

RefPtr<GenericPromise>
DecodedAudioDataSink::Init(const PlaybackParams& aParams)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());

  mAudioQueueListener = mAudioQueue.PushEvent().Connect(
    mOwnerThread, this, &DecodedAudioDataSink::OnAudioPushed);
  mAudioQueueFinishListener = mAudioQueue.FinishEvent().Connect(
    mOwnerThread, this, &DecodedAudioDataSink::NotifyAudioNeeded);
  mProcessedQueueListener = mProcessedQueue.PopEvent().Connect(
    mOwnerThread, this, &DecodedAudioDataSink::OnAudioPopped);

  // To ensure at least one audio packet will be popped from AudioQueue and
  // ready to be played.
  NotifyAudioNeeded();
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
  int64_t total;
  {
    MonitorAutoLock mon(mMonitor);
    total = mWritten + (mCursor.get() ? mCursor->Available() : 0);
  }
  return mProcessedQueue.GetSize() ||
         (mAudioStream && mAudioStream->GetPositionInFrames() + 1 < total);
}

void
DecodedAudioDataSink::Shutdown()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());

  mAudioQueueListener.Disconnect();
  mAudioQueueFinishListener.Disconnect();
  mProcessedQueueListener.Disconnect();

  if (mAudioStream) {
    mAudioStream->Shutdown();
    mAudioStream = nullptr;
  }
  mProcessedQueue.Reset();
  mProcessedQueue.Finish();
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
  if (!mAudioStream || mPlaying == aPlaying || mPlaybackComplete) {
    return;
  }
  // pause/resume AudioStream as necessary.
  if (!aPlaying) {
    mAudioStream->Pause();
  } else if (aPlaying) {
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
  int64_t written;
  {
    MonitorAutoLock mon(mMonitor);
    written = mWritten;
  }
  CheckedInt64 playedUsecs = FramesToUsecs(written, mOutputRate) + mStartTime;
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

  bool needPopping = false;
  if (!mCurrentData) {
    // No data in the queue. Return an empty chunk.
    if (!mProcessedQueue.GetSize()) {
      return MakeUnique<Chunk>();
    }

    // We need to update our values prior popping the processed queue in
    // order to prevent the pop event to fire too early (prior
    // mProcessedQueueLength being updated) or prevent HasUnplayedFrames
    // to incorrectly return true during the time interval betweeen the
    // when mProcessedQueue is read and mWritten is updated.
    needPopping = true;
    mCurrentData = mProcessedQueue.PeekFront();
    {
      MonitorAutoLock mon(mMonitor);
      mCursor = MakeUnique<AudioBufferCursor>(mCurrentData->mAudioData.get(),
                                              mCurrentData->mChannels,
                                              mCurrentData->mFrames);
    }
    MOZ_ASSERT(mCurrentData->mFrames > 0);
    mProcessedQueueLength -=
      FramesToUsecs(mCurrentData->mFrames, mOutputRate).value();
  }

  auto framesToPop = std::min(aFrames, mCursor->Available());

  SINK_LOG_V("playing audio at time=%lld offset=%u length=%u",
             mCurrentData->mTime, mCurrentData->mFrames - mCursor->Available(), framesToPop);

  UniquePtr<AudioStream::Chunk> chunk =
    MakeUnique<Chunk>(mCurrentData, framesToPop, mCursor->Ptr());

  {
    MonitorAutoLock mon(mMonitor);
    mWritten += framesToPop;
    mCursor->Advance(framesToPop);
  }

  // All frames are popped. Reset mCurrentData so we can pop new elements from
  // the audio queue in next calls to PopFrames().
  if (!mCursor->Available()) {
    mCurrentData = nullptr;
  }

  if (needPopping) {
    // We can now safely pop the audio packet from the processed queue.
    // This will fire the popped event, triggering a call to NotifyAudioNeeded.
    RefPtr<AudioData> releaseMe = mProcessedQueue.PopFront();
    CheckIsAudible(releaseMe);
  }

  return chunk;
}

bool
DecodedAudioDataSink::Ended() const
{
  // Return true when error encountered so AudioStream can start draining.
  return mProcessedQueue.IsFinished() || mErrored;
}

void
DecodedAudioDataSink::Drained()
{
  SINK_LOG("Drained");
  mPlaybackComplete = true;
  mEndPromise.ResolveIfExists(true, __func__);
}

void
DecodedAudioDataSink::CheckIsAudible(const AudioData* aData)
{
  MOZ_ASSERT(aData);

  bool isAudible = aData->IsAudible();
  if (isAudible != mIsAudioDataAudible) {
    mIsAudioDataAudible = isAudible;
    mAudibleEvent.Notify(mIsAudioDataAudible);
  }
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
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn(),
             "Not called from the owner's thread");

  // Always ensure we have two processed frames pending to allow for processing
  // latency.
  while (AudioQueue().GetSize() && (AudioQueue().IsFinished() ||
                                    mProcessedQueueLength < LOW_AUDIO_USECS ||
                                    mProcessedQueue.GetSize() < 2)) {
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
        CheckedInt64 result = SaferMultDiv(mFramesParsed, newRate, oldRate);
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

      // We need to calculate how many frames are missing at the output rate.
      missingFrames =
        SaferMultDiv(missingFrames.value(), mOutputRate, data->mRate);
      if (!missingFrames.isValid()) {
        NS_WARNING("Int overflow in DecodedAudioDataSink");
        mErrored = true;
        return;
      }

      // We need to insert silence, first use drained frames if any.
      missingFrames -= DrainConverter(missingFrames.value());
      // Insert silence if still needed.
      if (missingFrames.value()) {
        AlignedAudioBuffer silenceData(missingFrames.value() * mOutputChannels);
        if (!silenceData) {
          NS_WARNING("OOM in DecodedAudioDataSink");
          mErrored = true;
          return;
        }
        RefPtr<AudioData> silence = CreateAudioFromBuffer(Move(silenceData), data);
        PushProcessedAudio(silence);
      }
    }

    mLastEndTime = data->GetEndTime();
    mFramesParsed += data->mFrames;

    if (mConverter->InputConfig() != mConverter->OutputConfig()) {
      AlignedAudioBuffer convertedData =
        mConverter->Process(AudioSampleBuffer(Move(data->mAudioData))).Forget();
      data = CreateAudioFromBuffer(Move(convertedData), data);
    }
    if (PushProcessedAudio(data)) {
      mLastProcessedPacket = Some(data);
    }
  }

  if (AudioQueue().IsFinished()) {
    // We have reached the end of the data, drain the resampler.
    DrainConverter();
    mProcessedQueue.Finish();
  }
}

uint32_t
DecodedAudioDataSink::PushProcessedAudio(AudioData* aData)
{
  if (!aData || !aData->mFrames) {
    return 0;
  }
  mProcessedQueue.Push(aData);
  mProcessedQueueLength += FramesToUsecs(aData->mFrames, mOutputRate).value();
  return aData->mFrames;
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
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());

  if (!mConverter || !mLastProcessedPacket || !aMaxFrames) {
    // nothing to drain.
    return 0;
  }

  RefPtr<AudioData> lastPacket = mLastProcessedPacket.ref();
  mLastProcessedPacket.reset();

  // To drain we simply provide an empty packet to the audio converter.
  AlignedAudioBuffer convertedData =
    mConverter->Process(AudioSampleBuffer(AlignedAudioBuffer())).Forget();

  uint32_t frames = convertedData.Length() / mOutputChannels;
  if (!convertedData.SetLength(std::min(frames, aMaxFrames) * mOutputChannels)) {
    // This can never happen as we were reducing the length of convertData.
    mErrored = true;
    return 0;
  }

  RefPtr<AudioData> data =
    CreateAudioFromBuffer(Move(convertedData), lastPacket);
  if (!data) {
    return 0;
  }
  mProcessedQueue.Push(data);
  return data->mFrames;
}

} // namespace media
} // namespace mozilla
