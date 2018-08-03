/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Attributes.h"
#include "nsTArray.h"
#define ENABLE_SET_CUBEB_BACKEND 1
#include "CubebUtils.h"
#include "MediaEngineWebRTC.h"

using namespace mozilla;

const bool DEBUG_PRINTS = false;

// Keep those and the struct definition in sync with cubeb.h and
// cubeb-internal.h
void
cubeb_mock_destroy(cubeb* context);
static int
cubeb_mock_enumerate_devices(cubeb* context,
                             cubeb_device_type type,
                             cubeb_device_collection* out);

static int
cubeb_mock_device_collection_destroy(cubeb* context,
                                     cubeb_device_collection* collection);

static int
cubeb_mock_register_device_collection_changed(
  cubeb* context,
  cubeb_device_type devtype,
  cubeb_device_collection_changed_callback callback,
  void* user_ptr);

struct cubeb_ops
{
  int (*init)(cubeb** context, char const* context_name);
  char const* (*get_backend_id)(cubeb* context);
  int (*get_max_channel_count)(cubeb* context, uint32_t* max_channels);
  int (*get_min_latency)(cubeb* context,
                         cubeb_stream_params params,
                         uint32_t* latency_ms);
  int (*get_preferred_sample_rate)(cubeb* context, uint32_t* rate);
  int (*enumerate_devices)(cubeb* context,
                           cubeb_device_type type,
                           cubeb_device_collection* collection);
  int (*device_collection_destroy)(cubeb* context,
                                   cubeb_device_collection* collection);
  void (*destroy)(cubeb* context);
  int (*stream_init)(cubeb* context,
                     cubeb_stream** stream,
                     char const* stream_name,
                     cubeb_devid input_device,
                     cubeb_stream_params* input_stream_params,
                     cubeb_devid output_device,
                     cubeb_stream_params* output_stream_params,
                     unsigned int latency,
                     cubeb_data_callback data_callback,
                     cubeb_state_callback state_callback,
                     void* user_ptr);
  void (*stream_destroy)(cubeb_stream* stream);
  int (*stream_start)(cubeb_stream* stream);
  int (*stream_stop)(cubeb_stream* stream);
  int (*stream_reset_default_device)(cubeb_stream* stream);
  int (*stream_get_position)(cubeb_stream* stream, uint64_t* position);
  int (*stream_get_latency)(cubeb_stream* stream, uint32_t* latency);
  int (*stream_set_volume)(cubeb_stream* stream, float volumes);
  int (*stream_set_panning)(cubeb_stream* stream, float panning);
  int (*stream_get_current_device)(cubeb_stream* stream,
                                   cubeb_device** const device);
  int (*stream_device_destroy)(cubeb_stream* stream, cubeb_device* device);
  int (*stream_register_device_changed_callback)(
    cubeb_stream* stream,
    cubeb_device_changed_callback device_changed_callback);
  int (*register_device_collection_changed)(
    cubeb* context,
    cubeb_device_type devtype,
    cubeb_device_collection_changed_callback callback,
    void* user_ptr);
};

// Mock cubeb impl, only supports device enumeration for now.
cubeb_ops const mock_ops = {
  /*.init =*/NULL,
  /*.get_backend_id =*/NULL,
  /*.get_max_channel_count =*/NULL,
  /*.get_min_latency =*/NULL,
  /*.get_preferred_sample_rate =*/NULL,
  /*.enumerate_devices =*/cubeb_mock_enumerate_devices,
  /*.device_collection_destroy =*/cubeb_mock_device_collection_destroy,
  /*.destroy =*/cubeb_mock_destroy,
  /*.stream_init =*/NULL,
  /*.stream_destroy =*/NULL,
  /*.stream_start =*/NULL,
  /*.stream_stop =*/NULL,
  /*.stream_reset_default_device =*/NULL,
  /*.stream_get_position =*/NULL,
  /*.stream_get_latency =*/NULL,
  /*.stream_set_volume =*/NULL,
  /*.stream_set_panning =*/NULL,
  /*.stream_get_current_device =*/NULL,
  /*.stream_device_destroy =*/NULL,
  /*.stream_register_device_changed_callback =*/NULL,
  /*.register_device_collection_changed =*/
  cubeb_mock_register_device_collection_changed
};

