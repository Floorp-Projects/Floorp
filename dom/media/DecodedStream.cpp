/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioSegment.h"
#include "DecodedStream.h"
#include "MediaData.h"
#include "MediaQueue.h"
#include "MediaStreamGraph.h"
#include "SharedBuffer.h"
#include "VideoSegment.h"
#include "VideoUtils.h"

namespace mozilla {

class DecodedStreamGraphListener : public MediaStreamListener {
  typedef MediaStreamListener::MediaStreamGraphEvent MediaStreamGraphEvent;
public:
  DecodedStreamGraphListener(MediaStream* aStream,
                             MozPromiseHolder<GenericPromise>&& aPromise)
    : mMutex("DecodedStreamGraphListener::mMutex")
    , mStream(aStream)
    , mLastOutputTime(aStream->StreamTimeToMicroseconds(aStream->GetCurrentTime()))
    , mStreamFinishedOnMainThread(false)
  {
    mFinishPromise = Move(aPromise);
  }

  void NotifyOutput(MediaStreamGraph* aGraph, GraphTime aCurrentTime) override
  {
    MutexAutoLock lock(mMutex);
    if (mStream) {
      mLastOutputTime = mStream->StreamTimeToMicroseconds(mStream->GraphTimeToStreamTime(aCurrentTime));
    }
  }

  void NotifyEvent(MediaStreamGraph* aGraph, MediaStreamGraphEvent event) override
  {
    if (event == EVENT_FINISHED) {
      nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethod(this, &DecodedStreamGraphListener::DoNotifyFinished);
      aGraph->DispatchToMainThreadAfterStreamStateUpdate(event.forget());
    }
  }

  void DoNotifyFinished()
  {
    mFinishPromise.ResolveIfExists(true, __func__);
    MutexAutoLock lock(mMutex);
    mStreamFinishedOnMainThread = true;
  }

  int64_t GetLastOutputTime()
  {
    MutexAutoLock lock(mMutex);
    return mLastOutputTime;
  }

  void Forget()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mFinishPromise.ResolveIfExists(true, __func__);
    MutexAutoLock lock(mMutex);
    mStream = nullptr;
  }

  bool IsFinishedOnMainThread()
  {
    MutexAutoLock lock(mMutex);
    return mStreamFinishedOnMainThread;
  }

private:
  Mutex mMutex;
  // Members below are protected by mMutex.
  nsRefPtr<MediaStream> mStream;
  int64_t mLastOutputTime; // microseconds
  bool mStreamFinishedOnMainThread;
  // Main thread only.
  MozPromiseHolder<GenericPromise> mFinishPromise;
};

