/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINEWEBRTC_H_
#define MEDIAENGINEWEBRTC_H_

#include "prcvar.h"
#include "prthread.h"
#include "prprf.h"
#include "nsIThread.h"
#include "nsIRunnable.h"

#include "mozilla/dom/File.h"
#include "mozilla/Mutex.h"
#include "mozilla/Monitor.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"
#include "DOMMediaStream.h"
#include "nsDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"
#include "nsRefPtrHashtable.h"

#include "VideoUtils.h"
#include "MediaEngineCameraVideoSource.h"
#include "VideoSegment.h"
#include "AudioSegment.h"
#include "StreamBuffer.h"
#include "MediaStreamGraph.h"
#include "cubeb/cubeb.h"
#include "CubebUtils.h"
#include "AudioPacketizer.h"

#include "MediaEngineWrapper.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
// WebRTC library includes follow
#include "webrtc/common.h"
// Audio Engine
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_hardware.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/include/voe_audio_processing.h"
#include "webrtc/voice_engine/include/voe_volume_control.h"
#include "webrtc/voice_engine/include/voe_external_media.h"
#include "webrtc/voice_engine/include/voe_audio_processing.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"

// Video Engine
// conflicts with #include of scoped_ptr.h
#undef FF
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_render.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "CamerasChild.h"

#include "NullTransport.h"
#include "AudioOutputObserver.h"

namespace mozilla {

class MediaEngineWebRTCAudioCaptureSource : public MediaEngineAudioSource
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit MediaEngineWebRTCAudioCaptureSource(const char* aUuid)
    : MediaEngineAudioSource(kReleased)
  {
  }
  void GetName(nsAString& aName) override;
  void GetUUID(nsACString& aUUID) override;
  nsresult Allocate(const dom::MediaTrackConstraints& aConstraints,
                    const MediaEnginePrefs& aPrefs,
                    const nsString& aDeviceId,
                    const nsACString& aOrigin) override
  {
    // Nothing to do here, everything is managed in MediaManager.cpp
    return NS_OK;
  }
  nsresult Deallocate() override
  {
    // Nothing to do here, everything is managed in MediaManager.cpp
    return NS_OK;
  }
  void Shutdown() override
  {
    // Nothing to do here, everything is managed in MediaManager.cpp
  }
  nsresult Start(SourceMediaStream* aMediaStream, TrackID aId) override;
  nsresult Stop(SourceMediaStream* aMediaStream, TrackID aId) override;
  nsresult Restart(const dom::MediaTrackConstraints& aConstraints,
                   const MediaEnginePrefs &aPrefs,
                   const nsString& aDeviceId) override;
  void SetDirectListeners(bool aDirect) override
  {}
  void NotifyOutputData(MediaStreamGraph* aGraph,
                        AudioDataValue* aBuffer, size_t aFrames,
                        TrackRate aRate, uint32_t aChannels) override
  {}
  void NotifyInputData(MediaStreamGraph* aGraph,
                       const AudioDataValue* aBuffer, size_t aFrames,
                       TrackRate aRate, uint32_t aChannels) override
  {}
  void NotifyPull(MediaStreamGraph* aGraph, SourceMediaStream* aSource,
                  TrackID aID, StreamTime aDesiredTime) override
  {}
  dom::MediaSourceEnum GetMediaSource() const override
  {
    return dom::MediaSourceEnum::AudioCapture;
  }
  bool IsFake() override
  {
    return false;
  }
  nsresult TakePhoto(PhotoCallback* aCallback) override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  uint32_t GetBestFitnessDistance(
    const nsTArray<const dom::MediaTrackConstraintSet*>& aConstraintSets,
    const nsString& aDeviceId) override;

protected:
  virtual ~MediaEngineWebRTCAudioCaptureSource() { Shutdown(); }
  nsCString mUUID;
};

// Small subset of VoEHardware
class AudioInput
{
public:
  explicit AudioInput(webrtc::VoiceEngine* aVoiceEngine) : mVoiceEngine(aVoiceEngine) {};
  // Threadsafe because it's referenced from an MicrophoneSource, which can
  // had references to it on other threads.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AudioInput)

