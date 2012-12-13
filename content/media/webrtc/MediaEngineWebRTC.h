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
#include "nsCOMPtr.h"
#include "nsDOMFile.h"
#include "nsThreadUtils.h"
#include "nsDOMMediaStream.h"
#include "nsDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"

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

// Video Engine
#include "video_engine/include/vie_base.h"
#include "video_engine/include/vie_codec.h"
#include "video_engine/include/vie_render.h"
#include "video_engine/include/vie_capture.h"
#include "video_engine/include/vie_file.h"

#include "NullTransport.h"

namespace mozilla {

/**
 * The WebRTC implementation of the MediaEngine interface.
 */
class MediaEngineWebRTCVideoSource : public MediaEngineVideoSource,
                                     public webrtc::ExternalRenderer,
                                     public nsRunnable
{
public:
  static const int DEFAULT_VIDEO_FPS = 60;
  static const int DEFAULT_MIN_VIDEO_FPS = 10;

  // ViEExternalRenderer.
  virtual int FrameSizeChange(unsigned int, unsigned int, unsigned int);
  virtual int DeliverFrame(unsigned char*, int, uint32_t, int64_t);

  MediaEngineWebRTCVideoSource(webrtc::VideoEngine* aVideoEnginePtr,
    int aIndex, int aMinFps = DEFAULT_MIN_VIDEO_FPS)
    : mVideoEngine(aVideoEnginePtr)
    , mCaptureIndex(aIndex)
    , mCapabilityChosen(false)
    , mWidth(640)
    , mHeight(480)
    , mLastEndTime(0)
    , mMonitor("WebRTCCamera.Monitor")
    , mFps(DEFAULT_VIDEO_FPS)
    , mMinFps(aMinFps)
    , mInitDone(false)
    , mInSnapshotMode(false)
    , mSnapshotPath(NULL) {
    MOZ_ASSERT(aVideoEnginePtr);
    mState = kReleased;
    Init();
  }
  ~MediaEngineWebRTCVideoSource() { Shutdown(); }

  virtual void GetName(nsAString&);
  virtual void GetUUID(nsAString&);
  virtual const MediaEngineVideoOptions *GetOptions();
  virtual nsresult Allocate();
  virtual nsresult Deallocate();
  virtual nsresult Start(SourceMediaStream*, TrackID);
  virtual nsresult Stop();
  virtual nsresult Snapshot(uint32_t aDuration, nsIDOMFile** aFile);
  virtual void NotifyPull(MediaStreamGraph* aGraph, StreamTime aDesiredTime);

  NS_DECL_ISUPPORTS

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

  webrtc::VideoEngine* mVideoEngine; // Weak reference, don't free.
  webrtc::ViEBase* mViEBase;
  webrtc::ViECapture* mViECapture;
  webrtc::ViERender* mViERender;
  webrtc::CaptureCapability mCapability; // Doesn't work on OS X.

  int mCaptureIndex;
  bool mCapabilityChosen;
  int mWidth, mHeight;
  TrackID mTrackID;
  TrackTicks mLastEndTime;

  mozilla::ReentrantMonitor mMonitor; // Monitor for processing WebRTC frames.
  SourceMediaStream* mSource;

  int mFps; // Track rate (30 fps by default)
  int mMinFps; // Min rate we want to accept
  bool mInitDone;
  bool mInSnapshotMode;
  nsString* mSnapshotPath;

  nsRefPtr<layers::Image> mImage;
  nsRefPtr<layers::ImageContainer> mImageContainer;

  PRLock* mSnapshotLock;
  PRCondVar* mSnapshotCondVar;

  // These are in UTF-8 but webrtc api uses char arrays
  char mDeviceName[KMaxDeviceNameLength];
  char mUniqueId[KMaxUniqueIdLength];

  void ChooseCapability(uint32_t aWidth, uint32_t aHeight, uint32_t aMinFPS);
  MediaEngineVideoOptions mOpts;
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

  virtual nsresult Allocate();
  virtual nsresult Deallocate();
  virtual nsresult Start(SourceMediaStream*, TrackID);
  virtual nsresult Stop();
  virtual nsresult Snapshot(uint32_t aDuration, nsIDOMFile** aFile);
  virtual void NotifyPull(MediaStreamGraph* aGraph, StreamTime aDesiredTime);

  // VoEMediaProcess.
  void Process(const int channel, const webrtc::ProcessingTypes type,
               WebRtc_Word16 audio10ms[], const int length,
               const int samplingFreq, const bool isStereo);

  NS_DECL_ISUPPORTS

private:
  static const unsigned int KMaxDeviceNameLength = 128;
  static const unsigned int KMaxUniqueIdLength = 256;

  void Init();
  void Shutdown();

  webrtc::VoiceEngine* mVoiceEngine;
  webrtc::VoEBase* mVoEBase;
  webrtc::VoEExternalMedia* mVoERender;
  webrtc::VoENetwork*  mVoENetwork;

  mozilla::ReentrantMonitor mMonitor;

  int mCapIndex;
  int mChannel;
  TrackID mTrackID;
  bool mInitDone;

  nsString mDeviceName;
  nsString mDeviceUUID;

  SourceMediaStream* mSource;
  NullTransport *mNullTransport;
};

class MediaEngineWebRTC : public MediaEngine
{
public:
  MediaEngineWebRTC()
  : mMutex("mozilla::MediaEngineWebRTC")
  , mVideoEngine(NULL)
  , mVoiceEngine(NULL)
  , mVideoEngineInit(false)
  , mAudioEngineInit(false)
  {
    mVideoSources.Init();
    mAudioSources.Init();
  }
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
};

}

#endif /* NSMEDIAENGINEWEBRTC_H_ */
