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

class MediaEngineWebRTCMicrophoneSource;

// This class is instantiated on the MediaManager thread, and is then sent and
// only ever access again on the MediaStreamGraph.
class WebRTCAudioDataListener : public AudioDataListener
{
protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~WebRTCAudioDataListener() {}

public:
  explicit WebRTCAudioDataListener(MediaEngineWebRTCMicrophoneSource* aAudioSource)
    : mAudioSource(aAudioSource)
  {}

  // AudioDataListenerInterface methods
  void NotifyOutputData(MediaStreamGraphImpl* aGraph,
                        AudioDataValue* aBuffer,
                        size_t aFrames,
                        TrackRate aRate,
                        uint32_t aChannels) override;

  void NotifyInputData(MediaStreamGraphImpl* aGraph,
                       const AudioDataValue* aBuffer,
                       size_t aFrames,
                       TrackRate aRate,
                       uint32_t aChannels) override;

  uint32_t RequestedInputChannelCount(MediaStreamGraphImpl* aGraph) override;

  void DeviceChanged(MediaStreamGraphImpl* aGraph) override;

  void Disconnect(MediaStreamGraphImpl* aGraph) override;

private:
  RefPtr<MediaEngineWebRTCMicrophoneSource> mAudioSource;
};

class MediaEngineWebRTCMicrophoneSource : public MediaEngineSource,
                                          public AudioDataListenerInterface
{
public:
  MediaEngineWebRTCMicrophoneSource(RefPtr<AudioDeviceInfo> aInfo,
                                    const nsString& name,
                                    const nsCString& uuid,
                                    uint32_t maxChannelCount,
                                    bool aDelayAgnostic,
                                    bool aExtendedFilter);

  bool RequiresSharing() const override
  {
    return false;
  }

  nsString GetName() const override;
  nsCString GetUUID() const override;

  nsresult Allocate(const dom::MediaTrackConstraints &aConstraints,
                    const MediaEnginePrefs& aPrefs,
                    const nsString& aDeviceId,
                    const ipc::PrincipalInfo& aPrincipalInfo,
                    AllocationHandle** aOutHandle,
                    const char** aOutBadConstraint) override;
  nsresult Deallocate(const RefPtr<const AllocationHandle>& aHandle) override;
  nsresult SetTrack(const RefPtr<const AllocationHandle>& aHandle,
                    const RefPtr<SourceMediaStream>& aStream,
                    TrackID aTrackID,
                    const PrincipalHandle& aPrincipal) override;
  nsresult Start(const RefPtr<const AllocationHandle>& aHandle) override;
  nsresult Stop(const RefPtr<const AllocationHandle>& aHandle) override;
  nsresult Reconfigure(const RefPtr<AllocationHandle>& aHandle,
                       const dom::MediaTrackConstraints& aConstraints,
                       const MediaEnginePrefs& aPrefs,
                       const nsString& aDeviceId,
                       const char** aOutBadConstraint) override;

  /**
   * Assigns the current settings of the capture to aOutSettings.
   * Main thread only.
   */
  void GetSettings(dom::MediaTrackSettings& aOutSettings) const override;

  void Pull(const RefPtr<const AllocationHandle>& aHandle,
            const RefPtr<SourceMediaStream>& aStream,
            TrackID aTrackID,
            StreamTime aDesiredTime,
            const PrincipalHandle& aPrincipalHandle) override;

  // AudioDataListenerInterface methods
  void NotifyOutputData(MediaStreamGraphImpl* aGraph,
                        AudioDataValue* aBuffer, size_t aFrames,
                        TrackRate aRate, uint32_t aChannels) override;
  void NotifyInputData(MediaStreamGraphImpl* aGraph,
                       const AudioDataValue* aBuffer, size_t aFrames,
                       TrackRate aRate, uint32_t aChannels) override;

  void DeviceChanged(MediaStreamGraphImpl* aGraph) override;

  uint32_t RequestedInputChannelCount(MediaStreamGraphImpl* aGraph) override
  {
    return GetRequestedInputChannelCount(aGraph);
  }

  void Disconnect(MediaStreamGraphImpl* aGraph) override;

  dom::MediaSourceEnum GetMediaSource() const override
  {
    return dom::MediaSourceEnum::Microphone;
  }

  nsresult TakePhoto(MediaEnginePhotoCallback* aCallback) override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  uint32_t GetBestFitnessDistance(
    const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
    const nsString& aDeviceId) const override;

  void Shutdown() override;

protected:
  ~MediaEngineWebRTCMicrophoneSource() {}

private:
  /**
   * Reevaluates the aggregated constraints of all allocations and restarts the
   * underlying device if necessary.
   *
   * If the given AllocationHandle was already registered, its constraints will
   * be updated before reevaluation. If not, they will be added before
   * reevaluation.
   */
  nsresult ReevaluateAllocation(const RefPtr<AllocationHandle>& aHandle,
                                const NormalizedConstraints* aConstraintsUpdate,
                                const MediaEnginePrefs& aPrefs,
                                const nsString& aDeviceId,
                                const char** aOutBadConstraint);

  /**
   * Updates the underlying (single) device with the aggregated constraints
   * aNetConstraints. If the chosen settings for the device changes based on
   * these new constraints, and capture is active, the device will be restarted.
   */
  nsresult UpdateSingleSource(const RefPtr<const AllocationHandle>& aHandle,
                              const NormalizedConstraints& aNetConstraints,
                              const MediaEnginePrefs& aPrefs,
                              const nsString& aDeviceId,
                              const char** aOutBadConstraint);


  void UpdateAECSettingsIfNeeded(bool aEnable, webrtc::EcModes aMode);
  void UpdateAGCSettingsIfNeeded(bool aEnable, webrtc::AgcModes aMode);
  void UpdateNSSettingsIfNeeded(bool aEnable, webrtc::NsModes aMode);

  void ApplySettings(const MediaEnginePrefs& aPrefs,
                     RefPtr<MediaStreamGraphImpl> aGraph);

  bool HasEnabledTrack() const;

  template<typename T>
  void InsertInGraph(const T* aBuffer,
                     size_t aFrames,
                     uint32_t aChannels);

  void PacketizeAndProcess(MediaStreamGraphImpl* aGraph,
                           const AudioDataValue* aBuffer,
                           size_t aFrames,
                           TrackRate aRate,
                           uint32_t aChannels);


  // Graph thread only.
  void SetPassThrough(bool aPassThrough);
  // Graph thread only.
  uint32_t GetRequestedInputChannelCount(MediaStreamGraphImpl* aGraphImpl);
  // Graph thread only.
  void SetRequestedInputChannelCount(uint32_t aRequestedInputChannelCount);
  // This is true when all processing is disabled, we can skip
  // packetization, resampling and other processing passes.
  // Graph thread only.
  bool PassThrough(MediaStreamGraphImpl* aGraphImpl) const;

  // Those are written on the MediaManager thread, read on either the
  // MediaManager thread or the MSG thread. Guarded by mMutex.
  RefPtr<AllocationHandle> mHandle;
  RefPtr<SourceMediaStream> mStream;
  TrackID mTrackID = TRACK_NONE;
  PrincipalHandle mPrincipal = PRINCIPAL_HANDLE_NONE;
  bool mEnabled = false;

  // Set on construction and then immutable. Used on the MediaManager thread.
  const RefPtr<AudioDeviceInfo> mDeviceInfo;
  // Those four members are set on construction, on the MediaManager thread.
  const bool mDelayAgnostic;
  const bool mExtendedFilter;
  const nsString mDeviceName;
  const nsCString mDeviceUUID;
  // The current settings for the underlying device.
  // Constructed on the MediaManager thread, and then only ever accessed on the
  // main thread.
  const nsMainThreadPtrHandle<media::Refcountable<dom::MediaTrackSettings>> mSettings;
  // To only update microphone when needed, we keep track of the prefs
  // representing the currently applied settings for this source. This is the
  // net result of the prefs across all allocations.
  // Owning thread only.
  MediaEnginePrefs mNetPrefs;

  // Current state of the shared resource for this source. Written on the
  // owning thread, read on either the owning thread or the MSG thread.
  Atomic<MediaEngineSourceState> mState;
  // This mutex must be held to access mAllocation (and its members) and
  // modifying mListener.
  Mutex mMutex;
  // mListener is created on the MediaManager thread, and then sent to the MSG
  // thread. On shutdown, we send this pointer to the MSG thread again, telling
  // it to clean up.
  RefPtr<WebRTCAudioDataListener> mListener;
  // Created on the MediaManager thread, then used on the graph thread for
  // processing, and on the MediaManager thread when setting parameters (this is
  // thread safe).
  const UniquePtr<webrtc::AudioProcessing> mAudioProcessing;
  // Accessed from the GraphDriver thread except for deletion, at which point
  // the GraphDriver thread does not touch those values.
  nsAutoPtr<AudioPacketizer<AudioDataValue, float>> mPacketizerInput;
  nsAutoPtr<AudioPacketizer<AudioDataValue, float>> mPacketizerOutput;
  // The number of channels asked for by content, after clamping to the range of
  // legal channel count for this particular device. This is the number of
  // channels of the input buffer passed as parameter in NotifyInputData.
  // Initially set on MediaManger thread in the ctor, then only ever accessed on
  // the MSG thread.
  uint32_t mRequestedInputChannelCount;
  // mSkipProcessing is true if none of the processing passes are enabled,
  // because of prefs or constraints. This allows simply copying the audio into
  // the MSG, skipping resampling and the whole webrtc.org code.
  // This is read and written to only on the MSG thread.
  bool mSkipProcessing;
  // All these are only used on the MSG thread.
  // Stores the mixed audio output for the reverse-stream of the AEC.
  AlignedFloatBuffer mOutputBuffer;
  // Stores the microphone audio, to be processed by the APM.
  AlignedFloatBuffer mInputBuffer;
  // Stores the deinterleaved microphone audio
  AlignedFloatBuffer mDeinterleavedBuffer;
  // Stores the mixed down input audio
  AlignedFloatBuffer mInputDownmixBuffer;
#ifdef DEBUG
  // The MSGImpl::IterationEnd() of the last time we appended data from an
  // audio callback.
  // Guarded by MediaEngineWebRTCMicrophoneSource::mMutex.
  GraphTime mLastCallbackAppendTime = 0;
#endif
  // Set to false by Start(). Becomes true after the first time we append real
  // audio frames from the audio callback.
  // Guarded by MediaEngineWebRTCMicrophoneSource::mMutex.
  bool mLiveFramesAppended = false;

  // Set to false by Start(). Becomes true after the first time we append
  // silence *after* the first audio callback has appended real frames.
  // Guarded by MediaEngineWebRTCMicrophoneSource::mMutex.
  bool mLiveSilenceAppended = false;
};


