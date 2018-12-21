/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AbstractThread.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/SyncRunnable.h"
#include "nsProxyRelease.h"

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

using media::TimeUnit;

/*
 * A container class to make it easier to pass the playback info all the
 * way to DecodedStreamGraphListener from DecodedStream.
 */
struct PlaybackInfoInit {
  TimeUnit mStartTime;
  MediaInfo mInfo;
  TrackID mAudioTrackID;
  TrackID mVideoTrackID;
};

class DecodedStreamGraphListener;

class DecodedStreamTrackListener : public MediaStreamTrackListener {
 public:
  DecodedStreamTrackListener(DecodedStreamGraphListener* aGraphListener,
                             SourceMediaStream* aStream, TrackID aTrackID);

  void NotifyOutput(MediaStreamGraph* aGraph,
                    StreamTime aCurrentTrackTime) override;
  void NotifyEnded() override;

 private:
  const RefPtr<DecodedStreamGraphListener> mGraphListener;
  const RefPtr<SourceMediaStream> mStream;
  const mozilla::TrackID mTrackID;
};

class DecodedStreamGraphListener {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DecodedStreamGraphListener)
 public:
  DecodedStreamGraphListener(SourceMediaStream* aStream, TrackID aAudioTrackID,
                             MozPromiseHolder<GenericPromise>&& aAudioEndHolder,
                             TrackID aVideoTrackID,
                             MozPromiseHolder<GenericPromise>&& aVideoEndHolder,
                             AbstractThread* aMainThread)
      : mAudioTrackListener(IsTrackIDExplicit(aAudioTrackID)
                                ? MakeRefPtr<DecodedStreamTrackListener>(
                                      this, aStream, aAudioTrackID)
                                : nullptr),
        mAudioTrackID(aAudioTrackID),
        mAudioEndHolder(std::move(aAudioEndHolder)),
        mVideoTrackListener(IsTrackIDExplicit(aVideoTrackID)
                                ? MakeRefPtr<DecodedStreamTrackListener>(
                                      this, aStream, aVideoTrackID)
                                : nullptr),
        mVideoTrackID(aVideoTrackID),
        mVideoEndHolder(std::move(aVideoEndHolder)),
        mStream(aStream),
        mAbstractMainThread(aMainThread) {
    MOZ_ASSERT(NS_IsMainThread());
    if (mAudioTrackListener) {
      mStream->AddTrackListener(mAudioTrackListener, mAudioTrackID);
    } else {
      mAudioEndHolder.ResolveIfExists(true, __func__);
    }

    if (mVideoTrackListener) {
      mStream->AddTrackListener(mVideoTrackListener, mVideoTrackID);
    } else {
      mVideoEndHolder.ResolveIfExists(true, __func__);
    }
  }

  void NotifyOutput(TrackID aTrackID, StreamTime aCurrentTrackTime) {
    if (aTrackID != mAudioTrackID && mAudioTrackID != TRACK_NONE) {
      // Only audio playout drives the clock forward, if present.
      return;
    }
    mOnOutput.Notify(mStream->StreamTimeToMicroseconds(aCurrentTrackTime));
  }

  TrackID AudioTrackID() const { return mAudioTrackID; }

  TrackID VideoTrackID() const { return mVideoTrackID; }

  void DoNotifyTrackEnded(TrackID aTrackID) {
    MOZ_ASSERT(NS_IsMainThread());
    if (aTrackID == mAudioTrackID) {
      mAudioEndHolder.ResolveIfExists(true, __func__);
    } else if (aTrackID == mVideoTrackID) {
      mVideoEndHolder.ResolveIfExists(true, __func__);
    } else {
      MOZ_CRASH("Unexpected track id");
    }
  }

  void Forget() {
    MOZ_ASSERT(NS_IsMainThread());

    if (mAudioTrackListener && !mStream->IsDestroyed()) {
      mStream->EndTrack(mAudioTrackID);
      mStream->RemoveTrackListener(mAudioTrackListener, mAudioTrackID);
    }
    mAudioTrackListener = nullptr;
    mAudioEndHolder.ResolveIfExists(false, __func__);

    if (mVideoTrackListener && !mStream->IsDestroyed()) {
      mStream->EndTrack(mVideoTrackID);
      mStream->RemoveTrackListener(mVideoTrackListener, mVideoTrackID);
    }
    mVideoTrackListener = nullptr;
    mVideoEndHolder.ResolveIfExists(false, __func__);
  }

  MediaEventSource<int64_t>& OnOutput() { return mOnOutput; }

 private:
  ~DecodedStreamGraphListener() {
    MOZ_ASSERT(mAudioEndHolder.IsEmpty());
    MOZ_ASSERT(mVideoEndHolder.IsEmpty());
  }

  MediaEventProducer<int64_t> mOnOutput;

  // Main thread only.
  RefPtr<DecodedStreamTrackListener> mAudioTrackListener;
  const TrackID mAudioTrackID;
  MozPromiseHolder<GenericPromise> mAudioEndHolder;
  RefPtr<DecodedStreamTrackListener> mVideoTrackListener;
  const TrackID mVideoTrackID;
  MozPromiseHolder<GenericPromise> mVideoEndHolder;

  const RefPtr<SourceMediaStream> mStream;
  const RefPtr<AbstractThread> mAbstractMainThread;
};

