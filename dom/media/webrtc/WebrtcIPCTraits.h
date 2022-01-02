/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _WEBRTC_IPC_TRAITS_H_
#define _WEBRTC_IPC_TRAITS_H_

#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtils.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/RTCConfigurationBinding.h"
#include "mozilla/media/webrtc/WebrtcGlobal.h"
#include "mozilla/dom/CandidateInfo.h"
#include "mozilla/MacroForEach.h"
#include "transport/dtlsidentity.h"
#include <vector>

namespace mozilla {
typedef std::vector<std::string> StringVector;
}

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::OwningStringOrStringSequence> {
  typedef mozilla::dom::OwningStringOrStringSequence paramType;

  // Ugh. OwningStringOrStringSequence already has this enum, but it is
  // private generated code. So we have to re-create it.
  enum Type { kUninitialized, kString, kStringSequence };

  static void Write(Message* aMsg, const paramType& aParam) {
    if (aParam.IsString()) {
      aMsg->WriteInt16(kString);
      WriteParam(aMsg, aParam.GetAsString());
    } else if (aParam.IsStringSequence()) {
      aMsg->WriteInt16(kStringSequence);
      WriteParam(aMsg, aParam.GetAsStringSequence());
    } else {
      aMsg->WriteInt16(kUninitialized);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    int16_t type;
    if (!aMsg->ReadInt16(aIter, &type)) {
      return false;
    }

    switch (type) {
      case kUninitialized:
        aResult->Uninit();
        return true;
      case kString:
        return ReadParam(aMsg, aIter, &aResult->SetAsString());
      case kStringSequence:
        return ReadParam(aMsg, aIter, &aResult->SetAsStringSequence());
    }

    return false;
  }
};

template <typename T>
struct WebidlEnumSerializer
    : public ContiguousEnumSerializer<T, T(0), T::EndGuard_> {};

template <>
struct ParamTraits<mozilla::dom::RTCIceCredentialType>
    : public WebidlEnumSerializer<mozilla::dom::RTCIceCredentialType> {};

template <>
struct ParamTraits<mozilla::dom::RTCIceTransportPolicy>
    : public WebidlEnumSerializer<mozilla::dom::RTCIceTransportPolicy> {};

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::RTCIceServer, mCredential,
                                  mCredentialType, mUrl, mUrls, mUsername)

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::CandidateInfo, mCandidate, mUfrag,
                                  mDefaultHostRtp, mDefaultPortRtp,
                                  mDefaultHostRtcp, mDefaultPortRtcp,
                                  mMDNSAddress, mActualAddress)

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::DtlsDigest, algorithm_, value_)

}  // namespace IPC

#endif  // _WEBRTC_IPC_TRAITS_H_
