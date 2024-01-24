/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef MOCKCUBEB_H_
#define MOCKCUBEB_H_

#include "AudioDeviceInfo.h"
#include "AudioGenerator.h"
#include "AudioVerifier.h"
#include "MediaEventSource.h"
#include "mozilla/DataMutex.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ThreadSafeWeakPtr.h"
#include "nsTArray.h"

#include <thread>
#include <atomic>
#include <chrono>

namespace mozilla {
const uint32_t MAX_OUTPUT_CHANNELS = 2;
const uint32_t MAX_INPUT_CHANNELS = 2;

struct cubeb_ops {
  int (*init)(cubeb** context, char const* context_name);
  char const* (*get_backend_id)(cubeb* context);
  int (*get_max_channel_count)(cubeb* context, uint32_t* max_channels);
  int (*get_min_latency)(cubeb* context, cubeb_stream_params params,
                         uint32_t* latency_ms);
  int (*get_preferred_sample_rate)(cubeb* context, uint32_t* rate);
  int (*get_supported_input_processing_params)(
      cubeb* context, cubeb_input_processing_params* params);
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
  int (*stream_get_position)(cubeb_stream* stream, uint64_t* position);
  int (*stream_get_latency)(cubeb_stream* stream, uint32_t* latency);
  int (*stream_get_input_latency)(cubeb_stream* stream, uint32_t* latency);
  int (*stream_set_volume)(cubeb_stream* stream, float volumes);
  int (*stream_set_name)(cubeb_stream* stream, char const* stream_name);
  int (*stream_get_current_device)(cubeb_stream* stream,
                                   cubeb_device** const device);
  int (*stream_set_input_mute)(cubeb_stream* stream, int mute);
  int (*stream_set_input_processing_params)(
      cubeb_stream* stream, cubeb_input_processing_params params);
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

static int cubeb_mock_stream_get_position(cubeb_stream* stream,
                                          uint64_t* position);

static void cubeb_mock_stream_destroy(cubeb_stream* stream);

static char const* cubeb_mock_get_backend_id(cubeb* context);

static int cubeb_mock_stream_set_volume(cubeb_stream* stream, float volume);

static int cubeb_mock_stream_set_name(cubeb_stream* stream,
                                      char const* stream_name);

static int cubeb_mock_stream_register_device_changed_callback(
    cubeb_stream* stream,
    cubeb_device_changed_callback device_changed_callback);

static int cubeb_mock_get_min_latency(cubeb* context,
                                      cubeb_stream_params params,
                                      uint32_t* latency_ms);

static int cubeb_mock_get_preferred_sample_rate(cubeb* context, uint32_t* rate);

static int cubeb_mock_get_max_channel_count(cubeb* context,
                                            uint32_t* max_channels);

// Mock cubeb impl, only supports device enumeration for now.
cubeb_ops const mock_ops = {
    /*.init =*/NULL,
    /*.get_backend_id =*/cubeb_mock_get_backend_id,
    /*.get_max_channel_count =*/cubeb_mock_get_max_channel_count,
    /*.get_min_latency =*/cubeb_mock_get_min_latency,
    /*.get_preferred_sample_rate =*/cubeb_mock_get_preferred_sample_rate,
    /*.get_supported_input_processing_params =*/
    NULL,
    /*.enumerate_devices =*/cubeb_mock_enumerate_devices,
    /*.device_collection_destroy =*/cubeb_mock_device_collection_destroy,
    /*.destroy =*/cubeb_mock_destroy,
    /*.stream_init =*/cubeb_mock_stream_init,
    /*.stream_destroy =*/cubeb_mock_stream_destroy,
    /*.stream_start =*/cubeb_mock_stream_start,
    /*.stream_stop =*/cubeb_mock_stream_stop,
    /*.stream_get_position =*/cubeb_mock_stream_get_position,
    /*.stream_get_latency =*/NULL,
    /*.stream_get_input_latency =*/NULL,
    /*.stream_set_volume =*/cubeb_mock_stream_set_volume,
    /*.stream_set_name =*/cubeb_mock_stream_set_name,
    /*.stream_get_current_device =*/NULL,

    /*.stream_set_input_mute =*/NULL,
    /*.stream_set_input_processing_params =*/
    NULL,
    /*.stream_device_destroy =*/NULL,
    /*.stream_register_device_changed_callback =*/
    cubeb_mock_stream_register_device_changed_callback,
    /*.register_device_collection_changed =*/
    cubeb_mock_register_device_collection_changed};

class SmartMockCubebStream;

// Represents the fake cubeb_stream. The context instance is needed to
// provide access on cubeb_ops struct.
class MockCubebStream {
  friend class MockCubeb;