DecodedStreamTrackListener::DecodedStreamTrackListener(
    DecodedStreamGraphListener* aGraphListener, SourceMediaStream* aStream,
    mozilla::TrackID aTrackID)
    : mGraphListener(aGraphListener), mStream(aStream), mTrackID(aTrackID) {}

void DecodedStreamTrackListener::NotifyOutput(MediaStreamGraph* aGraph,
                                              StreamTime aCurrentTrackTime) {
  mGraphListener->NotifyOutput(mTrackID, aCurrentTrackTime);
}

void DecodedStreamTrackListener::NotifyEnded() {
  mStream->Graph()->DispatchToMainThreadAfterStreamStateUpdate(
      NewRunnableMethod<mozilla::TrackID>(
          "DecodedStreamGraphListener::DoNotifyTrackEnded", mGraphListener,
          &DecodedStreamGraphListener::DoNotifyTrackEnded, mTrackID));
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
                    MozPromiseHolder<GenericPromise>&& aAudioPromise,
                    MozPromiseHolder<GenericPromise>&& aVideoPromise,
                    AbstractThread* aMainThread);
  ~DecodedStreamData();
  MediaEventSource<int64_t>& OnOutput();
  void Forget();
  nsCString GetDebugInfo();

  /* The following group of fields are protected by the decoder's monitor
   * and can be read or written on any thread.
   */
  // Count of audio frames written to the stream
  int64_t mAudioFramesWritten;
  // Count of video frames written to the stream in the stream's rate
  StreamTime mStreamVideoWritten;
  // Count of audio frames written to the stream in the stream's rate
  StreamTime mStreamAudioWritten;
  // mNextVideoTime is the end timestamp for the last packet sent to the stream.
  // Therefore video packets starting at or after this time need to be copied
  // to the output stream.
  TimeUnit mNextVideoTime;
  TimeUnit mNextAudioTime;
  // The last video image sent to the stream. Useful if we need to replicate
  // the image.
  RefPtr<layers::Image> mLastVideoImage;
  gfx::IntSize mLastVideoImageDisplaySize;
  bool mHaveSentFinishAudio;
  bool mHaveSentFinishVideo;

  // The decoder is responsible for calling Destroy() on this stream.
  const RefPtr<SourceMediaStream> mStream;
  const RefPtr<DecodedStreamGraphListener> mListener;
  // True if we need to send a compensation video frame to ensure the
  // StreamTime going forward.
  bool mEOSVideoCompensation;

  const RefPtr<OutputStreamManager> mOutputStreamManager;
  const RefPtr<AbstractThread> mAbstractMainThread;
};

