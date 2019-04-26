/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineWebRTC.h"

#include "CamerasChild.h"
#include "CSFLog.h"
#include "MediaEngineTabVideoSource.h"
#include "MediaEngineRemoteVideoSource.h"
#include "MediaEngineWebRTCAudio.h"
#include "MediaManager.h"
#include "MediaTrackConstraints.h"
#include "mozilla/dom/MediaDeviceInfo.h"
#include "mozilla/Logging.h"
#include "nsIComponentRegistrar.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsITabSource.h"
#include "prenv.h"

static mozilla::LazyLogModule sGetUserMediaLog("GetUserMedia");
#undef LOG
#define LOG(args) MOZ_LOG(sGetUserMediaLog, mozilla::LogLevel::Debug, args)

namespace mozilla {

MediaEngineWebRTC::MediaEngineWebRTC(MediaEnginePrefs& aPrefs)
    : mMutex("mozilla::MediaEngineWebRTC"),
      mDelayAgnostic(aPrefs.mDelayAgnostic),
      mExtendedFilter(aPrefs.mExtendedFilter),
      mHasTabVideoSource(false) {
  nsCOMPtr<nsIComponentRegistrar> compMgr;
  NS_GetComponentRegistrar(getter_AddRefs(compMgr));
  if (compMgr) {
    compMgr->IsContractIDRegistered(NS_TABSOURCESERVICE_CONTRACTID,
                                    &mHasTabVideoSource);
  }

  camera::GetChildAndCall(&camera::CamerasChild::AddDeviceChangeCallback, this);
}

void MediaEngineWebRTC::SetFakeDeviceChangeEvents() {
  camera::GetChildAndCall(&camera::CamerasChild::SetFakeDeviceChangeEvents);
}

void MediaEngineWebRTC::EnumerateVideoDevices(
    uint64_t aWindowId, camera::CaptureEngine aCapEngine,
    nsTArray<RefPtr<MediaDevice>>* aDevices) {
  mMutex.AssertCurrentThreadOwns();
  // flag sources with cross-origin exploit potential
  bool scaryKind = (aCapEngine == camera::ScreenEngine ||
                    aCapEngine == camera::BrowserEngine);
  /*
   * We still enumerate every time, in case a new device was plugged in since
   * the last call. TODO: Verify that WebRTC actually does deal with hotplugging
   * new devices (with or without new engine creation) and accordingly adjust.
   * Enumeration is not neccessary if GIPS reports the same set of devices
   * for a given instance of the engine.
   */
  int num;
#if defined(_ARM64_) && defined(XP_WIN)
  // There are problems with using DirectShow on versions of Windows before
  // 19H1 on arm64. This disables the camera on older versions of Windows.
  if (aCapEngine == camera::CameraEngine) {
    typedef ULONG (*RtlGetVersionFn)(LPOSVERSIONINFOEXW);
    RtlGetVersionFn RtlGetVersion;
    RtlGetVersion = (RtlGetVersionFn)GetProcAddress(GetModuleHandleA("ntdll"),
                                                    "RtlGetVersion");
    if (RtlGetVersion) {
      OSVERSIONINFOEXW info;
      info.dwOSVersionInfoSize = sizeof(info);
      RtlGetVersion(&info);
      // 19H1 is 18346
      if (info.dwBuildNumber < 18346) {
        return;
      }
    }
  }
#endif
  num = mozilla::camera::GetChildAndCall(
      &mozilla::camera::CamerasChild::NumberOfCaptureDevices, aCapEngine);

  for (int i = 0; i < num; i++) {
    char deviceName[MediaEngineSource::kMaxDeviceNameLength];
    char uniqueId[MediaEngineSource::kMaxUniqueIdLength];
    bool scarySource = false;

    // paranoia
    deviceName[0] = '\0';
    uniqueId[0] = '\0';
    int error;

    error = mozilla::camera::GetChildAndCall(
        &mozilla::camera::CamerasChild::GetCaptureDevice, aCapEngine, i,
        deviceName, sizeof(deviceName), uniqueId, sizeof(uniqueId),
        &scarySource);
    if (error) {
      LOG(("camera:GetCaptureDevice: Failed %d", error));
      continue;
    }
#ifdef DEBUG
    LOG(("  Capture Device Index %d, Name %s", i, deviceName));

    webrtc::CaptureCapability cap;
    int numCaps = mozilla::camera::GetChildAndCall(
        &mozilla::camera::CamerasChild::NumberOfCapabilities, aCapEngine,
        uniqueId);
    LOG(("Number of Capabilities %d", numCaps));
    for (int j = 0; j < numCaps; j++) {
      if (mozilla::camera::GetChildAndCall(
              &mozilla::camera::CamerasChild::GetCaptureCapability, aCapEngine,
              uniqueId, j, cap) != 0) {
        break;
      }
      LOG(("type=%d width=%d height=%d maxFPS=%d",
           static_cast<int>(cap.videoType), cap.width, cap.height, cap.maxFPS));
    }
#endif

    if (uniqueId[0] == '\0') {
      // In case a device doesn't set uniqueId!
      strncpy(uniqueId, deviceName, sizeof(uniqueId));
      uniqueId[sizeof(uniqueId) - 1] = '\0';  // strncpy isn't safe
    }

    NS_ConvertUTF8toUTF16 uuid(uniqueId);
    RefPtr<MediaEngineSource> vSource;

    vSource = new MediaEngineRemoteVideoSource(i, aCapEngine,
                                               scaryKind || scarySource);
    aDevices->AppendElement(MakeRefPtr<MediaDevice>(
        vSource, vSource->GetName(), NS_ConvertUTF8toUTF16(vSource->GetUUID()),
        vSource->GetGroupId(), NS_LITERAL_STRING("")));
  }

  if (mHasTabVideoSource || aCapEngine == camera::BrowserEngine) {
    RefPtr<MediaEngineSource> tabVideoSource = new MediaEngineTabVideoSource();
    aDevices->AppendElement(MakeRefPtr<MediaDevice>(
        tabVideoSource, tabVideoSource->GetName(),
        NS_ConvertUTF8toUTF16(tabVideoSource->GetUUID()),
        tabVideoSource->GetGroupId(), NS_LITERAL_STRING("")));
  }
}

void MediaEngineWebRTC::EnumerateMicrophoneDevices(
    uint64_t aWindowId, nsTArray<RefPtr<MediaDevice>>* aDevices) {
  mMutex.AssertCurrentThreadOwns();

  mEnumerator = CubebDeviceEnumerator::GetInstance();

  nsTArray<RefPtr<AudioDeviceInfo>> devices;
  mEnumerator->EnumerateAudioInputDevices(devices);

  DebugOnly<bool> foundPreferredDevice = false;

  for (uint32_t i = 0; i < devices.Length(); i++) {
#ifndef ANDROID
    MOZ_ASSERT(devices[i]->DeviceID());
#endif
    LOG(("Cubeb device %u: type 0x%x, state 0x%x, name %s, id %p", i,
         devices[i]->Type(), devices[i]->State(),
         NS_ConvertUTF16toUTF8(devices[i]->Name()).get(),
         devices[i]->DeviceID()));

    if (devices[i]->State() == CUBEB_DEVICE_STATE_ENABLED) {
      MOZ_ASSERT(devices[i]->Type() == CUBEB_DEVICE_TYPE_INPUT);
      RefPtr<MediaEngineSource> source = new MediaEngineWebRTCMicrophoneSource(
          devices[i], devices[i]->Name(),
          // Lie and provide the name as UUID
          NS_ConvertUTF16toUTF8(devices[i]->Name()), devices[i]->GroupID(),
          devices[i]->MaxChannels(), mDelayAgnostic, mExtendedFilter);
      RefPtr<MediaDevice> device = MakeRefPtr<MediaDevice>(
          source, source->GetName(), NS_ConvertUTF8toUTF16(source->GetUUID()),
          source->GetGroupId(), NS_LITERAL_STRING(""));
      if (devices[i]->Preferred()) {
#ifdef DEBUG
        if (!foundPreferredDevice) {
          foundPreferredDevice = true;
        } else {
          // This is possible on windows, there is a default communication
          // device, and a default device:
          // See https://bugzilla.mozilla.org/show_bug.cgi?id=1542739
#  ifndef XP_WIN
          MOZ_ASSERT(!foundPreferredDevice,
                     "Found more than one preferred audio input device"
                     "while enumerating");
#  endif
        }
#endif
        aDevices->InsertElementAt(0, device);
      } else {
        aDevices->AppendElement(device);
      }
    }
  }
}

void MediaEngineWebRTC::EnumerateSpeakerDevices(
    uint64_t aWindowId, nsTArray<RefPtr<MediaDevice>>* aDevices) {
  if (!mEnumerator) {
    mEnumerator = CubebDeviceEnumerator::GetInstance();
  }
  nsTArray<RefPtr<AudioDeviceInfo>> devices;
  mEnumerator->EnumerateAudioOutputDevices(devices);

  DebugOnly<bool> preferredDeviceFound = false;
  for (auto& device : devices) {
    if (device->State() == CUBEB_DEVICE_STATE_ENABLED) {
      MOZ_ASSERT(device->Type() == CUBEB_DEVICE_TYPE_OUTPUT);
      nsString uuid(device->Name());
      // If, for example, input and output are in the same device, uuid
      // would be the same for both which ends up to create the same
      // deviceIDs (in JS).
      uuid.Append(NS_LITERAL_STRING("_Speaker"));
      nsString groupId(device->GroupID());
      if (device->Preferred()) {
#ifdef DEBUG
        MOZ_ASSERT(!preferredDeviceFound);
        preferredDeviceFound = true;
#endif
        aDevices->InsertElementAt(
            0, MakeRefPtr<MediaDevice>(device, uuid, groupId));
      } else {
        aDevices->AppendElement(MakeRefPtr<MediaDevice>(device, uuid, groupId));
      }
    }
  }
}

void MediaEngineWebRTC::EnumerateDevices(
    uint64_t aWindowId, dom::MediaSourceEnum aMediaSource,
    MediaSinkEnum aMediaSink, nsTArray<RefPtr<MediaDevice>>* aDevices) {
  MOZ_ASSERT(aMediaSource != dom::MediaSourceEnum::Other ||
             aMediaSink != MediaSinkEnum::Other);
  MutexAutoLock lock(mMutex);
  if (MediaEngineSource::IsVideo(aMediaSource)) {
    switch (aMediaSource) {
      case dom::MediaSourceEnum::Window:
        // Since the mediaSource constraint is deprecated, treat the Window
        // value as a request for getDisplayMedia-equivalent sharing: Combine
        // window and fullscreen into a single list of choices. The other values
        // are still useful for testing.
        EnumerateVideoDevices(aWindowId, camera::WinEngine, aDevices);
        EnumerateVideoDevices(aWindowId, camera::ScreenEngine, aDevices);
        break;
      case dom::MediaSourceEnum::Screen:
        EnumerateVideoDevices(aWindowId, camera::ScreenEngine, aDevices);
        break;
      case dom::MediaSourceEnum::Browser:
        EnumerateVideoDevices(aWindowId, camera::BrowserEngine, aDevices);
        break;
      case dom::MediaSourceEnum::Camera:
        EnumerateVideoDevices(aWindowId, camera::CameraEngine, aDevices);
        break;
      default:
        MOZ_CRASH("No valid video source");
        break;
    }
  } else if (aMediaSource == dom::MediaSourceEnum::AudioCapture) {
    RefPtr<MediaEngineWebRTCAudioCaptureSource> audioCaptureSource =
        new MediaEngineWebRTCAudioCaptureSource(nullptr);
    aDevices->AppendElement(MakeRefPtr<MediaDevice>(
        audioCaptureSource, audioCaptureSource->GetName(),
        NS_ConvertUTF8toUTF16(audioCaptureSource->GetUUID()),
        audioCaptureSource->GetGroupId(), NS_LITERAL_STRING("")));
  } else if (aMediaSource == dom::MediaSourceEnum::Microphone) {
    MOZ_ASSERT(aMediaSource == dom::MediaSourceEnum::Microphone);
    EnumerateMicrophoneDevices(aWindowId, aDevices);
  }

  if (aMediaSink == MediaSinkEnum::Speaker) {
    EnumerateSpeakerDevices(aWindowId, aDevices);
  }
}

void MediaEngineWebRTC::Shutdown() {
  // This is likely paranoia
  MutexAutoLock lock(mMutex);

  if (camera::GetCamerasChildIfExists()) {
    camera::GetChildAndCall(&camera::CamerasChild::RemoveDeviceChangeCallback,
                            this);
  }

  LOG(("%s", __FUNCTION__));
  mEnumerator = nullptr;
  mozilla::camera::Shutdown();
}

}  // namespace mozilla