  virtual int GetNumOfRecordingDevices(int& aDevices) = 0;
  virtual int GetRecordingDeviceName(int aIndex, char aStrNameUTF8[128],
                                     char aStrGuidUTF8[128]) = 0;
  virtual int GetRecordingDeviceStatus(bool& aIsAvailable) = 0;
  virtual void StartRecording(SourceMediaStream *aStream, AudioDataListener *aListener) = 0;
  virtual void StopRecording(SourceMediaStream *aStream) = 0;
  virtual int SetRecordingDevice(int aIndex) = 0;

protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~AudioInput() {}

  webrtc::VoiceEngine* mVoiceEngine;
};

class AudioInputCubeb final : public AudioInput
{
public:
  explicit AudioInputCubeb(webrtc::VoiceEngine* aVoiceEngine, int aIndex = 0) :
    AudioInput(aVoiceEngine), mSelectedDevice(aIndex), mInUseCount(0)
  {
    if (!mDeviceIndexes) {
      mDeviceIndexes = new nsTArray<int>;
      mDeviceNames = new nsTArray<nsCString>;
    }
  }

  static void CleanupGlobalData()
  {
    if (mDevices) {
      // This doesn't require anything more than support for free()
      cubeb_device_collection_destroy(mDevices);
      mDevices = nullptr;
    }
    delete mDeviceIndexes;
    mDeviceIndexes = nullptr;
    delete mDeviceNames;
    mDeviceNames = nullptr;
  }

  int GetNumOfRecordingDevices(int& aDevices)
  {
    UpdateDeviceList();
    aDevices = mDeviceIndexes->Length();
    return 0;
  }

  int32_t DeviceIndex(int aIndex)
  {
    if (aIndex == -1) {
      aIndex = 0; // -1 = system default
    }
    if (aIndex >= (int) mDeviceIndexes->Length()) {
      return -1;
    }
    // Note: if the device is gone, this will be -1
    return (*mDeviceIndexes)[aIndex]; // translate to mDevices index
  }

  int GetRecordingDeviceName(int aIndex, char aStrNameUTF8[128],
                             char aStrGuidUTF8[128])
  {
    int32_t devindex = DeviceIndex(aIndex);
    if (!mDevices || devindex < 0) {
      return 1;
    }
    PR_snprintf(aStrNameUTF8, 128, "%s%s", aIndex == -1 ? "default: " : "",
                mDevices->device[devindex]->friendly_name);
    aStrGuidUTF8[0] = '\0';
    return 0;
  }

  int GetRecordingDeviceStatus(bool& aIsAvailable)
  {
    // With cubeb, we only expose devices of type CUBEB_DEVICE_TYPE_INPUT,
    // so unless it was removed, say it's available
    aIsAvailable = true;
    return 0;
  }

