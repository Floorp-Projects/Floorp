/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineWebRTCAudio.h"

#include <stdio.h>
#include <algorithm>

#include "AudioConverter.h"
#include "MediaManager.h"
#include "MediaStreamGraphImpl.h"
#include "MediaTrackConstraints.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorNames.h"
#include "mtransport/runnable_utils.h"
#include "nsAutoPtr.h"
#include "Tracing.h"

// scoped_ptr.h uses FF
#ifdef FF
#  undef FF
#endif
#include "webrtc/voice_engine/voice_engine_defines.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/common_audio/include/audio_util.h"

using namespace webrtc;

// These are restrictions from the webrtc.org code
#define MAX_CHANNELS 2
#define MAX_SAMPLING_FREQ 48000  // Hz - multiple of 100

namespace mozilla {

extern LazyLogModule gMediaManagerLog;
#define LOG(...) MOZ_LOG(gMediaManagerLog, LogLevel::Debug, (__VA_ARGS__))
#define LOG_FRAME(...) \
  MOZ_LOG(gMediaManagerLog, LogLevel::Verbose, (__VA_ARGS__))
#define LOG_ERROR(...) MOZ_LOG(gMediaManagerLog, LogLevel::Error, (__VA_ARGS__))

/**
 * WebRTC Microphone MediaEngineSource.
 */

MediaEngineWebRTCMicrophoneSource::MediaEngineWebRTCMicrophoneSource(
    RefPtr<AudioDeviceInfo> aInfo, const nsString& aDeviceName,
    const nsCString& aDeviceUUID, const nsString& aDeviceGroup,
    uint32_t aMaxChannelCount, bool aDelayAgnostic, bool aExtendedFilter)
    : mTrackID(TRACK_NONE),
      mPrincipal(PRINCIPAL_HANDLE_NONE),
      mDeviceInfo(std::move(aInfo)),
      mDelayAgnostic(aDelayAgnostic),
      mExtendedFilter(aExtendedFilter),
      mDeviceName(aDeviceName),
      mDeviceUUID(aDeviceUUID),
      mDeviceGroup(aDeviceGroup),
      mDeviceMaxChannelCount(aMaxChannelCount),
      mSettings(new nsMainThreadPtrHolder<
                media::Refcountable<dom::MediaTrackSettings>>(
          "MediaEngineWebRTCMicrophoneSource::mSettings",
          new media::Refcountable<dom::MediaTrackSettings>(),
          // Non-strict means it won't assert main thread for us.
          // It would be great if it did but we're already on the media thread.
          /* aStrict = */ false)) {
#ifndef ANDROID
  MOZ_ASSERT(mDeviceInfo->DeviceID());
#endif

  // We'll init lazily as needed
  mSettings->mEchoCancellation.Construct(0);
  mSettings->mAutoGainControl.Construct(0);
  mSettings->mNoiseSuppression.Construct(0);
  mSettings->mChannelCount.Construct(0);

  mState = kReleased;
}

nsString MediaEngineWebRTCMicrophoneSource::GetName() const {
  return mDeviceName;
}

nsCString MediaEngineWebRTCMicrophoneSource::GetUUID() const {
  return mDeviceUUID;
}

nsString MediaEngineWebRTCMicrophoneSource::GetGroupId() const {
  return mDeviceGroup;
}

// GetBestFitnessDistance returns the best distance the capture device can offer
// as a whole, given an accumulated number of ConstraintSets.
// Ideal values are considered in the first ConstraintSet only.
// Plain values are treated as Ideal in the first ConstraintSet.
// Plain values are treated as Exact in subsequent ConstraintSets.
// Infinity = UINT32_MAX e.g. device cannot satisfy accumulated ConstraintSets.
// A finite result may be used to calculate this device's ranking as a choice.

uint32_t MediaEngineWebRTCMicrophoneSource::GetBestFitnessDistance(
    const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
    const nsString& aDeviceId) const {
  uint32_t distance = 0;

  for (const auto* cs : aConstraintSets) {
    distance =
        MediaConstraintsHelper::GetMinimumFitnessDistance(*cs, aDeviceId);
    break;  // distance is read from first entry only
  }
  return distance;
}

nsresult MediaEngineWebRTCMicrophoneSource::EvaluateSettings(
    const NormalizedConstraints& aConstraintsUpdate,
    const MediaEnginePrefs& aInPrefs, MediaEnginePrefs* aOutPrefs,
    const char** aOutBadConstraint) {
  AssertIsOnOwningThread();

  FlattenedConstraints c(aConstraintsUpdate);
  MediaEnginePrefs prefs = aInPrefs;

  prefs.mAecOn = c.mEchoCancellation.Get(aInPrefs.mAecOn);
  prefs.mAgcOn = c.mAutoGainControl.Get(aInPrefs.mAgcOn && prefs.mAecOn);
  prefs.mNoiseOn = c.mNoiseSuppression.Get(aInPrefs.mNoiseOn && prefs.mAecOn);

  // Determine an actual channel count to use for this source. Three factors at
  // play here: the device capabilities, the constraints passed in by content,
  // and a pref that can force things (for testing)
  int32_t maxChannels = mDeviceInfo->MaxChannels();

  // First, check channelCount violation wrt constraints. This fails in case of
  // error.
  if (c.mChannelCount.mMin > maxChannels) {
    *aOutBadConstraint = "channelCount";
    return NS_ERROR_FAILURE;
  }
  // A pref can force the channel count to use. If the pref has a value of zero
  // or lower, it has no effect.
  if (aInPrefs.mChannels <= 0) {
    prefs.mChannels = maxChannels;
  }

  // Get the number of channels asked for by content, and clamp it between the
  // pref and the maximum number of channels that the device supports.
  prefs.mChannels =
      c.mChannelCount.Get(std::min(aInPrefs.mChannels, maxChannels));
  prefs.mChannels = std::max(1, std::min(prefs.mChannels, maxChannels));

  LOG("Audio config: aec: %d, agc: %d, noise: %d, channels: %d",
      prefs.mAecOn ? prefs.mAec : -1, prefs.mAgcOn ? prefs.mAgc : -1,
      prefs.mNoiseOn ? prefs.mNoise : -1, prefs.mChannels);

  *aOutPrefs = prefs;

  return NS_OK;
}

nsresult MediaEngineWebRTCMicrophoneSource::Reconfigure(
    const dom::MediaTrackConstraints& aConstraints,
    const MediaEnginePrefs& aPrefs, const nsString& /* aDeviceId */,
    const char** aOutBadConstraint) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mStream);

  LOG("Mic source %p Reconfigure ", this);

  NormalizedConstraints constraints(aConstraints);
  MediaEnginePrefs outputPrefs;
  nsresult rv =
      EvaluateSettings(constraints, aPrefs, &outputPrefs, aOutBadConstraint);
  if (NS_FAILED(rv)) {
    if (aOutBadConstraint) {
      return NS_ERROR_INVALID_ARG;
    }

    nsAutoCString name;
    GetErrorName(rv, name);
    LOG("Mic source %p Reconfigure() failed unexpectedly. rv=%s", this,
        name.Data());
    Stop();
    return NS_ERROR_UNEXPECTED;
  }

  ApplySettings(outputPrefs);

  mCurrentPrefs = outputPrefs;

  return NS_OK;
}

