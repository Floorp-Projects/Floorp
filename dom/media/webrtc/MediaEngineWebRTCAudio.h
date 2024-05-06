/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaEngineWebRTCAudio_h
#define MediaEngineWebRTCAudio_h

#include "AudioPacketizer.h"
#include "AudioSegment.h"
#include "AudioDeviceInfo.h"
#include "DeviceInputTrack.h"
#include "MediaEngineWebRTC.h"
#include "MediaEnginePrefs.h"
#include "MediaTrackListener.h"
#include "modules/audio_processing/include/audio_processing.h"

namespace mozilla {

class AudioInputProcessing;
class AudioProcessingTrack;

// This class is created and used exclusively on the Media Manager thread, with
// exactly two exceptions:
// - Pull is always called on the MTG thread. It only ever uses
//   mInputProcessing. mInputProcessing is set, then a message is sent first to
//   the main thread and then the MTG thread so that it can be used as part of
//   the graph processing. On destruction, similarly, a message is sent to the
//   graph so that it stops using it, and then it is deleted.
// - mSettings is created on the MediaManager thread is always ever accessed on
//   the Main Thread. It is const.
class MediaEngineWebRTCMicrophoneSource : public MediaEngineSource {
 public:
  explicit MediaEngineWebRTCMicrophoneSource(const MediaDevice* aMediaDevice);

  nsresult Allocate(const dom::MediaTrackConstraints& aConstraints,
                    const MediaEnginePrefs& aPrefs, uint64_t aWindowID,
                    const char** aOutBadConstraint) override;
  nsresult Deallocate() override;
  void SetTrack(const RefPtr<MediaTrack>& aTrack,
                const PrincipalHandle& aPrincipal) override;
  nsresult Start() override;
  nsresult Stop() override;
  nsresult Reconfigure(const dom::MediaTrackConstraints& aConstraints,
                       const MediaEnginePrefs& aPrefs,
                       const char** aOutBadConstraint) override;

  /**
   * Assigns the current settings of the capture to aOutSettings.
   * Main thread only.
   */
  void GetSettings(dom::MediaTrackSettings& aOutSettings) const override;

  nsresult TakePhoto(MediaEnginePhotoCallback* aCallback) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

 protected:
  ~MediaEngineWebRTCMicrophoneSource() = default;

 private:
  /**
   * From a set of constraints and about:config preferences, output the correct
   * set of preferences that can be sent to AudioInputProcessing.
   *
   * This can fail if the number of channels requested is zero, negative, or
   * more than the device supports.
   */
  nsresult EvaluateSettings(const NormalizedConstraints& aConstraintsUpdate,
                            const MediaEnginePrefs& aInPrefs,
                            MediaEnginePrefs* aOutPrefs,
                            const char** aOutBadConstraint);
  /**
   * From settings output by EvaluateSettings, send those settings to the
   * AudioInputProcessing instance and the main thread (for use in GetSettings).
   */
  void ApplySettings(const MediaEnginePrefs& aPrefs);

  PrincipalHandle mPrincipal = PRINCIPAL_HANDLE_NONE;

  const RefPtr<AudioDeviceInfo> mDeviceInfo;

  // The maximum number of channels that this device supports.
  const uint32_t mDeviceMaxChannelCount;
  // The current settings for the underlying device.
  // Constructed on the MediaManager thread, and then only ever accessed on the
  // main thread.
  const nsMainThreadPtrHandle<media::Refcountable<dom::MediaTrackSettings>>
      mSettings;

  // Current state of the resource for this source.
  MediaEngineSourceState mState;

  // The current preferences that will be forwarded to mInputProcessing below.
  MediaEnginePrefs mCurrentPrefs;

  // The AudioProcessingTrack used to inteface with the MediaTrackGraph. Set in
  // SetTrack as part of the initialization, and nulled in ::Deallocate.
  RefPtr<AudioProcessingTrack> mTrack;

  // See note at the top of this class.
  RefPtr<AudioInputProcessing> mInputProcessing;
};

// This class is created on the MediaManager thread, and then exclusively used
// on the MTG thread.
// All communication is done via message passing using MTG ControlMessages
class AudioInputProcessing : public AudioDataListener {
 public:
  explicit AudioInputProcessing(uint32_t aMaxChannelCount);
  void Process(AudioProcessingTrack* aTrack, GraphTime aFrom, GraphTime aTo,
               AudioSegment* aInput, AudioSegment* aOutput);

  void ProcessOutputData(AudioProcessingTrack* aTrack,
                         const AudioChunk& aChunk);
  bool IsVoiceInput(MediaTrackGraph* aGraph) const override {
    // If we're passing data directly without AEC or any other process, this
    // means that all voice-processing has been disabled intentionaly. In this
    // case, consider that the device is not used for voice input.
    return !IsPassThrough(aGraph) ||
           mPlatformProcessingSetParams != CUBEB_INPUT_PROCESSING_PARAM_NONE;
  }

