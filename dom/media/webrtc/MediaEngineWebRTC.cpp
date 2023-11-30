/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineWebRTC.h"

#include "CamerasChild.h"
#include "MediaEngineRemoteVideoSource.h"
#include "MediaEngineWebRTCAudio.h"
#include "MediaManager.h"
#include "mozilla/Logging.h"

// Pipewire detection support
#if defined(WEBRTC_USE_PIPEWIRE)
#  include "mozilla/StaticPrefs_media.h"
#  include "modules/desktop_capture/desktop_capturer.h"
#endif

#define FAKE_ONDEVICECHANGE_EVENT_PERIOD_IN_MS 500

static mozilla::LazyLogModule sGetUserMediaLog("GetUserMedia");
#define LOG(args) MOZ_LOG(sGetUserMediaLog, mozilla::LogLevel::Debug, args)

namespace mozilla {

using AudioDeviceSet = CubebDeviceEnumerator::AudioDeviceSet;
using camera::CamerasChild;
using camera::GetChildAndCall;
using dom::MediaSourceEnum;

CubebDeviceEnumerator* GetEnumerator() {
  return CubebDeviceEnumerator::GetInstance();
}

MediaEngineWebRTC::MediaEngineWebRTC() {
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

void MediaEngineWebRTC::EnumerateVideoDevices(
    MediaSourceEnum aMediaSource, nsTArray<RefPtr<MediaDevice>>* aDevices) {
  AssertIsOnOwningThread();
  // flag sources with cross-origin exploit potential
  bool scaryKind = (aMediaSource == MediaSourceEnum::Screen ||
                    aMediaSource == MediaSourceEnum::Browser);
#if defined(WEBRTC_USE_PIPEWIRE)
  bool canRequestOsLevelPrompt =
      mozilla::StaticPrefs::media_webrtc_capture_allow_pipewire() &&
      webrtc::DesktopCapturer::IsRunningUnderWayland() &&
      (aMediaSource == MediaSourceEnum::Application ||
       aMediaSource == MediaSourceEnum::Screen ||
       aMediaSource == MediaSourceEnum::Window);
#else
  bool canRequestOsLevelPrompt = false;
#endif
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
  if (aMediaSource == MediaSourceEnum::Camera) {
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
  camera::CaptureEngine capEngine =
      MediaEngineRemoteVideoSource::CaptureEngine(aMediaSource);
  num = GetChildAndCall(&CamerasChild::NumberOfCaptureDevices, capEngine);

  for (int i = 0; i < num; i++) {
    char deviceName[MediaEngineSource::kMaxDeviceNameLength];
    char uniqueId[MediaEngineSource::kMaxUniqueIdLength];
    bool scarySource = false;
    bool placeholder = false;

    // paranoia
    deviceName[0] = '\0';
    uniqueId[0] = '\0';
    int error;

    error = GetChildAndCall(&CamerasChild::GetCaptureDevice, capEngine, i,
                            deviceName, sizeof(deviceName), uniqueId,
                            sizeof(uniqueId), &scarySource, &placeholder);
    if (error) {
      LOG(("camera:GetCaptureDevice: Failed %d", error));
      continue;
    }
#ifdef DEBUG
    LOG(("  Capture Device Index %d, Name %s", i, deviceName));

    webrtc::CaptureCapability cap;
    int numCaps = GetChildAndCall(&CamerasChild::NumberOfCapabilities,
                                  capEngine, uniqueId);
    LOG(("Number of Capabilities %d", numCaps));
    for (int j = 0; j < numCaps; j++) {
      if (GetChildAndCall(&CamerasChild::GetCaptureCapability, capEngine,
                          uniqueId, j, &cap) != 0) {
        break;
      }
      LOG(("type=%d width=%d height=%d maxFPS=%d",
           static_cast<int>(cap.videoType), cap.width, cap.height, cap.maxFPS));
    }
#endif

    NS_ConvertUTF8toUTF16 name(deviceName);
    NS_ConvertUTF8toUTF16 uuid(uniqueId);
    // The remote video backend doesn't implement group id. We return the
    // device name and higher layers will correlate this with the name of
    // audio devices.

    aDevices->EmplaceBack(
        new MediaDevice(this, aMediaSource, name, uuid, uuid,
                        MediaDevice::IsScary(scaryKind || scarySource),
                        canRequestOsLevelPrompt ? MediaDevice::OsPromptable::Yes
                                                : MediaDevice::OsPromptable::No,
                        MediaDevice::IsPlaceholder(placeholder)));
  }
}

void MediaEngineWebRTC::EnumerateMicrophoneDevices(
    nsTArray<RefPtr<MediaDevice>>* aDevices) {
  AssertIsOnOwningThread();

  RefPtr<const AudioDeviceSet> devices =
      GetEnumerator()->EnumerateAudioInputDevices();

  DebugOnly<bool> foundPreferredDevice = false;

  for (const auto& deviceInfo : *devices) {
#ifndef ANDROID
    MOZ_ASSERT(deviceInfo->DeviceID());
#endif
    LOG(("Cubeb device: type 0x%x, state 0x%x, name %s, id %p",
         deviceInfo->Type(), deviceInfo->State(),
         NS_ConvertUTF16toUTF8(deviceInfo->Name()).get(),
         deviceInfo->DeviceID()));

    if (deviceInfo->State() == CUBEB_DEVICE_STATE_ENABLED) {
      MOZ_ASSERT(deviceInfo->Type() == CUBEB_DEVICE_TYPE_INPUT);
      // Lie and provide the name as UUID
      RefPtr device = new MediaDevice(this, deviceInfo, deviceInfo->Name());
      if (deviceInfo->Preferred()) {
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
        aDevices->InsertElementAt(0, std::move(device));
      } else {
        aDevices->AppendElement(std::move(device));
      }
    }
  }
}

void MediaEngineWebRTC::EnumerateSpeakerDevices(
    nsTArray<RefPtr<MediaDevice>>* aDevices) {
  AssertIsOnOwningThread();

  RefPtr<const AudioDeviceSet> devices =
      GetEnumerator()->EnumerateAudioOutputDevices();

#ifndef XP_WIN
  DebugOnly<bool> preferredDeviceFound = false;
#endif
  for (const auto& deviceInfo : *devices) {
    LOG(("Cubeb device: type 0x%x, state 0x%x, name %s, id %p",
         deviceInfo->Type(), deviceInfo->State(),
         NS_ConvertUTF16toUTF8(deviceInfo->Name()).get(),
         deviceInfo->DeviceID()));
    if (deviceInfo->State() == CUBEB_DEVICE_STATE_ENABLED) {
      MOZ_ASSERT(deviceInfo->Type() == CUBEB_DEVICE_TYPE_OUTPUT);
      nsString uuid(deviceInfo->Name());
      // If, for example, input and output are in the same device, uuid
      // would be the same for both which ends up to create the same
      // deviceIDs (in JS).
      uuid.Append(u"_Speaker"_ns);
      RefPtr device = new MediaDevice(this, deviceInfo, uuid);
      if (deviceInfo->Preferred()) {
        // In windows is possible to have more than one preferred device
#if defined(DEBUG) && !defined(XP_WIN)
        MOZ_ASSERT(!preferredDeviceFound, "More than one preferred device");
        preferredDeviceFound = true;
#endif
        aDevices->InsertElementAt(0, std::move(device));
      } else {
        aDevices->AppendElement(std::move(device));
      }
    }
  }
}

void MediaEngineWebRTC::EnumerateDevices(
    MediaSourceEnum aMediaSource, MediaSinkEnum aMediaSink,
    nsTArray<RefPtr<MediaDevice>>* aDevices) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aMediaSource != MediaSourceEnum::Other ||
             aMediaSink != MediaSinkEnum::Other);
  if (MediaEngineSource::IsVideo(aMediaSource)) {
    switch (aMediaSource) {
      case MediaSourceEnum::Window:
        // Since the mediaSource constraint is deprecated, treat the Window
        // value as a request for getDisplayMedia-equivalent sharing: Combine
        // window and fullscreen into a single list of choices. The other values
        // are still useful for testing.
        EnumerateVideoDevices(MediaSourceEnum::Window, aDevices);
        EnumerateVideoDevices(MediaSourceEnum::Browser, aDevices);
        EnumerateVideoDevices(MediaSourceEnum::Screen, aDevices);
        break;
      case MediaSourceEnum::Screen:
      case MediaSourceEnum::Browser:
      case MediaSourceEnum::Camera:
        EnumerateVideoDevices(aMediaSource, aDevices);
        break;
      default:
        MOZ_CRASH("No valid video source");
        break;
    }
  } else if (aMediaSource == MediaSourceEnum::AudioCapture) {
    aDevices->EmplaceBack(new MediaDevice(
        this, aMediaSource, u"AudioCapture"_ns,
        MediaEngineWebRTCAudioCaptureSource::GetUUID(),
        MediaEngineWebRTCAudioCaptureSource::GetGroupId(),
        MediaDevice::IsScary::No, MediaDevice::OsPromptable::No));
  } else if (aMediaSource == MediaSourceEnum::Microphone) {
    EnumerateMicrophoneDevices(aDevices);
  }

  if (aMediaSink == MediaSinkEnum::Speaker) {
    EnumerateSpeakerDevices(aDevices);
  }
}

RefPtr<MediaEngineSource> MediaEngineWebRTC::CreateSource(
    const MediaDevice* aMediaDevice) {
  MOZ_ASSERT(aMediaDevice->mEngine == this);
  if (MediaEngineSource::IsVideo(aMediaDevice->mMediaSource)) {
    return new MediaEngineRemoteVideoSource(aMediaDevice);
  }
  switch (aMediaDevice->mMediaSource) {
    case MediaSourceEnum::AudioCapture:
      return new MediaEngineWebRTCAudioCaptureSource(aMediaDevice);
    case MediaSourceEnum::Microphone:
      return new MediaEngineWebRTCMicrophoneSource(aMediaDevice);
    default:
      MOZ_CRASH("Unsupported source type");
      return nullptr;
  }
}

void MediaEngineWebRTC::Shutdown() {
  AssertIsOnOwningThread();
  mCameraListChangeListener.DisconnectIfExists();
  mMicrophoneListChangeListener.DisconnectIfExists();
  mSpeakerListChangeListener.DisconnectIfExists();

  LOG(("%s", __FUNCTION__));
  mozilla::camera::Shutdown();
}

}  // namespace mozilla

#undef LOG
