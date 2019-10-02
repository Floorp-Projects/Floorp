/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecodedStream.h"
#include "AudioSegment.h"
#include "MediaData.h"
#include "MediaQueue.h"
#include "MediaStreamGraph.h"
#include "MediaStreamListener.h"
#include "OutputStreamManager.h"
#include "SharedBuffer.h"
#include "VideoSegment.h"
#include "VideoUtils.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/gfx/Point.h"
#include "nsProxyRelease.h"

namespace mozilla {

using media::NullableTimeUnit;
using media::TimeUnit;

/*
 * A container class to make it easier to pass the playback info all the
 * way to DecodedStreamGraphListener from DecodedStream.
 */
struct PlaybackInfoInit {
  TimeUnit mStartTime;
  MediaInfo mInfo;
};

class DecodedStreamGraphListener;

class DecodedStreamTrackListener : public MediaStreamTrackListener {
 public:
  DecodedStreamTrackListener(DecodedStreamGraphListener* aGraphListener,
                             SourceMediaStream* aStream);

  void NotifyOutput(MediaStreamGraph* aGraph,
                    StreamTime aCurrentTrackTime) override;
  void NotifyEnded(MediaStreamGraph* aGraph) override;

 private:
  const RefPtr<DecodedStreamGraphListener> mGraphListener;
  const RefPtr<SourceMediaStream> mStream;
};

class DecodedStreamGraphListener {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DecodedStreamGraphListener)
 public:
  DecodedStreamGraphListener(
      SourceMediaStream* aAudioStream,
      MozPromiseHolder<DecodedStream::EndedPromise>&& aAudioEndedHolder,
      SourceMediaStream* aVideoStream,
      MozPromiseHolder<DecodedStream::EndedPromise>&& aVideoEndedHolder,
      AbstractThread* aMainThread)
      : mAudioTrackListener(
            aAudioStream
                ? MakeRefPtr<DecodedStreamTrackListener>(this, aAudioStream)
                : nullptr),
        mAudioEndedHolder(std::move(aAudioEndedHolder)),
        mVideoTrackListener(
            aVideoStream
                ? MakeRefPtr<DecodedStreamTrackListener>(this, aVideoStream)
                : nullptr),
        mVideoEndedHolder(std::move(aVideoEndedHolder)),
        mAudioStream(aAudioStream),
        mVideoStream(aVideoStream),
        mAbstractMainThread(aMainThread) {
    MOZ_ASSERT(NS_IsMainThread());
    if (mAudioTrackListener) {
      mAudioStream->AddTrackListener(mAudioTrackListener,
                                     OutputStreamManager::sTrackID);
    } else {
      mAudioEnded = true;
      mAudioEndedHolder.ResolveIfExists(true, __func__);
    }

    if (mVideoTrackListener) {
      mVideoStream->AddTrackListener(mVideoTrackListener,
                                     OutputStreamManager::sTrackID);
    } else {
      mVideoEnded = true;
      mVideoEndedHolder.ResolveIfExists(true, __func__);
    }
  }

  void NotifyOutput(SourceMediaStream* aStream, StreamTime aCurrentTrackTime) {
    if (aStream == mAudioStream) {
      if (aCurrentTrackTime >= mAudioEnd) {
        mAudioStream->EndTrack(OutputStreamManager::sTrackID);
      }
    } else if (aStream == mVideoStream) {
      if (aCurrentTrackTime >= mVideoEnd) {
        mVideoStream->EndTrack(OutputStreamManager::sTrackID);
      }
    } else {
      MOZ_CRASH("Unexpected source stream");
    }
    if (aStream != mAudioStream && mAudioStream && !mAudioEnded) {
      // Only audio playout drives the clock forward, if present and live.
      return;
    }
    MOZ_ASSERT_IF(aStream == mAudioStream, !mAudioEnded);
    MOZ_ASSERT_IF(aStream == mVideoStream, !mVideoEnded);
    mOnOutput.Notify(aStream->StreamTimeToMicroseconds(aCurrentTrackTime));
  }