  void Start(MediaTrackGraph* aGraph);
  void Stop(MediaTrackGraph* aGraph);

  void DeviceChanged(MediaTrackGraph* aGraph) override;

  uint32_t RequestedInputChannelCount(MediaTrackGraph*) const override {
    return GetRequestedInputChannelCount();
  }

  cubeb_input_processing_params RequestedInputProcessingParams(
      MediaTrackGraph* aGraph) const override;

  void Disconnect(MediaTrackGraph* aGraph) override;

  void NotifySetRequestedInputProcessingParamsResult(
      MediaTrackGraph* aGraph, cubeb_input_processing_params aRequestedParams,
      const Result<cubeb_input_processing_params, int>& aResult) override;

  void PacketizeAndProcess(AudioProcessingTrack* aTrack,
                           const AudioSegment& aSegment);

  uint32_t GetRequestedInputChannelCount() const;

  // This is true when all processing is disabled, in which case we can skip
  // packetization, resampling and other processing passes. Processing may still
  // be applied by the platform on the underlying input track.
  bool IsPassThrough(MediaTrackGraph* aGraph) const;

  // This allow changing the APM options, enabling or disabling processing
  // steps. The settings get applied the next time we're about to process input
  // data.
  void ApplySettings(MediaTrackGraph* aGraph,
                     CubebUtils::AudioDeviceID aDeviceID,
                     const MediaEnginePrefs& aSettings);

  // The config currently applied to the audio processing module.
  webrtc::AudioProcessing::Config AppliedConfig(MediaTrackGraph* aGraph) const;

  void End();

  TrackTime NumBufferedFrames(MediaTrackGraph* aGraph) const;

  // The packet size contains samples in 10ms. The unit of aRate is hz.
  static uint32_t GetPacketSize(TrackRate aRate) {
    return webrtc::AudioProcessing::GetFrameSize(aRate);
  }

  bool IsEnded() const { return mEnded; }

  // For testing:
  bool HadAECAndDrift() const { return mHadAECAndDrift; }

 private:
  ~AudioInputProcessing() = default;
  webrtc::AudioProcessing::Config ConfigForPrefs(
      const MediaEnginePrefs& aPrefs) const;
  void PassThroughChanged(MediaTrackGraph* aGraph);
  void RequestedInputChannelCountChanged(MediaTrackGraph* aGraph,
                                         CubebUtils::AudioDeviceID aDeviceId);
  void EnsurePacketizer(AudioProcessingTrack* aTrack);
  void EnsureAudioProcessing(AudioProcessingTrack* aTrack);
  void ResetAudioProcessing(MediaTrackGraph* aGraph);
  void ApplySettingsInternal(MediaTrackGraph* aGraph,
                             const MediaEnginePrefs& aSettings);
  PrincipalHandle GetCheckedPrincipal(const AudioSegment& aSegment);
  // This implements the processing algoritm to apply to the input (e.g. a
  // microphone). If all algorithms are disabled, this class in not used. This
  // class only accepts audio chunks of 10ms. It has two inputs and one output:
  // it is fed the speaker data and the microphone data. It outputs processed
  // input data.
  UniquePtr<webrtc::AudioProcessing> mAudioProcessing;
  // Whether mAudioProcessing was created for AEC with clock drift.
  // Meaningful only when mAudioProcessing is non-null;
  bool mHadAECAndDrift = false;
  // Packetizer to be able to feed 10ms packets to the input side of
  // mAudioProcessing. Not used if the processing is bypassed.
  Maybe<AudioPacketizer<AudioDataValue, float>> mPacketizerInput;
  // The current settings from about:config preferences and content-provided
  // constraints.
  MediaEnginePrefs mSettings;
  // When false, RequestedInputProcessingParams() returns no params, resulting
  // in platform processing getting disabled in the platform.
  bool mPlatformProcessingEnabled = false;
  // The latest error notified to us through
  // NotifySetRequestedInputProcessingParamsResult, or Nothing if the latest
  // request was successful, or if a request is pending a result.
  Maybe<int> mPlatformProcessingSetError;
  // The processing params currently applied in the platform. This allows
  // adapting the AudioProcessingConfig accordingly.
  cubeb_input_processing_params mPlatformProcessingSetParams =
      CUBEB_INPUT_PROCESSING_PARAM_NONE;
  // Buffer for up to one 10ms packet of planar mixed audio output for the
  // reverse-stream (speaker data) of mAudioProcessing AEC.
  // Length is packet size * channel count, regardless of how many frames are
  // buffered.  Not used if the processing is bypassed.
  AlignedFloatBuffer mOutputBuffer;
  // Number of channels into which mOutputBuffer is divided.
  uint32_t mOutputBufferChannelCount = 0;
  // Number of frames buffered in mOutputBuffer for the reverse stream.
  uint32_t mOutputBufferFrameCount = 0;
  // Stores the input audio, to be processed by the APM.
  AlignedFloatBuffer mInputBuffer;
  // Stores the deinterleaved microphone audio
  AlignedFloatBuffer mDeinterleavedBuffer;
  // Stores the mixed down input audio
  AlignedFloatBuffer mInputDownmixBuffer;
  // Stores data waiting to be pulled.
  AudioSegment mSegment;
  // Whether or not this MediaEngine is enabled. If it's not enabled, it
  // operates in "pull" mode, and we append silence only, releasing the audio
  // input track.
  bool mEnabled;
  // Whether or not we've ended and removed the AudioProcessingTrack.
  bool mEnded;
  // When processing is enabled, the number of packets received by this
  // instance, to implement periodic logging.
  uint64_t mPacketCount;
  // Temporary descriptor for a slice of an AudioChunk parameter passed to
  // ProcessOutputData().  This is a member rather than on the stack so that
  // any memory allocated for its mChannelData pointer array is not
  // reallocated on each iteration.
  AudioChunk mSubChunk;
  // A storage holding the interleaved audio data converted the AudioSegment.
  // This will be used as an input parameter for PacketizeAndProcess. This
  // should be removed once bug 1729041 is done.
  AutoTArray<AudioDataValue,
             SilentChannel::AUDIO_PROCESSING_FRAMES * GUESS_AUDIO_CHANNELS>
      mInterleavedBuffer;
  // Tracks the pending frames with paired principals piled up in packetizer.
  std::deque<std::pair<TrackTime, PrincipalHandle>> mChunksInPacketizer;
};

// MediaTrack subclass tailored for MediaEngineWebRTCMicrophoneSource.
class AudioProcessingTrack : public DeviceInputConsumerTrack {
  // Only accessed on the graph thread.
  RefPtr<AudioInputProcessing> mInputProcessing;

