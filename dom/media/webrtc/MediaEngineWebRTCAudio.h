/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaEngineWebRTCAudio_h
#define MediaEngineWebRTCAudio_h

#include "AudioPacketizer.h"
#include "AudioSegment.h"
#include "AudioDeviceInfo.h"
#include "MediaEngineWebRTC.h"
#include "MediaTrackListener.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"

namespace mozilla {

class AudioInputProcessing;
class AudioInputTrack;

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
  MediaEngineWebRTCMicrophoneSource(RefPtr<AudioDeviceInfo> aInfo,
                                    const nsString& aDeviceName,
                                    const nsCString& aDeviceUUID,
                                    const nsString& aDeviceGroup,
                                    uint32_t aMaxChannelCount,
                                    bool aDelayAgnostic, bool aExtendedFilter);

  nsString GetName() const override;
  nsCString GetUUID() const override;
  nsString GetGroupId() const override;

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

  dom::MediaSourceEnum GetMediaSource() const override {
    return dom::MediaSourceEnum::Microphone;
  }

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

  /**
   * Sent the AudioProcessingModule parameter for a given processing algorithm.
   */
  void UpdateAECSettings(bool aEnable, bool aUseAecMobile,
                         webrtc::EchoCancellation::SuppressionLevel aLevel,
                         webrtc::EchoControlMobile::RoutingMode aRoutingMode);
  void UpdateAGCSettings(bool aEnable, webrtc::GainControl::Mode aMode);
  void UpdateHPFSettings(bool aEnable);
  void UpdateNSSettings(bool aEnable, webrtc::NoiseSuppression::Level aLevel);
  void UpdateAPMExtraOptions(bool aExtendedFilter, bool aDelayAgnostic);

  PrincipalHandle mPrincipal = PRINCIPAL_HANDLE_NONE;

  const RefPtr<AudioDeviceInfo> mDeviceInfo;
  const bool mDelayAgnostic;
  const bool mExtendedFilter;
  const nsString mDeviceName;
  const nsCString mDeviceUUID;
  const nsString mDeviceGroup;

  // The maximum number of channels that this device supports.
  const uint32_t mDeviceMaxChannelCount;
  // The current settings for the underlying device.
  // Constructed on the MediaManager thread, and then only ever accessed on the
  // main thread.
  const nsMainThreadPtrHandle<media::Refcountable<dom::MediaTrackSettings>>
      mSettings;

  // Current state of the resource for this source.
  MediaEngineSourceState mState;

  // The current preferences for the APM's various processing stages.
  MediaEnginePrefs mCurrentPrefs;

  // The AudioInputTrack used to inteface with the MediaTrackGraph. Set in
  // SetTrack as part of the initialization, and nulled in ::Deallocate.
  RefPtr<AudioInputTrack> mTrack;

  // See note at the top of this class.
  RefPtr<AudioInputProcessing> mInputProcessing;
};

// This class is created on the MediaManager thread, and then exclusively used
// on the MTG thread.
// All communication is done via message passing using MTG ControlMessages
class AudioInputProcessing : public AudioDataListener {
 public:
  AudioInputProcessing(uint32_t aMaxChannelCount,
                       const PrincipalHandle& aPrincipalHandle);

  void Pull(MediaTrackGraphImpl* aGraph, GraphTime aFrom, GraphTime aTo,
            GraphTime aTrackEnd, AudioSegment* aSegment,
            bool aLastPullThisIteration, bool* aEnded);

  void NotifyOutputData(MediaTrackGraphImpl* aGraph, BufferInfo aInfo) override;
  void NotifyInputStopped(MediaTrackGraphImpl* aGraph) override;
  void NotifyInputData(MediaTrackGraphImpl* aGraph, const BufferInfo aInfo,
                       uint32_t aAlreadyBuffered) override;
  bool IsVoiceInput(MediaTrackGraphImpl* aGraph) const override {
    // If we're passing data directly without AEC or any other process, this
    // means that all voice-processing has been disabled intentionaly. In this
    // case, consider that the device is not used for voice input.
    return !PassThrough(aGraph);
  }

