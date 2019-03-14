/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CUBEBDEVICEENUMERATOR_H_
#define CUBEBDEVICEENUMERATOR_H_

#include "AudioDeviceInfo.h"
#include "CubebUtils.h"
#include "cubeb/cubeb.h"
#include "mozilla/Mutex.h"
#include "nsTArray.h"

namespace mozilla {
// This class implements a cache for accessing the audio device list.
// It can be accessed on any thread.
class CubebDeviceEnumerator final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CubebDeviceEnumerator)

  static already_AddRefed<CubebDeviceEnumerator> GetInstance();
  static void Shutdown();
  // This method returns a list of all the input audio devices
  // (sources) available on this machine.
  // This method is safe to call from all threads.
  void EnumerateAudioInputDevices(
      nsTArray<RefPtr<AudioDeviceInfo>>& aOutDevices);
  // Similar for the audio audio devices (sinks). Also thread safe.
  void EnumerateAudioOutputDevices(
      nsTArray<RefPtr<AudioDeviceInfo>>& aOutDevices);
  // From a cubeb device id, return the info for this device, if it's still a
  // valid id, or nullptr otherwise.
  // This method is safe to call from any thread.
  already_AddRefed<AudioDeviceInfo> DeviceInfoFromID(
      CubebUtils::AudioDeviceID aID);
  // From a device name, return the info for this device, if it's a valid name,
  // or nullptr otherwise.
  // This method is safe to call from any thread.
  already_AddRefed<AudioDeviceInfo> DeviceInfoFromName(const nsString& aName);
  enum class Side {
    INPUT,
    OUTPUT,
  };
  already_AddRefed<AudioDeviceInfo> DeviceInfoFromName(const nsString& aName,
                                                       Side aSide);

 private:
  CubebDeviceEnumerator();
  ~CubebDeviceEnumerator();
  // Static functions called by cubeb when the audio device list changes
  // (i.e. when a new device is made available, or non-available). This
  // simply calls `AudioDeviceListChanged` below.
  static void InputAudioDeviceListChanged_s(cubeb* aContext, void* aUser);
  static void OutputAudioDeviceListChanged_s(cubeb* aContext, void* aUser);
  // Invalidates the cached audio input device list, can be called on any
  // thread.
  void AudioDeviceListChanged(Side aSide);
  void EnumerateAudioDevices(Side aSide);
  // Synchronize access to mInputDevices and mOutputDevices;
  Mutex mMutex;
  nsTArray<RefPtr<AudioDeviceInfo>> mInputDevices;
  nsTArray<RefPtr<AudioDeviceInfo>> mOutputDevices;
  // If mManual*Invalidation is true, then it is necessary to query the device
  // list each time instead of relying on automatic invalidation of the cache by
  // cubeb itself. Set in the constructor and then can be access on any thread.
  bool mManualInputInvalidation;
  bool mManualOutputInvalidation;
  // The singleton instance.
  static StaticRefPtr<CubebDeviceEnumerator> sInstance;
};

typedef CubebDeviceEnumerator Enumerator;
typedef CubebDeviceEnumerator::Side EnumeratorSide;
}  // namespace mozilla

#endif  // CUBEBDEVICEENUMERATOR_H_
