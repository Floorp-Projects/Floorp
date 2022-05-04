/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFTDecoder.h"
#include "WMFUtils.h"
#include "mozilla/Logging.h"
#include "nsThreadUtils.h"
#include "mozilla/mscom/COMWrappers.h"
#include "mozilla/mscom/Utils.h"
#include "PlatformDecoderModule.h"

#define LOG(...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {
MFTDecoder::MFTDecoder() {
  memset(&mInputStreamInfo, 0, sizeof(MFT_INPUT_STREAM_INFO));
  memset(&mOutputStreamInfo, 0, sizeof(MFT_OUTPUT_STREAM_INFO));
}

MFTDecoder::~MFTDecoder() {
  if (mActivate) {
    // Releases all internal references to the created IMFTransform.
    // https://docs.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imfactivate-shutdownobject
    mActivate->ShutdownObject();
  }
}

HRESULT MFTDecoder::Create(const GUID& aCLSID) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  HRESULT hr = mscom::wrapped::CoCreateInstance(
      aCLSID, nullptr, CLSCTX_INPROC_SERVER,
      IID_PPV_ARGS(static_cast<IMFTransform**>(getter_AddRefs(mDecoder))));
  NS_WARNING_ASSERTION(SUCCEEDED(hr), "Failed to create MFT by CLSID");
  return hr;
}

HRESULT
MFTDecoder::Create(const GUID& aCategory, const GUID& aInSubtype,
                   const GUID& aOutSubtype) {
  // Note: IMFTransform is documented to only be safe on MTA threads.
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  // Use video by default, but select audio if necessary.
  const GUID major = aCategory == MFT_CATEGORY_AUDIO_DECODER
                         ? MFMediaType_Audio
                         : MFMediaType_Video;

  // Ignore null GUIDs to allow searching for all decoders supporting
  // just one input or output type.
  auto createInfo = [&major](const GUID& subtype) -> MFT_REGISTER_TYPE_INFO* {
    if (IsEqualGUID(subtype, GUID_NULL)) {
      return nullptr;
    }

    MFT_REGISTER_TYPE_INFO* info = new MFT_REGISTER_TYPE_INFO();
    info->guidMajorType = major;
    info->guidSubtype = subtype;
    return info;
  };
  const MFT_REGISTER_TYPE_INFO* inInfo = createInfo(aInSubtype);
  const MFT_REGISTER_TYPE_INFO* outInfo = createInfo(aOutSubtype);

  // Request a decoder from the Windows API.
  HRESULT hr;
  IMFActivate** acts = nullptr;
  UINT32 actsNum = 0;

  hr = wmf::MFTEnumEx(aCategory, MFT_ENUM_FLAG_SORTANDFILTER, inInfo, outInfo,
                      &acts, &actsNum);
  delete inInfo;
  delete outInfo;
  if (FAILED(hr)) {
    NS_WARNING(nsPrintfCString("MFTEnumEx failed with code %lx", hr).get());
    return hr;
  }
  if (actsNum == 0) {
    NS_WARNING("MFTEnumEx returned no IMFActivate instances");
    return WINCODEC_ERR_COMPONENTNOTFOUND;
  }
  auto guard = MakeScopeExit([&] {
    // Start from index 1, acts[0] will be stored as a RefPtr to release later.
    for (UINT32 i = 1; i < actsNum; i++) {
      acts[i]->Release();
    }
    CoTaskMemFree(acts);
  });

  // Create the IMFTransform to do the decoding.
  // Note: Ideally we would cache the IMFActivate and call
  // IMFActivate::DetachObject, but doing so causes the MFTs to fail on
  // MFT_MESSAGE_SET_D3D_MANAGER.
  mActivate = RefPtr<IMFActivate>(acts[0]);
  hr = mActivate->ActivateObject(
      IID_PPV_ARGS(static_cast<IMFTransform**>(getter_AddRefs(mDecoder))));
  NS_WARNING_ASSERTION(
      SUCCEEDED(hr),
      nsPrintfCString("IMFActivate::ActivateObject failed with code %lx", hr)
          .get());
  return hr;
}

HRESULT
MFTDecoder::SetMediaTypes(IMFMediaType* aInputType, IMFMediaType* aOutputType,
                          std::function<HRESULT(IMFMediaType*)>&& aCallback) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  // Set the input type to the one the caller gave us...
  HRESULT hr = mDecoder->SetInputType(0, aInputType, 0);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  GUID currentSubtype = {0};
  hr = aOutputType->GetGUID(MF_MT_SUBTYPE, &currentSubtype);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = SetDecoderOutputType(currentSubtype, aOutputType, std::move(aCallback));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = mDecoder->GetInputStreamInfo(0, &mInputStreamInfo);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = SendMFTMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = SendMFTMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  return S_OK;
}

