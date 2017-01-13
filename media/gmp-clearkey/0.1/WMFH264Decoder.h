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

#if !defined(WMFH264Decoder_h_)
#define WMFH264Decoder_h_

#include "WMFUtils.h"

namespace wmf {

class WMFH264Decoder {
public:
  WMFH264Decoder();
  ~WMFH264Decoder();

  HRESULT Init(int32_t aCoreCount);

  HRESULT Input(const uint8_t* aData,
                uint32_t aDataSize,
                Microseconds aTimestamp,
                Microseconds aDuration);

  HRESULT Output(IMFSample** aOutput);

  HRESULT Reset();

  int32_t GetFrameWidth() const;
  int32_t GetFrameHeight() const;
  const IntRect& GetPictureRegion() const;
  int32_t GetStride() const;

  HRESULT Drain();

private:

  HRESULT SetDecoderInputType();
  HRESULT SetDecoderOutputType();
  HRESULT SendMFTMessage(MFT_MESSAGE_TYPE aMsg, UINT32 aData);

  HRESULT CreateInputSample(const uint8_t* aData,
                            uint32_t aDataSize,
                            Microseconds aTimestamp,
                            Microseconds aDuration,
                            IMFSample** aOutSample);

  HRESULT CreateOutputSample(IMFSample** aOutSample);

  HRESULT GetOutputSample(IMFSample** aOutSample);
  HRESULT ConfigureVideoFrameGeometry(IMFMediaType* aMediaType);

  MFT_INPUT_STREAM_INFO mInputStreamInfo;
  MFT_OUTPUT_STREAM_INFO mOutputStreamInfo;

  CComPtr<IMFTransform> mDecoder;

  int32_t mVideoWidth;
  int32_t mVideoHeight;
  IntRect mPictureRegion;
  int32_t mStride;

};

} // namespace wmf

#endif
