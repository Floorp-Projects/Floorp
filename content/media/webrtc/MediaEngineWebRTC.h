/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINEWEBRTC_H_
#define MEDIAENGINEWEBRTC_H_

#include "prcvar.h"
#include "prthread.h"
#include "nsIThread.h"
#include "nsIRunnable.h"

#include "mozilla/Mutex.h"
#include "mozilla/Monitor.h"
#include "nsCOMPtr.h"
#include "nsDOMFile.h"
#include "nsThreadUtils.h"
#include "DOMMediaStream.h"
#include "nsDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"
#include "nsRefPtrHashtable.h"

#include "VideoUtils.h"
#include "MediaEngine.h"
#include "VideoSegment.h"
#include "AudioSegment.h"
#include "StreamBuffer.h"
#include "MediaStreamGraph.h"
#include "LoadMonitor.h"

// WebRTC library includes follow

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
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_render.h"
#include "webrtc/video_engine/include/vie_capture.h"
#ifdef MOZ_B2G_CAMERA
#include "CameraControlListener.h"
#include "ICameraControl.h"
#include "ImageContainer.h"
#include "nsGlobalWindow.h"
#include "prprf.h"
#endif

#include "NullTransport.h"

namespace mozilla {

#ifdef MOZ_B2G_CAMERA
class CameraAllocateRunnable;
class GetCameraNameRunnable;
#endif

/**
 * The WebRTC implementation of the MediaEngine interface.
 *
 * On B2G platform, member data may accessed from different thread after construction:
 *
 * MediaThread:
 *   mState, mImage, mWidth, mHeight, mCapability, mPrefs, mDeviceName, mUniqueId, mInitDone,
 *   mSources, mImageContainer, mSources, mState, mImage, mLastCapture
 *
 * MainThread:
 *   mCaptureIndex, mWindowId,
 *   mNativeCameraControl, mPreviewStream, mState, mLastCapture, mWidth, mHeight
 *
 * Where mWidth, mHeight, mImage are protected by mMonitor
 *       mState, mLastCapture is protected by mCallbackMonitor
 * Other variable is accessed only from single thread
 */
class MediaEngineWebRTCVideoSource : public MediaEngineVideoSource
                                   , public nsRunnable
#ifdef MOZ_B2G_CAMERA
                                   , public CameraControlListener
#else
                                   , public webrtc::ExternalRenderer
#endif
{
public:
#ifdef MOZ_B2G_CAMERA
  MediaEngineWebRTCVideoSource(int aIndex)
    : mCameraControl(nullptr)
    , mCallbackMonitor("WebRTCCamera.CallbackMonitor")
    , mSensorAngle(0)
    , mCaptureIndex(aIndex)
    , mMonitor("WebRTCCamera.Monitor")
    , mWidth(0)
    , mHeight(0)
    , mInitDone(false)
    , mInSnapshotMode(false)
    , mSnapshotPath(nullptr)
  {
    mState = kReleased;
    Init();
  }
#else
  // ViEExternalRenderer.
  virtual int FrameSizeChange(unsigned int, unsigned int, unsigned int);
  virtual int DeliverFrame(unsigned char*,int, uint32_t , int64_t,
                           void *handle);
  /**
   * Does DeliverFrame() support a null buffer and non-null handle
   * (video texture)?
   * XXX Investigate!  Especially for Android/B2G
   */
  virtual bool IsTextureSupported() { return false; }

  MediaEngineWebRTCVideoSource(webrtc::VideoEngine* aVideoEnginePtr, int aIndex)
    : mVideoEngine(aVideoEnginePtr)
    , mCaptureIndex(aIndex)
    , mFps(-1)
    , mMinFps(-1)
    , mMonitor("WebRTCCamera.Monitor")
    , mWidth(0)
    , mHeight(0)
    , mInitDone(false)
    , mInSnapshotMode(false)
    , mSnapshotPath(nullptr) {
    MOZ_ASSERT(aVideoEnginePtr);
    mState = kReleased;
    Init();
  }
#endif

  ~MediaEngineWebRTCVideoSource() { Shutdown(); }

  virtual void GetName(nsAString&);
  virtual void GetUUID(nsAString&);
  virtual nsresult Allocate(const MediaEnginePrefs &aPrefs);
  virtual nsresult Deallocate();
  virtual nsresult Start(SourceMediaStream*, TrackID);
  virtual nsresult Stop(SourceMediaStream*, TrackID);
  virtual nsresult Snapshot(uint32_t aDuration, nsIDOMFile** aFile);
  virtual nsresult Config(bool aEchoOn, uint32_t aEcho,
                          bool aAgcOn, uint32_t aAGC,
                          bool aNoiseOn, uint32_t aNoise) { return NS_OK; };
  virtual void NotifyPull(MediaStreamGraph* aGraph,
                          SourceMediaStream *aSource,
                          TrackID aId,
                          StreamTime aDesiredTime,
                          TrackTicks &aLastEndTime);

  virtual bool IsFake() {
    return false;
  }

#ifndef MOZ_B2G_CAMERA
  NS_DECL_THREADSAFE_ISUPPORTS
#else
  // We are subclassed from CameraControlListener, which implements a
  // threadsafe reference-count for us.
  NS_DECL_ISUPPORTS_INHERITED

  void OnHardwareStateChange(HardwareState aState);
  bool OnNewPreviewFrame(layers::Image* aImage, uint32_t aWidth, uint32_t aHeight);
  void OnError(CameraErrorContext aContext, CameraError aError);
  void OnTakePictureComplete(uint8_t* aData, uint32_t aLength, const nsAString& aMimeType);

  void AllocImpl();
  void DeallocImpl();
  void StartImpl(webrtc::CaptureCapability aCapability);
  void StopImpl();
  void SnapshotImpl();
  void RotateImage(layers::Image* aImage);
#endif

  // This runnable is for creating a temporary file on the main thread.
  NS_IMETHODIMP
  Run()
  {
    nsCOMPtr<nsIFile> tmp;
    nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tmp));
    NS_ENSURE_SUCCESS(rv, rv);