// This class has two facets: it is both a fake cubeb backend that is intended
// to be used for testing, and passed to Gecko code that expects a normal
// backend, but is also controllable by the test code to decide what the backend
// should do, depending on what is being tested.
class MockCubeb
{
public:
  MockCubeb()
    : ops(&mock_ops)
    , mDeviceCollectionChangeCallback(nullptr)
    , mDeviceCollectionChangeType(CUBEB_DEVICE_TYPE_UNKNOWN)
    , mDeviceCollectionChangeUserPtr(nullptr)
    , mSupportsDeviceCollectionChangedCallback(true)
  {
  }
  // Cubeb backend implementation
  // This allows passing this class as a cubeb* instance.
  cubeb* AsCubebContext() { return reinterpret_cast<cubeb*>(this); }
  // Fill in the collection parameter with all devices of aType.
  int EnumerateDevices(cubeb_device_type aType,
                       cubeb_device_collection* collection)
  {
#ifdef ANDROID
    EXPECT_TRUE(false) << "This is not to be called on Android.";
#endif
    size_t count = 0;
    if (aType & CUBEB_DEVICE_TYPE_INPUT) {
      count += mInputDevices.Length();
    }
    if (aType & CUBEB_DEVICE_TYPE_OUTPUT) {
      count += mOutputDevices.Length();
    }
    collection->device = new cubeb_device_info[count];
    collection->count = count;

    uint32_t collection_index = 0;
    if (aType & CUBEB_DEVICE_TYPE_INPUT) {
      for (auto& device : mInputDevices) {
        collection->device[collection_index] = device;
        collection_index++;
      }
    }
    if (aType & CUBEB_DEVICE_TYPE_OUTPUT) {
      for (auto& device : mOutputDevices) {
        collection->device[collection_index] = device;
        collection_index++;
      }
    }

    return CUBEB_OK;
  }

  // For a given device type, add a callback, called with a user pointer, when
  // the device collection for this backend changes (i.e. a device has been
  // removed or added).
  int RegisterDeviceCollectionChangeCallback(
    cubeb_device_type aDevType,
    cubeb_device_collection_changed_callback aCallback,
    void* aUserPtr)
  {
    if (!mSupportsDeviceCollectionChangedCallback) {
      return CUBEB_ERROR;
    }

    mDeviceCollectionChangeType = aDevType;
    mDeviceCollectionChangeCallback = aCallback;
    mDeviceCollectionChangeUserPtr = aUserPtr;

    return CUBEB_OK;
  }

  // Control API

