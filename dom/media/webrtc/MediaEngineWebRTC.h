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

#include "NullTransport.h"
#include "AudioOutputObserver.h"

namespace mozilla {

/**
 * The WebRTC implementation of the MediaEngine interface.
 */
class MediaEngineWebRTCVideoSource : public MediaEngineCameraVideoSource
                                   , public webrtc::ExternalRenderer
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  // ViEExternalRenderer.
  virtual int FrameSizeChange(unsigned int w, unsigned int h, unsigned int streams) MOZ_OVERRIDE;
  virtual int DeliverFrame(unsigned char* buffer,
                           int size,
                           uint32_t time_stamp,
                           int64_t ntp_time_ms,
                           int64_t render_time,
                           void *handle) MOZ_OVERRIDE;
  /**
   * Does DeliverFrame() support a null buffer and non-null handle
   * (video texture)?
   * XXX Investigate!  Especially for Android/B2G
   */
  virtual bool IsTextureSupported() MOZ_OVERRIDE { return false; }

  MediaEngineWebRTCVideoSource(webrtc::VideoEngine* aVideoEnginePtr, int aIndex,
                               dom::MediaSourceEnum aMediaSource = dom::MediaSourceEnum::Camera)
    : MediaEngineCameraVideoSource(aIndex, "WebRTCCamera.Monitor")
    , mVideoEngine(aVideoEnginePtr)
    , mMinFps(-1)
    , mMediaSource(aMediaSource)
  {
    MOZ_ASSERT(aVideoEnginePtr);
    Init();
  }

  virtual nsresult Allocate(const dom::MediaTrackConstraints& aConstraints,
                            const MediaEnginePrefs& aPrefs) MOZ_OVERRIDE;
  virtual nsresult Deallocate() MOZ_OVERRIDE;
  virtual nsresult Start(SourceMediaStream*, TrackID) MOZ_OVERRIDE;
  virtual nsresult Stop(SourceMediaStream*, TrackID) MOZ_OVERRIDE;
  virtual void NotifyPull(MediaStreamGraph* aGraph,
                          SourceMediaStream* aSource,
                          TrackID aId,
                          StreamTime aDesiredTime) MOZ_OVERRIDE;

  virtual const dom::MediaSourceEnum GetMediaSource() MOZ_OVERRIDE {
    return mMediaSource;
  }
  virtual nsresult TakePhoto(PhotoCallback* aCallback) MOZ_OVERRIDE
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  void Refresh(int aIndex);

protected:
  ~MediaEngineWebRTCVideoSource() { Shutdown(); }

private:
  // Initialize the needed Video engine interfaces.
  void Init();
  void Shutdown();

  // Engine variables.
  webrtc::VideoEngine* mVideoEngine; // Weak reference, don't free.
  webrtc::ViEBase* mViEBase;
  webrtc::ViECapture* mViECapture;
  webrtc::ViERender* mViERender;

  int mMinFps; // Min rate we want to accept
  dom::MediaSourceEnum mMediaSource; // source of media (camera | application | screen)

  size_t NumCapabilities() MOZ_OVERRIDE;
  void GetCapability(size_t aIndex, webrtc::CaptureCapability& aOut) MOZ_OVERRIDE;
};