  void StartRecording(SourceMediaStream *aStream, AudioDataListener *aListener)
  {
    MOZ_ASSERT(mDevices);

    if (mInUseCount == 0) {
      ScopedCustomReleasePtr<webrtc::VoEExternalMedia> ptrVoERender;
      ptrVoERender = webrtc::VoEExternalMedia::GetInterface(mVoiceEngine);
      if (ptrVoERender) {
        ptrVoERender->SetExternalRecordingStatus(true);
      }
      mAnyInUse = true;
    }
    mInUseCount++;
    // Always tell the stream we're using it for input
    aStream->OpenAudioInput(mDevices->device[mSelectedDevice]->devid, aListener);
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
    int32_t devindex = DeviceIndex(aIndex);
    if (!mDevices || devindex < 0) {
      return 1;
    }
    mSelectedDevice = devindex;
    return 0;
  }

protected:
  ~AudioInputCubeb() {
    MOZ_RELEASE_ASSERT(mInUseCount == 0);
  }

private:
  // It would be better to watch for device-change notifications
  void UpdateDeviceList()
  {
    cubeb_device_collection *devices = nullptr;

    if (CUBEB_OK != cubeb_enumerate_devices(CubebUtils::GetCubebContext(),
                                            CUBEB_DEVICE_TYPE_INPUT,
                                            &devices)) {
      return;
    }

    for (auto& device_index : (*mDeviceIndexes)) {
      device_index = -1; // unmapped
    }
    // We keep all the device names, but wipe the mappings and rebuild them

    // Calculate translation from existing mDevices to new devices. Note we
    // never end up with less devices than before, since people have
    // stashed indexes.
    // For some reason the "fake" device for automation is marked as DISABLED,
    // so white-list it.
    for (uint32_t i = 0; i < devices->count; i++) {
      if (devices->device[i]->type == CUBEB_DEVICE_TYPE_INPUT && // paranoia
          (devices->device[i]->state == CUBEB_DEVICE_STATE_ENABLED ||
           devices->device[i]->state == CUBEB_DEVICE_STATE_UNPLUGGED ||
           (devices->device[i]->state == CUBEB_DEVICE_STATE_DISABLED &&
            strcmp(devices->device[i]->friendly_name, "Sine source at 440 Hz") == 0)))
      {
        auto j = mDeviceNames->IndexOf(devices->device[i]->device_id);
        if (j != nsTArray<nsCString>::NoIndex) {
          // match! update the mapping
          (*mDeviceIndexes)[j] = i;
        } else {
          // new device, add to the array
          mDeviceIndexes->AppendElement(i);
          mDeviceNames->AppendElement(devices->device[i]->device_id);
        }
      }
    }
    // swap state
    if (mDevices) {
      cubeb_device_collection_destroy(mDevices);
    }
    mDevices = devices;
  }

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
  static nsTArray<nsCString>* mDeviceNames;
  static cubeb_device_collection *mDevices;
  static bool mAnyInUse;
};

class AudioInputWebRTC final : public AudioInput
{
public:
  explicit AudioInputWebRTC(webrtc::VoiceEngine* aVoiceEngine) : AudioInput(aVoiceEngine) {}

  int GetNumOfRecordingDevices(int& aDevices)
  {
    ScopedCustomReleasePtr<webrtc::VoEHardware> ptrVoEHw;
    ptrVoEHw = webrtc::VoEHardware::GetInterface(mVoiceEngine);
    if (!ptrVoEHw)  {
      return 1;
    }
    return ptrVoEHw->GetNumOfRecordingDevices(aDevices);
  }

  int GetRecordingDeviceName(int aIndex, char aStrNameUTF8[128],
                             char aStrGuidUTF8[128])
  {
    ScopedCustomReleasePtr<webrtc::VoEHardware> ptrVoEHw;
    ptrVoEHw = webrtc::VoEHardware::GetInterface(mVoiceEngine);
    if (!ptrVoEHw)  {
      return 1;
    }
    return ptrVoEHw->GetRecordingDeviceName(aIndex, aStrNameUTF8,
                                            aStrGuidUTF8);
  }

  int GetRecordingDeviceStatus(bool& aIsAvailable)
  {
    ScopedCustomReleasePtr<webrtc::VoEHardware> ptrVoEHw;
    ptrVoEHw = webrtc::VoEHardware::GetInterface(mVoiceEngine);
    if (!ptrVoEHw)  {
      return 1;
    }
    ptrVoEHw->GetRecordingDeviceStatus(aIsAvailable);
    return 0;
  }

  void StartRecording(SourceMediaStream *aStream, AudioDataListener *aListener) {}
  void StopRecording(SourceMediaStream *aStream) {}

  int SetRecordingDevice(int aIndex)
  {
    ScopedCustomReleasePtr<webrtc::VoEHardware> ptrVoEHw;
    ptrVoEHw = webrtc::VoEHardware::GetInterface(mVoiceEngine);
    if (!ptrVoEHw)  {
      return 1;
    }
    return ptrVoEHw->SetRecordingDevice(aIndex);
  }

protected:
  // Protected destructor, to discourage deletion outside of Release():
  ~AudioInputWebRTC() {}
};