already_AddRefed<IMFAttributes> MFTDecoder::GetAttributes() {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  RefPtr<IMFAttributes> attr;
  HRESULT hr = mDecoder->GetAttributes(getter_AddRefs(attr));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);
  return attr.forget();
}

already_AddRefed<IMFAttributes> MFTDecoder::GetOutputStreamAttributes() {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  RefPtr<IMFAttributes> attr;
  HRESULT hr = mDecoder->GetOutputStreamAttributes(0, getter_AddRefs(attr));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);
  return attr.forget();
}

HRESULT
MFTDecoder::FindDecoderOutputType() {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mOutputType, "SetDecoderTypes must have been called once");

  return FindDecoderOutputTypeWithSubtype(mOutputSubType);
}

HRESULT
MFTDecoder::FindDecoderOutputTypeWithSubtype(const GUID& aSubType) {
  return SetDecoderOutputType(aSubType, nullptr,
                              [](IMFMediaType*) { return S_OK; });
}

HRESULT
MFTDecoder::SetDecoderOutputType(
    const GUID& aSubType, IMFMediaType* aTypeToUse,
    std::function<HRESULT(IMFMediaType*)>&& aCallback) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  NS_ENSURE_TRUE(mDecoder != nullptr, E_POINTER);

  if (!aTypeToUse) {
    aTypeToUse = mOutputType;
  }

  // Iterate the enumerate the output types, until we find one compatible
  // with what we need.
  RefPtr<IMFMediaType> outputType;
  UINT32 typeIndex = 0;
  while (SUCCEEDED(mDecoder->GetOutputAvailableType(
      0, typeIndex++, getter_AddRefs(outputType)))) {
    GUID outSubtype = {0};
    HRESULT hr = outputType->GetGUID(MF_MT_SUBTYPE, &outSubtype);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

    if (aSubType == outSubtype) {
      hr = aCallback(outputType);
      NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

      hr = mDecoder->SetOutputType(0, outputType, 0);
      NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

      hr = mDecoder->GetOutputStreamInfo(0, &mOutputStreamInfo);
      NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

      mMFTProvidesOutputSamples = IsFlagSet(mOutputStreamInfo.dwFlags,
                                            MFT_OUTPUT_STREAM_PROVIDES_SAMPLES);

      mOutputType = outputType;
      mOutputSubType = outSubtype;

      return S_OK;
    }
    outputType = nullptr;
  }
  return E_FAIL;
}

HRESULT
MFTDecoder::SendMFTMessage(MFT_MESSAGE_TYPE aMsg, ULONG_PTR aData) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  NS_ENSURE_TRUE(mDecoder != nullptr, E_POINTER);
  LOG("Send message '%s'", MFTMessageTypeToStr(aMsg));
  HRESULT hr = mDecoder->ProcessMessage(aMsg, aData);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  return S_OK;
}

