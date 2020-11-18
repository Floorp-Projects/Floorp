/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MockCubeb.h"

#include "gtest/gtest.h"

namespace mozilla {

MockCubebStream::MockCubebStream(cubeb* aContext, cubeb_devid aInputDevice,
                                 cubeb_stream_params* aInputStreamParams,
                                 cubeb_devid aOutputDevice,
                                 cubeb_stream_params* aOutputStreamParams,
                                 cubeb_data_callback aDataCallback,
                                 cubeb_state_callback aStateCallback,
                                 void* aUserPtr)
    : context(aContext),
      mHasInput(aInputStreamParams),
      mHasOutput(aOutputStreamParams),
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
  mStreamStop = false;
  reinterpret_cast<MockCubeb*>(context)->StartStream(this);
  cubeb_stream* stream = reinterpret_cast<cubeb_stream*>(this);
  mStateCallback(stream, mUserPtr, CUBEB_STATE_STARTED);
  return CUBEB_OK;
}

int MockCubebStream::Stop() {
  mStreamStop = true;
  mOutputVerificationEvent.Notify(MakeTuple(
      mAudioVerifier.PreSilenceSamples(), mAudioVerifier.EstimatedFreq(),
      mAudioVerifier.CountDiscontinuities()));
  int rv = reinterpret_cast<MockCubeb*>(context)->StopStream(this);
  if (rv == CUBEB_OK) {
    cubeb_stream* stream = reinterpret_cast<cubeb_stream*>(this);
    mStateCallback(stream, mUserPtr, CUBEB_STATE_STOPPED);
  }
  return rv;
}

cubeb_devid MockCubebStream::GetInputDeviceID() const { return mInputDeviceID; }

cubeb_devid MockCubebStream::GetOutputDeviceID() const {
  return mOutputDeviceID;
}

uint32_t MockCubebStream::InputChannels() const {
  return mAudioGenerator.mChannels;
}

uint32_t MockCubebStream::InputSampleRate() const {
  return mAudioGenerator.mSampleRate;
}

uint32_t MockCubebStream::InputFrequency() const {
  return mAudioGenerator.mFrequency;
}

void MockCubebStream::SetDriftFactor(float aDriftFactor) {
  mDriftFactor = aDriftFactor;
}

void MockCubebStream::ForceError() { mForceErrorState = true; }

MediaEventSource<uint32_t>& MockCubebStream::FramesProcessedEvent() {
  return mFramesProcessedEvent;
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
  cubeb_stream* stream = reinterpret_cast<cubeb_stream*>(this);
  const long outframes =
      mDataCallback(stream, mUserPtr, mHasInput ? mInputBuffer : nullptr,
                    mHasOutput ? mOutputBuffer : nullptr, nrFrames);

  mAudioVerifier.AppendDataInterleaved(mOutputBuffer, outframes,
                                       NUM_OF_CHANNELS);

  if (mAudioVerifier.PreSilenceEnded()) {
    mFramesProcessedEvent.Notify(outframes);
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
    NS_DispatchBackgroundTask(NS_NewRunnableFunction(
        __func__, [cubeb = reinterpret_cast<MockCubeb*>(context), this] {
          cubeb->StopStream(this);
        }));
    mStateCallback(stream, mUserPtr, CUBEB_STATE_ERROR);
    mErrorForcedEvent.Notify();
    mStreamStop = true;
    return;
  }
}

MockCubeb::MockCubeb() : ops(&mock_ops) {}

MockCubeb::~MockCubeb() { MOZ_ASSERT(!mFakeAudioThread); };

cubeb* MockCubeb::AsCubebContext() { return reinterpret_cast<cubeb*>(this); }

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

int MockCubeb::StreamInit(cubeb* aContext, cubeb_stream** aStream,
                          cubeb_devid aInputDevice,
                          cubeb_stream_params* aInputStreamParams,
                          cubeb_devid aOutputDevice,
                          cubeb_stream_params* aOutputStreamParams,
                          cubeb_data_callback aDataCallback,
                          cubeb_state_callback aStateCallback, void* aUserPtr) {
  MockCubebStream* mockStream = new MockCubebStream(
      aContext, aInputDevice, aInputStreamParams, aOutputDevice,
      aOutputStreamParams, aDataCallback, aStateCallback, aUserPtr);
  *aStream = reinterpret_cast<cubeb_stream*>(mockStream);
  mStreamInitEvent.Notify(mockStream);
  return CUBEB_OK;
}

void MockCubeb::StreamDestroy(cubeb_stream* aStream) {
  mStreamDestroyEvent.Notify();
  MockCubebStream* mockStream = reinterpret_cast<MockCubebStream*>(aStream);
  delete mockStream;
}

void MockCubeb::GoFaster() { mFastMode = true; }

void MockCubeb::DontGoFaster() { mFastMode = false; }

MediaEventSource<MockCubebStream*>& MockCubeb::StreamInitEvent() {
  return mStreamInitEvent;
}

MediaEventSource<void>& MockCubeb::StreamDestroyEvent() {
  return mStreamDestroyEvent;
}

void MockCubeb::StartStream(MockCubebStream* aStream) {
  auto streams = mLiveStreams.Lock();
  MOZ_ASSERT(!streams->Contains(aStream));
  streams->AppendElement(aStream);
  if (!mFakeAudioThread) {
    mFakeAudioThread = WrapUnique(new std::thread(ThreadFunction_s, this));
  }
}

int MockCubeb::StopStream(MockCubebStream* aStream) {
  UniquePtr<std::thread> audioThread;
  {
    auto streams = mLiveStreams.Lock();
    if (!streams->Contains(aStream)) {
      return CUBEB_ERROR;
    }
    MOZ_ASSERT(streams->Contains(aStream));
    streams->RemoveElement(aStream);
    MOZ_ASSERT(mFakeAudioThread);
    if (streams->IsEmpty()) {
      audioThread = std::move(mFakeAudioThread);
    }
  }
  if (audioThread) {
    audioThread->join();
  }
  return CUBEB_OK;
}

void MockCubeb::ThreadFunction() {
  while (true) {
    {
      auto streams = mLiveStreams.Lock();
      for (auto& stream : *streams) {
        stream->Process10Ms();
      }
      if (streams->IsEmpty()) {
        break;
      }
    }
    std::this_thread::sleep_for(
        std::chrono::microseconds(mFastMode ? 0 : 10 * PR_USEC_PER_MSEC));
  }
}

}  // namespace mozilla
