/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_media_MediaIPCUtils_h
#define mozilla_dom_media_MediaIPCUtils_h

#include "DecoderDoctorDiagnostics.h"
#include "PlatformDecoderModule.h"
#include "ipc/EnumSerializer.h"
#include "mozilla/EnumSet.h"
#include "mozilla/GfxMessageUtils.h"
#include "mozilla/gfx/Rect.h"

namespace IPC {
template <>
struct ParamTraits<mozilla::VideoInfo> {
  typedef mozilla::VideoInfo paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    // TrackInfo
    WriteParam(aWriter, aParam.mMimeType);

    // VideoInfo
    WriteParam(aWriter, aParam.mDisplay);
    WriteParam(aWriter, aParam.mStereoMode);
    WriteParam(aWriter, aParam.mImage);
    WriteParam(aWriter, aParam.mImageRect);
    WriteParam(aWriter, *aParam.mCodecSpecificConfig);
    WriteParam(aWriter, *aParam.mExtraData);
    WriteParam(aWriter, aParam.mRotation);
    WriteParam(aWriter, aParam.mColorDepth);
    WriteParam(aWriter, aParam.mColorSpace);
    WriteParam(aWriter, aParam.mColorRange);
    WriteParam(aWriter, aParam.HasAlpha());
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    mozilla::gfx::IntRect imageRect;
    bool alphaPresent;
    if (ReadParam(aReader, &aResult->mMimeType) &&
        ReadParam(aReader, &aResult->mDisplay) &&
        ReadParam(aReader, &aResult->mStereoMode) &&
        ReadParam(aReader, &aResult->mImage) &&
        ReadParam(aReader, &aResult->mImageRect) &&
        ReadParam(aReader, aResult->mCodecSpecificConfig.get()) &&
        ReadParam(aReader, aResult->mExtraData.get()) &&
        ReadParam(aReader, &aResult->mRotation) &&
        ReadParam(aReader, &aResult->mColorDepth) &&
        ReadParam(aReader, &aResult->mColorSpace) &&
        ReadParam(aReader, &aResult->mColorRange) &&
        ReadParam(aReader, &alphaPresent)) {
      aResult->SetAlpha(alphaPresent);
      return true;
    }
    return false;
  }
};

template <>
struct ParamTraits<mozilla::TrackInfo::TrackType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::TrackInfo::TrackType,
          mozilla::TrackInfo::TrackType::kUndefinedTrack,
          mozilla::TrackInfo::TrackType::kTextTrack> {};

template <>
struct ParamTraits<mozilla::VideoInfo::Rotation>
    : public ContiguousEnumSerializerInclusive<
          mozilla::VideoInfo::Rotation, mozilla::VideoInfo::Rotation::kDegree_0,
          mozilla::VideoInfo::Rotation::kDegree_270> {};

template <>
struct ParamTraits<mozilla::MediaByteBuffer>
    : public ParamTraits<nsTArray<uint8_t>> {
  typedef mozilla::MediaByteBuffer paramType;
};

template <>
struct ParamTraits<mozilla::AudioInfo> {
  typedef mozilla::AudioInfo paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    // TrackInfo
    WriteParam(aWriter, aParam.mMimeType);

    // AudioInfo
    WriteParam(aWriter, aParam.mRate);
    WriteParam(aWriter, aParam.mChannels);
    WriteParam(aWriter, aParam.mChannelMap);
    WriteParam(aWriter, aParam.mBitDepth);
    WriteParam(aWriter, aParam.mProfile);
    WriteParam(aWriter, aParam.mExtendedProfile);
    WriteParam(aWriter, *aParam.mCodecSpecificConfig);
    WriteParam(aWriter, *aParam.mExtraData);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (ReadParam(aReader, &aResult->mMimeType) &&
        ReadParam(aReader, &aResult->mRate) &&
        ReadParam(aReader, &aResult->mChannels) &&
        ReadParam(aReader, &aResult->mChannelMap) &&
        ReadParam(aReader, &aResult->mBitDepth) &&
        ReadParam(aReader, &aResult->mProfile) &&
        ReadParam(aReader, &aResult->mExtendedProfile) &&
        ReadParam(aReader, aResult->mCodecSpecificConfig.get()) &&
        ReadParam(aReader, aResult->mExtraData.get())) {
      return true;
    }
    return false;
  }
};

