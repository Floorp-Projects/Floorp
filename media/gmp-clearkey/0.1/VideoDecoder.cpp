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
#include "ClearKeyUtils.h"
#include "ClearKeyDecryptionManager.h"
#include "VideoDecoder.h"
#include "mozilla/CheckedInt.h"

using namespace wmf;

VideoDecoder::VideoDecoder(cdm::Host_10* aHost)
    : mHost(aHost), mHasShutdown(false) {
  CK_LOGD("VideoDecoder created");

  // We drop the ref in DecodingComplete().
  AddRef();

  mDecoder = new WMFH264Decoder();

  uint32_t cores = std::max(1u, std::thread::hardware_concurrency());

  HRESULT hr = mDecoder->Init(cores);
  if (FAILED(hr)) {
    CK_LOGE("Failed to initialize mDecoder!");
  }
}

VideoDecoder::~VideoDecoder() { CK_LOGD("VideoDecoder destroyed"); }

cdm::Status VideoDecoder::InitDecode(const cdm::VideoDecoderConfig_2& aConfig) {
  CK_LOGD("VideoDecoder::InitDecode");

  if (!mDecoder) {
    CK_LOGD("VideoDecoder::InitDecode failed to init WMFH264Decoder");

    return cdm::Status::kDecodeError;
  }

  return cdm::Status::kSuccess;
}

cdm::Status VideoDecoder::Decode(const cdm::InputBuffer_2& aInputBuffer,
                                 cdm::VideoFrame* aVideoFrame) {
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
    return cdm::Status::kDecodeError;
  }

  std::vector<uint8_t>& buffer = data->mBuffer;

  if (data->mCrypto.IsValid()) {
    Status rv =
        ClearKeyDecryptionManager::Get()->Decrypt(buffer, data->mCrypto);

    if (STATUS_FAILED(rv)) {
      CK_LOGARRAY("Failed to decrypt video using key ", aInputBuffer.key_id,
                  aInputBuffer.key_id_size);
      return rv;
    }
  }

  hr = mDecoder->Input(buffer.data(), buffer.size(), data->mTimestamp);

  CK_LOGD("VideoDecoder::Decode() Input ret hr=0x%x", hr);

  if (FAILED(hr)) {
    assert(hr != MF_E_TRANSFORM_NEED_MORE_INPUT);

    CK_LOGE("VideoDecoder::Decode() decode failed ret=0x%x%s", hr,
            ((hr == MF_E_NOTACCEPTING) ? " (MF_E_NOTACCEPTING)" : ""));
    CK_LOGD("Decode failed. The decoder is not accepting input");
    return cdm::Status::kDecodeError;
  }

  return OutputFrame(aVideoFrame);
}

cdm::Status VideoDecoder::OutputFrame(cdm::VideoFrame* aVideoFrame) {
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
    return cdm::Status::kNeedMoreData;
  }

  // We will get a MF_E_TRANSFORM_NEED_MORE_INPUT every time, as we always
  // consume everything in the buffer.
  if (hr != MF_E_TRANSFORM_NEED_MORE_INPUT && FAILED(hr)) {
    CK_LOGD("Decode failed output ret=0x%x", hr);
    return cdm::Status::kDecodeError;
  }

  CComPtr<IMFSample> result = mOutputQueue.front();
  mOutputQueue.pop();

  // The Chromium CDM API doesn't have support for negative strides, though
  // they are theoretically possible in real world data.
  if (mDecoder->GetStride() <= 0) {
    CK_LOGD("VideoDecoder::OutputFrame Failed! (negative stride)");
    return cdm::Status::kDecodeError;
  }

  const IntRect& picture = mDecoder->GetPictureRegion();
  hr = SampleToVideoFrame(result, picture.width, picture.height,
                          mDecoder->GetStride(), mDecoder->GetFrameHeight(),
                          aVideoFrame);
  if (FAILED(hr)) {
    CK_LOGD("VideoDecoder::OutputFrame Failed!");
    return cdm::Status::kDecodeError;
  }

  CK_LOGD("VideoDecoder::OutputFrame Succeeded.");
  return cdm::Status::kSuccess;
}

