/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFTEncoder.h"
#include "mozilla/Logging.h"
#include "mozilla/mscom/Utils.h"

// Missing from MinGW.
#ifndef CODECAPI_AVEncAdaptiveMode
#  define STATIC_CODECAPI_AVEncAdaptiveMode \
    0x4419b185, 0xda1f, 0x4f53, 0xbc, 0x76, 0x9, 0x7d, 0xc, 0x1e, 0xfb, 0x1e
DEFINE_CODECAPI_GUID(AVEncAdaptiveMode, "4419b185-da1f-4f53-bc76-097d0c1efb1e",
                     0x4419b185, 0xda1f, 0x4f53, 0xbc, 0x76, 0x9, 0x7d, 0xc,
                     0x1e, 0xfb, 0x1e)
#  define CODECAPI_AVEncAdaptiveMode \
    DEFINE_CODECAPI_GUIDNAMED(AVEncAdaptiveMode)
#endif
#ifndef MF_E_NO_EVENTS_AVAILABLE
#  define MF_E_NO_EVENTS_AVAILABLE _HRESULT_TYPEDEF_(0xC00D3E80L)
#endif

#define MFT_ENC_LOGD(arg, ...)                        \
  MOZ_LOG(mozilla::sPEMLog, mozilla::LogLevel::Debug, \
          ("MFTEncoder(0x%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define MFT_ENC_LOGE(arg, ...)                        \
  MOZ_LOG(mozilla::sPEMLog, mozilla::LogLevel::Error, \
          ("MFTEncoder(0x%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define MFT_ENC_SLOGD(arg, ...)                       \
  MOZ_LOG(mozilla::sPEMLog, mozilla::LogLevel::Debug, \
          ("MFTEncoder::%s: " arg, __func__, ##__VA_ARGS__))
#define MFT_ENC_SLOGE(arg, ...)                       \
  MOZ_LOG(mozilla::sPEMLog, mozilla::LogLevel::Error, \
          ("MFTEncoder::%s: " arg, __func__, ##__VA_ARGS__))

namespace mozilla {
extern LazyLogModule sPEMLog;

static const char* ErrorStr(HRESULT hr) {
  switch (hr) {
    case S_OK:
      return "OK";
    case MF_E_INVALIDMEDIATYPE:
      return "INVALIDMEDIATYPE";
    case MF_E_INVALIDSTREAMNUMBER:
      return "INVALIDSTREAMNUMBER";
    case MF_E_INVALIDTYPE:
      return "INVALIDTYPE";
    case MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING:
      return "TRANSFORM_PROCESSING";
    case MF_E_TRANSFORM_TYPE_NOT_SET:
      return "TRANSFORM_TYPE_NO_SET";
    case MF_E_UNSUPPORTED_D3D_TYPE:
      return "UNSUPPORTED_D3D_TYPE";
    case E_INVALIDARG:
      return "INVALIDARG";
    case MF_E_NO_SAMPLE_DURATION:
      return "NO_SAMPLE_DURATION";
    case MF_E_NO_SAMPLE_TIMESTAMP:
      return "NO_SAMPLE_TIMESTAMP";
    case MF_E_NOTACCEPTING:
      return "NOTACCEPTING";
    case MF_E_ATTRIBUTENOTFOUND:
      return "NOTFOUND";
    case MF_E_BUFFERTOOSMALL:
      return "BUFFERTOOSMALL";
    case E_NOTIMPL:
      return "NOTIMPL";
    default:
      return "OTHER";
  }
}

static const char* CodecStr(const GUID& aGUID) {
  if (IsEqualGUID(aGUID, MFVideoFormat_H264)) {
    return "H.264";
  } else if (IsEqualGUID(aGUID, MFVideoFormat_VP80)) {
    return "VP8";
  } else if (IsEqualGUID(aGUID, MFVideoFormat_VP90)) {
    return "VP9";
  } else {
    return "Unsupported codec";
  }
}

static UINT32 EnumHW(const GUID& aSubtype, IMFActivate**& aActivates) {
  UINT32 num = 0;
  MFT_REGISTER_TYPE_INFO inType = {.guidMajorType = MFMediaType_Video,
                                   .guidSubtype = MFVideoFormat_NV12};
  MFT_REGISTER_TYPE_INFO outType = {.guidMajorType = MFMediaType_Video,
                                    .guidSubtype = aSubtype};
  HRESULT hr =
      wmf::MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER,
                     MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER,
                     &inType, &outType, &aActivates, &num);
  if (FAILED(hr)) {
    MFT_ENC_SLOGE("enumerate HW encoder for %s: error=%s", CodecStr(aSubtype),
                  ErrorStr(hr));
    return 0;
  }
  if (num == 0) {
    MFT_ENC_SLOGD("cannot find HW encoder for %s", CodecStr(aSubtype));
  }
  return num;
}

static HRESULT GetFriendlyName(IMFActivate* aAttributes, nsCString& aName) {
  UINT32 len = 0;
  HRESULT hr = aAttributes->GetStringLength(MFT_FRIENDLY_NAME_Attribute, &len);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  if (len > 0) {
    ++len;  // '\0'.
    WCHAR name[len];
    if (SUCCEEDED(aAttributes->GetString(MFT_FRIENDLY_NAME_Attribute, name, len,
                                         nullptr))) {
      aName.Append(NS_ConvertUTF16toUTF8(name));
    }
  }

  if (aName.Length() == 0) {
    aName.Append("Unknown MFT");
  }

  return S_OK;
}

static void PopulateHWEncoderInfo(const GUID& aSubtype,
                                  nsTArray<MFTEncoder::Info>& aInfos) {
  IMFActivate** activates = nullptr;
  UINT32 num = EnumHW(aSubtype, activates);
  for (UINT32 i = 0; i < num; ++i) {
    MFTEncoder::Info info = {.mSubtype = aSubtype};
    GetFriendlyName(activates[i], info.mName);
    aInfos.AppendElement(info);
    MFT_ENC_SLOGD("<ENC> [%s] %s\n", CodecStr(aSubtype), info.mName.Data());
    activates[i]->Release();
    activates[i] = nullptr;
  }
  CoTaskMemFree(activates);
}

Maybe<MFTEncoder::Info> MFTEncoder::GetInfo(const GUID& aSubtype) {
  nsTArray<Info>& infos = Infos();

  for (auto i : infos) {
    if (IsEqualGUID(aSubtype, i.mSubtype)) {
      return Some(i);
    }
  }
  return Nothing();
}

nsCString MFTEncoder::GetFriendlyName(const GUID& aSubtype) {
  Maybe<Info> info = GetInfo(aSubtype);

  return info ? info.ref().mName : "???"_ns;
}

// Called only once by Infos().
nsTArray<MFTEncoder::Info> MFTEncoder::Enumerate() {
  nsTArray<Info> infos;

  if (FAILED(wmf::MFStartup())) {
    MFT_ENC_SLOGE("cannot init Media Foundation");
    return infos;
  }

  PopulateHWEncoderInfo(MFVideoFormat_H264, infos);
  PopulateHWEncoderInfo(MFVideoFormat_VP90, infos);
  PopulateHWEncoderInfo(MFVideoFormat_VP80, infos);

  wmf::MFShutdown();
  return infos;
}

nsTArray<MFTEncoder::Info>& MFTEncoder::Infos() {
  static nsTArray<Info> infos = Enumerate();
  return infos;
}

already_AddRefed<IMFActivate> MFTEncoder::CreateFactory(const GUID& aSubtype) {
  Maybe<Info> info = GetInfo(aSubtype);
  if (!info) {
    return nullptr;
  }

  IMFActivate** activates = nullptr;
  UINT32 num = EnumHW(aSubtype, activates);
  if (num == 0) {
    return nullptr;
  }

  // Keep the first and throw out others, if there is any.
  RefPtr<IMFActivate> factory = activates[0];
  activates[0] = nullptr;
  for (UINT32 i = 1; i < num; ++i) {
    activates[i]->Release();
    activates[i] = nullptr;
  }
  CoTaskMemFree(activates);

  return factory.forget();
}

HRESULT MFTEncoder::Create(const GUID& aSubtype) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(!mEncoder);

  RefPtr<IMFActivate> factory = CreateFactory(aSubtype);
  if (!factory) {
    return E_FAIL;
  }

  // Create MFT via the activation object.
  RefPtr<IMFTransform> encoder;
  HRESULT hr = factory->ActivateObject(
      IID_PPV_ARGS(static_cast<IMFTransform**>(getter_AddRefs(encoder))));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<ICodecAPI> config;
  // Avoid IID_PPV_ARGS() here for MingGW fails to declare UUID for ICodecAPI.
  hr = encoder->QueryInterface(IID_ICodecAPI, getter_AddRefs(config));
  if (FAILED(hr)) {
    encoder = nullptr;
    factory->ShutdownObject();
    return hr;
  }

  mFactory = std::move(factory);
  mEncoder = std::move(encoder);
  mConfig = std::move(config);
  return S_OK;
}

HRESULT
MFTEncoder::Destroy() {
  if (!mEncoder) {
    return S_OK;
  }

  mEventSource = nullptr;
  mEncoder = nullptr;
  mConfig = nullptr;
  // Release MFT resources via activation object.
  HRESULT hr = mFactory->ShutdownObject();
  mFactory = nullptr;

  return hr;
}

HRESULT
MFTEncoder::SetMediaTypes(IMFMediaType* aInputType, IMFMediaType* aOutputType) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(aInputType && aOutputType);

  HRESULT hr = EnableAsync();
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = GetStreamIDs();
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Always set encoder output type before input.
  hr = mEncoder->SetOutputType(mOutputStreamID, aOutputType, 0);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  NS_ENSURE_TRUE(MatchInputSubtype(aInputType) != GUID_NULL,
                 MF_E_INVALIDMEDIATYPE);

  hr = mEncoder->SetInputType(mInputStreamID, aInputType, 0);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = mEncoder->GetInputStreamInfo(mInputStreamID, &mInputStreamInfo);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = mEncoder->GetOutputStreamInfo(mInputStreamID, &mOutputStreamInfo);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  mOutputStreamProvidesSample =
      IsFlagSet(mOutputStreamInfo.dwFlags, MFT_OUTPUT_STREAM_PROVIDES_SAMPLES);

  hr = SendMFTMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = SendMFTMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = SendMFTMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IMFMediaEventGenerator> source;
  hr = mEncoder->QueryInterface(IID_PPV_ARGS(
      static_cast<IMFMediaEventGenerator**>(getter_AddRefs(source))));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  mEventSource = std::move(source);
  mNumNeedInput = 0;
  return S_OK;
}

// Async MFT won't work without unlocking. See
// https://docs.microsoft.com/en-us/windows/win32/medfound/asynchronous-mfts#unlocking-asynchronous-mfts
HRESULT MFTEncoder::EnableAsync() {
  IMFAttributes* pAttributes = nullptr;
  HRESULT hr = mEncoder->GetAttributes(&pAttributes);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  UINT32 async = MFGetAttributeUINT32(pAttributes, MF_TRANSFORM_ASYNC, FALSE);
  if (async == TRUE) {
    hr = pAttributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
  } else {
    hr = E_NOTIMPL;
  }
  pAttributes->Release();

  return hr;
}

HRESULT MFTEncoder::GetStreamIDs() {
  DWORD numIns;
  DWORD numOuts;
  HRESULT hr = mEncoder->GetStreamCount(&numIns, &numOuts);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  if (numIns < 1 || numOuts < 1) {
    MFT_ENC_LOGE("stream count error");
    return MF_E_INVALIDSTREAMNUMBER;
  }

  DWORD inIDs[numIns];
  DWORD outIDs[numOuts];
  hr = mEncoder->GetStreamIDs(numIns, inIDs, numOuts, outIDs);
  if (SUCCEEDED(hr)) {
    mInputStreamID = inIDs[0];
    mOutputStreamID = outIDs[0];
  } else if (hr == E_NOTIMPL) {
    mInputStreamID = 0;
    mOutputStreamID = 0;
  } else {
    MFT_ENC_LOGE("failed to get stream IDs");
    return hr;
  }
  return S_OK;
}

GUID MFTEncoder::MatchInputSubtype(IMFMediaType* aInputType) {
  MOZ_ASSERT(mEncoder);
  MOZ_ASSERT(aInputType);

  GUID desired = GUID_NULL;
  HRESULT hr = aInputType->GetGUID(MF_MT_SUBTYPE, &desired);
  NS_ENSURE_TRUE(SUCCEEDED(hr), GUID_NULL);
  MOZ_ASSERT(desired != GUID_NULL);

  DWORD i = 0;
  IMFMediaType* inputType = nullptr;
  GUID preferred = GUID_NULL;
  while (true) {
    hr = mEncoder->GetInputAvailableType(mInputStreamID, i, &inputType);
    if (hr == MF_E_NO_MORE_TYPES) {
      break;
    }
    NS_ENSURE_TRUE(SUCCEEDED(hr), GUID_NULL);

    GUID sub = GUID_NULL;
    hr = inputType->GetGUID(MF_MT_SUBTYPE, &sub);
    NS_ENSURE_TRUE(SUCCEEDED(hr), GUID_NULL);

    if (IsEqualGUID(desired, sub)) {
      preferred = desired;
      break;
    }
    ++i;
  }

  return IsEqualGUID(preferred, desired) ? preferred : GUID_NULL;
}

HRESULT
MFTEncoder::SendMFTMessage(MFT_MESSAGE_TYPE aMsg, ULONG_PTR aData) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mEncoder);

  return mEncoder->ProcessMessage(aMsg, aData);
}

HRESULT MFTEncoder::SetModes(UINT32 aBitsPerSec) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mConfig);

  VARIANT var;
  var.vt = VT_UI4;
  var.ulVal = eAVEncCommonRateControlMode_CBR;
  HRESULT hr = mConfig->SetValue(&CODECAPI_AVEncCommonRateControlMode, &var);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  var.ulVal = aBitsPerSec;
  hr = mConfig->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &var);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  if (SUCCEEDED(mConfig->IsModifiable(&CODECAPI_AVEncAdaptiveMode))) {
    var.ulVal = eAVEncAdaptiveMode_Resolution;
    hr = mConfig->SetValue(&CODECAPI_AVEncAdaptiveMode, &var);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  }

  if (SUCCEEDED(mConfig->IsModifiable(&CODECAPI_AVLowLatencyMode))) {
    var.vt = VT_BOOL;
    var.boolVal = VARIANT_TRUE;
    hr = mConfig->SetValue(&CODECAPI_AVLowLatencyMode, &var);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  }

  return S_OK;
}

