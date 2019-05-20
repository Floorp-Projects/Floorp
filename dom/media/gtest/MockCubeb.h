#ifndef MOCKCUBEB_H_
#define MOCKCUBEB_H_

#include "AudioDeviceInfo.h"

#include <thread>
#include <atomic>
#include <chrono>

using namespace std::chrono_literals;

const long NUM_OF_FRAMES = 512;
const uint32_t NUM_OF_CHANNELS = 2;

struct cubeb_ops {
  int (*init)(cubeb** context, char const* context_name);
  char const* (*get_backend_id)(cubeb* context);
  int (*get_max_channel_count)(cubeb* context, uint32_t* max_channels);
  int (*get_min_latency)(cubeb* context, cubeb_stream_params params,
                         uint32_t* latency_ms);
  int (*get_preferred_sample_rate)(cubeb* context, uint32_t* rate);
  int (*enumerate_devices)(cubeb* context, cubeb_device_type type,
                           cubeb_device_collection* collection);
  int (*device_collection_destroy)(cubeb* context,
                                   cubeb_device_collection* collection);
  void (*destroy)(cubeb* context);
  int (*stream_init)(cubeb* context, cubeb_stream** stream,
                     char const* stream_name, cubeb_devid input_device,
                     cubeb_stream_params* input_stream_params,
                     cubeb_devid output_device,
                     cubeb_stream_params* output_stream_params,
                     unsigned int latency, cubeb_data_callback data_callback,
                     cubeb_state_callback state_callback, void* user_ptr);
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
      cubeb* context, cubeb_device_type devtype,
      cubeb_device_collection_changed_callback callback, void* user_ptr);
};

// Keep those and the struct definition in sync with cubeb.h and
// cubeb-internal.h
void cubeb_mock_destroy(cubeb* context);
static int cubeb_mock_enumerate_devices(cubeb* context, cubeb_device_type type,
                                        cubeb_device_collection* out);

static int cubeb_mock_device_collection_destroy(
    cubeb* context, cubeb_device_collection* collection);

static int cubeb_mock_register_device_collection_changed(
    cubeb* context, cubeb_device_type devtype,
    cubeb_device_collection_changed_callback callback, void* user_ptr);

static int cubeb_mock_stream_init(
    cubeb* context, cubeb_stream** stream, char const* stream_name,
    cubeb_devid input_device, cubeb_stream_params* input_stream_params,
    cubeb_devid output_device, cubeb_stream_params* output_stream_params,
    unsigned int latency, cubeb_data_callback data_callback,
    cubeb_state_callback state_callback, void* user_ptr);

static int cubeb_mock_stream_start(cubeb_stream* stream);

static int cubeb_mock_stream_stop(cubeb_stream* stream);

static void cubeb_mock_stream_destroy(cubeb_stream* stream);

static char const* cubeb_mock_get_backend_id(cubeb* context);

static int cubeb_mock_stream_set_volume(cubeb_stream* stream, float volume);

static int cubeb_mock_get_min_latency(cubeb* context,
                                      cubeb_stream_params params,
                                      uint32_t* latency_ms);

// Mock cubeb impl, only supports device enumeration for now.
cubeb_ops const mock_ops = {
    /*.init =*/NULL,
    /*.get_backend_id =*/cubeb_mock_get_backend_id,
    /*.get_max_channel_count =*/NULL,
    /*.get_min_latency =*/cubeb_mock_get_min_latency,
    /*.get_preferred_sample_rate =*/NULL,
    /*.enumerate_devices =*/cubeb_mock_enumerate_devices,
    /*.device_collection_destroy =*/cubeb_mock_device_collection_destroy,
    /*.destroy =*/cubeb_mock_destroy,
    /*.stream_init =*/cubeb_mock_stream_init,
    /*.stream_destroy =*/cubeb_mock_stream_destroy,
    /*.stream_start =*/cubeb_mock_stream_start,
    /*.stream_stop =*/cubeb_mock_stream_stop,
    /*.stream_reset_default_device =*/NULL,
    /*.stream_get_position =*/NULL,
    /*.stream_get_latency =*/NULL,
    /*.stream_set_volume =*/cubeb_mock_stream_set_volume,
    /*.stream_set_panning =*/NULL,
    /*.stream_get_current_device =*/NULL,
    /*.stream_device_destroy =*/NULL,
    /*.stream_register_device_changed_callback =*/NULL,
    /*.register_device_collection_changed =*/
    cubeb_mock_register_device_collection_changed};