class MediaEngineWebRTCAudioCaptureSource : public MediaEngineSource
{
public:
  explicit MediaEngineWebRTCAudioCaptureSource(const char* aUuid)
  {
  }
  nsString GetName() const override;
  nsCString GetUUID() const override;
  nsresult Allocate(const dom::MediaTrackConstraints &aConstraints,
                    const MediaEnginePrefs &aPrefs,
                    const nsString& aDeviceId,
                    const ipc::PrincipalInfo& aPrincipalInfo,
                    AllocationHandle** aOutHandle,
                    const char** aOutBadConstraint) override
  {
    // Nothing to do here, everything is managed in MediaManager.cpp
    *aOutHandle = nullptr;
    return NS_OK;
  }
  nsresult Deallocate(const RefPtr<const AllocationHandle>& aHandle) override
  {
    // Nothing to do here, everything is managed in MediaManager.cpp
    MOZ_ASSERT(!aHandle);
    return NS_OK;
  }
  nsresult SetTrack(const RefPtr<const AllocationHandle>& aHandle,
                    const RefPtr<SourceMediaStream>& aStream,
                    TrackID aTrackID,
                    const PrincipalHandle& aPrincipal) override;
  nsresult Start(const RefPtr<const AllocationHandle>& aHandle) override;
  nsresult Stop(const RefPtr<const AllocationHandle>& aHandle) override;
  nsresult Reconfigure(const RefPtr<AllocationHandle>& aHandle,
                       const dom::MediaTrackConstraints& aConstraints,
                       const MediaEnginePrefs& aPrefs,
                       const nsString& aDeviceId,
                       const char** aOutBadConstraint) override;

  void Pull(const RefPtr<const AllocationHandle>& aHandle,
            const RefPtr<SourceMediaStream>& aStream,
            TrackID aTrackID,
            StreamTime aDesiredTime,
            const PrincipalHandle& aPrincipalHandle) override
  {}

  dom::MediaSourceEnum GetMediaSource() const override
  {
    return dom::MediaSourceEnum::AudioCapture;
  }

  nsresult TakePhoto(MediaEnginePhotoCallback* aCallback) override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  uint32_t GetBestFitnessDistance(
    const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
    const nsString& aDeviceId) const override;

protected:
  virtual ~MediaEngineWebRTCAudioCaptureSource() = default;
};

} // end namespace mozilla

#endif // MediaEngineWebRTCAudio_h