static void
UpdateStreamBlocking(MediaStream* aStream, bool aBlocking)
{
  int32_t delta = aBlocking ? 1 : -1;
  if (NS_IsMainThread()) {
    aStream->ChangeExplicitBlockerCount(delta);
  } else {
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethodWithArg<int32_t>(
      aStream, &MediaStream::ChangeExplicitBlockerCount, delta);
    AbstractThread::MainThread()->Dispatch(r.forget());
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
  DecodedStreamData(SourceMediaStream* aStream, bool aPlaying,
                    MozPromiseHolder<GenericPromise>&& aPromise);
  ~DecodedStreamData();
  bool IsFinished() const;
  int64_t GetPosition() const;
  void SetPlaying(bool aPlaying);

  /* The following group of fields are protected by the decoder's monitor
   * and can be read or written on any thread.
   */
  // Count of audio frames written to the stream
  int64_t mAudioFramesWritten;
  // mNextVideoTime is the end timestamp for the last packet sent to the stream.
  // Therefore video packets starting at or after this time need to be copied
  // to the output stream.
  int64_t mNextVideoTime; // microseconds
  int64_t mNextAudioTime; // microseconds
  // The last video image sent to the stream. Useful if we need to replicate
  // the image.
  nsRefPtr<layers::Image> mLastVideoImage;
  gfx::IntSize mLastVideoImageDisplaySize;
  // This is set to true when the stream is initialized (audio and
  // video tracks added).
  bool mStreamInitialized;
  bool mHaveSentFinish;
  bool mHaveSentFinishAudio;
  bool mHaveSentFinishVideo;

  // The decoder is responsible for calling Destroy() on this stream.
  const nsRefPtr<SourceMediaStream> mStream;
  nsRefPtr<DecodedStreamGraphListener> mListener;
  bool mPlaying;
  // True if we need to send a compensation video frame to ensure the
  // StreamTime going forward.
  bool mEOSVideoCompensation;
};

DecodedStreamData::DecodedStreamData(SourceMediaStream* aStream, bool aPlaying,
                                     MozPromiseHolder<GenericPromise>&& aPromise)
  : mAudioFramesWritten(0)
  , mNextVideoTime(-1)
  , mNextAudioTime(-1)
  , mStreamInitialized(false)
  , mHaveSentFinish(false)
  , mHaveSentFinishAudio(false)
  , mHaveSentFinishVideo(false)
  , mStream(aStream)
  , mPlaying(aPlaying)
  , mEOSVideoCompensation(false)
{
  // DecodedStreamGraphListener will resolve this promise.
  mListener = new DecodedStreamGraphListener(mStream, Move(aPromise));
  mStream->AddListener(mListener);

  // Block the stream if we are not playing.
  if (!aPlaying) {
    UpdateStreamBlocking(mStream, true);
  }
}

DecodedStreamData::~DecodedStreamData()
{
  mListener->Forget();
  mStream->Destroy();
}

bool
DecodedStreamData::IsFinished() const
{
  return mListener->IsFinishedOnMainThread();
}

int64_t
DecodedStreamData::GetPosition() const
{
  return mListener->GetLastOutputTime();
}

void
DecodedStreamData::SetPlaying(bool aPlaying)
{
  if (mPlaying != aPlaying) {
    mPlaying = aPlaying;
    UpdateStreamBlocking(mStream, !mPlaying);
  }
}

class OutputStreamListener : public MediaStreamListener {
  typedef MediaStreamListener::MediaStreamGraphEvent MediaStreamGraphEvent;
public:
  explicit OutputStreamListener(OutputStreamData* aOwner) : mOwner(aOwner) {}

  void NotifyEvent(MediaStreamGraph* aGraph, MediaStreamGraphEvent event) override
  {
    if (event == EVENT_FINISHED) {
      nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(
        this, &OutputStreamListener::DoNotifyFinished);
      aGraph->DispatchToMainThreadAfterStreamStateUpdate(r.forget());
    }
  }

  void Forget()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mOwner = nullptr;
  }

private:
  void DoNotifyFinished()
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (mOwner) {
      // Remove the finished stream so it won't block the decoded stream.
      mOwner->Remove();
    }
  }

  // Main thread only
  OutputStreamData* mOwner;
};

OutputStreamData::~OutputStreamData()
{
  MOZ_ASSERT(NS_IsMainThread());
  mListener->Forget();
  // Break the connection to the input stream if necessary.
  if (mPort) {
    mPort->Destroy();
  }
}

void
OutputStreamData::Init(OutputStreamManager* aOwner, ProcessedMediaStream* aStream)
{
  mOwner = aOwner;
  mStream = aStream;
  mListener = new OutputStreamListener(this);
  aStream->AddListener(mListener);
}

void
OutputStreamData::Connect(MediaStream* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mPort, "Already connected?");
  MOZ_ASSERT(!mStream->IsDestroyed(), "Can't connect a destroyed stream.");

  // The output stream must stay in sync with the input stream, so if
  // either stream is blocked, we block the other.
  mPort = mStream->AllocateInputPort(aStream,
    MediaInputPort::FLAG_BLOCK_INPUT | MediaInputPort::FLAG_BLOCK_OUTPUT);
  // Unblock the output stream now. The input stream is responsible for
  // controlling blocking from now on.
  mStream->ChangeExplicitBlockerCount(-1);
}