HRESULT
MFTDecoder::CreateInputSample(const uint8_t* aData, uint32_t aDataSize,
                              int64_t aTimestamp, int64_t aDuration,
                              RefPtr<IMFSample>* aOutSample) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  NS_ENSURE_TRUE(mDecoder != nullptr, E_POINTER);

  HRESULT hr;
  RefPtr<IMFSample> sample;
  hr = wmf::MFCreateSample(getter_AddRefs(sample));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IMFMediaBuffer> buffer;
  int32_t bufferSize =
      std::max<uint32_t>(uint32_t(mInputStreamInfo.cbSize), aDataSize);
  UINT32 alignment =
      (mInputStreamInfo.cbAlignment > 1) ? mInputStreamInfo.cbAlignment - 1 : 0;
  hr = wmf::MFCreateAlignedMemoryBuffer(bufferSize, alignment,
                                        getter_AddRefs(buffer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  DWORD maxLength = 0;
  DWORD currentLength = 0;
  BYTE* dst = nullptr;
  hr = buffer->Lock(&dst, &maxLength, &currentLength);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Copy data into sample's buffer.
  memcpy(dst, aData, aDataSize);

  hr = buffer->Unlock();
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = buffer->SetCurrentLength(aDataSize);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = sample->AddBuffer(buffer);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = sample->SetSampleTime(UsecsToHNs(aTimestamp));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  if (aDuration == 0) {
    // If the sample duration is 0, the decoder will try and estimate the
    // duration. In practice this can lead to some wildly incorrect durations,
    // as in bug 1560440. The Microsoft docs seem conflicting here with
    // `IMFSample::SetSampleDuration` stating 'The duration can also be zero.
    // This might be valid for some types of data.' However,
    // `IMFSample::GetSampleDuration method` states 'If the retrieved duration
    // is zero, or if the method returns MF_E_NO_SAMPLE_DURATION, the duration
    // is unknown. In that case, it might be possible to calculate the duration
    // from the media type--for example, by using the video frame rate or the
    // audio sampling rate.' The latter of those seems to be how the decoder
    // handles 0 duration, hence why it estimates.
    //
    // Since our demuxing pipeline can create 0 duration samples, and since the
    // decoder will override them to something positive anyway, setting them to
    // have a trivial duration seems like the lesser of evils.
    aDuration = 1;
  }
  hr = sample->SetSampleDuration(UsecsToHNs(aDuration));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  *aOutSample = sample.forget();

  return S_OK;
}

HRESULT
MFTDecoder::CreateOutputSample(RefPtr<IMFSample>* aOutSample) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  NS_ENSURE_TRUE(mDecoder != nullptr, E_POINTER);

  HRESULT hr;
  RefPtr<IMFSample> sample;
  hr = wmf::MFCreateSample(getter_AddRefs(sample));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IMFMediaBuffer> buffer;
  int32_t bufferSize = mOutputStreamInfo.cbSize;
  UINT32 alignment = (mOutputStreamInfo.cbAlignment > 1)
                         ? mOutputStreamInfo.cbAlignment - 1
                         : 0;
  hr = wmf::MFCreateAlignedMemoryBuffer(bufferSize, alignment,
                                        getter_AddRefs(buffer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = sample->AddBuffer(buffer);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  *aOutSample = sample.forget();

  return S_OK;
}

HRESULT
MFTDecoder::Output(RefPtr<IMFSample>* aOutput) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  NS_ENSURE_TRUE(mDecoder != nullptr, E_POINTER);

  HRESULT hr;

  MFT_OUTPUT_DATA_BUFFER output = {0};

  bool providedSample = false;
  RefPtr<IMFSample> sample;
  if (*aOutput) {
    output.pSample = *aOutput;
    providedSample = true;
  } else if (!mMFTProvidesOutputSamples) {
    hr = CreateOutputSample(&sample);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
    output.pSample = sample;
  }

  DWORD status = 0;
  hr = mDecoder->ProcessOutput(0, 1, &output, &status);
  if (output.pEvents) {
    // We must release this, as per the IMFTransform::ProcessOutput()
    // MSDN documentation.
    output.pEvents->Release();
    output.pEvents = nullptr;
  }

  if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
    return MF_E_TRANSFORM_STREAM_CHANGE;
  }

  if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
    // Not enough input to produce output. This is an expected failure,
    // so don't warn on encountering it.
    return hr;
  }
  // Treat other errors as unexpected, and warn.
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  if (!output.pSample) {
    return S_OK;
  }

  if (mDiscontinuity) {
    output.pSample->SetUINT32(MFSampleExtension_Discontinuity, TRUE);
    mDiscontinuity = false;
  }

  *aOutput = output.pSample;  // AddRefs
  if (mMFTProvidesOutputSamples && !providedSample) {
    // If the MFT is providing samples, we must release the sample here.
    // Typically only the H.264 MFT provides samples when using DXVA,
    // and it always re-uses the same sample, so if we don't release it
    // MFT::ProcessOutput() deadlocks waiting for the sample to be released.
    output.pSample->Release();
    output.pSample = nullptr;
  }

  return S_OK;
}

HRESULT
MFTDecoder::Input(const uint8_t* aData, uint32_t aDataSize, int64_t aTimestamp,
                  int64_t aDuration) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  NS_ENSURE_TRUE(mDecoder != nullptr, E_POINTER);

  RefPtr<IMFSample> input;
  HRESULT hr =
      CreateInputSample(aData, aDataSize, aTimestamp, aDuration, &input);
  NS_ENSURE_TRUE(SUCCEEDED(hr) && input != nullptr, hr);

  return Input(input);
}

HRESULT
MFTDecoder::Input(IMFSample* aSample) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  HRESULT hr = mDecoder->ProcessInput(0, aSample, 0);
  if (hr == MF_E_NOTACCEPTING) {
    // MFT *already* has enough data to produce a sample. Retrieve it.
    return MF_E_NOTACCEPTING;
  }
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  return S_OK;
}

HRESULT
MFTDecoder::Flush() {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  HRESULT hr = SendMFTMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  mDiscontinuity = true;

  return S_OK;
}

HRESULT
MFTDecoder::GetInputMediaType(RefPtr<IMFMediaType>& aMediaType) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  NS_ENSURE_TRUE(mDecoder, E_POINTER);
  return mDecoder->GetInputCurrentType(0, getter_AddRefs(aMediaType));
}

HRESULT
MFTDecoder::GetOutputMediaType(RefPtr<IMFMediaType>& aMediaType) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  NS_ENSURE_TRUE(mDecoder, E_POINTER);
  return mDecoder->GetOutputCurrentType(0, getter_AddRefs(aMediaType));
}

#undef LOG

}  // namespace mozilla
