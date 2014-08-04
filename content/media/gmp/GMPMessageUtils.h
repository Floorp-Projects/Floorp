/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPMessageUtils_h_
#define GMPMessageUtils_h_

#include "gmp-video-codec.h"
#include "gmp-video-frame-encoded.h"
#include "gmp-audio-codec.h"
#include "gmp-decryption.h"

namespace IPC {

template <>
struct ParamTraits<GMPErr>
: public ContiguousEnumSerializer<GMPErr,
                                  GMPNoErr,
                                  GMPLastErr>
{};

struct GMPDomExceptionValidator {
  static bool IsLegalValue(GMPDOMException aValue) {
    switch (aValue) {
      case kGMPNoModificationAllowedError:
      case kGMPNotFoundError:
      case kGMPNotSupportedError:
      case kGMPInvalidStateError:
      case kGMPSyntaxError:
      case kGMPInvalidModificationError:
      case kGMPInvalidAccessError:
      case kGMPSecurityError:
      case kGMPAbortError:
      case kGMPQuotaExceededError:
      case kGMPTimeoutError:
        return true;
      default:
        return false;
    }
  }
};

template <>
struct ParamTraits<GMPVideoFrameType>
: public ContiguousEnumSerializer<GMPVideoFrameType,
                                  kGMPKeyFrame,
                                  kGMPSkipFrame>
{};

template<>
struct ParamTraits<GMPDOMException>
: public EnumSerializer<GMPDOMException, GMPDomExceptionValidator>
{};

template <>
struct ParamTraits<GMPSessionType>
: public ContiguousEnumSerializer<GMPSessionType,
                                  kGMPTemporySession,
                                  kGMPPersistentSession>
{};

template <>
struct ParamTraits<GMPAudioCodecType>
: public ContiguousEnumSerializer<GMPAudioCodecType,
                                  kGMPAudioCodecAAC,
                                  kGMPAudioCodecInvalid>
{};

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
struct ParamTraits<GMPBufferType>
: public ContiguousEnumSerializer<GMPBufferType,
                                  GMP_BufferSingle,
                                  GMP_BufferInvalid>
{};

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
    WriteParam(aMsg, aParam.mGMPApiVersion);
    WriteParam(aMsg, aParam.mCodecType);
    WriteParam(aMsg, nsAutoCString(aParam.mPLName));
    WriteParam(aMsg, aParam.mPLType);
    WriteParam(aMsg, aParam.mWidth);
    WriteParam(aMsg, aParam.mHeight);
    WriteParam(aMsg, aParam.mStartBitrate);
    WriteParam(aMsg, aParam.mMaxBitrate);
    WriteParam(aMsg, aParam.mMinBitrate);
    WriteParam(aMsg, aParam.mMaxFramerate);
    WriteParam(aMsg, aParam.mFrameDroppingOn);
    WriteParam(aMsg, aParam.mKeyFrameInterval);
    WriteParam(aMsg, aParam.mQPMax);
    WriteParam(aMsg, aParam.mNumberOfSimulcastStreams);
    for (uint32_t i = 0; i < aParam.mNumberOfSimulcastStreams; i++) {
      WriteParam(aMsg, aParam.mSimulcastStream[i]);
    }
    WriteParam(aMsg, aParam.mMode);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    // NOTE: make sure this matches any versions supported
    if (!ReadParam(aMsg, aIter, &(aResult->mGMPApiVersion)) ||
      aResult->mGMPApiVersion != kGMPVersion33) {
        return false;
    }
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
        !ReadParam(aMsg, aIter, &(aResult->mMaxFramerate)) ||
        !ReadParam(aMsg, aIter, &(aResult->mFrameDroppingOn)) ||
        !ReadParam(aMsg, aIter, &(aResult->mKeyFrameInterval))) {
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

} // namespace IPC

#endif // GMPMessageUtils_h_
