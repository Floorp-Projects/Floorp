/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineWebRTCAudio.h"

#include <stdio.h>
#include <algorithm>

#include "AudioConverter.h"
#include "MediaManager.h"
#include "MediaTrackGraphImpl.h"
#include "MediaTrackConstraints.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorNames.h"
#include "nsContentUtils.h"
#include "nsIDUtils.h"
#include "transport/runnable_utils.h"
#include "Tracing.h"

#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/include/audio_processing.h"

using namespace webrtc;

// These are restrictions from the webrtc.org code
#define MAX_CHANNELS 2
#define MONO 1
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
    uint32_t aMaxChannelCount)
    : mPrincipal(PRINCIPAL_HANDLE_NONE),
      mDeviceInfo(std::move(aInfo)),
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
  prefs.mChannels = c.mChannelCount.Get(std::min(prefs.mChannels, maxChannels));
  prefs.mChannels = std::max(1, std::min(prefs.mChannels, maxChannels));

  LOG("Audio config: agc: %d, noise: %d, channels: %d",
      prefs.mAgcOn ? prefs.mAgc : -1, prefs.mNoiseOn ? prefs.mNoise : -1,
      prefs.mChannels);

  *aOutPrefs = prefs;

  return NS_OK;
}

nsresult MediaEngineWebRTCMicrophoneSource::Reconfigure(
    const dom::MediaTrackConstraints& aConstraints,
    const MediaEnginePrefs& aPrefs, const char** aOutBadConstraint) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTrack);

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

void MediaEngineWebRTCMicrophoneSource::ApplySettings(
    const MediaEnginePrefs& aPrefs) {
  AssertIsOnOwningThread();

  TRACE("ApplySettings");
  MOZ_ASSERT(
      mTrack,
      "ApplySetting is to be called only after SetTrack has been called");

  mAudioProcessingConfig.pipeline.multi_channel_render = true;
  mAudioProcessingConfig.pipeline.multi_channel_capture = true;

  mAudioProcessingConfig.echo_canceller.enabled = aPrefs.mAecOn;
  mAudioProcessingConfig.echo_canceller.mobile_mode = aPrefs.mUseAecMobile;

  if ((mAudioProcessingConfig.gain_controller1.enabled =
           aPrefs.mAgcOn && !aPrefs.mAgc2Forced)) {
    auto mode = static_cast<AudioProcessing::Config::GainController1::Mode>(
        aPrefs.mAgc);
    if (mode != AudioProcessing::Config::GainController1::kAdaptiveAnalog &&
        mode != AudioProcessing::Config::GainController1::kAdaptiveDigital &&
        mode != AudioProcessing::Config::GainController1::kFixedDigital) {
      LOG_ERROR("AudioInputProcessing %p Attempt to set invalid AGC mode %d",
                mInputProcessing.get(), static_cast<int>(mode));
      mode = AudioProcessing::Config::GainController1::kAdaptiveDigital;
    }
#if defined(WEBRTC_IOS) || defined(ATA) || defined(WEBRTC_ANDROID)
    if (mode == AudioProcessing::Config::GainController1::kAdaptiveAnalog) {
      LOG_ERROR(
          "AudioInputProcessing %p Invalid AGC mode kAdaptiveAnalog on "
          "mobile",
          mInputProcessing.get());
      MOZ_ASSERT_UNREACHABLE(
          "Bad pref set in all.js or in about:config"
          " for the auto gain, on mobile.");
      mode = AudioProcessing::Config::GainController1::kFixedDigital;
    }
#endif
    mAudioProcessingConfig.gain_controller1.mode = mode;
  }
  mAudioProcessingConfig.gain_controller2.enabled =
      mAudioProcessingConfig.gain_controller2.adaptive_digital.enabled =
          aPrefs.mAgcOn && aPrefs.mAgc2Forced;

  if ((mAudioProcessingConfig.noise_suppression.enabled = aPrefs.mNoiseOn)) {
    auto level = static_cast<AudioProcessing::Config::NoiseSuppression::Level>(
        aPrefs.mNoise);
    if (level != AudioProcessing::Config::NoiseSuppression::kLow &&
        level != AudioProcessing::Config::NoiseSuppression::kModerate &&
        level != AudioProcessing::Config::NoiseSuppression::kHigh &&
        level != AudioProcessing::Config::NoiseSuppression::kVeryHigh) {
      LOG_ERROR(
          "AudioInputProcessing %p Attempt to set invalid noise suppression "
          "level %d",
          mInputProcessing.get(), static_cast<int>(level));

      level = AudioProcessing::Config::NoiseSuppression::kModerate;
    }
    mAudioProcessingConfig.noise_suppression.level = level;
  }

  mAudioProcessingConfig.transient_suppression.enabled = aPrefs.mTransientOn;

  mAudioProcessingConfig.high_pass_filter.enabled = aPrefs.mHPFOn;

  mAudioProcessingConfig.residual_echo_detector.enabled =
      aPrefs.mResidualEchoOn;

  RefPtr<MediaEngineWebRTCMicrophoneSource> that = this;
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__, [this, that, track = mTrack, prefs = aPrefs,
                 audioProcessingConfig = mAudioProcessingConfig] {
        mSettings->mEchoCancellation.Value() = prefs.mAecOn;
        mSettings->mAutoGainControl.Value() = prefs.mAgcOn;
        mSettings->mNoiseSuppression.Value() = prefs.mNoiseOn;
        mSettings->mChannelCount.Value() = prefs.mChannels;

        class Message : public ControlMessage {
          const RefPtr<AudioInputProcessing> mInputProcessing;
          const AudioProcessing::Config mAudioProcessingConfig;
          const bool mPassThrough;
          const uint32_t mRequestedInputChannelCount;

         public:
          Message(MediaTrack* aTrack, AudioInputProcessing* aInputProcessing,
                  const AudioProcessing::Config& aAudioProcessingConfig,
                  bool aPassThrough, uint32_t aRequestedInputChannelCount)
              : ControlMessage(aTrack),
                mInputProcessing(aInputProcessing),
                mAudioProcessingConfig(std::move(aAudioProcessingConfig)),
                mPassThrough(aPassThrough),
                mRequestedInputChannelCount(aRequestedInputChannelCount) {}

          void Run() override {
            mInputProcessing->ApplyConfig(mTrack->GraphImpl(),
                                          mAudioProcessingConfig);
            {
              TRACE("SetPassThrough")
              mInputProcessing->SetPassThrough(mTrack->GraphImpl(),
                                               mPassThrough);
            }
            {
              TRACE("SetRequestedInputChannelCount");
              mInputProcessing->SetRequestedInputChannelCount(
                  mTrack->GraphImpl(), mRequestedInputChannelCount);
            }
          }
        };

        // The high-pass filter is not taken into account when activating the
        // pass through, since it's not controllable from content.
        bool passThrough = !(prefs.mAecOn || prefs.mAgcOn || prefs.mNoiseOn);

        if (track->IsDestroyed()) {
          return;
        }
        track->GraphImpl()->AppendMessage(
            MakeUnique<Message>(track, mInputProcessing, audioProcessingConfig,
                                passThrough, prefs.mChannels));
      }));
}