  void NotifyEnded(SourceMediaStream* aStream) {
    if (aStream == mAudioStream) {
      mAudioEnded = true;
    } else if (aStream == mVideoStream) {
      mVideoEnded = true;
    } else {
      MOZ_CRASH("Unexpected source stream");
    }
    aStream->Graph()->DispatchToMainThreadStableState(
        NewRunnableMethod<RefPtr<SourceMediaStream>>(
            "DecodedStreamGraphListener::DoNotifyTrackEnded", this,
            &DecodedStreamGraphListener::DoNotifyTrackEnded, aStream));
  }

  /**
   * Tell the graph listener to end the track sourced by the given stream after
   * it has seen at least aEnd worth of output reported as processed by the
   * graph.
   *
   * A StreamTime of STREAM_TIME_MAX indicates that the track has no end and is
   * the default.
   *
   * This method of ending tracks is needed because the MediaStreamGraph
   * processes ended tracks (through SourceMediaStream::EndTrack) at the
   * beginning of an iteration, but waits until the end of the iteration to
   * process any ControlMessages. When such a ControlMessage is a listener that
   * is to be added to a track that has ended in its very first iteration, the
   * track ends before the listener tracking this ending is added. This can lead
   * to a MediaStreamTrack ending on main thread (it uses another listener)
   * before the listeners to render the track get added, potentially meaning a
   * media element doesn't progress before reaching the end although data was
   * available.
   *
   * Callable from any thread.
   */
  void EndTrackAt(SourceMediaStream* aStream, StreamTime aEnd) {
    if (aStream == mAudioStream) {
      mAudioEnd = aEnd;
    } else if (aStream == mVideoStream) {
      mVideoEnd = aEnd;
    } else {
      MOZ_CRASH("Unexpected source stream");
    }
  }

  void DoNotifyTrackEnded(SourceMediaStream* aStream) {
    MOZ_ASSERT(NS_IsMainThread());
    if (aStream == mAudioStream) {
      mAudioEndedHolder.ResolveIfExists(true, __func__);
    } else if (aStream == mVideoStream) {
      mVideoEndedHolder.ResolveIfExists(true, __func__);
    } else {
      MOZ_CRASH("Unexpected source stream");
    }
  }

  void Forget() {
    MOZ_ASSERT(NS_IsMainThread());

    if (mAudioTrackListener && !mAudioStream->IsDestroyed()) {
      mAudioStream->EndTrack(OutputStreamManager::sTrackID);
      mAudioStream->RemoveTrackListener(mAudioTrackListener,
                                        OutputStreamManager::sTrackID);
    }
    mAudioTrackListener = nullptr;
    mAudioEndedHolder.ResolveIfExists(false, __func__);

    if (mVideoTrackListener && !mVideoStream->IsDestroyed()) {
      mVideoStream->EndTrack(OutputStreamManager::sTrackID);
      mVideoStream->RemoveTrackListener(mVideoTrackListener,
                                        OutputStreamManager::sTrackID);
    }
    mVideoTrackListener = nullptr;
    mVideoEndedHolder.ResolveIfExists(false, __func__);
  }

  MediaEventSource<int64_t>& OnOutput() { return mOnOutput; }

 private:
  ~DecodedStreamGraphListener() {
    MOZ_ASSERT(mAudioEndedHolder.IsEmpty());
    MOZ_ASSERT(mVideoEndedHolder.IsEmpty());
  }

  MediaEventProducer<int64_t> mOnOutput;

  // Main thread only.
  RefPtr<DecodedStreamTrackListener> mAudioTrackListener;
  MozPromiseHolder<DecodedStream::EndedPromise> mAudioEndedHolder;
  RefPtr<DecodedStreamTrackListener> mVideoTrackListener;
  MozPromiseHolder<DecodedStream::EndedPromise> mVideoEndedHolder;

  // Graph thread only.
  bool mAudioEnded = false;
  bool mVideoEnded = false;

  // Any thread.
  const RefPtr<SourceMediaStream> mAudioStream;
  const RefPtr<SourceMediaStream> mVideoStream;
  Atomic<StreamTime> mAudioEnd{STREAM_TIME_MAX};
  Atomic<StreamTime> mVideoEnd{STREAM_TIME_MAX};
  const RefPtr<AbstractThread> mAbstractMainThread;
};