HRESULT
MFTEncoder::SetBitrate(UINT32 aBitsPerSec) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mConfig);

  VARIANT var = {.vt = VT_UI4, .ulVal = aBitsPerSec};
  return mConfig->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &var);
}

static HRESULT CreateSample(RefPtr<IMFSample>* aOutSample, DWORD aSize,
                            DWORD aAlignment) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  HRESULT hr;
  RefPtr<IMFSample> sample;
  hr = wmf::MFCreateSample(getter_AddRefs(sample));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IMFMediaBuffer> buffer;
  hr = wmf::MFCreateAlignedMemoryBuffer(aSize, aAlignment,
                                        getter_AddRefs(buffer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = sample->AddBuffer(buffer);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  *aOutSample = sample.forget();

  return S_OK;
}

HRESULT
MFTEncoder::CreateInputSample(RefPtr<IMFSample>* aSample, size_t aSize) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  return CreateSample(
      aSample, aSize,
      mInputStreamInfo.cbAlignment > 0 ? mInputStreamInfo.cbAlignment - 1 : 0);
}

HRESULT
MFTEncoder::PushInput(RefPtr<IMFSample>&& aInput) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mEncoder);
  MOZ_ASSERT(aInput);

  mPendingInputs.Push(aInput.forget());
  HRESULT hr = ProcessInput();
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  return ProcessEvents();
}

