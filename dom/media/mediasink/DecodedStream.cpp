/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AbstractThread.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/SyncRunnable.h"

#include "AudioSegment.h"
#include "DecodedStream.h"
#include "MediaData.h"
#include "MediaQueue.h"
#include "MediaStreamGraph.h"
#include "MediaStreamListener.h"
#include "OutputStreamManager.h"
#include "SharedBuffer.h"
#include "VideoSegment.h"
#include "VideoUtils.h"

namespace mozilla {

/*
 * A container class to make it easier to pass the playback info all the
 * way to DecodedStreamGraphListener from DecodedStream.
 */
struct PlaybackInfoInit {
  media::TimeUnit mStartTime;
  MediaInfo mInfo;
};

class DecodedStreamGraphListener : public MediaStreamListener {
public:
  DecodedStreamGraphListener(MediaStream* aStream,
                             MozPromiseHolder<GenericPromise>&& aPromise,
                             AbstractThread* aMainThread)
    : mMutex("DecodedStreamGraphListener::mMutex")
    , mStream(aStream)
    , mAbstractMainThread(aMainThread)
  {
    mFinishPromise = Move(aPromise);
  }

  void NotifyOutput(MediaStreamGraph* aGraph, GraphTime aCurrentTime) override
  {
    MutexAutoLock lock(mMutex);
    if (mStream) {
      int64_t t = mStream->StreamTimeToMicroseconds(
        mStream->GraphTimeToStreamTime(aCurrentTime));
      mOnOutput.Notify(t);
    }
  }

  void NotifyEvent(MediaStreamGraph* aGraph, MediaStreamGraphEvent event) override
  {
    if (event == MediaStreamGraphEvent::EVENT_FINISHED) {
      aGraph->DispatchToMainThreadAfterStreamStateUpdate(
        mAbstractMainThread,
        NewRunnableMethod(this, &DecodedStreamGraphListener::DoNotifyFinished));
    }
  }

  void DoNotifyFinished()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mFinishPromise.ResolveIfExists(true, __func__);
  }

  void Forget()
  {
    RefPtr<DecodedStreamGraphListener> self = this;
    mAbstractMainThread->Dispatch(NS_NewRunnableFunction([self] () {
      MOZ_ASSERT(NS_IsMainThread());
      self->mFinishPromise.ResolveIfExists(true, __func__);
    }));
    MutexAutoLock lock(mMutex);
    mStream = nullptr;
  }

  MediaEventSource<int64_t>& OnOutput()
  {
    return mOnOutput;
  }

private:
  MediaEventProducer<int64_t> mOnOutput;

  Mutex mMutex;
  // Members below are protected by mMutex.
  RefPtr<MediaStream> mStream;
  // Main thread only.
  MozPromiseHolder<GenericPromise> mFinishPromise;

  const RefPtr<AbstractThread> mAbstractMainThread;
};

static void
UpdateStreamSuspended(AbstractThread* aMainThread, MediaStream* aStream, bool aBlocking)
{
  if (NS_IsMainThread()) {
    if (aBlocking) {
      aStream->Suspend();
    } else {
      aStream->Resume();
    }
  } else {
    nsCOMPtr<nsIRunnable> r;
    if (aBlocking) {
      r = NewRunnableMethod(aStream, &MediaStream::Suspend);
    } else {
      r = NewRunnableMethod(aStream, &MediaStream::Resume);
    }
    aMainThread->Dispatch(r.forget());
  }
}

/*
 * All MediaStream-related data is protected by the decoder's monitor.
 * We have at most one DecodedStreamDaata per MediaDecoder. Its stream
 * is used as the input for each ProcessedMediaStream created by calls to
 * captureStream(UntilEnded). Seeking creates a new source stream, as does
 * replaying after the input as ended. In the latter case, the new source is
 * not connected to streams created by captureStreamUntilEnded.
 */