DecodedStreamTrackListener::DecodedStreamTrackListener(
    DecodedStreamGraphListener* aGraphListener, SourceMediaStream* aStream)
    : mGraphListener(aGraphListener), mStream(aStream) {}

void DecodedStreamTrackListener::NotifyOutput(MediaStreamGraph* aGraph,
                                              StreamTime aCurrentTrackTime) {
  mGraphListener->NotifyOutput(mStream, aCurrentTrackTime);
}

void DecodedStreamTrackListener::NotifyEnded(MediaStreamGraph* aGraph) {
  mGraphListener->NotifyEnded(mStream);
}

/**
 * All MediaStream-related data is protected by the decoder's monitor. We have
 * at most one DecodedStreamData per MediaDecoder. Its streams are used as
 * inputs for all output tracks created by OutputStreamManager after calls to
 * captureStream/UntilEnded. Seeking creates new source streams, as does
 * replaying after the input as ended. In the latter case, the new sources are
 * not connected to tracks created by captureStreamUntilEnded.
 */
class DecodedStreamData final {
 public:
  DecodedStreamData(
      OutputStreamManager* aOutputStreamManager, PlaybackInfoInit&& aInit,
      RefPtr<SourceMediaStream> aAudioStream,
      RefPtr<SourceMediaStream> aVideoStream,
      MozPromiseHolder<DecodedStream::EndedPromise>&& aAudioEndedPromise,
      MozPromiseHolder<DecodedStream::EndedPromise>&& aVideoEndedPromise,
      AbstractThread* aMainThread);
  ~DecodedStreamData();
  MediaEventSource<int64_t>& OnOutput();
  void Forget();
  void GetDebugInfo(dom::DecodedStreamDataDebugInfo& aInfo);

  void WriteVideoToSegment(layers::Image* aImage, const TimeUnit& aStart,
                           const TimeUnit& aEnd,
                           const gfx::IntSize& aIntrinsicSize,
                           const TimeStamp& aTimeStamp, VideoSegment* aOutput,
                           const PrincipalHandle& aPrincipalHandle);

  /* The following group of fields are protected by the decoder's monitor
   * and can be read or written on any thread.
   */
  // Count of audio frames written to the stream
  int64_t mAudioFramesWritten;
  // Count of video frames written to the stream in the stream's rate
  StreamTime mVideoStreamWritten;
  // Count of audio frames written to the stream in the stream's rate
  StreamTime mAudioStreamWritten;
  // mNextAudioTime is the end timestamp for the last packet sent to the stream.
  // Therefore audio packets starting at or after this time need to be copied
  // to the output stream.
  TimeUnit mNextAudioTime;
  // mLastVideoStartTime is the start timestamp for the last packet sent to the
  // stream. Therefore video packets starting after this time need to be copied
  // to the output stream.
  NullableTimeUnit mLastVideoStartTime;
  // mLastVideoEndTime is the end timestamp for the last packet sent to the
  // stream. It is used to adjust durations of chunks sent to the output stream
  // when there are overlaps in VideoData.
  NullableTimeUnit mLastVideoEndTime;
  // The timestamp of the last frame, so we can ensure time never goes
  // backwards.
  TimeStamp mLastVideoTimeStamp;
  // The last video image sent to the stream. Useful if we need to replicate
  // the image.
  RefPtr<layers::Image> mLastVideoImage;
  gfx::IntSize mLastVideoImageDisplaySize;
  bool mHaveSentFinishAudio;
  bool mHaveSentFinishVideo;

  const RefPtr<SourceMediaStream> mAudioStream;
  const RefPtr<SourceMediaStream> mVideoStream;
  const RefPtr<DecodedStreamGraphListener> mListener;

  const RefPtr<OutputStreamManager> mOutputStreamManager;
  const RefPtr<AbstractThread> mAbstractMainThread;
};

