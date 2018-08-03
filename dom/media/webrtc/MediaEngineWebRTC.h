/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINEWEBRTC_H_
#define MEDIAENGINEWEBRTC_H_

#include "AudioPacketizer.h"
#include "AudioSegment.h"
#include "AudioDeviceInfo.h"
#include "CamerasChild.h"
#include "cubeb/cubeb.h"
#include "CubebUtils.h"
#include "DOMMediaStream.h"
#include "ipc/IPCMessageUtils.h"
#include "MediaEngine.h"
#include "MediaEnginePrefs.h"
#include "MediaEngineSource.h"
#include "MediaEngineWrapper.h"
#include "MediaStreamGraph.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/Mutex.h"
#include "mozilla/Mutex.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/UniquePtr.h"
#include "nsAutoPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIThread.h"
#include "nsIRunnable.h"
#include "nsRefPtrHashtable.h"
#include "nsThreadUtils.h"
#include "NullTransport.h"
#include "prcvar.h"
#include "prthread.h"
#include "StreamTracks.h"
#include "VideoSegment.h"
#include "VideoUtils.h"

// WebRTC library includes follow
// Audio Engine
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/include/voe_audio_processing.h"
#include "webrtc/voice_engine/include/voe_volume_control.h"
#include "webrtc/voice_engine/include/voe_external_media.h"
#include "webrtc/voice_engine/include/voe_audio_processing.h"
#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
// Video Engine
// conflicts with #include of scoped_ptr.h
#undef FF
#include "webrtc/modules/video_capture/video_capture_defines.h"

namespace mozilla {

class MediaEngineWebRTCMicrophoneSource;

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

// This class implements a cache for accessing the audio device list. It can be
// accessed on any thread.
class CubebDeviceEnumerator final
{
public:
  CubebDeviceEnumerator();
  ~CubebDeviceEnumerator();
  // This method returns a list of all the input and output audio devices
  // available on this machine.
  // This method is safe to call from all threads.
  void EnumerateAudioInputDevices(nsTArray<RefPtr<AudioDeviceInfo>>& aOutDevices);
  // From a cubeb device id, return the info for this device, if it's still a
  // valid id, or nullptr otherwise.
  // This method is safe to call from any thread.
  already_AddRefed<AudioDeviceInfo>
  DeviceInfoFromID(CubebUtils::AudioDeviceID aID);

protected:

  // Static function called by cubeb when the audio input device list changes
  // (i.e. when a new device is made available, or non-available). This
  // re-binds to the MediaEngineWebRTC that instantiated this
  // CubebDeviceEnumerator, and simply calls `AudioDeviceListChanged` below.
  static void AudioDeviceListChanged_s(cubeb* aContext, void* aUser);
  // Invalidates the cached audio input device list, can be called on any
  // thread.
  void AudioDeviceListChanged();

private:
  // Synchronize access to mDevices
  Mutex mMutex;
  nsTArray<RefPtr<AudioDeviceInfo>> mDevices;
  // If mManualInvalidation is true, then it is necessary to query the device
  // list each time instead of relying on automatic invalidation of the cache by
  // cubeb itself. Set in the constructor and then can be access on any thread.
  bool mManualInvalidation;
};

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
   * Representation of data tied to an AllocationHandle rather than to the source.
   */
  struct Allocation {
    Allocation() = delete;
    explicit Allocation(const RefPtr<AllocationHandle>& aHandle);
    ~Allocation();

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

    const RefPtr<AllocationHandle> mHandle;
    RefPtr<SourceMediaStream> mStream;
    TrackID mTrackID = TRACK_NONE;
    PrincipalHandle mPrincipal = PRINCIPAL_HANDLE_NONE;
    bool mEnabled = false;
  };

  /**
   * Used with nsTArray<Allocation>::IndexOf to locate an Allocation by a handle.
   */
  class AllocationHandleComparator {
  public:
    bool Equals(const Allocation& aAllocation,
                const RefPtr<const AllocationHandle>& aHandle) const
    {
      return aHandle == aAllocation.mHandle;
    }
  };

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


  // This is true when all processing is disabled, we can skip
  // packetization, resampling and other processing passes.
  // Graph thread only.
  bool PassThrough(MediaStreamGraphImpl* aGraphImpl) const;

  // Graph thread only.
  void SetPassThrough(bool aPassThrough);
  uint32_t GetRequestedInputChannelCount(MediaStreamGraphImpl* aGraphImpl);
  void SetRequestedInputChannelCount(uint32_t aRequestedInputChannelCount);