class DecodedStreamData {
public:
  DecodedStreamData(OutputStreamManager* aOutputStreamManager,
                    PlaybackInfoInit&& aInit,
                    MozPromiseHolder<GenericPromise>&& aPromise,
                    AbstractThread* aMainThread);
  ~DecodedStreamData();
  void SetPlaying(bool aPlaying);
  MediaEventSource<int64_t>& OnOutput();
  void Forget();
  nsCString GetDebugInfo();

  /* The following group of fields are protected by the decoder's monitor
   * and can be read or written on any thread.
   */
  // Count of audio frames written to the stream
  int64_t mAudioFramesWritten;
  // mNextVideoTime is the end timestamp for the last packet sent to the stream.
  // Therefore video packets starting at or after this time need to be copied
  // to the output stream.
  media::TimeUnit mNextVideoTime;
  media::TimeUnit mNextAudioTime;
  // The last video image sent to the stream. Useful if we need to replicate
  // the image.
  RefPtr<layers::Image> mLastVideoImage;
  gfx::IntSize mLastVideoImageDisplaySize;
  bool mHaveSentFinish;
  bool mHaveSentFinishAudio;
  bool mHaveSentFinishVideo;

  // The decoder is responsible for calling Destroy() on this stream.
  const RefPtr<SourceMediaStream> mStream;
  const RefPtr<DecodedStreamGraphListener> mListener;
  bool mPlaying;
  // True if we need to send a compensation video frame to ensure the
  // StreamTime going forward.
  bool mEOSVideoCompensation;

  const RefPtr<OutputStreamManager> mOutputStreamManager;
  const RefPtr<AbstractThread> mAbstractMainThread;
};

DecodedStreamData::DecodedStreamData(OutputStreamManager* aOutputStreamManager,
                                     PlaybackInfoInit&& aInit,
                                     MozPromiseHolder<GenericPromise>&& aPromise,
                                     AbstractThread* aMainThread)
  : mAudioFramesWritten(0)
  , mNextVideoTime(aInit.mStartTime)
  , mNextAudioTime(aInit.mStartTime)
  , mHaveSentFinish(false)
  , mHaveSentFinishAudio(false)
  , mHaveSentFinishVideo(false)
  , mStream(aOutputStreamManager->Graph()->CreateSourceStream(aMainThread))
  // DecodedStreamGraphListener will resolve this promise.
  , mListener(new DecodedStreamGraphListener(mStream, Move(aPromise), aMainThread))
  // mPlaying is initially true because MDSM won't start playback until playing
  // becomes true. This is consistent with the settings of AudioSink.
  , mPlaying(true)
  , mEOSVideoCompensation(false)
  , mOutputStreamManager(aOutputStreamManager)
  , mAbstractMainThread(aMainThread)
{
  mStream->AddListener(mListener);
  mOutputStreamManager->Connect(mStream);

  // Initialize tracks.
  if (aInit.mInfo.HasAudio()) {
    mStream->AddAudioTrack(aInit.mInfo.mAudio.mTrackId,
                           aInit.mInfo.mAudio.mRate,
                           0, new AudioSegment());
  }
  if (aInit.mInfo.HasVideo()) {
    mStream->AddTrack(aInit.mInfo.mVideo.mTrackId, 0, new VideoSegment());
  }
}

DecodedStreamData::~DecodedStreamData()
{
  mOutputStreamManager->Disconnect();
  mStream->Destroy();
}

MediaEventSource<int64_t>&
DecodedStreamData::OnOutput()
{
  return mListener->OnOutput();
}

void
DecodedStreamData::SetPlaying(bool aPlaying)
{
  if (mPlaying != aPlaying) {
    mPlaying = aPlaying;
    UpdateStreamSuspended(mAbstractMainThread, mStream, !mPlaying);
  }
}

void
DecodedStreamData::Forget()
{
  mListener->Forget();
}

nsCString
DecodedStreamData::GetDebugInfo()
{
  return nsPrintfCString(
    "DecodedStreamData=%p mPlaying=%d mAudioFramesWritten=%" PRId64
    " mNextAudioTime=%" PRId64 " mNextVideoTime=%" PRId64 " mHaveSentFinish=%d "
    "mHaveSentFinishAudio=%d mHaveSentFinishVideo=%d",
    this, mPlaying, mAudioFramesWritten, mNextAudioTime.ToMicroseconds(),
    mNextVideoTime.ToMicroseconds(), mHaveSentFinish, mHaveSentFinishAudio,
    mHaveSentFinishVideo);
}

