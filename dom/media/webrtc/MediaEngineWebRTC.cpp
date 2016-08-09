/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "CSFLog.h"
#include "prenv.h"

#include "mozilla/Logging.h"
#ifdef XP_WIN
#include "mozilla/WindowsVersion.h"
#endif

static mozilla::LazyLogModule sGetUserMediaLog("GetUserMedia");

#include "MediaEngineWebRTC.h"
#include "ImageContainer.h"
#include "nsIComponentRegistrar.h"
#include "MediaEngineTabVideoSource.h"
#include "MediaEngineRemoteVideoSource.h"
#include "CamerasChild.h"
#include "nsITabSource.h"
#include "MediaTrackConstraints.h"

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidJNIWrapper.h"
#include "AndroidBridge.h"
#endif

#if defined(MOZ_B2G_CAMERA) && defined(MOZ_WIDGET_GONK)
#include "ICameraControl.h"
#include "MediaEngineGonkVideoSource.h"
#endif

#undef LOG
#define LOG(args) MOZ_LOG(sGetUserMediaLog, mozilla::LogLevel::Debug, args)

namespace mozilla {

// statics from AudioInputCubeb
nsTArray<int>* AudioInputCubeb::mDeviceIndexes;
int AudioInputCubeb::mDefaultDevice = -1;
nsTArray<nsCString>* AudioInputCubeb::mDeviceNames;
cubeb_device_collection* AudioInputCubeb::mDevices = nullptr;
bool AudioInputCubeb::mAnyInUse = false;
StaticMutex AudioInputCubeb::sMutex;

// AudioDeviceID is an annoying opaque value that's really a string
// pointer, and is freed when the cubeb_device_collection is destroyed

void AudioInputCubeb::UpdateDeviceList()
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
  mDefaultDevice = -1;
  for (uint32_t i = 0; i < devices->count; i++) {
    if (devices->device[i]->type == CUBEB_DEVICE_TYPE_INPUT && // paranoia
        (devices->device[i]->state == CUBEB_DEVICE_STATE_ENABLED ||
         (devices->device[i]->state == CUBEB_DEVICE_STATE_DISABLED &&
          devices->device[i]->friendly_name &&
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
      if (devices->device[i]->preferred & CUBEB_DEVICE_PREF_VOICE) {
        // There can be only one... we hope
        NS_ASSERTION(mDefaultDevice == -1, "multiple default cubeb input devices!");
        mDefaultDevice = i;
      }
    }
  }
  StaticMutexAutoLock lock(sMutex);
  // swap state
  if (mDevices) {
    cubeb_device_collection_destroy(mDevices);
  }
  mDevices = devices;
}

MediaEngineWebRTC::MediaEngineWebRTC(MediaEnginePrefs &aPrefs)
  : mMutex("mozilla::MediaEngineWebRTC"),
    mVoiceEngine(nullptr),
    mAudioInput(nullptr),
    mFullDuplex(aPrefs.mFullDuplex),
    mExtendedFilter(aPrefs.mExtendedFilter),
    mDelayAgnostic(aPrefs.mDelayAgnostic)
{
#ifndef MOZ_B2G_CAMERA
  nsCOMPtr<nsIComponentRegistrar> compMgr;
  NS_GetComponentRegistrar(getter_AddRefs(compMgr));
  if (compMgr) {
    compMgr->IsContractIDRegistered(NS_TABSOURCESERVICE_CONTRACTID, &mHasTabVideoSource);
  }
#else
#ifdef MOZ_WIDGET_GONK
  AsyncLatencyLogger::Get()->AddRef();
#endif
#endif
  // XXX
  gFarendObserver = new AudioOutputObserver();
}

void
MediaEngineWebRTC::EnumerateVideoDevices(dom::MediaSourceEnum aMediaSource,
                                         nsTArray<RefPtr<MediaEngineVideoSource> >* aVSources)
{
  // We spawn threads to handle gUM runnables, so we must protect the member vars
  MutexAutoLock lock(mMutex);

#if defined(MOZ_B2G_CAMERA) && defined(MOZ_WIDGET_GONK)
  if (aMediaSource != dom::MediaSourceEnum::Camera) {
    // only supports camera sources
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
  result = ICameraControl::GetNumberOfCameras(num);
  if (num <= 0 || result != NS_OK) {
    return;
  }

  for (int i = 0; i < num; i++) {
    nsCString cameraName;
    result = ICameraControl::GetCameraName(i, cameraName);
    if (result != NS_OK) {
      continue;
    }

    RefPtr<MediaEngineVideoSource> vSource;
    NS_ConvertUTF8toUTF16 uuid(cameraName);
    if (mVideoSources.Get(uuid, getter_AddRefs(vSource))) {
      // We've already seen this device, just append.
      aVSources->AppendElement(vSource.get());
    } else {
      vSource = new MediaEngineGonkVideoSource(i);
      mVideoSources.Put(uuid, vSource); // Hashtable takes ownership.
      aVSources->AppendElement(vSource);
    }
  }

  return;
#else
  mozilla::camera::CaptureEngine capEngine = mozilla::camera::InvalidEngine;

#ifdef MOZ_WIDGET_ANDROID
  // get the JVM
  JavaVM* jvm;
  JNIEnv* const env = jni::GetEnvForThread();
  MOZ_ALWAYS_TRUE(!env->GetJavaVM(&jvm));

  if (webrtc::VideoEngine::SetAndroidObjects(jvm) != 0) {
    LOG(("VieCapture:SetAndroidObjects Failed"));
    return;
  }
#endif

  switch (aMediaSource) {
    case dom::MediaSourceEnum::Window:
      capEngine = mozilla::camera::WinEngine;
      break;
    case dom::MediaSourceEnum::Application:
      capEngine = mozilla::camera::AppEngine;
      break;
    case dom::MediaSourceEnum::Screen:
      capEngine = mozilla::camera::ScreenEngine;
      break;
    case dom::MediaSourceEnum::Browser:
      capEngine = mozilla::camera::BrowserEngine;
      break;
    case dom::MediaSourceEnum::Camera:
      capEngine = mozilla::camera::CameraEngine;
      break;
    default:
      // BOOM
      MOZ_CRASH("No valid video engine");
      break;
  }

  /**
   * We still enumerate every time, in case a new device was plugged in since
   * the last call. TODO: Verify that WebRTC actually does deal with hotplugging
   * new devices (with or without new engine creation) and accordingly adjust.
   * Enumeration is not neccessary if GIPS reports the same set of devices
   * for a given instance of the engine. Likewise, if a device was plugged out,
   * mVideoSources must be updated.
   */
  int num;
  num = mozilla::camera::GetChildAndCall(
    &mozilla::camera::CamerasChild::NumberOfCaptureDevices,
    capEngine);

  for (int i = 0; i < num; i++) {
    char deviceName[MediaEngineSource::kMaxDeviceNameLength];
    char uniqueId[MediaEngineSource::kMaxUniqueIdLength];

    // paranoia
    deviceName[0] = '\0';
    uniqueId[0] = '\0';
    int error;

    error =  mozilla::camera::GetChildAndCall(
      &mozilla::camera::CamerasChild::GetCaptureDevice,
      capEngine,
      i, deviceName,
      sizeof(deviceName), uniqueId,
      sizeof(uniqueId));
    if (error) {
      LOG(("camera:GetCaptureDevice: Failed %d", error ));
      continue;
    }
#ifdef DEBUG
    LOG(("  Capture Device Index %d, Name %s", i, deviceName));

    webrtc::CaptureCapability cap;
    int numCaps = mozilla::camera::GetChildAndCall(
      &mozilla::camera::CamerasChild::NumberOfCapabilities,
      capEngine,
      uniqueId);
    LOG(("Number of Capabilities %d", numCaps));
    for (int j = 0; j < numCaps; j++) {
      if (mozilla::camera::GetChildAndCall(
            &mozilla::camera::CamerasChild::GetCaptureCapability,
            capEngine,
            uniqueId,
            j, cap) != 0) {
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

    RefPtr<MediaEngineVideoSource> vSource;
    NS_ConvertUTF8toUTF16 uuid(uniqueId);
    if (mVideoSources.Get(uuid, getter_AddRefs(vSource))) {
      // We've already seen this device, just refresh and append.
      static_cast<MediaEngineRemoteVideoSource*>(vSource.get())->Refresh(i);
      aVSources->AppendElement(vSource.get());
    } else {
      vSource = new MediaEngineRemoteVideoSource(i, capEngine, aMediaSource);
      mVideoSources.Put(uuid, vSource); // Hashtable takes ownership.
      aVSources->AppendElement(vSource);
    }
  }

  if (mHasTabVideoSource || dom::MediaSourceEnum::Browser == aMediaSource) {
    aVSources->AppendElement(new MediaEngineTabVideoSource());
  }
#endif
}

bool
MediaEngineWebRTC::SupportsDuplex()
{
#ifndef XP_WIN
  return mFullDuplex;
#else
  return IsVistaOrLater() && mFullDuplex;
#endif
}

void
MediaEngineWebRTC::EnumerateAudioDevices(dom::MediaSourceEnum aMediaSource,
                                         nsTArray<RefPtr<MediaEngineAudioSource> >* aASources)
{
  ScopedCustomReleasePtr<webrtc::VoEBase> ptrVoEBase;
  // We spawn threads to handle gUM runnables, so we must protect the member vars
  MutexAutoLock lock(mMutex);

  if (aMediaSource == dom::MediaSourceEnum::AudioCapture) {
    RefPtr<MediaEngineWebRTCAudioCaptureSource> audioCaptureSource =
      new MediaEngineWebRTCAudioCaptureSource(nullptr);
    aASources->AppendElement(audioCaptureSource);
    return;
  }

#ifdef MOZ_WIDGET_ANDROID
  jobject context = mozilla::AndroidBridge::Bridge()->GetGlobalContextRef();

  // get the JVM
  JavaVM* jvm;
  JNIEnv* const env = jni::GetEnvForThread();
  MOZ_ALWAYS_TRUE(!env->GetJavaVM(&jvm));

  if (webrtc::VoiceEngine::SetAndroidObjects(jvm, (void*)context) != 0) {
    LOG(("VoiceEngine:SetAndroidObjects Failed"));
    return;
  }
#endif

  if (!mVoiceEngine) {
    mConfig.Set<webrtc::ExtendedFilter>(new webrtc::ExtendedFilter(mExtendedFilter));
    mConfig.Set<webrtc::DelayAgnostic>(new webrtc::DelayAgnostic(mDelayAgnostic));

    mVoiceEngine = webrtc::VoiceEngine::Create(mConfig);
    if (!mVoiceEngine) {
      return;
    }
  }

  ptrVoEBase = webrtc::VoEBase::GetInterface(mVoiceEngine);
  if (!ptrVoEBase) {
    return;
  }

  // Always re-init the voice engine, since if we close the last use we
  // DeInitEngine() and Terminate(), which shuts down Process() - but means
  // we have to Init() again before using it.  Init() when already inited is
  // just a no-op, so call always.
  if (ptrVoEBase->Init() < 0) {
    return;
  }

  if (!mAudioInput) {
    if (SupportsDuplex()) {
      // The platform_supports_full_duplex.
      mAudioInput = new mozilla::AudioInputCubeb(mVoiceEngine);
    } else {
      mAudioInput = new mozilla::AudioInputWebRTC(mVoiceEngine);
    }
  }

  int nDevices = 0;
  mAudioInput->GetNumOfRecordingDevices(nDevices);
  int i;
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
  i = 0; // Bug 1037025 - let the OS handle defaulting for now on android/b2g
#else
  // -1 is "default communications device" depending on OS in webrtc.org code
  i = -1;
#endif
  for (; i < nDevices; i++) {
    // We use constants here because GetRecordingDeviceName takes char[128].
    char deviceName[128];
    char uniqueId[128];
    // paranoia; jingle doesn't bother with this
    deviceName[0] = '\0';
    uniqueId[0] = '\0';

    int error = mAudioInput->GetRecordingDeviceName(i, deviceName, uniqueId);
    if (error) {
      LOG((" VoEHardware:GetRecordingDeviceName: Failed %d", error));
      continue;
    }

    if (uniqueId[0] == '\0') {
      // Mac and Linux don't set uniqueId!
      MOZ_ASSERT(sizeof(deviceName) == sizeof(uniqueId)); // total paranoia
      strcpy(uniqueId, deviceName); // safe given assert and initialization/error-check
    }

    RefPtr<MediaEngineAudioSource> aSource;
    NS_ConvertUTF8toUTF16 uuid(uniqueId);
    if (mAudioSources.Get(uuid, getter_AddRefs(aSource))) {
      // We've already seen this device, just append.
      aASources->AppendElement(aSource.get());
    } else {
      AudioInput* audioinput = mAudioInput;
      if (SupportsDuplex()) {
        // The platform_supports_full_duplex.

        // For cubeb, it has state (the selected ID)
        // XXX just use the uniqueID for cubeb and support it everywhere, and get rid of this
        // XXX Small window where the device list/index could change!
        audioinput = new mozilla::AudioInputCubeb(mVoiceEngine, i);
      }
      aSource = new MediaEngineWebRTCMicrophoneSource(mVoiceEngine, audioinput,
                                                      i, deviceName, uniqueId);
      mAudioSources.Put(uuid, aSource); // Hashtable takes ownership.
      aASources->AppendElement(aSource);
    }
  }
}

void
MediaEngineWebRTC::Shutdown()
{
  // This is likely paranoia
  MutexAutoLock lock(mMutex);

  LOG(("%s", __FUNCTION__));
  // Shutdown all the sources, since we may have dangling references to the
  // sources in nsDOMUserMediaStreams waiting for GC/CC
  for (auto iter = mVideoSources.Iter(); !iter.Done(); iter.Next()) {
    MediaEngineVideoSource* source = iter.UserData();
    if (source) {
      source->Shutdown();
    }
  }
  for (auto iter = mAudioSources.Iter(); !iter.Done(); iter.Next()) {
    MediaEngineAudioSource* source = iter.UserData();
    if (source) {
      source->Shutdown();
    }
  }
  mVideoSources.Clear();
  mAudioSources.Clear();

  if (mVoiceEngine) {
    mVoiceEngine->SetTraceCallback(nullptr);
    webrtc::VoiceEngine::Delete(mVoiceEngine);
  }

  mVoiceEngine = nullptr;

  mozilla::camera::Shutdown();
  AudioInputCubeb::CleanupGlobalData();
}

}
