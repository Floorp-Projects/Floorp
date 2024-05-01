/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFDataEncoderUtils.h"

#include "EncoderConfig.h"
#include "MFTEncoder.h"
#include "MediaData.h"
#include "mozilla/Logging.h"

namespace mozilla {

#define WMF_ENC_LOG(arg, ...)                         \
  MOZ_LOG(mozilla::sPEMLog, mozilla::LogLevel::Error, \
          ("WMFDataEncoderUtils::%s: " arg, __func__, ##__VA_ARGS__))

GUID CodecToSubtype(CodecType aCodec) {
  switch (aCodec) {
    case CodecType::H264:
      return MFVideoFormat_H264;
    case CodecType::VP8:
      return MFVideoFormat_VP80;
    case CodecType::VP9:
      return MFVideoFormat_VP90;
    default:
      return GUID_NULL;
  }
}

bool CanCreateWMFEncoder(CodecType aCodec) {
  bool canCreate = false;
  mscom::EnsureMTA([&]() {
    if (!wmf::MediaFoundationInitializer::HasInitialized()) {
      return;
    }
    // Try HW encoder first.
    auto enc = MakeRefPtr<MFTEncoder>(false /* HW not allowed */);
    canCreate = SUCCEEDED(enc->Create(CodecToSubtype(aCodec)));
    if (!canCreate) {
      // Try SW encoder.
      enc = MakeRefPtr<MFTEncoder>(true /* HW not allowed */);
      canCreate = SUCCEEDED(enc->Create(CodecToSubtype(aCodec)));
    }
  });
  return canCreate;
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

static uint32_t GetProfile(H264_PROFILE aProfileLevel) {
  switch (aProfileLevel) {
    case H264_PROFILE_BASE:
      return eAVEncH264VProfile_Base;
    case H264_PROFILE_MAIN:
      return eAVEncH264VProfile_Main;
    case H264_PROFILE_HIGH:
      return eAVEncH264VProfile_High;
    default:
      return eAVEncH264VProfile_unknown;
  }
}

already_AddRefed<IMFMediaType> CreateInputType(EncoderConfig& aConfig) {
  RefPtr<IMFMediaType> type;
  HRESULT hr = wmf::MFCreateMediaType(getter_AddRefs(type));
  if (FAILED(hr)) {
    WMF_ENC_LOG("MFCreateMediaType (input) error: %lx", hr);
    return nullptr;
  }
  hr = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  if (FAILED(hr)) {
    WMF_ENC_LOG("Create input type: SetGUID (major type) error: %lx", hr);
    return nullptr;
  }
  // Always NV12 input
  hr = type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
  if (FAILED(hr)) {
    WMF_ENC_LOG("Create input type: SetGUID (subtype) error: %lx", hr);
    return nullptr;
  }
  hr = type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
  if (FAILED(hr)) {
    WMF_ENC_LOG("Create input type: interlace mode (input) error: %lx", hr);
    return nullptr;
  }
  // WMF requires a framerate to intialize properly. Provide something
  // reasonnable if not provided.
  if (!aConfig.mFramerate) {
    aConfig.mFramerate = 30;
  }
  if (aConfig.mFramerate) {
    hr = MFSetAttributeRatio(type, MF_MT_FRAME_RATE, aConfig.mFramerate, 1);
    if (FAILED(hr)) {
      WMF_ENC_LOG("Create input type: frame rate (input) error: %lx", hr);
      return nullptr;
    }
  }
  hr = MFSetAttributeSize(type, MF_MT_FRAME_SIZE, aConfig.mSize.width,
                          aConfig.mSize.height);
  if (FAILED(hr)) {
    WMF_ENC_LOG("Create input type: frame size (input) error: %lx", hr);
    return nullptr;
  }
  return type.forget();
}

already_AddRefed<IMFMediaType> CreateOutputType(EncoderConfig& aConfig) {
  RefPtr<IMFMediaType> type;
  HRESULT hr = wmf::MFCreateMediaType(getter_AddRefs(type));
  if (FAILED(hr)) {
    WMF_ENC_LOG("MFCreateMediaType (output) error: %lx", hr);
    return nullptr;
  }
  hr = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  if (FAILED(hr)) {
    WMF_ENC_LOG("Create output type: set major type error: %lx", hr);
    return nullptr;
  }
  hr = type->SetGUID(MF_MT_SUBTYPE, CodecToSubtype(aConfig.mCodec));
  if (FAILED(hr)) {
    WMF_ENC_LOG("Create output type: set subtype error: %lx", hr);
    return nullptr;
  }
  // A bitrate need to be set here, attempt to make an educated guess if none is
  // provided. This could be per codec to have nicer defaults.
  size_t longDimension = std::max(aConfig.mSize.width, aConfig.mSize.height);
  if (!aConfig.mBitrate) {
    if (longDimension < 720) {
      aConfig.mBitrate = 2000000;
    } else if (longDimension < 1080) {
      aConfig.mBitrate = 4000000;
    } else {
      aConfig.mBitrate = 8000000;
    }
  }
  // No way to set variable / constant here.
  hr = type->SetUINT32(MF_MT_AVG_BITRATE, aConfig.mBitrate);
  if (FAILED(hr)) {
    WMF_ENC_LOG("Create output type: set bitrate error: %lx", hr);
    return nullptr;
  }
  hr = type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
  if (FAILED(hr)) {
    WMF_ENC_LOG("Create output type set interlave mode error: %lx", hr);
    return nullptr;
  }
  // A positive rate must always be preset here, see the Input config part.
  MOZ_ASSERT(aConfig.mFramerate);
  if (aConfig.mFramerate) {
    hr = MFSetAttributeRatio(type, MF_MT_FRAME_RATE, aConfig.mFramerate, 1);
    if (FAILED(hr)) {
      WMF_ENC_LOG("Create output type set frame rate error: %lx", hr);
      return nullptr;
    }
  }
  // Required
  hr = MFSetAttributeSize(type, MF_MT_FRAME_SIZE, aConfig.mSize.width,
                          aConfig.mSize.height);
  if (FAILED(hr)) {
    WMF_ENC_LOG("Create output type set frame size error: %lx", hr);
    return nullptr;
  }

  if (aConfig.mCodecSpecific) {
    if (aConfig.mCodecSpecific->is<H264Specific>()) {
      MOZ_ASSERT(aConfig.mCodec == CodecType::H264);
      hr = FAILED(type->SetUINT32(
          MF_MT_MPEG2_PROFILE,
          GetProfile(aConfig.mCodecSpecific->as<H264Specific>().mProfile)));
      if (hr) {
        WMF_ENC_LOG("Create output type set profile error: %lx", hr);
        return nullptr;
      }
    }
  }

  return type.forget();
}

HRESULT SetMediaTypes(RefPtr<MFTEncoder>& aEncoder, EncoderConfig& aConfig) {
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

}  // namespace mozilla