  // These members need to have the exact same memory layout as a real
  // cubeb_stream, so that AsMock() returns a pointer to this that can be used
  // as a cubeb_stream.
  cubeb* context;
  void* mUserPtr;

 public:
  enum class KeepProcessing { No, Yes };
  enum class RunningMode { Automatic, Manual };

  MockCubebStream(cubeb* aContext, char const* aStreamName,
                  cubeb_devid aInputDevice,
                  cubeb_stream_params* aInputStreamParams,
                  cubeb_devid aOutputDevice,
                  cubeb_stream_params* aOutputStreamParams,
                  cubeb_data_callback aDataCallback,
                  cubeb_state_callback aStateCallback, void* aUserPtr,
                  SmartMockCubebStream* aSelf, RunningMode aRunningMode,
                  bool aFrozenStart);

  ~MockCubebStream();

  int Start();
  int Stop();
  uint64_t Position();
  void Destroy();
  int SetName(char const* aName);
  int RegisterDeviceChangedCallback(
      cubeb_device_changed_callback aDeviceChangedCallback);

  cubeb_stream* AsCubebStream();
  static MockCubebStream* AsMock(cubeb_stream* aStream);

  char const* StreamName() const { return mName.get(); }
  cubeb_devid GetInputDeviceID() const;
  cubeb_devid GetOutputDeviceID() const;

  uint32_t InputChannels() const;
  uint32_t OutputChannels() const;
  uint32_t SampleRate() const;
  uint32_t InputFrequency() const;

  void SetDriftFactor(float aDriftFactor);
  void ForceError();
  void ForceDeviceChanged();
  void Thaw();

  // For RunningMode::Manual, drive this MockCubebStream forward.
  KeepProcessing ManualDataCallback(long aNrFrames);

  // Enable input recording for this driver. This is best called before
  // the thread is running, but is safe to call whenever.
  void SetOutputRecordingEnabled(bool aEnabled);
  // Enable input recording for this driver. This is best called before
  // the thread is running, but is safe to call whenever.
  void SetInputRecordingEnabled(bool aEnabled);
  // Get the recorded output from this stream. This doesn't copy, and therefore
  // only works once.
  nsTArray<AudioDataValue>&& TakeRecordedOutput();
  // Get the recorded input from this stream. This doesn't copy, and therefore
  // only works once.
  nsTArray<AudioDataValue>&& TakeRecordedInput();

  MediaEventSource<nsCString>& NameSetEvent();
  MediaEventSource<cubeb_state>& StateEvent();
  MediaEventSource<uint32_t>& FramesProcessedEvent();
  // Notified when frames are processed after first non-silent output
  MediaEventSource<uint32_t>& FramesVerifiedEvent();
  // Notified when the stream is Stop()ed
  MediaEventSource<std::tuple<uint64_t, float, uint32_t>>&
  OutputVerificationEvent();
  MediaEventSource<void>& ErrorForcedEvent();
  MediaEventSource<void>& DeviceChangeForcedEvent();

 private:
  KeepProcessing Process(long aNrFrames);
  KeepProcessing Process10Ms();

 public:
  const RunningMode mRunningMode;
  const bool mHasInput;
  const bool mHasOutput;
  SmartMockCubebStream* const mSelf;

 private:
  void NotifyState(cubeb_state aState);

  static constexpr long kMaxNrFrames = 1920;
  // Monitor used to block start until mFrozenStart is false.
  Monitor mFrozenStartMonitor MOZ_UNANNOTATED;
  // Whether this stream should wait for an explicit start request before
  // starting. Protected by FrozenStartMonitor.
  bool mFrozenStart;
  // Used to abort a frozen start if cubeb_stream_start() is called currently
  // with a blocked cubeb_stream_start() call.
  std::atomic_bool mStreamStop{true};
  // Whether or not the output-side of this stream (what is written from the
  // callback output buffer) is recorded in an internal buffer. The data is then
  // available via `GetRecordedOutput`.
  std::atomic_bool mOutputRecordingEnabled{false};
  // Whether or not the input-side of this stream (what is written from the
  // callback input buffer) is recorded in an internal buffer. The data is then
  // available via `TakeRecordedInput`.
  std::atomic_bool mInputRecordingEnabled{false};
  // The audio buffer used on data callback.
  AudioDataValue mOutputBuffer[MAX_OUTPUT_CHANNELS * kMaxNrFrames] = {};
  AudioDataValue mInputBuffer[MAX_INPUT_CHANNELS * kMaxNrFrames] = {};
  // The audio callback
  cubeb_data_callback mDataCallback = nullptr;
  // The stream state callback
  cubeb_state_callback mStateCallback = nullptr;
  // The device changed callback
  cubeb_device_changed_callback mDeviceChangedCallback = nullptr;
  // A name for this stream
  nsCString mName;
  // The stream params
  cubeb_stream_params mOutputParams = {};
  cubeb_stream_params mInputParams = {};
  /* Device IDs */
  cubeb_devid mInputDeviceID;
  cubeb_devid mOutputDeviceID;

