/*
 * Copyright 2013, Mozilla Foundation and contributors
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

#include "AnnexB.h"
#include "ClearKeyDecryptionManager.h"
#include "ClearKeyUtils.h"
#include "gmp-task-utils.h"
#include "mozilla/Endian.h"
#include "VideoDecoder.h"

using namespace wmf;

VideoDecoder::VideoDecoder(GMPVideoHost *aHostAPI)
  : mHostAPI(aHostAPI)
  , mCallback(nullptr)
  , mWorkerThread(nullptr)
  , mMutex(nullptr)
  , mNumInputTasks(0)
  , mSentExtraData(false)
{
}

VideoDecoder::~VideoDecoder()
{
  mMutex->Destroy();
}

void
VideoDecoder::InitDecode(const GMPVideoCodec& aCodecSettings,
                         const uint8_t* aCodecSpecific,
                         uint32_t aCodecSpecificLength,
                         GMPVideoDecoderCallback* aCallback,
                         int32_t aCoreCount)
{
  mCallback = aCallback;
  assert(mCallback);
  mDecoder = new WMFH264Decoder();
  HRESULT hr = mDecoder->Init();
  if (FAILED(hr)) {
    mCallback->Error(GMPGenericErr);
    return;
  }

  auto err = GetPlatform()->createmutex(&mMutex);
  if (GMP_FAILED(err)) {
    mCallback->Error(GMPGenericErr);
    return;
  }

  // The first byte is mPacketizationMode, which is only relevant for
  // WebRTC/OpenH264 usecase.
  const uint8_t* avcc = aCodecSpecific + 1;
  const uint8_t* avccEnd = aCodecSpecific + aCodecSpecificLength;
  mExtraData.insert(mExtraData.end(), avcc, avccEnd);

  AnnexB::ConvertConfig(mExtraData, mAnnexB);
}

void
VideoDecoder::EnsureWorker()
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
VideoDecoder::Decode(GMPVideoEncodedFrame* aInputFrame,
                     bool aMissingFrames,
                     const uint8_t* aCodecSpecificInfo,
                     uint32_t aCodecSpecificInfoLength,
                     int64_t aRenderTimeMs)
{
  if (aInputFrame->BufferType() != GMP_BufferLength32) {
    // Gecko should only send frames with 4 byte NAL sizes to GMPs.
    mCallback->Error(GMPGenericErr);
    return;
  }

  EnsureWorker();

  {
    AutoLock lock(mMutex);
    mNumInputTasks++;
  }

  // Note: we don't need the codec specific info on a per-frame basis.
  // It's mostly useful for WebRTC use cases.

  mWorkerThread->Post(WrapTask(this,
                               &VideoDecoder::DecodeTask,
                               aInputFrame));
}

void
VideoDecoder::DecodeTask(GMPVideoEncodedFrame* aInput)
{
  CK_LOGD("VideoDecoder::DecodeTask");
  HRESULT hr;

  {
    AutoLock lock(mMutex);
    mNumInputTasks--;
    assert(mNumInputTasks >= 0);
  }

  if (!aInput || !mHostAPI || !mDecoder) {
    CK_LOGE("Decode job not set up correctly!");
    return;
  }

  const uint8_t* inBuffer = aInput->Buffer();
  if (!inBuffer) {
    CK_LOGE("No buffer for encoded frame!\n");
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
      GetPlatform()->runonmainthread(WrapTask(mCallback, &GMPVideoDecoderCallback::Error, rv));
      return;
    }
  }

  AnnexB::ConvertFrameInPlace(buffer);

  if (aInput->FrameType() == kGMPKeyFrame) {
    // We must send the SPS and PPS to Windows Media Foundation's decoder.
    // Note: We do this *after* decryption, otherwise the subsample info
    // would be incorrect.
    buffer.insert(buffer.begin(), mAnnexB.begin(), mAnnexB.end());
  }

  hr = mDecoder->Input(buffer.data(),
                       buffer.size(),
                       aInput->TimeStamp(),
                       aInput->Duration());

  // We must delete the input sample!
  GetPlatform()->runonmainthread(WrapTask(aInput, &GMPVideoEncodedFrame::Destroy));

  CK_LOGD("VideoDecoder::DecodeTask() Input ret hr=0x%x\n", hr);
  if (FAILED(hr)) {
    CK_LOGE("VideoDecoder::DecodeTask() decode failed ret=0x%x%s\n",
        hr,
        ((hr == MF_E_NOTACCEPTING) ? " (MF_E_NOTACCEPTING)" : ""));
    return;
  }

  while (hr == S_OK) {
    CComPtr<IMFSample> output;
    hr = mDecoder->Output(&output);
    CK_LOGD("VideoDecoder::DecodeTask() output ret=0x%x\n", hr);
    if (hr == S_OK) {
      ReturnOutput(output);
    }
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      AutoLock lock(mMutex);
      if (mNumInputTasks == 0) {
        // We have run all input tasks. We *must* notify Gecko so that it will
        // send us more data.
        GetPlatform()->runonmainthread(WrapTask(mCallback, &GMPVideoDecoderCallback::InputDataExhausted));
      }
    }
    if (FAILED(hr)) {
      CK_LOGE("VideoDecoder::DecodeTask() output failed hr=0x%x\n", hr);
    }
  }
}

void
VideoDecoder::ReturnOutput(IMFSample* aSample)
{
  CK_LOGD("[%p] VideoDecoder::ReturnOutput()\n", this);
  assert(aSample);

  HRESULT hr;

  GMPVideoFrame* f = nullptr;
  mHostAPI->CreateFrame(kGMPI420VideoFrame, &f);
  if (!f) {
    CK_LOGE("Failed to create i420 frame!\n");
    return;
  }
  auto vf = static_cast<GMPVideoi420Frame*>(f);

  hr = SampleToVideoFrame(aSample, vf);
  ENSURE(SUCCEEDED(hr), /*void*/);

  GetPlatform()->runonmainthread(WrapTask(mCallback, &GMPVideoDecoderCallback::Decoded, vf));

}

