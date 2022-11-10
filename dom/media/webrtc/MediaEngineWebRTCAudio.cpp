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
#include "nsIDUtils.h"
#include "transport/runnable_utils.h"
#include "Tracing.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Logging.h"

#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/include/audio_processing.h"

using namespace webrtc;

// These are restrictions from the webrtc.org code
#define MAX_CHANNELS 2
#define MONO 1
#define MAX_SAMPLING_FREQ 48000  // Hz - multiple of 100

namespace mozilla {

using dom::MediaSourceEnum;

extern LazyLogModule gMediaManagerLog;
#define LOG(...) MOZ_LOG(gMediaManagerLog, LogLevel::Debug, (__VA_ARGS__))
#define LOG_FRAME(...) \
  MOZ_LOG(gMediaManagerLog, LogLevel::Verbose, (__VA_ARGS__))
#define LOG_ERROR(...) MOZ_LOG(gMediaManagerLog, LogLevel::Error, (__VA_ARGS__))

/**
 * WebRTC Microphone MediaEngineSource.
 */

MediaEngineWebRTCMicrophoneSource::MediaEngineWebRTCMicrophoneSource(
    const MediaDevice* aMediaDevice)
    : mPrincipal(PRINCIPAL_HANDLE_NONE),
      mDeviceInfo(aMediaDevice->mAudioDeviceInfo),
      mDeviceMaxChannelCount(mDeviceInfo->MaxChannels()),
      mSettings(new nsMainThreadPtrHolder<
                media::Refcountable<dom::MediaTrackSettings>>(
          "MediaEngineWebRTCMicrophoneSource::mSettings",
          new media::Refcountable<dom::MediaTrackSettings>(),
          // Non-strict means it won't assert main thread for us.
          // It would be great if it did but we're already on the media thread.
          /* aStrict = */ false)) {
  MOZ_ASSERT(aMediaDevice->mMediaSource == MediaSourceEnum::Microphone);
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
  int32_t maxChannels = static_cast<int32_t>(mDeviceInfo->MaxChannels());

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

  // See https://bugs.chromium.org/p/webrtc/issues/detail?id=11539 for more
  // info.  Our pref defaults to false, and if this is truly as unhelpful
  // as the upstream bug claim, we could delete the pref that drive this:
  // media.getusermedia.residual_echo_enabled.  See Bug 1779498.
  // mAudioProcessingConfig.residual_echo_detector.enabled =
  //    aPrefs.mResidualEchoOn;

  RefPtr<MediaEngineWebRTCMicrophoneSource> that = this;
  CubebUtils::AudioDeviceID deviceID = mDeviceInfo->DeviceID();
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__, [this, that, deviceID, track = mTrack, prefs = aPrefs,
                 audioProcessingConfig = mAudioProcessingConfig] {
        mSettings->mEchoCancellation.Value() = prefs.mAecOn;
        mSettings->mAutoGainControl.Value() = prefs.mAgcOn;
        mSettings->mNoiseSuppression.Value() = prefs.mNoiseOn;
        mSettings->mChannelCount.Value() = prefs.mChannels;

        class Message : public ControlMessage {
          CubebUtils::AudioDeviceID mDeviceID;
          const RefPtr<AudioInputProcessing> mInputProcessing;
          const AudioProcessing::Config mAudioProcessingConfig;
          const bool mPassThrough;
          const uint32_t mRequestedInputChannelCount;

         public:
          Message(MediaTrack* aTrack, CubebUtils::AudioDeviceID aDeviceID,
                  AudioInputProcessing* aInputProcessing,
                  const AudioProcessing::Config& aAudioProcessingConfig,
                  bool aPassThrough, uint32_t aRequestedInputChannelCount)
              : ControlMessage(aTrack),
                mDeviceID(aDeviceID),
                mInputProcessing(aInputProcessing),
                mAudioProcessingConfig(aAudioProcessingConfig),
                mPassThrough(aPassThrough),
                mRequestedInputChannelCount(aRequestedInputChannelCount) {}

          void Run() override {
            mInputProcessing->ApplyConfig(mTrack->GraphImpl(),
                                          mAudioProcessingConfig);
            {
              TRACE("SetRequestedInputChannelCount");
              mInputProcessing->SetRequestedInputChannelCount(
                  mTrack->GraphImpl(), mDeviceID, mRequestedInputChannelCount);
            }
            {
              TRACE("SetPassThrough")
              mInputProcessing->SetPassThrough(mTrack->GraphImpl(),
                                               mPassThrough);
            }
          }
        };

        // The high-pass filter is not taken into account when activating the
        // pass through, since it's not controllable from content.
        bool passThrough = !(prefs.mAecOn || prefs.mAgcOn || prefs.mNoiseOn);

        if (track->IsDestroyed()) {
          return;
        }
        track->GraphImpl()->AppendMessage(MakeUnique<Message>(
            track, deviceID, mInputProcessing, audioProcessingConfig,
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
      NS_ConvertUTF16toUTF8(mDeviceInfo->Name()).get());
  return NS_OK;
}

void MediaEngineWebRTCMicrophoneSource::SetTrack(
    const RefPtr<MediaTrack>& aTrack, const PrincipalHandle& aPrincipal) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aTrack);
  MOZ_ASSERT(aTrack->AsAudioProcessingTrack());

  MOZ_ASSERT(!mTrack);
  MOZ_ASSERT(mPrincipal == PRINCIPAL_HANDLE_NONE);
  mTrack = aTrack->AsAudioProcessingTrack();
  mPrincipal = aPrincipal;

  mInputProcessing =
      MakeAndAddRef<AudioInputProcessing>(mDeviceMaxChannelCount);

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

  StartStopMessage(MediaTrack* aTrack, AudioInputProcessing* aInputProcessing,
                   StartStop aAction)
      : ControlMessage(aTrack),
        mInputProcessing(aInputProcessing),
        mAction(aAction) {}

  void Run() override {
    if (mAction == StartStopMessage::Start) {
      TRACE("InputProcessing::Start")
      mInputProcessing->Start(mTrack->GraphImpl());
    } else if (mAction == StartStopMessage::Stop) {
      TRACE("InputProcessing::Stop")
      mInputProcessing->Stop(mTrack->GraphImpl());
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

  ApplySettings(mCurrentPrefs);

  CubebUtils::AudioDeviceID deviceID = mDeviceInfo->DeviceID();
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__, [inputProcessing = mInputProcessing, deviceID, track = mTrack,
                 principal = mPrincipal] {
        if (track->IsDestroyed()) {
          return;
        }

        track->GraphImpl()->AppendMessage(MakeUnique<StartStopMessage>(
            track, inputProcessing, StartStopMessage::Start));
        track->ConnectDeviceInput(deviceID, inputProcessing.get(), principal);
      }));

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

        MOZ_ASSERT(track->DeviceId().value() == deviceInfo->DeviceID());
        track->DisconnectDeviceInput();
        track->GraphImpl()->AppendMessage(MakeUnique<StartStopMessage>(
            track, inputProcessing, StartStopMessage::Stop));
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

AudioInputProcessing::AudioInputProcessing(uint32_t aMaxChannelCount)
    : mAudioProcessing(AudioProcessingBuilder().Create().release()),
      mRequestedInputChannelCount(aMaxChannelCount),
      mSkipProcessing(false),
      mInputDownmixBuffer(MAX_SAMPLING_FREQ * MAX_CHANNELS / 100),
      mEnabled(false),
      mEnded(false),
      mPacketCount(0) {}

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

  if (aPassThrough == mSkipProcessing) {
    return;
  }
  mSkipProcessing = aPassThrough;

  if (!mEnabled) {
    MOZ_ASSERT(!mPacketizerInput);
    return;
  }

  if (aPassThrough) {
    // Turn on pass-through
    ResetAudioProcessing(aGraph);
  } else {
    // Turn off pass-through
    MOZ_ASSERT(!mPacketizerInput);
    EnsureAudioProcessing(aGraph, mRequestedInputChannelCount);
  }
}

uint32_t AudioInputProcessing::GetRequestedInputChannelCount() {
  return mRequestedInputChannelCount;
}

void AudioInputProcessing::SetRequestedInputChannelCount(
    MediaTrackGraphImpl* aGraph, CubebUtils::AudioDeviceID aDeviceId,
    uint32_t aRequestedInputChannelCount) {
  mRequestedInputChannelCount = aRequestedInputChannelCount;

  aGraph->ReevaluateInputDevice(aDeviceId);
}

void AudioInputProcessing::Start(MediaTrackGraphImpl* aGraph) {
  MOZ_ASSERT(aGraph->OnGraphThread());

  if (mEnabled) {
    return;
  }
  mEnabled = true;

  if (mSkipProcessing) {
    return;
  }

  MOZ_ASSERT(!mPacketizerInput);
  EnsureAudioProcessing(aGraph, mRequestedInputChannelCount);
}

void AudioInputProcessing::Stop(MediaTrackGraphImpl* aGraph) {
  MOZ_ASSERT(aGraph->OnGraphThread());

  if (!mEnabled) {
    return;
  }

  mEnabled = false;

  if (mSkipProcessing) {
    return;
  }

  // Packetizer is active and we were just stopped. Stop the packetizer and
  // processing.
  ResetAudioProcessing(aGraph);
}

// The following is how how Process() works in pass-through and non-pass-through
// mode. In both mode, Process() outputs the same amount of the frames as its
// input data.
//
// I. In non-pass-through mode:
//
// We will use webrtc::AudioProcessing to process the input audio data in this
// mode. The data input in webrtc::AudioProcessing needs to be a 10ms chunk,
// while the input data passed to Process() is not necessary to have times of
// 10ms-chunk length. To divide the input data into 10ms chunks,
// mPacketizerInput is introduced.
//
// We will add one 10ms-chunk silence into the internal buffer before Process()
// works. Those extra frames is called pre-buffering. It aims to avoid glitches
// we may have when producing data in mPacketizerInput. Without pre-buffering,
// when the input data length is not 10ms-times, we could end up having no
// enough output needs since mPacketizerInput would keep some input data, which
// is the remainder of the 10ms-chunk length. To force processing those data
// left in mPacketizerInput, we would need to add some extra frames to make
// mPacketizerInput produce a 10ms-chunk. For example, if the sample rate is
// 44100 Hz, then the packet-size is 441 frames. When we only have 384 input
// frames, we would need to put additional 57 frames to mPacketizerInput to
// produce a packet. However, those extra 57 frames result in a glitch sound.
//
// By adding one 10ms-chunk silence in advance to the internal buffer, we won't
// need to add extra frames between the input data no matter what data length it
// is. The only drawback is the input data won't be processed and send to output
// immediately. Process() will consume pre-buffering data for its output first.
// The below describes how it works:
//
//
//                          Process()
//               +-----------------------------+
//   input D(N)  |   +--------+   +--------+   |  output D(N)
// --------------|-->|  P(N)  |-->|  S(N)  |---|-------------->
//               |   +--------+   +--------+   |
//               |   packetizer    mSegment    |
//               +-----------------------------+
//               <------ internal buffer ------>
//
//
//   D(N): number of frames from the input and the output needs in the N round
//      Z: number of frames of a 10ms chunk(packet) in mPacketizerInput, Z >= 1
//         (if Z = 1, packetizer has no effect)
//   P(N): number of frames left in mPacketizerInput after the N round. Once the
//         frames in packetizer >= Z, packetizer will produce a packet to
//         mSegment, so P(N) = (P(N-1) + D(N)) % Z, 0 <= P(N) <= Z-1
//   S(N): number of frames left in mSegment after the N round. The input D(N)
//         frames will be passed to mPacketizerInput first, and then
//         mPacketizerInput may append some packets to mSegment, so
//         S(N) = S(N-1) + Z * floor((P(N-1) + D(N)) / Z) - D(N)
//
// At the first, we set P(0) = 0, S(0) = X, where X >= Z-1. X is the
// pre-buffering put in the internal buffer. With this settings, P(K) + S(K) = X
// always holds.
//
// Intuitively, this seems true: We put X frames in the internal buffer at
// first. If the data won't be blocked in packetizer, after the Process(), the
// internal buffer should still hold X frames since the number of frames coming
// from input is the same as the output needs. The key of having enough data for
// output needs, while the input data is piled up in packetizer, is by putting
// at least Z-1 frames as pre-buffering, since the maximum number of frames
// stuck in the packetizer before it can emit a packet is packet-size - 1.
// Otherwise, we don't have enough data for output if the new input data plus
// the data left in packetizer produces a smaller-than-10ms chunk, which will be
// left in packetizer. Thus we must have some pre-buffering frames in the
// mSegment to make up the length of the left chunk we need for output. This can
// also be told by by induction:
//   (1) This holds when K = 0
//   (2) Assume this holds when K = N: so P(N) + S(N) = X
//       => P(N) + S(N) = X >= Z-1 => S(N) >= Z-1-P(N)
//   (3) When K = N+1, we have D(N+1) input frames comes
//     a. if P(N) + D(N+1) < Z, then packetizer has no enough data for one
//        packet. No data produced by packertizer, so the mSegment now has
//        S(N) >= Z-1-P(N) frames. Output needs D(N+1) < Z-P(N) frames. So it
//        needs at most Z-P(N)-1 frames, and mSegment has enough frames for
//        output, Then, P(N+1) = P(N) + D(N+1) and S(N+1) = S(N) - D(N+1)
//        => P(N+1) + S(N+1) = P(N) + S(N) = X
//     b. if P(N) + D(N+1) = Z, then packetizer will produce one packet for
//        mSegment, so mSegment now has S(N) + Z frames. Output needs D(N+1)
//        = Z-P(N) frames. S(N) has at least Z-1-P(N)+Z >= Z-P(N) frames, since
//        Z >= 1. So mSegment has enough frames for output. Then, P(N+1) = 0 and
//        S(N+1) = S(N) + Z - D(N+1) = S(N) + P(N)
//        => P(N+1) + S(N+1) = P(N) + S(N) = X
//     c. if P(N) + D(N+1) > Z, and let P(N) + D(N+1) = q * Z + r, where q >= 1
//        and 0 <= r <= Z-1, then packetizer will produce can produce q packets
//        for mSegment. Output needs D(N+1) = q * Z - P(N) + r frames and
//        mSegment has S(N) + q * z >= q * z - P(N) + Z-1 >= q*z -P(N) + r,
//        since r <= Z-1. So mSegment has enough frames for output. Then,
//        P(N+1) = r and S(N+1) = S(N) + q * Z - D(N+1)
//         => P(N+1) + S(N+1) = S(N) + (q * Z + r - D(N+1)) =  S(N) + P(N) = X
//   => P(K) + S(K) = X always holds
//
// Since P(K) + S(K) = X and P(K) is in [0, Z-1], the S(K) is in [X-Z+1, X]
// range. In our implementation, X is set to Z so S(K) is in [1, Z].
// By the above workflow, we always have enough data for output and no extra
// frames put into packetizer. It means we don't have any glitch!
//
// II. In pass-through mode:
//
//                Process()
//               +--------+
//   input D(N)  |        |  output D(N)
// -------------->-------->--------------->
//               |        |
//               +--------+
//
// The D(N) frames of data are just forwarded from input to output without any
// processing
void AudioInputProcessing::Process(MediaTrackGraphImpl* aGraph, GraphTime aFrom,
                                   GraphTime aTo, AudioSegment* aInput,
                                   AudioSegment* aOutput) {
  MOZ_ASSERT(aGraph->OnGraphThread());
  MOZ_ASSERT(aFrom <= aTo);
  MOZ_ASSERT(!mEnded);

  TrackTime need = aTo - aFrom;
  if (need == 0) {
    return;
  }

  if (!mEnabled) {
    LOG_FRAME("(Graph %p, Driver %p) AudioInputProcessing %p Filling %" PRId64
              " frames of silence to output (disabled)",
              aGraph, aGraph->CurrentDriver(), this, need);
    aOutput->AppendNullData(need);
    return;
  }

  MOZ_ASSERT(aInput->GetDuration() == need,
             "Wrong data length from input port source");

  if (PassThrough(aGraph)) {
    LOG_FRAME(
        "(Graph %p, Driver %p) AudioInputProcessing %p Forwarding %" PRId64
        " frames of input data to output directly (PassThrough)",
        aGraph, aGraph->CurrentDriver(), this, aInput->GetDuration());
    aOutput->AppendSegment(aInput);
    return;
  }

  // SetPassThrough(false) must be called before reaching here.
  MOZ_ASSERT(mPacketizerInput);
  // If mRequestedInputChannelCount is updated, create a new packetizer. No
  // need to change the pre-buffering since the rate is always the same. The
  // frames left in the packetizer would be replaced by null data and then
  // transferred to mSegment.
  EnsureAudioProcessing(aGraph, mRequestedInputChannelCount);

  // Preconditions of the audio-processing logic.
  MOZ_ASSERT(static_cast<uint32_t>(mSegment.GetDuration()) +
                 mPacketizerInput->FramesAvailable() ==
             mPacketizerInput->mPacketSize);
  // We pre-buffer mPacketSize frames, but the maximum number of frames stuck in
  // the packetizer before it can emit a packet is mPacketSize-1. Thus that
  // remaining 1 frame will always be present in mSegment.
  MOZ_ASSERT(mSegment.GetDuration() >= 1);
  MOZ_ASSERT(mSegment.GetDuration() <= mPacketizerInput->mPacketSize);

  PacketizeAndProcess(aGraph, *aInput);
  LOG_FRAME("(Graph %p, Driver %p) AudioInputProcessing %p Buffer has %" PRId64
            " frames of data now, after packetizing and processing",
            aGraph, aGraph->CurrentDriver(), this, mSegment.GetDuration());

  // By setting pre-buffering to the number of frames of one packet, and
  // because the maximum number of frames stuck in the packetizer before
  // it can emit a packet is the mPacketSize-1, we always have at least
  // one more frame than output needs.
  MOZ_ASSERT(mSegment.GetDuration() > need);
  aOutput->AppendSlice(mSegment, 0, need);
  mSegment.RemoveLeading(need);
  LOG_FRAME("(Graph %p, Driver %p) AudioInputProcessing %p moving %" PRId64
            " frames of data to output, leaving %" PRId64 " frames in buffer",
            aGraph, aGraph->CurrentDriver(), this, need,
            mSegment.GetDuration());

  // Postconditions of the audio-processing logic.
  MOZ_ASSERT(static_cast<uint32_t>(mSegment.GetDuration()) +
                 mPacketizerInput->FramesAvailable() ==
             mPacketizerInput->mPacketSize);
  MOZ_ASSERT(mSegment.GetDuration() >= 1);
  MOZ_ASSERT(mSegment.GetDuration() <= mPacketizerInput->mPacketSize);
}

void AudioInputProcessing::ProcessOutputData(MediaTrackGraphImpl* aGraph,
                                             AudioDataValue* aBuffer,
                                             size_t aFrames, TrackRate aRate,
                                             uint32_t aChannels) {
  MOZ_ASSERT(aGraph->OnGraphThread());

  if (!mEnabled || PassThrough(aGraph)) {
    return;
  }

  if (!mPacketizerOutput ||
      mPacketizerOutput->mPacketSize != GetPacketSize(aRate) ||
      mPacketizerOutput->mChannels != aChannels) {
    // It's ok to drop the audio still in the packetizer here: if this changes,
    // we changed devices or something.
    mPacketizerOutput = Nothing();
    mPacketizerOutput.emplace(GetPacketSize(aRate), aChannels);
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
    StreamConfig inputConfig(aRate, channelCountFarend);
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
                                               const AudioSegment& aSegment) {
  MOZ_ASSERT(!PassThrough(aGraph),
             "This should be bypassed when in PassThrough mode.");
  MOZ_ASSERT(mEnabled);
  MOZ_ASSERT(mPacketizerInput);
  MOZ_ASSERT(mPacketizerInput->mPacketSize ==
             GetPacketSize(aGraph->GraphRate()));

  // Calculate number of the pending frames in mChunksInPacketizer.
  auto pendingFrames = [&]() {
    TrackTime frames = 0;
    for (const auto& p : mChunksInPacketizer) {
      frames += p.first;
    }
    return frames;
  };

  // Precondition of the Principal-labelling logic below.
  MOZ_ASSERT(mPacketizerInput->FramesAvailable() ==
             static_cast<uint32_t>(pendingFrames()));

  // The WriteToInterleavedBuffer will do upmix or downmix if the channel-count
  // in aSegment's chunks is different from mPacketizerInput->mChannels
  // WriteToInterleavedBuffer could be avoided once Bug 1729041 is done.
  size_t sampleCount = aSegment.WriteToInterleavedBuffer(
      mInterleavedBuffer, mPacketizerInput->mChannels);
  size_t frameCount =
      sampleCount / static_cast<size_t>(mPacketizerInput->mChannels);

  // Packetize our input data into 10ms chunks, deinterleave into planar channel
  // buffers, process, and append to the right MediaStreamTrack.
  mPacketizerInput->Input(mInterleavedBuffer.Elements(),
                          static_cast<uint32_t>(frameCount));

  // Update mChunksInPacketizer and make sure the precondition for the
  // Principal-labelling logic still holds.
  for (AudioSegment::ConstChunkIterator iter(aSegment); !iter.IsEnded();
       iter.Next()) {
    MOZ_ASSERT(iter->mDuration > 0);
    mChunksInPacketizer.emplace_back(
        std::make_pair(iter->mDuration, iter->mPrincipalHandle));
  }
  MOZ_ASSERT(mPacketizerInput->FramesAvailable() ==
             static_cast<uint32_t>(pendingFrames()));

  LOG_FRAME(
      "(Graph %p, Driver %p) AudioInputProcessing %p Packetizing %zu frames. "
      "Packetizer has %u frames (enough for %u packets) now",
      aGraph, aGraph->CurrentDriver(), this, frameCount,
      mPacketizerInput->FramesAvailable(),
      mPacketizerInput->PacketsAvailable());

  size_t offset = 0;

  while (mPacketizerInput->PacketsAvailable()) {
    mPacketCount++;
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

    // Downmix from mPacketizerInput->mChannels to mono if needed. We always
    // have floats here, the packetizer performed the conversion. This handles
    // sound cards with multiple physical jacks exposed as a single device with
    // _n_ discrete channels, where only a single mic is plugged in. Those
    // channels are not correlated temporaly since they are discrete channels,
    // mixing is just a sum.
    AutoTArray<float*, 8> deinterleavedPacketizedInputDataChannelPointers;
    uint32_t channelCountInput = 0;
    if (mPacketizerInput->mChannels > MAX_CHANNELS) {
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
        for (size_t j = 0; j < mPacketizerInput->mChannels; j++) {
          mDeinterleavedBuffer.Data()[i] += packet[readIndex++];
        }
      }
    } else {
      channelCountInput = mPacketizerInput->mChannels;
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

    StreamConfig inputConfig(aGraph->GraphRate(), channelCountInput);
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

    // If logging is enabled, dump the audio processing stats twice a second
    if (MOZ_LOG_TEST(gMediaManagerLog, LogLevel::Debug) &&
        !(mPacketCount % 50)) {
      AudioProcessingStats stats = mAudioProcessing->GetStatistics();
      char msg[1024];
      size_t offset = 0;
#define AddIfValue(format, member)                                       \
  if (stats.member.has_value()) {                                        \
    offset += SprintfBuf(msg + offset, sizeof(msg) - offset,             \
                         #member ":" format ", ", stats.member.value()); \
  }
      AddIfValue("%d", voice_detected);
      AddIfValue("%lf", echo_return_loss);
      AddIfValue("%lf", echo_return_loss_enhancement);
      AddIfValue("%lf", divergent_filter_fraction);
      AddIfValue("%d", delay_median_ms);
      AddIfValue("%d", delay_standard_deviation_ms);
      AddIfValue("%lf", residual_echo_likelihood);
      AddIfValue("%lf", residual_echo_likelihood_recent_max);
      AddIfValue("%d", delay_ms);
#undef AddIfValue
      LOG("AudioProcessing statistics: %s", msg);
    }

    if (mEnded) {
      continue;
    }

    // We already have planar audio data of the right format. Insert into the
    // MTG.
    MOZ_ASSERT(processedOutputChannelPointers.Length() == channelCountInput);

    // Insert the processed data chunk by chunk to mSegment with the paired
    // PrincipalHandle value. The chunks are tracked in mChunksInPacketizer.

    auto getAudioChunk = [&](TrackTime aStart, TrackTime aEnd,
                             const PrincipalHandle& aPrincipalHandle) {
      if (aStart == aEnd) {
        return AudioChunk();
      }
      RefPtr<SharedBuffer> other = buffer;
      AudioChunk c =
          AudioChunk(other.forget(), processedOutputChannelPointersConst,
                     static_cast<TrackTime>(mPacketizerInput->mPacketSize),
                     aPrincipalHandle);
      c.SliceTo(aStart, aEnd);
      return c;
    };

    // The number of frames of data that needs to be labelled with Principal
    // values.
    TrackTime len = static_cast<TrackTime>(mPacketizerInput->mPacketSize);
    // The start offset of the unlabelled chunk.
    TrackTime start = 0;
    // By mChunksInPacketizer's information, we can keep labelling the
    // unlabelled frames chunk by chunk.
    while (!mChunksInPacketizer.empty()) {
      auto& [frames, principal] = mChunksInPacketizer.front();
      const TrackTime end = start + frames;
      if (end > len) {
        // If the left unlabelled frames are part of this chunk, then we need to
        // adjust the number of frames in the chunk.
        if (len > start) {
          mSegment.AppendAndConsumeChunk(getAudioChunk(start, len, principal));
          frames -= len - start;
        }
        break;
      }
      // Otherwise, the number of unlabelled frames is larger than or equal to
      // this chunk. We can label the whole chunk directly.
      mSegment.AppendAndConsumeChunk(getAudioChunk(start, end, principal));
      start = end;
      mChunksInPacketizer.pop_front();
    }

    LOG_FRAME(
        "(Graph %p, Driver %p) AudioInputProcessing %p Appending %u frames of "
        "packetized audio, leaving %u frames in packetizer (%" PRId64
        " frames in mChunksInPacketizer)",
        aGraph, aGraph->CurrentDriver(), this, mPacketizerInput->mPacketSize,
        mPacketizerInput->FramesAvailable(), pendingFrames());

    // Postcondition of the Principal-labelling logic.
    MOZ_ASSERT(mPacketizerInput->FramesAvailable() ==
               static_cast<uint32_t>(pendingFrames()));
  }
}

void AudioInputProcessing::DeviceChanged(MediaTrackGraphImpl* aGraph) {
  MOZ_ASSERT(aGraph->OnGraphThread());

  // Reset some processing
  mAudioProcessing->Initialize();
  LOG_FRAME(
      "(Graph %p, Driver %p) AudioInputProcessing %p Reinitializing audio "
      "processing",
      aGraph, aGraph->CurrentDriver(), this);
}

void AudioInputProcessing::ApplyConfig(MediaTrackGraphImpl* aGraph,
                                       const AudioProcessing::Config& aConfig) {
  MOZ_ASSERT(aGraph->OnGraphThread());
  mAudioProcessing->ApplyConfig(aConfig);
}

void AudioInputProcessing::End() {
  mEnded = true;
  mSegment.Clear();
}

TrackTime AudioInputProcessing::NumBufferedFrames(
    MediaTrackGraphImpl* aGraph) const {
  MOZ_ASSERT(aGraph->OnGraphThread());
  return mSegment.GetDuration();
}

void AudioInputProcessing::EnsureAudioProcessing(MediaTrackGraphImpl* aGraph,
                                                 uint32_t aChannels) {
  MOZ_ASSERT(aGraph->OnGraphThread());
  MOZ_ASSERT(aChannels > 0);
  MOZ_ASSERT(mEnabled);
  MOZ_ASSERT(!mSkipProcessing);

  if (mPacketizerInput && mPacketizerInput->mChannels == aChannels) {
    return;
  }

  // If mPacketizerInput exists but with different channel-count, there is no
  // need to change pre-buffering since the packet size is the same as the old
  // one, since the rate is a constant.
  MOZ_ASSERT_IF(mPacketizerInput, mPacketizerInput->mPacketSize ==
                                      GetPacketSize(aGraph->GraphRate()));
  bool needPreBuffering = !mPacketizerInput;
  if (mPacketizerInput) {
    const TrackTime numBufferedFrames =
        static_cast<TrackTime>(mPacketizerInput->FramesAvailable());
    mSegment.AppendNullData(numBufferedFrames);
    mPacketizerInput = Nothing();
    mChunksInPacketizer.clear();
  }

  mPacketizerInput.emplace(GetPacketSize(aGraph->GraphRate()), aChannels);

  if (needPreBuffering) {
    LOG_FRAME(
        "(Graph %p, Driver %p) AudioInputProcessing %p: Adding %u frames of "
        "silence as pre-buffering",
        aGraph, aGraph->CurrentDriver(), this, mPacketizerInput->mPacketSize);

    AudioSegment buffering;
    buffering.AppendNullData(
        static_cast<TrackTime>(mPacketizerInput->mPacketSize));
    PacketizeAndProcess(aGraph, buffering);
  }
}

void AudioInputProcessing::ResetAudioProcessing(MediaTrackGraphImpl* aGraph) {
  MOZ_ASSERT(aGraph->OnGraphThread());
  MOZ_ASSERT(mSkipProcessing || !mEnabled);
  MOZ_ASSERT(mPacketizerInput);

  LOG_FRAME(
      "(Graph %p, Driver %p) AudioInputProcessing %p Resetting audio "
      "processing",
      aGraph, aGraph->CurrentDriver(), this);

  // Reset AudioProcessing so that if we resume processing in the future it
  // doesn't depend on old state.
  mAudioProcessing->Initialize();

  MOZ_ASSERT(static_cast<uint32_t>(mSegment.GetDuration()) +
                 mPacketizerInput->FramesAvailable() ==
             mPacketizerInput->mPacketSize);

  // It's ok to clear all the internal buffer here since we won't use mSegment
  // in pass-through mode or when audio processing is disabled.
  LOG_FRAME(
      "(Graph %p, Driver %p) AudioInputProcessing %p Emptying out %" PRId64
      " frames of data",
      aGraph, aGraph->CurrentDriver(), this, mSegment.GetDuration());
  mSegment.Clear();

  mPacketizerInput = Nothing();
  mChunksInPacketizer.clear();
}

void AudioProcessingTrack::Destroy() {
  MOZ_ASSERT(NS_IsMainThread());
  DisconnectDeviceInput();

  MediaTrack::Destroy();
}

void AudioProcessingTrack::SetInputProcessing(
    RefPtr<AudioInputProcessing> aInputProcessing) {
  class Message : public ControlMessage {
    const RefPtr<AudioProcessingTrack> mTrack;
    const RefPtr<AudioInputProcessing> mProcessing;

   public:
    Message(RefPtr<AudioProcessingTrack> aTrack,
            RefPtr<AudioInputProcessing> aProcessing)
        : ControlMessage(aTrack),
          mTrack(std::move(aTrack)),
          mProcessing(std::move(aProcessing)) {}
    void Run() override {
      TRACE("AudioProcessingTrack::SetInputProcessingImpl");
      mTrack->SetInputProcessingImpl(mProcessing);
    }
  };

  if (IsDestroyed()) {
    return;
  }
  GraphImpl()->AppendMessage(
      MakeUnique<Message>(std::move(this), std::move(aInputProcessing)));
}

AudioProcessingTrack* AudioProcessingTrack::Create(MediaTrackGraph* aGraph) {
  MOZ_ASSERT(NS_IsMainThread());
  AudioProcessingTrack* track = new AudioProcessingTrack(aGraph->GraphRate());
  aGraph->AddTrack(track);
  return track;
}

void AudioProcessingTrack::DestroyImpl() {
  ProcessedMediaTrack::DestroyImpl();
  if (mInputProcessing) {
    mInputProcessing->End();
  }
}

void AudioProcessingTrack::ProcessInput(GraphTime aFrom, GraphTime aTo,
                                        uint32_t aFlags) {
  TRACE_COMMENT("AudioProcessingTrack::ProcessInput", "AudioProcessingTrack %p",
                this);
  MOZ_ASSERT(mInputProcessing);

  LOG_FRAME(
      "(Graph %p, Driver %p) AudioProcessingTrack %p ProcessInput from %" PRId64
      " to %" PRId64 ", needs %" PRId64 " frames",
      mGraph, mGraph->CurrentDriver(), this, aFrom, aTo, aTo - aFrom);

  if (aFrom >= aTo) {
    return;
  }

  if (!mInputProcessing->IsEnded()) {
    MOZ_ASSERT(TrackTimeToGraphTime(GetEnd()) == aFrom);
    if (mInputs.IsEmpty()) {
      GetData<AudioSegment>()->AppendNullData(aTo - aFrom);
      LOG_FRAME("(Graph %p, Driver %p) AudioProcessingTrack %p Filling %" PRId64
                " frames of null data (no input source)",
                mGraph, mGraph->CurrentDriver(), this, aTo - aFrom);
    } else {
      MOZ_ASSERT(mInputs.Length() == 1);
      AudioSegment data;
      DeviceInputConsumerTrack::GetInputSourceData(data, mInputs[0], aFrom,
                                                   aTo);
      mInputProcessing->Process(GraphImpl(), aFrom, aTo, &data,
                                GetData<AudioSegment>());
    }
    MOZ_ASSERT(TrackTimeToGraphTime(GetEnd()) == aTo);

    ApplyTrackDisabling(mSegment.get());
  } else if (aFlags & ALLOW_END) {
    mEnded = true;
  }
}

void AudioProcessingTrack::NotifyOutputData(MediaTrackGraphImpl* aGraph,
                                            AudioDataValue* aBuffer,
                                            size_t aFrames, TrackRate aRate,
                                            uint32_t aChannels) {
  MOZ_ASSERT(mGraph == aGraph, "Cannot feed audio output to another graph");
  MOZ_ASSERT(mGraph->OnGraphThread());
  if (mInputProcessing) {
    mInputProcessing->ProcessOutputData(aGraph, aBuffer, aFrames, aRate,
                                        aChannels);
  }
}

void AudioProcessingTrack::SetInputProcessingImpl(
    RefPtr<AudioInputProcessing> aInputProcessing) {
  MOZ_ASSERT(GraphImpl()->OnGraphThread());
  mInputProcessing = std::move(aInputProcessing);
}

MediaEngineWebRTCAudioCaptureSource::MediaEngineWebRTCAudioCaptureSource(
    const MediaDevice* aMediaDevice) {
  MOZ_ASSERT(aMediaDevice->mMediaSource == MediaSourceEnum::AudioCapture);
}

/* static */
nsString MediaEngineWebRTCAudioCaptureSource::GetUUID() {
  nsID uuid{};
  char uuidBuffer[NSID_LENGTH];
  nsCString asciiString;
  ErrorResult rv;

  rv = nsID::GenerateUUIDInPlace(uuid);
  if (rv.Failed()) {
    return u""_ns;
  }

  uuid.ToProvidedString(uuidBuffer);
  asciiString.AssignASCII(uuidBuffer);

  // Remove {} and the null terminator
  return NS_ConvertASCIItoUTF16(Substring(asciiString, 1, NSID_LENGTH - 3));
}

/* static */
nsString MediaEngineWebRTCAudioCaptureSource::GetGroupId() {
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

// Don't allow our macros to leak into other cpps in our unified build unit.
#undef MAX_CHANNELS
#undef MONO
#undef MAX_SAMPLING_FREQ