DecodedStreamData::DecodedStreamData(
    OutputStreamManager* aOutputStreamManager, PlaybackInfoInit&& aInit,
    MozPromiseHolder<GenericPromise>&& aAudioPromise,
    MozPromiseHolder<GenericPromise>&& aVideoPromise,
    AbstractThread* aMainThread)
    : mAudioFramesWritten(0),
      mStreamVideoWritten(0),
      mStreamAudioWritten(0),
      mNextVideoTime(aInit.mStartTime),
      mNextAudioTime(aInit.mStartTime),
      mHaveSentFinishAudio(false),
      mHaveSentFinishVideo(false),
      mStream(aOutputStreamManager->mSourceStream),
      // DecodedStreamGraphListener will resolve these promises.
      mListener(MakeRefPtr<DecodedStreamGraphListener>(
          mStream, aInit.mAudioTrackID, std::move(aAudioPromise),
          aInit.mVideoTrackID, std::move(aVideoPromise), aMainThread)),
      mEOSVideoCompensation(false),
      mOutputStreamManager(aOutputStreamManager),
      mAbstractMainThread(aMainThread) {
  MOZ_ASSERT(NS_IsMainThread());
  // Initialize tracks on main thread and in the MediaStreamGraph.
  // Tracks on main thread may have been created early in OutputStreamManager
  // by the state machine, since creating them here is async from the js call.
  // If they were pre-created in OutputStreamManager and the MediaInfo has
  // changed since then, we end them and create new tracks.
  if (!mOutputStreamManager->HasTracks(aInit.mAudioTrackID,
                                       aInit.mVideoTrackID)) {
    // Because these tracks were pre-allocated, we also have to increment the
    // internal track allocator by the same number of tracks, so we don't risk
    // a TrackID collision.
    for (size_t i = 0; i < mOutputStreamManager->NumberOfTracks(); ++i) {
      Unused << mOutputStreamManager->AllocateNextTrackID();
    }
    mOutputStreamManager->RemoveTracks();
  }
  if (IsTrackIDExplicit(aInit.mAudioTrackID)) {
    if (!mOutputStreamManager->HasTrack(aInit.mAudioTrackID)) {
      mOutputStreamManager->AddTrack(aInit.mAudioTrackID, MediaSegment::AUDIO);
    }
    mStream->AddAudioTrack(aInit.mAudioTrackID, aInit.mInfo.mAudio.mRate,
                           new AudioSegment());
  }
  if (IsTrackIDExplicit(aInit.mVideoTrackID)) {
    if (!mOutputStreamManager->HasTrack(aInit.mVideoTrackID)) {
      mOutputStreamManager->AddTrack(aInit.mVideoTrackID, MediaSegment::VIDEO);
    }
    mStream->AddTrack(aInit.mVideoTrackID, new VideoSegment());
  }
}

DecodedStreamData::~DecodedStreamData() { MOZ_ASSERT(NS_IsMainThread()); }

MediaEventSource<int64_t>& DecodedStreamData::OnOutput() {
  return mListener->OnOutput();
}

void DecodedStreamData::Forget() { mListener->Forget(); }

nsCString DecodedStreamData::GetDebugInfo() {
  return nsPrintfCString(
      "DecodedStreamData=%p mAudioFramesWritten=%" PRId64
      " mStreamAudioWritten=%" PRId64 " mStreamVideoWritten=%" PRId64
      " mNextAudioTime=%" PRId64 " mNextVideoTime=%" PRId64
      "mHaveSentFinishAudio=%d mHaveSentFinishVideo=%d",
      this, mAudioFramesWritten, mStreamAudioWritten, mStreamVideoWritten,
      mNextAudioTime.ToMicroseconds(), mNextVideoTime.ToMicroseconds(),
      mHaveSentFinishAudio, mHaveSentFinishVideo);
}