nsresult MediaEngineWebRTCMicrophoneSource::Allocate(
    const dom::MediaTrackConstraints& aConstraints,
    const MediaEnginePrefs& aPrefs, uint64_t aWindowID,
    const char** aOutBadConstraint) {
  AssertIsOnOwningThread();

  mState = kAllocated;

  NormalizedConstraints normalized(aConstraints);
  MediaEnginePrefs outputPrefs;
  nsresult rv =
      EvaluateSettings(normalized, aPrefs, &outputPrefs, aOutBadConstraint);
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__, [settings = mSettings, prefs = outputPrefs] {
        settings->mEchoCancellation.Value() = prefs.mAecOn;
        settings->mAutoGainControl.Value() = prefs.mAgcOn;
        settings->mNoiseSuppression.Value() = prefs.mNoiseOn;
        settings->mChannelCount.Value() = prefs.mChannels;
      }));

  mCurrentPrefs = outputPrefs;

  return rv;
}

nsresult MediaEngineWebRTCMicrophoneSource::Deallocate() {
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kStopped || mState == kAllocated);

  class EndTrackMessage : public ControlMessage {
    const RefPtr<AudioInputProcessing> mInputProcessing;

   public:
    explicit EndTrackMessage(AudioInputProcessing* aAudioInputProcessing)
        : ControlMessage(nullptr), mInputProcessing(aAudioInputProcessing) {}

    void Run() override {
      TRACE("mInputProcessing::End");
      mInputProcessing->End();
    }
  };

  if (mTrack) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        __func__,
        [track = std::move(mTrack), inputProcessing = mInputProcessing] {
          if (track->IsDestroyed()) {
            // This track has already been destroyed on main thread by its
            // DOMMediaStream. No cleanup left to do.
            return;
          }
          track->GraphImpl()->AppendMessage(
              MakeUnique<EndTrackMessage>(inputProcessing));
        }));
  }

  // Reset all state. This is not strictly necessary, this instance will get
  // destroyed soon.
  mTrack = nullptr;
  mPrincipal = PRINCIPAL_HANDLE_NONE;

  // If empty, no callbacks to deliver data should be occuring
  MOZ_ASSERT(mState != kReleased, "Source not allocated");
  MOZ_ASSERT(mState != kStarted, "Source not stopped");

  mState = kReleased;
  LOG("Mic source %p Audio device %s deallocated", this,
      NS_ConvertUTF16toUTF8(mDeviceName).get());
  return NS_OK;
}