  std::atomic<float> mDriftFactor{1.0};
  std::atomic_bool mFastMode{false};
  std::atomic_bool mForceErrorState{false};
  std::atomic_bool mForceDeviceChanged{false};
  std::atomic_bool mDestroyed{false};
  std::atomic<uint64_t> mPosition{0};
  AudioGenerator<AudioDataValue> mAudioGenerator;
  AudioVerifier<AudioDataValue> mAudioVerifier;

  MediaEventProducer<nsCString> mNameSetEvent;
  MediaEventProducer<cubeb_state> mStateEvent;
  MediaEventProducer<uint32_t> mFramesProcessedEvent;
  MediaEventProducer<uint32_t> mFramesVerifiedEvent;
  MediaEventProducer<std::tuple<uint64_t, float, uint32_t>>
      mOutputVerificationEvent;
  MediaEventProducer<void> mErrorForcedEvent;
  MediaEventProducer<void> mDeviceChangedForcedEvent;
  // The recorded data, copied from the output_buffer of the callback.
  // Interleaved.
  nsTArray<AudioDataValue> mRecordedOutput;
  // The recorded data, copied from the input buffer of the callback.
  // Interleaved.
  nsTArray<AudioDataValue> mRecordedInput;
};

class SmartMockCubebStream
    : public MockCubebStream,
      public SupportsThreadSafeWeakPtr<SmartMockCubebStream> {
 public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(SmartMockCubebStream)
  SmartMockCubebStream(cubeb* aContext, char const* aStreamName,
                       cubeb_devid aInputDevice,
                       cubeb_stream_params* aInputStreamParams,
                       cubeb_devid aOutputDevice,
                       cubeb_stream_params* aOutputStreamParams,
                       cubeb_data_callback aDataCallback,
                       cubeb_state_callback aStateCallback, void* aUserPtr,
                       RunningMode aRunningMode, bool aFrozenStart)
      : MockCubebStream(aContext, aStreamName, aInputDevice, aInputStreamParams,
                        aOutputDevice, aOutputStreamParams, aDataCallback,
                        aStateCallback, aUserPtr, this, aRunningMode,
                        aFrozenStart) {}
};

// This class has two facets: it is both a fake cubeb backend that is intended
// to be used for testing, and passed to Gecko code that expects a normal
// backend, but is also controllable by the test code to decide what the backend
// should do, depending on what is being tested.
class MockCubeb {
  // This needs to have the exact same memory layout as a real cubeb backend.
  // It's very important for the `ops` member to be the very first member of
  // the class, and for MockCubeb to not have any virtual members (to avoid
  // having a vtable), so that AsMock() returns a pointer to this that can be
  // used as a cubeb backend.
  const cubeb_ops* ops;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MockCubeb);

 public:
  using RunningMode = MockCubebStream::RunningMode;

  MockCubeb();
  explicit MockCubeb(RunningMode aRunningMode);
  // Cubeb backend implementation
  // This allows passing this class as a cubeb* instance.
  // cubeb_destroy(context) should eventually be called on the return value
  // iff this method is called.
  cubeb* AsCubebContext();
  static MockCubeb* AsMock(cubeb* aContext);
  void Destroy();
  // Fill in the collection parameter with all devices of aType.
  int EnumerateDevices(cubeb_device_type aType,
                       cubeb_device_collection* aCollection);
  // Clear the collection parameter and deallocate its related memory space.
  int DestroyDeviceCollection(cubeb_device_collection* aCollection);

  // For a given device type, add a callback, called with a user pointer, when
  // the device collection for this backend changes (i.e. a device has been
  // removed or added).
  int RegisterDeviceCollectionChangeCallback(
      cubeb_device_type aDevType,
      cubeb_device_collection_changed_callback aCallback, void* aUserPtr);