// This class has two facets: it is both a fake cubeb backend that is intended
// to be used for testing, and passed to Gecko code that expects a normal
// backend, but is also controllable by the test code to decide what the backend
// should do, depending on what is being tested.
class MockCubeb {
 public:
  MockCubeb() : ops(&mock_ops) {}
  ~MockCubeb() {
    assert(!mFakeAudioThread);
    assert(!mMockStream);
  }
  // Cubeb backend implementation
  // This allows passing this class as a cubeb* instance.
  cubeb* AsCubebContext() { return reinterpret_cast<cubeb*>(this); }
  // Fill in the collection parameter with all devices of aType.
  int EnumerateDevices(cubeb_device_type aType,
                       cubeb_device_collection* collection) {
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
      cubeb_device_collection_changed_callback aCallback, void* aUserPtr) {
    if (!mSupportsDeviceCollectionChangedCallback) {
      return CUBEB_ERROR;
    }

    if (aDevType & CUBEB_DEVICE_TYPE_INPUT) {
      mInputDeviceCollectionChangeCallback = aCallback;
      mInputDeviceCollectionChangeUserPtr = aUserPtr;
    }
    if (aDevType & CUBEB_DEVICE_TYPE_OUTPUT) {
      mOutputDeviceCollectionChangeCallback = aCallback;
      mOutputDeviceCollectionChangeUserPtr = aUserPtr;
    }

    return CUBEB_OK;
  }

  // Control API

