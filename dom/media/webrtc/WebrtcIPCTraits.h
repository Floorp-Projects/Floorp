/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _WEBRTC_IPC_TRAITS_H_
#define _WEBRTC_IPC_TRAITS_H_

#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/RTCConfigurationBinding.h"
#include "mozilla/media/webrtc/WebrtcGlobal.h"
#include "mozilla/dom/CandidateInfo.h"
#include "mozilla/MacroForEach.h"
#include "mtransport/transportlayerdtls.h"
#include <vector>

namespace mozilla {
typedef std::vector<std::string> StringVector;
}

namespace IPC {

template <typename T>
struct ParamTraits<std::vector<T>> {
  typedef std::vector<T> paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    aMsg->WriteUInt32(aParam.size());
    for (const T& elem : aParam) {
      WriteParam(aMsg, elem);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    uint32_t size;
    if (!aMsg->ReadUInt32(aIter, &size)) {
      return false;
    }
    while (size--) {
      // Only works when T is movable. Meh.
      T elem;
      if (!ReadParam(aMsg, aIter, &elem)) {
        return false;
      }
      aResult->emplace_back(std::move(elem));
    }

    return true;
  }
};

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

// A couple of recursive helper functions, allows syntax like:
// WriteParams(aMsg, aParam.foo, aParam.bar, aParam.baz)
// ReadParams(aMsg, aIter, aParam.foo, aParam.bar, aParam.baz)

// Base case
static void WriteParams(Message* aMsg) {}

template <typename T0, typename... Tn>
static void WriteParams(Message* aMsg, const T0& aArg,
                        const Tn&... aRemainingArgs) {
  WriteParam(aMsg, aArg);                // Write first arg
  WriteParams(aMsg, aRemainingArgs...);  // Recurse for the rest
}

// Base case
static bool ReadParams(const Message* aMsg, PickleIterator* aIter) {
  return true;
}

template <typename T0, typename... Tn>
static bool ReadParams(const Message* aMsg, PickleIterator* aIter, T0& aArg,
                       Tn&... aRemainingArgs) {
  return ReadParam(aMsg, aIter, &aArg) &&             // Read first arg
         ReadParams(aMsg, aIter, aRemainingArgs...);  // Recurse for the rest
}

// Macros that allow syntax like:
// DEFINE_IPC_SERIALIZER_WITH_FIELDS(SomeType, member1, member2, member3)
// Makes sure that serialize/deserialize code do the same members in the same
// order.
#define ACCESS_PARAM_FIELD(Field) aParam.Field

#define DEFINE_IPC_SERIALIZER_WITH_FIELDS(Type, ...)                         \
  template <>                                                                \
  struct ParamTraits<Type> {                                                 \
    typedef Type paramType;                                                  \
    static void Write(Message* aMsg, const paramType& aParam) {              \
      WriteParams(aMsg, MOZ_FOR_EACH_SEPARATED(ACCESS_PARAM_FIELD, (, ), (), \
                                               (__VA_ARGS__)));              \
    }                                                                        \
                                                                             \
    static bool Read(const Message* aMsg, PickleIterator* aIter,             \
                     paramType* aResult) {                                   \
      paramType& aParam = *aResult;                                          \
      return ReadParams(aMsg, aIter,                                         \
                        MOZ_FOR_EACH_SEPARATED(ACCESS_PARAM_FIELD, (, ), (), \
                                               (__VA_ARGS__)));              \
    }                                                                        \
  };

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::RTCIceServer, mCredential,
                                  mCredentialType, mUrl, mUrls, mUsername)

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::CandidateInfo, mCandidate, mUfrag,
                                  mDefaultHostRtp, mDefaultPortRtp,
                                  mDefaultHostRtcp, mDefaultPortRtcp)

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::DtlsDigest, algorithm_, value_)

}  // namespace IPC

#endif  // _WEBRTC_IPC_TRAITS_H_