  // Control API

  // Add an input or output device to this backend. This calls the device
  // collection invalidation callback if needed.
  void AddDevice(cubeb_device_info aDevice);
  // Remove a specific input or output device to this backend, returns true if
  // a device was removed. This calls the device collection invalidation
  // callback if needed.
  bool RemoveDevice(cubeb_devid aId);
  // Remove all input or output devices from this backend, without calling the
  // callback. This is meant to clean up in between tests.
  void ClearDevices(cubeb_device_type aType);

  // This allows simulating a backend that does not support setting a device
  // collection invalidation callback, to be able to test the fallback path.
  void SetSupportDeviceChangeCallback(bool aSupports);

  // This causes the next stream init with this context to return failure;
  void ForceStreamInitError();

  // Makes MockCubebStreams starting after this point wait for AllowStart().
  // Callers must ensure they get a hold of the stream through StreamInitEvent
  // to be able to start them.
  void SetStreamStartFreezeEnabled(bool aEnabled);

  // Helper class that automatically unforces a forced audio thread on release.
  class AudioThreadAutoUnforcer {
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AudioThreadAutoUnforcer)

   public:
    explicit AudioThreadAutoUnforcer(MockCubeb* aContext)
        : mContext(aContext) {}

   protected:
    virtual ~AudioThreadAutoUnforcer() { mContext->UnforceAudioThread(); }
    MockCubeb* mContext;
  };

  // Creates the audio thread if one is not available. The audio thread remains
  // forced until UnforceAudioThread is called. The returned promise is resolved
  // when the audio thread is running. With this, a test can ensure starting
  // audio streams is deterministically fast across platforms for more accurate
  // results.
  using ForcedAudioThreadPromise =
      MozPromise<RefPtr<AudioThreadAutoUnforcer>, nsresult, false>;
  RefPtr<ForcedAudioThreadPromise> ForceAudioThread();

  // Allows a forced audio thread to stop.
  void UnforceAudioThread();

  int StreamInit(cubeb* aContext, cubeb_stream** aStream,
                 char const* aStreamName, cubeb_devid aInputDevice,
                 cubeb_stream_params* aInputStreamParams,
                 cubeb_devid aOutputDevice,
                 cubeb_stream_params* aOutputStreamParams,
                 cubeb_data_callback aDataCallback,
                 cubeb_state_callback aStateCallback, void* aUserPtr);

  void StreamDestroy(MockCubebStream* aStream);

  void GoFaster();
  void DontGoFaster();

  MediaEventSource<RefPtr<SmartMockCubebStream>>& StreamInitEvent();
  MediaEventSource<RefPtr<SmartMockCubebStream>>& StreamDestroyEvent();

  // MockCubeb specific API
  void StartStream(MockCubebStream* aStream);
  void StopStream(MockCubebStream* aStream);

  // Simulates the audio thread. The thread is created at Start and destroyed
  // at Stop. At next StreamStart a new thread is created.
  static void ThreadFunction_s(MockCubeb* aContext) {
    aContext->ThreadFunction();
  }

  void ThreadFunction();

 private:
  ~MockCubeb();
  // The callback to call when the device list has been changed.
  cubeb_device_collection_changed_callback
      mInputDeviceCollectionChangeCallback = nullptr;
  cubeb_device_collection_changed_callback
      mOutputDeviceCollectionChangeCallback = nullptr;
  // The pointer to pass in the callback.
  void* mInputDeviceCollectionChangeUserPtr = nullptr;
  void* mOutputDeviceCollectionChangeUserPtr = nullptr;
  void* mUserPtr = nullptr;
  // Whether or not this backend supports device collection change
  // notification via a system callback. If not, Gecko is expected to re-query
  // the list every time.
  bool mSupportsDeviceCollectionChangedCallback = true;
  const RunningMode mRunningMode;
  Atomic<bool> mStreamInitErrorState;
  // Whether new MockCubebStreams should be frozen on start.
  Atomic<bool> mStreamStartFreezeEnabled{false};
  // Whether the audio thread is forced, i.e., whether it remains active even
  // with no live streams.
  Atomic<bool> mForcedAudioThread{false};
  Atomic<bool> mHasCubebContext{false};
  Atomic<bool> mDestroyed{false};
  MozPromiseHolder<ForcedAudioThreadPromise> mForcedAudioThreadPromise;
  // Our input and output devices.
  nsTArray<cubeb_device_info> mInputDevices;
  nsTArray<cubeb_device_info> mOutputDevices;