DecodedStream::DecodedStream(AbstractThread* aOwnerThread,
                             AbstractThread* aMainThread,
                             MediaQueue<AudioData>& aAudioQueue,
                             MediaQueue<VideoData>& aVideoQueue,
                             OutputStreamManager* aOutputStreamManager,
                             const bool& aSameOrigin,
                             const PrincipalHandle& aPrincipalHandle)
  : mOwnerThread(aOwnerThread)
  , mAbstractMainThread(aMainThread)
  , mOutputStreamManager(aOutputStreamManager)
  , mPlaying(false)
  , mSameOrigin(aSameOrigin)
  , mPrincipalHandle(aPrincipalHandle)
  , mAudioQueue(aAudioQueue)
  , mVideoQueue(aVideoQueue)
{
}

DecodedStream::~DecodedStream()
{
  MOZ_ASSERT(mStartTime.isNothing(), "playback should've ended.");
}

const media::MediaSink::PlaybackParams&
DecodedStream::GetPlaybackParams() const
{
  AssertOwnerThread();
  return mParams;
}

void
DecodedStream::SetPlaybackParams(const PlaybackParams& aParams)
{
  AssertOwnerThread();
  mParams = aParams;
}

RefPtr<GenericPromise>
DecodedStream::OnEnded(TrackType aType)
{
  AssertOwnerThread();
  MOZ_ASSERT(mStartTime.isSome());

  if (aType == TrackInfo::kAudioTrack && mInfo.HasAudio()) {
    // TODO: we should return a promise which is resolved when the audio track
    // is finished. For now this promise is resolved when the whole stream is
    // finished.
    return mFinishPromise;
  } else if (aType == TrackInfo::kVideoTrack && mInfo.HasVideo()) {
    return mFinishPromise;
  }
  return nullptr;
}

void
DecodedStream::Start(const media::TimeUnit& aStartTime, const MediaInfo& aInfo)
{
  AssertOwnerThread();
  MOZ_ASSERT(mStartTime.isNothing(), "playback already started.");

  mStartTime.emplace(aStartTime);
  mLastOutputTime = media::TimeUnit::Zero();
  mInfo = aInfo;
  mPlaying = true;
  ConnectListener();

  class R : public Runnable {
    typedef MozPromiseHolder<GenericPromise> Promise;
  public:
    R(PlaybackInfoInit&& aInit, Promise&& aPromise,
      OutputStreamManager* aManager, AbstractThread* aMainThread)
      : Runnable("CreateDecodedStreamData")
      , mInit(Move(aInit))
      , mOutputStreamManager(aManager)
      , mAbstractMainThread(aMainThread)
    {
      mPromise = Move(aPromise);
    }
    NS_IMETHOD Run() override
    {
      MOZ_ASSERT(NS_IsMainThread());
      // No need to create a source stream when there are no output streams. This
      // happens when RemoveOutput() is called immediately after StartPlayback().
      if (!mOutputStreamManager->Graph()) {
        // Resolve the promise to indicate the end of playback.
        mPromise.Resolve(true, __func__);
        return NS_OK;
      }
      mData = MakeUnique<DecodedStreamData>(
        mOutputStreamManager, Move(mInit), Move(mPromise), mAbstractMainThread);
      return NS_OK;
    }
    UniquePtr<DecodedStreamData> ReleaseData()
    {
      return Move(mData);
    }
  private:
    PlaybackInfoInit mInit;
    Promise mPromise;
    RefPtr<OutputStreamManager> mOutputStreamManager;
    UniquePtr<DecodedStreamData> mData;
    const RefPtr<AbstractThread> mAbstractMainThread;
  };

  MozPromiseHolder<GenericPromise> promise;
  mFinishPromise = promise.Ensure(__func__);
  PlaybackInfoInit init {
    aStartTime, aInfo
  };
  nsCOMPtr<nsIRunnable> r =
    new R(Move(init), Move(promise), mOutputStreamManager, mAbstractMainThread);
  SyncRunnable::DispatchToThread(
    SystemGroup::EventTargetFor(mozilla::TaskCategory::Other), r);
  mData = static_cast<R*>(r.get())->ReleaseData();

  if (mData) {
    mOutputListener = mData->OnOutput().Connect(
      mOwnerThread, this, &DecodedStream::NotifyOutput);
    mData->SetPlaying(mPlaying);
    SendData();
  }
}

