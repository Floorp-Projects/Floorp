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

#ifndef __VideoDecoder_h__
#define __VideoDecoder_h__

#include <atomic>
#include <queue>
#include <thread>

// This include is required in order for content_decryption_module to work
// on Unix systems.
#include "stddef.h"
#include "content_decryption_module.h"
#include "WMFH264Decoder.h"

class VideoDecoder : public RefCounted
{
public:
  explicit VideoDecoder(cdm::Host_8 *aHost);

  cdm::Status InitDecode(const cdm::VideoDecoderConfig& aConfig);

  cdm::Status Decode(const cdm::InputBuffer& aEncryptedBuffer,
                     cdm::VideoFrame* aVideoFrame);

  void Reset();

  void DecodingComplete();

  bool HasShutdown() { return mHasShutdown; }

private:

  virtual ~VideoDecoder();

  cdm::Status Drain(cdm::VideoFrame* aVideoFrame);

  struct DecodeData {
    std::vector<uint8_t> mBuffer;
    uint64_t mTimestamp = 0;
    CryptoMetaData mCrypto;
  };

  cdm::Status OutputFrame(cdm::VideoFrame* aVideoFrame);

  HRESULT SampleToVideoFrame(IMFSample* aSample,
                             int32_t aWidth,
                             int32_t aHeight,
                             int32_t aStride,
                             cdm::VideoFrame* aVideoFrame);

  cdm::Host_8* mHost;
  wmf::AutoPtr<wmf::WMFH264Decoder> mDecoder;

  std::queue<wmf::CComPtr<IMFSample>> mOutputQueue;

  bool mHasShutdown;
};

#endif // __VideoDecoder_h__