DecodedStreamData::DecodedStreamData(
    OutputStreamManager* aOutputStreamManager, PlaybackInfoInit&& aInit,
    RefPtr<SourceMediaStream> aAudioStream,
    RefPtr<SourceMediaStream> aVideoStream,
    MozPromiseHolder<DecodedStream::EndedPromise>&& aAudioEndedPromise,
    MozPromiseHolder<DecodedStream::EndedPromise>&& aVideoEndedPromise,
    AbstractThread* aMainThread)
    : mAudioFramesWritten(0),
      mVideoStreamWritten(0),
      mAudioStreamWritten(0),
      mNextAudioTime(aInit.mStartTime),
      mHaveSentFinishAudio(false),
      mHaveSentFinishVideo(false),
      mAudioStream(std::move(aAudioStream)),
      mVideoStream(std::move(aVideoStream)),
      // DecodedStreamGraphListener will resolve these promises.
      mListener(MakeRefPtr<DecodedStreamGraphListener>(
          mAudioStream, std::move(aAudioEndedPromise), mVideoStream,
          std::move(aVideoEndedPromise), aMainThread)),
      mOutputStreamManager(aOutputStreamManager),
      mAbstractMainThread(aMainThread) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(
      mOutputStreamManager->HasTracks(mAudioStream, mVideoStream),
      "Tracks must be pre-created on main thread");
  if (mAudioStream) {
    mAudioStream->AddAudioTrack(OutputStreamManager::sTrackID,
                                aInit.mInfo.mAudio.mRate, new AudioSegment());
  }
  if (mVideoStream) {
    mVideoStream->AddTrack(OutputStreamManager::sTrackID, new VideoSegment());
  }
}

DecodedStreamData::~DecodedStreamData() { MOZ_ASSERT(NS_IsMainThread()); }

MediaEventSource<int64_t>& DecodedStreamData::OnOutput() {
  return mListener->OnOutput();
}

void DecodedStreamData::Forget() { mListener->Forget(); }

void DecodedStreamData::GetDebugInfo(dom::DecodedStreamDataDebugInfo& aInfo) {
  aInfo.mInstance = NS_ConvertUTF8toUTF16(nsPrintfCString("%p", this));
  aInfo.mAudioFramesWritten = mAudioFramesWritten;
  aInfo.mStreamAudioWritten = mAudioStreamWritten;
  aInfo.mNextAudioTime = mNextAudioTime.ToMicroseconds();
  aInfo.mLastVideoStartTime =
      mLastVideoStartTime.valueOr(TimeUnit::FromMicroseconds(-1))
          .ToMicroseconds();
  aInfo.mLastVideoEndTime =
      mLastVideoEndTime.valueOr(TimeUnit::FromMicroseconds(-1))
          .ToMicroseconds();
  aInfo.mHaveSentFinishAudio = mHaveSentFinishAudio;
  aInfo.mHaveSentFinishVideo = mHaveSentFinishVideo;
}

DecodedStream::DecodedStream(AbstractThread* aOwnerThread,
                             AbstractThread* aMainThread,
                             MediaQueue<AudioData>& aAudioQueue,
                             MediaQueue<VideoData>& aVideoQueue,
                             OutputStreamManager* aOutputStreamManager)
    : mOwnerThread(aOwnerThread),
      mAbstractMainThread(aMainThread),
      mOutputStreamManager(aOutputStreamManager),
      mWatchManager(this, mOwnerThread),
      mPlaying(false, "DecodedStream::mPlaying"),
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

const MediaSink::PlaybackParams& DecodedStream::GetPlaybackParams() const {
  AssertOwnerThread();
  return mParams;
}

void DecodedStream::SetPlaybackParams(const PlaybackParams& aParams) {
  AssertOwnerThread();
  mParams = aParams;
}