  void Start();
  void Stop();

  void DeviceChanged(MediaTrackGraphImpl* aGraph) override;

  uint32_t RequestedInputChannelCount(MediaTrackGraphImpl*) override {
    return GetRequestedInputChannelCount();
  }

  void Disconnect(MediaTrackGraphImpl* aGraph) override;

  // aSegment stores the unprocessed non-interleaved audio input data from mic
  void ProcessInput(MediaTrackGraphImpl* aGraph, const AudioSegment* aSegment);

  void PacketizeAndProcess(MediaTrackGraphImpl* aGraph,
                           const AudioDataValue* aBuffer, size_t aFrames,
                           TrackRate aRate, uint32_t aChannels);

  void SetPassThrough(MediaTrackGraphImpl* aGraph, bool aPassThrough);
  uint32_t GetRequestedInputChannelCount();
  void SetRequestedInputChannelCount(MediaTrackGraphImpl* aGraph,
                                     uint32_t aRequestedInputChannelCount);
  // This is true when all processing is disabled, we can skip
  // packetization, resampling and other processing passes.
  bool PassThrough(MediaTrackGraphImpl* aGraphImpl) const;

  // This allow changing the APM options, enabling or disabling processing
  // steps.
  void UpdateAECSettings(bool aEnable, bool aUseAecMobile,
                         webrtc::EchoCancellation::SuppressionLevel aLevel,
                         webrtc::EchoControlMobile::RoutingMode aRoutingMode);
  void UpdateAGCSettings(bool aEnable, webrtc::GainControl::Mode aMode);
  void UpdateHPFSettings(bool aEnable);
  void UpdateNSSettings(bool aEnable, webrtc::NoiseSuppression::Level aLevel);
  void UpdateAPMExtraOptions(bool aExtendedFilter, bool aDelayAgnostic);

  void End();

  TrackTime NumBufferedFrames(MediaTrackGraphImpl* aGraph) const;

 private:
  ~AudioInputProcessing() = default;
  // This implements the processing algoritm to apply to the input (e.g. a
  // microphone). If all algorithms are disabled, this class in not used. This
  // class only accepts audio chunks of 10ms. It has two inputs and one output:
  // it is fed the speaker data and the microphone data. It outputs processed
  // input data.
  const UniquePtr<webrtc::AudioProcessing> mAudioProcessing;
  // Packetizer to be able to feed 10ms packets to the input side of
  // mAudioProcessing. Not used if the processing is bypassed.
  UniquePtr<AudioPacketizer<AudioDataValue, float>> mPacketizerInput;
  // Packetizer to be able to feed 10ms packets to the output side of
  // mAudioProcessing. Not used if the processing is bypassed.
  UniquePtr<AudioPacketizer<AudioDataValue, float>> mPacketizerOutput;
  // The number of channels asked for by content, after clamping to the range of
  // legal channel count for this particular device. This is the number of
  // channels of the input buffer passed as parameter in NotifyInputData.
  uint32_t mRequestedInputChannelCount;
  // mSkipProcessing is true if none of the processing passes are enabled,
  // because of prefs or constraints. This allows simply copying the audio into
  // the MTG, skipping resampling and the whole webrtc.org code.
  bool mSkipProcessing;
  // Stores the mixed audio output for the reverse-stream of the AEC (the
  // speaker data).
  AlignedFloatBuffer mOutputBuffer;
  // Stores the input audio, to be processed by the APM.
  AlignedFloatBuffer mInputBuffer;
  // Stores the deinterleaved microphone audio
  AlignedFloatBuffer mDeinterleavedBuffer;
  // Stores the mixed down input audio
  AlignedFloatBuffer mInputDownmixBuffer;
  // Stores data waiting to be pulled.
  AudioSegment mSegment;
  // Set to Nothing() by Start(). Once live frames have been appended from the
  // audio callback, this is the number of frames appended as pre-buffer for
  // that data, to avoid underruns. Buffering in the track might be needed
  // because of the AUDIO_BLOCK interval at which we run the graph, the
  // packetizer keeping some input data. Care must be taken when turning on and
  // off the packetizer.
  Maybe<TrackTime> mLiveBufferingAppended;
  // Principal for the data that flows through this class.
  const PrincipalHandle mPrincipal;
  // Whether or not this MediaEngine is enabled. If it's not enabled, it
  // operates in "pull" mode, and we append silence only, releasing the audio
  // input track.
  bool mEnabled;
  // Whether or not we've ended and removed the AudioInputTrack.
  bool mEnded;
  // Store the unprocessed interleaved audio input data
  Maybe<BufferInfo> mInputData;
};