void
DecodedStream::Stop()
{
  AssertOwnerThread();
  MOZ_ASSERT(mStartTime.isSome(), "playback not started.");

  mStartTime.reset();
  DisconnectListener();
  mFinishPromise = nullptr;

  // Clear mData immediately when this playback session ends so we won't
  // send data to the wrong stream in SendData() in next playback session.
  DestroyData(Move(mData));
}

bool
DecodedStream::IsStarted() const
{
  AssertOwnerThread();
  return mStartTime.isSome();
}

bool
DecodedStream::IsPlaying() const
{
  AssertOwnerThread();
  return IsStarted() && mPlaying;
}

void
DecodedStream::DestroyData(UniquePtr<DecodedStreamData> aData)
{
  AssertOwnerThread();

  if (!aData) {
    return;
  }

  mOutputListener.Disconnect();

  DecodedStreamData* data = aData.release();
  data->Forget();
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([=] () {
    delete data;
  });
  mAbstractMainThread->Dispatch(r.forget());
}

void
DecodedStream::SetPlaying(bool aPlaying)
{
  AssertOwnerThread();

  // Resume/pause matters only when playback started.
  if (mStartTime.isNothing()) {
    return;
  }

  mPlaying = aPlaying;
  if (mData) {
    mData->SetPlaying(aPlaying);
  }
}

void
DecodedStream::SetVolume(double aVolume)
{
  AssertOwnerThread();
  mParams.mVolume = aVolume;
}

void
DecodedStream::SetPlaybackRate(double aPlaybackRate)
{
  AssertOwnerThread();
  mParams.mPlaybackRate = aPlaybackRate;
}

void
DecodedStream::SetPreservesPitch(bool aPreservesPitch)
{
  AssertOwnerThread();
  mParams.mPreservesPitch = aPreservesPitch;
}

static void
SendStreamAudio(DecodedStreamData* aStream, const media::TimeUnit& aStartTime,
                AudioData* aData, AudioSegment* aOutput, uint32_t aRate,
                const PrincipalHandle& aPrincipalHandle)
{
  // The amount of audio frames that is used to fuzz rounding errors.
  static const int64_t AUDIO_FUZZ_FRAMES = 1;

  MOZ_ASSERT(aData);
  AudioData* audio = aData;
  // This logic has to mimic AudioSink closely to make sure we write
  // the exact same silences
  CheckedInt64 audioWrittenOffset = aStream->mAudioFramesWritten
    + TimeUnitToFrames(aStartTime, aRate);
  CheckedInt64 frameOffset = UsecsToFrames(audio->mTime, aRate);

  if (!audioWrittenOffset.isValid() ||
      !frameOffset.isValid() ||
      // ignore packet that we've already processed
      audio->GetEndTime() <= aStream->mNextAudioTime) {
    return;
  }

  if (audioWrittenOffset.value() + AUDIO_FUZZ_FRAMES < frameOffset.value()) {
    int64_t silentFrames = frameOffset.value() - audioWrittenOffset.value();
    // Write silence to catch up
    AudioSegment silence;
    silence.InsertNullDataAtStart(silentFrames);
    aStream->mAudioFramesWritten += silentFrames;
    audioWrittenOffset += silentFrames;
    aOutput->AppendFrom(&silence);
  }

  // Always write the whole sample without truncation to be consistent with
  // DecodedAudioDataSink::PlayFromAudioQueue()
  audio->EnsureAudioBuffer();
  RefPtr<SharedBuffer> buffer = audio->mAudioBuffer;
  AudioDataValue* bufferData = static_cast<AudioDataValue*>(buffer->Data());
  AutoTArray<const AudioDataValue*, 2> channels;
  for (uint32_t i = 0; i < audio->mChannels; ++i) {
    channels.AppendElement(bufferData + i * audio->mFrames);
  }
  aOutput->AppendFrames(buffer.forget(), channels, audio->mFrames, aPrincipalHandle);
  aStream->mAudioFramesWritten += audio->mFrames;

  aStream->mNextAudioTime = audio->GetEndTime();
}

