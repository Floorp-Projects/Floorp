/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFMediaDataEncoder.h"

#include "AnnexB.h"
#include "H264.h"
#include "libyuv.h"
#include "mozilla/Logging.h"

#define WMF_ENC_LOGD(arg, ...)                    \
  MOZ_LOG(                                        \
      mozilla::sPEMLog, mozilla::LogLevel::Debug, \
      ("WMFMediaDataEncoder(0x%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define WMF_ENC_LOGE(arg, ...)                    \
  MOZ_LOG(                                        \
      mozilla::sPEMLog, mozilla::LogLevel::Error, \
      ("WMFMediaDataEncoder(0x%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {
extern LazyLogModule sPEMLog;

static const GUID CodecToSubtype(MediaDataEncoder::CodecType aCodec) {
  switch (aCodec) {
    case MediaDataEncoder::CodecType::H264:
      return MFVideoFormat_H264;
    default:
      MOZ_ASSERT(false, "Unsupported codec");
      return GUID_NULL;
  }
}

bool CanCreateWMFEncoder(MediaDataEncoder::CodecType aCodec) {
  bool canCreate = false;
  mscom::EnsureMTA([&]() {
    if (FAILED(wmf::MFStartup())) {
      return;
    }
    RefPtr<MFTEncoder> enc(new MFTEncoder());
    canCreate = SUCCEEDED(enc->Create(CodecToSubtype(aCodec)));
    wmf::MFShutdown();
  });
  return canCreate;
}

template <typename ConfigType>
WMFMediaDataEncoder<ConfigType>::WMFMediaDataEncoder(
    const ConfigType& aConfig, RefPtr<TaskQueue> aTaskQueue)
    : mConfig(aConfig), mTaskQueue(aTaskQueue) {
  MOZ_ASSERT(mTaskQueue);
}

template <typename T>
RefPtr<MediaDataEncoder::InitPromise> WMFMediaDataEncoder<T>::Init() {
  return InvokeAsync(mTaskQueue, this, __func__,
                     &WMFMediaDataEncoder<T>::ProcessInit);
}

template <typename T>
RefPtr<MediaDataEncoder::InitPromise> WMFMediaDataEncoder<T>::ProcessInit() {
  AssertOnTaskQueue();

  MOZ_ASSERT(!mEncoder,
             "Should not initialize encoder again without shutting down");

  if (FAILED(wmf::MFStartup())) {
    return InitPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Can't create the MFT encoder.")),
        __func__);
  }

  RefPtr<MFTEncoder> encoder = new MFTEncoder();
  HRESULT hr;
  mscom::EnsureMTA([&]() { hr = InitMFTEncoder(encoder); });

  if (FAILED(hr)) {
    wmf::MFShutdown();
    WMF_ENC_LOGE("init MFTEncoder: error = 0x%X", hr);
    return InitPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Can't create the MFT encoder.")),
        __func__);
  }

  mEncoder = std::move(encoder);
  return InitPromise::CreateAndResolve(TrackInfo::TrackType::kVideoTrack,
                                       __func__);
}

static already_AddRefed<MediaByteBuffer> ParseH264Parameters(
    nsTArray<uint8_t>& aHeader, const bool aAsAnnexB) {
  size_t length = aHeader.Length();
  auto annexB = MakeRefPtr<MediaByteBuffer>(length);
  PodCopy(annexB->Elements(), aHeader.Elements(), length);
  annexB->SetLength(length);
  if (aAsAnnexB) {
    return annexB.forget();
  }

  // Convert to avcC.
  nsTArray<AnnexB::NALEntry> paramSets;
  AnnexB::ParseNALEntries(
      Span<const uint8_t>(annexB->Elements(), annexB->Length()), paramSets);

  auto avcc = MakeRefPtr<MediaByteBuffer>();
  AnnexB::NALEntry& sps = paramSets.ElementAt(0);
  AnnexB::NALEntry& pps = paramSets.ElementAt(1);
  const uint8_t* spsPtr = annexB->Elements() + sps.mOffset;
  H264::WriteExtraData(
      avcc, spsPtr[1], spsPtr[2], spsPtr[3],
      Span<const uint8_t>(spsPtr, sps.mSize),
      Span<const uint8_t>(annexB->Elements() + pps.mOffset, pps.mSize));
  return avcc.forget();
}

static already_AddRefed<MediaByteBuffer> ExtractCodecConfigIfPresent(
    RefPtr<MFTEncoder>& aEncoder, const bool aAsAnnexB) {
  nsTArray<UINT8> header;
  NS_ENSURE_TRUE(SUCCEEDED(aEncoder->GetMPEGSequenceHeader(header)), nullptr);
  return header.Length() > 0 ? ParseH264Parameters(header, aAsAnnexB) : nullptr;
}

template <typename T>
HRESULT WMFMediaDataEncoder<T>::InitMFTEncoder(RefPtr<MFTEncoder>& aEncoder) {
  HRESULT hr = aEncoder->Create(CodecToSubtype(mConfig.mCodecType));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = SetMediaTypes(aEncoder, mConfig);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = aEncoder->SetModes(mConfig.mBitsPerSec);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  mConfigData =
      ExtractCodecConfigIfPresent(aEncoder, mConfig.mUsage == Usage::Realtime);

  return S_OK;
}

static uint32_t GetProfile(
    MediaDataEncoder::H264Specific::ProfileLevel aProfileLevel) {
  switch (aProfileLevel) {
    case MediaDataEncoder::H264Specific::ProfileLevel::BaselineAutoLevel:
      return eAVEncH264VProfile_Base;
    case MediaDataEncoder::H264Specific::ProfileLevel::MainAutoLevel:
      return eAVEncH264VProfile_Main;
    default:
      return eAVEncH264VProfile_unknown;
  }
}

template <typename Config>
HRESULT SetMediaTypes(RefPtr<MFTEncoder>& aEncoder, Config& aConfig) {
  RefPtr<IMFMediaType> inputType = CreateInputType(aConfig);
  if (!inputType) {
    return E_FAIL;
  }

  RefPtr<IMFMediaType> outputType = CreateOutputType(aConfig);
  if (!outputType) {
    return E_FAIL;
  }

  return aEncoder->SetMediaTypes(inputType, outputType);
}

template <typename Config>
already_AddRefed<IMFMediaType> CreateInputType(Config& aConfig) {
  RefPtr<IMFMediaType> type;
  return SUCCEEDED(wmf::MFCreateMediaType(getter_AddRefs(type))) &&
                 SUCCEEDED(
                     type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) &&
                 SUCCEEDED(type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12)) &&
                 SUCCEEDED(type->SetUINT32(MF_MT_INTERLACE_MODE,
                                           MFVideoInterlace_Progressive)) &&
                 SUCCEEDED(MFSetAttributeRatio(type, MF_MT_FRAME_RATE,
                                               aConfig.mFramerate, 1)) &&
                 SUCCEEDED(MFSetAttributeSize(type, MF_MT_FRAME_SIZE,
                                              aConfig.mSize.width,
                                              aConfig.mSize.height))
             ? type.forget()
             : nullptr;
}

static HRESULT SetCodecSpecific(
    IMFMediaType* aOutputType,
    const MediaDataEncoder::H264Specific& aSpecific) {
  return aOutputType->SetUINT32(MF_MT_MPEG2_PROFILE,
                                GetProfile(aSpecific.mProfileLevel));
}

template <typename Config>
already_AddRefed<IMFMediaType> CreateOutputType(Config& aConfig) {
  RefPtr<IMFMediaType> type;
  if (FAILED(wmf::MFCreateMediaType(getter_AddRefs(type))) ||
      FAILED(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) ||
      FAILED(type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264)) ||
      FAILED(type->SetUINT32(MF_MT_AVG_BITRATE, aConfig.mBitsPerSec)) ||
      FAILED(type->SetUINT32(MF_MT_INTERLACE_MODE,
                             MFVideoInterlace_Progressive)) ||
      FAILED(
          MFSetAttributeRatio(type, MF_MT_FRAME_RATE, aConfig.mFramerate, 1)) ||
      FAILED(MFSetAttributeSize(type, MF_MT_FRAME_SIZE, aConfig.mSize.width,
                                aConfig.mSize.height))) {
    return nullptr;
  }
  if (aConfig.mCodecSpecific &&
      FAILED(SetCodecSpecific(type, aConfig.mCodecSpecific.ref()))) {
    return nullptr;
  }

  return type.forget();
}

