/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPMessageUtils_h_
#define GMPMessageUtils_h_

#include "gmp-video-codec.h"

namespace IPC {

template <>
struct ParamTraits<GMPVideoCodecComplexity>
: public ContiguousEnumSerializer<GMPVideoCodecComplexity,
                                  kGMPComplexityNormal,
                                  kGMPComplexityInvalid>
{};

template <>
struct ParamTraits<GMPVP8ResilienceMode>
: public ContiguousEnumSerializer<GMPVP8ResilienceMode,
                                  kResilienceOff,
                                  kResilienceInvalid>
{};

template <>
struct ParamTraits<GMPVideoCodecType>
: public ContiguousEnumSerializer<GMPVideoCodecType,
                                  kGMPVideoCodecVP8,
                                  kGMPVideoCodecInvalid>
{};

template <>
struct ParamTraits<GMPVideoCodecMode>
: public ContiguousEnumSerializer<GMPVideoCodecMode,
                                  kGMPRealtimeVideo,
                                  kGMPCodecModeInvalid>
{};

template <>
struct ParamTraits<GMPVideoCodecVP8>
{
  typedef GMPVideoCodecVP8 paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mPictureLossIndicationOn);
    WriteParam(aMsg, aParam.mFeedbackModeOn);
    WriteParam(aMsg, aParam.mComplexity);
    WriteParam(aMsg, aParam.mResilience);
    WriteParam(aMsg, aParam.mNumberOfTemporalLayers);
    WriteParam(aMsg, aParam.mDenoisingOn);
    WriteParam(aMsg, aParam.mErrorConcealmentOn);
    WriteParam(aMsg, aParam.mAutomaticResizeOn);
    WriteParam(aMsg, aParam.mFrameDroppingOn);
    WriteParam(aMsg, aParam.mKeyFrameInterval);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (ReadParam(aMsg, aIter, &(aResult->mPictureLossIndicationOn)) &&
        ReadParam(aMsg, aIter, &(aResult->mFeedbackModeOn)) &&
        ReadParam(aMsg, aIter, &(aResult->mComplexity)) &&
        ReadParam(aMsg, aIter, &(aResult->mResilience)) &&
        ReadParam(aMsg, aIter, &(aResult->mNumberOfTemporalLayers)) &&
        ReadParam(aMsg, aIter, &(aResult->mDenoisingOn)) &&
        ReadParam(aMsg, aIter, &(aResult->mErrorConcealmentOn)) &&
        ReadParam(aMsg, aIter, &(aResult->mAutomaticResizeOn)) &&
        ReadParam(aMsg, aIter, &(aResult->mFrameDroppingOn)) &&
        ReadParam(aMsg, aIter, &(aResult->mKeyFrameInterval))) {
      return true;
    }

    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%d, %d, %d, %d, %u, %d, %d, %d, %d, %d]",
                              aParam.mPictureLossIndicationOn,
                              aParam.mFeedbackModeOn,
                              aParam.mComplexity,
                              aParam.mResilience,
                              aParam.mNumberOfTemporalLayers,
                              aParam.mDenoisingOn,
                              aParam.mErrorConcealmentOn,
                              aParam.mAutomaticResizeOn,
                              aParam.mFrameDroppingOn,
                              aParam.mKeyFrameInterval));
  }
};

template <>
struct ParamTraits<GMPSimulcastStream>
{
  typedef GMPSimulcastStream paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mWidth);
    WriteParam(aMsg, aParam.mHeight);
    WriteParam(aMsg, aParam.mNumberOfTemporalLayers);
    WriteParam(aMsg, aParam.mMaxBitrate);
    WriteParam(aMsg, aParam.mTargetBitrate);
    WriteParam(aMsg, aParam.mMinBitrate);
    WriteParam(aMsg, aParam.mQPMax);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (ReadParam(aMsg, aIter, &(aResult->mWidth)) &&
        ReadParam(aMsg, aIter, &(aResult->mHeight)) &&
        ReadParam(aMsg, aIter, &(aResult->mNumberOfTemporalLayers)) &&
        ReadParam(aMsg, aIter, &(aResult->mMaxBitrate)) &&
        ReadParam(aMsg, aIter, &(aResult->mTargetBitrate)) &&
        ReadParam(aMsg, aIter, &(aResult->mMinBitrate)) &&
        ReadParam(aMsg, aIter, &(aResult->mQPMax))) {
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%u, %u, %u, %u, %u, %u, %u]", aParam.mWidth, aParam.mHeight,
                              aParam.mNumberOfTemporalLayers, aParam.mMaxBitrate,
                              aParam.mTargetBitrate, aParam.mMinBitrate, aParam.mQPMax));
  }
};