void MediaEngineWebRTCMicrophoneSource::UpdateAECSettings(
    bool aEnable, bool aUseAecMobile,
    EchoCancellation::SuppressionLevel aLevel) {
  AssertIsOnOwningThread();

  RefPtr<MediaEngineWebRTCMicrophoneSource> that = this;
  RefPtr<MediaStreamGraphImpl> gripGraph = mStream->GraphImpl();
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__,
      [that, graph = std::move(gripGraph), aEnable, aUseAecMobile, aLevel] {
        class Message : public ControlMessage {
         public:
          Message(AudioInputProcessing* aInputProcessing, bool aEnable,
                  bool aUseAecMobile, EchoCancellation::SuppressionLevel aLevel)
              : ControlMessage(nullptr),
                mInputProcessing(aInputProcessing),
                mEnable(aEnable),
                mUseAecMobile(aUseAecMobile),
                mLevel(aLevel) {}

          void Run() override {
            mInputProcessing->UpdateAECSettings(mEnable, mUseAecMobile, mLevel);
          }

         protected:
          RefPtr<AudioInputProcessing> mInputProcessing;
          bool mEnable;
          bool mUseAecMobile;
          EchoCancellation::SuppressionLevel mLevel;
        };

        if (graph) {
          graph->AppendMessage(MakeUnique<Message>(
              that->mInputProcessing, aEnable, aUseAecMobile, aLevel));
        }
      }));
}

void MediaEngineWebRTCMicrophoneSource::UpdateAGCSettings(
    bool aEnable, GainControl::Mode aMode) {
  AssertIsOnOwningThread();

  RefPtr<MediaEngineWebRTCMicrophoneSource> that = this;
  RefPtr<MediaStreamGraphImpl> gripGraph = mStream->GraphImpl();
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__, [that, graph = std::move(gripGraph), aEnable, aMode] {
        class Message : public ControlMessage {
         public:
          Message(AudioInputProcessing* aInputProcessing, bool aEnable,
                  GainControl::Mode aMode)
              : ControlMessage(nullptr),
                mInputProcessing(aInputProcessing),
                mEnable(aEnable),
                mMode(aMode) {}

          void Run() override {
            mInputProcessing->UpdateAGCSettings(mEnable, mMode);
          }

         protected:
          RefPtr<AudioInputProcessing> mInputProcessing;
          bool mEnable;
          GainControl::Mode mMode;
        };

        if (graph) {
          graph->AppendMessage(
              MakeUnique<Message>(that->mInputProcessing, aEnable, aMode));
        }
      }));
}

void MediaEngineWebRTCMicrophoneSource::UpdateNSSettings(
    bool aEnable, webrtc::NoiseSuppression::Level aLevel) {
  AssertIsOnOwningThread();

  RefPtr<MediaEngineWebRTCMicrophoneSource> that = this;
  RefPtr<MediaStreamGraphImpl> gripGraph = mStream->GraphImpl();
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__, [that, graph = std::move(gripGraph), aEnable, aLevel] {
        class Message : public ControlMessage {
         public:
          Message(AudioInputProcessing* aInputProcessing, bool aEnable,
                  webrtc::NoiseSuppression::Level aLevel)
              : ControlMessage(nullptr),
                mInputProcessing(aInputProcessing),
                mEnable(aEnable),
                mLevel(aLevel) {}

          void Run() override {
            mInputProcessing->UpdateNSSettings(mEnable, mLevel);
          }

         protected:
          RefPtr<AudioInputProcessing> mInputProcessing;
          bool mEnable;
          webrtc::NoiseSuppression::Level mLevel;
        };

        if (graph) {
          graph->AppendMessage(
              MakeUnique<Message>(that->mInputProcessing, aEnable, aLevel));
        }
      }));
}