void
DecodedStream::SendAudio(double aVolume, bool aIsSameOrigin,
                         const PrincipalHandle& aPrincipalHandle)
{
  AssertOwnerThread();

  if (!mInfo.HasAudio()) {
    return;
  }

  AudioSegment output;
  uint32_t rate = mInfo.mAudio.mRate;
  AutoTArray<RefPtr<AudioData>,10> audio;
  TrackID audioTrackId = mInfo.mAudio.mTrackId;
  SourceMediaStream* sourceStream = mData->mStream;

  // It's OK to hold references to the AudioData because AudioData
  // is ref-counted.
  mAudioQueue.GetElementsAfter(mData->mNextAudioTime, &audio);
  for (uint32_t i = 0; i < audio.Length(); ++i) {
    SendStreamAudio(mData.get(), mStartTime.ref(), audio[i], &output, rate,
                    aPrincipalHandle);
  }

  output.ApplyVolume(aVolume);

  if (!aIsSameOrigin) {
    output.ReplaceWithDisabled();
  }

  // |mNextAudioTime| is updated as we process each audio sample in
  // SendStreamAudio(). This is consistent with how |mNextVideoTime|
  // is updated for video samples.
  if (output.GetDuration() > 0) {
    sourceStream->AppendToTrack(audioTrackId, &output);
  }

  if (mAudioQueue.IsFinished() && !mData->mHaveSentFinishAudio) {
    sourceStream->EndTrack(audioTrackId);
    mData->mHaveSentFinishAudio = true;
  }
}

static void
WriteVideoToMediaStream(MediaStream* aStream,
                        layers::Image* aImage,
                        const media::TimeUnit& aEnd,
                        const media::TimeUnit& aStart,
                        const mozilla::gfx::IntSize& aIntrinsicSize,
                        const TimeStamp& aTimeStamp,
                        VideoSegment* aOutput,
                        const PrincipalHandle& aPrincipalHandle)
{
  RefPtr<layers::Image> image = aImage;
  auto end = aStream->MicrosecondsToStreamTimeRoundDown(aEnd.ToMicroseconds());
  auto start = aStream->MicrosecondsToStreamTimeRoundDown(aStart.ToMicroseconds());
  StreamTime duration = end - start;
  aOutput->AppendFrame(image.forget(), duration, aIntrinsicSize,
                       aPrincipalHandle, false, aTimeStamp);
}

static bool
ZeroDurationAtLastChunk(VideoSegment& aInput)
{
  // Get the last video frame's start time in VideoSegment aInput.
  // If the start time is equal to the duration of aInput, means the last video
  // frame's duration is zero.
  StreamTime lastVideoStratTime;
  aInput.GetLastFrame(&lastVideoStratTime);
  return lastVideoStratTime == aInput.GetDuration();
}