HRESULT MFTEncoder::ProcessInput() {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mEncoder);

  if (mNumNeedInput == 0 || mPendingInputs.GetSize() == 0) {
    return S_OK;
  }

  RefPtr<IMFSample> input = mPendingInputs.PopFront();
  HRESULT hr = mEncoder->ProcessInput(mInputStreamID, input, 0);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  --mNumNeedInput;

  return S_OK;
}

HRESULT MFTEncoder::ProcessEvents() {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mEncoder);
  MOZ_ASSERT(mEventSource, "no event generator");

  HRESULT hr = E_FAIL;
  while (true) {
    RefPtr<IMFMediaEvent> event;
    hr = mEventSource->GetEvent(MF_EVENT_FLAG_NO_WAIT, getter_AddRefs(event));
    switch (hr) {
      case S_OK:
        break;
      case MF_E_NO_EVENTS_AVAILABLE:
        return S_OK;
      case MF_E_MULTIPLE_SUBSCRIBERS:
      default:
        MFT_ENC_LOGE("failed to get event: %s", ErrorStr(hr));
        return hr;
    }

    MediaEventType type = MEUnknown;
    HRESULT hr = event->GetType(&type);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
    switch (type) {
      case METransformNeedInput:
        ++mNumNeedInput;
        hr = ProcessInput();
        NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
        break;
      case METransformHaveOutput:
        hr = ProcessOutput();
        break;
      case METransformDrainComplete:
        mDrainState = DrainState::DRAINED;
        break;
      default:
        MFT_ENC_LOGE("event: error=%s", ErrorStr(hr));
    }
  }

  return hr;
}