void MediaEngineWebRTCMicrophoneSource::UpdateAPMExtraOptions(
    bool aExtendedFilter, bool aDelayAgnostic) {
  AssertIsOnOwningThread();

  RefPtr<MediaEngineWebRTCMicrophoneSource> that = this;
  RefPtr<MediaStreamGraphImpl> gripGraph = mStream->GraphImpl();
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__,
      [that, graph = std::move(gripGraph), aExtendedFilter, aDelayAgnostic] {
        class Message : public ControlMessage {
         public:
          Message(AudioInputProcessing* aInputProcessing, bool aExtendedFilter,
                  bool aDelayAgnostic)
              : ControlMessage(nullptr),
                mInputProcessing(aInputProcessing),
                mExtendedFilter(aExtendedFilter),
                mDelayAgnostic(aDelayAgnostic) {}

          void Run() override {
            mInputProcessing->UpdateAPMExtraOptions(mExtendedFilter,
                                                    mDelayAgnostic);
          }

         protected:
          RefPtr<AudioInputProcessing> mInputProcessing;
          bool mExtendedFilter;
          bool mDelayAgnostic;
        };

        if (graph) {
          graph->AppendMessage(MakeUnique<Message>(
              that->mInputProcessing, aExtendedFilter, aDelayAgnostic));
        }
      }));
}

void MediaEngineWebRTCMicrophoneSource::ApplySettings(
    const MediaEnginePrefs& aPrefs) {
  AssertIsOnOwningThread();

  MOZ_ASSERT(
      mStream,
      "ApplySetting is to be called only after SetTrack has been called");

  if (mStream) {
    UpdateAGCSettings(aPrefs.mAgcOn,
                      static_cast<webrtc::GainControl::Mode>(aPrefs.mAgc));
    UpdateNSSettings(
        aPrefs.mNoiseOn,
        static_cast<webrtc::NoiseSuppression::Level>(aPrefs.mNoise));
    UpdateAECSettings(
        aPrefs.mAecOn, aPrefs.mUseAecMobile,
        static_cast<webrtc::EchoCancellation::SuppressionLevel>(aPrefs.mAec));

    UpdateAPMExtraOptions(mExtendedFilter, mDelayAgnostic);
  }

  RefPtr<MediaEngineWebRTCMicrophoneSource> that = this;
  RefPtr<MediaStreamGraphImpl> graphImpl = mStream->GraphImpl();
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__, [that, graph = std::move(graphImpl), prefs = aPrefs] {
        that->mSettings->mEchoCancellation.Value() = prefs.mAecOn;
        that->mSettings->mAutoGainControl.Value() = prefs.mAgcOn;
        that->mSettings->mNoiseSuppression.Value() = prefs.mNoiseOn;
        that->mSettings->mChannelCount.Value() = prefs.mChannels;

        class Message : public ControlMessage {
         public:
          Message(AudioInputProcessing* aInputProcessing, bool aPassThrough,
                  uint32_t aRequestedInputChannelCount)
              : ControlMessage(nullptr),
                mInputProcessing(aInputProcessing),
                mPassThrough(aPassThrough),
                mRequestedInputChannelCount(aRequestedInputChannelCount) {}

          void Run() override {
            mInputProcessing->SetPassThrough(mPassThrough);
            mInputProcessing->SetRequestedInputChannelCount(
                mRequestedInputChannelCount);
          }

         protected:
          RefPtr<AudioInputProcessing> mInputProcessing;
          bool mPassThrough;
          uint32_t mRequestedInputChannelCount;
        };

        bool passThrough = !(prefs.mAecOn || prefs.mAgcOn || prefs.mNoiseOn);
        if (graph) {
          graph->AppendMessage(MakeUnique<Message>(
              that->mInputProcessing, passThrough, prefs.mChannels));
        }
      }));
}

nsresult MediaEngineWebRTCMicrophoneSource::Allocate(
    const dom::MediaTrackConstraints& aConstraints,
    const MediaEnginePrefs& aPrefs, const nsString& aDeviceId,
    const ipc::PrincipalInfo& aPrincipalInfo, const char** aOutBadConstraint) {
  AssertIsOnOwningThread();

  mState = kAllocated;

  NormalizedConstraints normalized(aConstraints);
  MediaEnginePrefs outputPrefs;
  nsresult rv =
      EvaluateSettings(normalized, aPrefs, &outputPrefs, aOutBadConstraint);
  if (NS_FAILED(rv)) {
    return rv;
  }

  RefPtr<MediaEngineWebRTCMicrophoneSource> that = this;
  NS_DispatchToMainThread(
      NS_NewRunnableFunction(__func__, [that, prefs = outputPrefs] {
        that->mSettings->mEchoCancellation.Value() = prefs.mAecOn;
        that->mSettings->mAutoGainControl.Value() = prefs.mAgcOn;
        that->mSettings->mNoiseSuppression.Value() = prefs.mNoiseOn;
        that->mSettings->mChannelCount.Value() = prefs.mChannels;
      }));

  mCurrentPrefs = outputPrefs;

  return rv;
}

nsresult MediaEngineWebRTCMicrophoneSource::Deallocate() {
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kStopped || mState == kAllocated);

  class EndTrackMessage : public ControlMessage {
   public:
    EndTrackMessage(MediaStream* aStream,
                    AudioInputProcessing* aAudioInputProcessing,
                    TrackID aTrackID)
        : ControlMessage(aStream),
          mInputProcessing(aAudioInputProcessing),
          mTrackID(aTrackID) {}

    void Run() override {
      mInputProcessing->End();
      mStream->AsSourceStream()->EndTrack(mTrackID);
    }

   protected:
    RefPtr<AudioInputProcessing> mInputProcessing;
    TrackID mTrackID;
  };

  if (mStream && IsTrackIDExplicit(mTrackID)) {
    RefPtr<MediaStream> sourceStream = mStream;
    RefPtr<AudioInputProcessing> inputProcessing = mInputProcessing;
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        __func__, [stream = std::move(sourceStream),
                   audioInputProcessing = std::move(inputProcessing),
                   trackID = mTrackID] {
          if (stream->IsDestroyed()) {
            // This stream has already been destroyed on main thread by its
            // DOMMediaStream. No cleanup left to do.
            return;
          }
          MOZ_ASSERT(stream->GraphImpl());
          stream->GraphImpl()->AppendMessage(MakeUnique<EndTrackMessage>(
              stream, audioInputProcessing, trackID));
        }));
  }

  MOZ_ASSERT(mTrackID != TRACK_NONE, "Only deallocate once");

  // Reset all state. This is not strictly necessary, this instance will get
  // destroyed soon.
  mStream = nullptr;
  mTrackID = TRACK_NONE;
  mPrincipal = PRINCIPAL_HANDLE_NONE;

  // If empty, no callbacks to deliver data should be occuring
  MOZ_ASSERT(mState != kReleased, "Source not allocated");
  MOZ_ASSERT(mState != kStarted, "Source not stopped");

  mState = kReleased;
  LOG("Audio device %s deallocated", NS_ConvertUTF16toUTF8(mDeviceName).get());
  return NS_OK;
}

