/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecodedStream.h"

#include "AudioDecoderInputTrack.h"
#include "AudioSegment.h"
#include "MediaData.h"
#include "MediaDecoderStateMachine.h"
#include "MediaQueue.h"
#include "MediaTrackGraph.h"
#include "MediaTrackListener.h"
#include "SharedBuffer.h"
#include "Tracing.h"
#include "VideoSegment.h"
#include "VideoUtils.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ProfilerMarkerTypes.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsProxyRelease.h"

namespace mozilla {

using media::NullableTimeUnit;
using media::TimeUnit;

extern LazyLogModule gMediaDecoderLog;

#define LOG_DS(type, fmt, ...)    \
  MOZ_LOG(gMediaDecoderLog, type, \
          ("DecodedStream=%p " fmt, this, ##__VA_ARGS__))

#define PLAYBACK_PROFILER_MARKER(markerString) \
  PROFILER_MARKER_TEXT(FUNCTION_SIGNATURE, MEDIA_PLAYBACK, {}, markerString)

/*
 * A container class to make it easier to pass the playback info all the
 * way to DecodedStreamGraphListener from DecodedStream.
 */
struct PlaybackInfoInit {
  TimeUnit mStartTime;
  MediaInfo mInfo;
};

class DecodedStreamGraphListener;

class SourceVideoTrackListener : public MediaTrackListener {
 public:
  SourceVideoTrackListener(DecodedStreamGraphListener* aGraphListener,
                           SourceMediaTrack* aVideoTrack,
                           MediaTrack* aAudioTrack,
                           nsISerialEventTarget* aDecoderThread);

  void NotifyOutput(MediaTrackGraph* aGraph,
                    TrackTime aCurrentTrackTime) override;
  void NotifyEnded(MediaTrackGraph* aGraph) override;

 private:
  const RefPtr<DecodedStreamGraphListener> mGraphListener;
  const RefPtr<SourceMediaTrack> mVideoTrack;
  const RefPtr<const MediaTrack> mAudioTrack;
  const RefPtr<nsISerialEventTarget> mDecoderThread;
  TrackTime mLastVideoOutputTime = 0;
};

class DecodedStreamGraphListener {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DecodedStreamGraphListener)
 public:
  DecodedStreamGraphListener(
      nsISerialEventTarget* aDecoderThread, AudioDecoderInputTrack* aAudioTrack,
      MozPromiseHolder<DecodedStream::EndedPromise>&& aAudioEndedHolder,
      SourceMediaTrack* aVideoTrack,
      MozPromiseHolder<DecodedStream::EndedPromise>&& aVideoEndedHolder)
      : mDecoderThread(aDecoderThread),
        mVideoTrackListener(
            aVideoTrack ? MakeRefPtr<SourceVideoTrackListener>(
                              this, aVideoTrack, aAudioTrack, aDecoderThread)
                        : nullptr),
        mAudioEndedHolder(std::move(aAudioEndedHolder)),
        mVideoEndedHolder(std::move(aVideoEndedHolder)),
        mAudioTrack(aAudioTrack),
        mVideoTrack(aVideoTrack) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mDecoderThread);

    if (mAudioTrack) {
      mOnAudioOutput = mAudioTrack->OnOutput().Connect(
          mDecoderThread,
          [self = RefPtr<DecodedStreamGraphListener>(this)](TrackTime aTime) {
            self->NotifyOutput(MediaSegment::AUDIO, aTime);
          });
      mOnAudioEnd = mAudioTrack->OnEnd().Connect(
          mDecoderThread, [self = RefPtr<DecodedStreamGraphListener>(this)]() {
            self->NotifyEnded(MediaSegment::AUDIO);
          });
    } else {
      mAudioEnded = true;
      mAudioEndedHolder.ResolveIfExists(true, __func__);
    }

    if (mVideoTrackListener) {
      mVideoTrack->AddListener(mVideoTrackListener);
    } else {
      mVideoEnded = true;
      mVideoEndedHolder.ResolveIfExists(true, __func__);
    }
  }

  void Close() {
    AssertOnDecoderThread();
    if (mAudioTrack) {
      mAudioTrack->Close();
    }
    if (mVideoTrack) {
      mVideoTrack->End();
    }
    mAudioEndedHolder.ResolveIfExists(false, __func__);
    mVideoEndedHolder.ResolveIfExists(false, __func__);
    mOnAudioOutput.DisconnectIfExists();
    mOnAudioEnd.DisconnectIfExists();
  }