RefPtr<DecodedStream::EndedPromise> DecodedStream::OnEnded(TrackType aType) {
  AssertOwnerThread();
  MOZ_ASSERT(mStartTime.isSome());

  if (aType == TrackInfo::kAudioTrack && mInfo.HasAudio()) {
    return mAudioEndedPromise;
  } else if (aType == TrackInfo::kVideoTrack && mInfo.HasVideo()) {
    return mVideoEndedPromise;
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
    typedef MozPromiseHolder<MediaSink::EndedPromise> Promise;

   public:
    R(PlaybackInfoInit&& aInit, Promise&& aAudioEndedPromise,
      Promise&& aVideoEndedPromise, OutputStreamManager* aManager,
      AbstractThread* aMainThread)
        : Runnable("CreateDecodedStreamData"),
          mInit(std::move(aInit)),
          mAudioEndedPromise(std::move(aAudioEndedPromise)),
          mVideoEndedPromise(std::move(aVideoEndedPromise)),
          mOutputStreamManager(aManager),
          mAbstractMainThread(aMainThread) {}
    NS_IMETHOD Run() override {
      MOZ_ASSERT(NS_IsMainThread());
      // No need to create a source stream when there are no output streams.
      // This happens when RemoveOutput() is called immediately after
      // StartPlayback().
      if (mOutputStreamManager->IsEmpty()) {
        // Resolve the promise to indicate the end of playback.
        mAudioEndedPromise.Resolve(true, __func__);
        mVideoEndedPromise.Resolve(true, __func__);
        return NS_OK;
      }
      RefPtr<SourceMediaStream> audioStream =
          mOutputStreamManager->GetPrecreatedTrackOfType(MediaSegment::AUDIO);
      if (mInit.mInfo.HasAudio() && !audioStream) {
        MOZ_DIAGNOSTIC_ASSERT(
            !mOutputStreamManager->HasTrackType(MediaSegment::AUDIO));
        audioStream = mOutputStreamManager->AddTrack(MediaSegment::AUDIO);
      }
      RefPtr<SourceMediaStream> videoStream =
          mOutputStreamManager->GetPrecreatedTrackOfType(MediaSegment::VIDEO);
      if (mInit.mInfo.HasVideo() && !videoStream) {
        MOZ_DIAGNOSTIC_ASSERT(
            !mOutputStreamManager->HasTrackType(MediaSegment::VIDEO));
        videoStream = mOutputStreamManager->AddTrack(MediaSegment::VIDEO);
      }
      mData = MakeUnique<DecodedStreamData>(
          mOutputStreamManager, std::move(mInit), std::move(audioStream),
          std::move(videoStream), std::move(mAudioEndedPromise),
          std::move(mVideoEndedPromise), mAbstractMainThread);
      return NS_OK;
    }
    UniquePtr<DecodedStreamData> ReleaseData() { return std::move(mData); }

   private:
    PlaybackInfoInit mInit;
    Promise mAudioEndedPromise;
    Promise mVideoEndedPromise;
    RefPtr<OutputStreamManager> mOutputStreamManager;
    UniquePtr<DecodedStreamData> mData;
    const RefPtr<AbstractThread> mAbstractMainThread;
  };

  MozPromiseHolder<DecodedStream::EndedPromise> audioEndedHolder;
  mAudioEndedPromise = audioEndedHolder.Ensure(__func__);
  MozPromiseHolder<DecodedStream::EndedPromise> videoEndedHolder;
  mVideoEndedPromise = videoEndedHolder.Ensure(__func__);
  PlaybackInfoInit init{aStartTime, aInfo};
  nsCOMPtr<nsIRunnable> r = new R(std::move(init), std::move(audioEndedHolder),
                                  std::move(videoEndedHolder),
                                  mOutputStreamManager, mAbstractMainThread);
  SyncRunnable::DispatchToThread(
      SystemGroup::EventTargetFor(TaskCategory::Other), r);
  mData = static_cast<R*>(r.get())->ReleaseData();

  if (mData) {
    mOutputListener = mData->OnOutput().Connect(mOwnerThread, this,
                                                &DecodedStream::NotifyOutput);
    SendData();
  }
  return NS_OK;
}

