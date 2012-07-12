/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineWebRTC.h"

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
    memset(deviceName, 0, sizeof(deviceName));

    char uniqueID[128];
    memset(uniqueID, 0, sizeof(uniqueID));

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
