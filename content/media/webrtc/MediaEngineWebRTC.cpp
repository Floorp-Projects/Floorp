/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

#if defined(PR_LOG)
#error "This file must be #included before any IPDL-generated files or other files that #include prlog.h"
#endif

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "CSFLog.h"
#include "prenv.h"

#ifdef PR_LOGGING
static PRLogModuleInfo*
GetUserMediaLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog)
    sLog = PR_NewLogModule("GetUserMedia");
  return sLog;
}
#endif

#include "MediaEngineWebRTC.h"
#include "ImageContainer.h"
#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

#undef LOG
#define LOG(args) PR_LOG(GetUserMediaLog(), PR_LOG_DEBUG, args)

namespace mozilla {

void
MediaEngineWebRTC::EnumerateVideoDevices(nsTArray<nsRefPtr<MediaEngineVideoSource> >* aVSources)
{
#ifdef MOZ_B2G_CAMERA
  MutexAutoLock lock(mMutex);
  if (!mCameraManager) {
    return;
  }

  /**
   * We still enumerate every time, in case a new device was plugged in since
   * the last call. TODO: Verify that WebRTC actually does deal with hotplugging
   * new devices (with or without new engine creation) and accordingly adjust.
   * Enumeration is not neccessary if GIPS reports the same set of devices
   * for a given instance of the engine. Likewise, if a device was plugged out,
   * mVideoSources must be updated.
   */
  int num = 0;
  nsresult result;
  result = mCameraManager->GetNumberOfCameras(num);
  if (num <= 0 || result != NS_OK) {
    return;
  }

  for (int i = 0; i < num; i++) {
    nsCString cameraName;
    result = mCameraManager->GetCameraName(i, cameraName);
    if (result != NS_OK) {
      continue;
    }

    nsRefPtr<MediaEngineWebRTCVideoSource> vSource;
    NS_ConvertUTF8toUTF16 uuid(cameraName);
    if (mVideoSources.Get(uuid, getter_AddRefs(vSource))) {
      // We've already seen this device, just append.
      aVSources->AppendElement(vSource.get());
    } else {
      vSource = new MediaEngineWebRTCVideoSource(mCameraManager, i, mWindowId);
      mVideoSources.Put(uuid, vSource); // Hashtable takes ownership.
      aVSources->AppendElement(vSource);
    }
  }

  return;
#else
  webrtc::ViEBase* ptrViEBase;
  webrtc::ViECapture* ptrViECapture;
  // We spawn threads to handle gUM runnables, so we must protect the member vars
  MutexAutoLock lock(mMutex);

#ifdef MOZ_WIDGET_ANDROID
  jobject context = mozilla::AndroidBridge::Bridge()->GetGlobalContextRef();

  // get the JVM
  JavaVM *jvm = mozilla::AndroidBridge::Bridge()->GetVM();

  JNIEnv *env;
  jint res = jvm->AttachCurrentThread(&env, NULL);

  if (webrtc::VideoEngine::SetAndroidObjects(jvm, (void*)context) != 0) {
    LOG(("VieCapture:SetAndroidObjects Failed"));
    return;
  }

  env->DeleteGlobalRef(context);
#endif

  if (!mVideoEngine) {
    if (!(mVideoEngine = webrtc::VideoEngine::Create())) {
      return;
    }
  }

  PRLogModuleInfo *logs = GetWebRTCLogInfo();
  if (!gWebrtcTraceLoggingOn && logs && logs->level > 0) {
    // no need to a critical section or lock here
    gWebrtcTraceLoggingOn = 1;

    const char *file = PR_GetEnv("WEBRTC_TRACE_FILE");
    if (!file) {
      file = "WebRTC.log";
    }

    LOG(("%s Logging webrtc to %s level %d", __FUNCTION__, file, logs->level));

    mVideoEngine->SetTraceFilter(logs->level);
    mVideoEngine->SetTraceFile(file);
  }

  ptrViEBase = webrtc::ViEBase::GetInterface(mVideoEngine);
  if (!ptrViEBase) {
    return;
  }

  if (!mVideoEngineInit) {
    if (ptrViEBase->Init() < 0) {
      return;
    }
    mVideoEngineInit = true;
  }

  ptrViECapture = webrtc::ViECapture::GetInterface(mVideoEngine);
  if (!ptrViECapture) {
    return;
  }

  /**
   * We still enumerate every time, in case a new device was plugged in since
   * the last call. TODO: Verify that WebRTC actually does deal with hotplugging
   * new devices (with or without new engine creation) and accordingly adjust.
   * Enumeration is not neccessary if GIPS reports the same set of devices
   * for a given instance of the engine. Likewise, if a device was plugged out,
   * mVideoSources must be updated.
   */
  int num = ptrViECapture->NumberOfCaptureDevices();
  if (num <= 0) {
    return;
  }

  for (int i = 0; i < num; i++) {
    const unsigned int kMaxDeviceNameLength = 128; // XXX FIX!
    const unsigned int kMaxUniqueIdLength = 256;
    char deviceName[kMaxDeviceNameLength];
    char uniqueId[kMaxUniqueIdLength];

    // paranoia
    deviceName[0] = '\0';
    uniqueId[0] = '\0';
    int error = ptrViECapture->GetCaptureDevice(i, deviceName,
                                                sizeof(deviceName), uniqueId,
                                                sizeof(uniqueId));

    if (error) {
      LOG((" VieCapture:GetCaptureDevice: Failed %d",
           ptrViEBase->LastError() ));
      continue;
    }
#ifdef DEBUG
    LOG(("  Capture Device Index %d, Name %s", i, deviceName));

    webrtc::CaptureCapability cap;
    int numCaps = ptrViECapture->NumberOfCapabilities(uniqueId, kMaxUniqueIdLength);
    LOG(("Number of Capabilities %d", numCaps));
    for (int j = 0; j < numCaps; j++) {
      if (ptrViECapture->GetCaptureCapability(uniqueId, kMaxUniqueIdLength,
                                              j, cap ) != 0 ) {
        break;
      }
      LOG(("type=%d width=%d height=%d maxFPS=%d",
           cap.rawType, cap.width, cap.height, cap.maxFPS ));
    }
#endif

    if (uniqueId[0] == '\0') {
      // In case a device doesn't set uniqueId!
      strncpy(uniqueId, deviceName, sizeof(uniqueId));
      uniqueId[sizeof(uniqueId)-1] = '\0'; // strncpy isn't safe
    }

    nsRefPtr<MediaEngineWebRTCVideoSource> vSource;
    NS_ConvertUTF8toUTF16 uuid(uniqueId);
    if (mVideoSources.Get(uuid, getter_AddRefs(vSource))) {
      // We've already seen this device, just append.
      aVSources->AppendElement(vSource.get());
    } else {
      vSource = new MediaEngineWebRTCVideoSource(mVideoEngine, i);
      mVideoSources.Put(uuid, vSource); // Hashtable takes ownership.
      aVSources->AppendElement(vSource);
    }
  }

  ptrViEBase->Release();
  ptrViECapture->Release();

  return;
#endif
}

void
MediaEngineWebRTC::EnumerateAudioDevices(nsTArray<nsRefPtr<MediaEngineAudioSource> >* aASources)
{
  webrtc::VoEBase* ptrVoEBase = NULL;
  webrtc::VoEHardware* ptrVoEHw = NULL;
  // We spawn threads to handle gUM runnables, so we must protect the member vars
  MutexAutoLock lock(mMutex);

#ifdef MOZ_WIDGET_ANDROID
  jobject context = mozilla::AndroidBridge::Bridge()->GetGlobalContextRef();

  // get the JVM
  JavaVM *jvm = mozilla::AndroidBridge::Bridge()->GetVM();

  JNIEnv *env;
  jvm->AttachCurrentThread(&env, NULL);

  if (webrtc::VoiceEngine::SetAndroidObjects(jvm, (void*)context) != 0) {
    LOG(("VoiceEngine:SetAndroidObjects Failed"));
    return;
  }

  env->DeleteGlobalRef(context);
#endif

  if (!mVoiceEngine) {
    mVoiceEngine = webrtc::VoiceEngine::Create();
    if (!mVoiceEngine) {
      return;
    }
  }

  PRLogModuleInfo *logs = GetWebRTCLogInfo();
  if (!gWebrtcTraceLoggingOn && logs && logs->level > 0) {
    // no need to a critical section or lock here
    gWebrtcTraceLoggingOn = 1;

    const char *file = PR_GetEnv("WEBRTC_TRACE_FILE");
    if (!file) {
      file = "WebRTC.log";
    }

    LOG(("Logging webrtc to %s level %d", __FUNCTION__, file, logs->level));

    mVoiceEngine->SetTraceFilter(logs->level);
    mVoiceEngine->SetTraceFile(file);
  }

  ptrVoEBase = webrtc::VoEBase::GetInterface(mVoiceEngine);
  if (!ptrVoEBase) {
    return;
  }

  if (!mAudioEngineInit) {
    if (ptrVoEBase->Init() < 0) {
      return;
    }
    mAudioEngineInit = true;
  }

  ptrVoEHw = webrtc::VoEHardware::GetInterface(mVoiceEngine);
  if (!ptrVoEHw)  {
    return;
  }

  int nDevices = 0;
  ptrVoEHw->GetNumOfRecordingDevices(nDevices);
  for (int i = 0; i < nDevices; i++) {
    // We use constants here because GetRecordingDeviceName takes char[128].
    char deviceName[128];
    char uniqueId[128];
    // paranoia; jingle doesn't bother with this
    deviceName[0] = '\0';
    uniqueId[0] = '\0';

    int error = ptrVoEHw->GetRecordingDeviceName(i, deviceName, uniqueId);
    if (error) {
      LOG((" VoEHardware:GetRecordingDeviceName: Failed %d",
           ptrVoEBase->LastError() ));
      continue;
    }

    if (uniqueId[0] == '\0') {
      // Mac and Linux don't set uniqueId!
      MOZ_ASSERT(sizeof(deviceName) == sizeof(uniqueId)); // total paranoia
      strcpy(uniqueId,deviceName); // safe given assert and initialization/error-check
    }

    nsRefPtr<MediaEngineWebRTCAudioSource> aSource;
    NS_ConvertUTF8toUTF16 uuid(uniqueId);
    if (mAudioSources.Get(uuid, getter_AddRefs(aSource))) {
      // We've already seen this device, just append.
      aASources->AppendElement(aSource.get());
    } else {
      aSource = new MediaEngineWebRTCAudioSource(
        mVoiceEngine, i, deviceName, uniqueId
      );
      mAudioSources.Put(uuid, aSource); // Hashtable takes ownership.
      aASources->AppendElement(aSource);
    }
  }

  ptrVoEHw->Release();
  ptrVoEBase->Release();
}

void
MediaEngineWebRTC::Shutdown()
{
  // This is likely paranoia
  MutexAutoLock lock(mMutex);

  if (mVideoEngine) {
    mVideoSources.Clear();
    webrtc::VideoEngine::Delete(mVideoEngine);
  }

  if (mVoiceEngine) {
    mAudioSources.Clear();
    webrtc::VoiceEngine::Delete(mVoiceEngine);
  }

  mVideoEngine = NULL;
  mVoiceEngine = NULL;
}

}
