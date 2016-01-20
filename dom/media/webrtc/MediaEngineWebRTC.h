/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINEWEBRTC_H_
#define MEDIAENGINEWEBRTC_H_

#include "prcvar.h"
#include "prthread.h"
#include "nsIThread.h"
#include "nsIRunnable.h"

#include "mozilla/dom/File.h"
#include "mozilla/Mutex.h"
#include "mozilla/Monitor.h"
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
                    const nsString& aDeviceId) override
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
  nsresult Config(bool aEchoOn, uint32_t aEcho, bool aAgcOn,
                  uint32_t aAGC, bool aNoiseOn, uint32_t aNoise,
                  int32_t aPlayoutDelay) override
  {
    return NS_OK;
  }
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

class MediaEngineWebRTCMicrophoneSource : public MediaEngineAudioSource,
                                          public webrtc::VoEMediaProcess,
                                          private MediaConstraintsHelper
{
public:
  MediaEngineWebRTCMicrophoneSource(nsIThread* aThread,
                                    webrtc::VoiceEngine* aVoiceEnginePtr,
                                    int aIndex,
                                    const char* name,
                                    const char* uuid)
    : MediaEngineAudioSource(kReleased)
    , mVoiceEngine(aVoiceEnginePtr)
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
    , mNullTransport(nullptr) {
    MOZ_ASSERT(aVoiceEnginePtr);
    mDeviceName.Assign(NS_ConvertUTF8toUTF16(name));
    mDeviceUUID.Assign(uuid);
    Init();
  }

  void GetName(nsAString& aName) override;
  void GetUUID(nsACString& aUUID) override;

  nsresult Allocate(const dom::MediaTrackConstraints& aConstraints,
                    const MediaEnginePrefs& aPrefs,
                    const nsString& aDeviceId) override;
  nsresult Deallocate() override;
  nsresult Start(SourceMediaStream* aStream, TrackID aID) override;
  nsresult Stop(SourceMediaStream* aSource, TrackID aID) override;
  nsresult Restart(const dom::MediaTrackConstraints& aConstraints,
                   const MediaEnginePrefs &aPrefs,
                   const nsString& aDeviceId) override;
  void SetDirectListeners(bool aHasDirectListeners) override {};
  nsresult Config(bool aEchoOn, uint32_t aEcho,
                  bool aAgcOn, uint32_t aAGC,
                  bool aNoiseOn, uint32_t aNoise,
                  int32_t aPlayoutDelay) override;

  void NotifyPull(MediaStreamGraph* aGraph,
                  SourceMediaStream* aSource,
                  TrackID aId,
                  StreamTime aDesiredTime) override;

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
  ScopedCustomReleasePtr<webrtc::VoEBase> mVoEBase;
  ScopedCustomReleasePtr<webrtc::VoEExternalMedia> mVoERender;
  ScopedCustomReleasePtr<webrtc::VoENetwork> mVoENetwork;
  ScopedCustomReleasePtr<webrtc::VoEAudioProcessing> mVoEProcessing;

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
  bool mAudioEngineInit;

  bool mHasTabVideoSource;

  // Store devices we've already seen in a hashtable for quick return.
  // Maps UUID to MediaEngineSource (one set for audio, one for video).
  nsRefPtrHashtable<nsStringHashKey, MediaEngineVideoSource> mVideoSources;
  nsRefPtrHashtable<nsStringHashKey, MediaEngineAudioSource> mAudioSources;
};

}

#endif /* NSMEDIAENGINEWEBRTC_H_ */
