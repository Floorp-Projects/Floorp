/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEncoder.h"

#include <algorithm>
#include "AudioNodeEngine.h"
#include "AudioNodeTrack.h"
#include "DriftCompensation.h"
#include "GeckoProfiler.h"
#include "MediaDecoder.h"
#include "MediaTrackGraphImpl.h"
#include "MediaTrackListener.h"
#include "mozilla/dom/AudioNode.h"
#include "mozilla/dom/AudioStreamTrack.h"
#include "mozilla/dom/MediaStreamTrack.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "mozilla/gfx/Point.h"  // IntSize
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/Unused.h"
#include "Muxer.h"
#include "nsMimeTypes.h"
#include "nsThreadUtils.h"
#include "OggWriter.h"
#include "OpusTrackEncoder.h"
#include "TimeUnits.h"
#include "Tracing.h"

#ifdef MOZ_WEBM_ENCODER
#  include "VP8TrackEncoder.h"
#  include "WebMWriter.h"
#endif

mozilla::LazyLogModule gMediaEncoderLog("MediaEncoder");
#define LOG(type, msg) MOZ_LOG(gMediaEncoderLog, type, msg)

namespace mozilla {

using namespace dom;
using namespace media;

class MediaEncoder::AudioTrackListener : public DirectMediaTrackListener {
 public:
  AudioTrackListener(RefPtr<DriftCompensator> aDriftCompensator,
                     RefPtr<MediaEncoder> aMediaEncoder)
      : mDirectConnected(false),
        mInitialized(false),
        mRemoved(false),
        mDriftCompensator(std::move(aDriftCompensator)),
        mMediaEncoder(std::move(aMediaEncoder)),
        mEncoderThread(mMediaEncoder->mEncoderThread),
        mShutdownPromise(mShutdownHolder.Ensure(__func__)) {
    MOZ_ASSERT(mMediaEncoder);
    MOZ_ASSERT(mMediaEncoder->mAudioEncoder);
    MOZ_ASSERT(mEncoderThread);
  }

  void NotifyDirectListenerInstalled(InstallationResult aResult) override {
    if (aResult == InstallationResult::SUCCESS) {
      LOG(LogLevel::Info, ("Audio track direct listener installed"));
      mDirectConnected = true;
    } else {
      LOG(LogLevel::Info, ("Audio track failed to install direct listener"));
      MOZ_ASSERT(!mDirectConnected);
    }
  }

  void NotifyDirectListenerUninstalled() override {
    mDirectConnected = false;

    if (mRemoved) {
      mMediaEncoder = nullptr;
      mEncoderThread = nullptr;
    }
  }