void MediaEngineWebRTCMicrophoneSource::SetTrack(
    const RefPtr<SourceMediaStream>& aStream, TrackID aTrackID,
    const PrincipalHandle& aPrincipal) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(IsTrackIDExplicit(aTrackID));

  MOZ_ASSERT(!mStream);
  MOZ_ASSERT(mTrackID == TRACK_NONE);
  MOZ_ASSERT(mPrincipal == PRINCIPAL_HANDLE_NONE);
  mStream = aStream;
  mTrackID = aTrackID;
  mPrincipal = aPrincipal;

  AudioSegment* segment = new AudioSegment();

  mStream->AddAudioTrack(mTrackID, mStream->GraphRate(), segment,
                         SourceMediaStream::ADDTRACK_QUEUED);

  mInputProcessing = new AudioInputProcessing(mDeviceMaxChannelCount, mStream,
                                              mTrackID, mPrincipal);

  // We only add the listener once -- AudioInputProcessing wants pull
  // notifications also when stopped for appending silence.
  mPullListener = new AudioInputProcessingPullListener(mInputProcessing);
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__, [self = RefPtr<MediaEngineWebRTCMicrophoneSource>(this),
                 stream = mStream, track = mTrackID, listener = mPullListener] {
        if (stream->IsDestroyed()) {
          return;
        }

        stream->AddTrackListener(listener, track);
      }));

  LOG("Stream %p registered for microphone capture", aStream.get());
}

class StartStopMessage : public ControlMessage {
 public:
  enum StartStop { Start, Stop };

  StartStopMessage(AudioInputProcessing* aInputProcessing, StartStop aAction)
      : ControlMessage(nullptr),
        mInputProcessing(aInputProcessing),
        mAction(aAction) {}

  void Run() override {
    if (mAction == StartStopMessage::Start) {
      mInputProcessing->Start();
    } else if (mAction == StartStopMessage::Stop) {
      mInputProcessing->Stop();
    } else {
      MOZ_CRASH("Invalid enum value");
    }
  }

 protected:
  RefPtr<AudioInputProcessing> mInputProcessing;
  StartStop mAction;
};

nsresult MediaEngineWebRTCMicrophoneSource::Start() {
  AssertIsOnOwningThread();

  // This spans setting both the enabled state and mState.
  if (mState == kStarted) {
    return NS_OK;
  }

  MOZ_ASSERT(mState == kAllocated || mState == kStopped);

  CubebUtils::AudioDeviceID deviceID = mDeviceInfo->DeviceID();
  if (mStream->GraphImpl()->InputDeviceID() &&
      mStream->GraphImpl()->InputDeviceID() != deviceID) {
    // For now, we only allow opening a single audio input device per document,
    // because we can only have one MSG per document.
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<MediaEngineWebRTCMicrophoneSource> that = this;
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__, [that, deviceID, stream = mStream, track = mTrackID] {
        if (stream->IsDestroyed()) {
          return;
        }

        stream->GraphImpl()->AppendMessage(MakeUnique<StartStopMessage>(
            that->mInputProcessing, StartStopMessage::Start));
        stream->SetPullingEnabled(track, true);
        stream->OpenAudioInput(deviceID, that->mInputProcessing);
      }));

  ApplySettings(mCurrentPrefs);

  MOZ_ASSERT(mState != kReleased);
  mState = kStarted;

  return NS_OK;
}

nsresult MediaEngineWebRTCMicrophoneSource::Stop() {
  AssertIsOnOwningThread();

  LOG("Mic source %p Stop()", this);
  MOZ_ASSERT(mStream, "SetTrack must have been called before ::Stop");

  if (mState == kStopped) {
    // Already stopped - this is allowed
    return NS_OK;
  }

  RefPtr<MediaEngineWebRTCMicrophoneSource> that = this;
  NS_DispatchToMainThread(
      NS_NewRunnableFunction(__func__, [that, stream = mStream] {
        if (stream->IsDestroyed()) {
          return;
        }

        stream->GraphImpl()->AppendMessage(MakeUnique<StartStopMessage>(
            that->mInputProcessing, StartStopMessage::Stop));
        CubebUtils::AudioDeviceID deviceID = that->mDeviceInfo->DeviceID();
        Maybe<CubebUtils::AudioDeviceID> id = Some(deviceID);
        stream->CloseAudioInput(id);
      }));

  MOZ_ASSERT(mState == kStarted, "Should be started when stopping");
  mState = kStopped;

  return NS_OK;
}

void MediaEngineWebRTCMicrophoneSource::GetSettings(
    dom::MediaTrackSettings& aOutSettings) const {
  MOZ_ASSERT(NS_IsMainThread());
  aOutSettings = *mSettings;
}

AudioInputProcessing::AudioInputProcessing(
    uint32_t aMaxChannelCount, RefPtr<SourceMediaStream> aStream,
    TrackID aTrackID, const PrincipalHandle& aPrincipalHandle)
    : mStream(std::move(aStream)),
      mAudioProcessing(AudioProcessing::Create()),
      mRequestedInputChannelCount(aMaxChannelCount),
      mSkipProcessing(false),
      mInputDownmixBuffer(MAX_SAMPLING_FREQ * MAX_CHANNELS / 100)
