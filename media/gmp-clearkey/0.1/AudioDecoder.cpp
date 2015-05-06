/*
 * Copyright 2015, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdint>
#include <limits>

#include "AudioDecoder.h"
#include "ClearKeyDecryptionManager.h"
#include "ClearKeyUtils.h"
#include "gmp-task-utils.h"

using namespace wmf;

AudioDecoder::AudioDecoder(GMPAudioHost *aHostAPI)
  : mHostAPI(aHostAPI)
  , mCallback(nullptr)
  , mWorkerThread(nullptr)
  , mMutex(nullptr)
  , mNumInputTasks(0)
  , mHasShutdown(false)
{
  // We drop the ref in DecodingComplete().
  AddRef();
}

AudioDecoder::~AudioDecoder()
{
  if (mMutex) {
    mMutex->Destroy();
  }
}

void
AudioDecoder::InitDecode(const GMPAudioCodec& aConfig,
                         GMPAudioDecoderCallback* aCallback)
{
  mCallback = aCallback;
  assert(mCallback);
  mDecoder = new WMFAACDecoder();
  HRESULT hr = mDecoder->Init(aConfig.mChannelCount,
                              aConfig.mSamplesPerSecond,
                              (BYTE*)aConfig.mExtraData,
                              aConfig.mExtraDataLen);
  LOG("[%p] AudioDecoder::InitializeAudioDecoder() hr=0x%x\n", this, hr);
  if (FAILED(hr)) {
    mCallback->Error(GMPGenericErr);
    return;
  }
  auto err = GetPlatform()->createmutex(&mMutex);
  if (GMP_FAILED(err)) {
    mCallback->Error(GMPGenericErr);
    return;
  }
}

void
AudioDecoder::EnsureWorker()
{
  if (!mWorkerThread) {
    GetPlatform()->createthread(&mWorkerThread);
    if (!mWorkerThread) {
      mCallback->Error(GMPAllocErr);
      return;
    }
  }
}

void
AudioDecoder::Decode(GMPAudioSamples* aInput)
{
  EnsureWorker();
  {
    AutoLock lock(mMutex);
    mNumInputTasks++;
  }
  mWorkerThread->Post(WrapTaskRefCounted(this,
                                         &AudioDecoder::DecodeTask,
                                         aInput));
}

void
AudioDecoder::DecodeTask(GMPAudioSamples* aInput)
{
  HRESULT hr;

  {
    AutoLock lock(mMutex);
    mNumInputTasks--;
    assert(mNumInputTasks >= 0);
  }

  if (!aInput || !mHostAPI || !mDecoder) {
    LOG("Decode job not set up correctly!");
    return;
  }

  const uint8_t* inBuffer = aInput->Buffer();
  if (!inBuffer) {
    LOG("No buffer for encoded samples!\n");
    return;
  }

  const GMPEncryptedBufferMetadata* crypto = aInput->GetDecryptionData();
  std::vector<uint8_t> buffer(inBuffer, inBuffer + aInput->Size());
  if (crypto) {
    // Plugin host should have set up its decryptor/key sessions
    // before trying to decode!
    GMPErr rv =
      ClearKeyDecryptionManager::Get()->Decrypt(&buffer[0], buffer.size(), crypto);

    if (GMP_FAILED(rv)) {
      CK_LOGE("Failed to decrypt with key id %08x...", *(uint32_t*)crypto->KeyId());
      MaybeRunOnMainThread(WrapTask(mCallback, &GMPAudioDecoderCallback::Error, rv));
      return;
    }
  }

  hr = mDecoder->Input(&buffer[0],
                       buffer.size(),
                       aInput->TimeStamp());

  // We must delete the input sample!
  GetPlatform()->runonmainthread(WrapTask(aInput, &GMPAudioSamples::Destroy));

  SAMPLE_LOG("AudioDecoder::DecodeTask() Input ret hr=0x%x\n", hr);
  if (FAILED(hr)) {
    LOG("AudioDecoder::DecodeTask() decode failed ret=0x%x%s\n",
        hr,
        ((hr == MF_E_NOTACCEPTING) ? " (MF_E_NOTACCEPTING)" : ""));
    return;
  }

  while (hr == S_OK) {
    CComPtr<IMFSample> output;
    hr = mDecoder->Output(&output);
    SAMPLE_LOG("AudioDecoder::DecodeTask() output ret=0x%x\n", hr);
    if (hr == S_OK) {
      ReturnOutput(output);
    }
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      AutoLock lock(mMutex);
      if (mNumInputTasks == 0) {
        // We have run all input tasks. We *must* notify Gecko so that it will
        // send us more data.
        MaybeRunOnMainThread(WrapTask(mCallback, &GMPAudioDecoderCallback::InputDataExhausted));
      }
    } else if (FAILED(hr)) {
      LOG("AudioDecoder::DecodeTask() output failed hr=0x%x\n", hr);
    }
  }
}

void
AudioDecoder::ReturnOutput(IMFSample* aSample)
{
  SAMPLE_LOG("[%p] AudioDecoder::ReturnOutput()\n", this);
  assert(aSample);

  HRESULT hr;

  GMPAudioSamples* samples = nullptr;
  mHostAPI->CreateSamples(kGMPAudioIS16Samples, &samples);
  if (!samples) {
    LOG("Failed to create i420 frame!\n");
    return;
  }

  hr = MFToGMPSample(aSample, samples);
  if (FAILED(hr)) {
    samples->Destroy();
    LOG("Failed to prepare output sample!");
    return;
  }
  ENSURE(SUCCEEDED(hr), /*void*/);

  MaybeRunOnMainThread(WrapTask(mCallback, &GMPAudioDecoderCallback::Decoded, samples));
}