    tmp->Append(NS_LITERAL_STRING("webrtc_snapshot.jpeg"));
    rv = tmp->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
    NS_ENSURE_SUCCESS(rv, rv);

    mSnapshotPath = new nsString();
    rv = tmp->GetPath(*mSnapshotPath);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

private:
  static const unsigned int KMaxDeviceNameLength = 128;
  static const unsigned int KMaxUniqueIdLength = 256;

  // Initialize the needed Video engine interfaces.
  void Init();
  void Shutdown();

  // Engine variables.
#ifdef MOZ_B2G_CAMERA
  nsRefPtr<ICameraControl> mCameraControl;
  mozilla::ReentrantMonitor mCallbackMonitor; // Monitor for camera callback handling
  nsRefPtr<nsIDOMFile> mLastCapture;
  int mSensorAngle;
#else
  webrtc::VideoEngine* mVideoEngine; // Weak reference, don't free.
  webrtc::ViEBase* mViEBase;
  webrtc::ViECapture* mViECapture;
  webrtc::ViERender* mViERender;
#endif
  webrtc::CaptureCapability mCapability; // Doesn't work on OS X.

  int mCaptureIndex;
  int mFps; // Track rate (30 fps by default)
  int mMinFps; // Min rate we want to accept

  // mMonitor protects mImage access/changes, and transitions of mState
  // from kStarted to kStopped (which are combined with EndTrack() and
  // image changes).  Note that mSources is not accessed from other threads
  // for video and is not protected.
  Monitor mMonitor; // Monitor for processing WebRTC frames.
  int mWidth, mHeight;
  nsRefPtr<layers::Image> mImage;
  nsRefPtr<layers::ImageContainer> mImageContainer;

  nsTArray<SourceMediaStream *> mSources; // When this goes empty, we shut down HW

  bool mInitDone;
  bool mInSnapshotMode;
  nsString* mSnapshotPath;

  nsString mDeviceName;
  nsString mUniqueId;

  void ChooseCapability(const MediaEnginePrefs &aPrefs);
};