HRESULT
VideoDecoder::SampleToVideoFrame(IMFSample* aSample, int32_t aPictureWidth,
                                 int32_t aPictureHeight, int32_t aStride,
                                 int32_t aFrameHeight,
                                 cdm::VideoFrame* aVideoFrame) {
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

  // WMF stores the U and V planes 16-row-aligned, so we need to add padding
  // to the row heights to ensure the source offsets of the Y'CbCr planes are
  // referenced properly.
  // YV12, planar format: [YYYY....][UUUU....][VVVV....]
  // i.e., Y, then U, then V.
  uint32_t padding = 0;
  if (aFrameHeight % 16 != 0) {
    padding = 16 - (aFrameHeight % 16);
  }
  uint32_t srcYSize = stride * (aFrameHeight + padding);
  uint32_t srcUVSize = stride * (aFrameHeight + padding) / 4;
  uint32_t halfStride = (stride + 1) / 2;

  aVideoFrame->SetStride(cdm::VideoPlane::kYPlane, stride);
  aVideoFrame->SetStride(cdm::VideoPlane::kUPlane, halfStride);
  aVideoFrame->SetStride(cdm::VideoPlane::kVPlane, halfStride);

  aVideoFrame->SetSize(cdm::Size{aPictureWidth, aPictureHeight});

  // Note: We allocate the minimal sized buffer required to send the
  // frame back over to the parent process. This is so that we request the
  // same sized frame as the buffer allocator expects.
  using mozilla::CheckedUint32;
  CheckedUint32 bufferSize = CheckedUint32(stride) * aPictureHeight +
                             ((CheckedUint32(stride) * aPictureHeight) / 4) * 2;

  // If the buffer is bigger than the max for a 32 bit, fail to avoid buffer
  // overflows.
  if (!bufferSize.isValid()) {
    CK_LOGD("VideoDecoder::SampleToFrame Buffersize bigger than UINT32_MAX");
    return E_FAIL;
  }

  // Get the buffer from the host.
  cdm::Buffer* buffer = mHost->Allocate(bufferSize.value());
  aVideoFrame->SetFrameBuffer(buffer);

  // Make sure the buffer is non-null (allocate guarantees it will be of
  // sufficient size).
  if (!buffer) {
    CK_LOGD("VideoDecoder::SampleToFrame Out of memory");
    return E_OUTOFMEMORY;
  }

  uint8_t* outBuffer = buffer->Data();

  aVideoFrame->SetPlaneOffset(cdm::VideoPlane::kYPlane, 0);

  // Offset of U plane is the size of the Y plane, excluding the padding that
  // WMF adds.
  uint32_t dstUOffset = stride * aPictureHeight;
  aVideoFrame->SetPlaneOffset(cdm::VideoPlane::kUPlane, dstUOffset);

  // Offset of the V plane is the size of the Y plane + the size of the U plane,
  // excluding any padding WMF adds.
  uint32_t dstVOffset = stride * aPictureHeight + (stride * aPictureHeight) / 4;
  aVideoFrame->SetPlaneOffset(cdm::VideoPlane::kVPlane, dstVOffset);

  // Copy the pixel data, excluding WMF's padding.
  memcpy(outBuffer, data, stride * aPictureHeight);
  memcpy(outBuffer + dstUOffset, data + srcYSize,
         (stride * aPictureHeight) / 4);
  memcpy(outBuffer + dstVOffset, data + srcYSize + srcUVSize,
         (stride * aPictureHeight) / 4);

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

void VideoDecoder::Reset() {
  CK_LOGD("VideoDecoder::Reset");

  if (mDecoder) {
    mDecoder->Reset();
  }

  // Remove all the frames from the output queue.
  while (!mOutputQueue.empty()) {
    mOutputQueue.pop();
  }
}

cdm::Status VideoDecoder::Drain(cdm::VideoFrame* aVideoFrame) {
  CK_LOGD("VideoDecoder::Drain()");

  if (!mDecoder) {
    CK_LOGD("Drain failed! Decoder was not initialized");
    return Status::kDecodeError;
  }

  mDecoder->Drain();

  // Return any pending output.
  return OutputFrame(aVideoFrame);
}

void VideoDecoder::DecodingComplete() {
  CK_LOGD("VideoDecoder::DecodingComplete()");

  mHasShutdown = true;

  // Release the reference we added in the constructor. There may be
  // WrapRefCounted tasks that also hold references to us, and keep
  // us alive a little longer.
  Release();
}
