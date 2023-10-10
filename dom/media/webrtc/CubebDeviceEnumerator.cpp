/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CubebDeviceEnumerator.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/media/MediaUtils.h"
#include "nsThreadUtils.h"
#ifdef XP_WIN
#  include "mozilla/mscom/EnsureMTA.h"
#endif

namespace mozilla {

using namespace CubebUtils;
using AudioDeviceSet = CubebDeviceEnumerator::AudioDeviceSet;

/* static */
static StaticRefPtr<CubebDeviceEnumerator> sInstance;
static StaticMutex sInstanceMutex MOZ_UNANNOTATED;

/* static */
CubebDeviceEnumerator* CubebDeviceEnumerator::GetInstance() {
  StaticMutexAutoLock lock(sInstanceMutex);
  if (!sInstance) {
    sInstance = new CubebDeviceEnumerator();
    static bool clearOnShutdownSetup = []() -> bool {
      auto setClearOnShutdown = []() -> void {
        ClearOnShutdown(&sInstance, ShutdownPhase::XPCOMShutdownThreads);
      };
      if (NS_IsMainThread()) {
        setClearOnShutdown();
      } else {
        SchedulerGroup::Dispatch(
            NS_NewRunnableFunction("CubebDeviceEnumerator::::GetInstance()",
                                   std::move(setClearOnShutdown)));
      }
      return true;
    }();
    Unused << clearOnShutdownSetup;
  }
  return sInstance.get();
}

CubebDeviceEnumerator::CubebDeviceEnumerator()
    : mMutex("CubebDeviceListMutex"),
      mManualInputInvalidation(false),
      mManualOutputInvalidation(false) {
#ifdef XP_WIN
  // Ensure the MTA thread exists and gets instantiated before the
  // CubebDeviceEnumerator so that this instance will always gets destructed
  // before the MTA thread gets shutdown.
  mozilla::mscom::EnsureMTA([&]() -> void {
#endif
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
#ifdef XP_WIN
  });
#endif
}

/* static */
void CubebDeviceEnumerator::Shutdown() {
  StaticMutexAutoLock lock(sInstanceMutex);
  if (sInstance) {
    sInstance = nullptr;
  }
}

CubebDeviceEnumerator::~CubebDeviceEnumerator() {
#ifdef XP_WIN
  mozilla::mscom::EnsureMTA([&]() -> void {
#endif
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
#ifdef XP_WIN
  });
#endif
}

RefPtr<const AudioDeviceSet>
CubebDeviceEnumerator::EnumerateAudioInputDevices() {
  return EnumerateAudioDevices(Side::INPUT);
}

RefPtr<const AudioDeviceSet>
CubebDeviceEnumerator::EnumerateAudioOutputDevices() {
  return EnumerateAudioDevices(Side::OUTPUT);
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

static RefPtr<AudioDeviceSet> GetDeviceCollection(Side aSide) {
  RefPtr set = new AudioDeviceSet();
  cubeb* context = GetCubebContext();
  if (context) {
    cubeb_device_collection collection = {nullptr, 0};
#  ifdef XP_WIN
    mozilla::mscom::EnsureMTA([&]() -> void {
#  endif
      if (cubeb_enumerate_devices(context,
                                  aSide == Input ? CUBEB_DEVICE_TYPE_INPUT
                                                 : CUBEB_DEVICE_TYPE_OUTPUT,
                                  &collection) == CUBEB_OK) {
        for (unsigned int i = 0; i < collection.count; ++i) {
          auto device = collection.device[i];
          if (device.max_channels == 0) {
            continue;
          }
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
          set->AppendElement(std::move(info));
        }
      }
      cubeb_device_collection_destroy(context, &collection);
#  ifdef XP_WIN
    });
#  endif
  }
  return set;
}
#endif  // non ANDROID

RefPtr<const AudioDeviceSet> CubebDeviceEnumerator::EnumerateAudioDevices(
    CubebDeviceEnumerator::Side aSide) {
  MOZ_ASSERT(aSide == Side::INPUT || aSide == Side::OUTPUT);

  RefPtr<const AudioDeviceSet>* devicesCache;
  bool manualInvalidation = true;

  if (aSide == Side::INPUT) {
    devicesCache = &mInputDevices;
    manualInvalidation = mManualInputInvalidation;
  } else {
    MOZ_ASSERT(aSide == Side::OUTPUT);
    devicesCache = &mOutputDevices;
    manualInvalidation = mManualOutputInvalidation;
  }

  cubeb* context = GetCubebContext();
  if (!context) {
    return new AudioDeviceSet();
  }
  if (!manualInvalidation) {
    MutexAutoLock lock(mMutex);
    if (*devicesCache) {
      return *devicesCache;
    }
  }

#ifdef ANDROID
  cubeb_device_type type = CUBEB_DEVICE_TYPE_UNKNOWN;
  uint32_t channels = 0;
  nsAutoString name;
  if (aSide == Side::INPUT) {
    type = CUBEB_DEVICE_TYPE_INPUT;
    channels = 1;
    name = u"Default audio input device"_ns;
  } else {
    MOZ_ASSERT(aSide == Side::OUTPUT);
    type = CUBEB_DEVICE_TYPE_OUTPUT;
    channels = 2;
    name = u"Default audio output device"_ns;
  }
  RefPtr devices = new AudioDeviceSet();
  // Bug 1473346: enumerating devices is not supported on Android in cubeb,
  // simply state that there is a single sink, that it is the default, and has
  // a single channel. All the other values are made up and are not to be used.
  // Bug 1660391: we can't use fluent here yet to get localized strings, so
  // those are hard-coded en_US strings for now.
  RefPtr<AudioDeviceInfo> info = new AudioDeviceInfo(
      nullptr, name, u""_ns, u""_ns, type, CUBEB_DEVICE_STATE_ENABLED,
      CUBEB_DEVICE_PREF_ALL, CUBEB_DEVICE_FMT_ALL, CUBEB_DEVICE_FMT_S16NE,
      channels, 44100, 44100, 44100, 441, 128);
  devices->AppendElement(std::move(info));
#else
  RefPtr devices = GetDeviceCollection(
      (aSide == Side::INPUT) ? CubebUtils::Input : CubebUtils::Output);
#endif
  {
    MutexAutoLock lock(mMutex);
    *devicesCache = devices;
  }
  return devices;
}

already_AddRefed<AudioDeviceInfo> CubebDeviceEnumerator::DeviceInfoFromName(
    const nsString& aName, Side aSide) {
  RefPtr devices = EnumerateAudioDevices(aSide);
  for (const RefPtr<AudioDeviceInfo>& device : *devices) {
    if (device->Name().Equals(aName)) {
      RefPtr<AudioDeviceInfo> other = device;
      return other.forget();
    }
  }

  return nullptr;
}

RefPtr<AudioDeviceInfo> CubebDeviceEnumerator::DefaultDevice(Side aSide) {
  RefPtr devices = EnumerateAudioDevices(aSide);
  for (const RefPtr<AudioDeviceInfo>& device : *devices) {
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
  MutexAutoLock lock(mMutex);
  if (aSide == Side::INPUT) {
    mInputDevices = nullptr;
    mOnInputDeviceListChange.Notify();
  } else {
    MOZ_ASSERT(aSide == Side::OUTPUT);
    mOutputDevices = nullptr;
    mOnOutputDeviceListChange.Notify();
  }
}

}  // namespace mozilla