  void NotifyOutput(MediaSegment::Type aType, TrackTime aCurrentTrackTime) {
    AssertOnDecoderThread();
    if (aType == MediaSegment::AUDIO) {
      mAudioOutputFrames = aCurrentTrackTime;
    } else if (aType == MediaSegment::VIDEO) {
      if (aCurrentTrackTime >= mVideoEndTime) {
        mVideoTrack->End();
      }
    } else {
      MOZ_CRASH("Unexpected track type");
    }

    MOZ_ASSERT_IF(aType == MediaSegment::AUDIO, !mAudioEnded);
    MOZ_ASSERT_IF(aType == MediaSegment::VIDEO, !mVideoEnded);
    MOZ_ASSERT(aCurrentTrackTime > mLastOutputTime);
    mLastOutputTime = aCurrentTrackTime;

    // Only when audio track doesn't exists or has reached the end, video
    // track should drive the clock.
    MOZ_ASSERT_IF(aType == MediaSegment::VIDEO, mAudioEnded);
    const MediaTrack* track = aType == MediaSegment::VIDEO
                                  ? static_cast<MediaTrack*>(mVideoTrack)
                                  : static_cast<MediaTrack*>(mAudioTrack);
    mOnOutput.Notify(track->TrackTimeToMicroseconds(aCurrentTrackTime));
  }

  void NotifyEnded(MediaSegment::Type aType) {
    AssertOnDecoderThread();
    if (aType == MediaSegment::AUDIO) {
      MOZ_ASSERT(!mAudioEnded);
      mAudioEnded = true;
      mAudioEndedHolder.ResolveIfExists(true, __func__);
    } else if (aType == MediaSegment::VIDEO) {
      MOZ_ASSERT(!mVideoEnded);
      mVideoEnded = true;
      mVideoEndedHolder.ResolveIfExists(true, __func__);
    } else {
      MOZ_CRASH("Unexpected track type");
    }
  }

  /**
   * Tell the graph listener to end the track sourced by the given track after
   * it has seen at least aEnd worth of output reported as processed by the
   * graph.
   *
   * A TrackTime of TRACK_TIME_MAX indicates that the track has no end and is
   * the default.
   *
   * This method of ending tracks is needed because the MediaTrackGraph
   * processes ended tracks (through SourceMediaTrack::EndTrack) at the
   * beginning of an iteration, but waits until the end of the iteration to
   * process any ControlMessages. When such a ControlMessage is a listener that
   * is to be added to a track that has ended in its very first iteration, the
   * track ends before the listener tracking this ending is added. This can lead
   * to a MediaStreamTrack ending on main thread (it uses another listener)
   * before the listeners to render the track get added, potentially meaning a
   * media element doesn't progress before reaching the end although data was
   * available.
   */
  void EndVideoTrackAt(MediaTrack* aTrack, TrackTime aEnd) {
    AssertOnDecoderThread();
    MOZ_DIAGNOSTIC_ASSERT(aTrack == mVideoTrack);
    mVideoEndTime = aEnd;
  }

  void Forget() {
    MOZ_ASSERT(NS_IsMainThread());
    if (mVideoTrackListener && !mVideoTrack->IsDestroyed()) {
      mVideoTrack->RemoveListener(mVideoTrackListener);
    }
    mVideoTrackListener = nullptr;
  }

  TrackTime GetAudioFramesPlayed() {
    AssertOnDecoderThread();
    return mAudioOutputFrames;
  }

  MediaEventSource<int64_t>& OnOutput() { return mOnOutput; }

 private:
  ~DecodedStreamGraphListener() {
    MOZ_ASSERT(mAudioEndedHolder.IsEmpty());
    MOZ_ASSERT(mVideoEndedHolder.IsEmpty());
  }

  inline void AssertOnDecoderThread() const {
    MOZ_ASSERT(mDecoderThread->IsOnCurrentThread());
  }

  const RefPtr<nsISerialEventTarget> mDecoderThread;

  // Accessible on any thread, but only notify on the decoder thread.
  MediaEventProducer<int64_t> mOnOutput;

  RefPtr<SourceVideoTrackListener> mVideoTrackListener;