  // The streams that are currently running.
  DataMutex<nsTArray<RefPtr<SmartMockCubebStream>>> mLiveStreams{
      "MockCubeb::mLiveStreams"};
  // Thread that simulates the audio thread, shared across MockCubebStreams to
  // avoid unintended drift. This is set together with mLiveStreams, under the
  // mLiveStreams DataMutex.
  UniquePtr<std::thread> mFakeAudioThread;
  // Whether to run the fake audio thread in fast mode, not caring about wall
  // clock time. false is default and means data is processed every 10ms. When
  // true we sleep(0) between iterations instead of 10ms.
  std::atomic<bool> mFastMode{false};

  MediaEventProducer<RefPtr<SmartMockCubebStream>> mStreamInitEvent;
  MediaEventProducer<RefPtr<SmartMockCubebStream>> mStreamDestroyEvent;
};

int cubeb_mock_enumerate_devices(cubeb* context, cubeb_device_type type,
                                 cubeb_device_collection* out) {
  return MockCubeb::AsMock(context)->EnumerateDevices(type, out);
}

int cubeb_mock_device_collection_destroy(cubeb* context,
                                         cubeb_device_collection* collection) {
  return MockCubeb::AsMock(context)->DestroyDeviceCollection(collection);
}

int cubeb_mock_register_device_collection_changed(
    cubeb* context, cubeb_device_type devtype,
    cubeb_device_collection_changed_callback callback, void* user_ptr) {
  return MockCubeb::AsMock(context)->RegisterDeviceCollectionChangeCallback(
      devtype, callback, user_ptr);
}

int cubeb_mock_stream_init(
    cubeb* context, cubeb_stream** stream, char const* stream_name,
    cubeb_devid input_device, cubeb_stream_params* input_stream_params,
    cubeb_devid output_device, cubeb_stream_params* output_stream_params,
    unsigned int latency, cubeb_data_callback data_callback,
    cubeb_state_callback state_callback, void* user_ptr) {
  return MockCubeb::AsMock(context)->StreamInit(
      context, stream, stream_name, input_device, input_stream_params,
      output_device, output_stream_params, data_callback, state_callback,
      user_ptr);
}

int cubeb_mock_stream_start(cubeb_stream* stream) {
  return MockCubebStream::AsMock(stream)->Start();
}

int cubeb_mock_stream_stop(cubeb_stream* stream) {
  return MockCubebStream::AsMock(stream)->Stop();
}

int cubeb_mock_stream_get_position(cubeb_stream* stream, uint64_t* position) {
  *position = MockCubebStream::AsMock(stream)->Position();
  return CUBEB_OK;
}

void cubeb_mock_stream_destroy(cubeb_stream* stream) {
  MockCubebStream::AsMock(stream)->Destroy();
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

static int cubeb_mock_stream_set_name(cubeb_stream* stream,
                                      char const* stream_name) {
  return MockCubebStream::AsMock(stream)->SetName(stream_name);
  return CUBEB_OK;
}

int cubeb_mock_stream_register_device_changed_callback(
    cubeb_stream* stream,
    cubeb_device_changed_callback device_changed_callback) {
  return MockCubebStream::AsMock(stream)->RegisterDeviceChangedCallback(
      device_changed_callback);
}

int cubeb_mock_get_min_latency(cubeb* context, cubeb_stream_params params,
                               uint32_t* latency_ms) {
  *latency_ms = 10;
  return CUBEB_OK;
}

int cubeb_mock_get_preferred_sample_rate(cubeb* context, uint32_t* rate) {
  *rate = 44100;
  return CUBEB_OK;
}

int cubeb_mock_get_max_channel_count(cubeb* context, uint32_t* max_channels) {
  *max_channels = MAX_OUTPUT_CHANNELS;
  return CUBEB_OK;
}

void PrintDevice(cubeb_device_info aInfo);

void PrintDevice(AudioDeviceInfo* aInfo);

cubeb_device_info DeviceTemplate(cubeb_devid aId, cubeb_device_type aType,
                                 const char* name);

cubeb_device_info DeviceTemplate(cubeb_devid aId, cubeb_device_type aType);

void AddDevices(MockCubeb* mock, uint32_t device_count,
                cubeb_device_type deviceType);

}  // namespace mozilla

#endif  // MOCKCUBEB_H_
