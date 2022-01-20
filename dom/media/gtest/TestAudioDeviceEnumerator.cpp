/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define ENABLE_SET_CUBEB_BACKEND 1
#include "CubebDeviceEnumerator.h"
#include "gtest/gtest-printers.h"
#include "gtest/gtest.h"
#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/media/MediaUtils.h"
#include "nsTArray.h"

#include "MockCubeb.h"

using namespace mozilla;
using AudioDeviceSet = CubebDeviceEnumerator::AudioDeviceSet;

const bool DEBUG_PRINTS = false;

enum DeviceOperation { ADD, REMOVE };

void TestEnumeration(MockCubeb* aMock, uint32_t aExpectedDeviceCount,
                     DeviceOperation aOperation, cubeb_device_type aType) {
  RefPtr<CubebDeviceEnumerator> enumerator =
      CubebDeviceEnumerator::GetInstance();

  RefPtr<const AudioDeviceSet> devices;

  if (aType == CUBEB_DEVICE_TYPE_INPUT) {
    devices = enumerator->EnumerateAudioInputDevices();
  }

  if (aType == CUBEB_DEVICE_TYPE_OUTPUT) {
    devices = enumerator->EnumerateAudioOutputDevices();
  }

  EXPECT_EQ(devices->Length(), aExpectedDeviceCount)
      << "Device count is correct when enumerating";

  if (DEBUG_PRINTS) {
    for (const auto& deviceInfo : *devices) {
      printf("=== Before removal\n");
      PrintDevice(deviceInfo);
    }
  }

  if (aOperation == DeviceOperation::REMOVE) {
    aMock->RemoveDevice(reinterpret_cast<cubeb_devid>(1));
  } else {
    aMock->AddDevice(DeviceTemplate(reinterpret_cast<cubeb_devid>(123), aType));
  }

  if (aType == CUBEB_DEVICE_TYPE_INPUT) {
    devices = enumerator->EnumerateAudioInputDevices();
  }

  if (aType == CUBEB_DEVICE_TYPE_OUTPUT) {
    devices = enumerator->EnumerateAudioOutputDevices();
  }

  uint32_t newExpectedDeviceCount = aOperation == DeviceOperation::REMOVE
                                        ? aExpectedDeviceCount - 1
                                        : aExpectedDeviceCount + 1;

  EXPECT_EQ(devices->Length(), newExpectedDeviceCount)
      << "Device count is correct when enumerating after operation";

  if (DEBUG_PRINTS) {
    for (const auto& deviceInfo : *devices) {
      printf("=== After removal\n");
      PrintDevice(deviceInfo);
    }
  }
}

#ifndef ANDROID
TEST(CubebDeviceEnumerator, EnumerateSimple)
{
  // It looks like we're leaking this object, but in fact it will be freed by
  // gecko sometime later: `cubeb_destroy` is called when layout statics are
  // shutdown and we cast back to a MockCubeb* and call the dtor.
  MockCubeb* mock = new MockCubeb();
  mozilla::CubebUtils::ForceSetCubebContext(mock->AsCubebContext());

  // We want to test whether CubebDeviceEnumerator works with and without a
  // backend that can notify of a device collection change via callback.
  // Additionally, we're testing that both adding and removing a device
  // invalidates the list correctly.
  bool supportsDeviceChangeCallback[2] = {true, false};
  DeviceOperation operations[2] = {DeviceOperation::ADD,
                                   DeviceOperation::REMOVE};

  for (bool supports : supportsDeviceChangeCallback) {
    // Shutdown for `supports` to take effect
    CubebDeviceEnumerator::Shutdown();
    mock->SetSupportDeviceChangeCallback(supports);
    for (DeviceOperation op : operations) {
      uint32_t device_count = 4;

      cubeb_device_type deviceType = CUBEB_DEVICE_TYPE_INPUT;
      AddDevices(mock, device_count, deviceType);
      TestEnumeration(mock, device_count, op, deviceType);

      deviceType = CUBEB_DEVICE_TYPE_OUTPUT;
      AddDevices(mock, device_count, deviceType);
      TestEnumeration(mock, device_count, op, deviceType);
    }
  }
  // Shutdown to clean up the last `supports` effect
  CubebDeviceEnumerator::Shutdown();
}