  // mListener is created on the MediaManager thread, and then sent to the MSG
  // thread. On shutdown, we send this pointer to the MSG thread again, telling
  // it to clean up.
  RefPtr<WebRTCAudioDataListener> mListener;

  // Can be shared on any thread.
  const RefPtr<AudioDeviceInfo> mDeviceInfo;

  const UniquePtr<webrtc::AudioProcessing> mAudioProcessing;

  // accessed from the GraphDriver thread except for deletion.
  nsAutoPtr<AudioPacketizer<AudioDataValue, float>> mPacketizerInput;
  nsAutoPtr<AudioPacketizer<AudioDataValue, float>> mPacketizerOutput;

  // mMutex protects some of our members off the owning thread.
  Mutex mMutex;

  // We append an allocation in Allocate() and remove it in Deallocate().
  // Both the array and the Allocation members are modified under mMutex on
  // the owning thread. Accessed under one of the two.
  nsTArray<Allocation> mAllocations;

  // Current state of the shared resource for this source. Written on the
  // owning thread, read on either the owning thread or the MSG thread.
  Atomic<MediaEngineSourceState> mState;

  bool mDelayAgnostic;
  bool mExtendedFilter;
  bool mStarted;

  const nsString mDeviceName;
  const nsCString mDeviceUUID;

  // The current settings for the underlying device.
  // Member access is main thread only after construction.
  const nsMainThreadPtrHandle<media::Refcountable<dom::MediaTrackSettings>> mSettings;

  // The number of channels asked for by content, after clamping to the range of
  // legal channel count for this particular device. This is the number of
  // channels of the input buffer passed as parameter in NotifyInputData.
  uint32_t mRequestedInputChannelCount;
  uint64_t mTotalFrames;
  uint64_t mLastLogFrames;

  // mSkipProcessing is true if none of the processing passes are enabled,
  // because of prefs or constraints. This allows simply copying the audio into
  // the MSG, skipping resampling and the whole webrtc.org code.
  // This is read and written to only on the MSG thread.
  bool mSkipProcessing;

  // To only update microphone when needed, we keep track of the prefs
  // representing the currently applied settings for this source. This is the
  // net result of the prefs across all allocations.
  // Owning thread only.
  MediaEnginePrefs mNetPrefs;

  // Stores the mixed audio output for the reverse-stream of the AEC.
  AlignedFloatBuffer mOutputBuffer;

  AlignedFloatBuffer mInputBuffer;
  AlignedFloatBuffer mDeinterleavedBuffer;
  AlignedFloatBuffer mInputDownmixBuffer;
};

class MediaEngineWebRTC : public MediaEngine
{
  typedef MediaEngine Super;
public:
  explicit MediaEngineWebRTC(MediaEnginePrefs& aPrefs);

  virtual void SetFakeDeviceChangeEvents() override;

  // Clients should ensure to clean-up sources video/audio sources
  // before invoking Shutdown on this class.
  void Shutdown() override;

  // Returns whether the host supports duplex audio stream.
  bool SupportsDuplex();

  void EnumerateDevices(uint64_t aWindowId,
                        dom::MediaSourceEnum,
                        MediaSinkEnum,
                        nsTArray<RefPtr<MediaDevice>>*) override;
  void ReleaseResourcesForWindow(uint64_t aWindowId) override;
private:
  ~MediaEngineWebRTC() = default;
  void EnumerateVideoDevices(uint64_t aWindowId,
                             dom::MediaSourceEnum,
                             nsTArray<RefPtr<MediaDevice>>*);
  void EnumerateMicrophoneDevices(uint64_t aWindowId,
                                  nsTArray<RefPtr<MediaDevice>>*);
  void EnumerateSpeakerDevices(uint64_t aWindowId,
                               nsTArray<RefPtr<MediaDevice> >*);

  // gUM runnables can e.g. Enumerate from multiple threads
  Mutex mMutex;
  UniquePtr<mozilla::CubebDeviceEnumerator> mEnumerator;
  const bool mDelayAgnostic;
  const bool mExtendedFilter;
  // This also is set in the ctor and then never changed, but we can't make it
  // const because we pass it to a function that takes bool* in the ctor.
  bool mHasTabVideoSource;

  // Maps WindowID to a map of device uuid to their MediaEngineSource,
  // separately for audio and video.
  nsClassHashtable<nsUint64HashKey,
                    nsRefPtrHashtable<nsStringHashKey,
                                      MediaEngineSource>> mVideoSources;
  nsClassHashtable<nsUint64HashKey,
                    nsRefPtrHashtable<nsStringHashKey,
                                      MediaEngineSource>> mAudioSources;
};

}

#endif /* NSMEDIAENGINEWEBRTC_H_ */