class MediaEngineWebRTCAudioSource : public MediaEngineAudioSource,
                                     public webrtc::VoEMediaProcess
{
public:
  MediaEngineWebRTCAudioSource(nsIThread* aThread, webrtc::VoiceEngine* aVoiceEnginePtr,
                               int aIndex, const char* name, const char* uuid)
    : MediaEngineAudioSource(kReleased)
    , mSamples(0)
    , mVoiceEngine(aVoiceEnginePtr)
    , mMonitor("WebRTCMic.Monitor")
    , mThread(aThread)
    , mCapIndex(aIndex)
    , mChannel(-1)
    , mInitDone(false)
    , mStarted(false)
    , mEchoOn(false), mAgcOn(false), mNoiseOn(false)
    , mEchoCancel(webrtc::kEcDefault)
    , mAGC(webrtc::kAgcDefault)
    , mNoiseSuppress(webrtc::kNsDefault)
    , mPlayoutDelay(0)
    , mNullTransport(nullptr) {
    MOZ_ASSERT(aVoiceEnginePtr);
    mDeviceName.Assign(NS_ConvertUTF8toUTF16(name));
    mDeviceUUID.Assign(NS_ConvertUTF8toUTF16(uuid));
    Init();
  }

  virtual void GetName(nsAString& aName) MOZ_OVERRIDE;
  virtual void GetUUID(nsAString& aUUID) MOZ_OVERRIDE;

  virtual nsresult Allocate(const dom::MediaTrackConstraints& aConstraints,
                            const MediaEnginePrefs& aPrefs) MOZ_OVERRIDE;
  virtual nsresult Deallocate() MOZ_OVERRIDE;
  virtual nsresult Start(SourceMediaStream* aStream, TrackID aID) MOZ_OVERRIDE;
  virtual nsresult Stop(SourceMediaStream* aSource, TrackID aID) MOZ_OVERRIDE;
  virtual void SetDirectListeners(bool aHasDirectListeners) MOZ_OVERRIDE {};
  virtual nsresult Config(bool aEchoOn, uint32_t aEcho,
                          bool aAgcOn, uint32_t aAGC,
                          bool aNoiseOn, uint32_t aNoise,
                          int32_t aPlayoutDelay) MOZ_OVERRIDE;

  virtual void NotifyPull(MediaStreamGraph* aGraph,
                          SourceMediaStream* aSource,
                          TrackID aId,
                          StreamTime aDesiredTime) MOZ_OVERRIDE;

  virtual bool IsFake() MOZ_OVERRIDE {
    return false;
  }

  virtual const dom::MediaSourceEnum GetMediaSource() MOZ_OVERRIDE {
    return dom::MediaSourceEnum::Microphone;
  }

  virtual nsresult TakePhoto(PhotoCallback* aCallback) MOZ_OVERRIDE
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // VoEMediaProcess.
  void Process(int channel, webrtc::ProcessingTypes type,
               int16_t audio10ms[], int length,
               int samplingFreq, bool isStereo) MOZ_OVERRIDE;

  NS_DECL_THREADSAFE_ISUPPORTS

protected:
  ~MediaEngineWebRTCAudioSource() { Shutdown(); }

  // mSamples is an int to avoid conversions when comparing/etc to
  // samplingFreq & length. Making mSamples protected instead of private is a
  // silly way to avoid -Wunused-private-field warnings when PR_LOGGING is not
  // #defined. mSamples is not actually expected to be used by a derived class.
  int mSamples;

private:
  void Init();
  void Shutdown();

  webrtc::VoiceEngine* mVoiceEngine;
  ScopedCustomReleasePtr<webrtc::VoEBase> mVoEBase;
  ScopedCustomReleasePtr<webrtc::VoEExternalMedia> mVoERender;
  ScopedCustomReleasePtr<webrtc::VoENetwork> mVoENetwork;
  ScopedCustomReleasePtr<webrtc::VoEAudioProcessing> mVoEProcessing;

  // mMonitor protects mSources[] access/changes, and transitions of mState
  // from kStarted to kStopped (which are combined with EndTrack()).
  // mSources[] is accessed from webrtc threads.
  Monitor mMonitor;
  nsTArray<SourceMediaStream*> mSources; // When this goes empty, we shut down HW
  nsCOMPtr<nsIThread> mThread;
  int mCapIndex;
  int mChannel;
  TrackID mTrackID;
  bool mInitDone;
  bool mStarted;

  nsString mDeviceName;
  nsString mDeviceUUID;

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
  void Shutdown();

  virtual void EnumerateVideoDevices(dom::MediaSourceEnum,
                                    nsTArray<nsRefPtr<MediaEngineVideoSource> >*);
  virtual void EnumerateAudioDevices(dom::MediaSourceEnum,
                                    nsTArray<nsRefPtr<MediaEngineAudioSource> >*);
private:
  ~MediaEngineWebRTC() {
    Shutdown();
#ifdef MOZ_B2G_CAMERA
    AsyncLatencyLogger::Get()->Release();
#endif
    gFarendObserver = nullptr;
  }

  nsCOMPtr<nsIThread> mThread;

  Mutex mMutex;

  // protected with mMutex:
  webrtc::VideoEngine* mScreenEngine;
  webrtc::VideoEngine* mBrowserEngine;
  webrtc::VideoEngine* mWinEngine;
  webrtc::VideoEngine* mAppEngine;
  webrtc::VideoEngine* mVideoEngine;
  webrtc::VoiceEngine* mVoiceEngine;

  // specialized configurations
  webrtc::Config mAppEngineConfig;
  webrtc::Config mWinEngineConfig;
  webrtc::Config mScreenEngineConfig;
  webrtc::Config mBrowserEngineConfig;

  // Need this to avoid unneccesary WebRTC calls while enumerating.
  bool mVideoEngineInit;
  bool mAudioEngineInit;
  bool mScreenEngineInit;
  bool mBrowserEngineInit;
  bool mWinEngineInit;
  bool mAppEngineInit;
  bool mHasTabVideoSource;

  // Store devices we've already seen in a hashtable for quick return.
  // Maps UUID to MediaEngineSource (one set for audio, one for video).
  nsRefPtrHashtable<nsStringHashKey, MediaEngineVideoSource> mVideoSources;
  nsRefPtrHashtable<nsStringHashKey, MediaEngineWebRTCAudioSource> mAudioSources;
};

}

#endif /* NSMEDIAENGINEWEBRTC_H_ */