void MediaEngineWebRTCMicrophoneSource::SetTrack(
    const RefPtr<MediaTrack>& aTrack, const PrincipalHandle& aPrincipal) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aTrack);
  MOZ_ASSERT(aTrack->AsAudioInputTrack());

  MOZ_ASSERT(!mTrack);
  MOZ_ASSERT(mPrincipal == PRINCIPAL_HANDLE_NONE);
  mTrack = aTrack->AsAudioInputTrack();
  mPrincipal = aPrincipal;

  mInputProcessing =
      MakeAndAddRef<AudioInputProcessing>(mDeviceMaxChannelCount, mPrincipal);

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__, [track = mTrack, processing = mInputProcessing]() mutable {
        track->SetInputProcessing(std::move(processing));
        track->Resume();  // Suspended by MediaManager
      }));

  LOG("Mic source %p Track %p registered for microphone capture", this,
      aTrack.get());
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
      TRACE("InputProcessing::Start")
      mInputProcessing->Start();
    } else if (mAction == StartStopMessage::Stop) {
      TRACE("InputProcessing::Stop")
      mInputProcessing->Stop();
    } else {
      MOZ_CRASH("Invalid enum value");
    }
  }

 protected:
  const RefPtr<AudioInputProcessing> mInputProcessing;
  const StartStop mAction;
};

nsresult MediaEngineWebRTCMicrophoneSource::Start() {
  AssertIsOnOwningThread();

  // This spans setting both the enabled state and mState.
  if (mState == kStarted) {
    return NS_OK;
  }

  MOZ_ASSERT(mState == kAllocated || mState == kStopped);

  // This check is unreliable due to potential in-flight device updates.
  // Multiple input devices are reliably excluded in OpenAudioInputImpl(),
  // but the check here provides some error reporting most of the
  // time.
  CubebUtils::AudioDeviceID deviceID = mDeviceInfo->DeviceID();
  if (mTrack->GraphImpl()->InputDeviceID() &&
      mTrack->GraphImpl()->InputDeviceID() != deviceID) {
    // For now, we only allow opening a single audio input device per document,
    // because we can only have one MTG per document.
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__, [inputProcessing = mInputProcessing, deviceID, track = mTrack] {
        if (track->IsDestroyed()) {
          return;
        }

        track->GraphImpl()->AppendMessage(MakeUnique<StartStopMessage>(
            inputProcessing, StartStopMessage::Start));
        track->OpenAudioInput(deviceID, inputProcessing);
      }));

  ApplySettings(mCurrentPrefs);

  MOZ_ASSERT(mState != kReleased);
  mState = kStarted;

  return NS_OK;
}

