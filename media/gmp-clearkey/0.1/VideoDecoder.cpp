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

#include <algorithm>
#include <cstdint>
#include <limits>

#include "BigEndian.h"
#include "ClearKeyDecryptionManager.h"
#include "ClearKeyUtils.h"
#include "VideoDecoder.h"

using namespace wmf;
using namespace cdm;

VideoDecoder::VideoDecoder(Host_8 *aHost)
  : mHost(aHost)
  , mHasShutdown(false)
{
  CK_LOGD("VideoDecoder created");

  // We drop the ref in DecodingComplete().
  AddRef();

  mDecoder = new WMFH264Decoder();

  uint32_t cores = std::max(1u, std::thread::hardware_concurrency());
  HRESULT hr = mDecoder->Init(cores);
}

VideoDecoder::~VideoDecoder()
{
  CK_LOGD("VideoDecoder destroyed");
}

Status
VideoDecoder::InitDecode(const VideoDecoderConfig& aConfig)
{
  CK_LOGD("VideoDecoder::InitDecode");

  if (!mDecoder) {
    CK_LOGD("VideoDecoder::InitDecode failed to init WMFH264Decoder");

    return Status::kDecodeError;
  }

  return Status::kSuccess;
}

Status
VideoDecoder::Decode(const InputBuffer& aInputBuffer, VideoFrame* aVideoFrame)
{
  CK_LOGD("VideoDecoder::Decode");
  // If the input buffer we have been passed has a null buffer, it means we
  // should drain.
  if (!aInputBuffer.data) {
    // This will drain the decoder until there are no frames left to drain,
    // whereupon it will return 'NeedsMoreData'.
    CK_LOGD("VideoDecoder::Decode Input buffer null: Draining");
    return Drain(aVideoFrame);
  }

  DecodeData* data = new DecodeData();
  Assign(data->mBuffer, aInputBuffer.data, aInputBuffer.data_size);
  data->mTimestamp = aInputBuffer.timestamp;
  data->mCrypto = CryptoMetaData(&aInputBuffer);

  AutoPtr<DecodeData> d(data);
  HRESULT hr;

  if (!data || !mDecoder) {
    CK_LOGE("Decode job not set up correctly!");
    return Status::kDecodeError;
  }

  std::vector<uint8_t>& buffer = data->mBuffer;

  if (data->mCrypto.IsValid()) {
    Status rv =
      ClearKeyDecryptionManager::Get()->Decrypt(buffer, data->mCrypto);

    if (STATUS_FAILED(rv)) {
      CK_LOGARRAY("Failed to decrypt video using key ",
                  aInputBuffer.key_id,
                  aInputBuffer.key_id_size);
      return rv;
    }
  }

  hr = mDecoder->Input(buffer.data(),
                       buffer.size(),
                       data->mTimestamp);

  CK_LOGD("VideoDecoder::Decode() Input ret hr=0x%x", hr);


  if (FAILED(hr)) {
    assert(hr != MF_E_TRANSFORM_NEED_MORE_INPUT);

    CK_LOGE("VideoDecoder::Decode() decode failed ret=0x%x%s",
      hr,
      ((hr == MF_E_NOTACCEPTING) ? " (MF_E_NOTACCEPTING)" : ""));
    CK_LOGD("Decode failed. The decoder is not accepting input");
    return Status::kDecodeError;
  }

  return OutputFrame(aVideoFrame);
}

Status VideoDecoder::OutputFrame(VideoFrame* aVideoFrame) {
  CK_LOGD("VideoDecoder::OutputFrame");

  HRESULT hr = S_OK;

  // Read all the output from the decoder. Ideally, this would be a while loop
  // where we read the output and check the result as the condition. However,
  // this produces a memory leak connected to assigning a new CComPtr to the
  // address of the old one, which avoids the CComPtr cleaning up.
  while (true) {
    CComPtr<IMFSample> output;
    hr = mDecoder->Output(&output);

    if (hr != S_OK) {
      break;
    }

    CK_LOGD("VideoDecoder::OutputFrame Decoder output ret=0x%x", hr);

    mOutputQueue.push(output);
    CK_LOGD("VideoDecoder::OutputFrame: Queue size: %u", mOutputQueue.size());
  }

  // If we don't have any inputs, we need more data.
  if (mOutputQueue.empty()) {
    CK_LOGD("Decode failed. Not enought data; Requesting more input");
    return Status::kNeedMoreData;
  }

  // We will get a MF_E_TRANSFORM_NEED_MORE_INPUT every time, as we always
  // consume everything in the buffer.
  if (hr != MF_E_TRANSFORM_NEED_MORE_INPUT && FAILED(hr)) {
    CK_LOGD("Decode failed output ret=0x%x", hr);
    return Status::kDecodeError;
  }

  CComPtr<IMFSample> result = mOutputQueue.front();
  mOutputQueue.pop();

  // The Chromium CDM API doesn't have support for negative strides, though
  // they are theoretically possible in real world data.
  if (mDecoder->GetStride() <= 0) {
    CK_LOGD("VideoDecoder::OutputFrame Failed! (negative stride)");
    return Status::kDecodeError;
  }

  hr = SampleToVideoFrame(result,
                          mDecoder->GetFrameWidth(),
                          mDecoder->GetFrameHeight(),
                          mDecoder->GetStride(),
                          aVideoFrame);
  if (FAILED(hr)) {
    CK_LOGD("VideoDecoder::OutputFrame Failed!");
    return Status::kDecodeError;
  }

  CK_LOGD("VideoDecoder::OutputFrame Succeeded.");
  return Status::kSuccess;
}