HRESULT
AudioDecoder::MFToGMPSample(IMFSample* aInput,
                            GMPAudioSamples* aOutput)
{
  ENSURE(aInput != nullptr, E_POINTER);
  ENSURE(aOutput != nullptr, E_POINTER);

  HRESULT hr;
  CComPtr<IMFMediaBuffer> mediaBuffer;

  hr = aInput->ConvertToContiguousBuffer(&mediaBuffer);
  ENSURE(SUCCEEDED(hr), hr);

  BYTE* data = nullptr; // Note: *data will be owned by the IMFMediaBuffer, we don't need to free it.
  DWORD maxLength = 0, currentLength = 0;
  hr = mediaBuffer->Lock(&data, &maxLength, &currentLength);
  ENSURE(SUCCEEDED(hr), hr);

  auto err = aOutput->SetBufferSize(currentLength);
  ENSURE(GMP_SUCCEEDED(err), E_FAIL);

  memcpy(aOutput->Buffer(), data, currentLength);

  mediaBuffer->Unlock();

  LONGLONG hns = 0;
  hr = aInput->GetSampleTime(&hns);
  ENSURE(SUCCEEDED(hr), hr);
  aOutput->SetTimeStamp(HNsToUsecs(hns));
  aOutput->SetChannels(mDecoder->Channels());
  aOutput->SetRate(mDecoder->Rate());

  return S_OK;
}

void
AudioDecoder::Reset()
{
  if (mDecoder) {
    mDecoder->Reset();
  }
  if (mCallback) {
    mCallback->ResetComplete();
  }
}

void
AudioDecoder::DrainTask()
{
  mDecoder->Drain();

  // Return any pending output.
  HRESULT hr = S_OK;
  while (hr == S_OK) {
    CComPtr<IMFSample> output;
    hr = mDecoder->Output(&output);
    SAMPLE_LOG("AudioDecoder::DrainTask() output ret=0x%x\n", hr);
    if (hr == S_OK) {
      ReturnOutput(output);
    }
  }
  MaybeRunOnMainThread(WrapTask(mCallback, &GMPAudioDecoderCallback::DrainComplete));
}

void
AudioDecoder::Drain()
{
  if (!mDecoder) {
    return;
  }
  EnsureWorker();
  mWorkerThread->Post(WrapTaskRefCounted(this,
                                         &AudioDecoder::DrainTask));
}

void
AudioDecoder::DecodingComplete()
{
  if (mWorkerThread) {
    mWorkerThread->Join();
  }
  mHasShutdown = true;

  // Release the reference we added in the constructor. There may be
  // WrapRefCounted tasks that also hold references to us, and keep
  // us alive a little longer.
  Release();
}

void
AudioDecoder::MaybeRunOnMainThread(GMPTask* aTask)
{
  class MaybeRunTask : public GMPTask
  {
  public:
    MaybeRunTask(AudioDecoder* aDecoder, GMPTask* aTask)
      : mDecoder(aDecoder), mTask(aTask)
    { }

    virtual void Run(void) {
      if (mDecoder->HasShutdown()) {
        CK_LOGD("Trying to dispatch to main thread after AudioDecoder has shut down");
        return;
      }

      mTask->Run();
    }

    virtual void Destroy()
    {
      mTask->Destroy();
      delete this;
    }

  private:
    RefPtr<AudioDecoder> mDecoder;
    GMPTask* mTask;
  };

  GetPlatform()->runonmainthread(new MaybeRunTask(this, aTask));
}