bool
OutputStreamData::Disconnect()
{
  MOZ_ASSERT(NS_IsMainThread());

  // During cycle collection, DOMMediaStream can be destroyed and send
  // its Destroy message before this decoder is destroyed. So we have to
  // be careful not to send any messages after the Destroy().
  if (mStream->IsDestroyed()) {
    return false;
  }

  // Disconnect the existing port if necessary.
  if (mPort) {
    mPort->Destroy();
    mPort = nullptr;
  }
  // Block the stream again. It will be unlocked when connecting
  // to the input stream.
  mStream->ChangeExplicitBlockerCount(1);
  return true;
}

void
OutputStreamData::Remove()
{
  MOZ_ASSERT(NS_IsMainThread());
  mOwner->Remove(mStream);
}

MediaStreamGraph*
OutputStreamData::Graph() const
{
  return mStream->Graph();
}

void
OutputStreamManager::Add(ProcessedMediaStream* aStream, bool aFinishWhenEnded)
{
  MOZ_ASSERT(NS_IsMainThread());
  // All streams must belong to the same graph.
  MOZ_ASSERT(!Graph() || Graph() == aStream->Graph());

  // Ensure that aStream finishes the moment mDecodedStream does.
  if (aFinishWhenEnded) {
    aStream->SetAutofinish(true);
  }

  OutputStreamData* p = mStreams.AppendElement();
  p->Init(this, aStream);

  // Connect to the input stream if we have one. Otherwise the output stream
  // will be connected in Connect().
  if (mInputStream) {
    p->Connect(mInputStream);
  }
}

void
OutputStreamManager::Remove(MediaStream* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  for (int32_t i = mStreams.Length() - 1; i >= 0; --i) {
    if (mStreams[i].Equals(aStream)) {
      mStreams.RemoveElementAt(i);
      break;
    }
  }
}

void
OutputStreamManager::Connect(MediaStream* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  mInputStream = aStream;
  for (auto&& os : mStreams) {
    os.Connect(aStream);
  }
}

void
OutputStreamManager::Disconnect()
{
  MOZ_ASSERT(NS_IsMainThread());
  mInputStream = nullptr;
  for (int32_t i = mStreams.Length() - 1; i >= 0; --i) {
    if (!mStreams[i].Disconnect()) {
      // Probably the DOMMediaStream was GCed. Clean up.
      mStreams.RemoveElementAt(i);
    }
  }
}

DecodedStream::DecodedStream(AbstractThread* aOwnerThread,
                             MediaQueue<MediaData>& aAudioQueue,
                             MediaQueue<MediaData>& aVideoQueue)
  : mOwnerThread(aOwnerThread)
  , mMonitor("DecodedStream::mMonitor")
  , mPlaying(false)
  , mVolume(1.0)
  , mAudioQueue(aAudioQueue)
  , mVideoQueue(aVideoQueue)
{
}

DecodedStream::~DecodedStream()
{
  MOZ_ASSERT(mStartTime.isNothing(), "playback should've ended.");
}

nsRefPtr<GenericPromise>
DecodedStream::StartPlayback(int64_t aStartTime, const MediaInfo& aInfo)
{
  AssertOwnerThread();
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  MOZ_ASSERT(mStartTime.isNothing(), "playback already started.");

  mStartTime.emplace(aStartTime);
  mInfo = aInfo;
  ConnectListener();

  class R : public nsRunnable {
    typedef MozPromiseHolder<GenericPromise> Promise;
    typedef void(DecodedStream::*Method)(Promise&&);
  public:
    R(DecodedStream* aThis, Method aMethod, Promise&& aPromise)
      : mThis(aThis), mMethod(aMethod)
    {
      mPromise = Move(aPromise);
    }
    NS_IMETHOD Run() override
    {
      (mThis->*mMethod)(Move(mPromise));
      return NS_OK;
    }
  private:
    nsRefPtr<DecodedStream> mThis;
    Method mMethod;
    Promise mPromise;
  };

  MozPromiseHolder<GenericPromise> promise;
  nsRefPtr<GenericPromise> rv = promise.Ensure(__func__);
  nsCOMPtr<nsIRunnable> r = new R(this, &DecodedStream::CreateData, Move(promise));
  AbstractThread::MainThread()->Dispatch(r.forget());

  return rv.forget();
}