void DecodedStream::Stop() {
  AssertOwnerThread();
  MOZ_ASSERT(mStartTime.isSome(), "playback not started.");

  DisconnectListener();
  ResetVideo(mPrincipalHandle);
  mStartTime.reset();
  mAudioEndedPromise = nullptr;
  mVideoEndedPromise = nullptr;

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

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "DecodedStream::DestroyData",
      [data = std::move(aData), manager = mOutputStreamManager]() {
        data->Forget();
        manager->RemoveTracks();
      }));
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
    channels.AppendElement(bufferData + i * audio->Frames());
  }
  aOutput->AppendFrames(buffer.forget(), channels, audio->Frames(),
                        aPrincipalHandle);
  aStream->mAudioFramesWritten += audio->Frames();

  aStream->mNextAudioTime = audio->GetEndTime();
}

void DecodedStream::SendAudio(double aVolume,
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

  // It's OK to hold references to the AudioData because AudioData
  // is ref-counted.
  mAudioQueue.GetElementsAfter(mData->mNextAudioTime, &audio);
  for (uint32_t i = 0; i < audio.Length(); ++i) {
    SendStreamAudio(mData.get(), mStartTime.ref(), audio[i], &output, rate,
                    aPrincipalHandle);
  }

  output.ApplyVolume(aVolume);

  // |mNextAudioTime| is updated as we process each audio sample in
  // SendStreamAudio().
  if (output.GetDuration() > 0) {
    mData->mAudioStreamWritten += mData->mAudioStream->AppendToTrack(
        OutputStreamManager::sTrackID, &output);
  }

  if (mAudioQueue.IsFinished() && !mData->mHaveSentFinishAudio) {
    mData->mListener->EndTrackAt(mData->mAudioStream,
                                 mData->mAudioStreamWritten);
    mData->mHaveSentFinishAudio = true;
  }
}

void DecodedStreamData::WriteVideoToSegment(
    layers::Image* aImage, const TimeUnit& aStart, const TimeUnit& aEnd,
    const gfx::IntSize& aIntrinsicSize, const TimeStamp& aTimeStamp,
    VideoSegment* aOutput, const PrincipalHandle& aPrincipalHandle) {
  RefPtr<layers::Image> image = aImage;
  auto end =
      mVideoStream->MicrosecondsToStreamTimeRoundDown(aEnd.ToMicroseconds());
  auto start =
      mVideoStream->MicrosecondsToStreamTimeRoundDown(aStart.ToMicroseconds());
  aOutput->AppendFrame(image.forget(), aIntrinsicSize, aPrincipalHandle, false,
                       aTimeStamp);
  // Extend this so we get accurate durations for all frames.
  // Because this track is pushed, we need durations so the graph can track
  // when playout of the track has finished.
  aOutput->ExtendLastFrameBy(end - start);

  mLastVideoStartTime = Some(aStart);
  mLastVideoEndTime = Some(aEnd);
  mLastVideoTimeStamp = aTimeStamp;
}

static bool ZeroDurationAtLastChunk(VideoSegment& aInput) {
  // Get the last video frame's start time in VideoSegment aInput.
  // If the start time is equal to the duration of aInput, means the last video
  // frame's duration is zero.
  StreamTime lastVideoStratTime;
  aInput.GetLastFrame(&lastVideoStratTime);
  return lastVideoStratTime == aInput.GetDuration();
}

void DecodedStream::ResetVideo(const PrincipalHandle& aPrincipalHandle) {
  AssertOwnerThread();

  if (!mData) {
    return;
  }

  if (!mInfo.HasVideo()) {
    return;
  }

  VideoSegment resetter;
  TimeStamp currentTime;
  TimeUnit currentPosition = GetPosition(&currentTime);

  // Giving direct consumers a frame (really *any* frame, so in this case:
  // nullptr) at an earlier time than the previous, will signal to that consumer
  // to discard any frames ahead in time of the new frame. To be honest, this is
  // an ugly hack because the direct listeners of the MediaStreamGraph do not
  // have an API that supports clearing the future frames. ImageContainer and
  // VideoFrameContainer do though, and we will need to move to a similar API
  // for video tracks as part of bug 1493618.
  resetter.AppendFrame(nullptr, mData->mLastVideoImageDisplaySize,
                       aPrincipalHandle, false, currentTime);
  mData->mVideoStream->AppendToTrack(OutputStreamManager::sTrackID, &resetter);

  // Consumer buffers have been reset. We now set the next time to the start
  // time of the current frame, so that it can be displayed again on resuming.
  if (RefPtr<VideoData> v = mVideoQueue.PeekFront()) {
    mData->mLastVideoStartTime = Some(v->mTime - TimeUnit::FromMicroseconds(1));
    mData->mLastVideoEndTime = Some(v->mTime);
  } else {
    // There was no current frame in the queue. We set the next time to the
    // current time, so we at least don't resume starting in the future.
    mData->mLastVideoStartTime =
        Some(currentPosition - TimeUnit::FromMicroseconds(1));
    mData->mLastVideoEndTime = Some(currentPosition);
  }

  mData->mLastVideoTimeStamp = currentTime;
}