// MediaTrack subclass tailored for MediaEngineWebRTCMicrophoneSource.
class AudioInputTrack : public ProcessedMediaTrack {
  // Only accessed on the graph thread.
  RefPtr<AudioInputProcessing> mInputProcessing;

  // Only accessed on the main thread. Link to the track producing raw audio
  // input data. Graph thread should use mInputs to get the source
  RefPtr<MediaInputPort> mPort;

  // Only accessed on the main thread. Used for bookkeeping on main thread, such
  // that CloseAudioInput can be idempotent.
  // XXX Should really be a CubebUtils::AudioDeviceID, but they aren't
  // copyable (opaque pointers)
  RefPtr<AudioDataListener> mInputListener;

  // Only accessed on the main thread.
  Maybe<CubebUtils::AudioDeviceID> mDeviceId;

  explicit AudioInputTrack(TrackRate aSampleRate)
      : ProcessedMediaTrack(aSampleRate, MediaSegment::AUDIO,
                            new AudioSegment()) {}

  ~AudioInputTrack() = default;

 public:
  // Main Thread API
  // Users of audio inputs go through the track so it can track when the
  // last track referencing an input goes away, so it can close the cubeb
  // input. Main thread only.
  nsresult OpenAudioInput(CubebUtils::AudioDeviceID aId,
                          AudioDataListener* aListener);
  void CloseAudioInput();
  Maybe<CubebUtils::AudioDeviceID> DeviceId() const;
  void Destroy() override;
  void SetInputProcessing(RefPtr<AudioInputProcessing> aInputProcessing);
  static AudioInputTrack* Create(MediaTrackGraph* aGraph);

  // Graph Thread API
  void DestroyImpl() override;
  void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override;
  uint32_t NumberOfChannels() const override {
    MOZ_DIAGNOSTIC_ASSERT(
        mInputProcessing,
        "Must set mInputProcessing before exposing to content");
    return mInputProcessing->GetRequestedInputChannelCount();
  }

  // Any thread
  AudioInputTrack* AsAudioInputTrack() override { return this; }

 private:
  // Graph thread API
  void SetInputProcessingImpl(RefPtr<AudioInputProcessing> aInputProcessing);
};

class MediaEngineWebRTCAudioCaptureSource : public MediaEngineSource {
 public:
  explicit MediaEngineWebRTCAudioCaptureSource(const char* aUuid) {}
  nsString GetName() const override;
  nsCString GetUUID() const override;
  nsString GetGroupId() const override;
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

  dom::MediaSourceEnum GetMediaSource() const override {
    return dom::MediaSourceEnum::AudioCapture;
  }

  nsresult TakePhoto(MediaEnginePhotoCallback* aCallback) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  void GetSettings(dom::MediaTrackSettings& aOutSettings) const override;

 protected:
  virtual ~MediaEngineWebRTCAudioCaptureSource() = default;
};

}  // end namespace mozilla

#endif  // MediaEngineWebRTCAudio_h