void DecodedStream::StopPlayback()
{
  AssertOwnerThread();
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  // Playback didn't even start at all.
  if (mStartTime.isNothing()) {
    return;
  }

  mStartTime.reset();
  DisconnectListener();

  // Clear mData immediately when this playback session ends so we won't
  // send data to the wrong stream in SendData() in next playback session.
  DecodedStreamData* data = mData.release();
  // mData is not yet created on the main thread.
  if (!data) {
    return;
  }

  nsRefPtr<DecodedStream> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([=] () {
    self->mOutputStreamManager.Disconnect();
    delete data;
  });
  AbstractThread::MainThread()->Dispatch(r.forget());
}

void
DecodedStream::CreateData(MozPromiseHolder<GenericPromise>&& aPromise)
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  MOZ_ASSERT(!mData, "Already created.");

  // No need to create a source stream when there are no output streams. This
  // happens when RemoveOutput() is called immediately after StartPlayback().
  // We also bail out when the playback session has ended. This happens when
  // StopPlayback() is called immediately after StartPlayback().
  if (!mOutputStreamManager.Graph() || mStartTime.isNothing()) {
    // Resolve the promise to indicate the end of playback.
    aPromise.Resolve(true, __func__);
    return;
  }

  auto source = mOutputStreamManager.Graph()->CreateSourceStream(nullptr);
  mData.reset(new DecodedStreamData(source, mPlaying, Move(aPromise)));
  mOutputStreamManager.Connect(mData->mStream);

  // Start to send data to the stream immediately
  nsRefPtr<DecodedStream> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([=] () {
    ReentrantMonitorAutoEnter mon(self->GetReentrantMonitor());
    // Don't send data if playback has ended.
    if (self->mStartTime.isSome()) {
      self->SendData();
    }
  });
  // Don't assert success because the owner thread might have begun shutdown
  // while we are still dealing with jobs on the main thread.
  mOwnerThread->Dispatch(r.forget(), AbstractThread::DontAssertDispatchSuccess);
}

bool
DecodedStream::HasConsumers() const
{
  return !mOutputStreamManager.IsEmpty();
}

ReentrantMonitor&
DecodedStream::GetReentrantMonitor() const
{
  return mMonitor;
}

void
DecodedStream::AddOutput(ProcessedMediaStream* aStream, bool aFinishWhenEnded)
{
  mOutputStreamManager.Add(aStream, aFinishWhenEnded);
}

void
DecodedStream::RemoveOutput(MediaStream* aStream)
{
  mOutputStreamManager.Remove(aStream);
}

void
DecodedStream::SetPlaying(bool aPlaying)
{
  AssertOwnerThread();
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  mPlaying = aPlaying;
  if (mData) {
    mData->SetPlaying(aPlaying);
  }
}

void
DecodedStream::SetVolume(double aVolume)
{
  AssertOwnerThread();
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  mVolume = aVolume;
}

void
DecodedStream::SetSameOrigin(bool aSameOrigin)
{
  AssertOwnerThread();
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  mSameOrigin = aSameOrigin;
}

void
DecodedStream::InitTracks()
{
  AssertOwnerThread();
  GetReentrantMonitor().AssertCurrentThreadIn();

  if (mData->mStreamInitialized) {
    return;
  }

  SourceMediaStream* sourceStream = mData->mStream;

  if (mInfo.HasAudio()) {
    TrackID audioTrackId = mInfo.mAudio.mTrackId;
    AudioSegment* audio = new AudioSegment();
    sourceStream->AddAudioTrack(audioTrackId, mInfo.mAudio.mRate, 0, audio,
                                SourceMediaStream::ADDTRACK_QUEUED);
    mData->mNextAudioTime = mStartTime.ref();
  }

  if (mInfo.HasVideo()) {
    TrackID videoTrackId = mInfo.mVideo.mTrackId;
    VideoSegment* video = new VideoSegment();
    sourceStream->AddTrack(videoTrackId, 0, video,
                           SourceMediaStream::ADDTRACK_QUEUED);
    mData->mNextVideoTime = mStartTime.ref();
  }

  sourceStream->FinishAddTracks();
  mData->mStreamInitialized = true;
}