  // These can be resolved on the main thread on creation if there is no
  // corresponding track, otherwise they are resolved on the decoder thread.
  MozPromiseHolder<DecodedStream::EndedPromise> mAudioEndedHolder;
  MozPromiseHolder<DecodedStream::EndedPromise> mVideoEndedHolder;

  // Decoder thread only.
  TrackTime mAudioOutputFrames = 0;
  TrackTime mLastOutputTime = 0;
  bool mAudioEnded = false;
  bool mVideoEnded = false;

  // Any thread.
  const RefPtr<AudioDecoderInputTrack> mAudioTrack;
  const RefPtr<SourceMediaTrack> mVideoTrack;
  MediaEventListener mOnAudioOutput;
  MediaEventListener mOnAudioEnd;
  Atomic<TrackTime> mVideoEndTime{TRACK_TIME_MAX};
};

SourceVideoTrackListener::SourceVideoTrackListener(
    DecodedStreamGraphListener* aGraphListener, SourceMediaTrack* aVideoTrack,
    MediaTrack* aAudioTrack, nsISerialEventTarget* aDecoderThread)
    : mGraphListener(aGraphListener),
      mVideoTrack(aVideoTrack),
      mAudioTrack(aAudioTrack),
      mDecoderThread(aDecoderThread) {}

void SourceVideoTrackListener::NotifyOutput(MediaTrackGraph* aGraph,
                                            TrackTime aCurrentTrackTime) {
  aGraph->AssertOnGraphThreadOrNotRunning();
  if (mAudioTrack && !mAudioTrack->Ended()) {
    // Only audio playout drives the clock forward, if present and live.
    return;
  }
  // The graph can iterate without time advancing, but the invariant is that
  // time can never go backwards.
  if (aCurrentTrackTime <= mLastVideoOutputTime) {
    MOZ_ASSERT(aCurrentTrackTime == mLastVideoOutputTime);
    return;
  }
  mLastVideoOutputTime = aCurrentTrackTime;
  mDecoderThread->Dispatch(NS_NewRunnableFunction(
      "SourceVideoTrackListener::NotifyOutput",
      [self = RefPtr<SourceVideoTrackListener>(this), aCurrentTrackTime]() {
        self->mGraphListener->NotifyOutput(MediaSegment::VIDEO,
                                           aCurrentTrackTime);
      }));
}

void SourceVideoTrackListener::NotifyEnded(MediaTrackGraph* aGraph) {
  aGraph->AssertOnGraphThreadOrNotRunning();
  mDecoderThread->Dispatch(NS_NewRunnableFunction(
      "SourceVideoTrackListener::NotifyEnded",
      [self = RefPtr<SourceVideoTrackListener>(this)]() {
        self->mGraphListener->NotifyEnded(MediaSegment::VIDEO);
      }));
}

/**
 * All MediaStream-related data is protected by the decoder's monitor. We have
 * at most one DecodedStreamData per MediaDecoder. XXX Its tracks are used as
 * inputs for all output tracks created by OutputStreamManager after calls to
 * captureStream/UntilEnded. Seeking creates new source tracks, as does
 * replaying after the input as ended. In the latter case, the new sources are
 * not connected to tracks created by captureStreamUntilEnded.
 */
class DecodedStreamData final {
 public:
  DecodedStreamData(
      PlaybackInfoInit&& aInit, MediaTrackGraph* aGraph,
      RefPtr<ProcessedMediaTrack> aAudioOutputTrack,
      RefPtr<ProcessedMediaTrack> aVideoOutputTrack,
      MozPromiseHolder<DecodedStream::EndedPromise>&& aAudioEndedPromise,
      MozPromiseHolder<DecodedStream::EndedPromise>&& aVideoEndedPromise,
      float aPlaybackRate, float aVolume, bool aPreservesPitch,
      nsISerialEventTarget* aDecoderThread);
  ~DecodedStreamData();
  MediaEventSource<int64_t>& OnOutput();
  // This is used to mark track as closed and should be called before Forget().
  // Decoder thread only.
  void Close();
  // After calling this function, the DecodedStreamData would be destroyed.
  // Main thread only.
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
  // Count of audio frames written to the track
  int64_t mAudioFramesWritten;
  // Count of video frames written to the track in the track's rate
  TrackTime mVideoTrackWritten;
  // mNextAudioTime is the end timestamp for the last packet sent to the track.
  // Therefore audio packets starting at or after this time need to be copied
  // to the output track.
  TimeUnit mNextAudioTime;
  // mLastVideoStartTime is the start timestamp for the last packet sent to the
  // track. Therefore video packets starting after this time need to be copied
  // to the output track.
  NullableTimeUnit mLastVideoStartTime;
  // mLastVideoEndTime is the end timestamp for the last packet sent to the
  // track. It is used to adjust durations of chunks sent to the output track
  // when there are overlaps in VideoData.
  NullableTimeUnit mLastVideoEndTime;
  // The timestamp of the last frame, so we can ensure time never goes
  // backwards.
  TimeStamp mLastVideoTimeStamp;
  // The last video image sent to the track. Useful if we need to replicate
  // the image.
  RefPtr<layers::Image> mLastVideoImage;
  gfx::IntSize mLastVideoImageDisplaySize;
  bool mHaveSentFinishAudio;
  bool mHaveSentFinishVideo;

