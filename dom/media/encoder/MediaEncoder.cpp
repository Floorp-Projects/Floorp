/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEncoder.h"

#include <algorithm>
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "DriftCompensation.h"
#include "GeckoProfiler.h"
#include "MediaDecoder.h"
#include "MediaStreamGraphImpl.h"
#include "MediaStreamListener.h"
#include "mozilla/dom/AudioNode.h"
#include "mozilla/dom/AudioStreamTrack.h"
#include "mozilla/dom/MediaStreamTrack.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "mozilla/gfx/Point.h"  // IntSize
#include "mozilla/Logging.h"
#include "mozilla/media/MediaUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/Unused.h"
#include "nsIPrincipal.h"
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

#ifdef LOG
#  undef LOG
#endif

mozilla::LazyLogModule gMediaEncoderLog("MediaEncoder");
#define LOG(type, msg) MOZ_LOG(gMediaEncoderLog, type, msg)

namespace mozilla {

using namespace dom;
using namespace media;

class MediaEncoder::AudioTrackListener : public DirectMediaStreamTrackListener {
 public:
  AudioTrackListener(DriftCompensator* aDriftCompensator,
                     AudioTrackEncoder* aEncoder, TaskQueue* aEncoderThread)
      : mDirectConnected(false),
        mInitialized(false),
        mRemoved(false),
        mDriftCompensator(aDriftCompensator),
        mEncoder(aEncoder),
        mEncoderThread(aEncoderThread) {
    MOZ_ASSERT(mEncoder);
    MOZ_ASSERT(mEncoderThread);
  }