class MediaEngineWebRTCAudioSource : public MediaEngineAudioSource,
                                     public webrtc::VoEMediaProcess
{
public:
  MediaEngineWebRTCAudioSource(webrtc::VoiceEngine* aVoiceEnginePtr, int aIndex,
    const char* name, const char* uuid)
    : mVoiceEngine(aVoiceEnginePtr)
    , mMonitor("WebRTCMic.Monitor")
    , mCapIndex(aIndex)
    , mChannel(-1)
    , mInitDone(false)
    , mEchoOn(false), mAgcOn(false), mNoiseOn(false)
    , mEchoCancel(webrtc::kEcDefault)
    , mAGC(webrtc::kAgcDefault)
    , mNoiseSuppress(webrtc::kNsDefault)
    , mNullTransport(nullptr) {
    MOZ_ASSERT(aVoiceEnginePtr);
    mState = kReleased;
    mDeviceName.Assign(NS_ConvertUTF8toUTF16(name));
    mDeviceUUID.Assign(NS_ConvertUTF8toUTF16(uuid));
    Init();
  }
  ~MediaEngineWebRTCAudioSource() { Shutdown(); }

  virtual void GetName(nsAString&);
  virtual void GetUUID(nsAString&);

  virtual nsresult Allocate(const MediaEnginePrefs &aPrefs);
  virtual nsresult Deallocate();
  virtual nsresult Start(SourceMediaStream*, TrackID);
  virtual nsresult Stop(SourceMediaStream*, TrackID);
  virtual nsresult Snapshot(uint32_t aDuration, nsIDOMFile** aFile);
  virtual nsresult Config(bool aEchoOn, uint32_t aEcho,
                          bool aAgcOn, uint32_t aAGC,
                          bool aNoiseOn, uint32_t aNoise);

  virtual void NotifyPull(MediaStreamGraph* aGraph,
                          SourceMediaStream *aSource,
                          TrackID aId,
                          StreamTime aDesiredTime,
                          TrackTicks &aLastEndTime);

  virtual bool IsFake() {
    return false;
  }

  // VoEMediaProcess.
  void Process(int channel, webrtc::ProcessingTypes type,
               int16_t audio10ms[], int length,
               int samplingFreq, bool isStereo);

  NS_DECL_THREADSAFE_ISUPPORTS

private:
  static const unsigned int KMaxDeviceNameLength = 128;
  static const unsigned int KMaxUniqueIdLength = 256;

  void Init();
  void Shutdown();

  webrtc::VoiceEngine* mVoiceEngine;
  webrtc::VoEBase* mVoEBase;
  webrtc::VoEExternalMedia* mVoERender;
  webrtc::VoENetwork*  mVoENetwork;
  webrtc::VoEAudioProcessing *mVoEProcessing;

  // mMonitor protects mSources[] access/changes, and transitions of mState
  // from kStarted to kStopped (which are combined with EndTrack()).
  // mSources[] is accessed from webrtc threads.
  Monitor mMonitor;
  nsTArray<SourceMediaStream *> mSources; // When this goes empty, we shut down HW

  int mCapIndex;
  int mChannel;
  TrackID mTrackID;
  bool mInitDone;

  nsString mDeviceName;
  nsString mDeviceUUID;

  bool mEchoOn, mAgcOn, mNoiseOn;
  webrtc::EcModes  mEchoCancel;
  webrtc::AgcModes mAGC;
  webrtc::NsModes  mNoiseSuppress;

  NullTransport *mNullTransport;
};

class MediaEngineWebRTC : public MediaEngine
{
public:
  MediaEngineWebRTC(MediaEnginePrefs &aPrefs);
  ~MediaEngineWebRTC() {
    Shutdown();
#ifdef MOZ_B2G_CAMERA
    AsyncLatencyLogger::Get()->Release();
#endif
  }

  // Clients should ensure to clean-up sources video/audio sources
  // before invoking Shutdown on this class.
  void Shutdown();

  virtual void EnumerateVideoDevices(nsTArray<nsRefPtr<MediaEngineVideoSource> >*);
  virtual void EnumerateAudioDevices(nsTArray<nsRefPtr<MediaEngineAudioSource> >*);

private:
  Mutex mMutex;
  // protected with mMutex:

  webrtc::VideoEngine* mVideoEngine;
  webrtc::VoiceEngine* mVoiceEngine;

  // Need this to avoid unneccesary WebRTC calls while enumerating.
  bool mVideoEngineInit;
  bool mAudioEngineInit;
  bool mHasTabVideoSource;

  // Store devices we've already seen in a hashtable for quick return.
  // Maps UUID to MediaEngineSource (one set for audio, one for video).
  nsRefPtrHashtable<nsStringHashKey, MediaEngineWebRTCVideoSource > mVideoSources;
  nsRefPtrHashtable<nsStringHashKey, MediaEngineWebRTCAudioSource > mAudioSources;

  nsRefPtr<LoadMonitor> mLoadMonitor;
};

}

#endif /* NSMEDIAENGINEWEBRTC_H_ */