nsresult MediaEngineWebRTCMicrophoneSource::Stop() {
  AssertIsOnOwningThread();

  LOG("Mic source %p Stop()", this);
  MOZ_ASSERT(mTrack, "SetTrack must have been called before ::Stop");

  if (mState == kStopped) {
    // Already stopped - this is allowed
    return NS_OK;
  }

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__, [inputProcessing = mInputProcessing, deviceInfo = mDeviceInfo,
                 track = mTrack] {
        if (track->IsDestroyed()) {
          return;
        }

        track->GraphImpl()->AppendMessage(MakeUnique<StartStopMessage>(
            inputProcessing, StartStopMessage::Stop));
        MOZ_ASSERT(track->DeviceId().value() == deviceInfo->DeviceID());
        track->CloseAudioInput();
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
    uint32_t aMaxChannelCount, const PrincipalHandle& aPrincipalHandle)
    : mAudioProcessing(AudioProcessingBuilder().Create()),
      mRequestedInputChannelCount(aMaxChannelCount),
      mSkipProcessing(false),
      mInputDownmixBuffer(MAX_SAMPLING_FREQ * MAX_CHANNELS / 100),
      mLiveBufferingAppended(Nothing()),
      mPrincipal(aPrincipalHandle),
      mEnabled(false),
      mEnded(false) {}

void AudioInputProcessing::Disconnect(MediaTrackGraphImpl* aGraph) {
  // This method is just for asserts.
  MOZ_ASSERT(aGraph->OnGraphThread());
}

bool AudioInputProcessing::PassThrough(MediaTrackGraphImpl* aGraph) const {
  MOZ_ASSERT(aGraph->OnGraphThread());
  return mSkipProcessing;
}

void AudioInputProcessing::SetPassThrough(MediaTrackGraphImpl* aGraph,
                                          bool aPassThrough) {
  MOZ_ASSERT(aGraph->OnGraphThread());

  if (!mSkipProcessing && aPassThrough) {
    // Reset AudioProcessing so that if we resume processing in the future it
    // doesn't depend on old state.
    mAudioProcessing->Initialize();

    if (mPacketizerInput) {
      MOZ_ASSERT(mPacketizerInput->PacketsAvailable() == 0);
      LOG_FRAME(
          "AudioInputProcessing %p Appending %u frames of null data for data "
          "discarded in the packetizer",
          this, mPacketizerInput->FramesAvailable());
      mSegment.AppendNullData(mPacketizerInput->FramesAvailable());
      mPacketizerInput->Clear();
    }
  }
  mSkipProcessing = aPassThrough;
}

uint32_t AudioInputProcessing::GetRequestedInputChannelCount() {
  return mRequestedInputChannelCount;
}

void AudioInputProcessing::SetRequestedInputChannelCount(
    MediaTrackGraphImpl* aGraph, uint32_t aRequestedInputChannelCount) {
  mRequestedInputChannelCount = aRequestedInputChannelCount;

  aGraph->ReevaluateInputDevice();
}

void AudioInputProcessing::Start() {
  mEnabled = true;
  mLiveBufferingAppended = Nothing();
}

void AudioInputProcessing::Stop() { mEnabled = false; }

void AudioInputProcessing::Pull(MediaTrackGraphImpl* aGraph, GraphTime aFrom,
                                GraphTime aTo, GraphTime aTrackEnd,
                                AudioSegment* aSegment,
                                bool aLastPullThisIteration, bool* aEnded) {
  MOZ_ASSERT(aGraph->OnGraphThread());

  if (mEnded) {
    *aEnded = true;
    return;
  }

  TrackTime delta = aTo - aTrackEnd;
  MOZ_ASSERT(delta >= 0, "We shouldn't append more than requested");
  TrackTime buffering = 0;

  // Add the amount of buffering required to not underrun and glitch.

  // Make sure there's at least one extra block buffered until audio callbacks
  // come in, since we round graph iteration durations up to the nearest block.
  buffering += WEBAUDIO_BLOCK_SIZE;

  if (!PassThrough(aGraph) && mPacketizerInput) {
    // Processing is active and is processed in chunks of 10ms through the
    // input packetizer. We allow for 10ms of silence on the track to
    // accomodate the buffering worst-case.
    buffering += mPacketizerInput->mPacketSize;
  }

  if (delta <= 0) {
    return;
  }

  if (MOZ_LIKELY(mLiveBufferingAppended)) {
    if (MOZ_UNLIKELY(buffering > *mLiveBufferingAppended)) {
      // We need to buffer more data. This could happen the first time we pull
      // input data, or the first iteration after starting to use the
      // packetizer.
      TrackTime silence = buffering - *mLiveBufferingAppended;
      LOG_FRAME("AudioInputProcessing %p Inserting %" PRId64
                " frames of silence due to buffer increase",
                this, silence);
      mSegment.InsertNullDataAtStart(silence);
      mLiveBufferingAppended = Some(buffering);
    } else if (MOZ_UNLIKELY(buffering < *mLiveBufferingAppended)) {
      // We need to clear some buffered data to reduce latency now that the
      // packetizer is no longer used.
      MOZ_ASSERT(PassThrough(aGraph), "Must have turned on passthrough");
      TrackTime removal = *mLiveBufferingAppended - buffering;
      MOZ_ASSERT(mSegment.GetDuration() >= removal);
      TrackTime frames = std::min(mSegment.GetDuration(), removal);
      LOG_FRAME("AudioInputProcessing %p Removing %" PRId64
                " frames of silence due to buffer decrease",
                this, frames);
      *mLiveBufferingAppended -= frames;
      mSegment.RemoveLeading(frames);
    }
  }

  if (mSegment.GetDuration() > 0) {
    MOZ_ASSERT(buffering == *mLiveBufferingAppended);
    TrackTime frames = std::min(mSegment.GetDuration(), delta);
    LOG_FRAME("AudioInputProcessing %p Appending %" PRId64
              " frames of real data for %u channels.",
              this, frames, mRequestedInputChannelCount);
    aSegment->AppendSlice(mSegment, 0, frames);
    mSegment.RemoveLeading(frames);
    delta -= frames;

    // Assert that the amount of data buffered doesn't grow unboundedly.
    MOZ_ASSERT_IF(aLastPullThisIteration, mSegment.GetDuration() <= buffering);
  }

  if (delta <= 0) {
    if (mSegment.GetDuration() == 0) {
      mLiveBufferingAppended = Some(-delta);
    }
    return;
  }

  LOG_FRAME("AudioInputProcessing %p Pulling %" PRId64
            " frames of silence for %u channels.",
            this, delta, mRequestedInputChannelCount);

  // This assertion fails if we append silence here after having appended live
  // frames. Before appending live frames we should add sufficient buffering to
  // not have to glitch (aka append silence). Failing this meant the buffering
  // was not sufficient.
  MOZ_ASSERT_IF(mEnabled, !mLiveBufferingAppended);
  mLiveBufferingAppended = Nothing();

  aSegment->AppendNullData(delta);
}

void AudioInputProcessing::NotifyOutputData(MediaTrackGraphImpl* aGraph,
                                            AudioDataValue* aBuffer,
                                            size_t aFrames, TrackRate aRate,
                                            uint32_t aChannels) {
  MOZ_ASSERT(aGraph->OnGraphThread());
  MOZ_ASSERT(mEnabled);

  if (PassThrough(aGraph)) {
    return;
  }

  if (!mPacketizerOutput || mPacketizerOutput->mPacketSize != aRate / 100u ||
      mPacketizerOutput->mChannels != aChannels) {
    // It's ok to drop the audio still in the packetizer here: if this changes,
    // we changed devices or something.
    mPacketizerOutput = MakeUnique<AudioPacketizer<AudioDataValue, float>>(
        aRate / 100, aChannels);
  }

  mPacketizerOutput->Input(aBuffer, aFrames);

  while (mPacketizerOutput->PacketsAvailable()) {
    uint32_t samplesPerPacket =
        mPacketizerOutput->mPacketSize * mPacketizerOutput->mChannels;
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

    // Downmix from aChannels to MAX_CHANNELS if needed. We always have
    // floats here, the packetized performed the conversion.
    if (aChannels > MAX_CHANNELS) {
      AudioConverter converter(
          AudioConfig(aChannels, 0, AudioConfig::FORMAT_FLT),
          AudioConfig(MAX_CHANNELS, 0, AudioConfig::FORMAT_FLT));
      framesPerPacketFarend = mPacketizerOutput->mPacketSize;
      framesPerPacketFarend =
          converter.Process(mInputDownmixBuffer, packet, framesPerPacketFarend);
      interleavedFarend = mInputDownmixBuffer.Data();
      channelCountFarend = MAX_CHANNELS;
      deinterleavedPacketDataChannelPointers.SetLength(MAX_CHANNELS);
    } else {
      interleavedFarend = packet;
      channelCountFarend = aChannels;
      framesPerPacketFarend = mPacketizerOutput->mPacketSize;
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
void AudioInputProcessing::PacketizeAndProcess(MediaTrackGraphImpl* aGraph,
                                               const AudioDataValue* aBuffer,
                                               size_t aFrames, TrackRate aRate,
                                               uint32_t aChannels) {
  MOZ_ASSERT(!PassThrough(aGraph),
             "This should be bypassed when in PassThrough mode.");
  MOZ_ASSERT(mEnabled);
  size_t offset = 0;

  if (!mPacketizerInput || mPacketizerInput->mPacketSize != aRate / 100u ||
      mPacketizerInput->mChannels != aChannels) {
    // It's ok to drop the audio still in the packetizer here.
    mPacketizerInput = MakeUnique<AudioPacketizer<AudioDataValue, float>>(
        aRate / 100, aChannels);
  }

  LOG_FRAME("AudioInputProcessing %p Appending %zu frames to packetizer", this,
            aFrames);

  // Packetize our input data into 10ms chunks, deinterleave into planar channel
  // buffers, process, and append to the right MediaStreamTrack.
  mPacketizerInput->Input(aBuffer, static_cast<uint32_t>(aFrames));

  while (mPacketizerInput->PacketsAvailable()) {
    uint32_t samplesPerPacket =
        mPacketizerInput->mPacketSize * mPacketizerInput->mChannels;
    if (mInputBuffer.Length() < samplesPerPacket) {
      mInputBuffer.SetLength(samplesPerPacket);
    }
    if (mDeinterleavedBuffer.Length() < samplesPerPacket) {
      mDeinterleavedBuffer.SetLength(samplesPerPacket);
    }
    float* packet = mInputBuffer.Data();
    mPacketizerInput->Output(packet);

    // Downmix from aChannels to mono if needed. We always have floats
    // here, the packetizer performed the conversion. This handles sound cards
    // with multiple physical jacks exposed as a single device with _n_
    // discrete channels, where only a single mic is plugged in. Those channels
    // are not correlated temporaly since they are discrete channels, mixing is
    // just a sum.
    AutoTArray<float*, 8> deinterleavedPacketizedInputDataChannelPointers;
    uint32_t channelCountInput = 0;
    if (aChannels > MAX_CHANNELS) {
      channelCountInput = MONO;
      deinterleavedPacketizedInputDataChannelPointers.SetLength(
          channelCountInput);
      deinterleavedPacketizedInputDataChannelPointers[0] =
          mDeinterleavedBuffer.Data();
      // Downmix to mono (and effectively have a planar buffer) by summing all
      // channels in the first channel.
      size_t readIndex = 0;
      for (size_t i = 0; i < mPacketizerInput->mPacketSize; i++) {
        mDeinterleavedBuffer.Data()[i] = 0.;
        for (size_t j = 0; j < aChannels; j++) {
          mDeinterleavedBuffer.Data()[i] += packet[readIndex++];
        }
      }
    } else {
      channelCountInput = aChannels;
      // Deinterleave the input data
      // Prepare an array pointing to deinterleaved channels.
      deinterleavedPacketizedInputDataChannelPointers.SetLength(
          channelCountInput);
      offset = 0;
      for (size_t i = 0;
           i < deinterleavedPacketizedInputDataChannelPointers.Length(); ++i) {
        deinterleavedPacketizedInputDataChannelPointers[i] =
            mDeinterleavedBuffer.Data() + offset;
        offset += mPacketizerInput->mPacketSize;
      }
      // Deinterleave to mInputBuffer, pointed to by inputBufferChannelPointers.
      Deinterleave(packet, mPacketizerInput->mPacketSize, channelCountInput,
                   deinterleavedPacketizedInputDataChannelPointers.Elements());
    }

    StreamConfig inputConfig(aRate, channelCountInput,
                             false /* we don't use typing detection*/);
    StreamConfig outputConfig = inputConfig;

    // Bug 1404965: Get the right delay here, it saves some work down the line.
    mAudioProcessing->set_stream_delay_ms(0);

    // Bug 1414837: find a way to not allocate here.
    CheckedInt<size_t> bufferSize(sizeof(float));
    bufferSize *= mPacketizerInput->mPacketSize;
    bufferSize *= channelCountInput;
    RefPtr<SharedBuffer> buffer = SharedBuffer::Create(bufferSize);

    // Prepare channel pointers to the SharedBuffer created above.
    AutoTArray<float*, 8> processedOutputChannelPointers;
    AutoTArray<const float*, 8> processedOutputChannelPointersConst;
    processedOutputChannelPointers.SetLength(channelCountInput);
    processedOutputChannelPointersConst.SetLength(channelCountInput);

    offset = 0;
    for (size_t i = 0; i < processedOutputChannelPointers.Length(); ++i) {
      processedOutputChannelPointers[i] =
          static_cast<float*>(buffer->Data()) + offset;
      processedOutputChannelPointersConst[i] =
          static_cast<float*>(buffer->Data()) + offset;
      offset += mPacketizerInput->mPacketSize;
    }

    mAudioProcessing->ProcessStream(
        deinterleavedPacketizedInputDataChannelPointers.Elements(), inputConfig,
        outputConfig, processedOutputChannelPointers.Elements());

    if (mEnded) {
      continue;
    }

    LOG_FRAME("AudioInputProcessing %p Appending %u frames of packetized audio",
              this, mPacketizerInput->mPacketSize);

    // We already have planar audio data of the right format. Insert into the
    // MTG.
    MOZ_ASSERT(processedOutputChannelPointers.Length() == channelCountInput);
    RefPtr<SharedBuffer> other = buffer;
    mSegment.AppendFrames(other.forget(), processedOutputChannelPointersConst,
                          mPacketizerInput->mPacketSize, mPrincipal);
  }
}

void AudioInputProcessing::ProcessInput(MediaTrackGraphImpl* aGraph,
                                        const AudioSegment* aSegment) {
  MOZ_ASSERT(aGraph);
  MOZ_ASSERT(aGraph->OnGraphThread());

  if (mEnded || !mEnabled || !mLiveBufferingAppended ||
      mPendingData.IsEmpty()) {
    return;
  }

  // The number of NotifyInputData and ProcessInput calls could be different. We
  // always process the input data from NotifyInputData in the first
  // ProcessInput after the NotifyInputData

  // If some processing is necessary, packetize and insert in the WebRTC.org
  // code. Otherwise, directly insert the mic data in the MTG, bypassing all
  // processing.
  if (PassThrough(aGraph)) {
    if (aSegment && !aSegment->IsEmpty()) {
      mSegment.AppendSegment(aSegment, mPrincipal);
    } else {
      mSegment.AppendFromInterleavedBuffer(mPendingData.Data(),
                                           mPendingData.FrameCount(),
                                           mPendingData.Channels(), mPrincipal);
    }
  } else {
    MOZ_ASSERT(aGraph->GraphRate() == mPendingData.Rate());
    // Bug 1729041: Feed aSegment to PacketizeAndProcess so mPendingData can be
    // removed, and save a copy.
    PacketizeAndProcess(aGraph, mPendingData.Data(), mPendingData.FrameCount(),
                        mPendingData.Rate(), mPendingData.Channels());
  }

  mPendingData.Clear();
}

void AudioInputProcessing::NotifyInputStopped(MediaTrackGraphImpl* aGraph) {
  MOZ_ASSERT(aGraph->OnGraphThread());
  // This is called when an AudioCallbackDriver switch has happened for any
  // reason, including other reasons than starting this audio input stream. We
  // reset state when this happens, as a fallback driver may have fiddled with
  // the amount of buffered silence during the switch.
  mLiveBufferingAppended = Nothing();
  mSegment.Clear();
  if (mPacketizerInput) {
    mPacketizerInput->Clear();
  }
  mPendingData.Clear();
}

// Called back on GraphDriver thread!
// Note this can be called back after ::Stop()
void AudioInputProcessing::NotifyInputData(MediaTrackGraphImpl* aGraph,
                                           const AudioDataValue* aBuffer,
                                           size_t aFrames, TrackRate aRate,
                                           uint32_t aChannels,
                                           uint32_t aAlreadyBuffered) {
  MOZ_ASSERT(aGraph->OnGraphThread());
  TRACE("AudioInputProcessing::NotifyInputData");

  MOZ_ASSERT(mEnabled);

  if (!mLiveBufferingAppended) {
    // First time we see live frames getting added. Use what's already buffered
    // in the driver's scratch buffer as a starting point.
    mLiveBufferingAppended = Some(aAlreadyBuffered);
  }

  mPendingData.Push(aBuffer, aFrames, aRate, aChannels);
}

void AudioInputProcessing::DeviceChanged(MediaTrackGraphImpl* aGraph) {
  MOZ_ASSERT(aGraph->OnGraphThread());

  // Reset some processing
  mAudioProcessing->Initialize();
}

void AudioInputProcessing::ApplyConfig(MediaTrackGraphImpl* aGraph,
                                       const AudioProcessing::Config& aConfig) {
  MOZ_ASSERT(aGraph->OnGraphThread());
  mAudioProcessing->ApplyConfig(aConfig);
}

void AudioInputProcessing::End() {
  mEnded = true;
  mSegment.Clear();
  mPendingData.Clear();
}

TrackTime AudioInputProcessing::NumBufferedFrames(
    MediaTrackGraphImpl* aGraph) const {
  MOZ_ASSERT(aGraph->OnGraphThread());
  return mSegment.GetDuration();
}

void AudioInputTrack::Destroy() {
  MOZ_ASSERT(NS_IsMainThread());
  CloseAudioInput();

  MediaTrack::Destroy();
}

void AudioInputTrack::SetInputProcessing(
    RefPtr<AudioInputProcessing> aInputProcessing) {
  class Message : public ControlMessage {
    const RefPtr<AudioInputTrack> mTrack;
    const RefPtr<AudioInputProcessing> mProcessing;

   public:
    Message(RefPtr<AudioInputTrack> aTrack,
            RefPtr<AudioInputProcessing> aProcessing)
        : ControlMessage(aTrack),
          mTrack(std::move(aTrack)),
          mProcessing(std::move(aProcessing)) {}
    void Run() override {
      TRACE("AudioInputTrack::SetInputProcessingImpl");
      mTrack->SetInputProcessingImpl(std::move(mProcessing));
    }
  };

  if (IsDestroyed()) {
    return;
  }
  GraphImpl()->AppendMessage(
      MakeUnique<Message>(std::move(this), std::move(aInputProcessing)));
}

AudioInputTrack* AudioInputTrack::Create(MediaTrackGraph* aGraph) {
  MOZ_ASSERT(NS_IsMainThread());
  AudioInputTrack* track = new AudioInputTrack(aGraph->GraphRate());
  aGraph->AddTrack(track);
  return track;
}

void AudioInputTrack::DestroyImpl() {
  ProcessedMediaTrack::DestroyImpl();
  if (mInputProcessing) {
    mInputProcessing->End();
  }
}

void AudioInputTrack::ProcessInput(GraphTime aFrom, GraphTime aTo,
                                   uint32_t aFlags) {
  TRACE_COMMENT("AudioInputTrack::ProcessInput", "AudioInputTrack %p", this);

  // Check if there is a connected NativeInputTrack
  NativeInputTrack* source = nullptr;
  if (!mInputs.IsEmpty()) {
    for (const MediaInputPort* input : mInputs) {
      MOZ_ASSERT(input->GetSource());
      if (input->GetSource()->AsNativeInputTrack()) {
        source = input->GetSource()->AsNativeInputTrack();
        break;
      }
    }
  }

  // Push the input data from the connected NativeInputTrack to mInputProcessing
  if (source) {
    MOZ_ASSERT(source->GraphImpl() == GraphImpl());
    MOZ_ASSERT(source->mSampleRate == mSampleRate);
    MOZ_ASSERT(GraphImpl()->GraphRate() == mSampleRate);
    mInputProcessing->ProcessInput(GraphImpl(),
                                   source->GetData<AudioSegment>());
  }

  bool ended = false;
  mInputProcessing->Pull(
      GraphImpl(), aFrom, aTo, TrackTimeToGraphTime(GetEnd()),
      GetData<AudioSegment>(), aTo == GraphImpl()->mStateComputedTime, &ended);
  ApplyTrackDisabling(mSegment.get());
  if (ended && (aFlags & ALLOW_END)) {
    mEnded = true;
  }
}

void AudioInputTrack::SetInputProcessingImpl(
    RefPtr<AudioInputProcessing> aInputProcessing) {
  MOZ_ASSERT(GraphImpl()->OnGraphThread());
  mInputProcessing = std::move(aInputProcessing);
}

nsresult AudioInputTrack::OpenAudioInput(CubebUtils::AudioDeviceID aId,
                                         AudioDataListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(GraphImpl());
  MOZ_ASSERT(!mInputListener);
  MOZ_ASSERT(mDeviceId.isNothing());
  mInputListener = aListener;
  ProcessedMediaTrack* input = GraphImpl()->GetDeviceTrack(aId);
  MOZ_ASSERT(input);
  LOG("Open device %p (InputTrack=%p) for Mic source %p", aId, input, this);
  mPort = AllocateInputPort(input);
  mDeviceId.emplace(aId);
  return GraphImpl()->OpenAudioInput(aId, aListener);
}

void AudioInputTrack::CloseAudioInput() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(GraphImpl());
  if (!mInputListener) {
    return;
  }
  MOZ_ASSERT(mPort);
  MOZ_ASSERT(mDeviceId.isSome());
  LOG("Close device %p (InputTrack=%p) for Mic source %p ", mDeviceId.value(),
      mPort->GetSource(), this);
  mPort->Destroy();
  GraphImpl()->CloseAudioInput(mDeviceId.extract(), mInputListener);
  mInputListener = nullptr;
}

Maybe<CubebUtils::AudioDeviceID> AudioInputTrack::DeviceId() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mDeviceId;
}

nsString MediaEngineWebRTCAudioCaptureSource::GetName() const {
  return u"AudioCapture"_ns;
}

nsCString MediaEngineWebRTCAudioCaptureSource::GetUUID() const {
  nsID uuid;
  char uuidBuffer[NSID_LENGTH];
  nsCString asciiString;
  ErrorResult rv;

  rv = nsContentUtils::GenerateUUIDInPlace(uuid);
  if (rv.Failed()) {
    return ""_ns;
  }

  uuid.ToProvidedString(uuidBuffer);
  asciiString.AssignASCII(uuidBuffer);

  // Remove {} and the null terminator
  return nsCString(Substring(asciiString, 1, NSID_LENGTH - 3));
}

nsString MediaEngineWebRTCAudioCaptureSource::GetGroupId() const {
  return u"AudioCaptureGroup"_ns;
}

void MediaEngineWebRTCAudioCaptureSource::SetTrack(
    const RefPtr<MediaTrack>& aTrack, const PrincipalHandle& aPrincipalHandle) {
  AssertIsOnOwningThread();
  // Nothing to do here. aTrack is a placeholder dummy and not exposed.
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
    const MediaEnginePrefs& aPrefs, const char** aOutBadConstraint) {
  return NS_OK;
}

void MediaEngineWebRTCAudioCaptureSource::GetSettings(
    dom::MediaTrackSettings& aOutSettings) const {
  aOutSettings.mAutoGainControl.Construct(false);
  aOutSettings.mEchoCancellation.Construct(false);
  aOutSettings.mNoiseSuppression.Construct(false);
  aOutSettings.mChannelCount.Construct(1);
}

}  // namespace mozilla
