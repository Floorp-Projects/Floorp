/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CUBEBDEVICEENUMERATOR_H_
#define CUBEBDEVICEENUMERATOR_H_

#include "AudioDeviceInfo.h"
#include "cubeb/cubeb.h"
#include "CubebUtils.h"
#include "mozilla/Mutex.h"
#include "nsTArray.h"

namespace mozilla {
// This class implements a cache for accessing the audio device list.
// It can be accessed on any thread.
class CubebDeviceEnumerator final
{
public:
  CubebDeviceEnumerator();
  ~CubebDeviceEnumerator();
  // This method returns a list of all the input and output audio devices
  // available on this machine.
  // This method is safe to call from all threads.
  void EnumerateAudioInputDevices(nsTArray<RefPtr<AudioDeviceInfo>>& aOutDevices);
  // From a cubeb device id, return the info for this device, if it's still a
  // valid id, or nullptr otherwise.
  // This method is safe to call from any thread.
  already_AddRefed<AudioDeviceInfo>
  DeviceInfoFromID(CubebUtils::AudioDeviceID aID);

private:
  // Static function called by cubeb when the audio input device list changes
  // (i.e. when a new device is made available, or non-available). This
  // re-binds to the MediaEngineWebRTC that instantiated this
  // CubebDeviceEnumerator, and simply calls `AudioDeviceListChanged` below.
  static void AudioDeviceListChanged_s(cubeb* aContext, void* aUser);
  // Invalidates the cached audio input device list, can be called on any
  // thread.
  void AudioDeviceListChanged();
  // Synchronize access to mDevices
  Mutex mMutex;
  nsTArray<RefPtr<AudioDeviceInfo>> mDevices;
  // If mManualInvalidation is true, then it is necessary to query the device
  // list each time instead of relying on automatic invalidation of the cache by
  // cubeb itself. Set in the constructor and then can be access on any thread.
  bool mManualInvalidation;
};

}

#endif // CUBEBDEVICEENUMERATOR_H_