void
DecodedStream::SendVideo(bool aIsSameOrigin, const PrincipalHandle& aPrincipalHandle)
{
  AssertOwnerThread();

  if (!mInfo.HasVideo()) {
    return;
  }

  VideoSegment output;
  TrackID videoTrackId = mInfo.mVideo.mTrackId;
  AutoTArray<RefPtr<VideoData>, 10> video;
  SourceMediaStream* sourceStream = mData->mStream;

  // It's OK to hold references to the VideoData because VideoData
  // is ref-counted.
  mVideoQueue.GetElementsAfter(mData->mNextVideoTime, &video);

  // tracksStartTimeStamp might be null when the SourceMediaStream not yet
  // be added to MediaStreamGraph.
  TimeStamp tracksStartTimeStamp = sourceStream->GetStreamTracksStrartTimeStamp();
  if (tracksStartTimeStamp.IsNull()) {
    tracksStartTimeStamp = TimeStamp::Now();
  }

  for (uint32_t i = 0; i < video.Length(); ++i) {
    VideoData* v = video[i];

    if (mData->mNextVideoTime.ToMicroseconds() < v->mTime) {
      // Write last video frame to catch up. mLastVideoImage can be null here
      // which is fine, it just means there's no video.

      // TODO: |mLastVideoImage| should come from the last image rendered
      // by the state machine. This will avoid the black frame when capture
      // happens in the middle of playback (especially in th middle of a
      // video frame). E.g. if we have a video frame that is 30 sec long
      // and capture happens at 15 sec, we'll have to append a black frame
      // that is 15 sec long.
      WriteVideoToMediaStream(sourceStream, mData->mLastVideoImage,
        FromMicroseconds(v->mTime),
        mData->mNextVideoTime, mData->mLastVideoImageDisplaySize,
        tracksStartTimeStamp + TimeDuration::FromMicroseconds(v->mTime),
        &output, aPrincipalHandle);
      mData->mNextVideoTime = FromMicroseconds(v->mTime);
    }

    if (mData->mNextVideoTime < v->GetEndTime()) {
      WriteVideoToMediaStream(sourceStream, v->mImage, v->GetEndTime(),
        mData->mNextVideoTime, v->mDisplay,
        tracksStartTimeStamp + v->GetEndTime().ToTimeDuration(),
        &output, aPrincipalHandle);
      mData->mNextVideoTime = v->GetEndTime();
      mData->mLastVideoImage = v->mImage;
      mData->mLastVideoImageDisplaySize = v->mDisplay;
    }
  }

  // Check the output is not empty.
  if (output.GetLastFrame()) {
    mData->mEOSVideoCompensation = ZeroDurationAtLastChunk(output);
  }

  if (!aIsSameOrigin) {
    output.ReplaceWithDisabled();
  }

  if (output.GetDuration() > 0) {
    sourceStream->AppendToTrack(videoTrackId, &output);
  }

  if (mVideoQueue.IsFinished() && !mData->mHaveSentFinishVideo) {
    if (mData->mEOSVideoCompensation) {
      VideoSegment endSegment;
      // Calculate the deviation clock time from DecodedStream.
      auto deviation = FromMicroseconds(sourceStream->StreamTimeToMicroseconds(1));
      WriteVideoToMediaStream(sourceStream, mData->mLastVideoImage,
        mData->mNextVideoTime + deviation, mData->mNextVideoTime,
        mData->mLastVideoImageDisplaySize,
        tracksStartTimeStamp + (mData->mNextVideoTime + deviation).ToTimeDuration(),
        &endSegment, aPrincipalHandle);
      mData->mNextVideoTime += deviation;
      MOZ_ASSERT(endSegment.GetDuration() > 0);
      if (!aIsSameOrigin) {
        endSegment.ReplaceWithDisabled();
      }
      sourceStream->AppendToTrack(videoTrackId, &endSegment);
    }
    sourceStream->EndTrack(videoTrackId);
    mData->mHaveSentFinishVideo = true;
  }
}

void
DecodedStream::AdvanceTracks()
{
  AssertOwnerThread();

  StreamTime endPosition = 0;

  if (mInfo.HasAudio()) {
    StreamTime audioEnd = mData->mStream->TicksToTimeRoundDown(
        mInfo.mAudio.mRate, mData->mAudioFramesWritten);
    endPosition = std::max(endPosition, audioEnd);
  }

  if (mInfo.HasVideo()) {
    StreamTime videoEnd = mData->mStream->MicrosecondsToStreamTimeRoundDown(
      (mData->mNextVideoTime - mStartTime.ref()).ToMicroseconds());
    endPosition = std::max(endPosition, videoEnd);
  }

  if (!mData->mHaveSentFinish) {
    mData->mStream->AdvanceKnownTracksTime(endPosition);
  }
}

