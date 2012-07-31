/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINEWEBRTC_H_
#define MEDIAENGINEWEBRTC_H_

#include "prmem.h"
#include "prcvar.h"
#include "prthread.h"
#include "nsIThread.h"
#include "nsIRunnable.h"

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
#include "voice_engine/main/interface/voe_base.h"
#include "voice_engine/main/interface/voe_codec.h"
#include "voice_engine/main/interface/voe_hardware.h"
#include "voice_engine/main/interface/voe_audio_processing.h"
#include "voice_engine/main/interface/voe_volume_control.h"
#include "voice_engine/main/interface/voe_external_media.h"

// Video Engine
#include "video_engine/include/vie_base.h"
#include "video_engine/include/vie_codec.h"
#include "video_engine/include/vie_render.h"
#include "video_engine/include/vie_capture.h"
#include "video_engine/include/vie_file.h"


namespace mozilla {

/**
 * The WebRTC implementation of the MediaEngine interface.
 */

enum WebRTCEngineState {
  kAllocated,
  kStarted,
  kStopped,
  kReleased
};

class MediaEngineWebRTCVideoSource : public MediaEngineVideoSource,
                                     public webrtc::ExternalRenderer,
                                     public nsRunnable
{
public:
  // ViEExternalRenderer.
  virtual int FrameSizeChange(unsigned int, unsigned int, unsigned int);
  virtual int DeliverFrame(unsigned char*, int, uint32_t, int64_t);

  MediaEngineWebRTCVideoSource(webrtc::VideoEngine* videoEnginePtr,
    int index, int aFps = 30)
    : mVideoEngine(videoEnginePtr)
    , mCapIndex(index)
    , mWidth(640)
    , mHeight(480)
    , mState(kReleased)
    , mMonitor("WebRTCCamera.Monitor")
    , mFps(aFps)
    , mInitDone(false)
    , mInSnapshotMode(false)
    , mSnapshotPath(NULL) { Init(); }

  ~MediaEngineWebRTCVideoSource() { Shutdown(); }

  virtual void GetName(nsAString&);
  virtual void GetUUID(nsAString&);
  virtual MediaEngineVideoOptions GetOptions();
  virtual nsresult Allocate();
  virtual nsresult Deallocate();
  virtual nsresult Start(SourceMediaStream*, TrackID);
  virtual nsresult Stop();
  virtual nsresult Snapshot(PRUint32 aDuration, nsIDOMFile** aFile);

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
  static const unsigned int KMaxDeviceNameLength;
  static const unsigned int KMaxUniqueIdLength;

  // Initialize the needed Video engine interfaces.
  void Init();
  void Shutdown();

  // Engine variables.

  webrtc::VideoEngine* mVideoEngine; // Weak reference, don't free.
  webrtc::ViEBase* mViEBase;
  webrtc::ViECapture* mViECapture;
  webrtc::ViERender* mViERender;
  webrtc::CaptureCapability mCaps; // Doesn't work on OS X.

  int mCapIndex;
  int mWidth, mHeight;
  TrackID mTrackID;

  WebRTCEngineState mState;
  mozilla::ReentrantMonitor mMonitor; // Monitor for processing WebRTC frames.
  SourceMediaStream* mSource;

  int mFps; // Track rate (30 fps by default)
  bool mInitDone;
  bool mInSnapshotMode;
  nsString* mSnapshotPath;

  nsRefPtr<layers::ImageContainer> mImageContainer;

  PRLock* mSnapshotLock;
  PRCondVar* mSnapshotCondVar;

};

class MediaEngineWebRTCAudioSource : public MediaEngineAudioSource,
                                     public webrtc::VoEMediaProcess
{
public:
  MediaEngineWebRTCAudioSource(webrtc::VoiceEngine* voiceEngine, int aIndex,
    char* name, char* uuid)
    : mVoiceEngine(voiceEngine)
    , mMonitor("WebRTCMic.Monitor")
    , mCapIndex(aIndex)
    , mChannel(-1)
    , mInitDone(false)
    , mState(kReleased) {

    mVoEBase = webrtc::VoEBase::GetInterface(mVoiceEngine);
    mDeviceName.Assign(NS_ConvertASCIItoUTF16(name));
    mDeviceUUID.Assign(NS_ConvertASCIItoUTF16(uuid));
    mInitDone = true;
  }

  ~MediaEngineWebRTCAudioSource() { Shutdown(); }

  virtual void GetName(nsAString&);
  virtual void GetUUID(nsAString&);

  virtual nsresult Allocate();
  virtual nsresult Deallocate();
  virtual nsresult Start(SourceMediaStream*, TrackID);
  virtual nsresult Stop();
  virtual nsresult Snapshot(PRUint32 aDuration, nsIDOMFile** aFile);

  // VoEMediaProcess.
  void Process(const int channel, const webrtc::ProcessingTypes type,
               WebRtc_Word16 audio10ms[], const int length,
               const int samplingFreq, const bool isStereo);

  NS_DECL_ISUPPORTS

private:
  static const unsigned int KMaxDeviceNameLength;
  static const unsigned int KMaxUniqueIdLength;

  void Init();
  void Shutdown();

  webrtc::VoiceEngine* mVoiceEngine;
  webrtc::VoEBase* mVoEBase;
  webrtc::VoEExternalMedia* mVoERender;

  mozilla::ReentrantMonitor mMonitor;

  int mCapIndex;
  int mChannel;
  TrackID mTrackID;
  bool mInitDone;
  WebRTCEngineState mState;

  nsString mDeviceName;
  nsString mDeviceUUID;

  SourceMediaStream* mSource;
};

class MediaEngineWebRTC : public MediaEngine
{
public:
  MediaEngineWebRTC()
  : mVideoEngine(NULL)
  , mVoiceEngine(NULL)
  , mVideoEngineInit(false)
  , mAudioEngineInit(false) {}

  ~MediaEngineWebRTC() { Shutdown(); }

  // Clients should ensure to clean-up sources video/audio sources
  // before invoking Shutdown on this class.
  void Shutdown();

  virtual void EnumerateVideoDevices(nsTArray<nsRefPtr<MediaEngineVideoSource> >*);
  virtual void EnumerateAudioDevices(nsTArray<nsRefPtr<MediaEngineAudioSource> >*);

private:
  webrtc::VideoEngine* mVideoEngine;
  webrtc::VoiceEngine* mVoiceEngine;

  // Need this to avoid unneccesary WebRTC calls while enumerating.
  bool mVideoEngineInit;
  bool mAudioEngineInit;
};

}

#endif /* NSMEDIAENGINEWEBRTC_H_ */