DecodedStream::DecodedStream(AbstractThread* aOwnerThread,
                             AbstractThread* aMainThread,
                             MediaQueue<AudioData>& aAudioQueue,
                             MediaQueue<VideoData>& aVideoQueue,
                             OutputStreamManager* aOutputStreamManager,
                             const bool& aSameOrigin)
    : mOwnerThread(aOwnerThread),
      mAbstractMainThread(aMainThread),
      mOutputStreamManager(aOutputStreamManager),
      mWatchManager(this, mOwnerThread),
      mPlaying(false, "DecodedStream::mPlaying"),
      mSameOrigin(aSameOrigin),
      mPrincipalHandle(aOwnerThread, PRINCIPAL_HANDLE_NONE,
                       "DecodedStream::mPrincipalHandle (Mirror)"),
      mAudioQueue(aAudioQueue),
      mVideoQueue(aVideoQueue) {
  mPrincipalHandle.Connect(mOutputStreamManager->CanonicalPrincipalHandle());

  mWatchManager.Watch(mPlaying, &DecodedStream::PlayingChanged);

  PlayingChanged();  // Notify of the initial state
}

DecodedStream::~DecodedStream() {
  MOZ_ASSERT(mStartTime.isNothing(), "playback should've ended.");
  NS_ProxyRelease("DecodedStream::mOutputStreamManager", mAbstractMainThread,
                  do_AddRef(mOutputStreamManager));
}

const media::MediaSink::PlaybackParams& DecodedStream::GetPlaybackParams()
    const {
  AssertOwnerThread();
  return mParams;
}

void DecodedStream::SetPlaybackParams(const PlaybackParams& aParams) {
  AssertOwnerThread();
  mParams = aParams;
}

RefPtr<GenericPromise> DecodedStream::OnEnded(TrackType aType) {
  AssertOwnerThread();
  MOZ_ASSERT(mStartTime.isSome());

  if (aType == TrackInfo::kAudioTrack && mInfo.HasAudio()) {
    return mAudioEndPromise;
  } else if (aType == TrackInfo::kVideoTrack && mInfo.HasVideo()) {
    return mVideoEndPromise;
  }
  return nullptr;
}

nsresult DecodedStream::Start(const TimeUnit& aStartTime,
                              const MediaInfo& aInfo) {
  AssertOwnerThread();
  MOZ_ASSERT(mStartTime.isNothing(), "playback already started.");

  mStartTime.emplace(aStartTime);
  mLastOutputTime = TimeUnit::Zero();
  mInfo = aInfo;
  mPlaying = true;
  ConnectListener();

  class R : public Runnable {
    typedef MozPromiseHolder<GenericPromise> Promise;

   public:
    R(PlaybackInfoInit&& aInit, Promise&& aAudioPromise,
      Promise&& aVideoPromise, OutputStreamManager* aManager,
      AbstractThread* aMainThread)
        : Runnable("CreateDecodedStreamData"),
          mInit(std::move(aInit)),
          mAudioPromise(std::move(aAudioPromise)),
          mVideoPromise(std::move(aVideoPromise)),
          mOutputStreamManager(aManager),
          mAbstractMainThread(aMainThread) {}
    NS_IMETHOD Run() override {
      MOZ_ASSERT(NS_IsMainThread());
      // No need to create a source stream when there are no output streams.
      // This happens when RemoveOutput() is called immediately after
      // StartPlayback().
      if (mOutputStreamManager->IsEmpty()) {
        // Resolve the promise to indicate the end of playback.
        mAudioPromise.Resolve(true, __func__);
        mVideoPromise.Resolve(true, __func__);
        return NS_OK;
      }
      mInit.mAudioTrackID = mInit.mInfo.HasAudio()
                                ? mOutputStreamManager->AllocateNextTrackID()
                                : TRACK_NONE;
      mInit.mVideoTrackID = mInit.mInfo.HasVideo()
                                ? mOutputStreamManager->AllocateNextTrackID()
                                : TRACK_NONE;
      mData = MakeUnique<DecodedStreamData>(
          mOutputStreamManager, std::move(mInit), std::move(mAudioPromise),
          std::move(mVideoPromise), mAbstractMainThread);
      return NS_OK;
    }
    UniquePtr<DecodedStreamData> ReleaseData() { return std::move(mData); }

   private:
    PlaybackInfoInit mInit;
    Promise mAudioPromise;
    Promise mVideoPromise;
    RefPtr<OutputStreamManager> mOutputStreamManager;
    UniquePtr<DecodedStreamData> mData;
    const RefPtr<AbstractThread> mAbstractMainThread;
  };

  MozPromiseHolder<GenericPromise> audioHolder;
  mAudioEndPromise = audioHolder.Ensure(__func__);
  MozPromiseHolder<GenericPromise> videoHolder;
  mVideoEndPromise = videoHolder.Ensure(__func__);
  PlaybackInfoInit init{aStartTime, aInfo, TRACK_INVALID, TRACK_INVALID};
  nsCOMPtr<nsIRunnable> r =
      new R(std::move(init), std::move(audioHolder), std::move(videoHolder),
            mOutputStreamManager, mAbstractMainThread);
  SyncRunnable::DispatchToThread(
      SystemGroup::EventTargetFor(mozilla::TaskCategory::Other), r);
  mData = static_cast<R*>(r.get())->ReleaseData();

  if (mData) {
    mInfo.mAudio.mTrackId = mData->mListener->AudioTrackID();
    mInfo.mVideo.mTrackId = mData->mListener->VideoTrackID();
    mOutputListener = mData->OnOutput().Connect(mOwnerThread, this,
                                                &DecodedStream::NotifyOutput);
    SendData();
  }
  return NS_OK;
}