void
DecodedStream::SendData()
{
  AssertOwnerThread();
  MOZ_ASSERT(mStartTime.isSome(), "Must be called after StartPlayback()");

  // Not yet created on the main thread. MDSM will try again later.
  if (!mData) {
    return;
  }

  // Nothing to do when the stream is finished.
  if (mData->mHaveSentFinish) {
    return;
  }

  SendAudio(mParams.mVolume, mSameOrigin, mPrincipalHandle);
  SendVideo(mSameOrigin, mPrincipalHandle);
  AdvanceTracks();

  bool finished = (!mInfo.HasAudio() || mAudioQueue.IsFinished()) &&
                  (!mInfo.HasVideo() || mVideoQueue.IsFinished());

  if (finished && !mData->mHaveSentFinish) {
    mData->mHaveSentFinish = true;
    mData->mStream->Finish();
  }
}

media::TimeUnit
DecodedStream::GetEndTime(TrackType aType) const
{
  AssertOwnerThread();
  if (aType == TrackInfo::kAudioTrack && mInfo.HasAudio() && mData) {
    auto t = mStartTime.ref() + FramesToTimeUnit(
      mData->mAudioFramesWritten, mInfo.mAudio.mRate);
    if (t.IsValid()) {
      return t;
    }
  } else if (aType == TrackInfo::kVideoTrack && mData) {
    return mData->mNextVideoTime;
  }
  return media::TimeUnit::Zero();
}

media::TimeUnit
DecodedStream::GetPosition(TimeStamp* aTimeStamp) const
{
  AssertOwnerThread();
  // This is only called after MDSM starts playback. So mStartTime is
  // guaranteed to be something.
  MOZ_ASSERT(mStartTime.isSome());
  if (aTimeStamp) {
    *aTimeStamp = TimeStamp::Now();
  }
  return mStartTime.ref() + mLastOutputTime;
}

void
DecodedStream::NotifyOutput(int64_t aTime)
{
  AssertOwnerThread();
  mLastOutputTime = FromMicroseconds(aTime);
  int64_t currentTime = GetPosition().ToMicroseconds();

  // Remove audio samples that have been played by MSG from the queue.
  RefPtr<AudioData> a = mAudioQueue.PeekFront();
  for (; a && a->mTime < currentTime;) {
    RefPtr<AudioData> releaseMe = mAudioQueue.PopFront();
    a = mAudioQueue.PeekFront();
  }
}

void
DecodedStream::ConnectListener()
{
  AssertOwnerThread();

  mAudioPushListener = mAudioQueue.PushEvent().Connect(
    mOwnerThread, this, &DecodedStream::SendData);
  mAudioFinishListener = mAudioQueue.FinishEvent().Connect(
    mOwnerThread, this, &DecodedStream::SendData);
  mVideoPushListener = mVideoQueue.PushEvent().Connect(
    mOwnerThread, this, &DecodedStream::SendData);
  mVideoFinishListener = mVideoQueue.FinishEvent().Connect(
    mOwnerThread, this, &DecodedStream::SendData);
}

void
DecodedStream::DisconnectListener()
{
  AssertOwnerThread();

  mAudioPushListener.Disconnect();
  mVideoPushListener.Disconnect();
  mAudioFinishListener.Disconnect();
  mVideoFinishListener.Disconnect();
}

nsCString
DecodedStream::GetDebugInfo()
{
  AssertOwnerThread();
  int64_t startTime = mStartTime.isSome() ? mStartTime->ToMicroseconds() : -1;
  return nsPrintfCString(
    "DecodedStream=%p mStartTime=%" PRId64 " mLastOutputTime=%" PRId64 " mPlaying=%d mData=%p",
    this, startTime, mLastOutputTime.ToMicroseconds(), mPlaying, mData.get())
    + (mData ? nsCString("\n") + mData->GetDebugInfo() : nsCString());
}

} // namespace mozilla
