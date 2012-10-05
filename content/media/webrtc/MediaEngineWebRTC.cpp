/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

#if defined(PR_LOG)
#error "This file must be #included before any IPDL-generated files or other files that #include prlog.h"
#endif

#include "prlog.h"

#ifdef PR_LOGGING
PRLogModuleInfo* GetUserMediaLog = PR_NewLogModule("GetUserMedia");
#endif

#undef LOG
#define LOG(args) PR_LOG(GetUserMediaLog, PR_LOG_DEBUG, args)

#include "MediaEngineWebRTC.h"
#include "ImageContainer.h"

namespace mozilla {

void
MediaEngineWebRTC::EnumerateVideoDevices(nsTArray<nsRefPtr<MediaEngineVideoSource> >* aVSources)
{
  webrtc::ViEBase* ptrViEBase;
  webrtc::ViECapture* ptrViECapture;

  if (!mVideoEngine) {
    if (!(mVideoEngine = webrtc::VideoEngine::Create())) {
      return;
    }
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

  int num = ptrViECapture->NumberOfCaptureDevices();
  if (num <= 0) {
    return;
  }

  for (int i = 0; i < num; i++) {
#ifdef DEBUG
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

    nsRefPtr<MediaEngineVideoSource> vSource = new MediaEngineWebRTCVideoSource(mVideoEngine, i);
    aVSources->AppendElement(vSource.forget());
  }

  ptrViEBase->Release();
  ptrViECapture->Release();

  return;
}

void
MediaEngineWebRTC::EnumerateAudioDevices(nsTArray<nsRefPtr<MediaEngineAudioSource> >* aASources)
{
  webrtc::VoEBase* ptrVoEBase = NULL;
  webrtc::VoEHardware* ptrVoEHw = NULL;

  if (!mVoiceEngine) {
    mVoiceEngine = webrtc::VoiceEngine::Create();
    if (!mVoiceEngine) {
      return;
    }
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
    char uniqueID[128];
    // paranoia; jingle doesn't bother with this
    deviceName[0] = '\0';
    uniqueID[0] = '\0';

    ptrVoEHw->GetRecordingDeviceName(i, deviceName, uniqueID);
    nsRefPtr<MediaEngineAudioSource> aSource = new MediaEngineWebRTCAudioSource(
      mVoiceEngine, i, deviceName, uniqueID
    );
    aASources->AppendElement(aSource.forget());
  }

  ptrVoEHw->Release();
  ptrVoEBase->Release();
}


void
MediaEngineWebRTC::Shutdown()
{
  if (mVideoEngine) {
    webrtc::VideoEngine::Delete(mVideoEngine);
  }

  if (mVoiceEngine) {
    webrtc::VoiceEngine::Delete(mVoiceEngine);
  }

  mVideoEngine = NULL;
  mVoiceEngine = NULL;
}

}