#ifdef DEBUG
      ,
      mLastCallbackAppendTime(0)
#endif
      ,
      mLiveFramesAppended(false),
      mLiveSilenceAppended(false),
      mTrackID(aTrackID),
      mPrincipal(aPrincipalHandle),
      mEnabled(false),
      mEnded(false) {
}

void AudioInputProcessing::Disconnect(MediaStreamGraphImpl* aGraph) {
  // This method is just for asserts.
  MOZ_ASSERT(aGraph->OnGraphThread());
}

void MediaEngineWebRTCMicrophoneSource::Shutdown() {
  AssertIsOnOwningThread();

  if (mState == kStarted) {
    Stop();
    MOZ_ASSERT(mState == kStopped);
  }

  MOZ_ASSERT(mState == kAllocated || mState == kStopped);
  Deallocate();
  MOZ_ASSERT(mState == kReleased);
}

bool AudioInputProcessing::PassThrough(MediaStreamGraphImpl* aGraph) const {
  MOZ_ASSERT(aGraph->OnGraphThread());
  return mSkipProcessing;
}

void AudioInputProcessing::SetPassThrough(bool aPassThrough) {
  mSkipProcessing = aPassThrough;
}

uint32_t AudioInputProcessing::GetRequestedInputChannelCount(
    MediaStreamGraphImpl* aGraphImpl) {
  return mRequestedInputChannelCount;
}

void AudioInputProcessing::SetRequestedInputChannelCount(
    uint32_t aRequestedInputChannelCount) {
  mRequestedInputChannelCount = aRequestedInputChannelCount;

  mStream->GraphImpl()->ReevaluateInputDevice();
}