  const RefPtr<AudioDecoderInputTrack> mAudioTrack;
  const RefPtr<SourceMediaTrack> mVideoTrack;
  const RefPtr<ProcessedMediaTrack> mAudioOutputTrack;
  const RefPtr<ProcessedMediaTrack> mVideoOutputTrack;
  const RefPtr<MediaInputPort> mAudioPort;
  const RefPtr<MediaInputPort> mVideoPort;
  const RefPtr<DecodedStream::EndedPromise> mAudioEndedPromise;
  const RefPtr<DecodedStream::EndedPromise> mVideoEndedPromise;
  const RefPtr<DecodedStreamGraphListener> mListener;
};

DecodedStreamData::DecodedStreamData(
    PlaybackInfoInit&& aInit, MediaTrackGraph* aGraph,
    RefPtr<ProcessedMediaTrack> aAudioOutputTrack,
    RefPtr<ProcessedMediaTrack> aVideoOutputTrack,
    MozPromiseHolder<DecodedStream::EndedPromise>&& aAudioEndedPromise,
    MozPromiseHolder<DecodedStream::EndedPromise>&& aVideoEndedPromise,
    float aPlaybackRate, float aVolume, bool aPreservesPitch,
    nsISerialEventTarget* aDecoderThread)
    : mAudioFramesWritten(0),
      mVideoTrackWritten(0),
      mNextAudioTime(aInit.mStartTime),
      mHaveSentFinishAudio(false),
      mHaveSentFinishVideo(false),
      mAudioTrack(aInit.mInfo.HasAudio()
                      ? AudioDecoderInputTrack::Create(
                            aGraph, aDecoderThread, aInit.mInfo.mAudio,
                            aPlaybackRate, aVolume, aPreservesPitch)
                      : nullptr),
      mVideoTrack(aInit.mInfo.HasVideo()
                      ? aGraph->CreateSourceTrack(MediaSegment::VIDEO)
                      : nullptr),
      mAudioOutputTrack(std::move(aAudioOutputTrack)),
      mVideoOutputTrack(std::move(aVideoOutputTrack)),
      mAudioPort((mAudioOutputTrack && mAudioTrack)
                     ? mAudioOutputTrack->AllocateInputPort(mAudioTrack)
                     : nullptr),
      mVideoPort((mVideoOutputTrack && mVideoTrack)
                     ? mVideoOutputTrack->AllocateInputPort(mVideoTrack)
                     : nullptr),
      mAudioEndedPromise(aAudioEndedPromise.Ensure(__func__)),
      mVideoEndedPromise(aVideoEndedPromise.Ensure(__func__)),
      // DecodedStreamGraphListener will resolve these promises.
      mListener(MakeRefPtr<DecodedStreamGraphListener>(
          aDecoderThread, mAudioTrack, std::move(aAudioEndedPromise),
          mVideoTrack, std::move(aVideoEndedPromise))) {
  MOZ_ASSERT(NS_IsMainThread());
}

DecodedStreamData::~DecodedStreamData() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mAudioTrack) {
    mAudioTrack->Destroy();
  }
  if (mVideoTrack) {
    mVideoTrack->Destroy();
  }
  if (mAudioPort) {
    mAudioPort->Destroy();
  }
  if (mVideoPort) {
    mVideoPort->Destroy();
  }
}

MediaEventSource<int64_t>& DecodedStreamData::OnOutput() {
  return mListener->OnOutput();
}