template <>
struct ParamTraits<GMPVideoCodec>
{
  typedef GMPVideoCodec paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mCodecType);
    WriteParam(aMsg, nsAutoCString(aParam.mPLName));
    WriteParam(aMsg, aParam.mPLType);
    WriteParam(aMsg, aParam.mWidth);
    WriteParam(aMsg, aParam.mHeight);
    WriteParam(aMsg, aParam.mStartBitrate);
    WriteParam(aMsg, aParam.mMaxBitrate);
    WriteParam(aMsg, aParam.mMinBitrate);
    WriteParam(aMsg, aParam.mMaxFramerate);
    if (aParam.mCodecType == kGMPVideoCodecVP8) {
      WriteParam(aMsg, aParam.mCodecSpecific.mVP8);
    } else {
      MOZ_ASSERT(false, "Serializing unknown codec type!");
    }
    WriteParam(aMsg, aParam.mQPMax);
    WriteParam(aMsg, aParam.mNumberOfSimulcastStreams);
    for (uint32_t i = 0; i < aParam.mNumberOfSimulcastStreams; i++) {
      WriteParam(aMsg, aParam.mSimulcastStream[i]);
    }
    WriteParam(aMsg, aParam.mMode);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mCodecType))) {
      return false;
    }

    nsAutoCString plName;
    if (!ReadParam(aMsg, aIter, &plName) ||
        plName.Length() > kGMPPayloadNameSize - 1) {
      return false;
    }
    memcpy(aResult->mPLName, plName.get(), plName.Length());
    memset(aResult->mPLName + plName.Length(), 0, kGMPPayloadNameSize - plName.Length());

    if (!ReadParam(aMsg, aIter, &(aResult->mPLType)) ||
        !ReadParam(aMsg, aIter, &(aResult->mWidth)) ||
        !ReadParam(aMsg, aIter, &(aResult->mHeight)) ||
        !ReadParam(aMsg, aIter, &(aResult->mStartBitrate)) ||
        !ReadParam(aMsg, aIter, &(aResult->mMaxBitrate)) ||
        !ReadParam(aMsg, aIter, &(aResult->mMinBitrate)) ||
        !ReadParam(aMsg, aIter, &(aResult->mMaxFramerate))) {
      return false;
    }

    if (aResult->mCodecType == kGMPVideoCodecVP8) {
      if (!ReadParam(aMsg, aIter, &(aResult->mCodecSpecific.mVP8))) {
        return false;
      }
    } else {
      MOZ_ASSERT(false, "De-serializing unknown codec type!");
      return false;
    }

    if (!ReadParam(aMsg, aIter, &(aResult->mQPMax)) ||
        !ReadParam(aMsg, aIter, &(aResult->mNumberOfSimulcastStreams))) {
      return false;
    }

    if (aResult->mNumberOfSimulcastStreams > kGMPMaxSimulcastStreams) {
      return false;
    }

    for (uint32_t i = 0; i < aResult->mNumberOfSimulcastStreams; i++) {
      if (!ReadParam(aMsg, aIter, &(aResult->mSimulcastStream[i]))) {
        return false;
      }
    }

    if (!ReadParam(aMsg, aIter, &(aResult->mMode))) {
      return false;
    }

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    const char* codecName = nullptr;
    if (aParam.mCodecType == kGMPVideoCodecVP8) {
      codecName = "VP8";
    }
    aLog->append(StringPrintf(L"[%s, %u, %u]",
                              codecName,
                              aParam.mWidth,
                              aParam.mHeight));
  }
};

template <>
struct ParamTraits<GMPCodecSpecificInfoVP8>
{
  typedef GMPCodecSpecificInfoVP8 paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mHasReceivedSLI);
    WriteParam(aMsg, aParam.mPictureIdSLI);
    WriteParam(aMsg, aParam.mHasReceivedRPSI);
    WriteParam(aMsg, aParam.mPictureIdRPSI);
    WriteParam(aMsg, aParam.mPictureId);
    WriteParam(aMsg, aParam.mNonReference);
    WriteParam(aMsg, aParam.mSimulcastIdx);
    WriteParam(aMsg, aParam.mTemporalIdx);
    WriteParam(aMsg, aParam.mLayerSync);
    WriteParam(aMsg, aParam.mTL0PicIdx);
    WriteParam(aMsg, aParam.mKeyIdx);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (ReadParam(aMsg, aIter, &(aResult->mHasReceivedSLI)) &&
        ReadParam(aMsg, aIter, &(aResult->mPictureIdSLI)) &&
        ReadParam(aMsg, aIter, &(aResult->mHasReceivedRPSI)) &&
        ReadParam(aMsg, aIter, &(aResult->mPictureIdRPSI)) &&
        ReadParam(aMsg, aIter, &(aResult->mPictureId)) &&
        ReadParam(aMsg, aIter, &(aResult->mNonReference)) &&
        ReadParam(aMsg, aIter, &(aResult->mSimulcastIdx)) &&
        ReadParam(aMsg, aIter, &(aResult->mTemporalIdx)) &&
        ReadParam(aMsg, aIter, &(aResult->mLayerSync)) &&
        ReadParam(aMsg, aIter, &(aResult->mTL0PicIdx)) &&
        ReadParam(aMsg, aIter, &(aResult->mKeyIdx))) {
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%d, %u, %d, %u, %d, %d, %u, %u, %d, %d, %d]",
                              aParam.mHasReceivedSLI,
                              aParam.mPictureIdSLI,
                              aParam.mHasReceivedRPSI,
                              aParam.mPictureIdRPSI,
                              aParam.mPictureId,
                              aParam.mNonReference,
                              aParam.mSimulcastIdx,
                              aParam.mTemporalIdx,
                              aParam.mLayerSync,
                              aParam.mTL0PicIdx,
                              aParam.mKeyIdx));
  }
};

template <>
struct ParamTraits<GMPCodecSpecificInfo>
{
  typedef GMPCodecSpecificInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mCodecType);
    if (aParam.mCodecType == kGMPVideoCodecVP8) {
      WriteParam(aMsg, aParam.mCodecSpecific.mVP8);
    } else {
      MOZ_ASSERT(false, "Serializing unknown codec type!");
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mCodecType))) {
      return false;
    }

    if (aResult->mCodecType == kGMPVideoCodecVP8) {
      if (!ReadParam(aMsg, aIter, &(aResult->mCodecSpecific.mVP8))) {
        return false;
      }
    } else {
      MOZ_ASSERT(false, "De-serializing unknown codec type!");
      return false;
    }

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    const char* codecName = nullptr;
    if (aParam.mCodecType == kGMPVideoCodecVP8) {
      codecName = "VP8";
    }
    aLog->append(StringPrintf(L"[%s]", codecName));
  }
};

} // namespace IPC

#endif // GMPMessageUtils_h_