void DecodedStream::SendVideo(const PrincipalHandle& aPrincipalHandle) {
  AssertOwnerThread();

  if (!mInfo.HasVideo()) {
    return;
  }

  if (mData->mHaveSentFinishVideo) {
    return;
  }

  VideoSegment output;
  AutoTArray<RefPtr<VideoData>, 10> video;

  // It's OK to hold references to the VideoData because VideoData
  // is ref-counted.
  mVideoQueue.GetElementsAfter(
      mData->mLastVideoStartTime.valueOr(mStartTime.ref()), &video);

  TimeStamp currentTime;
  TimeUnit currentPosition = GetPosition(&currentTime);

  if (mData->mLastVideoTimeStamp.IsNull()) {
    mData->mLastVideoTimeStamp = currentTime;
  }

  for (uint32_t i = 0; i < video.Length(); ++i) {
    VideoData* v = video[i];
    TimeUnit lastStart = mData->mLastVideoStartTime.valueOr(
        mStartTime.ref() - TimeUnit::FromMicroseconds(1));
    TimeUnit lastEnd = mData->mLastVideoEndTime.valueOr(mStartTime.ref());

    if (lastEnd < v->mTime) {
      // Write last video frame to catch up. mLastVideoImage can be null here
      // which is fine, it just means there's no video.

      // TODO: |mLastVideoImage| should come from the last image rendered
      // by the state machine. This will avoid the black frame when capture
      // happens in the middle of playback (especially in th middle of a
      // video frame). E.g. if we have a video frame that is 30 sec long
      // and capture happens at 15 sec, we'll have to append a black frame
      // that is 15 sec long.
      TimeStamp t =
          std::max(mData->mLastVideoTimeStamp,
                   currentTime + (lastEnd - currentPosition).ToTimeDuration());
      mData->WriteVideoToSegment(mData->mLastVideoImage, lastEnd, v->mTime,
                                 mData->mLastVideoImageDisplaySize, t, &output,
                                 aPrincipalHandle);
      lastEnd = v->mTime;
    }

    if (lastStart < v->mTime) {
      // This frame starts after the last frame's start. Note that this could be
      // before the last frame's end time for some videos. This only matters for
      // the track's lifetime in the MSG, as rendering is based on timestamps,
      // aka frame start times.
      TimeStamp t =
          std::max(mData->mLastVideoTimeStamp,
                   currentTime + (lastEnd - currentPosition).ToTimeDuration());
      TimeUnit end = std::max(
          v->GetEndTime(),
          lastEnd + TimeUnit::FromMicroseconds(
                        mData->mVideoStream->StreamTimeToMicroseconds(1) + 1));
      mData->mLastVideoImage = v->mImage;
      mData->mLastVideoImageDisplaySize = v->mDisplay;
      mData->WriteVideoToSegment(v->mImage, lastEnd, end, v->mDisplay, t,
                                 &output, aPrincipalHandle);
    }
  }

  // Check the output is not empty.
  bool compensateEOS = false;
  bool forceBlack = false;
  if (output.GetLastFrame()) {
    compensateEOS = ZeroDurationAtLastChunk(output);
  }

  if (output.GetDuration() > 0) {
    mData->mVideoStreamWritten += mData->mVideoStream->AppendToTrack(
        OutputStreamManager::sTrackID, &output);
  }

  if (mVideoQueue.IsFinished() && !mData->mHaveSentFinishVideo) {
    if (!mData->mLastVideoImage) {
      // We have video, but the video queue finished before we received any
      // frame. We insert a black frame to progress any consuming
      // HTMLMediaElement. This mirrors the behavior of VideoSink.

      // Force a frame - can be null
      compensateEOS = true;
      // Force frame to be black
      forceBlack = true;
      // Override the frame's size (will be 0x0 otherwise)
      mData->mLastVideoImageDisplaySize = mInfo.mVideo.mDisplay;
    }
    if (compensateEOS) {
      VideoSegment endSegment;
      // Calculate the deviation clock time from DecodedStream.
      // We round the nr of microseconds up, because WriteVideoToSegment
      // will round the conversion from microseconds to StreamTime down.
      auto deviation = TimeUnit::FromMicroseconds(
          mData->mVideoStream->StreamTimeToMicroseconds(1) + 1);
      auto start = mData->mLastVideoEndTime.valueOr(mStartTime.ref());
      mData->WriteVideoToSegment(
          mData->mLastVideoImage, start, start + deviation,
          mData->mLastVideoImageDisplaySize,
          currentTime + (start + deviation - currentPosition).ToTimeDuration(),
          &endSegment, aPrincipalHandle);
      MOZ_ASSERT(endSegment.GetDuration() > 0);
      if (forceBlack) {
        endSegment.ReplaceWithDisabled();
      }
      mData->mVideoStreamWritten += mData->mVideoStream->AppendToTrack(
          OutputStreamManager::sTrackID, &endSegment);
    }
    mData->mListener->EndTrackAt(mData->mVideoStream,
                                 mData->mVideoStreamWritten);
    mData->mHaveSentFinishVideo = true;
  }
}

