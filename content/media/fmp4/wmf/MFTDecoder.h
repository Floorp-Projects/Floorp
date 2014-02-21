/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MFTDecoder_h_)
#define MFTDecoder_h_

#include "WMF.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsIThread.h"

namespace mozilla {

class MFTDecoder : public AtomicRefCounted<MFTDecoder> {
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(MTFDecoder)
  MFTDecoder();
  ~MFTDecoder();

  // Creates the MFT. First thing to do as part of setup.
  //
  // Params:
  //  - aMFTClsID the clsid used by CoCreateInstance to instantiate the
  //    decoder MFT.
  HRESULT Create(const GUID& aMFTClsID);

  // Sets the input and output media types. Call after Init().
  //
  // Params:
  //  - aInputType needs at least major and minor types set.
  //  - aOutputSubType is the minor type of the same major type e.g.
  //    MFVideoFormat_H264. This is used to select the output type out
  //    of all the available output types of the MFT.
  HRESULT SetMediaTypes(IMFMediaType* aInputType,
                        const GUID& aOutputSubType);

  // Returns the MFT's IMFAttributes object.
  TemporaryRef<IMFAttributes> GetAttributes();

  // Retrieves the media type being output. This may not be valid until
  //  the first sample is decoded.
  HRESULT GetOutputMediaType(RefPtr<IMFMediaType>& aMediaType);

  // Submits data into the MFT for processing.
  //
  // Returns:
  //  - MF_E_NOTACCEPTING if the decoder can't accept input. The data
  //    must be resubmitted after Output() stops producing output.
  HRESULT Input(const uint8_t* aData,
                uint32_t aDataSize,
                int64_t aTimestampUsecs);

  // Retrieves output from the MFT. Call this once Input() returns
  // MF_E_NOTACCEPTING. Some MFTs with hardware acceleration (the H.264
  // decoder MFT in particular) can't handle it if clients hold onto
  // references to the output IMFSample, so don't do that.
  //
  // Returns:
  //  - MF_E_TRANSFORM_STREAM_CHANGE if the underlying stream output
  //    type changed. Retrieve the output media type and reconfig client,
  //    else you may misinterpret the MFT's output.
  //  - MF_E_TRANSFORM_NEED_MORE_INPUT if no output can be produced
  //    due to lack of input.
  //  - S_OK if an output frame is produced.
  HRESULT Output(RefPtr<IMFSample>* aOutput);

  // Sends a flush message to the MFT. This causes it to discard all
  // input data. Use before seeking.
  HRESULT Flush();

  // Sends a message to the MFT.
  HRESULT SendMFTMessage(MFT_MESSAGE_TYPE aMsg, UINT32 aData);

private:

  HRESULT SetDecoderOutputType();

  HRESULT CreateInputSample(const uint8_t* aData,
                            uint32_t aDataSize,
                            int64_t aTimestampUsecs,
                            RefPtr<IMFSample>* aOutSample);

  HRESULT CreateOutputSample(RefPtr<IMFSample>* aOutSample);

  MFT_INPUT_STREAM_INFO mInputStreamInfo;
  MFT_OUTPUT_STREAM_INFO mOutputStreamInfo;

  RefPtr<IMFTransform> mDecoder;

  GUID mOutputSubtype;

  // True if the IMFTransform allocates the samples that it returns.
  bool mMFTProvidesOutputSamples;
};

} // namespace mozilla

#endif
