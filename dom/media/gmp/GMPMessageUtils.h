/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPMessageUtils_h_
#define GMPMessageUtils_h_

#include "gmp-video-codec.h"
#include "gmp-video-frame-encoded.h"
#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "GMPNativeTypes.h"
#include "GMPSanitizedExports.h"

namespace IPC {

template <>
struct ParamTraits<GMPPluginType>
    : public ContiguousEnumSerializerInclusive<
          GMPPluginType, GMPPluginType::Unknown, GMPPluginType::WidevineL1> {};

template <>
struct ParamTraits<GMPErr>
    : public ContiguousEnumSerializer<GMPErr, GMPNoErr, GMPLastErr> {};

template <>
struct ParamTraits<GMPVideoFrameType>
    : public ContiguousEnumSerializer<GMPVideoFrameType, kGMPKeyFrame,
                                      kGMPVideoFrameInvalid> {};

template <>
struct ParamTraits<GMPVideoCodecComplexity>
    : public ContiguousEnumSerializer<GMPVideoCodecComplexity,
                                      kGMPComplexityNormal,
                                      kGMPComplexityInvalid> {};

template <>
struct ParamTraits<GMPVP8ResilienceMode>
    : public ContiguousEnumSerializer<GMPVP8ResilienceMode, kResilienceOff,
                                      kResilienceInvalid> {};

template <>
struct ParamTraits<GMPVideoCodecType>
    : public ContiguousEnumSerializer<GMPVideoCodecType, kGMPVideoCodecVP8,
                                      kGMPVideoCodecInvalid> {};

template <>
struct ParamTraits<GMPVideoCodecMode>
    : public ContiguousEnumSerializer<GMPVideoCodecMode, kGMPRealtimeVideo,
                                      kGMPCodecModeInvalid> {};

template <>
struct ParamTraits<GMPLogLevel>
    : public ContiguousEnumSerializerInclusive<GMPLogLevel, kGMPLogDefault,
                                               kGMPLogInvalid> {};

template <>
struct ParamTraits<GMPBufferType>
    : public ContiguousEnumSerializer<GMPBufferType, GMP_BufferSingle,
                                      GMP_BufferInvalid> {};

template <>
struct ParamTraits<cdm::EncryptionScheme>
    : public ContiguousEnumSerializerInclusive<
          cdm::EncryptionScheme, cdm::EncryptionScheme::kUnencrypted,
          cdm::EncryptionScheme::kCbcs> {};

template <>
struct ParamTraits<cdm::HdcpVersion>
    : public ContiguousEnumSerializerInclusive<
          cdm::HdcpVersion, cdm::HdcpVersion::kHdcpVersionNone,
          cdm::HdcpVersion::kHdcpVersion2_2> {};

template <>
struct ParamTraits<GMPSimulcastStream> {
  typedef GMPSimulcastStream paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mWidth);
    WriteParam(aWriter, aParam.mHeight);
    WriteParam(aWriter, aParam.mNumberOfTemporalLayers);
    WriteParam(aWriter, aParam.mMaxBitrate);
    WriteParam(aWriter, aParam.mTargetBitrate);
    WriteParam(aWriter, aParam.mMinBitrate);
    WriteParam(aWriter, aParam.mQPMax);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (ReadParam(aReader, &(aResult->mWidth)) &&
        ReadParam(aReader, &(aResult->mHeight)) &&
        ReadParam(aReader, &(aResult->mNumberOfTemporalLayers)) &&
        ReadParam(aReader, &(aResult->mMaxBitrate)) &&
        ReadParam(aReader, &(aResult->mTargetBitrate)) &&
        ReadParam(aReader, &(aResult->mMinBitrate)) &&
        ReadParam(aReader, &(aResult->mQPMax))) {
      return true;
    }
    return false;
  }
};

template <>
struct ParamTraits<GMPVideoCodec> {
  typedef GMPVideoCodec paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mGMPApiVersion);
    WriteParam(aWriter, aParam.mCodecType);
    WriteParam(aWriter, static_cast<const nsCString&>(
                            nsDependentCString(aParam.mPLName)));
    WriteParam(aWriter, aParam.mPLType);
    WriteParam(aWriter, aParam.mWidth);
    WriteParam(aWriter, aParam.mHeight);
    WriteParam(aWriter, aParam.mStartBitrate);
    WriteParam(aWriter, aParam.mMaxBitrate);
    WriteParam(aWriter, aParam.mMinBitrate);
    WriteParam(aWriter, aParam.mMaxFramerate);
    WriteParam(aWriter, aParam.mFrameDroppingOn);
    WriteParam(aWriter, aParam.mKeyFrameInterval);
    WriteParam(aWriter, aParam.mQPMax);
    WriteParam(aWriter, aParam.mNumberOfSimulcastStreams);
    for (uint32_t i = 0; i < aParam.mNumberOfSimulcastStreams; i++) {
      WriteParam(aWriter, aParam.mSimulcastStream[i]);
    }
    WriteParam(aWriter, aParam.mMode);
    WriteParam(aWriter, aParam.mUseThreadedDecode);
    WriteParam(aWriter, aParam.mLogLevel);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    // NOTE: make sure this matches any versions supported
    if (!ReadParam(aReader, &(aResult->mGMPApiVersion)) ||
        (aResult->mGMPApiVersion != kGMPVersion33 &&
         aResult->mGMPApiVersion != kGMPVersion34)) {
      return false;
    }
    if (!ReadParam(aReader, &(aResult->mCodecType))) {
      return false;
    }

    nsAutoCString plName;
    if (!ReadParam(aReader, &plName) ||
        plName.Length() > kGMPPayloadNameSize - 1) {
      return false;
    }
    memcpy(aResult->mPLName, plName.get(), plName.Length());
    memset(aResult->mPLName + plName.Length(), 0,
           kGMPPayloadNameSize - plName.Length());

    if (!ReadParam(aReader, &(aResult->mPLType)) ||
        !ReadParam(aReader, &(aResult->mWidth)) ||
        !ReadParam(aReader, &(aResult->mHeight)) ||
        !ReadParam(aReader, &(aResult->mStartBitrate)) ||
        !ReadParam(aReader, &(aResult->mMaxBitrate)) ||
        !ReadParam(aReader, &(aResult->mMinBitrate)) ||
        !ReadParam(aReader, &(aResult->mMaxFramerate)) ||
        !ReadParam(aReader, &(aResult->mFrameDroppingOn)) ||
        !ReadParam(aReader, &(aResult->mKeyFrameInterval))) {
      return false;
    }

    if (!ReadParam(aReader, &(aResult->mQPMax)) ||
        !ReadParam(aReader, &(aResult->mNumberOfSimulcastStreams))) {
      return false;
    }

    if (aResult->mNumberOfSimulcastStreams > kGMPMaxSimulcastStreams) {
      return false;
    }

    for (uint32_t i = 0; i < aResult->mNumberOfSimulcastStreams; i++) {
      if (!ReadParam(aReader, &(aResult->mSimulcastStream[i]))) {
        return false;
      }
    }

    if (!ReadParam(aReader, &(aResult->mMode)) ||
        !ReadParam(aReader, &(aResult->mUseThreadedDecode)) ||
        !ReadParam(aReader, &(aResult->mLogLevel))) {
      return false;
    }

    return true;
  }
};

}  // namespace IPC

#endif  // GMPMessageUtils_h_