void DecodedStream::SendData() {
  AssertOwnerThread();

  // Not yet created on the main thread. MDSM will try again later.
  if (!mData) {
    return;
  }

  if (!mPlaying) {
    return;
  }

  SendAudio(mParams.mVolume, mPrincipalHandle);
  SendVideo(mPrincipalHandle);
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
    return mData->mLastVideoEndTime.valueOr(mStartTime.ref());
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
  TimeUnit time = TimeUnit::FromMicroseconds(aTime);
  if (time == mLastOutputTime) {
    return;
  }
  MOZ_ASSERT(mLastOutputTime < time);
  mLastOutputTime = time;
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

  if (!mPlaying) {
    // On seek or pause we discard future frames.
    ResetVideo(mPrincipalHandle);
  }

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
  mWatchManager.Watch(mPlaying, &DecodedStream::SendData);
}

void DecodedStream::DisconnectListener() {
  AssertOwnerThread();

  mAudioPushListener.Disconnect();
  mVideoPushListener.Disconnect();
  mAudioFinishListener.Disconnect();
  mVideoFinishListener.Disconnect();
  mWatchManager.Unwatch(mPlaying, &DecodedStream::SendData);
}

void DecodedStream::GetDebugInfo(dom::MediaSinkDebugInfo& aInfo) {
  AssertOwnerThread();
  int64_t startTime = mStartTime.isSome() ? mStartTime->ToMicroseconds() : -1;
  aInfo.mDecodedStream.mInstance =
      NS_ConvertUTF8toUTF16(nsPrintfCString("%p", this));
  aInfo.mDecodedStream.mStartTime = startTime;
  aInfo.mDecodedStream.mLastOutputTime = mLastOutputTime.ToMicroseconds();
  aInfo.mDecodedStream.mPlaying = mPlaying.Ref();
  auto lastAudio = mAudioQueue.PeekBack();
  aInfo.mDecodedStream.mLastAudio =
      lastAudio ? lastAudio->GetEndTime().ToMicroseconds() : -1;
  aInfo.mDecodedStream.mAudioQueueFinished = mAudioQueue.IsFinished();
  aInfo.mDecodedStream.mAudioQueueSize = mAudioQueue.GetSize();
  if (mData) {
    mData->GetDebugInfo(aInfo.mDecodedStream.mData);
  }
}

}  // namespace mozilla
