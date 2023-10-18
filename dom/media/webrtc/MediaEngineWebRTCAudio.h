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

  // The current preferences that will be forwarded to mAudioProcessingConfig
  // below.
  MediaEnginePrefs mCurrentPrefs;

  // The AudioProcessingTrack used to inteface with the MediaTrackGraph. Set in
  // SetTrack as part of the initialization, and nulled in ::Deallocate.
  RefPtr<AudioProcessingTrack> mTrack;

  // See note at the top of this class.
  RefPtr<AudioInputProcessing> mInputProcessing;

  // Copy of the config currently applied to AudioProcessing through
  // mInputProcessing.
  webrtc::AudioProcessing::Config mAudioProcessingConfig;
};

// This class is created on the MediaManager thread, and then exclusively used
// on the MTG thread.
// All communication is done via message passing using MTG ControlMessages
class AudioInputProcessing : public AudioDataListener {
 public:
  explicit AudioInputProcessing(uint32_t aMaxChannelCount);
  void Process(MediaTrackGraph* aGraph, GraphTime aFrom, GraphTime aTo,
               AudioSegment* aInput, AudioSegment* aOutput);

  void ProcessOutputData(MediaTrackGraph* aGraph, AudioDataValue* aBuffer,
                         size_t aFrames, TrackRate aRate, uint32_t aChannels);
  bool IsVoiceInput(MediaTrackGraph* aGraph) const override {
    // If we're passing data directly without AEC or any other process, this
    // means that all voice-processing has been disabled intentionaly. In this
    // case, consider that the device is not used for voice input.
    return !PassThrough(aGraph);
  }

  void Start(MediaTrackGraph* aGraph);
  void Stop(MediaTrackGraph* aGraph);

  void DeviceChanged(MediaTrackGraph* aGraph) override;

  uint32_t RequestedInputChannelCount(MediaTrackGraph*) override {
    return GetRequestedInputChannelCount();
  }

  void Disconnect(MediaTrackGraph* aGraph) override;

  void PacketizeAndProcess(MediaTrackGraph* aGraph,
                           const AudioSegment& aSegment);

  void SetPassThrough(MediaTrackGraph* aGraph, bool aPassThrough);
  uint32_t GetRequestedInputChannelCount();
  void SetRequestedInputChannelCount(MediaTrackGraph* aGraph,
                                     CubebUtils::AudioDeviceID aDeviceId,
                                     uint32_t aRequestedInputChannelCount);
  // This is true when all processing is disabled, we can skip
  // packetization, resampling and other processing passes.
  bool PassThrough(MediaTrackGraph* aGraph) const;

  // This allow changing the APM options, enabling or disabling processing
  // steps. The config gets applied the next time we're about to process input
  // data.
  void ApplyConfig(MediaTrackGraph* aGraph,
                   const webrtc::AudioProcessing::Config& aConfig);

  void End();

  TrackTime NumBufferedFrames(MediaTrackGraph* aGraph) const;

  // The packet size contains samples in 10ms. The unit of aRate is hz.
  constexpr static uint32_t GetPacketSize(TrackRate aRate) {
    return static_cast<uint32_t>(aRate) / 100u;
  }

  bool IsEnded() const { return mEnded; }

 private:
  ~AudioInputProcessing() = default;
  void EnsureAudioProcessing(MediaTrackGraph* aGraph, uint32_t aChannels);
  void ResetAudioProcessing(MediaTrackGraph* aGraph);
  PrincipalHandle GetCheckedPrincipal(const AudioSegment& aSegment);
  // This implements the processing algoritm to apply to the input (e.g. a
  // microphone). If all algorithms are disabled, this class in not used. This
  // class only accepts audio chunks of 10ms. It has two inputs and one output:
  // it is fed the speaker data and the microphone data. It outputs processed
  // input data.
  const UniquePtr<webrtc::AudioProcessing> mAudioProcessing;
  // Packetizer to be able to feed 10ms packets to the input side of
  // mAudioProcessing. Not used if the processing is bypassed.
  Maybe<AudioPacketizer<AudioDataValue, float>> mPacketizerInput;
  // Packetizer to be able to feed 10ms packets to the output side of
  // mAudioProcessing. Not used if the processing is bypassed.
  Maybe<AudioPacketizer<AudioDataValue, float>> mPacketizerOutput;
  // The number of channels asked for by content, after clamping to the range of
  // legal channel count for this particular device.
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
  // Whether or not this MediaEngine is enabled. If it's not enabled, it
  // operates in "pull" mode, and we append silence only, releasing the audio
  // input track.
  bool mEnabled;
  // Whether or not we've ended and removed the AudioProcessingTrack.
  bool mEnded;
  // When processing is enabled, the number of packets received by this
  // instance, to implement periodic logging.
  uint64_t mPacketCount;
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
  void NotifyOutputData(MediaTrackGraph* aGraph, AudioDataValue* aBuffer,
                        size_t aFrames, TrackRate aRate, uint32_t aChannels);

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