HRESULT MFTEncoder::ProcessOutput() {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mEncoder);

  MFT_OUTPUT_DATA_BUFFER output = {.dwStreamID = mOutputStreamID,
                                   .pSample = nullptr,
                                   .dwStatus = 0,
                                   .pEvents = nullptr};
  RefPtr<IMFSample> sample;
  HRESULT hr = E_FAIL;
  if (!mOutputStreamProvidesSample) {
    hr = CreateSample(&sample, mOutputStreamInfo.cbSize,
                      mOutputStreamInfo.cbAlignment > 1
                          ? mOutputStreamInfo.cbAlignment - 1
                          : 0);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
    output.pSample = sample;
  }

  DWORD status = 0;
  hr = mEncoder->ProcessOutput(0, 1, &output, &status);
  if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
    MFT_ENC_LOGD("output stream change");
    if (output.dwStatus & MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE) {
      // Follow the instructions in Microsoft doc:
      // https://docs.microsoft.com/en-us/windows/win32/medfound/handling-stream-changes#output-type
      IMFMediaType* outputType = nullptr;
      hr = mEncoder->GetOutputAvailableType(mOutputStreamID, 0, &outputType);
      NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
      hr = mEncoder->SetOutputType(mOutputStreamID, outputType, 0);
      NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
    }
    return MF_E_TRANSFORM_STREAM_CHANGE;
  }
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  mOutputs.AppendElement(output.pSample);
  if (mOutputStreamProvidesSample) {
    // Release MFT provided sample.
    output.pSample->Release();
    output.pSample = nullptr;
  }

  return S_OK;
}

