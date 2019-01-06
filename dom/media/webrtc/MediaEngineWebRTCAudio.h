/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaEngineWebRTCAudio_h
#define MediaEngineWebRTCAudio_h

#include "MediaEngineWebRTC.h"
#include "AudioPacketizer.h"
#include "AudioSegment.h"
#include "AudioDeviceInfo.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"

namespace mozilla {

class AudioInputProcessing;

// This class is created and used exclusively on the Media Manager thread, with
// exactly two exceptions:
// - Pull is always called on the MSG thread. It only ever uses
//   mInputProcessing. mInputProcessing is set, then a message is sent first to
//   the main thread and then the MSG thread so that it can be used as part of
//   the graph processing. On destruction, similarly, a message is sent to the
//   graph so that it stops using it, and then it is deleted.
// - mSettings is created on the MediaManager thread is always ever accessed on
//   the Main Thread. It is const.
class MediaEngineWebRTCMicrophoneSource : public MediaEngineSource {
 public:
  MediaEngineWebRTCMicrophoneSource(RefPtr<AudioDeviceInfo> aInfo,
                                    const nsString& name, const nsCString& uuid,
                                    uint32_t maxChannelCount,
                                    bool aDelayAgnostic, bool aExtendedFilter);

  bool RequiresSharing() const override { return false; }

  nsString GetName() const override;
  nsCString GetUUID() const override;

  nsresult Allocate(const dom::MediaTrackConstraints& aConstraints,
                    const MediaEnginePrefs& aPrefs, const nsString& aDeviceId,
                    const ipc::PrincipalInfo& aPrincipalInfo,
                    AllocationHandle** aOutHandle,
                    const char** aOutBadConstraint) override;
  nsresult Deallocate(const RefPtr<const AllocationHandle>& aHandle) override;
  void SetTrack(const RefPtr<const AllocationHandle>& aHandle,
                const RefPtr<SourceMediaStream>& aStream, TrackID aTrackID,
                const PrincipalHandle& aPrincipal) override;
  nsresult Start(const RefPtr<const AllocationHandle>& aHandle) override;
  nsresult Stop(const RefPtr<const AllocationHandle>& aHandle) override;
  nsresult Reconfigure(const RefPtr<AllocationHandle>& aHandle,
                       const dom::MediaTrackConstraints& aConstraints,
                       const MediaEnginePrefs& aPrefs,
                       const nsString& aDeviceId,
                       const char** aOutBadConstraint) override;

  void Pull(const RefPtr<const AllocationHandle>& aHandle,
            const RefPtr<SourceMediaStream>& aStream, TrackID aTrackID,
            StreamTime aEndOfAppendedData, StreamTime aDesiredTime,
            const PrincipalHandle& aPrincipalHandle) override;

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

  uint32_t GetBestFitnessDistance(
      const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) const override;

  void Shutdown() override;

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
                         webrtc::EchoCancellation::SuppressionLevel aLevel);
  void UpdateAGCSettings(bool aEnable, webrtc::GainControl::Mode aMode);
  void UpdateNSSettings(bool aEnable, webrtc::NoiseSuppression::Level aLevel);
  void UpdateAPMExtraOptions(bool aExtendedFilter, bool aDelayAgnostic);

  TrackID mTrackID = TRACK_NONE;
  PrincipalHandle mPrincipal = PRINCIPAL_HANDLE_NONE;

  const RefPtr<AudioDeviceInfo> mDeviceInfo;
  const bool mDelayAgnostic;
  const bool mExtendedFilter;
  const nsString mDeviceName;
  const nsCString mDeviceUUID;

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

  // The SourecMediaStream on which to append data for this microphone. Set in
  // SetTrack as part of the initialization, and nulled in ::Deallocate.
  RefPtr<SourceMediaStream> mStream;

  // See note at the top of this class.
  RefPtr<AudioInputProcessing> mInputProcessing;
};

// This class is created on the MediaManager thread, and then exclusively used
// on the MSG thread.
// All communication is done via message passing using MSG ControlMessages
class AudioInputProcessing : public AudioDataListener {
 public:
  AudioInputProcessing(uint32_t aMaxChannelCount,
                       RefPtr<SourceMediaStream> aStream, TrackID aTrackID,
                       const PrincipalHandle& aPrincipalHandle);

  void Pull(const RefPtr<SourceMediaStream>& aStream, TrackID aTrackID,
            StreamTime aEndOfAppendedData, StreamTime aDesiredTime,
            const PrincipalHandle& aPrincipalHandle);

  void NotifyOutputData(MediaStreamGraphImpl* aGraph, AudioDataValue* aBuffer,
                        size_t aFrames, TrackRate aRate,
                        uint32_t aChannels) override;
  void NotifyInputData(MediaStreamGraphImpl* aGraph,
                       const AudioDataValue* aBuffer, size_t aFrames,
                       TrackRate aRate, uint32_t aChannels) override;

  void Start();
  void Stop();

  void DeviceChanged(MediaStreamGraphImpl* aGraph) override;

  uint32_t RequestedInputChannelCount(MediaStreamGraphImpl* aGraph) override {
    return GetRequestedInputChannelCount(aGraph);
  }

  void Disconnect(MediaStreamGraphImpl* aGraph) override;

  template <typename T>
  void InsertInGraph(const T* aBuffer, size_t aFrames, uint32_t aChannels);

  void PacketizeAndProcess(MediaStreamGraphImpl* aGraph,
                           const AudioDataValue* aBuffer, size_t aFrames,
                           TrackRate aRate, uint32_t aChannels);

