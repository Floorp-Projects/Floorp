/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINEWEBRTC_H_
#define MEDIAENGINEWEBRTC_H_

#include "AudioPacketizer.h"
#include "AudioSegment.h"
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

// Small subset of VoEHardware
class AudioInput
{
public:
  AudioInput() = default;
  // Threadsafe because it's referenced from an MicrophoneSource, which can
  // had references to it on other threads.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AudioInput)

  virtual int GetNumOfRecordingDevices(int& aDevices) = 0;
  virtual int GetRecordingDeviceName(int aIndex, char (&aStrNameUTF8)[128],
                                     char aStrGuidUTF8[128]) = 0;
  virtual int GetRecordingDeviceStatus(bool& aIsAvailable) = 0;
  virtual void GetChannelCount(uint32_t& aChannels) = 0;
  virtual int GetMaxAvailableChannels(uint32_t& aChannels) = 0;
  virtual void StartRecording(SourceMediaStream *aStream, AudioDataListener *aListener) = 0;
  virtual void StopRecording(SourceMediaStream *aStream) = 0;
  virtual int SetRecordingDevice(int aIndex) = 0;
  virtual void SetUserChannelCount(uint32_t aChannels) = 0;

protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~AudioInput() = default;
};

class AudioInputCubeb final : public AudioInput
{
public:
  explicit AudioInputCubeb(int aIndex = 0) :
    AudioInput(), mSelectedDevice(aIndex), mInUseCount(0)
  {
    if (!mDeviceIndexes) {
      mDeviceIndexes = new nsTArray<int>;
      mDeviceNames = new nsTArray<nsCString>;
      mDefaultDevice = -1;
    }
  }

  static void CleanupGlobalData()
  {
    if (mDevices.device) {
      cubeb_device_collection_destroy(CubebUtils::GetCubebContext(), &mDevices);
    }
    delete mDeviceIndexes;
    mDeviceIndexes = nullptr;
    delete mDeviceNames;
    mDeviceNames = nullptr;
  }

  int GetNumOfRecordingDevices(int& aDevices)
  {
#ifdef MOZ_WIDGET_ANDROID
    // OpenSL ES does not support enumerate device.
    aDevices = 1;
#else
    UpdateDeviceList();
    aDevices = mDeviceIndexes->Length();
#endif
    return 0;
  }

  static int32_t DeviceIndex(int aIndex)
  {
    // -1 = system default if any
    if (aIndex == -1) {
      if (mDefaultDevice == -1) {
        aIndex = 0;
      } else {
        aIndex = mDefaultDevice;
      }
    }
    MOZ_ASSERT(mDeviceIndexes);
    if (aIndex < 0 || aIndex >= (int) mDeviceIndexes->Length()) {
      return -1;
    }
    // Note: if the device is gone, this will be -1
    return (*mDeviceIndexes)[aIndex]; // translate to mDevices index
  }

  static StaticMutex& Mutex()
  {
    return sMutex;
  }

  static bool GetDeviceID(int aDeviceIndex, CubebUtils::AudioDeviceID &aID)
  {
    // Assert sMutex is held
    sMutex.AssertCurrentThreadOwns();
#ifdef MOZ_WIDGET_ANDROID
    aID = nullptr;
    return true;
#else
    int dev_index = DeviceIndex(aDeviceIndex);
    if (dev_index != -1) {
      aID = mDevices.device[dev_index].devid;
      return true;
    }
    return false;
#endif
  }

  int GetRecordingDeviceName(int aIndex, char (&aStrNameUTF8)[128],
                             char aStrGuidUTF8[128])
  {
#ifdef MOZ_WIDGET_ANDROID
    aStrNameUTF8[0] = '\0';
    aStrGuidUTF8[0] = '\0';
#else
    int32_t devindex = DeviceIndex(aIndex);
    if (mDevices.count == 0 || devindex < 0) {
      return 1;
    }
    SprintfLiteral(aStrNameUTF8, "%s%s", aIndex == -1 ? "default: " : "",
                   mDevices.device[devindex].friendly_name);
    aStrGuidUTF8[0] = '\0';
#endif
    return 0;
  }

  int GetRecordingDeviceStatus(bool& aIsAvailable)
  {
    // With cubeb, we only expose devices of type CUBEB_DEVICE_TYPE_INPUT,
    // so unless it was removed, say it's available
    aIsAvailable = true;
    return 0;
  }

  void GetChannelCount(uint32_t& aChannels)
  {
    GetUserChannelCount(mSelectedDevice, aChannels);
  }

  static void GetUserChannelCount(int aDeviceIndex, uint32_t& aChannels)
  {
    aChannels = sUserChannelCount;
  }

  int GetMaxAvailableChannels(uint32_t& aChannels)
  {
    return GetDeviceMaxChannels(mSelectedDevice, aChannels);
  }

  static int GetDeviceMaxChannels(int aDeviceIndex, uint32_t& aChannels)
  {
#ifdef MOZ_WIDGET_ANDROID
    aChannels = 1;
#else
    int32_t devindex = DeviceIndex(aDeviceIndex);
    if (mDevices.count == 0 || devindex < 0) {
      return 1;
    }
    aChannels = mDevices.device[devindex].max_channels;
#endif
    return 0;
  }

  void SetUserChannelCount(uint32_t aChannels)
  {
    if (GetDeviceMaxChannels(mSelectedDevice, sUserChannelCount)) {
      sUserChannelCount = 1; // error capture mono
      return;
    }

    if (aChannels && aChannels < sUserChannelCount) {
      sUserChannelCount = aChannels;
    }
  }

