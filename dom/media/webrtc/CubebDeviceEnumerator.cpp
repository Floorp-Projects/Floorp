#include "CubebDeviceEnumerator.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {

using namespace CubebUtils;

/* static */
StaticRefPtr<CubebDeviceEnumerator> CubebDeviceEnumerator::sInstance;

/* static */
already_AddRefed<CubebDeviceEnumerator> CubebDeviceEnumerator::GetInstance() {
  if (!sInstance) {
    sInstance = new CubebDeviceEnumerator();
  }
  RefPtr<CubebDeviceEnumerator> instance = sInstance.get();
  MOZ_ASSERT(instance);
  return instance.forget();
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
    CubebUtils::GetDeviceCollection(devices, (aSide == Side::INPUT)
                                                 ? CubebUtils::Input
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
  for (uint32_t i = 0; i < mInputDevices.Length(); i++) {
    if (mInputDevices[i]->DeviceID() == aID) {
      RefPtr<AudioDeviceInfo> other = mInputDevices[i];
      return other.forget();
    }
  }

  if (mOutputDevices.IsEmpty() || mManualOutputInvalidation) {
    EnumerateAudioDevices(Side::OUTPUT);
  }
  for (uint32_t i = 0; i < mOutputDevices.Length(); i++) {
    if (mOutputDevices[i]->DeviceID() == aID) {
      RefPtr<AudioDeviceInfo> other = mOutputDevices[i];
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
  for (uint32_t i = 0; i < devices.Length(); i++) {
    if (devices[i]->Name().Equals(aName)) {
      RefPtr<AudioDeviceInfo> other = devices[i];
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
    mInputDevices.Clear();
  } else {
    MOZ_ASSERT(aSide == Side::OUTPUT);
    mOutputDevices.Clear();
  }
}

}  // namespace mozilla