void DecodedStreamData::Close() { mListener->Close(); }

void DecodedStreamData::Forget() { mListener->Forget(); }

void DecodedStreamData::GetDebugInfo(dom::DecodedStreamDataDebugInfo& aInfo) {
  CopyUTF8toUTF16(nsPrintfCString("%p", this), aInfo.mInstance);
  aInfo.mAudioFramesWritten = mAudioFramesWritten;
  aInfo.mStreamAudioWritten = mListener->GetAudioFramesPlayed();
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

DecodedStream::DecodedStream(
    MediaDecoderStateMachine* aStateMachine,
    nsMainThreadPtrHandle<SharedDummyTrack> aDummyTrack,
    CopyableTArray<RefPtr<ProcessedMediaTrack>> aOutputTracks, double aVolume,
    double aPlaybackRate, bool aPreservesPitch,
    MediaQueue<AudioData>& aAudioQueue, MediaQueue<VideoData>& aVideoQueue)
    : mOwnerThread(aStateMachine->OwnerThread()),
      mDummyTrack(std::move(aDummyTrack)),
      mWatchManager(this, mOwnerThread),
      mPlaying(false, "DecodedStream::mPlaying"),
      mPrincipalHandle(aStateMachine->OwnerThread(), PRINCIPAL_HANDLE_NONE,
                       "DecodedStream::mPrincipalHandle (Mirror)"),
      mCanonicalOutputPrincipal(aStateMachine->CanonicalOutputPrincipal()),
      mOutputTracks(std::move(aOutputTracks)),
      mVolume(aVolume),
      mPlaybackRate(aPlaybackRate),
      mPreservesPitch(aPreservesPitch),
      mAudioQueue(aAudioQueue),
      mVideoQueue(aVideoQueue) {}

DecodedStream::~DecodedStream() {
  MOZ_ASSERT(mStartTime.isNothing(), "playback should've ended.");
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

  AUTO_PROFILER_LABEL(FUNCTION_SIGNATURE, MEDIA_PLAYBACK);
  if (profiler_can_accept_markers()) {
    nsPrintfCString markerString("StartTime=%" PRId64,
                                 aStartTime.ToMicroseconds());
    PLAYBACK_PROFILER_MARKER(markerString);
  }
  LOG_DS(LogLevel::Debug, "Start() mStartTime=%" PRId64,
         aStartTime.ToMicroseconds());

  mStartTime.emplace(aStartTime);
  mLastOutputTime = TimeUnit::Zero();
  mInfo = aInfo;
  mPlaying = true;
  mPrincipalHandle.Connect(mCanonicalOutputPrincipal);
  mWatchManager.Watch(mPlaying, &DecodedStream::PlayingChanged);
  mAudibilityMonitor.emplace(
      mInfo.mAudio.mRate,
      StaticPrefs::dom_media_silence_duration_for_audibility());
  ConnectListener();

  class R : public Runnable {
   public:
    R(PlaybackInfoInit&& aInit,
      nsMainThreadPtrHandle<SharedDummyTrack> aDummyTrack,
      nsTArray<RefPtr<ProcessedMediaTrack>> aOutputTracks,
      MozPromiseHolder<MediaSink::EndedPromise>&& aAudioEndedPromise,
      MozPromiseHolder<MediaSink::EndedPromise>&& aVideoEndedPromise,
      float aPlaybackRate, float aVolume, bool aPreservesPitch,
      nsISerialEventTarget* aDecoderThread)
        : Runnable("CreateDecodedStreamData"),
          mInit(std::move(aInit)),
          mDummyTrack(std::move(aDummyTrack)),
          mOutputTracks(std::move(aOutputTracks)),
          mAudioEndedPromise(std::move(aAudioEndedPromise)),
          mVideoEndedPromise(std::move(aVideoEndedPromise)),
          mPlaybackRate(aPlaybackRate),
          mVolume(aVolume),
          mPreservesPitch(aPreservesPitch),
          mDecoderThread(aDecoderThread) {}
    NS_IMETHOD Run() override {
      MOZ_ASSERT(NS_IsMainThread());
      RefPtr<ProcessedMediaTrack> audioOutputTrack;
      RefPtr<ProcessedMediaTrack> videoOutputTrack;
      for (const auto& track : mOutputTracks) {
        if (track->mType == MediaSegment::AUDIO) {
          MOZ_DIAGNOSTIC_ASSERT(
              !audioOutputTrack,
              "We only support capturing to one output track per kind");
          audioOutputTrack = track;
        } else if (track->mType == MediaSegment::VIDEO) {
          MOZ_DIAGNOSTIC_ASSERT(
              !videoOutputTrack,
              "We only support capturing to one output track per kind");
          videoOutputTrack = track;
        } else {
          MOZ_CRASH("Unknown media type");
        }
      }
      if (!mDummyTrack) {
        // No dummy track - no graph. This could be intentional as the owning
        // media element needs access to the tracks on main thread to set up
        // forwarding of them before playback starts. MDSM will re-create
        // DecodedStream once a dummy track is available. This effectively halts
        // playback for this DecodedStream.
        return NS_OK;
      }
      if ((audioOutputTrack && audioOutputTrack->IsDestroyed()) ||
          (videoOutputTrack && videoOutputTrack->IsDestroyed())) {
        // A track has been destroyed and we'll soon get re-created with a
        // proper one. This effectively halts playback for this DecodedStream.
        return NS_OK;
      }
      mData = MakeUnique<DecodedStreamData>(
          std::move(mInit), mDummyTrack->mTrack->Graph(),
          std::move(audioOutputTrack), std::move(videoOutputTrack),
          std::move(mAudioEndedPromise), std::move(mVideoEndedPromise),
          mPlaybackRate, mVolume, mPreservesPitch, mDecoderThread);
      return NS_OK;
    }
    UniquePtr<DecodedStreamData> ReleaseData() { return std::move(mData); }

   private:
    PlaybackInfoInit mInit;
    nsMainThreadPtrHandle<SharedDummyTrack> mDummyTrack;
    const nsTArray<RefPtr<ProcessedMediaTrack>> mOutputTracks;
    MozPromiseHolder<MediaSink::EndedPromise> mAudioEndedPromise;
    MozPromiseHolder<MediaSink::EndedPromise> mVideoEndedPromise;
    UniquePtr<DecodedStreamData> mData;
    const float mPlaybackRate;
    const float mVolume;
    const bool mPreservesPitch;
    const RefPtr<nsISerialEventTarget> mDecoderThread;
  };

  MozPromiseHolder<DecodedStream::EndedPromise> audioEndedHolder;
  MozPromiseHolder<DecodedStream::EndedPromise> videoEndedHolder;
  PlaybackInfoInit init{aStartTime, aInfo};
  nsCOMPtr<nsIRunnable> r =
      new R(std::move(init), mDummyTrack, mOutputTracks.Clone(),
            std::move(audioEndedHolder), std::move(videoEndedHolder),
            static_cast<float>(mPlaybackRate), static_cast<float>(mVolume),
            mPreservesPitch, mOwnerThread);
  SyncRunnable::DispatchToThread(GetMainThreadSerialEventTarget(), r);
  mData = static_cast<R*>(r.get())->ReleaseData();

  if (mData) {
    mAudioEndedPromise = mData->mAudioEndedPromise;
    mVideoEndedPromise = mData->mVideoEndedPromise;
    mOutputListener = mData->OnOutput().Connect(mOwnerThread, this,
                                                &DecodedStream::NotifyOutput);
    SendData();
  }
  return NS_OK;
}

void DecodedStream::Stop() {
  AssertOwnerThread();
  MOZ_ASSERT(mStartTime.isSome(), "playback not started.");

  TRACE();
  LOG_DS(LogLevel::Debug, "Stop()");

  DisconnectListener();
  ResetVideo(mPrincipalHandle);
  ResetAudio();
  mStartTime.reset();
  mAudioEndedPromise = nullptr;
  mVideoEndedPromise = nullptr;

  // Clear mData immediately when this playback session ends so we won't
  // send data to the wrong track in SendData() in next playback session.
  DestroyData(std::move(mData));

  mPrincipalHandle.DisconnectIfConnected();
  mWatchManager.Unwatch(mPlaying, &DecodedStream::PlayingChanged);
  mAudibilityMonitor.reset();
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

  TRACE();
  mOutputListener.Disconnect();

  aData->Close();
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

  if (profiler_can_accept_markers()) {
    nsPrintfCString markerString("Playing=%s", aPlaying ? "true" : "false");
    PLAYBACK_PROFILER_MARKER(markerString);
  }
  LOG_DS(LogLevel::Debug, "playing (%d) -> (%d)", mPlaying.Ref(), aPlaying);
  mPlaying = aPlaying;
}

void DecodedStream::SetVolume(double aVolume) {
  AssertOwnerThread();
  if (profiler_can_accept_markers()) {
    nsPrintfCString markerString("Volume=%f", aVolume);
    PLAYBACK_PROFILER_MARKER(markerString);
  }
  if (mVolume == aVolume) {
    return;
  }
  mVolume = aVolume;
  if (mData && mData->mAudioTrack) {
    mData->mAudioTrack->SetVolume(static_cast<float>(aVolume));
  }
}

void DecodedStream::SetPlaybackRate(double aPlaybackRate) {
  AssertOwnerThread();
  if (profiler_can_accept_markers()) {
    nsPrintfCString markerString("PlaybackRate=%f", aPlaybackRate);
    PLAYBACK_PROFILER_MARKER(markerString);
  }
  if (mPlaybackRate == aPlaybackRate) {
    return;
  }
  mPlaybackRate = aPlaybackRate;
  if (mData && mData->mAudioTrack) {
    mData->mAudioTrack->SetPlaybackRate(static_cast<float>(aPlaybackRate));
  }
}

void DecodedStream::SetPreservesPitch(bool aPreservesPitch) {
  AssertOwnerThread();
  if (profiler_can_accept_markers()) {
    nsPrintfCString markerString("PreservesPitch=%s",
                                 aPreservesPitch ? "true" : "false");
    PLAYBACK_PROFILER_MARKER(markerString);
  }
  if (mPreservesPitch == aPreservesPitch) {
    return;
  }
  mPreservesPitch = aPreservesPitch;
  if (mData && mData->mAudioTrack) {
    mData->mAudioTrack->SetPreservesPitch(aPreservesPitch);
  }
}

double DecodedStream::PlaybackRate() const {
  AssertOwnerThread();
  return mPlaybackRate;
}

void DecodedStream::SendAudio(const PrincipalHandle& aPrincipalHandle) {
  AssertOwnerThread();

  if (!mInfo.HasAudio()) {
    return;
  }

  if (mData->mHaveSentFinishAudio) {
    return;
  }

  TRACE();
  // It's OK to hold references to the AudioData because AudioData
  // is ref-counted.
  AutoTArray<RefPtr<AudioData>, 10> audio;
  mAudioQueue.GetElementsAfter(mData->mNextAudioTime, &audio);

  // Append data which haven't been sent to audio track before.
  for (uint32_t i = 0; i < audio.Length(); ++i) {
    CheckIsDataAudible(audio[i]);
    mData->mAudioTrack->AppendData(audio[i], aPrincipalHandle);
    mData->mNextAudioTime = audio[i]->GetEndTime();
    mData->mAudioFramesWritten += audio[i]->Frames();
  }

  if (mAudioQueue.IsFinished() && !mData->mHaveSentFinishAudio) {
    mData->mAudioTrack->NotifyEndOfStream();
    mData->mHaveSentFinishAudio = true;
  }
}

void DecodedStream::CheckIsDataAudible(const AudioData* aData) {
  MOZ_ASSERT(aData);

  mAudibilityMonitor->Process(aData);
  bool isAudible = mAudibilityMonitor->RecentlyAudible();

  if (isAudible != mIsAudioDataAudible) {
    mIsAudioDataAudible = isAudible;
    mAudibleEvent.Notify(mIsAudioDataAudible);
  }
}

void DecodedStreamData::WriteVideoToSegment(
    layers::Image* aImage, const TimeUnit& aStart, const TimeUnit& aEnd,
    const gfx::IntSize& aIntrinsicSize, const TimeStamp& aTimeStamp,
    VideoSegment* aOutput, const PrincipalHandle& aPrincipalHandle) {
  RefPtr<layers::Image> image = aImage;
  auto end =
      mVideoTrack->MicrosecondsToTrackTimeRoundDown(aEnd.ToMicroseconds());
  auto start =
      mVideoTrack->MicrosecondsToTrackTimeRoundDown(aStart.ToMicroseconds());
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
  TrackTime lastVideoStratTime;
  aInput.GetLastFrame(&lastVideoStratTime);
  return lastVideoStratTime == aInput.GetDuration();
}

void DecodedStream::ResetAudio() {
  AssertOwnerThread();

  if (!mData) {
    return;
  }

  if (!mInfo.HasAudio()) {
    return;
  }

  TRACE();
  mData->mAudioTrack->ClearFutureData();
  if (const RefPtr<AudioData>& v = mAudioQueue.PeekFront()) {
    mData->mNextAudioTime = v->mTime;
    mData->mHaveSentFinishAudio = false;
  }
}

void DecodedStream::ResetVideo(const PrincipalHandle& aPrincipalHandle) {
  AssertOwnerThread();

  if (!mData) {
    return;
  }

  if (!mInfo.HasVideo()) {
    return;
  }

  TRACE();
  TrackTime cleared = mData->mVideoTrack->ClearFutureData();
  mData->mVideoTrackWritten -= cleared;
  if (mData->mHaveSentFinishVideo && cleared > 0) {
    mData->mHaveSentFinishVideo = false;
    mData->mListener->EndVideoTrackAt(mData->mVideoTrack, TRACK_TIME_MAX);
  }

  VideoSegment resetter;
  TimeStamp currentTime;
  TimeUnit currentPosition = GetPosition(&currentTime);

  // Giving direct consumers a frame (really *any* frame, so in this case:
  // nullptr) at an earlier time than the previous, will signal to that consumer
  // to discard any frames ahead in time of the new frame. To be honest, this is
  // an ugly hack because the direct listeners of the MediaTrackGraph do not
  // have an API that supports clearing the future frames. ImageContainer and
  // VideoFrameContainer do though, and we will need to move to a similar API
  // for video tracks as part of bug 1493618.
  resetter.AppendFrame(nullptr, mData->mLastVideoImageDisplaySize,
                       aPrincipalHandle, false, currentTime);
  mData->mVideoTrack->AppendData(&resetter);

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

  TRACE();
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
      // the track's lifetime in the MTG, as rendering is based on timestamps,
      // aka frame start times.
      TimeStamp t =
          std::max(mData->mLastVideoTimeStamp,
                   currentTime + (lastEnd - currentPosition).ToTimeDuration());
      TimeUnit end = std::max(
          v->GetEndTime(),
          lastEnd + TimeUnit::FromMicroseconds(
                        mData->mVideoTrack->TrackTimeToMicroseconds(1) + 1));
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
    mData->mVideoTrackWritten += mData->mVideoTrack->AppendData(&output);
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
      // will round the conversion from microseconds to TrackTime down.
      auto deviation = TimeUnit::FromMicroseconds(
          mData->mVideoTrack->TrackTimeToMicroseconds(1) + 1);
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
      mData->mVideoTrackWritten += mData->mVideoTrack->AppendData(&endSegment);
    }
    mData->mListener->EndVideoTrackAt(mData->mVideoTrack,
                                      mData->mVideoTrackWritten);
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

  LOG_DS(LogLevel::Verbose, "SendData()");
  SendAudio(mPrincipalHandle);
  SendVideo(mPrincipalHandle);
}