class WebRTCAudioDataListener : public AudioDataListener
{
protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~WebRTCAudioDataListener() {}

public:
  explicit WebRTCAudioDataListener(MediaEngineAudioSource* aAudioSource) :
    mAudioSource(aAudioSource)
  {}

  // AudioDataListenerInterface methods
  virtual void NotifyOutputData(MediaStreamGraph* aGraph,
                                AudioDataValue* aBuffer, size_t aFrames,
                                TrackRate aRate, uint32_t aChannels) override
  {
    mAudioSource->NotifyOutputData(aGraph, aBuffer, aFrames, aRate, aChannels);
  }
  virtual void NotifyInputData(MediaStreamGraph* aGraph,
                               const AudioDataValue* aBuffer, size_t aFrames,
                               TrackRate aRate, uint32_t aChannels) override
  {
    mAudioSource->NotifyInputData(aGraph, aBuffer, aFrames, aRate, aChannels);
  }

private:
  RefPtr<MediaEngineAudioSource> mAudioSource;
};

class MediaEngineWebRTCMicrophoneSource : public MediaEngineAudioSource,
                                          public webrtc::VoEMediaProcess,
                                          private MediaConstraintsHelper
{
public:
  MediaEngineWebRTCMicrophoneSource(nsIThread* aThread,
                                    webrtc::VoiceEngine* aVoiceEnginePtr,
                                    mozilla::AudioInput* aAudioInput,
                                    int aIndex,
                                    const char* name,
                                    const char* uuid)
    : MediaEngineAudioSource(kReleased)
    , mVoiceEngine(aVoiceEnginePtr)
    , mAudioInput(aAudioInput)
    , mMonitor("WebRTCMic.Monitor")
    , mThread(aThread)
    , mCapIndex(aIndex)
    , mChannel(-1)
    , mNrAllocations(0)
    , mInitDone(false)
    , mStarted(false)
    , mSampleFrequency(MediaEngine::DEFAULT_SAMPLE_RATE)
    , mEchoOn(false), mAgcOn(false), mNoiseOn(false)
    , mEchoCancel(webrtc::kEcDefault)
    , mAGC(webrtc::kAgcDefault)
    , mNoiseSuppress(webrtc::kNsDefault)
    , mPlayoutDelay(0)
    , mNullTransport(nullptr)
    , mInputBufferLen(0) {
    MOZ_ASSERT(aVoiceEnginePtr);
    MOZ_ASSERT(aAudioInput);
    mDeviceName.Assign(NS_ConvertUTF8toUTF16(name));
    mDeviceUUID.Assign(uuid);
    mListener = new mozilla::WebRTCAudioDataListener(this);
    Init();
  }

  void GetName(nsAString& aName) override;
  void GetUUID(nsACString& aUUID) override;

  nsresult Allocate(const dom::MediaTrackConstraints& aConstraints,
                    const MediaEnginePrefs& aPrefs,
                    const nsString& aDeviceId,
                    const nsACString& aOrigin) override;
  nsresult Deallocate() override;
  nsresult Start(SourceMediaStream* aStream, TrackID aID) override;
  nsresult Stop(SourceMediaStream* aSource, TrackID aID) override;
  nsresult Restart(const dom::MediaTrackConstraints& aConstraints,
                   const MediaEnginePrefs &aPrefs,
                   const nsString& aDeviceId) override;
  void SetDirectListeners(bool aHasDirectListeners) override {};

  void NotifyPull(MediaStreamGraph* aGraph,
                  SourceMediaStream* aSource,
                  TrackID aId,
                  StreamTime aDesiredTime) override;

  // AudioDataListenerInterface methods
  void NotifyOutputData(MediaStreamGraph* aGraph,
                        AudioDataValue* aBuffer, size_t aFrames,
                        TrackRate aRate, uint32_t aChannels) override;
  void NotifyInputData(MediaStreamGraph* aGraph,
                       const AudioDataValue* aBuffer, size_t aFrames,
                       TrackRate aRate, uint32_t aChannels) override;

  bool IsFake() override {
    return false;
  }

  dom::MediaSourceEnum GetMediaSource() const override {
    return dom::MediaSourceEnum::Microphone;
  }

  nsresult TakePhoto(PhotoCallback* aCallback) override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  uint32_t GetBestFitnessDistance(
      const nsTArray<const dom::MediaTrackConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) override;

  // VoEMediaProcess.
  void Process(int channel, webrtc::ProcessingTypes type,
               int16_t audio10ms[], int length,
               int samplingFreq, bool isStereo) override;

  void Shutdown() override;

  NS_DECL_THREADSAFE_ISUPPORTS

protected:
  ~MediaEngineWebRTCMicrophoneSource() { Shutdown(); }

private:
  void Init();

  webrtc::VoiceEngine* mVoiceEngine;
  RefPtr<mozilla::AudioInput> mAudioInput;
  RefPtr<WebRTCAudioDataListener> mListener;

  ScopedCustomReleasePtr<webrtc::VoEBase> mVoEBase;
  ScopedCustomReleasePtr<webrtc::VoEExternalMedia> mVoERender;
  ScopedCustomReleasePtr<webrtc::VoENetwork> mVoENetwork;
  ScopedCustomReleasePtr<webrtc::VoEAudioProcessing> mVoEProcessing;

  nsAutoPtr<AudioPacketizer<AudioDataValue, int16_t>> mPacketizer;

  // mMonitor protects mSources[] access/changes, and transitions of mState
  // from kStarted to kStopped (which are combined with EndTrack()).
  // mSources[] is accessed from webrtc threads.
  Monitor mMonitor;
  nsTArray<RefPtr<SourceMediaStream>> mSources;
  nsCOMPtr<nsIThread> mThread;
  int mCapIndex;
  int mChannel;
  int mNrAllocations; // When this becomes 0, we shut down HW
  TrackID mTrackID;
  bool mInitDone;
  bool mStarted;

  nsString mDeviceName;
  nsCString mDeviceUUID;

  uint32_t mSampleFrequency;
  bool mEchoOn, mAgcOn, mNoiseOn;
  webrtc::EcModes  mEchoCancel;
  webrtc::AgcModes mAGC;
  webrtc::NsModes  mNoiseSuppress;
  int32_t mPlayoutDelay;

  NullTransport *mNullTransport;

  // For full_duplex packetizer output
  size_t mInputBufferLen;
  UniquePtr<int16_t[]> mInputBuffer;
};