#else  // building for Android, which has no device enumeration support
TEST(CubebDeviceEnumerator, EnumerateAndroid)
{
  MockCubeb* mock = new MockCubeb();
  mozilla::CubebUtils::ForceSetCubebContext(mock->AsCubebContext());

  RefPtr<CubebDeviceEnumerator> enumerator =
      CubebDeviceEnumerator::GetInstance();

  RefPtr<const AudioDeviceSet> inputDevices =
      enumerator->EnumerateAudioInputDevices();
  EXPECT_EQ(inputDevices->Length(), 1u)
      << "Android always exposes a single input device.";
  EXPECT_EQ((*inputDevices)[0]->MaxChannels(), 1u) << "With a single channel.";
  EXPECT_EQ((*inputDevices)[0]->DeviceID(), nullptr)
      << "It's always the default input device.";
  EXPECT_TRUE((*inputDevices)[0]->Preferred())
      << "it's always the prefered input device.";

  RefPtr<const AudioDeviceSet> outputDevices =
      enumerator->EnumerateAudioOutputDevices();
  EXPECT_EQ(outputDevices->Length(), 1u)
      << "Android always exposes a single output device.";
  EXPECT_EQ((*outputDevices)[0]->MaxChannels(), 2u) << "With stereo channels.";
  EXPECT_EQ((*outputDevices)[0]->DeviceID(), nullptr)
      << "It's always the default output device.";
  EXPECT_TRUE((*outputDevices)[0]->Preferred())
      << "it's always the prefered output device.";
}
#endif

TEST(CubebDeviceEnumerator, ForceNullCubebContext)
{
  mozilla::CubebUtils::ForceSetCubebContext(nullptr);
  RefPtr<CubebDeviceEnumerator> enumerator =
      CubebDeviceEnumerator::GetInstance();

  RefPtr inputDevices = enumerator->EnumerateAudioInputDevices();
  EXPECT_EQ(inputDevices->Length(), 0u)
      << "Enumeration must fail, input device list must be empty.";

  RefPtr outputDevices = enumerator->EnumerateAudioOutputDevices();
  EXPECT_EQ(outputDevices->Length(), 0u)
      << "Enumeration must fail, output device list must be empty.";

  // Shutdown to clean up the null context effect
  CubebDeviceEnumerator::Shutdown();
}

TEST(CubebDeviceEnumerator, DeviceInfoFromName)
{
  MockCubeb* mock = new MockCubeb();
  mozilla::CubebUtils::ForceSetCubebContext(mock->AsCubebContext());

  cubeb_device_type deviceTypes[2] = {CUBEB_DEVICE_TYPE_INPUT,
                                      CUBEB_DEVICE_TYPE_OUTPUT};

  bool supportsDeviceChangeCallback[2] = {true, false};
  for (bool supports : supportsDeviceChangeCallback) {
    // Shutdown for `supports` to take effect
    CubebDeviceEnumerator::Shutdown();
    mock->SetSupportDeviceChangeCallback(supports);
    for (cubeb_device_type& deviceType : deviceTypes) {
      cubeb_devid id_1 = reinterpret_cast<cubeb_devid>(1);
      mock->AddDevice(DeviceTemplate(id_1, deviceType, "device name 1"));
      cubeb_devid id_2 = reinterpret_cast<cubeb_devid>(2);
      nsCString device_name = "device name 2"_ns;
      mock->AddDevice(DeviceTemplate(id_2, deviceType, device_name.get()));
      cubeb_devid id_3 = reinterpret_cast<cubeb_devid>(3);
      mock->AddDevice(DeviceTemplate(id_3, deviceType, "device name 3"));

      RefPtr<CubebDeviceEnumerator> enumerator =
          CubebDeviceEnumerator::GetInstance();

      EnumeratorSide side = (deviceType == CUBEB_DEVICE_TYPE_INPUT)
                                ? EnumeratorSide::INPUT
                                : EnumeratorSide::OUTPUT;
      RefPtr<AudioDeviceInfo> devInfo = enumerator->DeviceInfoFromName(
          NS_ConvertUTF8toUTF16(device_name), side);
      EXPECT_TRUE(devInfo) << "the device exist";
      EXPECT_EQ(devInfo->Name(), NS_ConvertUTF8toUTF16(device_name))
          << "verify the device";

      mock->RemoveDevice(id_2);

      devInfo = enumerator->DeviceInfoFromName(
          NS_ConvertUTF8toUTF16(device_name), side);
      EXPECT_FALSE(devInfo) << "the device does not exist any more";
    }
  }
  // Shutdown for `supports` to take effect
  CubebDeviceEnumerator::Shutdown();
}
#undef ENABLE_SET_CUBEB_BACKEND
