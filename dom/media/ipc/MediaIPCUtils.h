/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_media_MediaIPCUtils_h
#define mozilla_dom_media_MediaIPCUtils_h

#include "PlatformDecoderModule.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/GfxMessageUtils.h"
#include "mozilla/gfx/Rect.h"

namespace IPC {
template <>
struct ParamTraits<mozilla::VideoInfo> {
  typedef mozilla::VideoInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    // TrackInfo
    WriteParam(aMsg, aParam.mMimeType);

    // VideoInfo
    WriteParam(aMsg, aParam.mDisplay);
    WriteParam(aMsg, aParam.mStereoMode);
    WriteParam(aMsg, aParam.mImage);
    WriteParam(aMsg, aParam.ImageRect());
    WriteParam(aMsg, *aParam.mCodecSpecificConfig);
    WriteParam(aMsg, *aParam.mExtraData);
    WriteParam(aMsg, aParam.mRotation);
    WriteParam(aMsg, aParam.mColorDepth);
    WriteParam(aMsg, aParam.mColorSpace);
    WriteParam(aMsg, aParam.mFullRange);
    WriteParam(aMsg, aParam.HasAlpha());
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    mozilla::gfx::IntRect imageRect;
    bool alphaPresent;
    if (ReadParam(aMsg, aIter, &aResult->mMimeType) &&
        ReadParam(aMsg, aIter, &aResult->mDisplay) &&
        ReadParam(aMsg, aIter, &aResult->mStereoMode) &&
        ReadParam(aMsg, aIter, &aResult->mImage) &&
        ReadParam(aMsg, aIter, &imageRect) &&
        ReadParam(aMsg, aIter, aResult->mCodecSpecificConfig.get()) &&
        ReadParam(aMsg, aIter, aResult->mExtraData.get()) &&
        ReadParam(aMsg, aIter, &aResult->mRotation) &&
        ReadParam(aMsg, aIter, &aResult->mColorDepth) &&
        ReadParam(aMsg, aIter, &aResult->mColorSpace) &&
        ReadParam(aMsg, aIter, &aResult->mFullRange) &&
        ReadParam(aMsg, aIter, &alphaPresent)) {
      aResult->SetImageRect(imageRect);
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

  static void Write(Message* aMsg, const paramType& aParam) {
    // TrackInfo
    WriteParam(aMsg, aParam.mMimeType);

    // AudioInfo
    WriteParam(aMsg, aParam.mRate);
    WriteParam(aMsg, aParam.mChannels);
    WriteParam(aMsg, aParam.mChannelMap);
    WriteParam(aMsg, aParam.mBitDepth);
    WriteParam(aMsg, aParam.mProfile);
    WriteParam(aMsg, aParam.mExtendedProfile);
    WriteParam(aMsg, *aParam.mCodecSpecificConfig);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (ReadParam(aMsg, aIter, &aResult->mMimeType) &&
        ReadParam(aMsg, aIter, &aResult->mRate) &&
        ReadParam(aMsg, aIter, &aResult->mChannels) &&
        ReadParam(aMsg, aIter, &aResult->mChannelMap) &&
        ReadParam(aMsg, aIter, &aResult->mBitDepth) &&
        ReadParam(aMsg, aIter, &aResult->mProfile) &&
        ReadParam(aMsg, aIter, &aResult->mExtendedProfile) &&
        ReadParam(aMsg, aIter, aResult->mCodecSpecificConfig.get())) {
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

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.IsValid());
    WriteParam(aMsg, aParam.IsValid() ? aParam.ToMicroseconds() : 0);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    bool valid;
    int64_t value;
    if (ReadParam(aMsg, aIter, &valid) && ReadParam(aMsg, aIter, &value)) {
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

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mStart);
    WriteParam(aMsg, aParam.mEnd);
    WriteParam(aMsg, aParam.mFuzz);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (ReadParam(aMsg, aIter, &aResult->mStart) &&
        ReadParam(aMsg, aIter, &aResult->mEnd) &&
        ReadParam(aMsg, aIter, &aResult->mFuzz)) {
      return true;
    }
    return false;
  }
};

}  // namespace IPC

#endif  // mozilla_dom_media_MediaIPCUtils_h
