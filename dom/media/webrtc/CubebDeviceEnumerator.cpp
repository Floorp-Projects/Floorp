/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CubebDeviceEnumerator.h"
#include "mozilla/StaticPtr.h"
#include "nsThreadUtils.h"

namespace mozilla {

using namespace CubebUtils;

/* static */
StaticRefPtr<CubebDeviceEnumerator> CubebDeviceEnumerator::sInstance;

/* static */
CubebDeviceEnumerator* CubebDeviceEnumerator::GetInstance() {
  if (!sInstance) {
    sInstance = new CubebDeviceEnumerator();
  }
  return sInstance.get();
}

CubebDeviceEnumerator::CubebDeviceEnumerator()
    : mMutex("CubebDeviceListMutex"),
      mManualInputInvalidation(false),
      mManualOutputInvalidation(false) {
  int rv = cubeb_register_device_collection_changed(
      GetCubebContext(), CUBEB_DEVICE_TYPE_OUTPUT,
      &OutputAudioDeviceListChanged_s, this);
  if (rv != CUBEB_OK) {
    NS_WARNING(
        "Could not register the audio output"
        " device collection changed callback.");
    mManualOutputInvalidation = true;
  }
  rv = cubeb_register_device_collection_changed(
      GetCubebContext(), CUBEB_DEVICE_TYPE_INPUT,
      &InputAudioDeviceListChanged_s, this);
  if (rv != CUBEB_OK) {
    NS_WARNING(
        "Could not register the audio input"
        " device collection changed callback.");
    mManualInputInvalidation = true;
  }
}

/* static */
void CubebDeviceEnumerator::Shutdown() {
  if (sInstance) {
    sInstance = nullptr;
  }
}

CubebDeviceEnumerator::~CubebDeviceEnumerator() {
  int rv = cubeb_register_device_collection_changed(
      GetCubebContext(), CUBEB_DEVICE_TYPE_OUTPUT, nullptr, this);
  if (rv != CUBEB_OK) {
    NS_WARNING(
        "Could not unregister the audio output"
        " device collection changed callback.");
  }
  rv = cubeb_register_device_collection_changed(
      GetCubebContext(), CUBEB_DEVICE_TYPE_INPUT, nullptr, this);
  if (rv != CUBEB_OK) {
    NS_WARNING(
        "Could not unregister the audio input"
        " device collection changed callback.");
  }
}

void CubebDeviceEnumerator::EnumerateAudioInputDevices(
    nsTArray<RefPtr<AudioDeviceInfo>>& aOutDevices) {
  MutexAutoLock lock(mMutex);
  aOutDevices.Clear();
  EnumerateAudioDevices(Side::INPUT);
  aOutDevices.AppendElements(mInputDevices);
}

void CubebDeviceEnumerator::EnumerateAudioOutputDevices(
    nsTArray<RefPtr<AudioDeviceInfo>>& aOutDevices) {
  MutexAutoLock lock(mMutex);
  aOutDevices.Clear();
  EnumerateAudioDevices(Side::OUTPUT);
  aOutDevices.AppendElements(mOutputDevices);
}

#ifndef ANDROID
static uint16_t ConvertCubebType(cubeb_device_type aType) {
  uint16_t map[] = {
      nsIAudioDeviceInfo::TYPE_UNKNOWN,  // CUBEB_DEVICE_TYPE_UNKNOWN
      nsIAudioDeviceInfo::TYPE_INPUT,    // CUBEB_DEVICE_TYPE_INPUT,
      nsIAudioDeviceInfo::TYPE_OUTPUT    // CUBEB_DEVICE_TYPE_OUTPUT
  };
  return map[aType];
}

static uint16_t ConvertCubebState(cubeb_device_state aState) {
  uint16_t map[] = {
      nsIAudioDeviceInfo::STATE_DISABLED,   // CUBEB_DEVICE_STATE_DISABLED
      nsIAudioDeviceInfo::STATE_UNPLUGGED,  // CUBEB_DEVICE_STATE_UNPLUGGED
      nsIAudioDeviceInfo::STATE_ENABLED     // CUBEB_DEVICE_STATE_ENABLED
  };
  return map[aState];
}

static uint16_t ConvertCubebPreferred(cubeb_device_pref aPreferred) {
  if (aPreferred == CUBEB_DEVICE_PREF_NONE) {
    return nsIAudioDeviceInfo::PREF_NONE;
  }
  if (aPreferred == CUBEB_DEVICE_PREF_ALL) {
    return nsIAudioDeviceInfo::PREF_ALL;
  }

  uint16_t preferred = 0;
  if (aPreferred & CUBEB_DEVICE_PREF_MULTIMEDIA) {
    preferred |= nsIAudioDeviceInfo::PREF_MULTIMEDIA;
  }
  if (aPreferred & CUBEB_DEVICE_PREF_VOICE) {
    preferred |= nsIAudioDeviceInfo::PREF_VOICE;
  }
  if (aPreferred & CUBEB_DEVICE_PREF_NOTIFICATION) {
    preferred |= nsIAudioDeviceInfo::PREF_NOTIFICATION;
  }
  return preferred;
}

static uint16_t ConvertCubebFormat(cubeb_device_fmt aFormat) {
  uint16_t format = 0;
  if (aFormat & CUBEB_DEVICE_FMT_S16LE) {
    format |= nsIAudioDeviceInfo::FMT_S16LE;
  }
  if (aFormat & CUBEB_DEVICE_FMT_S16BE) {
    format |= nsIAudioDeviceInfo::FMT_S16BE;
  }
  if (aFormat & CUBEB_DEVICE_FMT_F32LE) {
    format |= nsIAudioDeviceInfo::FMT_F32LE;
  }
  if (aFormat & CUBEB_DEVICE_FMT_F32BE) {
    format |= nsIAudioDeviceInfo::FMT_F32BE;
  }
  return format;
}

static void GetDeviceCollection(nsTArray<RefPtr<AudioDeviceInfo>>& aDeviceInfos,
                                Side aSide) {
  cubeb* context = GetCubebContext();
  if (context) {
    cubeb_device_collection collection = {nullptr, 0};
    if (cubeb_enumerate_devices(
            context,
            aSide == Input ? CUBEB_DEVICE_TYPE_INPUT : CUBEB_DEVICE_TYPE_OUTPUT,
            &collection) == CUBEB_OK) {
      for (unsigned int i = 0; i < collection.count; ++i) {
        auto device = collection.device[i];
        RefPtr<AudioDeviceInfo> info = new AudioDeviceInfo(
            device.devid, NS_ConvertUTF8toUTF16(device.friendly_name),
            NS_ConvertUTF8toUTF16(device.group_id),
            NS_ConvertUTF8toUTF16(device.vendor_name),
            ConvertCubebType(device.type), ConvertCubebState(device.state),
            ConvertCubebPreferred(device.preferred),
            ConvertCubebFormat(device.format),
            ConvertCubebFormat(device.default_format), device.max_channels,
            device.default_rate, device.max_rate, device.min_rate,
            device.latency_hi, device.latency_lo);
        aDeviceInfos.AppendElement(info);
      }
    }
    cubeb_device_collection_destroy(context, &collection);
  }
}
#endif  // non ANDROID

void CubebDeviceEnumerator::EnumerateAudioDevices(
    CubebDeviceEnumerator::Side aSide) {
  mMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(aSide == Side::INPUT || aSide == Side::OUTPUT);

  nsTArray<RefPtr<AudioDeviceInfo>> devices;
  bool manualInvalidation = true;

  if (aSide == Side::INPUT) {
    devices.SwapElements(mInputDevices);
    manualInvalidation = mManualInputInvalidation;
  } else {
    MOZ_ASSERT(aSide == Side::OUTPUT);
    devices.SwapElements(mOutputDevices);
    manualInvalidation = mManualOutputInvalidation;
  }

  cubeb* context = GetCubebContext();
  if (!context) {
    return;
  }

#ifdef ANDROID
  cubeb_device_type type = CUBEB_DEVICE_TYPE_UNKNOWN;
  uint32_t channels = 0;
  if (aSide == Side::INPUT) {
    type = CUBEB_DEVICE_TYPE_INPUT;
    channels = 1;
  } else {
    MOZ_ASSERT(aSide == Side::OUTPUT);
    type = CUBEB_DEVICE_TYPE_OUTPUT;
    channels = 2;
  }

  if (devices.IsEmpty()) {
    // Bug 1473346: enumerating devices is not supported on Android in cubeb,
    // simply state that there is a single sink, that it is the default, and has
    // a single channel. All the other values are made up and are not to be
    // used.
    RefPtr<AudioDeviceInfo> info = new AudioDeviceInfo(
        nullptr, NS_ConvertUTF8toUTF16(""), NS_ConvertUTF8toUTF16(""),
        NS_ConvertUTF8toUTF16(""), type, CUBEB_DEVICE_STATE_ENABLED,
        CUBEB_DEVICE_PREF_ALL, CUBEB_DEVICE_FMT_ALL, CUBEB_DEVICE_FMT_S16NE,
        channels, 44100, 44100, 41000, 410, 128);
    devices.AppendElement(info);
  }
#else
  if (devices.IsEmpty() || manualInvalidation) {
    devices.Clear();

    MutexAutoUnlock unlock(mMutex);
    GetDeviceCollection(devices, (aSide == Side::INPUT) ? CubebUtils::Input
                                                        : CubebUtils::Output);
  }
#endif

  if (aSide == Side::INPUT) {
    mInputDevices.AppendElements(devices);
  } else {
    mOutputDevices.AppendElements(devices);
  }
}

already_AddRefed<AudioDeviceInfo> CubebDeviceEnumerator::DeviceInfoFromID(
    CubebUtils::AudioDeviceID aID) {
  MutexAutoLock lock(mMutex);

  if (mInputDevices.IsEmpty() || mManualInputInvalidation) {
    EnumerateAudioDevices(Side::INPUT);
  }
  for (RefPtr<AudioDeviceInfo>& device : mInputDevices) {
    if (device->DeviceID() == aID) {
      RefPtr<AudioDeviceInfo> other = device;
      return other.forget();
    }
  }

  if (mOutputDevices.IsEmpty() || mManualOutputInvalidation) {
    EnumerateAudioDevices(Side::OUTPUT);
  }
  for (RefPtr<AudioDeviceInfo>& device : mOutputDevices) {
    if (device->DeviceID() == aID) {
      RefPtr<AudioDeviceInfo> other = device;
      return other.forget();
    }
  }
  return nullptr;
}

already_AddRefed<AudioDeviceInfo> CubebDeviceEnumerator::DeviceInfoFromName(
    const nsString& aName) {
  RefPtr<AudioDeviceInfo> other = DeviceInfoFromName(aName, Side::INPUT);
  if (other) {
    return other.forget();
  }
  return DeviceInfoFromName(aName, Side::OUTPUT);
}

already_AddRefed<AudioDeviceInfo> CubebDeviceEnumerator::DeviceInfoFromName(
    const nsString& aName, Side aSide) {
  MutexAutoLock lock(mMutex);

  nsTArray<RefPtr<AudioDeviceInfo>>& devices =
      (aSide == Side::INPUT) ? mInputDevices : mOutputDevices;
  bool manualInvalidation = (aSide == Side::INPUT) ? mManualInputInvalidation
                                                   : mManualOutputInvalidation;

  if (devices.IsEmpty() || manualInvalidation) {
    EnumerateAudioDevices(aSide);
  }
  for (RefPtr<AudioDeviceInfo>& device : devices) {
    if (device->Name().Equals(aName)) {
      RefPtr<AudioDeviceInfo> other = device;
      return other.forget();
    }
  }

  return nullptr;
}

RefPtr<AudioDeviceInfo> CubebDeviceEnumerator::DefaultDevice(Side aSide) {
  MutexAutoLock lock(mMutex);

  nsTArray<RefPtr<AudioDeviceInfo>>& devices =
      (aSide == Side::INPUT) ? mInputDevices : mOutputDevices;
  bool manualInvalidation = (aSide == Side::INPUT) ? mManualInputInvalidation
                                                   : mManualOutputInvalidation;

  if (devices.IsEmpty() || manualInvalidation) {
    EnumerateAudioDevices(aSide);
  }
  for (RefPtr<AudioDeviceInfo>& device : devices) {
    if (device->Preferred()) {
      RefPtr<AudioDeviceInfo> other = device;
      return other.forget();
    }
  }

  return nullptr;
}

void CubebDeviceEnumerator::InputAudioDeviceListChanged_s(cubeb* aContext,
                                                          void* aUser) {
  CubebDeviceEnumerator* self = reinterpret_cast<CubebDeviceEnumerator*>(aUser);
  self->AudioDeviceListChanged(CubebDeviceEnumerator::Side::INPUT);
}

void CubebDeviceEnumerator::OutputAudioDeviceListChanged_s(cubeb* aContext,
                                                           void* aUser) {
  CubebDeviceEnumerator* self = reinterpret_cast<CubebDeviceEnumerator*>(aUser);
  self->AudioDeviceListChanged(CubebDeviceEnumerator::Side::OUTPUT);
}

void CubebDeviceEnumerator::AudioDeviceListChanged(Side aSide) {
  {
    MutexAutoLock lock(mMutex);
    if (aSide == Side::INPUT) {
      mInputDevices.Clear();
      mOnInputDeviceListChange.Notify();
    } else {
      MOZ_ASSERT(aSide == Side::OUTPUT);
      mOutputDevices.Clear();
      mOnOutputDeviceListChange.Notify();
    }
  }
}

}  // namespace mozilla