template <typename T>
RefPtr<MediaDataEncoder::EncodePromise> WMFMediaDataEncoder<T>::Encode(
    const MediaData* aSample) {
  MOZ_ASSERT(aSample);

  RefPtr<const VideoData> sample(aSample->As<const VideoData>());

  return InvokeAsync<RefPtr<const VideoData>>(
      mTaskQueue, this, __func__, &WMFMediaDataEncoder::ProcessEncode,
      std::move(sample));
}

template <typename T>
RefPtr<MediaDataEncoder::EncodePromise> WMFMediaDataEncoder<T>::ProcessEncode(
    RefPtr<const VideoData>&& aSample) {
  AssertOnTaskQueue();
  MOZ_ASSERT(mEncoder);
  MOZ_ASSERT(aSample);

  RefPtr<IMFSample> nv12 = ConvertToNV12InputSample(std::move(aSample));
  if (!nv12 || FAILED(mEncoder->PushInput(std::move(nv12)))) {
    WMF_ENC_LOGE("failed to process input sample");
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Failed to process input.")),
        __func__);
  }

  nsTArray<RefPtr<IMFSample>> outputs;
  HRESULT hr = mEncoder->TakeOutput(outputs);
  if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
    mConfigData = ExtractCodecConfigIfPresent(
        mEncoder, mConfig.mUsage == Usage::Realtime);
  } else if (FAILED(hr)) {
    WMF_ENC_LOGE("failed to process output");
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Failed to process output.")),
        __func__);
  }

  return ProcessOutputSamples(outputs);
}
template <typename T>
already_AddRefed<IMFSample> WMFMediaDataEncoder<T>::ConvertToNV12InputSample(
    RefPtr<const VideoData>&& aData) {
  AssertOnTaskQueue();
  MOZ_ASSERT(mEncoder);

  const PlanarYCbCrImage* image = aData->mImage->AsPlanarYCbCrImage();
  MOZ_ASSERT(image);
  const PlanarYCbCrData* yuv = image->GetData();
  size_t yLength = yuv->mYStride * yuv->mYSize.height;
  size_t length = yLength + (yuv->mCbCrStride * yuv->mCbCrSize.height * 2);

  RefPtr<IMFSample> input;
  HRESULT hr = mEncoder->CreateInputSample(&input, length);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  RefPtr<IMFMediaBuffer> buffer;
  hr = input->GetBufferByIndex(0, getter_AddRefs(buffer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  hr = buffer->SetCurrentLength(length);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  LockBuffer lockBuffer(buffer);
  NS_ENSURE_TRUE(SUCCEEDED(lockBuffer.Result()), nullptr);

  bool ok =
      libyuv::I420ToNV12(yuv->mYChannel, yuv->mYStride, yuv->mCbChannel,
                         yuv->mCbCrStride, yuv->mCrChannel, yuv->mCbCrStride,
                         lockBuffer.Data(), yuv->mYStride,
                         lockBuffer.Data() + yLength, yuv->mCbCrStride * 2,
                         yuv->mYSize.width, yuv->mYSize.height) == 0;
  NS_ENSURE_TRUE(ok, nullptr);

  hr = input->SetSampleTime(UsecsToHNs(aData->mTime.ToMicroseconds()));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  hr = input->SetSampleDuration(UsecsToHNs(aData->mDuration.ToMicroseconds()));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  return input.forget();
}

template <typename T>
RefPtr<MediaDataEncoder::EncodePromise>
WMFMediaDataEncoder<T>::ProcessOutputSamples(
    nsTArray<RefPtr<IMFSample>>& aSamples) {
  EncodedData frames;
  for (auto sample : aSamples) {
    RefPtr<MediaRawData> frame = IMFSampleToMediaData(sample);
    if (frame) {
      frames.AppendElement(std::move(frame));
    } else {
      WMF_ENC_LOGE("failed to convert output frame");
    }
  }
  aSamples.Clear();

  return EncodePromise::CreateAndResolve(std::move(frames), __func__);
}

template <typename T>
already_AddRefed<MediaRawData> WMFMediaDataEncoder<T>::IMFSampleToMediaData(
    RefPtr<IMFSample>& aSample) {
  AssertOnTaskQueue();
  MOZ_ASSERT(aSample);

  RefPtr<IMFMediaBuffer> buffer;
  HRESULT hr = aSample->GetBufferByIndex(0, getter_AddRefs(buffer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  LockBuffer lockBuffer(buffer);
  NS_ENSURE_TRUE(SUCCEEDED(lockBuffer.Result()), nullptr);

  LONGLONG time = 0;
  hr = aSample->GetSampleTime(&time);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  LONGLONG duration = 0;
  hr = aSample->GetSampleDuration(&duration);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  bool isKeyframe =
      MFGetAttributeUINT32(aSample, MFSampleExtension_CleanPoint, false);

  auto frame = MakeRefPtr<MediaRawData>();
  if (!WriteFrameData(frame, lockBuffer, isKeyframe)) {
    return nullptr;
  }

  frame->mTime = media::TimeUnit::FromMicroseconds(HNsToUsecs(time));
  frame->mDuration = media::TimeUnit::FromMicroseconds(HNsToUsecs(duration));
  frame->mKeyframe = isKeyframe;

  return frame.forget();
}

// Prepend SPS/PPS to keyframe in realtime mode, which expects AnnexB bitstream.
// Convert to AVCC otherwise.
template <>
bool WMFMediaDataEncoder<MediaDataEncoder::H264Config>::WriteFrameData(
    RefPtr<MediaRawData>& aDest, LockBuffer& aSrc, bool aIsKeyframe) {
  size_t prependLength = 0;
  RefPtr<MediaByteBuffer> avccHeader;
  if (aIsKeyframe && mConfigData) {
    if (mConfig.mUsage == Usage::Realtime) {
      prependLength = mConfigData->Length();
    } else {
      avccHeader = mConfigData;
    }
  }

  UniquePtr<MediaRawDataWriter> writer(aDest->CreateWriter());
  if (!writer->SetSize(prependLength + aSrc.Length())) {
    WMF_ENC_LOGE("fail to allocate output buffer");
    return false;
  }

  if (prependLength > 0) {
    PodCopy(writer->Data(), mConfigData->Elements(), prependLength);
  }
  PodCopy(writer->Data() + prependLength, aSrc.Data(), aSrc.Length());

  if (mConfig.mUsage != Usage::Realtime &&
      !AnnexB::ConvertSampleToAVCC(aDest, avccHeader)) {
    WMF_ENC_LOGE("fail to convert annex-b sample to AVCC");
    return false;
  }

  return true;
}

template <typename T>
bool WMFMediaDataEncoder<T>::WriteFrameData(RefPtr<MediaRawData>& aDest,
                                            LockBuffer& aSrc,
                                            bool aIsKeyframe) {
  Unused << aIsKeyframe;

  UniquePtr<MediaRawDataWriter> writer(aDest->CreateWriter());
  if (!writer->SetSize(aSrc.Length())) {
    WMF_ENC_LOGE("fail to allocate output buffer");
    return false;
  }

  PodCopy(writer->Data(), aSrc.Data(), aSrc.Length());
  return true;
}

template <typename T>
RefPtr<MediaDataEncoder::EncodePromise> WMFMediaDataEncoder<T>::Drain() {
  return InvokeAsync(mTaskQueue, __func__,
                     [self = RefPtr<WMFMediaDataEncoder>(this)]() {
                       nsTArray<RefPtr<IMFSample>> outputs;
                       return SUCCEEDED(self->mEncoder->Drain(outputs))
                                  ? self->ProcessOutputSamples(outputs)
                                  : EncodePromise::CreateAndReject(
                                        NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
                     });
}

template <typename T>
RefPtr<ShutdownPromise> WMFMediaDataEncoder<T>::Shutdown() {
  return InvokeAsync(mTaskQueue, __func__,
                     [self = RefPtr<WMFMediaDataEncoder>(this)]() {
                       if (self->mEncoder) {
                         self->mEncoder->Destroy();
                         self->mEncoder = nullptr;
                         wmf::MFShutdown();
                       }
                       return ShutdownPromise::CreateAndResolve(true, __func__);
                     });
}

template <typename T>
RefPtr<GenericPromise> WMFMediaDataEncoder<T>::SetBitrate(
    MediaDataEncoder::Rate aBitsPerSec) {
  return InvokeAsync(
      mTaskQueue, __func__,
      [self = RefPtr<WMFMediaDataEncoder>(this), aBitsPerSec]() {
        MOZ_ASSERT(self->mEncoder);
        return SUCCEEDED(self->mEncoder->SetBitrate(aBitsPerSec))
                   ? GenericPromise::CreateAndResolve(true, __func__)
                   : GenericPromise::CreateAndReject(
                         NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR, __func__);
      });
}

template <>
nsCString
WMFMediaDataEncoder<MediaDataEncoder::H264Config>::GetDescriptionName() const {
  MOZ_ASSERT(mConfig.mCodecType == CodecType::H264);
  return MFTEncoder::GetFriendlyName(MFVideoFormat_H264);
}

}  // namespace mozilla

#undef WMF_ENC_LOGE
#undef WMF_ENC_LOGD
