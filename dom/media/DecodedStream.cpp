/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecodedStream.h"
#include "MediaStreamGraph.h"
#include "AudioSegment.h"
#include "VideoSegment.h"
#include "MediaQueue.h"
#include "MediaData.h"
#include "SharedBuffer.h"
#include "VideoUtils.h"

namespace mozilla {

class DecodedStreamGraphListener : public MediaStreamListener {
  typedef MediaStreamListener::MediaStreamGraphEvent MediaStreamGraphEvent;
public:
  explicit DecodedStreamGraphListener(MediaStream* aStream)
    : mMutex("DecodedStreamGraphListener::mMutex")
    , mStream(aStream)
    , mLastOutputTime(aStream->StreamTimeToMicroseconds(aStream->GetCurrentTime()))
    , mStreamFinishedOnMainThread(false) {}

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

DecodedStreamData::DecodedStreamData(SourceMediaStream* aStream, bool aPlaying)
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
  mListener = new DecodedStreamGraphListener(mStream);
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
  OutputStreamListener(DecodedStream* aDecodedStream, MediaStream* aStream)
    : mDecodedStream(aDecodedStream), mStream(aStream) {}

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
    mDecodedStream = nullptr;
  }

private:
  void DoNotifyFinished()
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (mDecodedStream) {
      // Remove the finished stream so it won't block the decoded stream.
      mDecodedStream->Remove(mStream);
    }
  }

  // Main thread only
  DecodedStream* mDecodedStream;
  nsRefPtr<MediaStream> mStream;
};

OutputStreamData::~OutputStreamData()
{
  mListener->Forget();
}

void
OutputStreamData::Init(DecodedStream* aDecodedStream, ProcessedMediaStream* aStream)
{
  mStream = aStream;
  mListener = new OutputStreamListener(aDecodedStream, aStream);
  aStream->AddListener(mListener);
}

DecodedStream::DecodedStream(MediaQueue<AudioData>& aAudioQueue,
                             MediaQueue<VideoData>& aVideoQueue)
  : mMonitor("DecodedStream::mMonitor")
  , mPlaying(false)
  , mAudioQueue(aAudioQueue)
  , mVideoQueue(aVideoQueue)
{
  //
}

void
DecodedStream::StartPlayback(int64_t aStartTime, const MediaInfo& aInfo)
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  if (mStartTime.isNothing()) {
    mStartTime.emplace(aStartTime);
    mInfo = aInfo;
  }
}

void DecodedStream::StopPlayback()
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  mStartTime.reset();
}

void
DecodedStream::DestroyData()
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

  // Avoid the redundant blocking to output stream.
  if (!mData) {
    return;
  }

  // All streams are having their SourceMediaStream disconnected, so they
  // need to be explicitly blocked again.
  auto& outputStreams = OutputStreams();
  for (int32_t i = outputStreams.Length() - 1; i >= 0; --i) {
    OutputStreamData& os = outputStreams[i];
    // Explicitly remove all existing ports.
    // This is not strictly necessary but it's good form.
    MOZ_ASSERT(os.mPort, "Double-delete of the ports!");
    os.mPort->Destroy();
    os.mPort = nullptr;
    // During cycle collection, nsDOMMediaStream can be destroyed and send
    // its Destroy message before this decoder is destroyed. So we have to
    // be careful not to send any messages after the Destroy().
    if (os.mStream->IsDestroyed()) {
      // Probably the DOM MediaStream was GCed. Clean up.
      outputStreams.RemoveElementAt(i);
    } else {
      os.mStream->ChangeExplicitBlockerCount(1);
    }
  }

  mData = nullptr;
}

void
DecodedStream::RecreateData()
{
  nsRefPtr<DecodedStream> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([self] () -> void {
    self->RecreateData(nullptr);
  });
  AbstractThread::MainThread()->Dispatch(r.forget());
}

void
DecodedStream::RecreateData(MediaStreamGraph* aGraph)
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  MOZ_ASSERT((aGraph && !mData && OutputStreams().IsEmpty()) || // first time
             (!aGraph && mData)); // 2nd time and later

  if (!aGraph) {
    aGraph = mData->mStream->Graph();
  }
  auto source = aGraph->CreateSourceStream(nullptr);
  DestroyData();
  mData.reset(new DecodedStreamData(source, mPlaying));

  // Note that the delay between removing ports in DestroyDecodedStream
  // and adding new ones won't cause a glitch since all graph operations
  // between main-thread stable states take effect atomically.
  auto& outputStreams = OutputStreams();
  for (int32_t i = outputStreams.Length() - 1; i >= 0; --i) {
    OutputStreamData& os = outputStreams[i];
    MOZ_ASSERT(!os.mStream->IsDestroyed(), "Should've been removed in DestroyData()");
    Connect(&os);
  }
}

nsTArray<OutputStreamData>&
DecodedStream::OutputStreams()
{
  MOZ_ASSERT(NS_IsMainThread());
  GetReentrantMonitor().AssertCurrentThreadIn();
  return mOutputStreams;
}

ReentrantMonitor&
DecodedStream::GetReentrantMonitor() const
{
  return mMonitor;
}