  // Add an input or output device to this backend. This calls the device
  // collection invalidation callback if needed.
  void AddDevice(cubeb_device_info aDevice)
  {
    bool needToCall = false;

    if (aDevice.type == CUBEB_DEVICE_TYPE_INPUT) {
      mInputDevices.AppendElement(aDevice);
    } else if (aDevice.type == CUBEB_DEVICE_TYPE_OUTPUT) {
      mOutputDevices.AppendElement(aDevice);
    } else {
      MOZ_CRASH("bad device type when adding a device in mock cubeb backend");
    }

    bool isInput = aDevice.type & CUBEB_DEVICE_TYPE_INPUT;

    needToCall |=
      isInput && mDeviceCollectionChangeType & CUBEB_DEVICE_TYPE_INPUT;
    needToCall |=
      !isInput && mDeviceCollectionChangeType & CUBEB_DEVICE_TYPE_OUTPUT;

    if (needToCall && mDeviceCollectionChangeCallback) {
      mDeviceCollectionChangeCallback(AsCubebContext(),
                                      mDeviceCollectionChangeUserPtr);
    }
  }
  // Remove a specific input or output device to this backend, returns true if
  // a device was removed. This calls the device collection invalidation
  // callback if needed.
  bool RemoveDevice(cubeb_devid aId)
  {
    bool foundInput = false;
    bool foundOutput = false;
    mInputDevices.RemoveElementsBy(
      [aId, &foundInput](cubeb_device_info& aDeviceInfo) {
        bool foundThisTime = aDeviceInfo.devid == aId;
        foundInput |= foundThisTime;
        return foundThisTime;
      });
    mOutputDevices.RemoveElementsBy(
      [aId, &foundOutput](cubeb_device_info& aDeviceInfo) {
        bool foundThisTime = aDeviceInfo.devid == aId;
        foundOutput |= foundThisTime;
        return foundThisTime;
      });

    bool needToCall = false;

    needToCall |=
      foundInput && mDeviceCollectionChangeType & CUBEB_DEVICE_TYPE_INPUT;
    needToCall |=
      foundOutput && mDeviceCollectionChangeType & CUBEB_DEVICE_TYPE_OUTPUT;

    if (needToCall && mDeviceCollectionChangeCallback) {
      mDeviceCollectionChangeCallback(AsCubebContext(),
                                      mDeviceCollectionChangeUserPtr);
    }

    // If the device removed was a default device, set another device as the
    // default, if there are still devices available.
    bool foundDefault = false;
    for (uint32_t i = 0; i < mInputDevices.Length(); i++) {
      foundDefault |= mInputDevices[i].preferred != CUBEB_DEVICE_PREF_NONE;
    }

    if (!foundDefault) {
      if (!mInputDevices.IsEmpty()) {
        mInputDevices[mInputDevices.Length() - 1].preferred =
          CUBEB_DEVICE_PREF_ALL;
      }
    }

    foundDefault = false;
    for (uint32_t i = 0; i < mOutputDevices.Length(); i++) {
      foundDefault |= mOutputDevices[i].preferred != CUBEB_DEVICE_PREF_NONE;
    }

    if (!foundDefault) {
      if (!mOutputDevices.IsEmpty()) {
        mOutputDevices[mOutputDevices.Length() - 1].preferred =
          CUBEB_DEVICE_PREF_ALL;
      }
    }

    return foundInput | foundOutput;
  }
  // Remove all input or output devices from this backend, without calling the
  // callback. This is meant to clean up in between tests.
  void ClearDevices(cubeb_device_type aType)
  {
    mInputDevices.Clear();
    mOutputDevices.Clear();
  }

  // This allows simulating a backend that does not support setting a device
  // collection invalidation callback, to be able to test the fallback path.
  void SetSupportDeviceChangeCallback(bool aSupports)
  {
    mSupportsDeviceCollectionChangedCallback = aSupports;
  }

private:
  // This needs to have the exact same memory layout as a real cubeb backend.
  // It's very important for this `ops` member to be the very first member of
  // the class, and to not have any virtual members (to avoid having a vtable).
  const cubeb_ops* ops;
  // The callback to call when the device list has been changed.
  cubeb_device_collection_changed_callback mDeviceCollectionChangeCallback;
  // For which device type to call the callback.
  cubeb_device_type mDeviceCollectionChangeType;
  // The pointer to pass in the callback.
  void* mDeviceCollectionChangeUserPtr;
  // Whether or not this backed supports device collection change notification
  // via a system callback. If not, Gecko is expected to re-query the list every
  // time.
  bool mSupportsDeviceCollectionChangedCallback;
  // Our input and output devices.
  nsTArray<cubeb_device_info> mInputDevices;
  nsTArray<cubeb_device_info> mOutputDevices;
};

void
cubeb_mock_destroy(cubeb* context)
{
  delete reinterpret_cast<MockCubeb*>(context);
}

static int
cubeb_mock_enumerate_devices(cubeb* context,
                             cubeb_device_type type,
                             cubeb_device_collection* out)
{
  MockCubeb* mock = reinterpret_cast<MockCubeb*>(context);
  return mock->EnumerateDevices(type, out);
}

int
cubeb_mock_device_collection_destroy(cubeb* context,
                                     cubeb_device_collection* collection)
{
  delete[] collection->device;
  return CUBEB_OK;
}

