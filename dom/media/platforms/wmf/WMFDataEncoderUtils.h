/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFMediaDataEncoder.h"

#include "AnnexB.h"
#include "H264.h"
#include "libyuv.h"
#include "mozilla/Logging.h"
#include "mozilla/mscom/EnsureMTA.h"

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

static const GUID CodecToSubtype(CodecType aCodec) {
  switch (aCodec) {
    case CodecType::H264:
      return MFVideoFormat_H264;
    case CodecType::VP8:
      return MFVideoFormat_VP80;
    case CodecType::VP9:
      return MFVideoFormat_VP90;
    default:
      MOZ_ASSERT(false, "Unsupported codec");
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
    default:
      return eAVEncH264VProfile_unknown;
  }
}

already_AddRefed<IMFMediaType> CreateInputType(EncoderConfig& aConfig) {
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

already_AddRefed<IMFMediaType> CreateOutputType(EncoderConfig& aConfig) {
  RefPtr<IMFMediaType> type;
  if (FAILED(wmf::MFCreateMediaType(getter_AddRefs(type))) ||
      FAILED(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) ||
      FAILED(type->SetGUID(MF_MT_SUBTYPE, CodecToSubtype(aConfig.mCodec))) ||
      FAILED(type->SetUINT32(MF_MT_AVG_BITRATE, aConfig.mBitrate)) ||
      FAILED(type->SetUINT32(MF_MT_INTERLACE_MODE,
                             MFVideoInterlace_Progressive)) ||
      FAILED(
          MFSetAttributeRatio(type, MF_MT_FRAME_RATE, aConfig.mFramerate, 1)) ||
      FAILED(MFSetAttributeSize(type, MF_MT_FRAME_SIZE, aConfig.mSize.width,
                                aConfig.mSize.height))) {
    return nullptr;
  }
  if (aConfig.mCodecSpecific) {
    if (aConfig.mCodecSpecific->is<H264Specific>()) {
      if (FAILED(type->SetUINT32(
              MF_MT_MPEG2_PROFILE,
              GetProfile(
                  aConfig.mCodecSpecific->as<H264Specific>().mProfile)))) {
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