void DecodedStream::Stop() {
  AssertOwnerThread();
  MOZ_ASSERT(mStartTime.isSome(), "playback not started.");

  mStreamTimeOffset += SentDuration();
  mStartTime.reset();
  DisconnectListener();
  mAudioEndPromise = nullptr;
  mVideoEndPromise = nullptr;

  // Clear mData immediately when this playback session ends so we won't
  // send data to the wrong stream in SendData() in next playback session.
  DestroyData(std::move(mData));
}

bool DecodedStream::IsStarted() const {
  AssertOwnerThread();
  return mStartTime.isSome();
}

bool DecodedStream::IsPlaying() const {
  AssertOwnerThread();
  return IsStarted() && mPlaying;
}

void DecodedStream::Shutdown() {
  AssertOwnerThread();
  mPrincipalHandle.DisconnectIfConnected();
  mWatchManager.Shutdown();
}

void DecodedStream::DestroyData(UniquePtr<DecodedStreamData>&& aData) {
  AssertOwnerThread();

  if (!aData) {
    return;
  }

  mOutputListener.Disconnect();

  NS_DispatchToMainThread(
      NS_NewRunnableFunction("DecodedStream::DestroyData",
                             [data = std::move(aData)]() { data->Forget(); }));
}

void DecodedStream::SetPlaying(bool aPlaying) {
  AssertOwnerThread();

  // Resume/pause matters only when playback started.
  if (mStartTime.isNothing()) {
    return;
  }

  mPlaying = aPlaying;
}

void DecodedStream::SetVolume(double aVolume) {
  AssertOwnerThread();
  mParams.mVolume = aVolume;
}

void DecodedStream::SetPlaybackRate(double aPlaybackRate) {
  AssertOwnerThread();
  mParams.mPlaybackRate = aPlaybackRate;
}

void DecodedStream::SetPreservesPitch(bool aPreservesPitch) {
  AssertOwnerThread();
  mParams.mPreservesPitch = aPreservesPitch;
}

