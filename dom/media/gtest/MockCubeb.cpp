/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MockCubeb.h"

#include "gtest/gtest.h"

namespace mozilla {

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

void cubeb_mock_destroy(cubeb* context) { delete MockCubeb::AsMock(context); }

MockCubebStream::MockCubebStream(cubeb* aContext, cubeb_devid aInputDevice,
                                 cubeb_stream_params* aInputStreamParams,
                                 cubeb_devid aOutputDevice,
                                 cubeb_stream_params* aOutputStreamParams,
                                 cubeb_data_callback aDataCallback,
                                 cubeb_state_callback aStateCallback,
                                 void* aUserPtr, SmartMockCubebStream* aSelf,
                                 bool aFrozenStart)
    : context(aContext),
      mHasInput(aInputStreamParams),
      mHasOutput(aOutputStreamParams),
      mSelf(aSelf),
      mFrozenStartMonitor("MockCubebStream::mFrozenStartMonitor"),
      mFrozenStart(aFrozenStart),
      mDataCallback(aDataCallback),
      mStateCallback(aStateCallback),
      mUserPtr(aUserPtr),
      mInputDeviceID(aInputDevice),
      mOutputDeviceID(aOutputDevice),
      mAudioGenerator(NUM_OF_CHANNELS,
                      aInputStreamParams ? aInputStreamParams->rate
                                         : aOutputStreamParams->rate,
                      100 /* aFrequency */),
      mAudioVerifier(aInputStreamParams ? aInputStreamParams->rate
                                        : aOutputStreamParams->rate,
                     100 /* aFrequency */) {
  if (aInputStreamParams) {
    mInputParams = *aInputStreamParams;
  }
  if (aOutputStreamParams) {
    mOutputParams = *aOutputStreamParams;
  }
}

MockCubebStream::~MockCubebStream() = default;

int MockCubebStream::Start() {
  mStateCallback(AsCubebStream(), mUserPtr, CUBEB_STATE_STARTED);
  mStreamStop = false;
  MonitorAutoLock lock(mFrozenStartMonitor);
  if (mFrozenStart) {
    NS_DispatchBackgroundTask(NS_NewRunnableFunction(
        "MockCubebStream::WaitForThawBeforeStart",
        [this, self = RefPtr<SmartMockCubebStream>(mSelf)] {
          MonitorAutoLock lock(mFrozenStartMonitor);
          while (mFrozenStart) {
            mFrozenStartMonitor.Wait();
          }
          if (!mStreamStop) {
            MockCubeb::AsMock(context)->StartStream(mSelf);
          }
        }));
    return CUBEB_OK;
  }
  MockCubeb::AsMock(context)->StartStream(this);
  return CUBEB_OK;
}

int MockCubebStream::Stop() {
  mOutputVerificationEvent.Notify(MakeTuple(
      mAudioVerifier.PreSilenceSamples(), mAudioVerifier.EstimatedFreq(),
      mAudioVerifier.CountDiscontinuities()));
  int rv = MockCubeb::AsMock(context)->StopStream(this);
  mStreamStop = true;
  if (rv == CUBEB_OK) {
    mStateCallback(AsCubebStream(), mUserPtr, CUBEB_STATE_STOPPED);
  }
  return rv;
}

cubeb_stream* MockCubebStream::AsCubebStream() {
  return reinterpret_cast<cubeb_stream*>(this);
}

MockCubebStream* MockCubebStream::AsMock(cubeb_stream* aStream) {
  return reinterpret_cast<MockCubebStream*>(aStream);
}

cubeb_devid MockCubebStream::GetInputDeviceID() const { return mInputDeviceID; }

cubeb_devid MockCubebStream::GetOutputDeviceID() const {
  return mOutputDeviceID;
}

uint32_t MockCubebStream::InputChannels() const {
  return mAudioGenerator.ChannelCount();
}

uint32_t MockCubebStream::OutputChannels() const {
  return mOutputParams.channels;
}

uint32_t MockCubebStream::InputSampleRate() const {
  return mAudioGenerator.mSampleRate;
}

uint32_t MockCubebStream::InputFrequency() const {
  return mAudioGenerator.mFrequency;
}

nsTArray<AudioDataValue>&& MockCubebStream::TakeRecordedOutput() {
  return std::move(mRecordedOutput);
}

void MockCubebStream::SetDriftFactor(float aDriftFactor) {
  mDriftFactor = aDriftFactor;
}

void MockCubebStream::ForceError() { mForceErrorState = true; }

void MockCubebStream::Thaw() {
  MonitorAutoLock l(mFrozenStartMonitor);
  mFrozenStart = false;
  mFrozenStartMonitor.Notify();
}

void MockCubebStream::SetOutputRecordingEnabled(bool aEnabled) {
  mOutputRecordingEnabled = aEnabled;
}

MediaEventSource<uint32_t>& MockCubebStream::FramesProcessedEvent() {
  return mFramesProcessedEvent;
}

MediaEventSource<uint32_t>& MockCubebStream::FramesVerifiedEvent() {
  return mFramesVerifiedEvent;
}

MediaEventSource<Tuple<uint64_t, float, uint32_t>>&
MockCubebStream::OutputVerificationEvent() {
  return mOutputVerificationEvent;
}

MediaEventSource<void>& MockCubebStream::ErrorForcedEvent() {
  return mErrorForcedEvent;
}

void MockCubebStream::Process10Ms() {
  if (mStreamStop) {
    return;
  }

  uint32_t rate = mHasOutput ? mOutputParams.rate : mInputParams.rate;
  const long nrFrames =
      static_cast<long>(static_cast<float>(rate * 10) * mDriftFactor) /
      PR_MSEC_PER_SEC;
  if (mInputParams.rate) {
    mAudioGenerator.GenerateInterleaved(mInputBuffer, nrFrames);
  }
  cubeb_stream* stream = AsCubebStream();
  const long outframes =
      mDataCallback(stream, mUserPtr, mHasInput ? mInputBuffer : nullptr,
                    mHasOutput ? mOutputBuffer : nullptr, nrFrames);

  if (mOutputRecordingEnabled && mHasOutput) {
    mRecordedOutput.AppendElements(mOutputBuffer, outframes * OutputChannels());
  }
  mAudioVerifier.AppendDataInterleaved(mOutputBuffer, outframes,
                                       NUM_OF_CHANNELS);

  mFramesProcessedEvent.Notify(outframes);
  if (mAudioVerifier.PreSilenceEnded()) {
    mFramesVerifiedEvent.Notify(outframes);
  }

  if (outframes < nrFrames) {
    mStateCallback(stream, mUserPtr, CUBEB_STATE_DRAINED);
    mStreamStop = true;
    return;
  }
  if (mForceErrorState) {
    mForceErrorState = false;
    // Let the audio thread (this thread!) run to completion before
    // being released, by joining and releasing on main.
    NS_DispatchBackgroundTask(
        NS_NewRunnableFunction(__func__, [cubeb = MockCubeb::AsMock(context),
                                          this] { cubeb->StopStream(this); }));
    mStateCallback(stream, mUserPtr, CUBEB_STATE_ERROR);
    mErrorForcedEvent.Notify();
    mStreamStop = true;
    return;
  }
}

MockCubeb::MockCubeb() : ops(&mock_ops) {}

MockCubeb::~MockCubeb() { MOZ_ASSERT(!mFakeAudioThread); };

cubeb* MockCubeb::AsCubebContext() { return reinterpret_cast<cubeb*>(this); }

MockCubeb* MockCubeb::AsMock(cubeb* aContext) {
  return reinterpret_cast<MockCubeb*>(aContext);
}

int MockCubeb::EnumerateDevices(cubeb_device_type aType,
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

int MockCubeb::RegisterDeviceCollectionChangeCallback(
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

void MockCubeb::AddDevice(cubeb_device_info aDevice) {
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
    mOutputDeviceCollectionChangeCallback(AsCubebContext(),
                                          mOutputDeviceCollectionChangeUserPtr);
  }
}

bool MockCubeb::RemoveDevice(cubeb_devid aId) {
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
    mOutputDeviceCollectionChangeCallback(AsCubebContext(),
                                          mOutputDeviceCollectionChangeUserPtr);
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

void MockCubeb::ClearDevices(cubeb_device_type aType) {
  mInputDevices.Clear();
  mOutputDevices.Clear();
}

void MockCubeb::SetSupportDeviceChangeCallback(bool aSupports) {
  mSupportsDeviceCollectionChangedCallback = aSupports;
}

void MockCubeb::SetStreamStartFreezeEnabled(bool aEnabled) {
  mStreamStartFreezeEnabled = aEnabled;
}

auto MockCubeb::ForceAudioThread() -> RefPtr<ForcedAudioThreadPromise> {
  RefPtr<ForcedAudioThreadPromise> p =
      mForcedAudioThreadPromise.Ensure(__func__);
  mForcedAudioThread = true;
  StartStream(nullptr);
  return p;
}

void MockCubeb::UnforceAudioThread() {
  mForcedAudioThread = false;
  StopStream(nullptr);
}

int MockCubeb::StreamInit(cubeb* aContext, cubeb_stream** aStream,
                          cubeb_devid aInputDevice,
                          cubeb_stream_params* aInputStreamParams,
                          cubeb_devid aOutputDevice,
                          cubeb_stream_params* aOutputStreamParams,
                          cubeb_data_callback aDataCallback,
                          cubeb_state_callback aStateCallback, void* aUserPtr) {
  auto mockStream = MakeRefPtr<SmartMockCubebStream>(
      aContext, aInputDevice, aInputStreamParams, aOutputDevice,
      aOutputStreamParams, aDataCallback, aStateCallback, aUserPtr,
      mStreamStartFreezeEnabled);
  *aStream = mockStream->AsCubebStream();
  mStreamInitEvent.Notify(mockStream);
  // AddRef the stream to keep it alive. StreamDestroy releases it.
  Unused << mockStream.forget().take();
  return CUBEB_OK;
}

void MockCubeb::StreamDestroy(cubeb_stream* aStream) {
  mStreamDestroyEvent.Notify();
  RefPtr<SmartMockCubebStream> mockStream =
      dont_AddRef(MockCubebStream::AsMock(aStream)->mSelf);
}

void MockCubeb::GoFaster() { mFastMode = true; }

void MockCubeb::DontGoFaster() { mFastMode = false; }

MediaEventSource<RefPtr<SmartMockCubebStream>>& MockCubeb::StreamInitEvent() {
  return mStreamInitEvent;
}

MediaEventSource<void>& MockCubeb::StreamDestroyEvent() {
  return mStreamDestroyEvent;
}

void MockCubeb::StartStream(MockCubebStream* aStream) {
  auto streams = mLiveStreams.Lock();
  MOZ_ASSERT_IF(!aStream, mForcedAudioThread);
  // Forcing an audio thread must happen before starting streams
  MOZ_ASSERT_IF(!aStream, streams->IsEmpty());
  if (aStream) {
    MOZ_ASSERT(!streams->Contains(aStream->mSelf));
    streams->AppendElement(aStream->mSelf);
  }
  if (!mFakeAudioThread) {
    mFakeAudioThread = WrapUnique(new std::thread(ThreadFunction_s, this));
  }
}

int MockCubeb::StopStream(MockCubebStream* aStream) {
  UniquePtr<std::thread> audioThread;
  {
    auto streams = mLiveStreams.Lock();
    if (aStream) {
      if (!streams->Contains(aStream->mSelf)) {
        return CUBEB_ERROR;
      }
      streams->RemoveElement(aStream->mSelf);
    }
    MOZ_ASSERT(mFakeAudioThread);
    if (streams->IsEmpty() && !mForcedAudioThread) {
      audioThread = std::move(mFakeAudioThread);
    }
  }
  if (audioThread) {
    audioThread->join();
  }
  return CUBEB_OK;
}

void MockCubeb::ThreadFunction() {
  if (mForcedAudioThread) {
    mForcedAudioThreadPromise.Resolve(MakeRefPtr<AudioThreadAutoUnforcer>(this),
                                      __func__);
  }
  while (true) {
    {
      auto streams = mLiveStreams.Lock();
      for (auto& stream : *streams) {
        stream->Process10Ms();
      }
      if (streams->IsEmpty() && !mForcedAudioThread) {
        break;
      }
    }
    std::this_thread::sleep_for(
        std::chrono::microseconds(mFastMode ? 0 : 10 * PR_USEC_PER_MSEC));
  }
}

}  // namespace mozilla