int
cubeb_mock_register_device_collection_changed(
  cubeb* context,
  cubeb_device_type devtype,
  cubeb_device_collection_changed_callback callback,
  void* user_ptr)
{
  MockCubeb* mock = reinterpret_cast<MockCubeb*>(context);
  return mock->RegisterDeviceCollectionChangeCallback(
    devtype, callback, user_ptr);
  return CUBEB_OK;
}

void
PrintDevice(cubeb_device_info aInfo)
{
  printf("id: %zu\n"
         "device_id: %s\n"
         "friendly_name: %s\n"
         "group_id: %s\n"
         "vendor_name: %s\n"
         "type: %d\n"
         "state: %d\n"
         "preferred: %d\n"
         "format: %d\n"
         "default_format: %d\n"
         "max_channels: %d\n"
         "default_rate: %d\n"
         "max_rate: %d\n"
         "min_rate: %d\n"
         "latency_lo: %d\n"
         "latency_hi: %d\n",
         reinterpret_cast<uintptr_t>(aInfo.devid),
         aInfo.device_id,
         aInfo.friendly_name,
         aInfo.group_id,
         aInfo.vendor_name,
         aInfo.type,
         aInfo.state,
         aInfo.preferred,
         aInfo.format,
         aInfo.default_format,
         aInfo.max_channels,
         aInfo.default_rate,
         aInfo.max_rate,
         aInfo.min_rate,
         aInfo.latency_lo,
         aInfo.latency_hi);
}

void
PrintDevice(AudioDeviceInfo* aInfo)
{
  cubeb_devid id;
  nsString name;
  nsString groupid;
  nsString vendor;
  uint16_t type;
  uint16_t state;
  uint16_t preferred;
  uint16_t supportedFormat;
  uint16_t defaultFormat;
  uint32_t maxChannels;
  uint32_t defaultRate;
  uint32_t maxRate;
  uint32_t minRate;
  uint32_t maxLatency;
  uint32_t minLatency;

  id = aInfo->DeviceID();
  aInfo->GetName(name);
  aInfo->GetGroupId(groupid);
  aInfo->GetVendor(vendor);
  aInfo->GetType(&type);
  aInfo->GetState(&state);
  aInfo->GetPreferred(&preferred);
  aInfo->GetSupportedFormat(&supportedFormat);
  aInfo->GetDefaultFormat(&defaultFormat);
  aInfo->GetMaxChannels(&maxChannels);
  aInfo->GetDefaultRate(&defaultRate);
  aInfo->GetMaxRate(&maxRate);
  aInfo->GetMinRate(&minRate);
  aInfo->GetMinLatency(&minLatency);
  aInfo->GetMaxLatency(&maxLatency);

  printf("device id: %zu\n"
         "friendly_name: %s\n"
         "group_id: %s\n"
         "vendor_name: %s\n"
         "type: %d\n"
         "state: %d\n"
         "preferred: %d\n"
         "format: %d\n"
         "default_format: %d\n"
         "max_channels: %d\n"
         "default_rate: %d\n"
         "max_rate: %d\n"
         "min_rate: %d\n"
         "latency_lo: %d\n"
         "latency_hi: %d\n",
         reinterpret_cast<uintptr_t>(id),
         NS_LossyConvertUTF16toASCII(name).get(),
         NS_LossyConvertUTF16toASCII(groupid).get(),
         NS_LossyConvertUTF16toASCII(vendor).get(),
         type,
         state,
         preferred,
         supportedFormat,
         defaultFormat,
         maxChannels,
         defaultRate,
         maxRate,
         minRate,
         minLatency,
         maxLatency);
}