static void SendStreamAudio(DecodedStreamData* aStream,
                            const TimeUnit& aStartTime, AudioData* aData,
                            AudioSegment* aOutput, uint32_t aRate,
                            const PrincipalHandle& aPrincipalHandle) {
  // The amount of audio frames that is used to fuzz rounding errors.
  static const int64_t AUDIO_FUZZ_FRAMES = 1;

  MOZ_ASSERT(aData);
  AudioData* audio = aData;
  // This logic has to mimic AudioSink closely to make sure we write
  // the exact same silences
  CheckedInt64 audioWrittenOffset =
      aStream->mAudioFramesWritten + TimeUnitToFrames(aStartTime, aRate);
  CheckedInt64 frameOffset = TimeUnitToFrames(audio->mTime, aRate);

  if (!audioWrittenOffset.isValid() || !frameOffset.isValid() ||
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
  aOutput->AppendFrames(buffer.forget(), channels, audio->mFrames,
                        aPrincipalHandle);
  aStream->mAudioFramesWritten += audio->mFrames;

  aStream->mNextAudioTime = audio->GetEndTime();
}

void DecodedStream::SendAudio(double aVolume, bool aIsSameOrigin,
                              const PrincipalHandle& aPrincipalHandle) {
  AssertOwnerThread();

  if (!mInfo.HasAudio()) {
    return;
  }

  if (mData->mHaveSentFinishAudio) {
    return;
  }

  AudioSegment output;
  uint32_t rate = mInfo.mAudio.mRate;
  AutoTArray<RefPtr<AudioData>, 10> audio;
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
    mData->mStreamAudioWritten +=
        sourceStream->AppendToTrack(audioTrackId, &output);
  }

  if (mAudioQueue.IsFinished() && !mData->mHaveSentFinishAudio) {
    sourceStream->EndTrack(audioTrackId);
    mData->mHaveSentFinishAudio = true;
  }
}

static void WriteVideoToMediaStream(MediaStream* aStream, layers::Image* aImage,
                                    const TimeUnit& aEnd,
                                    const TimeUnit& aStart,
                                    const mozilla::gfx::IntSize& aIntrinsicSize,
                                    const TimeStamp& aTimeStamp,
                                    VideoSegment* aOutput,
                                    const PrincipalHandle& aPrincipalHandle) {
  RefPtr<layers::Image> image = aImage;
  auto end = aStream->MicrosecondsToStreamTimeRoundDown(aEnd.ToMicroseconds());
  auto start =
      aStream->MicrosecondsToStreamTimeRoundDown(aStart.ToMicroseconds());
  StreamTime duration = end - start;
  aOutput->AppendFrame(image.forget(), duration, aIntrinsicSize,
                       aPrincipalHandle, false, aTimeStamp);
}

static bool ZeroDurationAtLastChunk(VideoSegment& aInput) {
  // Get the last video frame's start time in VideoSegment aInput.
  // If the start time is equal to the duration of aInput, means the last video
  // frame's duration is zero.
  StreamTime lastVideoStratTime;
  aInput.GetLastFrame(&lastVideoStratTime);
  return lastVideoStratTime == aInput.GetDuration();
}

void DecodedStream::SendVideo(bool aIsSameOrigin,
                              const PrincipalHandle& aPrincipalHandle) {
  AssertOwnerThread();

  if (!mInfo.HasVideo()) {
    return;
  }

  if (mData->mHaveSentFinishVideo) {
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
  TimeStamp tracksStartTimeStamp =
      sourceStream->GetStreamTracksStrartTimeStamp();
  if (tracksStartTimeStamp.IsNull()) {
    tracksStartTimeStamp = TimeStamp::Now();
  }

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
                              mData->mNextVideoTime,
                              mData->mLastVideoImageDisplaySize,
                              tracksStartTimeStamp + v->mTime.ToTimeDuration(),
                              &output, aPrincipalHandle);
      mData->mNextVideoTime = v->mTime;
    }

    if (mData->mNextVideoTime < v->GetEndTime()) {
      WriteVideoToMediaStream(
          sourceStream, v->mImage, v->GetEndTime(), mData->mNextVideoTime,
          v->mDisplay, tracksStartTimeStamp + v->GetEndTime().ToTimeDuration(),
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
    mData->mStreamVideoWritten +=
        sourceStream->AppendToTrack(videoTrackId, &output);
  }

  if (mVideoQueue.IsFinished() && !mData->mHaveSentFinishVideo) {
    if (mData->mEOSVideoCompensation) {
      VideoSegment endSegment;
      // Calculate the deviation clock time from DecodedStream.
      auto deviation =
          FromMicroseconds(sourceStream->StreamTimeToMicroseconds(1));
      WriteVideoToMediaStream(
          sourceStream, mData->mLastVideoImage,
          mData->mNextVideoTime + deviation, mData->mNextVideoTime,
          mData->mLastVideoImageDisplaySize,
          tracksStartTimeStamp +
              (mData->mNextVideoTime + deviation).ToTimeDuration(),
          &endSegment, aPrincipalHandle);
      mData->mNextVideoTime += deviation;
      MOZ_ASSERT(endSegment.GetDuration() > 0);
      if (!aIsSameOrigin) {
        endSegment.ReplaceWithDisabled();
      }
      mData->mStreamVideoWritten +=
          sourceStream->AppendToTrack(videoTrackId, &endSegment);
    }
    sourceStream->EndTrack(videoTrackId);
    mData->mHaveSentFinishVideo = true;
  }
}

