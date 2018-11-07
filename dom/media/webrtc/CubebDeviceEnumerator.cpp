#include "CubebDeviceEnumerator.h"
#include "nsTArray.h"

namespace mozilla {

using namespace CubebUtils;

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

  cubeb* context = GetCubebContext();
  if (!context) {
    return;
  }

  MutexAutoLock lock(mMutex);

#ifdef ANDROID
  if (mDevices.IsEmpty()) {
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
    mDevices.AppendElement(info);
  }
#else
  if (mDevices.IsEmpty() || mManualInvalidation) {
    mDevices.Clear();
    CubebUtils::GetDeviceCollection(mDevices, CubebUtils::Input);
  }
#endif

  aOutDevices.AppendElements(mDevices);
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
