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

using namespace CubebUtils;

MediaEngineWebRTC::MediaEngineWebRTC(MediaEnginePrefs &aPrefs)
  : mMutex("mozilla::MediaEngineWebRTC")
  , mDelayAgnostic(aPrefs.mDelayAgnostic)
  , mExtendedFilter(aPrefs.mExtendedFilter)
  , mHasTabVideoSource(false)
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
MediaEngineWebRTC::EnumerateVideoDevices(uint64_t aWindowId,
                                         dom::MediaSourceEnum aMediaSource,
                                         nsTArray<RefPtr<MediaDevice> >* aDevices)
{
  mMutex.AssertCurrentThreadOwns();

  mozilla::camera::CaptureEngine capEngine = mozilla::camera::InvalidEngine;

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
    } else {
      vSource = new MediaEngineRemoteVideoSource(i, capEngine, aMediaSource,
                                                 scaryKind || scarySource);
      devicesForThisWindow->Put(uuid, vSource);
    }
    aDevices->AppendElement(MakeRefPtr<MediaDevice>(
                              vSource,
                              vSource->GetName(),
                              NS_ConvertUTF8toUTF16(vSource->GetUUID())));
  }

  if (mHasTabVideoSource || dom::MediaSourceEnum::Browser == aMediaSource) {
    RefPtr<MediaEngineSource> tabVideoSource = new MediaEngineTabVideoSource();
    aDevices->AppendElement(MakeRefPtr<MediaDevice>(
                              tabVideoSource,
                              tabVideoSource->GetName(),
                              NS_ConvertUTF8toUTF16(tabVideoSource->GetUUID())));
  }
}

void
MediaEngineWebRTC::EnumerateMicrophoneDevices(uint64_t aWindowId,
                                              nsTArray<RefPtr<MediaDevice> >* aDevices)
{
  mMutex.AssertCurrentThreadOwns();

  if (!mEnumerator) {
    mEnumerator.reset(new CubebDeviceEnumerator());
  }

  nsTArray<RefPtr<AudioDeviceInfo>> devices;
  mEnumerator->EnumerateAudioInputDevices(devices);

  DebugOnly<bool> foundPreferredDevice = false;

  for (uint32_t i = 0; i < devices.Length(); i++) {
#ifndef ANDROID
    MOZ_ASSERT(devices[i]->DeviceID());
#endif
    LOG(("Cubeb device %u: type 0x%x, state 0x%x, name %s, id %p",
          i,
          devices[i]->Type(),
          devices[i]->State(),
          NS_ConvertUTF16toUTF8(devices[i]->Name()).get(),
          devices[i]->DeviceID()));

    if (devices[i]->State() == CUBEB_DEVICE_STATE_ENABLED) {
      MOZ_ASSERT(devices[i]->Type() == CUBEB_DEVICE_TYPE_INPUT);
      RefPtr<MediaEngineSource> source =
        new MediaEngineWebRTCMicrophoneSource(
            devices[i],
            devices[i]->Name(),
            // Lie and provide the name as UUID
            NS_ConvertUTF16toUTF8(devices[i]->Name()),
            devices[i]->MaxChannels(),
            mDelayAgnostic,
            mExtendedFilter);
      RefPtr<MediaDevice> device = MakeRefPtr<MediaDevice>(
                                     source,
                                     source->GetName(),
                                     NS_ConvertUTF8toUTF16(source->GetUUID()));
      if (devices[i]->Preferred()) {
#ifdef DEBUG
        if (!foundPreferredDevice) {
          foundPreferredDevice = true;
        } else {
          MOZ_ASSERT(!foundPreferredDevice,
              "Found more than one preferred audio input device"
              "while enumerating");
        }
#endif
        aDevices->InsertElementAt(0, device);
      } else {
        aDevices->AppendElement(device);
      }
    }
  }
}

void
MediaEngineWebRTC::EnumerateSpeakerDevices(uint64_t aWindowId,
                                           nsTArray<RefPtr<MediaDevice> >* aDevices)
{
  nsTArray<RefPtr<AudioDeviceInfo>> devices;
  CubebUtils::GetDeviceCollection(devices, CubebUtils::Output);
  for (auto& device : devices) {
    if (device->State() == CUBEB_DEVICE_STATE_ENABLED) {
      MOZ_ASSERT(device->Type() == CUBEB_DEVICE_TYPE_OUTPUT);
      nsString uuid(device->Name());
      // If, for example, input and output are in the same device, uuid
      // would be the same for both which ends up to create the same
      // deviceIDs (in JS).
      uuid.Append(NS_LITERAL_STRING("_Speaker"));
      aDevices->AppendElement(MakeRefPtr<MediaDevice>(
                                device->Name(),
                                dom::MediaDeviceKind::Audiooutput,
                                uuid));
    }
  }
}