HRESULT MFTEncoder::TakeOutput(nsTArray<RefPtr<IMFSample>>& aOutput) {
  MOZ_ASSERT(aOutput.Length() == 0);
  aOutput.SwapElements(mOutputs);
  return S_OK;
}

HRESULT MFTEncoder::Drain(nsTArray<RefPtr<IMFSample>>& aOutput) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mEncoder);
  MOZ_ASSERT(aOutput.Length() == 0);

  switch (mDrainState) {
    case DrainState::DRAINABLE:
      // Exhaust pending inputs.
      while (mPendingInputs.GetSize() > 0) {
        HRESULT hr = ProcessEvents();
        NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
      }
      SendMFTMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
      mDrainState = DrainState::DRAINING;
      [[fallthrough]];  // To collect and return outputs.
    case DrainState::DRAINING:
      // Collect remaining outputs.
      while (mOutputs.Length() == 0 && mDrainState != DrainState::DRAINED) {
        HRESULT hr = ProcessEvents();
        NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
      }
      [[fallthrough]];  // To return outputs.
    case DrainState::DRAINED:
      aOutput.SwapElements(mOutputs);
      return S_OK;
  }
}

HRESULT MFTEncoder::GetMPEGSequenceHeader(nsTArray<UINT8>& aHeader) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mEncoder);
  MOZ_ASSERT(aHeader.Length() == 0);

  RefPtr<IMFMediaType> outputType;
  HRESULT hr = mEncoder->GetOutputCurrentType(mOutputStreamID,
                                              getter_AddRefs(outputType));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  UINT32 length = 0;
  hr = outputType->GetBlobSize(MF_MT_MPEG_SEQUENCE_HEADER, &length);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  aHeader.SetCapacity(length);
  hr = outputType->GetBlob(MF_MT_MPEG_SEQUENCE_HEADER, aHeader.Elements(),
                           length, nullptr);
  aHeader.SetLength(SUCCEEDED(hr) ? length : 0);

  return hr;
}

}  // namespace mozilla

#undef MFT_ENC_SLOGE
#undef MFT_ENC_SLOGD
#undef MFT_ENC_LOGE
#undef MFT_ENC_LOGD