static void
SendStreamAudio(DecodedStreamData* aStream, int64_t aStartTime,
                MediaData* aData, AudioSegment* aOutput,
                uint32_t aRate, double aVolume)
{
  MOZ_ASSERT(aData);
  AudioData* audio = aData->As<AudioData>();
  // This logic has to mimic AudioSink closely to make sure we write
  // the exact same silences
  CheckedInt64 audioWrittenOffset = aStream->mAudioFramesWritten +
                                    UsecsToFrames(aStartTime, aRate);
  CheckedInt64 frameOffset = UsecsToFrames(audio->mTime, aRate);

  if (!audioWrittenOffset.isValid() ||
      !frameOffset.isValid() ||
      // ignore packet that we've already processed
      frameOffset.value() + audio->mFrames <= audioWrittenOffset.value()) {
    return;
  }

  if (audioWrittenOffset.value() < frameOffset.value()) {
    int64_t silentFrames = frameOffset.value() - audioWrittenOffset.value();
    // Write silence to catch up
    AudioSegment silence;
    silence.InsertNullDataAtStart(silentFrames);
    aStream->mAudioFramesWritten += silentFrames;
    audioWrittenOffset += silentFrames;
    aOutput->AppendFrom(&silence);
  }

  MOZ_ASSERT(audioWrittenOffset.value() >= frameOffset.value());

  int64_t offset = audioWrittenOffset.value() - frameOffset.value();
  size_t framesToWrite = audio->mFrames - offset;

  audio->EnsureAudioBuffer();
  nsRefPtr<SharedBuffer> buffer = audio->mAudioBuffer;
  AudioDataValue* bufferData = static_cast<AudioDataValue*>(buffer->Data());
  nsAutoTArray<const AudioDataValue*, 2> channels;
  for (uint32_t i = 0; i < audio->mChannels; ++i) {
    channels.AppendElement(bufferData + i * audio->mFrames + offset);
  }
  aOutput->AppendFrames(buffer.forget(), channels, framesToWrite);
  aStream->mAudioFramesWritten += framesToWrite;
  aOutput->ApplyVolume(aVolume);

  aStream->mNextAudioTime = audio->GetEndTime();
}