void
MediaEngineWebRTC::EnumerateDevices(uint64_t aWindowId,
                                    dom::MediaSourceEnum aMediaSource,
                                    MediaSinkEnum aMediaSink,
                                    nsTArray<RefPtr<MediaDevice>>* aDevices)
{
  MOZ_ASSERT(aMediaSource != dom::MediaSourceEnum::Other ||
             aMediaSink != MediaSinkEnum::Other);
  // We spawn threads to handle gUM runnables, so we must protect the member vars
  MutexAutoLock lock(mMutex);
  if (MediaEngineSource::IsVideo(aMediaSource)) {
    EnumerateVideoDevices(aWindowId, aMediaSource, aDevices);
  } else if (aMediaSource == dom::MediaSourceEnum::AudioCapture) {
    RefPtr<MediaEngineWebRTCAudioCaptureSource> audioCaptureSource =
      new MediaEngineWebRTCAudioCaptureSource(nullptr);
    aDevices->AppendElement(MakeRefPtr<MediaDevice>(
                              audioCaptureSource,
                              audioCaptureSource->GetName(),
                              NS_ConvertUTF8toUTF16(audioCaptureSource->GetUUID())));
  } else if (aMediaSource == dom::MediaSourceEnum::Microphone) {
    MOZ_ASSERT(aMediaSource == dom::MediaSourceEnum::Microphone);
    EnumerateMicrophoneDevices(aWindowId, aDevices);
  }

  if (aMediaSink == MediaSinkEnum::Speaker) {
    EnumerateSpeakerDevices(aWindowId, aDevices);
  }
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

  mEnumerator = nullptr;

  mozilla::camera::Shutdown();
}

CubebDeviceEnumerator::CubebDeviceEnumerator()
  : mMutex("CubebDeviceListMutex")
  , mManualInvalidation(false)
{
  int rv = cubeb_register_device_collection_changed(GetCubebContext(),
     CUBEB_DEVICE_TYPE_INPUT,
     &mozilla::CubebDeviceEnumerator::AudioDeviceListChanged_s,
     this);

  if (rv != CUBEB_OK) {
    NS_WARNING("Could not register the audio input"
               " device collection changed callback.");
    mManualInvalidation = true;
  }
}

CubebDeviceEnumerator::~CubebDeviceEnumerator()
{
  int rv = cubeb_register_device_collection_changed(GetCubebContext(),
                                                    CUBEB_DEVICE_TYPE_INPUT,
                                                    nullptr,
                                                    this);
  if (rv != CUBEB_OK) {
    NS_WARNING("Could not unregister the audio input"
               " device collection changed callback.");
  }
}

void
CubebDeviceEnumerator::EnumerateAudioInputDevices(nsTArray<RefPtr<AudioDeviceInfo>>& aOutDevices)
{
  aOutDevices.Clear();

#ifdef ANDROID
  // Bug 1473346: enumerating devices is not supported on Android in cubeb,
  // simply state that there is a single mic, that it is the default, and has a
  // single channel. All the other values are made up and are not to be used.
  RefPtr<AudioDeviceInfo> info = new AudioDeviceInfo(nullptr,
                                                     NS_ConvertUTF8toUTF16(""),
                                                     NS_ConvertUTF8toUTF16(""),
                                                     NS_ConvertUTF8toUTF16(""),
                                                     CUBEB_DEVICE_TYPE_INPUT,
                                                     CUBEB_DEVICE_STATE_ENABLED,
                                                     CUBEB_DEVICE_PREF_ALL,
                                                     CUBEB_DEVICE_FMT_ALL,
                                                     CUBEB_DEVICE_FMT_S16NE,
                                                     1,
                                                     44100,
                                                     44100,
                                                     41000,
                                                     410,
                                                     128);
  if (mDevices.IsEmpty()) {
    mDevices.AppendElement(info);
  }
  aOutDevices.AppendElements(mDevices);
#else
  cubeb* context = GetCubebContext();

  if (!context) {
    return;
  }

  MutexAutoLock lock(mMutex);

  if (mDevices.IsEmpty() || mManualInvalidation) {
    mDevices.Clear();
    CubebUtils::GetDeviceCollection(mDevices, CubebUtils::Input);
  }

  aOutDevices.AppendElements(mDevices);
#endif
}

already_AddRefed<AudioDeviceInfo>
CubebDeviceEnumerator::DeviceInfoFromID(CubebUtils::AudioDeviceID aID)
{
  MutexAutoLock lock(mMutex);

  for (uint32_t i  = 0; i < mDevices.Length(); i++) {
    if (mDevices[i]->DeviceID() == aID) {
      RefPtr<AudioDeviceInfo> other = mDevices[i];
      return other.forget();
    }
  }
  return nullptr;
}

void
CubebDeviceEnumerator::AudioDeviceListChanged_s(cubeb* aContext, void* aUser)
{
  CubebDeviceEnumerator* self = reinterpret_cast<CubebDeviceEnumerator*>(aUser);
  self->AudioDeviceListChanged();
}

void
CubebDeviceEnumerator::AudioDeviceListChanged()
{
  MutexAutoLock lock(mMutex);

  mDevices.Clear();
}

}