  void SetPassThrough(bool aPassThrough);
  uint32_t GetRequestedInputChannelCount(MediaStreamGraphImpl* aGraphImpl);
  void SetRequestedInputChannelCount(uint32_t aRequestedInputChannelCount);
  // This is true when all processing is disabled, we can skip
  // packetization, resampling and other processing passes.
  bool PassThrough(MediaStreamGraphImpl* aGraphImpl) const;

  // This allow changing the APM options, enabling or disabling processing
  // steps.
  void UpdateAECSettings(bool aEnable, bool aUseAecMobile,
                         webrtc::EchoCancellation::SuppressionLevel aLevel);
  void UpdateAGCSettings(bool aEnable, webrtc::GainControl::Mode aMode);
  void UpdateNSSettings(bool aEnable, webrtc::NoiseSuppression::Level aLevel);
  void UpdateAPMExtraOptions(bool aExtendedFilter, bool aDelayAgnostic);

  void End();

 private:
  ~AudioInputProcessing() = default;
  RefPtr<SourceMediaStream> mStream;
  // This implements the processing algoritm to apply to the input (e.g. a
  // microphone). If all algorithms are disabled, this class in not used. This
  // class only accepts audio chunks of 10ms. It has two inputs and one output:
  // it is fed the speaker data and the microphone data. It outputs processed
  // input data.
  const UniquePtr<webrtc::AudioProcessing> mAudioProcessing;
  // Packetizer to be able to feed 10ms packets to the input side of
  // mAudioProcessing. Not used if the processing is bypassed.
  nsAutoPtr<AudioPacketizer<AudioDataValue, float>> mPacketizerInput;
  // Packetizer to be able to feed 10ms packets to the output side of
  // mAudioProcessing. Not used if the processing is bypassed.
  nsAutoPtr<AudioPacketizer<AudioDataValue, float>> mPacketizerOutput;
  // The number of channels asked for by content, after clamping to the range of
  // legal channel count for this particular device. This is the number of
  // channels of the input buffer passed as parameter in NotifyInputData.
  uint32_t mRequestedInputChannelCount;
  // mSkipProcessing is true if none of the processing passes are enabled,
  // because of prefs or constraints. This allows simply copying the audio into
  // the MSG, skipping resampling and the whole webrtc.org code.
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
#ifdef DEBUG
  // The MSGImpl::IterationEnd() of the last time we appended data from an
  // audio callback.
  GraphTime mLastCallbackAppendTime;
#endif
  // Set to false by Start(). Becomes true after the first time we append real
  // audio frames from the audio callback.
  bool mLiveFramesAppended;
  // Set to false by Start(). Becomes true after the first time we append
  // silence *after* the first audio callback has appended real frames.
  bool mLiveSilenceAppended;
  // Track ID on which the data is to be appended after processing
  TrackID mTrackID;
  // Principal for the data that flows through this class.
  PrincipalHandle mPrincipal;
  // Whether or not this MediaEngine is enabled. If it's not enabled, it
  // operates in "pull" mode, and we append silence only, releasing the audio
  // input stream.
  bool mEnabled;
  // Whether or not we've ended and removed the track in the SourceMediaStream
  bool mEnded;
};

class MediaEngineWebRTCAudioCaptureSource : public MediaEngineSource {
 public:
  explicit MediaEngineWebRTCAudioCaptureSource(const char* aUuid) {}
  nsString GetName() const override;
  nsCString GetUUID() const override;
  nsresult Allocate(const dom::MediaTrackConstraints& aConstraints,
                    const MediaEnginePrefs& aPrefs, const nsString& aDeviceId,
                    const ipc::PrincipalInfo& aPrincipalInfo,
                    AllocationHandle** aOutHandle,
                    const char** aOutBadConstraint) override {
    // Nothing to do here, everything is managed in MediaManager.cpp
    *aOutHandle = nullptr;
    return NS_OK;
  }
  nsresult Deallocate(const RefPtr<const AllocationHandle>& aHandle) override {
    // Nothing to do here, everything is managed in MediaManager.cpp
    MOZ_ASSERT(!aHandle);
    return NS_OK;
  }
  void SetTrack(const RefPtr<const AllocationHandle>& aHandle,
                const RefPtr<SourceMediaStream>& aStream, TrackID aTrackID,
                const PrincipalHandle& aPrincipal) override;
  nsresult Start(const RefPtr<const AllocationHandle>& aHandle) override;
  nsresult Stop(const RefPtr<const AllocationHandle>& aHandle) override;
  nsresult Reconfigure(const RefPtr<AllocationHandle>& aHandle,
                       const dom::MediaTrackConstraints& aConstraints,
                       const MediaEnginePrefs& aPrefs,
                       const nsString& aDeviceId,
                       const char** aOutBadConstraint) override;

  void Pull(const RefPtr<const AllocationHandle>& aHandle,
            const RefPtr<SourceMediaStream>& aStream, TrackID aTrackID,
            StreamTime aEndOfAppendedData, StreamTime aDesiredTime,
            const PrincipalHandle& aPrincipalHandle) override {
    MOZ_ASSERT_UNREACHABLE("Should never have to append silence");
  }

  dom::MediaSourceEnum GetMediaSource() const override {
    return dom::MediaSourceEnum::AudioCapture;
  }

  nsresult TakePhoto(MediaEnginePhotoCallback* aCallback) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  uint32_t GetBestFitnessDistance(
      const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) const override;

 protected:
  virtual ~MediaEngineWebRTCAudioCaptureSource() = default;
};

}  // end namespace mozilla

#endif  // MediaEngineWebRTCAudio_h