HRESULT
VideoDecoder::SampleToVideoFrame(IMFSample* aSample,
                                 int32_t aWidth,
                                 int32_t aHeight,
                                 int32_t aStride,
                                 VideoFrame* aVideoFrame)
{
  CK_LOGD("[%p] VideoDecoder::SampleToVideoFrame()", this);

  ENSURE(aSample != nullptr, E_POINTER);
  ENSURE(aVideoFrame != nullptr, E_POINTER);

  HRESULT hr;
  CComPtr<IMFMediaBuffer> mediaBuffer;

  aVideoFrame->SetFormat(kI420);

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
    hr = mediaBuffer->Lock(&data, nullptr, nullptr);
    ENSURE(SUCCEEDED(hr), hr);
    stride = aStride;
  }

  // The U and V planes are stored 16-row-aligned, so we need to add padding
  // to the row heights to ensure the Y'CbCr planes are referenced properly.
  // YV12, planar format: [YYYY....][UUUU....][VVVV....]
  // i.e., Y, then U, then V.
  uint32_t padding = 0;
  if (aHeight % 16 != 0) {
    padding = 16 - (aHeight % 16);
  }
  uint32_t ySize = stride * (aHeight + padding);
  uint32_t uSize = stride * (aHeight + padding) / 4;
  uint32_t halfStride = (stride + 1) / 2;
  uint32_t halfHeight = (aHeight + 1) / 2;

  aVideoFrame->SetStride(VideoFrame::kYPlane, stride);
  aVideoFrame->SetStride(VideoFrame::kUPlane, halfStride);
  aVideoFrame->SetStride(VideoFrame::kVPlane, halfStride);

  aVideoFrame->SetSize(Size(aWidth, aHeight));

  uint64_t bufferSize = ySize + 2 * uSize;

  // If the buffer is bigger than the max for a 32 bit, fail to avoid buffer
  // overflows.
  if (bufferSize > UINT32_MAX) {
    CK_LOGD("VideoDecoder::SampleToFrame Buffersize bigger than UINT32_MAX");
    return E_FAIL;
  }

  // Get the buffer from the host.
  Buffer* buffer = mHost->Allocate(bufferSize);
  aVideoFrame->SetFrameBuffer(buffer);

  // Make sure the buffer is non-null (allocate guarantees it will be of
  // sufficient size).
  if (!buffer) {
    CK_LOGD("VideoDecoder::SampleToFrame Out of memory");
    return E_OUTOFMEMORY;
  }

  uint8_t* outBuffer = buffer->Data();

  aVideoFrame->SetPlaneOffset(VideoFrame::kYPlane, 0);

  // Offset is the size of the copied y data.
  aVideoFrame->SetPlaneOffset(VideoFrame::kUPlane, ySize);

  // Offset is the size of the copied y data + the size of the copied u data.
  aVideoFrame->SetPlaneOffset(VideoFrame::kVPlane, ySize + uSize);

  // Copy the data.
  memcpy(outBuffer, data, ySize + uSize * 2);

  if (twoDBuffer) {
    twoDBuffer->Unlock2D();
  } else {
    mediaBuffer->Unlock();
  }

  LONGLONG hns = 0;
  hr = aSample->GetSampleTime(&hns);
  ENSURE(SUCCEEDED(hr), hr);

  aVideoFrame->SetTimestamp(HNsToUsecs(hns));

  return S_OK;
}

void
VideoDecoder::Reset()
{
  CK_LOGD("VideoDecoder::Reset");

  if (mDecoder) {
    mDecoder->Reset();
  }

  // Remove all the frames from the output queue.
  while (!mOutputQueue.empty()) {
    mOutputQueue.pop();
  }
}

Status
VideoDecoder::Drain(VideoFrame* aVideoFrame)
{
  CK_LOGD("VideoDecoder::Drain()");

  if (!mDecoder) {
    CK_LOGD("Drain failed! Decoder was not initialized");
    return Status::kDecodeError;
  }

  mDecoder->Drain();

  // Return any pending output.
  return OutputFrame(aVideoFrame);
}

void
VideoDecoder::DecodingComplete()
{
  CK_LOGD("VideoDecoder::DecodingComplete()");

  mHasShutdown = true;

  // Release the reference we added in the constructor. There may be
  // WrapRefCounted tasks that also hold references to us, and keep
  // us alive a little longer.
  Release();
}