cubeb_device_info
InputDeviceTemplate(cubeb_devid aId)
{
  // A fake input device
  cubeb_device_info device;
  device.devid = aId;
  device.device_id = "nice name";
  device.friendly_name = "an even nicer name";
  device.group_id = "the physical device";
  device.vendor_name = "mozilla";
  device.type = CUBEB_DEVICE_TYPE_INPUT;
  device.state = CUBEB_DEVICE_STATE_ENABLED;
  device.preferred = CUBEB_DEVICE_PREF_NONE;
  device.format = CUBEB_DEVICE_FMT_F32NE;
  device.default_format = CUBEB_DEVICE_FMT_F32NE;
  device.max_channels = 2;
  device.default_rate = 44100;
  device.max_rate = 44100;
  device.min_rate = 16000;
  device.latency_lo = 256;
  device.latency_hi = 1024;

  return device;
}

enum DeviceOperation
{
  ADD,
  REMOVE
};

void
TestEnumeration(MockCubeb* aMock,
                uint32_t aExpectedDeviceCount,
                DeviceOperation aOperation)
{
  CubebDeviceEnumerator enumerator;

  nsTArray<RefPtr<AudioDeviceInfo>> inputDevices;

  enumerator.EnumerateAudioInputDevices(inputDevices);

  EXPECT_EQ(inputDevices.Length(), aExpectedDeviceCount)
    << "Device count is correct when enumerating";

  if (DEBUG_PRINTS) {
    for (uint32_t i = 0; i < inputDevices.Length(); i++) {
      printf("=== Before removal\n");
      PrintDevice(inputDevices[i]);
    }
  }

  if (aOperation == DeviceOperation::REMOVE) {
    aMock->RemoveDevice(reinterpret_cast<cubeb_devid>(1));
  } else {
    aMock->AddDevice(InputDeviceTemplate(reinterpret_cast<cubeb_devid>(123)));
  }

  enumerator.EnumerateAudioInputDevices(inputDevices);

  uint32_t newExpectedDeviceCount = aOperation == DeviceOperation::REMOVE
                                      ? aExpectedDeviceCount - 1
                                      : aExpectedDeviceCount + 1;

  EXPECT_EQ(inputDevices.Length(), newExpectedDeviceCount)
    << "Device count is correct when enumerating after operation";

  if (DEBUG_PRINTS) {
    for (uint32_t i = 0; i < inputDevices.Length(); i++) {
      printf("=== After removal\n");
      PrintDevice(inputDevices[i]);
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
  bool supportsDeviceChangeCallback[2] = { true, false };
  DeviceOperation operations[2] = { DeviceOperation::ADD,
                                    DeviceOperation::REMOVE };

  for (DeviceOperation op : operations) {
    for (bool supports : supportsDeviceChangeCallback) {
      mock->ClearDevices(CUBEB_DEVICE_TYPE_INPUT);
      // Add a few input devices (almost all the same but it does not really
      // matter as long as they have distinct IDs and only one is the default
      // devices)
      uint32_t device_count = 4;
      for (uintptr_t i = 0; i < device_count; i++) {
        cubeb_device_info device =
          InputDeviceTemplate(reinterpret_cast<void*>(i + 1));
        // Make it so that the last device is the default input device.
        if (i == device_count - 1) {
          device.preferred = CUBEB_DEVICE_PREF_ALL;
        }
        mock->AddDevice(device);
      }

      mock->SetSupportDeviceChangeCallback(supports);
      TestEnumeration(mock, device_count, op);
    }
  }
}
#else // building for Android, which has no device enumeration support
TEST(CubebDeviceEnumerator, EnumerateAndroid)
{
  MockCubeb* mock = new MockCubeb();
  mozilla::CubebUtils::ForceSetCubebContext(mock->AsCubebContext());

  CubebDeviceEnumerator enumerator;

  nsTArray<RefPtr<AudioDeviceInfo>> inputDevices;
  enumerator.EnumerateAudioInputDevices(inputDevices);
  EXPECT_EQ(inputDevices.Length(), 1u) <<  "Android always exposes a single input device.";
  EXPECT_EQ(inputDevices[0]->MaxChannels(), 1u) << "With a single channel.";
  EXPECT_EQ(inputDevices[0]->DeviceID(), nullptr) << "It's always the default device.";
  EXPECT_TRUE(inputDevices[0]->Preferred()) << "it's always the prefered device.";
}
#endif