  // Add an input or output device to this backend. This calls the device
  // collection invalidation callback if needed.
  void AddDevice(cubeb_device_info aDevice) {
    if (aDevice.type == CUBEB_DEVICE_TYPE_INPUT) {
      mInputDevices.AppendElement(aDevice);
    } else if (aDevice.type == CUBEB_DEVICE_TYPE_OUTPUT) {
      mOutputDevices.AppendElement(aDevice);
    } else {
      MOZ_CRASH("bad device type when adding a device in mock cubeb backend");
    }

    bool isInput = aDevice.type & CUBEB_DEVICE_TYPE_INPUT;
    if (isInput && mInputDeviceCollectionChangeCallback) {
      mInputDeviceCollectionChangeCallback(AsCubebContext(),
                                           mInputDeviceCollectionChangeUserPtr);
    }
    if (!isInput && mOutputDeviceCollectionChangeCallback) {
      mOutputDeviceCollectionChangeCallback(
          AsCubebContext(), mOutputDeviceCollectionChangeUserPtr);
    }
  }
  // Remove a specific input or output device to this backend, returns true if
  // a device was removed. This calls the device collection invalidation
  // callback if needed.
  bool RemoveDevice(cubeb_devid aId) {
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

    if (foundInput && mInputDeviceCollectionChangeCallback) {
      mInputDeviceCollectionChangeCallback(AsCubebContext(),
                                           mInputDeviceCollectionChangeUserPtr);
    }
    if (foundOutput && mOutputDeviceCollectionChangeCallback) {
      mOutputDeviceCollectionChangeCallback(
          AsCubebContext(), mOutputDeviceCollectionChangeUserPtr);
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
  void ClearDevices(cubeb_device_type aType) {
    mInputDevices.Clear();
    mOutputDevices.Clear();
  }

  // This allows simulating a backend that does not support setting a device
  // collection invalidation callback, to be able to test the fallback path.
  void SetSupportDeviceChangeCallback(bool aSupports) {
    mSupportsDeviceCollectionChangedCallback = aSupports;
  }

  // Represents the fake cubeb_stream. The context instance is needed to
  // provide access on cubeb_ops struct.
  struct MockCubebStream {
    cubeb* context = nullptr;
  };

  // Simulates the audio thread. The thread is created at StreamStart and
  // destroyed at StreamStop. At next StreamStart a new thread is created.
  static void ThreadFunction_s(MockCubeb* that) { that->ThreadFunction(); }

  void ThreadFunction() {
    while (!mStreamStop) {
      cubeb_stream* stream = reinterpret_cast<cubeb_stream*>(mMockStream.get());
      long outframes = mDataCallback(stream, mUserPtr, nullptr, mOutputBuffer,
                                     NUM_OF_FRAMES);
      if (outframes < NUM_OF_FRAMES) {
        mStateCallback(stream, mUserPtr, CUBEB_STATE_DRAINED);
        break;
      }
      std::this_thread::sleep_for(
          std::chrono::milliseconds(NUM_OF_FRAMES * 1000 / mSampleRate));
    }
  }

  int StreamInit(cubeb* aContext, cubeb_stream** aStream,
                 cubeb_stream_params* aInputStreamParams,
                 cubeb_stream_params* aOutputStreamParams,
                 cubeb_data_callback aDataCallback,
                 cubeb_state_callback aStateCallback, void* aUserPtr) {
    assert(!mFakeAudioThread);
    mMockStream.reset(new MockCubebStream);
    mMockStream->context = aContext;
    *aStream = reinterpret_cast<cubeb_stream*>(mMockStream.get());
    mDataCallback = aDataCallback;
    mStateCallback = aStateCallback;
    mUserPtr = aUserPtr;
    mSampleRate = aInputStreamParams ? aInputStreamParams->rate
                                     : aOutputStreamParams->rate;
    return CUBEB_OK;
  }

  int StreamStart(cubeb_stream* aStream) {
    assert(!mFakeAudioThread);
    mStreamStop = false;
    mFakeAudioThread.reset(new std::thread(ThreadFunction_s, this));
    assert(mFakeAudioThread);
    mStateCallback(aStream, mUserPtr, CUBEB_STATE_STARTED);
    return CUBEB_OK;
  }

  int StreamStop(cubeb_stream* aStream) {
    assert(mFakeAudioThread);
    mStreamStop = true;
    mFakeAudioThread->join();
    mFakeAudioThread.reset();
    mStateCallback(aStream, mUserPtr, CUBEB_STATE_STOPPED);
    return CUBEB_OK;
  }

  void StreamDestroy(cubeb_stream* aStream) { mMockStream.reset(); }

 private:
  // This needs to have the exact same memory layout as a real cubeb backend.
  // It's very important for this `ops` member to be the very first member of
  // the class, and to not have any virtual members (to avoid having a vtable).
  const cubeb_ops* ops;
  // The callback to call when the device list has been changed.
  cubeb_device_collection_changed_callback
      mInputDeviceCollectionChangeCallback = nullptr;
  cubeb_device_collection_changed_callback
      mOutputDeviceCollectionChangeCallback = nullptr;
  cubeb_data_callback mDataCallback = nullptr;
  cubeb_state_callback mStateCallback = nullptr;
  // The pointer to pass in the callback.
  void* mInputDeviceCollectionChangeUserPtr = nullptr;
  void* mOutputDeviceCollectionChangeUserPtr = nullptr;
  void* mUserPtr = nullptr;
  // Whether or not this backend supports device collection change notification
  // via a system callback. If not, Gecko is expected to re-query the list every
  // time.
  bool mSupportsDeviceCollectionChangedCallback = true;
  // Our input and output devices.
  nsTArray<cubeb_device_info> mInputDevices;
  nsTArray<cubeb_device_info> mOutputDevices;

  // Thread that simulates the audio thread.
  std::unique_ptr<std::thread> mFakeAudioThread;
  // Signal to the audio thread that stream is stopped.
  std::atomic_bool mStreamStop{true};
  // The fake stream instance.
  std::unique_ptr<MockCubebStream> mMockStream;
  // The stream sample rate
  uint32_t mSampleRate = 0;
  // The audio buffer used on data callback.
  float mOutputBuffer[NUM_OF_CHANNELS * NUM_OF_FRAMES];
};

void cubeb_mock_destroy(cubeb* context) {
  delete reinterpret_cast<MockCubeb*>(context);
}

int cubeb_mock_enumerate_devices(cubeb* context, cubeb_device_type type,
                                 cubeb_device_collection* out) {
  MockCubeb* mock = reinterpret_cast<MockCubeb*>(context);
  return mock->EnumerateDevices(type, out);
}

int cubeb_mock_device_collection_destroy(cubeb* context,
                                         cubeb_device_collection* collection) {
  delete[] collection->device;
  return CUBEB_OK;
}

int cubeb_mock_register_device_collection_changed(
    cubeb* context, cubeb_device_type devtype,
    cubeb_device_collection_changed_callback callback, void* user_ptr) {
  MockCubeb* mock = reinterpret_cast<MockCubeb*>(context);
  return mock->RegisterDeviceCollectionChangeCallback(devtype, callback,
                                                      user_ptr);
}

int cubeb_mock_stream_init(
    cubeb* context, cubeb_stream** stream, char const* stream_name,
    cubeb_devid input_device, cubeb_stream_params* input_stream_params,
    cubeb_devid output_device, cubeb_stream_params* output_stream_params,
    unsigned int latency, cubeb_data_callback data_callback,
    cubeb_state_callback state_callback, void* user_ptr) {
  MockCubeb* mock = reinterpret_cast<MockCubeb*>(context);
  return mock->StreamInit(context, stream, input_stream_params,
                          output_stream_params, data_callback, state_callback,
                          user_ptr);
}

int cubeb_mock_stream_start(cubeb_stream* stream) {
  MockCubeb::MockCubebStream* mockStream =
      reinterpret_cast<MockCubeb::MockCubebStream*>(stream);
  MockCubeb* mock = reinterpret_cast<MockCubeb*>(mockStream->context);
  return mock->StreamStart(stream);
}

int cubeb_mock_stream_stop(cubeb_stream* stream) {
  MockCubeb::MockCubebStream* mockStream =
      reinterpret_cast<MockCubeb::MockCubebStream*>(stream);
  MockCubeb* mock = reinterpret_cast<MockCubeb*>(mockStream->context);
  return mock->StreamStop(stream);
}

void cubeb_mock_stream_destroy(cubeb_stream* stream) {
  MockCubeb::MockCubebStream* mockStream =
      reinterpret_cast<MockCubeb::MockCubebStream*>(stream);
  MockCubeb* mock = reinterpret_cast<MockCubeb*>(mockStream->context);
  return mock->StreamDestroy(stream);
}

static char const* cubeb_mock_get_backend_id(cubeb* context) {
#if defined(XP_MACOSX)
  return "audiounit";
#elif defined(XP_WIN)
  return "wasapi";
#elif defined(ANDROID)
  return "opensl";
#elif defined(__OpenBSD__)
  return "sndio";
#else
  return "pulse";
#endif
}

static int cubeb_mock_stream_set_volume(cubeb_stream* stream, float volume) {
  return CUBEB_OK;
}

int cubeb_mock_get_min_latency(cubeb* context, cubeb_stream_params params,
                               uint32_t* latency_ms) {
  *latency_ms = NUM_OF_FRAMES;
  return CUBEB_OK;
}

void PrintDevice(cubeb_device_info aInfo) {
  printf(
      "id: %zu\n"
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
      reinterpret_cast<uintptr_t>(aInfo.devid), aInfo.device_id,
      aInfo.friendly_name, aInfo.group_id, aInfo.vendor_name, aInfo.type,
      aInfo.state, aInfo.preferred, aInfo.format, aInfo.default_format,
      aInfo.max_channels, aInfo.default_rate, aInfo.max_rate, aInfo.min_rate,
      aInfo.latency_lo, aInfo.latency_hi);
}

void PrintDevice(AudioDeviceInfo* aInfo) {
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

  printf(
      "device id: %zu\n"
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
      reinterpret_cast<uintptr_t>(id), NS_LossyConvertUTF16toASCII(name).get(),
      NS_LossyConvertUTF16toASCII(groupid).get(),
      NS_LossyConvertUTF16toASCII(vendor).get(), type, state, preferred,
      supportedFormat, defaultFormat, maxChannels, defaultRate, maxRate,
      minRate, minLatency, maxLatency);
}

cubeb_device_info DeviceTemplate(cubeb_devid aId, cubeb_device_type aType,
                                 const char* name) {
  // A fake input device
  cubeb_device_info device;
  device.devid = aId;
  device.device_id = "nice name";
  device.friendly_name = name;
  device.group_id = "the physical device";
  device.vendor_name = "mozilla";
  device.type = aType;
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

cubeb_device_info DeviceTemplate(cubeb_devid aId, cubeb_device_type aType) {
  return DeviceTemplate(aId, aType, "nice name");
}

void AddDevices(MockCubeb* mock, uint32_t device_count,
                cubeb_device_type deviceType) {
  mock->ClearDevices(deviceType);
  // Add a few input devices (almost all the same but it does not really
  // matter as long as they have distinct IDs and only one is the default
  // devices)
  for (uintptr_t i = 0; i < device_count; i++) {
    cubeb_device_info device =
        DeviceTemplate(reinterpret_cast<void*>(i + 1), deviceType);
    // Make it so that the last device is the default input device.
    if (i == device_count - 1) {
      device.preferred = CUBEB_DEVICE_PREF_ALL;
    }
    mock->AddDevice(device);
  }
}

#endif  // MOCKCUBEB_H_