  void NotifyShutdown() { mShutdown = true; }

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
      mEncoder = nullptr;
      mEncoderThread = nullptr;
    }
  }

  void NotifyQueuedChanges(MediaStreamGraph* aGraph, StreamTime aTrackOffset,
                           const MediaSegment& aQueuedMedia) override {
    TRACE_COMMENT("Encoder %p", mEncoder.get());
    MOZ_ASSERT(mEncoder);
    MOZ_ASSERT(mEncoderThread);

    if (mShutdown) {
      return;
    }

    if (!mInitialized) {
      mDriftCompensator->NotifyAudioStart(TimeStamp::Now());
      mInitialized = true;
    }

    mDriftCompensator->NotifyAudio(aQueuedMedia.GetDuration());

    const AudioSegment& audio = static_cast<const AudioSegment&>(aQueuedMedia);

    AudioSegment copy;
    copy.AppendSlice(audio, 0, audio.GetDuration());

    nsresult rv = mEncoderThread->Dispatch(
        NewRunnableMethod<StoreCopyPassByRRef<AudioSegment>>(
            "mozilla::AudioTrackEncoder::AppendAudioSegment", mEncoder,
            &AudioTrackEncoder::AppendAudioSegment, std::move(copy)));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  void NotifyEnded() override {
    MOZ_ASSERT(mEncoder);
    MOZ_ASSERT(mEncoderThread);

    if (mShutdown) {
      return;
    }

    nsresult rv = mEncoderThread->Dispatch(
        NewRunnableMethod("mozilla::AudioTrackEncoder::NotifyEndOfStream",
                          mEncoder, &AudioTrackEncoder::NotifyEndOfStream));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  void NotifyRemoved() override {
    if (!mShutdown) {
      nsresult rv = mEncoderThread->Dispatch(
          NewRunnableMethod("mozilla::AudioTrackEncoder::NotifyEndOfStream",
                            mEncoder, &AudioTrackEncoder::NotifyEndOfStream));
      MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
      Unused << rv;
    }

    mRemoved = true;

    if (!mDirectConnected) {
      mEncoder = nullptr;
      mEncoderThread = nullptr;
    }
  }

 private:
  // True when MediaEncoder has shutdown and destroyed the TaskQueue.
  Atomic<bool> mShutdown;
  bool mDirectConnected;
  bool mInitialized;
  bool mRemoved;
  const RefPtr<DriftCompensator> mDriftCompensator;
  RefPtr<AudioTrackEncoder> mEncoder;
  RefPtr<TaskQueue> mEncoderThread;
};

class MediaEncoder::VideoTrackListener : public DirectMediaStreamTrackListener {
 public:
  VideoTrackListener(VideoTrackEncoder* aEncoder, TaskQueue* aEncoderThread)
      : mDirectConnected(false),
        mInitialized(false),
        mRemoved(false),
        mEncoder(aEncoder),
        mEncoderThread(aEncoderThread) {
    MOZ_ASSERT(mEncoder);
    MOZ_ASSERT(mEncoderThread);
  }

  void NotifyShutdown() { mShutdown = true; }

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
      mEncoder = nullptr;
      mEncoderThread = nullptr;
    }
  }

  void NotifyQueuedChanges(MediaStreamGraph* aGraph, StreamTime aTrackOffset,
                           const MediaSegment& aQueuedMedia) override {
    TRACE_COMMENT("Encoder %p", mEncoder.get());
    MOZ_ASSERT(mEncoder);
    MOZ_ASSERT(mEncoderThread);

    if (mShutdown) {
      return;
    }

    const TimeStamp now = TimeStamp::Now();
    if (!mInitialized) {
      nsresult rv = mEncoderThread->Dispatch(NewRunnableMethod<TimeStamp>(
          "mozilla::VideoTrackEncoder::SetStartOffset", mEncoder,
          &VideoTrackEncoder::SetStartOffset, now));
      MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
      Unused << rv;
      mInitialized = true;
    }

    nsresult rv = mEncoderThread->Dispatch(NewRunnableMethod<TimeStamp>(
        "mozilla::VideoTrackEncoder::AdvanceCurrentTime", mEncoder,
        &VideoTrackEncoder::AdvanceCurrentTime, now));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  void NotifyRealtimeTrackData(MediaStreamGraph* aGraph,
                               StreamTime aTrackOffset,
                               const MediaSegment& aMedia) override {
    TRACE_COMMENT("Encoder %p", mEncoder.get());
    MOZ_ASSERT(mEncoder);
    MOZ_ASSERT(mEncoderThread);
    MOZ_ASSERT(aMedia.GetType() == MediaSegment::VIDEO);

    if (mShutdown) {
      return;
    }

    const VideoSegment& video = static_cast<const VideoSegment&>(aMedia);
    VideoSegment copy;
    for (VideoSegment::ConstChunkIterator iter(video); !iter.IsEnded();
         iter.Next()) {
      copy.AppendFrame(do_AddRef(iter->mFrame.GetImage()),
                       iter->mFrame.GetIntrinsicSize(),
                       iter->mFrame.GetPrincipalHandle(),
                       iter->mFrame.GetForceBlack(), iter->mTimeStamp);
    }

    nsresult rv = mEncoderThread->Dispatch(
        NewRunnableMethod<StoreCopyPassByRRef<VideoSegment>>(
            "mozilla::VideoTrackEncoder::AppendVideoSegment", mEncoder,
            &VideoTrackEncoder::AppendVideoSegment, std::move(copy)));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  void NotifyEnabledStateChanged(bool aEnabled) override {
    MOZ_ASSERT(mEncoder);
    MOZ_ASSERT(mEncoderThread);

    if (mShutdown) {
      return;
    }

    nsresult rv;
    if (aEnabled) {
      rv = mEncoderThread->Dispatch(NewRunnableMethod<TimeStamp>(
          "mozilla::VideoTrackEncoder::Enable", mEncoder,
          &VideoTrackEncoder::Enable, TimeStamp::Now()));
    } else {
      rv = mEncoderThread->Dispatch(NewRunnableMethod<TimeStamp>(
          "mozilla::VideoTrackEncoder::Disable", mEncoder,
          &VideoTrackEncoder::Disable, TimeStamp::Now()));
    }
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  void NotifyEnded() override {
    MOZ_ASSERT(mEncoder);
    MOZ_ASSERT(mEncoderThread);

    if (mShutdown) {
      return;
    }

    nsresult rv = mEncoderThread->Dispatch(
        NewRunnableMethod("mozilla::VideoTrackEncoder::NotifyEndOfStream",
                          mEncoder, &VideoTrackEncoder::NotifyEndOfStream));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  void NotifyRemoved() override {
    if (!mShutdown) {
      nsresult rv = mEncoderThread->Dispatch(
          NewRunnableMethod("mozilla::VideoTrackEncoder::NotifyEndOfStream",
                            mEncoder, &VideoTrackEncoder::NotifyEndOfStream));
      MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
      Unused << rv;
    }

    mRemoved = true;

    if (!mDirectConnected) {
      mEncoder = nullptr;
      mEncoderThread = nullptr;
    }
  }

 private:
  // True when MediaEncoder has shutdown and destroyed the TaskQueue.
  Atomic<bool> mShutdown;
  bool mDirectConnected;
  bool mInitialized;
  bool mRemoved;
  RefPtr<VideoTrackEncoder> mEncoder;
  RefPtr<TaskQueue> mEncoderThread;
};

class MediaEncoder::EncoderListener : public TrackEncoderListener {
 public:
  EncoderListener(TaskQueue* aEncoderThread, MediaEncoder* aEncoder)
      : mEncoderThread(aEncoderThread),
        mEncoder(aEncoder),
        mPendingDataAvailable(false) {}

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

  void DataAvailable(TrackEncoder* aTrackEncoder) override {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
    MOZ_ASSERT(aTrackEncoder->IsInitialized());

    if (!mEncoder) {
      return;
    }

    if (mPendingDataAvailable) {
      return;
    }

    nsresult rv = mEncoderThread->Dispatch(NewRunnableMethod(
        "mozilla::MediaEncoder::EncoderListener::DataAvailableImpl", this,
        &EncoderListener::DataAvailableImpl));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;

    mPendingDataAvailable = true;
  }

  void DataAvailableImpl() {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

    if (!mEncoder) {
      return;
    }

    mEncoder->NotifyDataAvailable();
    mPendingDataAvailable = false;
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
  bool mPendingDataAvailable;
};

MediaEncoder::MediaEncoder(TaskQueue* aEncoderThread,
                           RefPtr<DriftCompensator> aDriftCompensator,
                           UniquePtr<ContainerWriter> aWriter,
                           AudioTrackEncoder* aAudioEncoder,
                           VideoTrackEncoder* aVideoEncoder,
                           TrackRate aTrackRate, const nsAString& aMIMEType)
    : mEncoderThread(aEncoderThread),
      mWriter(std::move(aWriter)),
      mAudioEncoder(aAudioEncoder),
      mVideoEncoder(aVideoEncoder),
      mEncoderListener(MakeAndAddRef<EncoderListener>(mEncoderThread, this)),
      mStartTime(TimeStamp::Now()),
      mMIMEType(aMIMEType),
      mInitialized(false),
      mMetadataEncoded(false),
      mCompleted(false),
      mError(false),
      mCanceled(false),
      mShutdown(false) {
  if (mAudioEncoder) {
    mAudioListener = MakeAndAddRef<AudioTrackListener>(
        aDriftCompensator, mAudioEncoder, mEncoderThread);
    nsresult rv =
        mEncoderThread->Dispatch(NewRunnableMethod<RefPtr<EncoderListener>>(
            "mozilla::AudioTrackEncoder::RegisterListener", mAudioEncoder,
            &AudioTrackEncoder::RegisterListener, mEncoderListener));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }
  if (mVideoEncoder) {
    mVideoListener =
        MakeAndAddRef<VideoTrackListener>(mVideoEncoder, mEncoderThread);
    nsresult rv =
        mEncoderThread->Dispatch(NewRunnableMethod<RefPtr<EncoderListener>>(
            "mozilla::VideoTrackEncoder::RegisterListener", mVideoEncoder,
            &VideoTrackEncoder::RegisterListener, mEncoderListener));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }
}

MediaEncoder::~MediaEncoder() { MOZ_ASSERT(mListeners.IsEmpty()); }

void MediaEncoder::RunOnGraph(already_AddRefed<Runnable> aRunnable) {
  MediaStreamGraphImpl* graph;
  if (mAudioTrack) {
    graph = mAudioTrack->GraphImpl();
  } else if (mVideoTrack) {
    graph = mVideoTrack->GraphImpl();
  } else if (mPipeStream) {
    graph = mPipeStream->GraphImpl();
  } else {
    MOZ_CRASH("No graph");
  }
  class Message : public ControlMessage {
   public:
    explicit Message(already_AddRefed<Runnable> aRunnable)
        : ControlMessage(nullptr), mRunnable(aRunnable) {}
    void Run() override { mRunnable->Run(); }
    const RefPtr<Runnable> mRunnable;
  };
  graph->AppendMessage(MakeUnique<Message>(std::move(aRunnable)));
}

void MediaEncoder::Suspend() {
  RunOnGraph(NS_NewRunnableFunction(
      "MediaEncoder::Suspend",
      [thread = mEncoderThread, audio = mAudioEncoder, video = mVideoEncoder] {
        if (audio) {
          nsresult rv = thread->Dispatch(
              NewRunnableMethod("AudioTrackEncoder::Suspend", audio,
                                &AudioTrackEncoder::Suspend));
          MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
          Unused << rv;
        }
        if (video) {
          nsresult rv = thread->Dispatch(NewRunnableMethod<TimeStamp>(
              "VideoTrackEncoder::Suspend", video, &VideoTrackEncoder::Suspend,
              TimeStamp::Now()));
          MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
          Unused << rv;
        }
      }));
}

void MediaEncoder::Resume() {
  RunOnGraph(NS_NewRunnableFunction(
      "MediaEncoder::Resume",
      [thread = mEncoderThread, audio = mAudioEncoder, video = mVideoEncoder] {
        if (audio) {
          nsresult rv = thread->Dispatch(NewRunnableMethod(
              "AudioTrackEncoder::Resume", audio, &AudioTrackEncoder::Resume));
          MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
          Unused << rv;
        }
        if (video) {
          nsresult rv = thread->Dispatch(NewRunnableMethod<TimeStamp>(
              "VideoTrackEncoder::Resume", video, &VideoTrackEncoder::Resume,
              TimeStamp::Now()));
          MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
          Unused << rv;
        }
      }));
}

void MediaEncoder::ConnectAudioNode(AudioNode* aNode, uint32_t aOutput) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mAudioNode) {
    MOZ_ASSERT(false, "Only one audio node supported");
    return;
  }

  // Only AudioNodeStream of kind EXTERNAL_OUTPUT stores output audio data in
  // the track (see AudioNodeStream::AdvanceOutputSegment()). That means track
  // union stream in recorder session won't be able to copy data from the
  // stream of non-destination node. Create a pipe stream in this case.
  if (aNode->NumberOfOutputs() > 0) {
    AudioContext* ctx = aNode->Context();
    AudioNodeEngine* engine = new AudioNodeEngine(nullptr);
    AudioNodeStream::Flags flags = AudioNodeStream::EXTERNAL_OUTPUT |
                                   AudioNodeStream::NEED_MAIN_THREAD_FINISHED;
    mPipeStream = AudioNodeStream::Create(ctx, engine, flags, ctx->Graph());
    AudioNodeStream* ns = aNode->GetStream();
    if (ns) {
      mInputPort = mPipeStream->AllocateInputPort(aNode->GetStream(), TRACK_ANY,
                                                  TRACK_ANY, 0, aOutput);
    }
  }

  mAudioNode = aNode;

  if (mPipeStream) {
    mPipeStream->AddTrackListener(mAudioListener, AudioNodeStream::AUDIO_TRACK);
  } else {
    mAudioNode->GetStream()->AddTrackListener(mAudioListener,
                                              AudioNodeStream::AUDIO_TRACK);
  }
}

void MediaEncoder::ConnectMediaStreamTrack(MediaStreamTrack* aTrack) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aTrack->Ended()) {
    NS_ASSERTION(false, "Cannot connect ended track");
    return;
  }

  if (AudioStreamTrack* audio = aTrack->AsAudioStreamTrack()) {
    if (!mAudioEncoder) {
      MOZ_ASSERT(false, "No audio encoder for this audio track");
      return;
    }
    if (mAudioTrack) {
      MOZ_ASSERT(false, "Only one audio track supported.");
      return;
    }
    if (!mAudioListener) {
      MOZ_ASSERT(false, "No audio listener for this audio track");
      return;
    }

    mAudioTrack = audio;
    // With full duplex we don't risk having audio come in late to the MSG
    // so we won't need a direct listener.
    const bool enableDirectListener =
        !Preferences::GetBool("media.navigator.audio.full_duplex", false);
    if (enableDirectListener) {
      audio->AddDirectListener(mAudioListener);
    }
    audio->AddListener(mAudioListener);
  } else if (VideoStreamTrack* video = aTrack->AsVideoStreamTrack()) {
    if (!mVideoEncoder) {
      MOZ_ASSERT(false, "No video encoder for this video track");
      return;
    }
    if (mVideoTrack) {
      MOZ_ASSERT(false, "Only one video track supported.");
      return;
    }
    if (!mVideoListener) {
      MOZ_ASSERT(false, "No video listener for this audio track");
      return;
    }

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
  RefPtr<AudioTrackEncoder> audioEncoder;
  RefPtr<VideoTrackEncoder> videoEncoder;
  auto driftCompensator =
      MakeRefPtr<DriftCompensator>(aEncoderThread, aTrackRate);
  nsString mimeType;

  if (!aTrackTypes) {
    MOZ_ASSERT(false);
    LOG(LogLevel::Error, ("No TrackTypes"));
    return nullptr;
  }
#ifdef MOZ_WEBM_ENCODER
  else if (MediaEncoder::IsWebMEncoderEnabled() &&
           (aMIMEType.EqualsLiteral(VIDEO_WEBM) ||
            (aTrackTypes & ContainerWriter::CREATE_VIDEO_TRACK))) {
    if (aTrackTypes & ContainerWriter::CREATE_AUDIO_TRACK &&
        MediaDecoder::IsOpusEnabled()) {
      audioEncoder = MakeAndAddRef<OpusTrackEncoder>(aTrackRate);
      NS_ENSURE_TRUE(audioEncoder, nullptr);
    }
    if (Preferences::GetBool("media.recorder.video.frame_drops", true)) {
      videoEncoder = MakeAndAddRef<VP8TrackEncoder>(
          driftCompensator, aTrackRate, FrameDroppingMode::ALLOW);
    } else {
      videoEncoder = MakeAndAddRef<VP8TrackEncoder>(
          driftCompensator, aTrackRate, FrameDroppingMode::DISALLOW);
    }
    writer = MakeUnique<WebMWriter>(aTrackTypes);
    NS_ENSURE_TRUE(writer, nullptr);
    NS_ENSURE_TRUE(videoEncoder, nullptr);
    mimeType = NS_LITERAL_STRING(VIDEO_WEBM);
  }
#endif  // MOZ_WEBM_ENCODER
  else if (MediaDecoder::IsOggEnabled() && MediaDecoder::IsOpusEnabled() &&
           (aMIMEType.EqualsLiteral(AUDIO_OGG) ||
            (aTrackTypes & ContainerWriter::CREATE_AUDIO_TRACK))) {
    writer = MakeUnique<OggWriter>();
    audioEncoder = MakeAndAddRef<OpusTrackEncoder>(aTrackRate);
    NS_ENSURE_TRUE(writer, nullptr);
    NS_ENSURE_TRUE(audioEncoder, nullptr);
    mimeType = NS_LITERAL_STRING(AUDIO_OGG);
  } else {
    LOG(LogLevel::Error,
        ("Can not find any encoder to record this media stream"));
    return nullptr;
  }

  LOG(LogLevel::Info,
      ("Create encoder result:a[%p](%u bps) v[%p](%u bps) w[%p] mimeType = %s.",
       audioEncoder.get(), aAudioBitrate, videoEncoder.get(), aVideoBitrate,
       writer.get(), NS_ConvertUTF16toUTF8(mimeType).get()));

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
      audioEncoder, videoEncoder, aTrackRate, mimeType);
}