  void NotifyQueuedChanges(MediaTrackGraph* aGraph, TrackTime aTrackOffset,
                           const MediaSegment& aQueuedMedia) override {
    TRACE_COMMENT("MediaEncoder %p", mMediaEncoder.get());
    MOZ_ASSERT(mMediaEncoder);
    MOZ_ASSERT(mEncoderThread);

    if (!mInitialized) {
      mDriftCompensator->NotifyAudioStart(TimeStamp::Now());
      mInitialized = true;
    }

    mDriftCompensator->NotifyAudio(aQueuedMedia.GetDuration());

    const AudioSegment& audio = static_cast<const AudioSegment&>(aQueuedMedia);

    AudioSegment copy;
    copy.AppendSlice(audio, 0, audio.GetDuration());

    nsresult rv = mEncoderThread->Dispatch(NS_NewRunnableFunction(
        "mozilla::AudioTrackEncoder::AppendAudioSegment",
        [encoder = mMediaEncoder, copy = std::move(copy)]() mutable {
          encoder->mAudioEncoder->AppendAudioSegment(std::move(copy));
        }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  void NotifyEnded(MediaTrackGraph* aGraph) override {
    MOZ_ASSERT(mMediaEncoder);
    MOZ_ASSERT(mMediaEncoder->mAudioEncoder);
    MOZ_ASSERT(mEncoderThread);

    nsresult rv = mEncoderThread->Dispatch(
        NS_NewRunnableFunction("mozilla::AudioTrackEncoder::NotifyEndOfStream",
                               [encoder = mMediaEncoder] {
                                 encoder->mAudioEncoder->NotifyEndOfStream();
                               }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  void NotifyRemoved(MediaTrackGraph* aGraph) override {
    nsresult rv = mEncoderThread->Dispatch(
        NS_NewRunnableFunction("mozilla::AudioTrackEncoder::NotifyEndOfStream",
                               [encoder = mMediaEncoder] {
                                 encoder->mAudioEncoder->NotifyEndOfStream();
                               }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;

    mRemoved = true;

    if (!mDirectConnected) {
      mMediaEncoder = nullptr;
      mEncoderThread = nullptr;
    }

    mShutdownHolder.Resolve(true, __func__);
  }

  const RefPtr<GenericNonExclusivePromise>& OnShutdown() const {
    return mShutdownPromise;
  }

 private:
  bool mDirectConnected;
  bool mInitialized;
  bool mRemoved;
  const RefPtr<DriftCompensator> mDriftCompensator;
  RefPtr<MediaEncoder> mMediaEncoder;
  RefPtr<TaskQueue> mEncoderThread;
  MozPromiseHolder<GenericNonExclusivePromise> mShutdownHolder;
  const RefPtr<GenericNonExclusivePromise> mShutdownPromise;
};

class MediaEncoder::VideoTrackListener : public DirectMediaTrackListener {
 public:
  explicit VideoTrackListener(RefPtr<MediaEncoder> aMediaEncoder)
      : mDirectConnected(false),
        mInitialized(false),
        mRemoved(false),
        mPendingAdvanceCurrentTime(false),
        mMediaEncoder(std::move(aMediaEncoder)),
        mEncoderThread(mMediaEncoder->mEncoderThread),
        mShutdownPromise(mShutdownHolder.Ensure(__func__)) {
    MOZ_ASSERT(mMediaEncoder);
    MOZ_ASSERT(mEncoderThread);
  }

  void NotifyDirectListenerInstalled(InstallationResult aResult) override {
    if (aResult == InstallationResult::SUCCESS) {
      LOG(LogLevel::Info, ("Video track direct listener installed"));
      mDirectConnected = true;
    } else {
      LOG(LogLevel::Info, ("Video track failed to install direct listener"));
      MOZ_ASSERT(!mDirectConnected);
      return;
    }
  }

  void NotifyDirectListenerUninstalled() override {
    mDirectConnected = false;

    if (mRemoved) {
      mMediaEncoder = nullptr;
      mEncoderThread = nullptr;
    }
  }

  void NotifyQueuedChanges(MediaTrackGraph* aGraph, TrackTime aTrackOffset,
                           const MediaSegment& aQueuedMedia) override {
    TRACE_COMMENT("MediaEncoder %p", mMediaEncoder.get());
    MOZ_ASSERT(mMediaEncoder);
    MOZ_ASSERT(mMediaEncoder->mVideoEncoder);
    MOZ_ASSERT(mEncoderThread);

    mCurrentTime = TimeStamp::Now();
    if (!mInitialized) {
      nsresult rv = mEncoderThread->Dispatch(
          NS_NewRunnableFunction("mozilla::VideoTrackEncoder::SetStartOffset",
                                 [encoder = mMediaEncoder, now = mCurrentTime] {
                                   encoder->mVideoEncoder->SetStartOffset(now);
                                 }));
      MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
      Unused << rv;
      mInitialized = true;
    }

    if (!mPendingAdvanceCurrentTime) {
      mPendingAdvanceCurrentTime = true;
      nsresult rv = mEncoderThread->Dispatch(NS_NewRunnableFunction(
          "mozilla::VideoTrackEncoder::AdvanceCurrentTime",
          [encoder = mMediaEncoder, now = mCurrentTime] {
            encoder->mVideoListener->mPendingAdvanceCurrentTime = false;
            encoder->mVideoEncoder->AdvanceCurrentTime(now);
          }));
      MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
      Unused << rv;
    }
  }

  void NotifyRealtimeTrackData(MediaTrackGraph* aGraph, TrackTime aTrackOffset,
                               const MediaSegment& aMedia) override {
    TRACE_COMMENT("MediaEncoder %p", mMediaEncoder.get());
    MOZ_ASSERT(mMediaEncoder);
    MOZ_ASSERT(mMediaEncoder->mVideoEncoder);
    MOZ_ASSERT(mEncoderThread);
    MOZ_ASSERT(aMedia.GetType() == MediaSegment::VIDEO);

    const VideoSegment& video = static_cast<const VideoSegment&>(aMedia);
    VideoSegment copy;
    for (VideoSegment::ConstChunkIterator iter(video); !iter.IsEnded();
         iter.Next()) {
      copy.AppendFrame(do_AddRef(iter->mFrame.GetImage()),
                       iter->mFrame.GetIntrinsicSize(),
                       iter->mFrame.GetPrincipalHandle(),
                       iter->mFrame.GetForceBlack(), iter->mTimeStamp);
    }

    nsresult rv = mEncoderThread->Dispatch(NS_NewRunnableFunction(
        "mozilla::VideoTrackEncoder::AppendVideoSegment",
        [encoder = mMediaEncoder, copy = std::move(copy)]() mutable {
          encoder->mVideoEncoder->AppendVideoSegment(std::move(copy));
        }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  void NotifyEnabledStateChanged(MediaTrackGraph* aGraph,
                                 bool aEnabled) override {
    MOZ_ASSERT(mMediaEncoder);
    MOZ_ASSERT(mMediaEncoder->mVideoEncoder);
    MOZ_ASSERT(mEncoderThread);

    nsresult rv;
    if (aEnabled) {
      rv = mEncoderThread->Dispatch(NS_NewRunnableFunction(
          "mozilla::VideoTrackEncoder::Enable",
          [encoder = mMediaEncoder, now = TimeStamp::Now()] {
            encoder->mVideoEncoder->Enable(now);
          }));
    } else {
      rv = mEncoderThread->Dispatch(NS_NewRunnableFunction(
          "mozilla::VideoTrackEncoder::Disable",
          [encoder = mMediaEncoder, now = TimeStamp::Now()] {
            encoder->mVideoEncoder->Disable(now);
          }));
    }
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  void NotifyEnded(MediaTrackGraph* aGraph) override {
    MOZ_ASSERT(mMediaEncoder);
    MOZ_ASSERT(mMediaEncoder->mVideoEncoder);
    MOZ_ASSERT(mEncoderThread);

    nsresult rv = mEncoderThread->Dispatch(NS_NewRunnableFunction(
        "mozilla::VideoTrackEncoder::NotifyEndOfStream",
        [encoder = mMediaEncoder, now = mCurrentTime] {
          if (!now.IsNull()) {
            encoder->mVideoEncoder->AdvanceCurrentTime(now);
          }
          encoder->mVideoEncoder->NotifyEndOfStream();
        }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  void NotifyRemoved(MediaTrackGraph* aGraph) override {
    nsresult rv = mEncoderThread->Dispatch(NS_NewRunnableFunction(
        "mozilla::VideoTrackEncoder::NotifyEndOfStream",
        [encoder = mMediaEncoder, now = mCurrentTime] {
          if (!now.IsNull()) {
            encoder->mVideoEncoder->AdvanceCurrentTime(now);
          }
          encoder->mVideoEncoder->NotifyEndOfStream();
        }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;

    mRemoved = true;

    if (!mDirectConnected) {
      mMediaEncoder = nullptr;
      mEncoderThread = nullptr;
    }

    mShutdownHolder.Resolve(true, __func__);
  }

  const RefPtr<GenericNonExclusivePromise>& OnShutdown() const {
    return mShutdownPromise;
  }

 private:
  bool mDirectConnected;
  bool mInitialized;
  bool mRemoved;
  TimeStamp mCurrentTime;
  Atomic<bool> mPendingAdvanceCurrentTime;
  RefPtr<MediaEncoder> mMediaEncoder;
  RefPtr<TaskQueue> mEncoderThread;
  MozPromiseHolder<GenericNonExclusivePromise> mShutdownHolder;
  const RefPtr<GenericNonExclusivePromise> mShutdownPromise;
};

class MediaEncoder::EncoderListener : public TrackEncoderListener {
 public:
  EncoderListener(TaskQueue* aEncoderThread, MediaEncoder* aEncoder)
      : mEncoderThread(aEncoderThread), mEncoder(aEncoder) {}

  void Forget() {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
    mEncoder = nullptr;
  }

  void Initialized(TrackEncoder* aTrackEncoder) override {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
    MOZ_ASSERT(aTrackEncoder->IsInitialized());

    if (!mEncoder) {
      return;
    }

    nsresult rv = mEncoderThread->Dispatch(
        NewRunnableMethod("mozilla::MediaEncoder::NotifyInitialized", mEncoder,
                          &MediaEncoder::NotifyInitialized));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  void Started(TrackEncoder* aTrackEncoder) override {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
    MOZ_ASSERT(aTrackEncoder->IsStarted());

    if (!mEncoder) {
      return;
    }

    nsresult rv = mEncoderThread->Dispatch(
        NewRunnableMethod("mozilla::MediaEncoder::NotifyStarted", mEncoder,
                          &MediaEncoder::NotifyStarted));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  void Error(TrackEncoder* aTrackEncoder) override {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

    if (!mEncoder) {
      return;
    }

    nsresult rv = mEncoderThread->Dispatch(NewRunnableMethod(
        "mozilla::MediaEncoder::SetError", mEncoder, &MediaEncoder::SetError));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

 protected:
  RefPtr<TaskQueue> mEncoderThread;
  RefPtr<MediaEncoder> mEncoder;
};

MediaEncoder::MediaEncoder(
    TaskQueue* aEncoderThread, RefPtr<DriftCompensator> aDriftCompensator,
    UniquePtr<ContainerWriter> aWriter,
    UniquePtr<AudioTrackEncoder> aAudioEncoder,
    UniquePtr<VideoTrackEncoder> aVideoEncoder,
    UniquePtr<MediaQueue<EncodedFrame>> aEncodedAudioQueue,
    UniquePtr<MediaQueue<EncodedFrame>> aEncodedVideoQueue,
    TrackRate aTrackRate, const nsAString& aMIMEType)
    : mEncoderThread(aEncoderThread),
      mEncodedAudioQueue(std::move(aEncodedAudioQueue)),
      mEncodedVideoQueue(std::move(aEncodedVideoQueue)),
      mMuxer(MakeUnique<Muxer>(std::move(aWriter), *mEncodedAudioQueue,
                               *mEncodedVideoQueue)),
      mAudioEncoder(std::move(aAudioEncoder)),
      mAudioListener(mAudioEncoder ? MakeAndAddRef<AudioTrackListener>(
                                         std::move(aDriftCompensator), this)
                                   : nullptr),
      mVideoEncoder(std::move(aVideoEncoder)),
      mVideoListener(mVideoEncoder ? MakeAndAddRef<VideoTrackListener>(this)
                                   : nullptr),
      mEncoderListener(MakeAndAddRef<EncoderListener>(mEncoderThread, this)),
      mStartTime(TimeStamp::Now()),
      mMIMEType(aMIMEType),
      mInitialized(false),
      mStarted(false),
      mCompleted(false),
      mError(false) {
  if (mAudioEncoder) {
    nsresult rv = mEncoderThread->Dispatch(NS_NewRunnableFunction(
        "mozilla::AudioTrackEncoder::RegisterListener",
        [self = RefPtr<MediaEncoder>(this), this] {
          mAudioEncoder->RegisterListener(mEncoderListener);
        }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  } else {
    mEncodedAudioQueue->Finish();
  }
  if (mVideoEncoder) {
    nsresult rv = mEncoderThread->Dispatch(NS_NewRunnableFunction(
        "mozilla::VideoTrackEncoder::RegisterListener",
        [self = RefPtr<MediaEncoder>(this), this] {
          mVideoEncoder->RegisterListener(mEncoderListener);
        }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  } else {
    mEncodedVideoQueue->Finish();
  }
}

MediaEncoder::~MediaEncoder() {
  MOZ_ASSERT(mListeners.IsEmpty());
  MOZ_ASSERT(!mAudioTrack);
  MOZ_ASSERT(!mVideoTrack);
  MOZ_ASSERT(!mAudioNode);
  MOZ_ASSERT(!mInputPort);
  MOZ_ASSERT(!mPipeTrack);
}

void MediaEncoder::EnsureGraphTrackFrom(MediaTrack* aTrack) {
  if (mGraphTrack) {
    return;
  }
  MOZ_DIAGNOSTIC_ASSERT(!aTrack->IsDestroyed());
  mGraphTrack = MakeAndAddRef<SharedDummyTrack>(
      aTrack->GraphImpl()->CreateSourceTrack(MediaSegment::VIDEO));
}

void MediaEncoder::RunOnGraph(already_AddRefed<Runnable> aRunnable) {
  MOZ_ASSERT(mGraphTrack);
  class Message : public ControlMessage {
   public:
    explicit Message(already_AddRefed<Runnable> aRunnable)
        : ControlMessage(nullptr), mRunnable(aRunnable) {}
    void Run() override { mRunnable->Run(); }
    const RefPtr<Runnable> mRunnable;
  };
  mGraphTrack->mTrack->GraphImpl()->AppendMessage(
      MakeUnique<Message>(std::move(aRunnable)));
}

void MediaEncoder::Suspend() {
  RunOnGraph(NS_NewRunnableFunction(
      "MediaEncoder::Suspend (graph)",
      [self = RefPtr<MediaEncoder>(this), this] {
        if (NS_FAILED(mEncoderThread->Dispatch(
                NS_NewRunnableFunction("MediaEncoder::Suspend (encoder)",
                                       [self, this, now = TimeStamp::Now()] {
                                         if (mAudioEncoder) {
                                           mAudioEncoder->Suspend();
                                         }
                                         if (mVideoEncoder) {
                                           mVideoEncoder->Suspend(now);
                                         }
                                       })))) {
          // RunOnGraph added an extra async step, and now `thread` has shut
          // down.
          return;
        }
      }));
}

void MediaEncoder::Resume() {
  RunOnGraph(NS_NewRunnableFunction(
      "MediaEncoder::Resume (graph)",
      [self = RefPtr<MediaEncoder>(this), this] {
        if (NS_FAILED(mEncoderThread->Dispatch(
                NS_NewRunnableFunction("MediaEncoder::Resume (encoder)",
                                       [self, this, now = TimeStamp::Now()] {
                                         if (mAudioEncoder) {
                                           mAudioEncoder->Resume();
                                         }
                                         if (mVideoEncoder) {
                                           mVideoEncoder->Resume(now);
                                         }
                                       })))) {
          // RunOnGraph added an extra async step, and now `thread` has shut
          // down.
          return;
        }
      }));
}

void MediaEncoder::ConnectAudioNode(AudioNode* aNode, uint32_t aOutput) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mAudioNode) {
    MOZ_ASSERT(false, "Only one audio node supported");
    return;
  }

  // Only AudioNodeTrack of kind EXTERNAL_OUTPUT stores output audio data in
  // the track (see AudioNodeTrack::AdvanceOutputSegment()). That means
  // forwarding input track in recorder session won't be able to copy data from
  // the track of non-destination node. Create a pipe track in this case.
  if (aNode->NumberOfOutputs() > 0) {
    AudioContext* ctx = aNode->Context();
    AudioNodeEngine* engine = new AudioNodeEngine(nullptr);
    AudioNodeTrack::Flags flags = AudioNodeTrack::EXTERNAL_OUTPUT |
                                  AudioNodeTrack::NEED_MAIN_THREAD_ENDED;
    mPipeTrack = AudioNodeTrack::Create(ctx, engine, flags, ctx->Graph());
    AudioNodeTrack* ns = aNode->GetTrack();
    if (ns) {
      mInputPort = mPipeTrack->AllocateInputPort(aNode->GetTrack(), 0, aOutput);
    }
  }

  mAudioNode = aNode;

  if (mPipeTrack) {
    mPipeTrack->AddListener(mAudioListener);
    EnsureGraphTrackFrom(mPipeTrack);
  } else {
    mAudioNode->GetTrack()->AddListener(mAudioListener);
    EnsureGraphTrackFrom(mAudioNode->GetTrack());
  }
}

void MediaEncoder::ConnectMediaStreamTrack(MediaStreamTrack* aTrack) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aTrack->Ended()) {
    MOZ_ASSERT_UNREACHABLE("Cannot connect ended track");
    return;
  }

  EnsureGraphTrackFrom(aTrack->GetTrack());

  if (AudioStreamTrack* audio = aTrack->AsAudioStreamTrack()) {
    if (!mAudioEncoder) {
      // No audio encoder for this audio track. It could be disabled.
      LOG(LogLevel::Warning, ("Cannot connect to audio track - no encoder"));
      return;
    }

    MOZ_ASSERT(!mAudioTrack, "Only one audio track supported.");
    MOZ_ASSERT(mAudioListener, "No audio listener for this audio track");

    LOG(LogLevel::Info, ("Connected to audio track %p", aTrack));

    mAudioTrack = audio;
    audio->AddListener(mAudioListener);
  } else if (VideoStreamTrack* video = aTrack->AsVideoStreamTrack()) {
    if (!mVideoEncoder) {
      // No video encoder for this video track. It could be disabled.
      LOG(LogLevel::Warning, ("Cannot connect to video track - no encoder"));
      return;
    }

    MOZ_ASSERT(!mVideoTrack, "Only one video track supported.");
    MOZ_ASSERT(mVideoListener, "No video listener for this video track");

    LOG(LogLevel::Info, ("Connected to video track %p", aTrack));

    mVideoTrack = video;
    video->AddDirectListener(mVideoListener);
    video->AddListener(mVideoListener);
  } else {
    MOZ_ASSERT(false, "Unknown track type");
  }
}

void MediaEncoder::RemoveMediaStreamTrack(MediaStreamTrack* aTrack) {
  if (!aTrack) {
    MOZ_ASSERT(false);
    return;
  }

  if (AudioStreamTrack* audio = aTrack->AsAudioStreamTrack()) {
    if (audio != mAudioTrack) {
      MOZ_ASSERT(false, "Not connected to this audio track");
      return;
    }

    if (mAudioListener) {
      audio->RemoveDirectListener(mAudioListener);
      audio->RemoveListener(mAudioListener);
    }
    mAudioTrack = nullptr;
  } else if (VideoStreamTrack* video = aTrack->AsVideoStreamTrack()) {
    if (video != mVideoTrack) {
      MOZ_ASSERT(false, "Not connected to this video track");
      return;
    }

    if (mVideoListener) {
      video->RemoveDirectListener(mVideoListener);
      video->RemoveListener(mVideoListener);
    }
    mVideoTrack = nullptr;
  }
}

/* static */
already_AddRefed<MediaEncoder> MediaEncoder::CreateEncoder(
    TaskQueue* aEncoderThread, const nsAString& aMIMEType,
    uint32_t aAudioBitrate, uint32_t aVideoBitrate, uint8_t aTrackTypes,
    TrackRate aTrackRate) {
  AUTO_PROFILER_LABEL("MediaEncoder::CreateEncoder", OTHER);

  UniquePtr<ContainerWriter> writer;
  UniquePtr<AudioTrackEncoder> audioEncoder;
  UniquePtr<VideoTrackEncoder> videoEncoder;
  auto encodedAudioQueue = MakeUnique<MediaQueue<EncodedFrame>>();
  auto encodedVideoQueue = MakeUnique<MediaQueue<EncodedFrame>>();
  auto driftCompensator =
      MakeRefPtr<DriftCompensator>(aEncoderThread, aTrackRate);

  Maybe<MediaContainerType> mimeType = MakeMediaContainerType(aMIMEType);
  if (!mimeType) {
    return nullptr;
  }

  for (const auto& codec : mimeType->ExtendedType().Codecs().Range()) {
    if (codec.EqualsLiteral("opus")) {
      MOZ_ASSERT(!audioEncoder);
      audioEncoder =
          MakeUnique<OpusTrackEncoder>(aTrackRate, *encodedAudioQueue);
    } else if (codec.EqualsLiteral("vp8") || codec.EqualsLiteral("vp8.0")) {
      MOZ_ASSERT(!videoEncoder);
      if (Preferences::GetBool("media.recorder.video.frame_drops", true)) {
        videoEncoder = MakeUnique<VP8TrackEncoder>(driftCompensator, aTrackRate,
                                                   *encodedVideoQueue,
                                                   FrameDroppingMode::ALLOW);
      } else {
        videoEncoder = MakeUnique<VP8TrackEncoder>(driftCompensator, aTrackRate,
                                                   *encodedVideoQueue,
                                                   FrameDroppingMode::DISALLOW);
      }
    } else {
      MOZ_CRASH("Unknown codec");
    }
  }

  if (mimeType->Type() == MEDIAMIMETYPE(VIDEO_WEBM) ||
      mimeType->Type() == MEDIAMIMETYPE(AUDIO_WEBM)) {
#ifdef MOZ_WEBM_ENCODER
    MOZ_ASSERT_IF(mimeType->Type() == MEDIAMIMETYPE(AUDIO_WEBM), !videoEncoder);
    writer = MakeUnique<WebMWriter>();
#else
    MOZ_CRASH("Webm cannot be selected if not supported");
#endif  // MOZ_WEBM_ENCODER
  } else if (mimeType->Type() == MEDIAMIMETYPE(AUDIO_OGG)) {
    MOZ_ASSERT(audioEncoder);
    MOZ_ASSERT(!videoEncoder);
    writer = MakeUnique<OggWriter>();
  }
  NS_ENSURE_TRUE(writer, nullptr);

  LOG(LogLevel::Info,
      ("Create encoder result:a[%p](%u bps) v[%p](%u bps) w[%p] mimeType = "
       "%s.",
       audioEncoder.get(), aAudioBitrate, videoEncoder.get(), aVideoBitrate,
       writer.get(), NS_ConvertUTF16toUTF8(aMIMEType).get()));

  if (audioEncoder) {
    audioEncoder->SetWorkerThread(aEncoderThread);
    if (aAudioBitrate != 0) {
      audioEncoder->SetBitrate(aAudioBitrate);
    }
  }
  if (videoEncoder) {
    videoEncoder->SetWorkerThread(aEncoderThread);
    if (aVideoBitrate != 0) {
      videoEncoder->SetBitrate(aVideoBitrate);
    }
  }
  return MakeAndAddRef<MediaEncoder>(
      aEncoderThread, std::move(driftCompensator), std::move(writer),
      std::move(audioEncoder), std::move(videoEncoder),
      std::move(encodedAudioQueue), std::move(encodedVideoQueue), aTrackRate,
      aMIMEType);
}

nsresult MediaEncoder::GetEncodedData(
    nsTArray<nsTArray<uint8_t>>* aOutputBufs) {
  AUTO_PROFILER_LABEL("MediaEncoder::GetEncodedData", OTHER);

  MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT_IF(mAudioEncoder, mAudioEncoder->IsInitialized());
  MOZ_ASSERT_IF(mVideoEncoder, mVideoEncoder->IsInitialized());

  LOG(LogLevel::Verbose,
      ("GetEncodedData TimeStamp = %f", GetEncodeTimeStamp()));

  nsresult rv = mMuxer->GetData(aOutputBufs);
  if (mMuxer->IsFinished()) {
    mCompleted = true;
    Shutdown();
  }

  LOG(LogLevel::Verbose,
      ("END GetEncodedData TimeStamp=%f "
       "mCompleted=%d, aComplete=%d, vComplete=%d",
       GetEncodeTimeStamp(), mCompleted,
       !mAudioEncoder || mAudioEncoder->IsEncodingComplete(),
       !mVideoEncoder || mVideoEncoder->IsEncodingComplete()));

  return rv;
}

RefPtr<GenericNonExclusivePromise> MediaEncoder::Shutdown() {
  MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
  if (mShutdownPromise) {
    return mShutdownPromise;
  }

  LOG(LogLevel::Info, ("MediaEncoder is shutting down."));
  if (mAudioEncoder) {
    mAudioEncoder->UnregisterListener(mEncoderListener);
  }
  if (mVideoEncoder) {
    mVideoEncoder->UnregisterListener(mEncoderListener);
  }
  mEncoderListener->Forget();

  for (auto& l : mListeners.Clone()) {
    // We dispatch here since this method is typically called from a listener
    // method already, and we don't want to call back in, synchronously.
    nsresult rv = mEncoderThread->Dispatch(
        NewRunnableMethod("mozilla::MediaEncoderListener::Shutdown", l,
                          &MediaEncoderListener::Shutdown));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  AutoTArray<RefPtr<GenericNonExclusivePromise>, 2> shutdownPromises;
  if (mAudioListener) {
    shutdownPromises.AppendElement(mAudioListener->OnShutdown());
  }
  if (mVideoListener) {
    shutdownPromises.AppendElement(mVideoListener->OnShutdown());
  }

  mShutdownPromise =
      GenericNonExclusivePromise::All(mEncoderThread, shutdownPromises)
          ->Then(mEncoderThread, __func__,
                 [](const GenericNonExclusivePromise::AllPromiseType::
                        ResolveOrRejectValue& aValue) {
                   if (aValue.IsResolve()) {
                     return GenericNonExclusivePromise::CreateAndResolve(
                         true, __func__);
                   }
                   return GenericNonExclusivePromise::CreateAndReject(
                       aValue.RejectValue(), __func__);
                 });

  mShutdownPromise->Then(
      mEncoderThread, __func__,
      [self = RefPtr<MediaEncoder>(this), this] { mMuxer->Disconnect(); });

  return mShutdownPromise;
}

RefPtr<GenericNonExclusivePromise> MediaEncoder::Stop() {
  MOZ_ASSERT(NS_IsMainThread());

  LOG(LogLevel::Info, ("MediaEncoder %p Stop", this));

  DisconnectTracks();

  return InvokeAsync(mEncoderThread, this, __func__, &MediaEncoder::Shutdown);
}

RefPtr<GenericNonExclusivePromise> MediaEncoder::Cancel() {
  MOZ_ASSERT(NS_IsMainThread());

  LOG(LogLevel::Info, ("MediaEncoder %p Cancel", this));

  DisconnectTracks();

  return InvokeAsync(mEncoderThread, __func__,
                     [self = RefPtr<MediaEncoder>(this), this]() {
                       if (mAudioEncoder) {
                         mAudioEncoder->Cancel();
                       }
                       if (mVideoEncoder) {
                         mVideoEncoder->Cancel();
                       }
                       return Shutdown();
                     });
}

bool MediaEncoder::HasError() {
  MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
  return mError;
}

void MediaEncoder::SetError() {
  MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

  if (mError) {
    return;
  }

  mError = true;
  for (auto& l : mListeners.Clone()) {
    l->Error();
  }
}

void MediaEncoder::DisconnectTracks() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mAudioNode) {
    mAudioNode->GetTrack()->RemoveListener(mAudioListener);
    if (mInputPort) {
      mInputPort->Destroy();
      mInputPort = nullptr;
    }
    if (mPipeTrack) {
      mPipeTrack->RemoveListener(mAudioListener);
      mPipeTrack->Destroy();
      mPipeTrack = nullptr;
    }
    mAudioNode = nullptr;
  }

  if (mAudioTrack) {
    RemoveMediaStreamTrack(mAudioTrack);
  }

  if (mVideoTrack) {
    RemoveMediaStreamTrack(mVideoTrack);
  }
}

bool MediaEncoder::IsWebMEncoderEnabled() {
#ifdef MOZ_WEBM_ENCODER
  return StaticPrefs::media_encoder_webm_enabled();
#else
  return false;
#endif
}

const nsString& MediaEncoder::MimeType() const {
  MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
  return mMIMEType;
}

void MediaEncoder::NotifyInitialized() {
  MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

  if (mInitialized) {
    // This could happen if an encoder re-inits due to a resolution change.
    return;
  }

  if (mAudioEncoder && !mAudioEncoder->IsInitialized()) {
    return;
  }

  if (mVideoEncoder && !mVideoEncoder->IsInitialized()) {
    return;
  }

  MOZ_ASSERT(mMuxer->NeedsMetadata());
  nsTArray<RefPtr<TrackMetadataBase>> meta;
  if (mAudioEncoder && !*meta.AppendElement(mAudioEncoder->GetMetadata())) {
    LOG(LogLevel::Error, ("Audio metadata is null"));
    SetError();
    return;
  }
  if (mVideoEncoder && !*meta.AppendElement(mVideoEncoder->GetMetadata())) {
    LOG(LogLevel::Error, ("Video metadata is null"));
    SetError();
    return;
  }

  if (NS_FAILED(mMuxer->SetMetadata(meta))) {
    LOG(LogLevel::Error, ("SetMetadata failed"));
    SetError();
    return;
  }

  mInitialized = true;
}

void MediaEncoder::NotifyStarted() {
  MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

  if (mStarted) {
    return;
  }

  if (mAudioEncoder && !mAudioEncoder->IsStarted()) {
    return;
  }

  if (mVideoEncoder && !mVideoEncoder->IsStarted()) {
    return;
  }

  mStarted = true;

  for (auto& l : mListeners.Clone()) {
    l->Started();
  }
}

void MediaEncoder::RegisterListener(MediaEncoderListener* aListener) {
  MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mListeners.Contains(aListener));
  mListeners.AppendElement(aListener);
}

bool MediaEncoder::UnregisterListener(MediaEncoderListener* aListener) {
  MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
  return mListeners.RemoveElement(aListener);
}

/*
 * SizeOfExcludingThis measures memory being used by the Media Encoder.
 * Currently it measures the size of the Encoder buffer and memory occupied
 * by mAudioEncoder and mVideoEncoder.
 */
size_t MediaEncoder::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) {
  MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

  size_t size = 0;
  if (mAudioEncoder) {
    size += mAudioEncoder->SizeOfExcludingThis(aMallocSizeOf);
  }
  if (mVideoEncoder) {
    size += mVideoEncoder->SizeOfExcludingThis(aMallocSizeOf);
  }
  return size;
}

void MediaEncoder::SetVideoKeyFrameInterval(
    Maybe<TimeDuration> aVideoKeyFrameInterval) {
  if (!mVideoEncoder) {
    return;
  }

  MOZ_ASSERT(mEncoderThread);
  nsresult rv = mEncoderThread->Dispatch(NS_NewRunnableFunction(
      "mozilla::VideoTrackEncoder::SetKeyFrameInterval",
      [self = RefPtr<MediaEncoder>(this), this, aVideoKeyFrameInterval] {
        mVideoEncoder->SetKeyFrameInterval(aVideoKeyFrameInterval);
      }));
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  Unused << rv;
}

}  // namespace mozilla

#undef LOG
