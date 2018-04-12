/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineWebRTC.h"

#include "AllocationHandle.h"
#include "CamerasChild.h"
#include "CSFLog.h"
#include "MediaEngineTabVideoSource.h"
#include "MediaEngineRemoteVideoSource.h"
#include "MediaTrackConstraints.h"
#include "mozilla/Logging.h"
#include "nsIComponentRegistrar.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsITabSource.h"
#include "prenv.h"

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#include "VideoEngine.h"
#endif

static mozilla::LazyLogModule sGetUserMediaLog("GetUserMedia");
#undef LOG
#define LOG(args) MOZ_LOG(sGetUserMediaLog, mozilla::LogLevel::Debug, args)

namespace mozilla {

// statics from AudioInputCubeb
nsTArray<int>* AudioInputCubeb::mDeviceIndexes;
int AudioInputCubeb::mDefaultDevice = -1;
nsTArray<nsCString>* AudioInputCubeb::mDeviceNames;
cubeb_device_collection AudioInputCubeb::mDevices = { nullptr, 0 };
bool AudioInputCubeb::mAnyInUse = false;
StaticMutex AudioInputCubeb::sMutex;
uint32_t AudioInputCubeb::sUserChannelCount = 0;

// AudioDeviceID is an annoying opaque value that's really a string
// pointer, and is freed when the cubeb_device_collection is destroyed

void AudioInputCubeb::UpdateDeviceList()
{
  // We keep all the device names, but wipe the mappings and rebuild them.
  // Do this first so that if cubeb has failed we've unmapped our devices
  // before we early return. Otherwise we'd keep the old list.
  for (auto& device_index : (*mDeviceIndexes)) {
    device_index = -1; // unmapped
  }

  cubeb* cubebContext = CubebUtils::GetCubebContext();
  if (!cubebContext) {
    return;
  }

  cubeb_device_collection devices = { nullptr, 0 };

  if (CUBEB_OK != cubeb_enumerate_devices(cubebContext,
                                          CUBEB_DEVICE_TYPE_INPUT,
                                          &devices)) {
    return;
  }

  // Calculate translation from existing mDevices to new devices. Note we
  // never end up with less devices than before, since people have
  // stashed indexes.
  // For some reason the "fake" device for automation is marked as DISABLED,
  // so white-list it.
  mDefaultDevice = -1;
  for (uint32_t i = 0; i < devices.count; i++) {
    LOG(("Cubeb device %u: type 0x%x, state 0x%x, name %s, id %p",
         i, devices.device[i].type, devices.device[i].state,
         devices.device[i].friendly_name, devices.device[i].device_id));
    if (devices.device[i].type == CUBEB_DEVICE_TYPE_INPUT && // paranoia
        devices.device[i].state == CUBEB_DEVICE_STATE_ENABLED )
    {
      auto j = mDeviceNames->IndexOf(devices.device[i].device_id);
      if (j != nsTArray<nsCString>::NoIndex) {
        // match! update the mapping
        (*mDeviceIndexes)[j] = i;
      } else {
        // new device, add to the array
        mDeviceIndexes->AppendElement(i);
        mDeviceNames->AppendElement(devices.device[i].device_id);
        j = mDeviceIndexes->Length()-1;
      }
      if (devices.device[i].preferred & CUBEB_DEVICE_PREF_VOICE) {
        // There can be only one... we hope
        NS_ASSERTION(mDefaultDevice == -1, "multiple default cubeb input devices!");
        mDefaultDevice = j;
      }
    }
  }
  LOG(("Cubeb default input device %d", mDefaultDevice));
  StaticMutexAutoLock lock(sMutex);
  // swap state
  cubeb_device_collection_destroy(cubebContext, &mDevices);
  mDevices = devices;
}

MediaEngineWebRTC::MediaEngineWebRTC(MediaEnginePrefs &aPrefs)
  : mMutex("MediaEngineWebRTC::mMutex"),
    mAudioInput(nullptr),
    mFullDuplex(aPrefs.mFullDuplex),
    mDelayAgnostic(aPrefs.mDelayAgnostic),
    mExtendedFilter(aPrefs.mExtendedFilter),
    mHasTabVideoSource(false)
{
  nsCOMPtr<nsIComponentRegistrar> compMgr;
  NS_GetComponentRegistrar(getter_AddRefs(compMgr));
  if (compMgr) {
    compMgr->IsContractIDRegistered(NS_TABSOURCESERVICE_CONTRACTID, &mHasTabVideoSource);
  }

  camera::GetChildAndCall(
    &camera::CamerasChild::AddDeviceChangeCallback,
    this);
}

void
MediaEngineWebRTC::SetFakeDeviceChangeEvents()
{
  camera::GetChildAndCall(
    &camera::CamerasChild::SetFakeDeviceChangeEvents);
}

void
MediaEngineWebRTC::EnumerateDevices(uint64_t aWindowId,
                                    dom::MediaSourceEnum aMediaSource,
                                    nsTArray<RefPtr<MediaEngineSource> >* aSources)
{
  if (MediaEngineSource::IsVideo(aMediaSource)) {
    // We spawn threads to handle gUM runnables, so we must protect the member vars
    MutexAutoLock lock(mMutex);

    mozilla::camera::CaptureEngine capEngine = mozilla::camera::InvalidEngine;

#ifdef MOZ_WIDGET_ANDROID
    // get the JVM
    JavaVM* jvm;
    JNIEnv* const env = jni::GetEnvForThread();
    MOZ_ALWAYS_TRUE(!env->GetJavaVM(&jvm));

    if (!jvm || mozilla::camera::VideoEngine::SetAndroidObjects(jvm)) {
      LOG(("VideoEngine::SetAndroidObjects Failed"));
      return;
    }
#endif
    bool scaryKind = false; // flag sources with cross-origin exploit potential

    switch (aMediaSource) {
      case dom::MediaSourceEnum::Window:
        capEngine = mozilla::camera::WinEngine;
        break;
      case dom::MediaSourceEnum::Application:
        capEngine = mozilla::camera::AppEngine;
        break;
      case dom::MediaSourceEnum::Screen:
        capEngine = mozilla::camera::ScreenEngine;
        scaryKind = true;
        break;
      case dom::MediaSourceEnum::Browser:
        capEngine = mozilla::camera::BrowserEngine;
        scaryKind = true;
        break;
      case dom::MediaSourceEnum::Camera:
        capEngine = mozilla::camera::CameraEngine;
        break;
      default:
        MOZ_CRASH("No valid video engine");
        break;
    }

    /*
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
      bool scarySource = false;

      // paranoia
      deviceName[0] = '\0';
      uniqueId[0] = '\0';
      int error;

      error =  mozilla::camera::GetChildAndCall(
        &mozilla::camera::CamerasChild::GetCaptureDevice,
        capEngine,
        i, deviceName,
        sizeof(deviceName), uniqueId,
        sizeof(uniqueId),
        &scarySource);
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

      NS_ConvertUTF8toUTF16 uuid(uniqueId);
      RefPtr<MediaEngineSource> vSource;

      nsRefPtrHashtable<nsStringHashKey, MediaEngineSource>*
        devicesForThisWindow = mVideoSources.LookupOrAdd(aWindowId);

      if (devicesForThisWindow->Get(uuid, getter_AddRefs(vSource)) &&
          vSource->RequiresSharing()) {
        // We've already seen this shared device, just refresh and append.
        static_cast<MediaEngineRemoteVideoSource*>(vSource.get())->Refresh(i);
        aSources->AppendElement(vSource.get());
      } else {
        vSource = new MediaEngineRemoteVideoSource(i, capEngine, aMediaSource,
                                                   scaryKind || scarySource);
        devicesForThisWindow->Put(uuid, vSource);
        aSources->AppendElement(vSource);
      }
    }

    if (mHasTabVideoSource || dom::MediaSourceEnum::Browser == aMediaSource) {
      aSources->AppendElement(new MediaEngineTabVideoSource());
    }
  } else {
    // We spawn threads to handle gUM runnables, so we must protect the member vars
    MutexAutoLock lock(mMutex);

    if (aMediaSource == dom::MediaSourceEnum::AudioCapture) {
      RefPtr<MediaEngineWebRTCAudioCaptureSource> audioCaptureSource =
        new MediaEngineWebRTCAudioCaptureSource(nullptr);
      aSources->AppendElement(audioCaptureSource);
      return;
    }

    if (!mAudioInput) {
      if (!SupportsDuplex()) {
        return;
      }
      mAudioInput = new mozilla::AudioInputCubeb();
    }

    int nDevices = 0;
    mAudioInput->GetNumOfRecordingDevices(nDevices);
    int i;
#if defined(MOZ_WIDGET_ANDROID)
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
        LOG((" AudioInput::GetRecordingDeviceName: Failed %d", error));
        continue;
      }

      if (uniqueId[0] == '\0') {
        // Mac and Linux don't set uniqueId!
        strcpy(uniqueId, deviceName); // safe given assert and initialization/error-check
      }


      RefPtr<MediaEngineSource> aSource;
      NS_ConvertUTF8toUTF16 uuid(uniqueId);

      nsRefPtrHashtable<nsStringHashKey, MediaEngineSource>*
        devicesForThisWindow = mAudioSources.LookupOrAdd(aWindowId);

      if (devicesForThisWindow->Get(uuid, getter_AddRefs(aSource)) &&
          aSource->RequiresSharing()) {
        // We've already seen this device, just append.
        aSources->AppendElement(aSource.get());
      } else {
        aSource = new MediaEngineWebRTCMicrophoneSource(
            new mozilla::AudioInputCubeb(i),
            i, deviceName, uniqueId,
            mDelayAgnostic, mExtendedFilter);
        devicesForThisWindow->Put(uuid, aSource);
        aSources->AppendElement(aSource);
      }
    }
  }
}

bool
MediaEngineWebRTC::SupportsDuplex()
{
  return mFullDuplex;
}

void
MediaEngineWebRTC::ReleaseResourcesForWindow(uint64_t aWindowId)
{
  {
    nsRefPtrHashtable<nsStringHashKey, MediaEngineSource>*
      audioDevicesForThisWindow = mAudioSources.Get(aWindowId);

    if (audioDevicesForThisWindow) {
      for (auto iter = audioDevicesForThisWindow->Iter(); !iter.Done();
           iter.Next()) {
        iter.UserData()->Shutdown();
      }

      // This makes audioDevicesForThisWindow invalid.
      mAudioSources.Remove(aWindowId);
    }
  }

  {
    nsRefPtrHashtable<nsStringHashKey, MediaEngineSource>*
      videoDevicesForThisWindow = mVideoSources.Get(aWindowId);
    if (videoDevicesForThisWindow) {
      for (auto iter = videoDevicesForThisWindow->Iter(); !iter.Done();
           iter.Next()) {
        iter.UserData()->Shutdown();
      }

      // This makes videoDevicesForThisWindow invalid.
      mVideoSources.Remove(aWindowId);
    }
  }
}

namespace {
template<typename T>
void ShutdownSources(T& aHashTable)
{
  for (auto iter = aHashTable.Iter(); !iter.Done(); iter.Next()) {
    for (auto iterInner = iter.UserData()->Iter(); !iterInner.Done();
         iterInner.Next()) {
      MediaEngineSource* source = iterInner.UserData();
      source->Shutdown();
    }
  }
}
}

void
MediaEngineWebRTC::Shutdown()
{
  // This is likely paranoia
  MutexAutoLock lock(mMutex);

  if (camera::GetCamerasChildIfExists()) {
    camera::GetChildAndCall(
      &camera::CamerasChild::RemoveDeviceChangeCallback, this);
  }

  LOG(("%s", __FUNCTION__));
  // Shutdown all the sources, since we may have dangling references to the
  // sources in nsDOMUserMediaStreams waiting for GC/CC
  ShutdownSources(mVideoSources);
  ShutdownSources(mAudioSources);

  mozilla::camera::Shutdown();
  AudioInputCubeb::CleanupGlobalData();
}

}