nsresult MediaEncoder::GetEncodedMetadata(
    nsTArray<nsTArray<uint8_t>>* aOutputBufs, nsAString& aMIMEType) {
  AUTO_PROFILER_LABEL("MediaEncoder::GetEncodedMetadata", OTHER);

  MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

  if (mShutdown) {
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  if (!mInitialized) {
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  if (mMetadataEncoded) {
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  aMIMEType = mMIMEType;

  LOG(LogLevel::Verbose,
      ("GetEncodedMetadata TimeStamp = %f", GetEncodeTimeStamp()));

  nsresult rv;

  if (mAudioEncoder) {
    if (!mAudioEncoder->IsInitialized()) {
      LOG(LogLevel::Error,
          ("GetEncodedMetadata Audio encoder not initialized"));
      MOZ_ASSERT(false);
      return NS_ERROR_FAILURE;
    }
    rv = CopyMetadataToMuxer(mAudioEncoder);
    if (NS_FAILED(rv)) {
      LOG(LogLevel::Error, ("Failed to Set Audio Metadata"));
      SetError();
      return rv;
    }
  }
  if (mVideoEncoder) {
    if (!mVideoEncoder->IsInitialized()) {
      LOG(LogLevel::Error,
          ("GetEncodedMetadata Video encoder not initialized"));
      MOZ_ASSERT(false);
      return NS_ERROR_FAILURE;
    }
    rv = CopyMetadataToMuxer(mVideoEncoder.get());
    if (NS_FAILED(rv)) {
      LOG(LogLevel::Error, ("Failed to Set Video Metadata"));
      SetError();
      return rv;
    }
  }

  rv = mWriter->GetContainerData(aOutputBufs, ContainerWriter::GET_HEADER);
  if (NS_FAILED(rv)) {
    LOG(LogLevel::Error, ("Writer fail to generate header!"));
    SetError();
    return rv;
  }
  LOG(LogLevel::Verbose,
      ("Finish GetEncodedMetadata TimeStamp = %f", GetEncodeTimeStamp()));
  mMetadataEncoded = true;

  return NS_OK;
}

nsresult MediaEncoder::GetEncodedData(
    nsTArray<nsTArray<uint8_t>>* aOutputBufs) {
  AUTO_PROFILER_LABEL("MediaEncoder::GetEncodedData", OTHER);

  MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

  if (!mMetadataEncoded) {
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  LOG(LogLevel::Verbose,
      ("GetEncodedData TimeStamp = %f", GetEncodeTimeStamp()));
  EncodedFrameContainer encodedData;

  if (mVideoEncoder) {
    // We're most likely to actually wait for a video frame, so do that first
    // to minimize capture offset/lipsync issues.
    rv = WriteEncodedDataToMuxer(mVideoEncoder);
    LOG(LogLevel::Verbose,
        ("Video encoded TimeStamp = %f", GetEncodeTimeStamp()));
    if (NS_FAILED(rv)) {
      LOG(LogLevel::Warning, ("Failed to write encoded video data to muxer"));
      return rv;
    }
  }

  if (mAudioEncoder) {
    rv = WriteEncodedDataToMuxer(mAudioEncoder);
    LOG(LogLevel::Verbose,
        ("Audio encoded TimeStamp = %f", GetEncodeTimeStamp()));
    if (NS_FAILED(rv)) {
      LOG(LogLevel::Warning, ("Failed to write encoded audio data to muxer"));
      return rv;
    }
  }

  // In audio only or video only case, let unavailable track's flag to be true.
  bool isAudioCompleted = !mAudioEncoder || mAudioEncoder->IsEncodingComplete();
  bool isVideoCompleted = !mVideoEncoder || mVideoEncoder->IsEncodingComplete();
  rv = mWriter->GetContainerData(
      aOutputBufs,
      isAudioCompleted && isVideoCompleted ? ContainerWriter::FLUSH_NEEDED : 0);
  if (mWriter->IsWritingComplete()) {
    mCompleted = true;
    Shutdown();
  }

  LOG(LogLevel::Verbose,
      ("END GetEncodedData TimeStamp=%f "
       "mCompleted=%d, aComplete=%d, vComplete=%d",
       GetEncodeTimeStamp(), mCompleted, isAudioCompleted, isVideoCompleted));

  return rv;
}

void MediaEncoder::Shutdown() {
  MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
  if (mShutdown) {
    return;
  }
  mShutdown = true;

  LOG(LogLevel::Info, ("MediaEncoder has been shut down."));
  if (mAudioEncoder) {
    mAudioEncoder->UnregisterListener(mEncoderListener);
  }
  if (mAudioListener) {
    mAudioListener->NotifyShutdown();
  }
  if (mVideoEncoder) {
    mVideoEncoder->UnregisterListener(mEncoderListener);
  }
  if (mVideoListener) {
    mVideoListener->NotifyShutdown();
  }
  mEncoderListener->Forget();

  if (mCanceled) {
    // Shutting down after being canceled. We cannot use the encoder thread.
    return;
  }

  auto listeners(mListeners);
  for (auto& l : listeners) {
    // We dispatch here since this method is typically called from
    // a DataAvailable() handler.
    nsresult rv = mEncoderThread->Dispatch(
        NewRunnableMethod("mozilla::MediaEncoderListener::Shutdown", l,
                          &MediaEncoderListener::Shutdown));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }
}

nsresult MediaEncoder::WriteEncodedDataToMuxer(TrackEncoder* aTrackEncoder) {
  AUTO_PROFILER_LABEL("MediaEncoder::WriteEncodedDataToMuxer", OTHER);

  MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

  if (!aTrackEncoder) {
    NS_ERROR("No track encoder to get data from");
    return NS_ERROR_FAILURE;
  }

  if (aTrackEncoder->IsEncodingComplete()) {
    return NS_OK;
  }

  EncodedFrameContainer encodedData;
  nsresult rv = aTrackEncoder->GetEncodedTrack(encodedData);
  if (NS_FAILED(rv)) {
    // Encoding might be canceled.
    LOG(LogLevel::Error, ("Failed to get encoded data from encoder."));
    SetError();
    return rv;
  }
  rv = mWriter->WriteEncodedTrack(
      encodedData,
      aTrackEncoder->IsEncodingComplete() ? ContainerWriter::END_OF_STREAM : 0);
  if (NS_FAILED(rv)) {
    LOG(LogLevel::Error,
        ("Failed to write encoded track to the media container."));
    SetError();
  }
  return rv;
}

nsresult MediaEncoder::CopyMetadataToMuxer(TrackEncoder* aTrackEncoder) {
  AUTO_PROFILER_LABEL("MediaEncoder::CopyMetadataToMuxer", OTHER);

  MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

  if (!aTrackEncoder) {
    NS_ERROR("No track encoder to get metadata from");
    return NS_ERROR_FAILURE;
  }

  RefPtr<TrackMetadataBase> meta = aTrackEncoder->GetMetadata();
  if (meta == nullptr) {
    LOG(LogLevel::Error, ("metadata == null"));
    SetError();
    return NS_ERROR_ABORT;
  }

  nsresult rv = mWriter->SetMetadata(meta);
  if (NS_FAILED(rv)) {
    LOG(LogLevel::Error, ("SetMetadata failed"));
    SetError();
  }
  return rv;
}

bool MediaEncoder::IsShutdown() {
  MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
  return mShutdown;
}

void MediaEncoder::Cancel() {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<MediaEncoder> self = this;
  nsresult rv = mEncoderThread->Dispatch(NewRunnableFrom([self]() mutable {
    self->mCanceled = true;

    if (self->mAudioEncoder) {
      self->mAudioEncoder->Cancel();
    }
    if (self->mVideoEncoder) {
      self->mVideoEncoder->Cancel();
    }
    self->Shutdown();
    return NS_OK;
  }));
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  Unused << rv;
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
  auto listeners(mListeners);
  for (auto& l : listeners) {
    l->Error();
  }
}

void MediaEncoder::Stop() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mAudioNode) {
    mAudioNode->GetStream()->RemoveTrackListener(mAudioListener,
                                                 AudioNodeStream::AUDIO_TRACK);
    if (mInputPort) {
      mInputPort->Destroy();
      mInputPort = nullptr;
    }
    if (mPipeStream) {
      mPipeStream->RemoveTrackListener(mAudioListener,
                                       AudioNodeStream::AUDIO_TRACK);
      mPipeStream->Destroy();
      mPipeStream = nullptr;
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

#ifdef MOZ_WEBM_ENCODER
bool MediaEncoder::IsWebMEncoderEnabled() {
  return StaticPrefs::MediaEncoderWebMEnabled();
}
#endif

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

  mInitialized = true;

  auto listeners(mListeners);
  for (auto& l : listeners) {
    l->Initialized();
  }
}

void MediaEncoder::NotifyDataAvailable() {
  MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

  if (!mInitialized) {
    return;
  }

  auto listeners(mListeners);
  for (auto& l : listeners) {
    l->DataAvailable();
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

void MediaEncoder::SetVideoKeyFrameInterval(int32_t aVideoKeyFrameInterval) {
  if (!mVideoEncoder) {
    return;
  }

  MOZ_ASSERT(mEncoderThread);
  nsresult rv = mEncoderThread->Dispatch(NewRunnableMethod<int32_t>(
      "mozilla::VideoTrackEncoder::SetKeyFrameInterval", mVideoEncoder,
      &VideoTrackEncoder::SetKeyFrameInterval, aVideoKeyFrameInterval));
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  Unused << rv;
}

}  // namespace mozilla