  explicit AudioProcessingTrack(TrackRate aSampleRate)
      : DeviceInputConsumerTrack(aSampleRate) {}

  ~AudioProcessingTrack() = default;

 public:
  // Main Thread API
  void Destroy() override;
  void SetInputProcessing(RefPtr<AudioInputProcessing> aInputProcessing);
  static AudioProcessingTrack* Create(MediaTrackGraph* aGraph);

  // Graph Thread API
  void DestroyImpl() override;
  void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override;
  uint32_t NumberOfChannels() const override {
    MOZ_DIAGNOSTIC_ASSERT(
        mInputProcessing,
        "Must set mInputProcessing before exposing to content");
    return mInputProcessing->GetRequestedInputChannelCount();
  }
  // Pass the graph's mixed audio output to mInputProcessing for processing as
  // the reverse stream.
  void NotifyOutputData(MediaTrackGraph* aGraph, const AudioChunk& aChunk);

  // Any thread
  AudioProcessingTrack* AsAudioProcessingTrack() override { return this; }

 private:
  // Graph thread API
  void SetInputProcessingImpl(RefPtr<AudioInputProcessing> aInputProcessing);
};

class MediaEngineWebRTCAudioCaptureSource : public MediaEngineSource {
 public:
  explicit MediaEngineWebRTCAudioCaptureSource(const MediaDevice* aMediaDevice);
  static nsString GetUUID();
  static nsString GetGroupId();
  nsresult Allocate(const dom::MediaTrackConstraints& aConstraints,
                    const MediaEnginePrefs& aPrefs, uint64_t aWindowID,
                    const char** aOutBadConstraint) override {
    // Nothing to do here, everything is managed in MediaManager.cpp
    return NS_OK;
  }
  nsresult Deallocate() override {
    // Nothing to do here, everything is managed in MediaManager.cpp
    return NS_OK;
  }
  void SetTrack(const RefPtr<MediaTrack>& aTrack,
                const PrincipalHandle& aPrincipal) override;
  nsresult Start() override;
  nsresult Stop() override;
  nsresult Reconfigure(const dom::MediaTrackConstraints& aConstraints,
                       const MediaEnginePrefs& aPrefs,
                       const char** aOutBadConstraint) override;

  nsresult TakePhoto(MediaEnginePhotoCallback* aCallback) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  void GetSettings(dom::MediaTrackSettings& aOutSettings) const override;

 protected:
  virtual ~MediaEngineWebRTCAudioCaptureSource() = default;
};

}  // end namespace mozilla

#endif  // MediaEngineWebRTCAudio_h