void
DecodedStream::SendAudio(double aVolume, bool aIsSameOrigin)
{
  AssertOwnerThread();
  GetReentrantMonitor().AssertCurrentThreadIn();

  if (!mInfo.HasAudio()) {
    return;
  }

  AudioSegment output;
  uint32_t rate = mInfo.mAudio.mRate;
  nsAutoTArray<nsRefPtr<MediaData>,10> audio;
  TrackID audioTrackId = mInfo.mAudio.mTrackId;
  SourceMediaStream* sourceStream = mData->mStream;

  // It's OK to hold references to the AudioData because AudioData
  // is ref-counted.
  mAudioQueue.GetElementsAfter(mData->mNextAudioTime, &audio);
  for (uint32_t i = 0; i < audio.Length(); ++i) {
    SendStreamAudio(mData.get(), mStartTime.ref(), audio[i], &output, rate, aVolume);
  }

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
                        int64_t aEndMicroseconds,
                        int64_t aStartMicroseconds,
                        const mozilla::gfx::IntSize& aIntrinsicSize,
                        VideoSegment* aOutput)
{
  nsRefPtr<layers::Image> image = aImage;
  StreamTime duration =
      aStream->MicrosecondsToStreamTimeRoundDown(aEndMicroseconds) -
      aStream->MicrosecondsToStreamTimeRoundDown(aStartMicroseconds);
  aOutput->AppendFrame(image.forget(), duration, aIntrinsicSize);
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
DecodedStream::SendVideo(bool aIsSameOrigin)
{
  AssertOwnerThread();
  GetReentrantMonitor().AssertCurrentThreadIn();

  if (!mInfo.HasVideo()) {
    return;
  }

  VideoSegment output;
  TrackID videoTrackId = mInfo.mVideo.mTrackId;
  nsAutoTArray<nsRefPtr<MediaData>, 10> video;
  SourceMediaStream* sourceStream = mData->mStream;

  // It's OK to hold references to the VideoData because VideoData
  // is ref-counted.
  mVideoQueue.GetElementsAfter(mData->mNextVideoTime, &video);

  for (uint32_t i = 0; i < video.Length(); ++i) {
    VideoData* v = video[i]->As<VideoData>();

    if (mData->mNextVideoTime < v->mTime) {
      // Write last video frame to catch up. mLastVideoImage can be null here
      // which is fine, it just means there's no video.

      // TODO: |mLastVideoImage| should come from the last image rendered
      // by the state machine. This will avoid the black frame when capture
      // happens in the middle of playback (especially in th middle of a
      // video frame). E.g. if we have a video frame that is 30 sec long
      // and capture happens at 15 sec, we'll have to append a black frame
      // that is 15 sec long.
      WriteVideoToMediaStream(sourceStream, mData->mLastVideoImage, v->mTime,
          mData->mNextVideoTime, mData->mLastVideoImageDisplaySize, &output);
      mData->mNextVideoTime = v->mTime;
    }

    if (mData->mNextVideoTime < v->GetEndTime()) {
      WriteVideoToMediaStream(sourceStream, v->mImage,
          v->GetEndTime(), mData->mNextVideoTime, v->mDisplay, &output);
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
      int64_t deviation_usec = sourceStream->StreamTimeToMicroseconds(1);
      WriteVideoToMediaStream(sourceStream, mData->mLastVideoImage,
          mData->mNextVideoTime + deviation_usec, mData->mNextVideoTime,
          mData->mLastVideoImageDisplaySize, &endSegment);
      mData->mNextVideoTime += deviation_usec;
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
  GetReentrantMonitor().AssertCurrentThreadIn();

  StreamTime endPosition = 0;

  if (mInfo.HasAudio()) {
    StreamTime audioEnd = mData->mStream->TicksToTimeRoundDown(
        mInfo.mAudio.mRate, mData->mAudioFramesWritten);
    endPosition = std::max(endPosition, audioEnd);
  }

  if (mInfo.HasVideo()) {
    StreamTime videoEnd = mData->mStream->MicrosecondsToStreamTimeRoundDown(
        mData->mNextVideoTime - mStartTime.ref());
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
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  MOZ_ASSERT(mStartTime.isSome(), "Must be called after StartPlayback()");

  // Not yet created on the main thread. MDSM will try again later.
  if (!mData) {
    return;
  }

  // Nothing to do when the stream is finished.
  if (mData->mHaveSentFinish) {
    return;
  }

  InitTracks();
  SendAudio(mVolume, mSameOrigin);
  SendVideo(mSameOrigin);
  AdvanceTracks();

  bool finished = (!mInfo.HasAudio() || mAudioQueue.IsFinished()) &&
                  (!mInfo.HasVideo() || mVideoQueue.IsFinished());

  if (finished && !mData->mHaveSentFinish) {
    mData->mHaveSentFinish = true;
    mData->mStream->Finish();
  }
}

int64_t
DecodedStream::AudioEndTime() const
{
  AssertOwnerThread();
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  if (mStartTime.isSome() && mInfo.HasAudio() && mData) {
    CheckedInt64 t = mStartTime.ref() +
      FramesToUsecs(mData->mAudioFramesWritten, mInfo.mAudio.mRate);
    if (t.isValid()) {
      return t.value();
    }
  }
  return -1;
}

int64_t
DecodedStream::GetPosition() const
{
  AssertOwnerThread();
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  // This is only called after MDSM starts playback. So mStartTime is
  // guaranteed to be something.
  MOZ_ASSERT(mStartTime.isSome());
  return mStartTime.ref() + (mData ? mData->GetPosition() : 0);
}

bool
DecodedStream::IsFinished() const
{
  AssertOwnerThread();
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  return mData && mData->IsFinished();
}

void
DecodedStream::ConnectListener()
{
  AssertOwnerThread();
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

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
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

  mAudioPushListener.Disconnect();
  mVideoPushListener.Disconnect();
  mAudioFinishListener.Disconnect();
  mVideoFinishListener.Disconnect();
}

} // namespace mozilla