  void StartRecording(SourceMediaStream *aStream, AudioDataListener *aListener)
  {
#ifdef MOZ_WIDGET_ANDROID
    // OpenSL ES does not support enumerating devices.
    MOZ_ASSERT(mDevices.count == 0);
#else
    MOZ_ASSERT(mDevices.count > 0);
#endif

    mAnyInUse = true;
    mInUseCount++;
    // Always tell the stream we're using it for input
    aStream->OpenAudioInput(mSelectedDevice, aListener);
  }

  void StopRecording(SourceMediaStream *aStream)
  {
    aStream->CloseAudioInput();
    if (--mInUseCount == 0) {
      mAnyInUse = false;
    }
  }

  int SetRecordingDevice(int aIndex)
  {
    mSelectedDevice = aIndex;
    return 0;
  }

protected:
  ~AudioInputCubeb() {
    MOZ_RELEASE_ASSERT(mInUseCount == 0);
  }

private:
  // It would be better to watch for device-change notifications
  void UpdateDeviceList();

  // We have an array, which consists of indexes to the current mDevices
  // list.  This is updated on mDevices updates.  Many devices in mDevices
  // won't be included in the array (wrong type, etc), or if a device is
  // removed it will map to -1 (and opens of this device will need to check
  // for this - and be careful of threading access.  The mappings need to
  // updated on each re-enumeration.
  int mSelectedDevice;
  uint32_t mInUseCount;

  // pointers to avoid static constructors
  static nsTArray<int>* mDeviceIndexes;
  static int mDefaultDevice; // -1 == not set
  static nsTArray<nsCString>* mDeviceNames;
  static cubeb_device_collection mDevices;
  static bool mAnyInUse;
  static StaticMutex sMutex;
  static uint32_t sUserChannelCount;
};

class WebRTCAudioDataListener : public AudioDataListener
{
protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~WebRTCAudioDataListener() {}

public:
  explicit WebRTCAudioDataListener(MediaEngineWebRTCMicrophoneSource* aAudioSource)
    : mMutex("WebRTCAudioDataListener::mMutex")
    , mAudioSource(aAudioSource)
  {}

  // AudioDataListenerInterface methods
  void NotifyOutputData(MediaStreamGraph* aGraph,
                        AudioDataValue* aBuffer,
                        size_t aFrames,
                        TrackRate aRate,
                        uint32_t aChannels) override;

  void NotifyInputData(MediaStreamGraph* aGraph,
                       const AudioDataValue* aBuffer,
                       size_t aFrames,
                       TrackRate aRate,
                       uint32_t aChannels) override;

  void DeviceChanged() override;

  void Shutdown();

private:
  Mutex mMutex;
  RefPtr<MediaEngineWebRTCMicrophoneSource> mAudioSource;
};

class MediaEngineWebRTCMicrophoneSource : public MediaEngineSource,
                                          public AudioDataListenerInterface
{
public:
  MediaEngineWebRTCMicrophoneSource(mozilla::AudioInput* aAudioInput,
                                    int aIndex,
                                    const char* name,
                                    const char* uuid,
                                    bool aDelayAgnostic,
                                    bool aExtendedFilter);

  bool RequiresSharing() const override
  {
    return true;
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
  void NotifyOutputData(MediaStreamGraph* aGraph,
                        AudioDataValue* aBuffer, size_t aFrames,
                        TrackRate aRate, uint32_t aChannels) override;
  void NotifyInputData(MediaStreamGraph* aGraph,
                       const AudioDataValue* aBuffer, size_t aFrames,
                       TrackRate aRate, uint32_t aChannels) override;

  void DeviceChanged() override;

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

  void PacketizeAndProcess(MediaStreamGraph* aGraph,
                           const AudioDataValue* aBuffer,
                           size_t aFrames,
                           TrackRate aRate,
                           uint32_t aChannels);


  // This is true when all processing is disabled, we can skip
  // packetization, resampling and other processing passes.
  // Graph thread only.
  bool PassThrough() const;

  // Graph thread only.
  void SetPassThrough(bool aPassThrough);

  // Owning thread only.
  RefPtr<WebRTCAudioDataListener> mListener;

  // Note: shared across all microphone sources. Owning thread only.
  static int sChannelsOpen;

  const RefPtr<mozilla::AudioInput> mAudioInput;
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

  // Current state of the shared resource for this source.
  // Set under mMutex on the owning thread. Accessed under one of the two
  MediaEngineSourceState mState = kReleased;

  int mCapIndex;
  bool mDelayAgnostic;
  bool mExtendedFilter;
  bool mStarted;

  const nsString mDeviceName;
  const nsCString mDeviceUUID;

  // The current settings for the underlying device.
  // Member access is main thread only after construction.
  const nsMainThreadPtrHandle<media::Refcountable<dom::MediaTrackSettings>> mSettings;

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
                        nsTArray<RefPtr<MediaDevice>>*) override;
  void ReleaseResourcesForWindow(uint64_t aWindowId) override;
private:
  ~MediaEngineWebRTC() = default;
  void EnumerateVideoDevices(uint64_t aWindowId,
                             dom::MediaSourceEnum,
                             nsTArray<RefPtr<MediaDevice>>*);
  void EnumerateMicrophoneDevices(uint64_t aWindowId,
                                  nsTArray<RefPtr<MediaDevice>>*);

  nsCOMPtr<nsIThread> mThread;

  // gUM runnables can e.g. Enumerate from multiple threads
  Mutex mMutex;
  RefPtr<mozilla::AudioInput> mAudioInput;
  bool mFullDuplex;
  bool mDelayAgnostic;
  bool mExtendedFilter;
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