TimeUnit DecodedStream::GetEndTime(TrackType aType) const {
  AssertOwnerThread();
  TRACE();
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
  TRACE();
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

  if (profiler_can_accept_markers()) {
    nsPrintfCString markerString("OutputTime=%" PRId64,
                                 currentTime.ToMicroseconds());
    PLAYBACK_PROFILER_MARKER(markerString);
  }
  LOG_DS(LogLevel::Verbose, "time is now %" PRId64,
         currentTime.ToMicroseconds());

  // Remove audio samples that have been played by MTG from the queue.
  RefPtr<AudioData> a = mAudioQueue.PeekFront();
  for (; a && a->GetEndTime() <= currentTime;) {
    LOG_DS(LogLevel::Debug, "Dropping audio [%" PRId64 ",%" PRId64 "]",
           a->mTime.ToMicroseconds(), a->GetEndTime().ToMicroseconds());
    RefPtr<AudioData> releaseMe = mAudioQueue.PopFront();
    a = mAudioQueue.PeekFront();
  }
}

void DecodedStream::PlayingChanged() {
  AssertOwnerThread();
  TRACE();

  if (!mPlaying) {
    // On seek or pause we discard future frames.
    ResetVideo(mPrincipalHandle);
    ResetAudio();
  }
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

#undef LOG_DS

}  // namespace mozilla