// This does an early return in case of error.
#define HANDLE_APM_ERROR(fn)                       \
  do {                                             \
    int rv = fn;                                   \
    if (rv != AudioProcessing::kNoError) {         \
      MOZ_ASSERT_UNREACHABLE("APM error in " #fn); \
      return;                                      \
    }                                              \
  } while (0);

void AudioInputProcessing::UpdateAECSettings(
    bool aEnable, bool aUseAecMobile,
    EchoCancellation::SuppressionLevel aLevel) {
  if (aUseAecMobile) {
    HANDLE_APM_ERROR(mAudioProcessing->echo_control_mobile()->Enable(aEnable));
    HANDLE_APM_ERROR(mAudioProcessing->echo_cancellation()->Enable(false));
  } else {
    if (aLevel != EchoCancellation::SuppressionLevel::kLowSuppression &&
        aLevel != EchoCancellation::SuppressionLevel::kModerateSuppression &&
        aLevel != EchoCancellation::SuppressionLevel::kHighSuppression) {
      LOG_ERROR("Attempt to set invalid AEC suppression level %d",
                static_cast<int>(aLevel));

      aLevel = EchoCancellation::SuppressionLevel::kModerateSuppression;
    }

    HANDLE_APM_ERROR(mAudioProcessing->echo_control_mobile()->Enable(false));
    HANDLE_APM_ERROR(mAudioProcessing->echo_cancellation()->Enable(aEnable));
    HANDLE_APM_ERROR(
        mAudioProcessing->echo_cancellation()->set_suppression_level(aLevel));
  }
}

void AudioInputProcessing::UpdateAGCSettings(bool aEnable,
                                             GainControl::Mode aMode) {
  if (aMode != GainControl::Mode::kAdaptiveAnalog &&
      aMode != GainControl::Mode::kAdaptiveDigital &&
      aMode != GainControl::Mode::kFixedDigital) {
    LOG_ERROR("Attempt to set invalid AGC mode %d", static_cast<int>(aMode));

    aMode = GainControl::Mode::kAdaptiveDigital;
  }

#if defined(WEBRTC_IOS) || defined(ATA) || defined(WEBRTC_ANDROID)
  if (aMode == GainControl::Mode::kAdaptiveAnalog) {
    LOG_ERROR("Invalid AGC mode kAgcAdaptiveAnalog on mobile");
    MOZ_ASSERT_UNREACHABLE(
        "Bad pref set in all.js or in about:config"
        " for the auto gain, on mobile.");
    aMode = GainControl::Mode::kFixedDigital;
  }
#endif
  HANDLE_APM_ERROR(mAudioProcessing->gain_control()->set_mode(aMode));
  HANDLE_APM_ERROR(mAudioProcessing->gain_control()->Enable(aEnable));
}

void AudioInputProcessing::UpdateNSSettings(
    bool aEnable, webrtc::NoiseSuppression::Level aLevel) {
  if (aLevel != NoiseSuppression::Level::kLow &&
      aLevel != NoiseSuppression::Level::kModerate &&
      aLevel != NoiseSuppression::Level::kHigh &&
      aLevel != NoiseSuppression::Level::kVeryHigh) {
    LOG_ERROR("Attempt to set invalid noise suppression level %d",
              static_cast<int>(aLevel));

    aLevel = NoiseSuppression::Level::kModerate;
  }

  HANDLE_APM_ERROR(mAudioProcessing->noise_suppression()->set_level(aLevel));
  HANDLE_APM_ERROR(mAudioProcessing->noise_suppression()->Enable(aEnable));
}

#undef HANDLE_APM_ERROR

void AudioInputProcessing::UpdateAPMExtraOptions(bool aExtendedFilter,
                                                 bool aDelayAgnostic) {
  webrtc::Config config;
  config.Set<webrtc::ExtendedFilter>(
      new webrtc::ExtendedFilter(aExtendedFilter));
  config.Set<webrtc::DelayAgnostic>(new webrtc::DelayAgnostic(aDelayAgnostic));

  mAudioProcessing->SetExtraOptions(config);
}

void AudioInputProcessing::Start() {
  mEnabled = true;
  mLiveFramesAppended = false;
  mLiveSilenceAppended = false;
#ifdef DEBUG
  mLastCallbackAppendTime = 0;
#endif
}

void AudioInputProcessing::Stop() { mEnabled = false; }

void AudioInputProcessing::Pull(StreamTime aEndOfAppendedData,
                                StreamTime aDesiredTime) {
  TRACE_AUDIO_CALLBACK_COMMENT("SourceMediaStream %p track %i", mStream.get(),
                               mTrackID);

  if (mEnded) {
    return;
  }

  StreamTime delta = aDesiredTime - aEndOfAppendedData;
  MOZ_ASSERT(delta > 0);

  if (!mLiveFramesAppended || !mLiveSilenceAppended) {
    // These are the iterations after starting or resuming audio capture.
    // Make sure there's at least one extra block buffered until audio
    // callbacks come in. We also allow appending silence one time after
    // audio callbacks have started, to cover the case where audio callbacks
    // start appending data immediately and there is no extra data buffered.
    delta += WEBAUDIO_BLOCK_SIZE;

    // If we're supposed to be packetizing but there's no packetizer yet,
    // there must not have been any live frames appended yet.
    // If there were live frames appended and we haven't appended the
    // right amount of silence, we'll have to append silence once more,
    // failing the other assert below.
    MOZ_ASSERT_IF(!PassThrough(mStream->GraphImpl()) && !mPacketizerInput,
                  !mLiveFramesAppended);

    if (!PassThrough(mStream->GraphImpl()) && mPacketizerInput) {
      // Processing is active and is processed in chunks of 10ms through the
      // input packetizer. We allow for 10ms of silence on the track to
      // accomodate the buffering worst-case.
      delta += mPacketizerInput->PacketSize();
    }
  }

  LOG_FRAME("Pulling %" PRId64 " frames of silence.", delta);

  // This assertion fails when we append silence here in the same iteration
  // as there were real audio samples already appended by the audio callback.
  // Note that this is exempted until live samples and a subsequent chunk of
  // silence have been appended to the track. This will cover cases like:
  // - After Start(), there is silence (maybe multiple times) appended before
  //   the first audio callback.
  // - After Start(), there is real data (maybe multiple times) appended
  //   before the first graph iteration.
  // And other combinations of order of audio sample sources.
  MOZ_ASSERT_IF(mEnabled && mLiveFramesAppended && mLiveSilenceAppended,
                mStream->GraphImpl()->IterationEnd() > mLastCallbackAppendTime);

  if (mLiveFramesAppended) {
    mLiveSilenceAppended = true;
  }

  AudioSegment audio;
  audio.AppendNullData(delta);
  mStream->AppendToTrack(mTrackID, &audio);
}

void AudioInputProcessing::NotifyOutputData(MediaStreamGraphImpl* aGraph,
                                            AudioDataValue* aBuffer,
                                            size_t aFrames, TrackRate aRate,
                                            uint32_t aChannels) {
  MOZ_ASSERT(aGraph->OnGraphThread());
  MOZ_ASSERT(mEnabled);

  if (!mPacketizerOutput || mPacketizerOutput->PacketSize() != aRate / 100u ||
      mPacketizerOutput->Channels() != aChannels) {
    // It's ok to drop the audio still in the packetizer here: if this changes,
    // we changed devices or something.
    mPacketizerOutput =
        new AudioPacketizer<AudioDataValue, float>(aRate / 100, aChannels);
  }

  mPacketizerOutput->Input(aBuffer, aFrames);

  while (mPacketizerOutput->PacketsAvailable()) {
    uint32_t samplesPerPacket =
        mPacketizerOutput->PacketSize() * mPacketizerOutput->Channels();
    if (mOutputBuffer.Length() < samplesPerPacket) {
      mOutputBuffer.SetLength(samplesPerPacket);
    }
    if (mDeinterleavedBuffer.Length() < samplesPerPacket) {
      mDeinterleavedBuffer.SetLength(samplesPerPacket);
    }
    float* packet = mOutputBuffer.Data();
    mPacketizerOutput->Output(packet);

    AutoTArray<float*, MAX_CHANNELS> deinterleavedPacketDataChannelPointers;
    float* interleavedFarend = nullptr;
    uint32_t channelCountFarend = 0;
    uint32_t framesPerPacketFarend = 0;

    // Downmix from aChannels to MAX_CHANNELS if needed. We always have floats
    // here, the packetized performed the conversion.
    if (aChannels > MAX_CHANNELS) {
      AudioConverter converter(
          AudioConfig(aChannels, 0, AudioConfig::FORMAT_FLT),
          AudioConfig(MAX_CHANNELS, 0, AudioConfig::FORMAT_FLT));
      framesPerPacketFarend = mPacketizerOutput->PacketSize();
      framesPerPacketFarend =
          converter.Process(mInputDownmixBuffer, packet, framesPerPacketFarend);
      interleavedFarend = mInputDownmixBuffer.Data();
      channelCountFarend = MAX_CHANNELS;
      deinterleavedPacketDataChannelPointers.SetLength(MAX_CHANNELS);
    } else {
      interleavedFarend = packet;
      channelCountFarend = aChannels;
      framesPerPacketFarend = mPacketizerOutput->PacketSize();
      deinterleavedPacketDataChannelPointers.SetLength(aChannels);
    }

    MOZ_ASSERT(interleavedFarend &&
               (channelCountFarend == 1 || channelCountFarend == 2) &&
               framesPerPacketFarend);

    if (mInputBuffer.Length() < framesPerPacketFarend * channelCountFarend) {
      mInputBuffer.SetLength(framesPerPacketFarend * channelCountFarend);
    }

    size_t offset = 0;
    for (size_t i = 0; i < deinterleavedPacketDataChannelPointers.Length();
         ++i) {
      deinterleavedPacketDataChannelPointers[i] = mInputBuffer.Data() + offset;
      offset += framesPerPacketFarend;
    }

    // Deinterleave, prepare a channel pointers array, with enough storage for
    // the frames.
    DeinterleaveAndConvertBuffer(
        interleavedFarend, framesPerPacketFarend, channelCountFarend,
        deinterleavedPacketDataChannelPointers.Elements());

    // Having the same config for input and output means we potentially save
    // some CPU.
    StreamConfig inputConfig(aRate, channelCountFarend, false);
    StreamConfig outputConfig = inputConfig;

    // Passing the same pointers here saves a copy inside this function.
    DebugOnly<int> err = mAudioProcessing->ProcessReverseStream(
        deinterleavedPacketDataChannelPointers.Elements(), inputConfig,
        outputConfig, deinterleavedPacketDataChannelPointers.Elements());

    MOZ_ASSERT(!err, "Could not process the reverse stream.");
  }
}

// Only called if we're not in passthrough mode
void AudioInputProcessing::PacketizeAndProcess(MediaStreamGraphImpl* aGraph,
                                               const AudioDataValue* aBuffer,
                                               size_t aFrames, TrackRate aRate,
                                               uint32_t aChannels) {
  MOZ_ASSERT(!PassThrough(aGraph),
             "This should be bypassed when in PassThrough mode.");
  MOZ_ASSERT(mEnabled);
  size_t offset = 0;

  if (!mPacketizerInput || mPacketizerInput->PacketSize() != aRate / 100u ||
      mPacketizerInput->Channels() != aChannels) {
    // It's ok to drop the audio still in the packetizer here.
    mPacketizerInput =
        new AudioPacketizer<AudioDataValue, float>(aRate / 100, aChannels);
  }

  // Packetize our input data into 10ms chunks, deinterleave into planar channel
  // buffers, process, and append to the right MediaStreamTrack.
  mPacketizerInput->Input(aBuffer, static_cast<uint32_t>(aFrames));

  while (mPacketizerInput->PacketsAvailable()) {
    uint32_t samplesPerPacket =
        mPacketizerInput->PacketSize() * mPacketizerInput->Channels();
    if (mInputBuffer.Length() < samplesPerPacket) {
      mInputBuffer.SetLength(samplesPerPacket);
    }
    if (mDeinterleavedBuffer.Length() < samplesPerPacket) {
      mDeinterleavedBuffer.SetLength(samplesPerPacket);
    }
    float* packet = mInputBuffer.Data();
    mPacketizerInput->Output(packet);

    // Deinterleave the input data
    // Prepare an array pointing to deinterleaved channels.
    AutoTArray<float*, 8> deinterleavedPacketizedInputDataChannelPointers;
    deinterleavedPacketizedInputDataChannelPointers.SetLength(aChannels);
    offset = 0;
    for (size_t i = 0;
         i < deinterleavedPacketizedInputDataChannelPointers.Length(); ++i) {
      deinterleavedPacketizedInputDataChannelPointers[i] =
          mDeinterleavedBuffer.Data() + offset;
      offset += mPacketizerInput->PacketSize();
    }

    // Deinterleave to mInputBuffer, pointed to by inputBufferChannelPointers.
    Deinterleave(packet, mPacketizerInput->PacketSize(), aChannels,
                 deinterleavedPacketizedInputDataChannelPointers.Elements());

    StreamConfig inputConfig(aRate, aChannels,
                             false /* we don't use typing detection*/);
    StreamConfig outputConfig = inputConfig;

    // Bug 1404965: Get the right delay here, it saves some work down the line.
    mAudioProcessing->set_stream_delay_ms(0);

    // Bug 1414837: find a way to not allocate here.
    RefPtr<SharedBuffer> buffer = SharedBuffer::Create(
        mPacketizerInput->PacketSize() * aChannels * sizeof(float));

    // Prepare channel pointers to the SharedBuffer created above.
    AutoTArray<float*, 8> processedOutputChannelPointers;
    AutoTArray<const float*, 8> processedOutputChannelPointersConst;
    processedOutputChannelPointers.SetLength(aChannels);
    processedOutputChannelPointersConst.SetLength(aChannels);

    offset = 0;
    for (size_t i = 0; i < processedOutputChannelPointers.Length(); ++i) {
      processedOutputChannelPointers[i] =
          static_cast<float*>(buffer->Data()) + offset;
      processedOutputChannelPointersConst[i] =
          static_cast<float*>(buffer->Data()) + offset;
      offset += mPacketizerInput->PacketSize();
    }

    mAudioProcessing->ProcessStream(
        deinterleavedPacketizedInputDataChannelPointers.Elements(), inputConfig,
        outputConfig, processedOutputChannelPointers.Elements());

    AudioSegment segment;
    if (!mStream->GraphImpl()) {
      // The DOMMediaStream that owns mStream has been cleaned up
      // and MediaStream::DestroyImpl() has run in the MSG. This is fine and
      // can happen before the MediaManager thread gets to stop capture for
      // this MediaStream.
      continue;
    }

    LOG_FRAME("Appending %" PRIu32 " frames of packetized audio",
              mPacketizerInput->PacketSize());

#ifdef DEBUG
    mLastCallbackAppendTime = mStream->GraphImpl()->IterationEnd();
#endif
    mLiveFramesAppended = true;

    // We already have planar audio data of the right format. Insert into the
    // MSG.
    MOZ_ASSERT(processedOutputChannelPointers.Length() == aChannels);
    RefPtr<SharedBuffer> other = buffer;
    segment.AppendFrames(other.forget(), processedOutputChannelPointersConst,
                         mPacketizerInput->PacketSize(), mPrincipal);
    mStream->AppendToTrack(mTrackID, &segment);
  }
}

template <typename T>
void AudioInputProcessing::InsertInGraph(const T* aBuffer, size_t aFrames,
                                         uint32_t aChannels) {
  if (!mStream->GraphImpl()) {
    // The DOMMediaStream that owns mStream has been cleaned up
    // and MediaStream::DestroyImpl() has run in the MSG. This is fine and
    // can happen before the MediaManager thread gets to stop capture for
    // this MediaStream.
    return;
  }

#ifdef DEBUG
  mLastCallbackAppendTime = mStream->GraphImpl()->IterationEnd();
#endif
  mLiveFramesAppended = true;

  MOZ_ASSERT(aChannels >= 1 && aChannels <= 8, "Support up to 8 channels");

  AudioSegment segment;
  RefPtr<SharedBuffer> buffer =
      SharedBuffer::Create(aFrames * aChannels * sizeof(T));
  AutoTArray<const T*, 8> channels;
  if (aChannels == 1) {
    PodCopy(static_cast<T*>(buffer->Data()), aBuffer, aFrames);
    channels.AppendElement(static_cast<T*>(buffer->Data()));
  } else {
    channels.SetLength(aChannels);
    AutoTArray<T*, 8> write_channels;
    write_channels.SetLength(aChannels);
    T* samples = static_cast<T*>(buffer->Data());

    size_t offset = 0;
    for (uint32_t i = 0; i < aChannels; ++i) {
      channels[i] = write_channels[i] = samples + offset;
      offset += aFrames;
    }

    DeinterleaveAndConvertBuffer(aBuffer, aFrames, aChannels,
                                 write_channels.Elements());
  }

  LOG_FRAME("Appending %zu frames of raw audio", aFrames);

  MOZ_ASSERT(aChannels == channels.Length());
  segment.AppendFrames(buffer.forget(), channels, aFrames, mPrincipal);

  mStream->AppendToTrack(mTrackID, &segment);
}

// Called back on GraphDriver thread!
// Note this can be called back after ::Shutdown()
void AudioInputProcessing::NotifyInputData(MediaStreamGraphImpl* aGraph,
                                           const AudioDataValue* aBuffer,
                                           size_t aFrames, TrackRate aRate,
                                           uint32_t aChannels) {
  MOZ_ASSERT(aGraph->OnGraphThread());
  TRACE_AUDIO_CALLBACK();

  MOZ_ASSERT(mEnabled);

  // If some processing is necessary, packetize and insert in the WebRTC.org
  // code. Otherwise, directly insert the mic data in the MSG, bypassing all
  // processing.
  if (PassThrough(aGraph)) {
    InsertInGraph<AudioDataValue>(aBuffer, aFrames, aChannels);
  } else {
    PacketizeAndProcess(aGraph, aBuffer, aFrames, aRate, aChannels);
  }
}

#define ResetProcessingIfNeeded(_processing)                         \
  do {                                                               \
    bool enabled = mAudioProcessing->_processing()->is_enabled();    \
                                                                     \
    if (enabled) {                                                   \
      int rv = mAudioProcessing->_processing()->Enable(!enabled);    \
      if (rv) {                                                      \
        NS_WARNING("Could not reset the status of the " #_processing \
                   " on device change.");                            \
        return;                                                      \
      }                                                              \
      rv = mAudioProcessing->_processing()->Enable(enabled);         \
      if (rv) {                                                      \
        NS_WARNING("Could not reset the status of the " #_processing \
                   " on device change.");                            \
        return;                                                      \
      }                                                              \
    }                                                                \
  } while (0)

void AudioInputProcessing::DeviceChanged(MediaStreamGraphImpl* aGraph) {
  MOZ_ASSERT(aGraph->OnGraphThread());
  // Reset some processing
  ResetProcessingIfNeeded(gain_control);
  ResetProcessingIfNeeded(echo_cancellation);
  ResetProcessingIfNeeded(noise_suppression);
}

void AudioInputProcessing::End() { mEnded = true; }

nsString MediaEngineWebRTCAudioCaptureSource::GetName() const {
  return NS_LITERAL_STRING(u"AudioCapture");
}

nsCString MediaEngineWebRTCAudioCaptureSource::GetUUID() const {
  nsID uuid;
  char uuidBuffer[NSID_LENGTH];
  nsCString asciiString;
  ErrorResult rv;

  rv = nsContentUtils::GenerateUUIDInPlace(uuid);
  if (rv.Failed()) {
    return NS_LITERAL_CSTRING("");
  }

  uuid.ToProvidedString(uuidBuffer);
  asciiString.AssignASCII(uuidBuffer);

  // Remove {} and the null terminator
  return nsCString(Substring(asciiString, 1, NSID_LENGTH - 3));
}

nsString MediaEngineWebRTCAudioCaptureSource::GetGroupId() const {
  return NS_LITERAL_STRING(u"AudioCaptureGroup");
}

void MediaEngineWebRTCAudioCaptureSource::SetTrack(
    const RefPtr<SourceMediaStream>& aStream, TrackID aTrackID,
    const PrincipalHandle& aPrincipalHandle) {
  AssertIsOnOwningThread();
  // Nothing to do here. aStream is a placeholder dummy and not exposed.
}

nsresult MediaEngineWebRTCAudioCaptureSource::Start() {
  AssertIsOnOwningThread();
  return NS_OK;
}

nsresult MediaEngineWebRTCAudioCaptureSource::Stop() {
  AssertIsOnOwningThread();
  return NS_OK;
}

nsresult MediaEngineWebRTCAudioCaptureSource::Reconfigure(
    const dom::MediaTrackConstraints& aConstraints,
    const MediaEnginePrefs& aPrefs, const nsString& aDeviceId,
    const char** aOutBadConstraint) {
  return NS_OK;
}

uint32_t MediaEngineWebRTCAudioCaptureSource::GetBestFitnessDistance(
    const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
    const nsString& aDeviceId) const {
  // There is only one way of capturing audio for now, and it's always adequate.
  return 0;
}

}  // namespace mozilla