HRESULT
VideoDecoder::SampleToVideoFrame(IMFSample* aSample,
                                 GMPVideoi420Frame* aVideoFrame)
{
  ENSURE(aSample != nullptr, E_POINTER);
  ENSURE(aVideoFrame != nullptr, E_POINTER);

  HRESULT hr;
  CComPtr<IMFMediaBuffer> mediaBuffer;

  // Must convert to contiguous mediaBuffer to use IMD2DBuffer interface.
  hr = aSample->ConvertToContiguousBuffer(&mediaBuffer);
  ENSURE(SUCCEEDED(hr), hr);

  // Try and use the IMF2DBuffer interface if available, otherwise fallback
  // to the IMFMediaBuffer interface. Apparently IMF2DBuffer is more efficient,
  // but only some systems (Windows 8?) support it.
  BYTE* data = nullptr;
  LONG stride = 0;
  CComPtr<IMF2DBuffer> twoDBuffer;
  hr = mediaBuffer->QueryInterface(static_cast<IMF2DBuffer**>(&twoDBuffer));
  if (SUCCEEDED(hr)) {
    hr = twoDBuffer->Lock2D(&data, &stride);
    ENSURE(SUCCEEDED(hr), hr);
  } else {
    hr = mediaBuffer->Lock(&data, NULL, NULL);
    ENSURE(SUCCEEDED(hr), hr);
    stride = mDecoder->GetStride();
  }
  int32_t width = mDecoder->GetFrameWidth();
  int32_t height = mDecoder->GetFrameHeight();

  // The V and U planes are stored 16-row-aligned, so we need to add padding
  // to the row heights to ensure the Y'CbCr planes are referenced properly.
  // YV12, planar format: [YYYY....][VVVV....][UUUU....]
  // i.e., Y, then V, then U.
  uint32_t padding = 0;
  if (height % 16 != 0) {
    padding = 16 - (height % 16);
  }
  int32_t y_size = stride * (height + padding);
  int32_t v_size = stride * (height + padding) / 4;
  int32_t halfStride = (stride + 1) / 2;
  int32_t halfHeight = (height + 1) / 2;

  GetPlatform()->syncrunonmainthread(WrapTask(aVideoFrame,
                                  &GMPVideoi420Frame::CreateEmptyFrame,
                                  stride, height, stride, halfStride, halfStride));

  auto err = aVideoFrame->SetWidth(width);
  ENSURE(GMP_SUCCEEDED(err), E_FAIL);
  err = aVideoFrame->SetHeight(height);
  ENSURE(GMP_SUCCEEDED(err), E_FAIL);

  uint8_t* outBuffer = aVideoFrame->Buffer(kGMPYPlane);
  ENSURE(outBuffer != nullptr, E_FAIL);
  assert(aVideoFrame->AllocatedSize(kGMPYPlane) >= stride*height);
  memcpy(outBuffer, data, stride*height);

  outBuffer = aVideoFrame->Buffer(kGMPUPlane);
  ENSURE(outBuffer != nullptr, E_FAIL);
  assert(aVideoFrame->AllocatedSize(kGMPUPlane) >= halfStride*halfHeight);
  memcpy(outBuffer, data+y_size, halfStride*halfHeight);

  outBuffer = aVideoFrame->Buffer(kGMPVPlane);
  ENSURE(outBuffer != nullptr, E_FAIL);
  assert(aVideoFrame->AllocatedSize(kGMPVPlane) >= halfStride*halfHeight);
  memcpy(outBuffer, data + y_size + v_size, halfStride*halfHeight);

  if (twoDBuffer) {
    twoDBuffer->Unlock2D();
  } else {
    mediaBuffer->Unlock();
  }

  LONGLONG hns = 0;
  hr = aSample->GetSampleTime(&hns);
  ENSURE(SUCCEEDED(hr), hr);
  aVideoFrame->SetTimestamp(HNsToUsecs(hns));

  hr = aSample->GetSampleDuration(&hns);
  ENSURE(SUCCEEDED(hr), hr);
  aVideoFrame->SetDuration(HNsToUsecs(hns));

  return S_OK;
}

void
VideoDecoder::Reset()
{
  mDecoder->Reset();
  mCallback->ResetComplete();
}

void
VideoDecoder::DrainTask()
{
  if (FAILED(mDecoder->Drain())) {
    GetPlatform()->syncrunonmainthread(WrapTask(mCallback, &GMPVideoDecoderCallback::DrainComplete));
  }

  // Return any pending output.
  HRESULT hr = S_OK;
  while (hr == S_OK) {
    CComPtr<IMFSample> output;
    hr = mDecoder->Output(&output);
    CK_LOGD("VideoDecoder::DrainTask() output ret=0x%x\n", hr);
    if (hr == S_OK) {
      ReturnOutput(output);
    }
  }
  GetPlatform()->syncrunonmainthread(WrapTask(mCallback, &GMPVideoDecoderCallback::DrainComplete));
}

void
VideoDecoder::Drain()
{
  EnsureWorker();
  mWorkerThread->Post(WrapTask(this,
                               &VideoDecoder::DrainTask));
}

void
VideoDecoder::DecodingComplete()
{
  if (mWorkerThread) {
    mWorkerThread->Join();
  }
  delete this;
}
