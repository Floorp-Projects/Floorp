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

// WebRTC library includes follow

// Audio Engine
#include "voice_engine/include/voe_base.h"
#include "voice_engine/include/voe_codec.h"
#include "voice_engine/include/voe_hardware.h"
#include "voice_engine/include/voe_network.h"
#include "voice_engine/include/voe_audio_processing.h"
#include "voice_engine/include/voe_volume_control.h"
#include "voice_engine/include/voe_external_media.h"
#include "voice_engine/include/voe_audio_processing.h"

// Video Engine
#include "video_engine/include/vie_base.h"
#include "video_engine/include/vie_codec.h"
#include "video_engine/include/vie_render.h"
#include "video_engine/include/vie_capture.h"
#include "video_engine/include/vie_file.h"
#ifdef MOZ_B2G_CAMERA
#include "CameraPreviewMediaStream.h"
#include "DOMCameraManager.h"
#include "GonkCameraControl.h"
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
 *   mDOMCameraControl, mCaptureIndex, mCameraThread, mWindowId, mCameraManager,
 *   mNativeCameraControl, mPreviewStream, mState, mLastCapture, mWidth, mHeight
 *
 * Where mWidth, mHeight, mImage are protected by mMonitor
 *       mState, mLastCapture is protected by mCallbackMonitor
 * Other variable is accessed only from single thread
 */
class MediaEngineWebRTCVideoSource : public MediaEngineVideoSource
                                   , public nsRunnable
#ifdef MOZ_B2G_CAMERA
                                   , public nsICameraGetCameraCallback
                                   , public nsICameraPreviewStreamCallback
                                   , public nsICameraTakePictureCallback
                                   , public nsICameraReleaseCallback
                                   , public nsICameraErrorCallback
                                   , public CameraPreviewFrameCallback
#else
                                   , public webrtc::ExternalRenderer
#endif
{
public:
#ifdef MOZ_B2G_CAMERA
  MediaEngineWebRTCVideoSource(nsDOMCameraManager* aCameraManager,
    int aIndex, uint64_t aWindowId)
    : mCameraManager(aCameraManager)
    , mNativeCameraControl(nullptr)
    , mPreviewStream(nullptr)
    , mWindowId(aWindowId)
    , mCallbackMonitor("WebRTCCamera.CallbackMonitor")
    , mCaptureIndex(aIndex)
    , mMonitor("WebRTCCamera.Monitor")
    , mWidth(0)
    , mHeight(0)
    , mInitDone(false)
    , mInSnapshotMode(false)
    , mSnapshotPath(nullptr)
  {
    mState = kReleased;
    NS_NewNamedThread("CameraThread", getter_AddRefs(mCameraThread), nullptr);
    Init();
  }
#else
  // ViEExternalRenderer.
  virtual int FrameSizeChange(unsigned int, unsigned int, unsigned int);
  virtual int DeliverFrame(unsigned char*, int, uint32_t, int64_t);

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
    , mSnapshotPath(NULL) {
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

  NS_DECL_THREADSAFE_ISUPPORTS
#ifdef MOZ_B2G_CAMERA
  NS_DECL_NSICAMERAGETCAMERACALLBACK
  NS_DECL_NSICAMERAPREVIEWSTREAMCALLBACK
  NS_DECL_NSICAMERATAKEPICTURECALLBACK
  NS_DECL_NSICAMERARELEASECALLBACK
  NS_DECL_NSICAMERAERRORCALLBACK

  void AllocImpl();
  void DeallocImpl();
  void StartImpl(webrtc::CaptureCapability aCapability);
  void StopImpl();
  void SnapshotImpl();

  virtual void OnNewFrame(const gfxIntSize& aIntrinsicSize, layers::Image* aImage);

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
  // MediaEngine hold this DOM object, and the MediaEngine is hold by Navigator
  // Their life time is always much longer than this object. Use a raw-pointer
  // here should be safe.
  // We need raw pointer here since such DOM-object should not addref/release on
  // any thread other than main thread, but we must use this object for now. To
  // avoid any bad thing do to addref/release DOM-object on other thread, we use
  // raw-pointer for now.
  nsDOMCameraManager* mCameraManager;
  nsRefPtr<nsDOMCameraControl> mDOMCameraControl;
  nsRefPtr<nsGonkCameraControl> mNativeCameraControl;
  nsRefPtr<DOMCameraPreview> mPreviewStream;
  uint64_t mWindowId;
  mozilla::ReentrantMonitor mCallbackMonitor; // Monitor for camera callback handling
  nsRefPtr<nsIThread> mCameraThread;
  nsRefPtr<nsIDOMFile> mLastCapture;
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

  // These are in UTF-8 but webrtc api uses char arrays
  char mDeviceName[KMaxDeviceNameLength];
  char mUniqueId[KMaxUniqueIdLength];

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
  void Process(const int channel, const webrtc::ProcessingTypes type,
               WebRtc_Word16 audio10ms[], const int length,
               const int samplingFreq, const bool isStereo);

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
#ifdef MOZ_B2G_CAMERA
  MediaEngineWebRTC(nsDOMCameraManager* aCameraManager, uint64_t aWindowId)
    : mMutex("mozilla::MediaEngineWebRTC")
    , mVideoEngine(nullptr)
    , mVoiceEngine(nullptr)
    , mVideoEngineInit(false)
    , mAudioEngineInit(false)
    , mCameraManager(aCameraManager)
    , mWindowId(aWindowId)
  {
	mVideoSources.Init();
	mAudioSources.Init();
  }
#else
  MediaEngineWebRTC()
    : mMutex("mozilla::MediaEngineWebRTC")
    , mVideoEngine(nullptr)
    , mVoiceEngine(nullptr)
    , mVideoEngineInit(false)
    , mAudioEngineInit(false)
  {
    mVideoSources.Init();
    mAudioSources.Init();
  }
#endif
  ~MediaEngineWebRTC() { Shutdown(); }

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

  // Store devices we've already seen in a hashtable for quick return.
  // Maps UUID to MediaEngineSource (one set for audio, one for video).
  nsRefPtrHashtable<nsStringHashKey, MediaEngineWebRTCVideoSource > mVideoSources;
  nsRefPtrHashtable<nsStringHashKey, MediaEngineWebRTCAudioSource > mAudioSources;

#ifdef MOZ_B2G_CAMERA
  // MediaEngine hold this DOM object, and the MediaEngine is hold by Navigator
  // Their life time is always much longer than this object. Use a raw-pointer
  // here should be safe.
  // We need raw pointer here since such DOM-object should not addref/release on
  // any thread other than main thread, but we must use this object for now. To
  // avoid any bad thing do to addref/release DOM-object on other thread, we use
  // raw-pointer for now.
  nsDOMCameraManager* mCameraManager;
  uint64_t mWindowId;
#endif
};

}

#endif /* NSMEDIAENGINEWEBRTC_H_ */
