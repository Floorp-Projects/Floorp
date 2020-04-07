/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecodedStream.h"
#include "AudioSegment.h"
#include "MediaData.h"
#include "MediaDecoderStateMachine.h"
#include "MediaQueue.h"
#include "MediaTrackGraph.h"
#include "MediaTrackListener.h"
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

extern LazyLogModule gMediaDecoderLog;

#define LOG_DS(type, fmt, ...)    \
  MOZ_LOG(gMediaDecoderLog, type, \
          ("DecodedStream=%p " fmt, this, ##__VA_ARGS__))

/*
 * A container class to make it easier to pass the playback info all the
 * way to DecodedStreamGraphListener from DecodedStream.
 */
struct PlaybackInfoInit {
  TimeUnit mStartTime;
  MediaInfo mInfo;
};

class DecodedStreamGraphListener;

class DecodedStreamTrackListener : public MediaTrackListener {
 public:
  DecodedStreamTrackListener(DecodedStreamGraphListener* aGraphListener,
                             SourceMediaTrack* aTrack);

  void NotifyOutput(MediaTrackGraph* aGraph,
                    TrackTime aCurrentTrackTime) override;
  void NotifyEnded(MediaTrackGraph* aGraph) override;

 private:
  const RefPtr<DecodedStreamGraphListener> mGraphListener;
  const RefPtr<SourceMediaTrack> mTrack;
};

class DecodedStreamGraphListener {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DecodedStreamGraphListener)
 public:
  DecodedStreamGraphListener(
      SourceMediaTrack* aAudioTrack,
      MozPromiseHolder<DecodedStream::EndedPromise>&& aAudioEndedHolder,
      SourceMediaTrack* aVideoTrack,
      MozPromiseHolder<DecodedStream::EndedPromise>&& aVideoEndedHolder)
      : mAudioTrackListener(
            aAudioTrack
                ? MakeRefPtr<DecodedStreamTrackListener>(this, aAudioTrack)
                : nullptr),
        mAudioEndedHolder(std::move(aAudioEndedHolder)),
        mVideoTrackListener(
            aVideoTrack
                ? MakeRefPtr<DecodedStreamTrackListener>(this, aVideoTrack)
                : nullptr),
        mVideoEndedHolder(std::move(aVideoEndedHolder)),
        mAudioTrack(aAudioTrack),
        mVideoTrack(aVideoTrack) {
    MOZ_ASSERT(NS_IsMainThread());
    if (mAudioTrackListener) {
      mAudioTrack->AddListener(mAudioTrackListener);
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

  void NotifyOutput(SourceMediaTrack* aTrack, TrackTime aCurrentTrackTime) {
    if (aTrack == mAudioTrack) {
      if (aCurrentTrackTime >= mAudioEnd) {
        mAudioTrack->End();
      }
    } else if (aTrack == mVideoTrack) {
      if (aCurrentTrackTime >= mVideoEnd) {
        mVideoTrack->End();
      }
    } else {
      MOZ_CRASH("Unexpected source track");
    }
    if (aTrack != mAudioTrack && mAudioTrack && !mAudioEnded) {
      // Only audio playout drives the clock forward, if present and live.
      return;
    }
    MOZ_ASSERT_IF(aTrack == mAudioTrack, !mAudioEnded);
    MOZ_ASSERT_IF(aTrack == mVideoTrack, !mVideoEnded);
    mOnOutput.Notify(aTrack->TrackTimeToMicroseconds(aCurrentTrackTime));
  }

  void NotifyEnded(SourceMediaTrack* aTrack) {
    if (aTrack == mAudioTrack) {
      mAudioEnded = true;
    } else if (aTrack == mVideoTrack) {
      mVideoEnded = true;
    } else {
      MOZ_CRASH("Unexpected source track");
    }
    aTrack->Graph()->DispatchToMainThreadStableState(
        NewRunnableMethod<RefPtr<SourceMediaTrack>>(
            "DecodedStreamGraphListener::DoNotifyTrackEnded", this,
            &DecodedStreamGraphListener::DoNotifyTrackEnded, aTrack));
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
   *
   * Callable from any thread.
   */
  void EndTrackAt(SourceMediaTrack* aTrack, TrackTime aEnd) {
    if (aTrack == mAudioTrack) {
      mAudioEnd = aEnd;
    } else if (aTrack == mVideoTrack) {
      mVideoEnd = aEnd;
    } else {
      MOZ_CRASH("Unexpected source track");
    }
  }

  void DoNotifyTrackEnded(SourceMediaTrack* aTrack) {
    MOZ_ASSERT(NS_IsMainThread());
    if (aTrack == mAudioTrack) {
      mAudioEndedHolder.ResolveIfExists(true, __func__);
    } else if (aTrack == mVideoTrack) {
      mVideoEndedHolder.ResolveIfExists(true, __func__);
    } else {
      MOZ_CRASH("Unexpected source track");
    }
  }

  void Forget() {
    MOZ_ASSERT(NS_IsMainThread());

    if (mAudioTrackListener && !mAudioTrack->IsDestroyed()) {
      mAudioTrack->End();
      mAudioTrack->RemoveListener(mAudioTrackListener);
    }
    mAudioTrackListener = nullptr;
    mAudioEndedHolder.ResolveIfExists(false, __func__);

    if (mVideoTrackListener && !mVideoTrack->IsDestroyed()) {
      mVideoTrack->End();
      mVideoTrack->RemoveListener(mVideoTrackListener);
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
  const RefPtr<SourceMediaTrack> mAudioTrack;
  const RefPtr<SourceMediaTrack> mVideoTrack;
  Atomic<TrackTime> mAudioEnd{TRACK_TIME_MAX};
  Atomic<TrackTime> mVideoEnd{TRACK_TIME_MAX};
};

DecodedStreamTrackListener::DecodedStreamTrackListener(
    DecodedStreamGraphListener* aGraphListener, SourceMediaTrack* aTrack)
    : mGraphListener(aGraphListener), mTrack(aTrack) {}

void DecodedStreamTrackListener::NotifyOutput(MediaTrackGraph* aGraph,
                                              TrackTime aCurrentTrackTime) {
  mGraphListener->NotifyOutput(mTrack, aCurrentTrackTime);
}

void DecodedStreamTrackListener::NotifyEnded(MediaTrackGraph* aGraph) {
  mGraphListener->NotifyEnded(mTrack);
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
      MozPromiseHolder<DecodedStream::EndedPromise>&& aVideoEndedPromise);
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
  // Count of audio frames written to the track
  int64_t mAudioFramesWritten;
  // Count of video frames written to the track in the track's rate
  TrackTime mVideoTrackWritten;
  // Count of audio frames written to the track in the track's rate
  TrackTime mAudioTrackWritten;
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

  const RefPtr<SourceMediaTrack> mAudioTrack;
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
    MozPromiseHolder<DecodedStream::EndedPromise>&& aVideoEndedPromise)
    : mAudioFramesWritten(0),
      mVideoTrackWritten(0),
      mAudioTrackWritten(0),
      mNextAudioTime(aInit.mStartTime),
      mHaveSentFinishAudio(false),
      mHaveSentFinishVideo(false),
      mAudioTrack(aInit.mInfo.HasAudio()
                      ? aGraph->CreateSourceTrack(MediaSegment::AUDIO)
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
          mAudioTrack, std::move(aAudioEndedPromise), mVideoTrack,
          std::move(aVideoEndedPromise))) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mAudioTrack) {
    mAudioTrack->SetAppendDataSourceRate(aInit.mInfo.mAudio.mRate);
  }
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

void DecodedStreamData::Forget() { mListener->Forget(); }

void DecodedStreamData::GetDebugInfo(dom::DecodedStreamDataDebugInfo& aInfo) {
  aInfo.mInstance = NS_ConvertUTF8toUTF16(nsPrintfCString("%p", this));
  aInfo.mAudioFramesWritten = mAudioFramesWritten;
  aInfo.mStreamAudioWritten = mAudioTrackWritten;
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
    nsTArray<RefPtr<ProcessedMediaTrack>> aOutputTracks, double aVolume,
    double aPlaybackRate, bool aPreservesPitch,
    MediaQueue<AudioData>& aAudioQueue, MediaQueue<VideoData>& aVideoQueue)
    : mOwnerThread(aStateMachine->OwnerThread()),
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
  MOZ_DIAGNOSTIC_ASSERT(!mOutputTracks.IsEmpty());

  LOG_DS(LogLevel::Debug, "Start() mStartTime=%" PRId64,
         aStartTime.ToMicroseconds());

  mStartTime.emplace(aStartTime);
  mLastOutputTime = TimeUnit::Zero();
  mInfo = aInfo;
  mPlaying = true;
  mPrincipalHandle.Connect(mCanonicalOutputPrincipal);
  mWatchManager.Watch(mPlaying, &DecodedStream::PlayingChanged);
  ConnectListener();

  class R : public Runnable {
   public:
    R(PlaybackInfoInit&& aInit,
      nsTArray<RefPtr<ProcessedMediaTrack>> aOutputTracks,
      MozPromiseHolder<MediaSink::EndedPromise>&& aAudioEndedPromise,
      MozPromiseHolder<MediaSink::EndedPromise>&& aVideoEndedPromise)
        : Runnable("CreateDecodedStreamData"),
          mInit(std::move(aInit)),
          mOutputTracks(std::move(aOutputTracks)),
          mAudioEndedPromise(std::move(aAudioEndedPromise)),
          mVideoEndedPromise(std::move(aVideoEndedPromise)) {}
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
      if ((!audioOutputTrack && !videoOutputTrack) ||
          (audioOutputTrack && audioOutputTrack->IsDestroyed()) ||
          (videoOutputTrack && videoOutputTrack->IsDestroyed())) {
        // No output tracks yet, or they're going away. Halt playback by not
        // creating DecodedStreamData. MDSM will try again with a new
        // DecodedStream sink when tracks are available.
        return NS_OK;
      }
      mData = MakeUnique<DecodedStreamData>(
          std::move(mInit), mOutputTracks[0]->Graph(),
          std::move(audioOutputTrack), std::move(videoOutputTrack),
          std::move(mAudioEndedPromise), std::move(mVideoEndedPromise));
      return NS_OK;
    }
    UniquePtr<DecodedStreamData> ReleaseData() { return std::move(mData); }

   private:
    PlaybackInfoInit mInit;
    const nsTArray<RefPtr<ProcessedMediaTrack>> mOutputTracks;
    MozPromiseHolder<MediaSink::EndedPromise> mAudioEndedPromise;
    MozPromiseHolder<MediaSink::EndedPromise> mVideoEndedPromise;
    UniquePtr<DecodedStreamData> mData;
  };

  MozPromiseHolder<DecodedStream::EndedPromise> audioEndedHolder;
  MozPromiseHolder<DecodedStream::EndedPromise> videoEndedHolder;
  PlaybackInfoInit init{aStartTime, aInfo};
  nsCOMPtr<nsIRunnable> r = new R(
      std::move(init), nsTArray<RefPtr<ProcessedMediaTrack>>(mOutputTracks),
      std::move(audioEndedHolder), std::move(videoEndedHolder));
  SyncRunnable::DispatchToThread(GetMainThreadSerialEventTarget(), r);
  mData = static_cast<R*>(r.get())->ReleaseData();

  if (mData) {
    mAudioEndedPromise = mData->mAudioEndedPromise;
    mVideoEndedPromise = mData->mVideoEndedPromise;
    mOutputListener = mData->OnOutput().Connect(mOwnerThread, this,
                                                &DecodedStream::NotifyOutput);
    if (mData->mAudioTrack) {
      mData->mAudioTrack->SetVolume(static_cast<float>(mVolume));
    }
    SendData();
  }
  return NS_OK;
}

void DecodedStream::Stop() {
  AssertOwnerThread();
  MOZ_ASSERT(mStartTime.isSome(), "playback not started.");

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

  LOG_DS(LogLevel::Debug, "playing (%d) -> (%d)", mPlaying.Ref(), aPlaying);
  mPlaying = aPlaying;
}

void DecodedStream::SetVolume(double aVolume) {
  AssertOwnerThread();
  mVolume = aVolume;
  if (mData && mData->mAudioTrack) {
    mData->mAudioTrack->SetVolume(static_cast<float>(aVolume));
  }
}

void DecodedStream::SetPlaybackRate(double aPlaybackRate) {
  AssertOwnerThread();
  mPlaybackRate = aPlaybackRate;
}

void DecodedStream::SetPreservesPitch(bool aPreservesPitch) {
  AssertOwnerThread();
  mPreservesPitch = aPreservesPitch;
}

double DecodedStream::PlaybackRate() const {
  AssertOwnerThread();
  return mPlaybackRate;
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
    LOG_DS(LogLevel::Verbose, "Queueing audio [%" PRId64 ",%" PRId64 "]",
           audio[i]->mTime.ToMicroseconds(),
           audio[i]->GetEndTime().ToMicroseconds());
    SendStreamAudio(mData.get(), mStartTime.ref(), audio[i], &output, rate,
                    aPrincipalHandle);
  }

  // |mNextAudioTime| is updated as we process each audio sample in
  // SendStreamAudio().
  if (output.GetDuration() > 0) {
    mData->mAudioTrackWritten += mData->mAudioTrack->AppendData(&output);
  }

  if (mAudioQueue.IsFinished() && !mData->mHaveSentFinishAudio) {
    mData->mListener->EndTrackAt(mData->mAudioTrack, mData->mAudioTrackWritten);
    mData->mHaveSentFinishAudio = true;
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

  TrackTime cleared = mData->mAudioTrack->ClearFutureData();
  mData->mAudioTrackWritten -= cleared;
  if (const RefPtr<AudioData>& v = mAudioQueue.PeekFront()) {
    mData->mNextAudioTime = v->mTime;
  }
  if (mData->mHaveSentFinishAudio && cleared > 0) {
    mData->mHaveSentFinishAudio = false;
    mData->mListener->EndTrackAt(mData->mAudioTrack, TRACK_TIME_MAX);
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

  TrackTime cleared = mData->mVideoTrack->ClearFutureData();
  mData->mVideoTrackWritten -= cleared;
  if (mData->mHaveSentFinishVideo && cleared > 0) {
    mData->mHaveSentFinishVideo = false;
    mData->mListener->EndTrackAt(mData->mVideoTrack, TRACK_TIME_MAX);
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
    mData->mListener->EndTrackAt(mData->mVideoTrack, mData->mVideoTrackWritten);
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
  SendAudio(mVolume, mPrincipalHandle);
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