class MediaEngineWebRTC : public MediaEngine
{
public:
  explicit MediaEngineWebRTC(MediaEnginePrefs& aPrefs);

  // Clients should ensure to clean-up sources video/audio sources
  // before invoking Shutdown on this class.
  void Shutdown() override;

  void EnumerateVideoDevices(dom::MediaSourceEnum,
                             nsTArray<RefPtr<MediaEngineVideoSource>>*) override;
  void EnumerateAudioDevices(dom::MediaSourceEnum,
                             nsTArray<RefPtr<MediaEngineAudioSource>>*) override;
private:
  ~MediaEngineWebRTC() {
    Shutdown();
#if defined(MOZ_B2G_CAMERA) && defined(MOZ_WIDGET_GONK)
    AsyncLatencyLogger::Get()->Release();
#endif
    gFarendObserver = nullptr;
  }

  nsCOMPtr<nsIThread> mThread;

  // gUM runnables can e.g. Enumerate from multiple threads
  Mutex mMutex;
  webrtc::VoiceEngine* mVoiceEngine;
  webrtc::Config mConfig;
  RefPtr<mozilla::AudioInput> mAudioInput;
  bool mAudioEngineInit;
  bool mFullDuplex;
  bool mExtendedFilter;
  bool mDelayAgnostic;
  bool mHasTabVideoSource;

  // Store devices we've already seen in a hashtable for quick return.
  // Maps UUID to MediaEngineSource (one set for audio, one for video).
  nsRefPtrHashtable<nsStringHashKey, MediaEngineVideoSource> mVideoSources;
  nsRefPtrHashtable<nsStringHashKey, MediaEngineAudioSource> mAudioSources;
};

}

#endif /* NSMEDIAENGINEWEBRTC_H_ */
