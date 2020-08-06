/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineWebRTC.h"

#include "CamerasChild.h"
#include "CSFLog.h"
#include "MediaEngineRemoteVideoSource.h"
#include "MediaEngineWebRTCAudio.h"
#include "MediaManager.h"
#include "MediaTrackConstraints.h"
#include "mozilla/dom/MediaDeviceInfo.h"
#include "mozilla/Logging.h"
#include "nsIComponentRegistrar.h"
#include "prenv.h"

#define FAKE_ONDEVICECHANGE_EVENT_PERIOD_IN_MS 500

static mozilla::LazyLogModule sGetUserMediaLog("GetUserMedia");
#undef LOG
#define LOG(args) MOZ_LOG(sGetUserMediaLog, mozilla::LogLevel::Debug, args)

namespace mozilla {

using camera::CamerasChild;
using camera::GetChildAndCall;

CubebDeviceEnumerator* GetEnumerator() {
  return CubebDeviceEnumerator::GetInstance();
}

MediaEngineWebRTC::MediaEngineWebRTC(MediaEnginePrefs& aPrefs)
    : mDelayAgnostic(aPrefs.mDelayAgnostic),
      mExtendedFilter(aPrefs.mExtendedFilter) {
  AssertIsOnOwningThread();

  GetChildAndCall(
      &CamerasChild::ConnectDeviceListChangeListener<MediaEngineWebRTC>,
      &mCameraListChangeListener, AbstractThread::MainThread(), this,
      &MediaEngineWebRTC::DeviceListChanged);
  mMicrophoneListChangeListener =
      GetEnumerator()->OnAudioInputDeviceListChange().Connect(
          AbstractThread::MainThread(), this,
          &MediaEngineWebRTC::DeviceListChanged);
  mSpeakerListChangeListener =
      GetEnumerator()->OnAudioOutputDeviceListChange().Connect(
          AbstractThread::MainThread(), this,
          &MediaEngineWebRTC::DeviceListChanged);
}

void MediaEngineWebRTC::SetFakeDeviceChangeEventsEnabled(bool aEnable) {
  AssertIsOnOwningThread();

  // To simulate the devicechange event in mochitest, we schedule a timer to
  // issue "devicechange" repeatedly until disabled.

  if (aEnable && !mFakeDeviceChangeEventTimer) {
    NS_NewTimerWithFuncCallback(
        getter_AddRefs(mFakeDeviceChangeEventTimer),
        &FakeDeviceChangeEventTimerTick, this,
        FAKE_ONDEVICECHANGE_EVENT_PERIOD_IN_MS, nsITimer::TYPE_REPEATING_SLACK,
        "MediaEngineWebRTC::mFakeDeviceChangeEventTimer",
        GetCurrentSerialEventTarget());
    return;
  }

  if (!aEnable && mFakeDeviceChangeEventTimer) {
    mFakeDeviceChangeEventTimer->Cancel();
    mFakeDeviceChangeEventTimer = nullptr;
    return;
  }
}

void MediaEngineWebRTC::EnumerateVideoDevices(
    uint64_t aWindowId, camera::CaptureEngine aCapEngine,
    nsTArray<RefPtr<MediaDevice>>* aDevices) {
  AssertIsOnOwningThread();

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
  num = GetChildAndCall(&CamerasChild::NumberOfCaptureDevices, aCapEngine);

  for (int i = 0; i < num; i++) {
    char deviceName[MediaEngineSource::kMaxDeviceNameLength];
    char uniqueId[MediaEngineSource::kMaxUniqueIdLength];
    bool scarySource = false;

    // paranoia
    deviceName[0] = '\0';
    uniqueId[0] = '\0';
    int error;

    error = GetChildAndCall(&CamerasChild::GetCaptureDevice, aCapEngine, i,
                            deviceName, sizeof(deviceName), uniqueId,
                            sizeof(uniqueId), &scarySource);
    if (error) {
      LOG(("camera:GetCaptureDevice: Failed %d", error));
      continue;
    }
#ifdef DEBUG
    LOG(("  Capture Device Index %d, Name %s", i, deviceName));

    webrtc::CaptureCapability cap;
    int numCaps = GetChildAndCall(&CamerasChild::NumberOfCapabilities,
                                  aCapEngine, uniqueId);
    LOG(("Number of Capabilities %d", numCaps));
    for (int j = 0; j < numCaps; j++) {
      if (GetChildAndCall(&CamerasChild::GetCaptureCapability, aCapEngine,
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
        vSource->GetGroupId(), u""_ns));
  }
}

void MediaEngineWebRTC::EnumerateMicrophoneDevices(
    uint64_t aWindowId, nsTArray<RefPtr<MediaDevice>>* aDevices) {
  AssertIsOnOwningThread();

  nsTArray<RefPtr<AudioDeviceInfo>> devices;
  GetEnumerator()->EnumerateAudioInputDevices(devices);

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
          source->GetGroupId(), u""_ns);
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
  AssertIsOnOwningThread();

  nsTArray<RefPtr<AudioDeviceInfo>> devices;
  GetEnumerator()->EnumerateAudioOutputDevices(devices);

#ifndef XP_WIN
  DebugOnly<bool> preferredDeviceFound = false;
#endif
  for (auto& device : devices) {
    if (device->State() == CUBEB_DEVICE_STATE_ENABLED) {
      MOZ_ASSERT(device->Type() == CUBEB_DEVICE_TYPE_OUTPUT);
      nsString uuid(device->Name());
      // If, for example, input and output are in the same device, uuid
      // would be the same for both which ends up to create the same
      // deviceIDs (in JS).
      uuid.Append(u"_Speaker"_ns);
      nsString groupId(device->GroupID());
      if (device->Preferred()) {
        // In windows is possible to have more than one preferred device
#if defined(DEBUG) && !defined(XP_WIN)
        MOZ_ASSERT(!preferredDeviceFound, "More than one preferred device");
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
  AssertIsOnOwningThread();
  MOZ_ASSERT(aMediaSource != dom::MediaSourceEnum::Other ||
             aMediaSink != MediaSinkEnum::Other);
  if (MediaEngineSource::IsVideo(aMediaSource)) {
    switch (aMediaSource) {
      case dom::MediaSourceEnum::Window:
        // Since the mediaSource constraint is deprecated, treat the Window
        // value as a request for getDisplayMedia-equivalent sharing: Combine
        // window and fullscreen into a single list of choices. The other values
        // are still useful for testing.
        EnumerateVideoDevices(aWindowId, camera::WinEngine, aDevices);
        EnumerateVideoDevices(aWindowId, camera::BrowserEngine, aDevices);
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
        audioCaptureSource->GetGroupId(), u""_ns));
  } else if (aMediaSource == dom::MediaSourceEnum::Microphone) {
    EnumerateMicrophoneDevices(aWindowId, aDevices);
  }

  if (aMediaSink == MediaSinkEnum::Speaker) {
    EnumerateSpeakerDevices(aWindowId, aDevices);
  }
}

void MediaEngineWebRTC::Shutdown() {
  AssertIsOnOwningThread();
  MOZ_DIAGNOSTIC_ASSERT(!mFakeDeviceChangeEventTimer);
  mCameraListChangeListener.DisconnectIfExists();
  mMicrophoneListChangeListener.DisconnectIfExists();
  mSpeakerListChangeListener.DisconnectIfExists();

  LOG(("%s", __FUNCTION__));
  mozilla::camera::Shutdown();
}

/* static */ void MediaEngineWebRTC::FakeDeviceChangeEventTimerTick(
    nsITimer* aTimer, void* aClosure) {
  MediaEngineWebRTC* self = static_cast<MediaEngineWebRTC*>(aClosure);
  self->AssertIsOnOwningThread();
  self->DeviceListChanged();
}

}  // namespace mozilla