StreamTime DecodedStream::SentDuration() {
  AssertOwnerThread();

  if (!mData) {
    return 0;
  }

  return std::max(mData->mStreamAudioWritten, mData->mStreamVideoWritten);
}

void DecodedStream::SendData() {
  AssertOwnerThread();
  MOZ_ASSERT(mStartTime.isSome(), "Must be called after StartPlayback()");

  // Not yet created on the main thread. MDSM will try again later.
  if (!mData) {
    return;
  }

  SendAudio(mParams.mVolume, mSameOrigin, mPrincipalHandle);
  SendVideo(mSameOrigin, mPrincipalHandle);
}

TimeUnit DecodedStream::GetEndTime(TrackType aType) const {
  AssertOwnerThread();
  if (aType == TrackInfo::kAudioTrack && mInfo.HasAudio() && mData) {
    auto t = mStartTime.ref() +
             FramesToTimeUnit(mData->mAudioFramesWritten, mInfo.mAudio.mRate);
    if (t.IsValid()) {
      return t;
    }
  } else if (aType == TrackInfo::kVideoTrack && mData) {
    return mData->mNextVideoTime;
  }
  return TimeUnit::Zero();
}

TimeUnit DecodedStream::GetPosition(TimeStamp* aTimeStamp) const {
  AssertOwnerThread();
  // This is only called after MDSM starts playback. So mStartTime is
  // guaranteed to be something.
  MOZ_ASSERT(mStartTime.isSome());
  if (aTimeStamp) {
    *aTimeStamp = TimeStamp::Now();
  }
  return mStartTime.ref() + mLastOutputTime;
}

void DecodedStream::NotifyOutput(int64_t aTime) {
  AssertOwnerThread();
  mLastOutputTime = FromMicroseconds(aTime);
  auto currentTime = GetPosition();

  // Remove audio samples that have been played by MSG from the queue.
  RefPtr<AudioData> a = mAudioQueue.PeekFront();
  for (; a && a->mTime < currentTime;) {
    RefPtr<AudioData> releaseMe = mAudioQueue.PopFront();
    a = mAudioQueue.PeekFront();
  }
}

void DecodedStream::PlayingChanged() {
  AssertOwnerThread();

  mAbstractMainThread->Dispatch(NewRunnableMethod<bool>(
      "OutputStreamManager::SetPlaying", mOutputStreamManager,
      &OutputStreamManager::SetPlaying, mPlaying));
}

void DecodedStream::ConnectListener() {
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

void DecodedStream::DisconnectListener() {
  AssertOwnerThread();

  mAudioPushListener.Disconnect();
  mVideoPushListener.Disconnect();
  mAudioFinishListener.Disconnect();
  mVideoFinishListener.Disconnect();
}

nsCString DecodedStream::GetDebugInfo() {
  AssertOwnerThread();
  int64_t startTime = mStartTime.isSome() ? mStartTime->ToMicroseconds() : -1;
  auto str =
      nsPrintfCString("DecodedStream=%p mStartTime=%" PRId64
                      " mLastOutputTime=%" PRId64 " mPlaying=%d mData=%p",
                      this, startTime, mLastOutputTime.ToMicroseconds(),
                      mPlaying.Ref(), mData.get());
  if (mData) {
    AppendStringIfNotEmpty(str, mData->GetDebugInfo());
  }
  return std::move(str);
}

}  // namespace mozilla
