/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CUBEBDEVICEENUMERATOR_H_
#define CUBEBDEVICEENUMERATOR_H_

#include "AudioDeviceInfo.h"
#include "cubeb/cubeb.h"
#include "MediaEventSource.h"
#include "mozilla/Mutex.h"
#include "nsTArray.h"

namespace mozilla {

namespace media {
template <typename T>
class Refcountable;
}

// This class implements a cache for accessing the audio device list.
// It can be accessed on any thread.
class CubebDeviceEnumerator final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CubebDeviceEnumerator)

  static CubebDeviceEnumerator* GetInstance();
  static void Shutdown();
  using AudioDeviceSet = media::Refcountable<nsTArray<RefPtr<AudioDeviceInfo>>>;
  // This method returns a list of all the input audio devices
  // (sources) available on this machine.
  // This method is safe to call from all threads.
  RefPtr<const AudioDeviceSet> EnumerateAudioInputDevices();
  // Similar for the audio audio devices (sinks). Also thread safe.
  RefPtr<const AudioDeviceSet> EnumerateAudioOutputDevices();
  // From a device name, return the info for this device, if it's a valid name,
  // or nullptr otherwise.
  // This method is safe to call from any thread.
  enum class Side {
    INPUT,
    OUTPUT,
  };
  already_AddRefed<AudioDeviceInfo> DeviceInfoFromName(const nsString& aName,
                                                       Side aSide);
  // Event source to listen for changes to the audio input device list on.
  MediaEventSource<void>& OnAudioInputDeviceListChange() {
    return mOnInputDeviceListChange;
  }

  // Event source to listen for changes to the audio output device list on.
  MediaEventSource<void>& OnAudioOutputDeviceListChange() {
    return mOnOutputDeviceListChange;
  }

  // Return the default device for a particular side.
  RefPtr<AudioDeviceInfo> DefaultDevice(Side aSide);

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
  RefPtr<const AudioDeviceSet> EnumerateAudioDevices(Side aSide);
  // Synchronize access to mInputDevices and mOutputDevices;
  Mutex mMutex MOZ_UNANNOTATED;
  RefPtr<const AudioDeviceSet> mInputDevices;
  RefPtr<const AudioDeviceSet> mOutputDevices;
  // If mManual*Invalidation is true, then it is necessary to query the device
  // list each time instead of relying on automatic invalidation of the cache by
  // cubeb itself. Set in the constructor and then can be access on any thread.
  bool mManualInputInvalidation;
  bool mManualOutputInvalidation;
  MediaEventProducer<void> mOnInputDeviceListChange;
  MediaEventProducer<void> mOnOutputDeviceListChange;
};

typedef CubebDeviceEnumerator Enumerator;
typedef CubebDeviceEnumerator::Side EnumeratorSide;
}  // namespace mozilla

#endif  // CUBEBDEVICEENUMERATOR_H_