void
DecodedStream::Connect(OutputStreamData* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  GetReentrantMonitor().AssertCurrentThreadIn();
  NS_ASSERTION(!aStream->mPort, "Already connected?");

  // The output stream must stay in sync with the decoded stream, so if
  // either stream is blocked, we block the other.
  aStream->mPort = aStream->mStream->AllocateInputPort(mData->mStream,
      MediaInputPort::FLAG_BLOCK_INPUT | MediaInputPort::FLAG_BLOCK_OUTPUT);
  // Unblock the output stream now. While it's connected to DecodedStream,
  // DecodedStream is responsible for controlling blocking.
  aStream->mStream->ChangeExplicitBlockerCount(-1);
}

void
DecodedStream::Connect(ProcessedMediaStream* aStream, bool aFinishWhenEnded)
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

  if (!mData) {
    RecreateData(aStream->Graph());
  }

  OutputStreamData* os = OutputStreams().AppendElement();
  os->Init(this, aStream);
  Connect(os);
  if (aFinishWhenEnded) {
    // Ensure that aStream finishes the moment mDecodedStream does.
    aStream->SetAutofinish(true);
  }
}

void
DecodedStream::Remove(MediaStream* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

  auto& streams = OutputStreams();
  for (int32_t i = streams.Length() - 1; i >= 0; --i) {
    auto& os = streams[i];
    MediaStream* p = os.mStream.get();
    if (p == aStream) {
      if (os.mPort) {
        os.mPort->Destroy();
        os.mPort = nullptr;
      }
      streams.RemoveElementAt(i);
      break;
    }
  }
}

void
DecodedStream::SetPlaying(bool aPlaying)
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  mPlaying = aPlaying;
  if (mData) {
    mData->SetPlaying(aPlaying);
  }
}

void
DecodedStream::InitTracks()
{
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
                AudioData* aAudio, AudioSegment* aOutput,
                uint32_t aRate, double aVolume)
{
  // This logic has to mimic AudioSink closely to make sure we write
  // the exact same silences
  CheckedInt64 audioWrittenOffset = aStream->mAudioFramesWritten +
                                    UsecsToFrames(aStartTime, aRate);
  CheckedInt64 frameOffset = UsecsToFrames(aAudio->mTime, aRate);

  if (!audioWrittenOffset.isValid() ||
      !frameOffset.isValid() ||
      // ignore packet that we've already processed
      frameOffset.value() + aAudio->mFrames <= audioWrittenOffset.value()) {
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
  size_t framesToWrite = aAudio->mFrames - offset;

  aAudio->EnsureAudioBuffer();
  nsRefPtr<SharedBuffer> buffer = aAudio->mAudioBuffer;
  AudioDataValue* bufferData = static_cast<AudioDataValue*>(buffer->Data());
  nsAutoTArray<const AudioDataValue*, 2> channels;
  for (uint32_t i = 0; i < aAudio->mChannels; ++i) {
    channels.AppendElement(bufferData + i * aAudio->mFrames + offset);
  }
  aOutput->AppendFrames(buffer.forget(), channels, framesToWrite);
  aStream->mAudioFramesWritten += framesToWrite;
  aOutput->ApplyVolume(aVolume);

  aStream->mNextAudioTime = aAudio->GetEndTime();
}

void
DecodedStream::SendAudio(double aVolume, bool aIsSameOrigin)
{
  GetReentrantMonitor().AssertCurrentThreadIn();

  if (!mInfo.HasAudio()) {
    return;
  }

  AudioSegment output;
  uint32_t rate = mInfo.mAudio.mRate;
  nsAutoTArray<nsRefPtr<AudioData>,10> audio;
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
  GetReentrantMonitor().AssertCurrentThreadIn();

  if (!mInfo.HasVideo()) {
    return;
  }

  VideoSegment output;
  TrackID videoTrackId = mInfo.mVideo.mTrackId;
  nsAutoTArray<nsRefPtr<VideoData>, 10> video;
  SourceMediaStream* sourceStream = mData->mStream;

  // It's OK to hold references to the VideoData because VideoData
  // is ref-counted.
  mVideoQueue.GetElementsAfter(mData->mNextVideoTime, &video);

  for (uint32_t i = 0; i < video.Length(); ++i) {
    VideoData* v = video[i];

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

bool
DecodedStream::SendData(double aVolume, bool aIsSameOrigin)
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  MOZ_ASSERT(mStartTime.isSome(), "Must be called after StartPlayback()");

  InitTracks();
  SendAudio(aVolume, aIsSameOrigin);
  SendVideo(aIsSameOrigin);
  AdvanceTracks();

  bool finished = (!mInfo.HasAudio() || mAudioQueue.IsFinished()) &&
                  (!mInfo.HasVideo() || mVideoQueue.IsFinished());

  if (finished && !mData->mHaveSentFinish) {
    mData->mHaveSentFinish = true;
    mData->mStream->Finish();
  }

  return finished;
}

CheckedInt64
DecodedStream::AudioEndTime() const
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  MOZ_ASSERT(mStartTime.isSome(), "Must be called after StartPlayback()");
  return mStartTime.ref() +
         FramesToUsecs(mData->mAudioFramesWritten, mInfo.mAudio.mRate);
}

int64_t
DecodedStream::GetPosition() const
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  return mData->GetPosition();
}

bool
DecodedStream::IsFinished() const
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  return mData->IsFinished();
}

} // namespace mozilla