template <>
struct ParamTraits<mozilla::MediaDataDecoder::ConversionRequired>
    : public ContiguousEnumSerializerInclusive<
          mozilla::MediaDataDecoder::ConversionRequired,
          mozilla::MediaDataDecoder::ConversionRequired(0),
          mozilla::MediaDataDecoder::ConversionRequired(
              mozilla::MediaDataDecoder::ConversionRequired::kNeedAnnexB)> {};

template <>
struct ParamTraits<mozilla::media::TimeUnit> {
  typedef mozilla::media::TimeUnit paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.IsValid());
    WriteParam(aWriter, aParam.IsValid() ? aParam.ToMicroseconds() : 0);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    bool valid;
    int64_t value;
    if (ReadParam(aReader, &valid) && ReadParam(aReader, &value)) {
      if (!valid) {
        *aResult = mozilla::media::TimeUnit::Invalid();
      } else {
        *aResult = mozilla::media::TimeUnit::FromMicroseconds(value);
      }
      return true;
    }
    return false;
  };
};

template <>
struct ParamTraits<mozilla::media::TimeInterval> {
  typedef mozilla::media::TimeInterval paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mStart);
    WriteParam(aWriter, aParam.mEnd);
    WriteParam(aWriter, aParam.mFuzz);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (ReadParam(aReader, &aResult->mStart) &&
        ReadParam(aReader, &aResult->mEnd) &&
        ReadParam(aReader, &aResult->mFuzz)) {
      return true;
    }
    return false;
  }
};

template <>
struct ParamTraits<mozilla::MediaResult> {
  typedef mozilla::MediaResult paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.Code());
    WriteParam(aWriter, aParam.Message());
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    nsresult result;
    nsCString message;
    if (ReadParam(aReader, &result) && ReadParam(aReader, &message)) {
      *aResult = paramType(result, std::move(message));
      return true;
    }
    return false;
  };
};

template <>
struct ParamTraits<mozilla::DecoderDoctorDiagnostics> {
  typedef mozilla::DecoderDoctorDiagnostics paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mDiagnosticsType);
    WriteParam(aWriter, aParam.mFormat);
    WriteParam(aWriter, aParam.mFlags);
    WriteParam(aWriter, aParam.mEvent);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (ReadParam(aReader, &aResult->mDiagnosticsType) &&
        ReadParam(aReader, &aResult->mFormat) &&
        ReadParam(aReader, &aResult->mFlags) &&
        ReadParam(aReader, &aResult->mEvent)) {
      return true;
    }
    return false;
  };
};

template <>
struct ParamTraits<mozilla::DecoderDoctorDiagnostics::DiagnosticsType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::DecoderDoctorDiagnostics::DiagnosticsType,
          mozilla::DecoderDoctorDiagnostics::DiagnosticsType::eUnsaved,
          mozilla::DecoderDoctorDiagnostics::DiagnosticsType::eDecodeWarning> {
};

template <>
struct ParamTraits<mozilla::DecoderDoctorEvent> {
  typedef mozilla::DecoderDoctorEvent paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    int domain = aParam.mDomain;
    WriteParam(aWriter, domain);
    WriteParam(aWriter, aParam.mResult);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    int domain = 0;
    if (ReadParam(aReader, &domain) && ReadParam(aReader, &aResult->mResult)) {
      aResult->mDomain = paramType::Domain(domain);
      return true;
    }
    return false;
  };
};

}  // namespace IPC

#endif  // mozilla_dom_media_MediaIPCUtils_h
