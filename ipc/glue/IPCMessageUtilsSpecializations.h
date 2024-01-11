/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __IPC_GLUE_IPCMESSAGEUTILSSPECIALIZATIONS_H__
#define __IPC_GLUE_IPCMESSAGEUTILSSPECIALIZATIONS_H__

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include "chrome/common/ipc_message.h"
#include "chrome/common/ipc_message_utils.h"
#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/BitSet.h"
#include "mozilla/EnumSet.h"
#include "mozilla/EnumTypeTraits.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#ifdef XP_WIN
#  include "mozilla/TimeStamp_windows.h"
#endif

#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/Vector.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/dom/UserActivation.h"
#include "nsCSSPropertyID.h"
#include "nsDebug.h"
#include "nsIContentPolicy.h"
#include "nsID.h"
#include "nsILoadInfo.h"
#include "nsIThread.h"
#include "nsLiteralString.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsTHashSet.h"

// XXX Includes that are only required by implementations which could be moved
// to the cpp file.
#include "base/string_util.h"    // for StringPrintf
#include "mozilla/ArrayUtils.h"  // for ArrayLength
#include "mozilla/CheckedInt.h"

#ifdef _MSC_VER
#  pragma warning(disable : 4800)
#endif

namespace mozilla {
template <typename... Ts>
class Variant;

namespace detail {
template <typename... Ts>
struct VariantTag;
}
}  // namespace mozilla

namespace mozilla::dom {
template <typename T>
class Optional;
}

class nsAtom;

namespace IPC {

template <class T>
struct ParamTraits<nsTSubstring<T>> {
  typedef nsTSubstring<T> paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    bool isVoid = aParam.IsVoid();
    aWriter->WriteBool(isVoid);

    if (isVoid) {
      // represents a nullptr pointer
      return;
    }

    WriteSequenceParam<const T&>(aWriter, aParam.BeginReading(),
                                 aParam.Length());
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    bool isVoid;
    if (!aReader->ReadBool(&isVoid)) {
      return false;
    }

    if (isVoid) {
      aResult->SetIsVoid(true);
      return true;
    }

    return ReadSequenceParam<T>(aReader, [&](uint32_t aLength) -> T* {
      T* data = nullptr;
      aResult->GetMutableData(&data, aLength);
      return data;
    });
  }
};

template <class T>
struct ParamTraits<nsTString<T>> : ParamTraits<nsTSubstring<T>> {};

template <class T>
struct ParamTraits<nsTLiteralString<T>> : ParamTraits<nsTSubstring<T>> {};

template <class T, size_t N>
struct ParamTraits<nsTAutoStringN<T, N>> : ParamTraits<nsTSubstring<T>> {};

template <class T>
struct ParamTraits<nsTDependentString<T>> : ParamTraits<nsTSubstring<T>> {};

// XXX While this has no special dependencies, it's currently only used in
// GfxMessageUtils and could be moved there, or generalized to potentially work
// with any nsTHashSet.
template <>
struct ParamTraits<nsTHashSet<uint64_t>> {
  typedef nsTHashSet<uint64_t> paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    uint32_t count = aParam.Count();
    WriteParam(aWriter, count);
    for (const auto& key : aParam) {
      WriteParam(aWriter, key);
    }
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    uint32_t count;
    if (!ReadParam(aReader, &count)) {
      return false;
    }
    paramType table(count);
    for (uint32_t i = 0; i < count; ++i) {
      uint64_t key;
      if (!ReadParam(aReader, &key)) {
        return false;
      }
      table.Insert(key);
    }
    *aResult = std::move(table);
    return true;
  }
};

template <typename E>
struct ParamTraits<nsTArray<E>> {
  typedef nsTArray<E> paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteSequenceParam<const E&>(aWriter, aParam.Elements(), aParam.Length());
  }

  static void Write(MessageWriter* aWriter, paramType&& aParam) {
    WriteSequenceParam<E&&>(aWriter, aParam.Elements(), aParam.Length());
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadSequenceParam<E>(aReader, [&](uint32_t aLength) {
      if constexpr (std::is_trivially_default_constructible_v<E>) {
        return aResult->AppendElements(aLength);
      } else {
        aResult->SetCapacity(aLength);
        return mozilla::Some(MakeBackInserter(*aResult));
      }
    });
  }
};

template <typename E>
struct ParamTraits<CopyableTArray<E>> : ParamTraits<nsTArray<E>> {};

template <typename E>
struct ParamTraits<FallibleTArray<E>> {
  typedef FallibleTArray<E> paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteSequenceParam<const E&>(aWriter, aParam.Elements(), aParam.Length());
  }

  static void Write(MessageWriter* aWriter, paramType&& aParam) {
    WriteSequenceParam<E&&>(aWriter, aParam.Elements(), aParam.Length());
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadSequenceParam<E>(aReader, [&](uint32_t aLength) {
      if constexpr (std::is_trivially_default_constructible_v<E>) {
        return aResult->AppendElements(aLength, mozilla::fallible);
      } else {
        if (!aResult->SetCapacity(aLength, mozilla::fallible)) {
          return mozilla::Maybe<BackInserter>{};
        }
        return mozilla::Some(BackInserter{.mArray = aResult});
      }
    });
  }

 private:
  struct BackInserter {
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = void;
    using pointer = void;
    using reference = void;

    struct Proxy {
      paramType& mArray;

      template <typename U>
      void operator=(U&& aValue) {
        // This won't fail because we've reserved capacity earlier.
        MOZ_ALWAYS_TRUE(mArray.AppendElement(aValue, mozilla::fallible));
      }
    };
    Proxy operator*() { return Proxy{.mArray = *mArray}; }

    BackInserter& operator++() { return *this; }
    BackInserter& operator++(int) { return *this; }

    paramType* mArray = nullptr;
  };
};

template <typename E, size_t N>
struct ParamTraits<AutoTArray<E, N>> : ParamTraits<nsTArray<E>> {
  typedef AutoTArray<E, N> paramType;
};

template <typename E, size_t N>
struct ParamTraits<CopyableAutoTArray<E, N>> : ParamTraits<AutoTArray<E, N>> {};

template <typename T>
struct ParamTraits<mozilla::dom::Sequence<T>> : ParamTraits<FallibleTArray<T>> {
};

template <typename E, size_t N, typename AP>
struct ParamTraits<mozilla::Vector<E, N, AP>> {
  typedef mozilla::Vector<E, N, AP> paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteSequenceParam<const E&>(aWriter, aParam.Elements(), aParam.Length());
  }

  static void Write(MessageWriter* aWriter, paramType&& aParam) {
    WriteSequenceParam<E&&>(aWriter, aParam.Elements(), aParam.Length());
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadSequenceParam<E>(aReader, [&](uint32_t aLength) -> E* {
      if (!aResult->resize(aLength)) {
        // So that OOM failure shows up as OOM crash instead of IPC FatalError.
        NS_ABORT_OOM(aLength * sizeof(E));
      }
      return aResult->begin();
    });
  }
};

template <typename E>
struct ParamTraits<std::vector<E>> {
  typedef std::vector<E> paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteSequenceParam<const E&>(aWriter, aParam.data(), aParam.size());
  }
  static void Write(MessageWriter* aWriter, paramType&& aParam) {
    WriteSequenceParam<E&&>(aWriter, aParam.data(), aParam.size());
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadSequenceParam<E>(aReader, [&](uint32_t aLength) {
      if constexpr (std::is_trivially_default_constructible_v<E>) {
        aResult->resize(aLength);
        return aResult->data();
      } else {
        aResult->reserve(aLength);
        return mozilla::Some(std::back_inserter(*aResult));
      }
    });
  }
};

template <typename K, typename V>
struct ParamTraits<std::unordered_map<K, V>> final {
  using T = std::unordered_map<K, V>;

  static void Write(MessageWriter* const writer, const T& in) {
    WriteParam(writer, in.size());
    for (const auto& pair : in) {
      WriteParam(writer, pair.first);
      WriteParam(writer, pair.second);
    }
  }

  static bool Read(MessageReader* const reader, T* const out) {
    size_t size = 0;
    if (!ReadParam(reader, &size)) return false;
    T map;
    map.reserve(size);
    for (const auto i : mozilla::IntegerRange(size)) {
      std::pair<K, V> pair;
      mozilla::Unused << i;
      if (!ReadParam(reader, &(pair.first)) ||
          !ReadParam(reader, &(pair.second))) {
        return false;
      }
      map.insert(std::move(pair));
    }
    *out = std::move(map);
    return true;
  }
};

template <>
struct ParamTraits<float> {
  typedef float paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    aWriter->WriteBytes(&aParam, sizeof(paramType));
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return aReader->ReadBytesInto(aResult, sizeof(*aResult));
  }
};

template <>
struct ParamTraits<nsCSSPropertyID>
    : public ContiguousEnumSerializer<nsCSSPropertyID, eCSSProperty_UNKNOWN,
                                      eCSSProperty_COUNT> {};

template <>
struct ParamTraits<nsID> {
  typedef nsID paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.m0);
    WriteParam(aWriter, aParam.m1);
    WriteParam(aWriter, aParam.m2);
    for (unsigned int i = 0; i < mozilla::ArrayLength(aParam.m3); i++) {
      WriteParam(aWriter, aParam.m3[i]);
    }
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &(aResult->m0)) ||
        !ReadParam(aReader, &(aResult->m1)) ||
        !ReadParam(aReader, &(aResult->m2)))
      return false;

    for (unsigned int i = 0; i < mozilla::ArrayLength(aResult->m3); i++)
      if (!ReadParam(aReader, &(aResult->m3[i]))) return false;

    return true;
  }
};

template <>
struct ParamTraits<nsContentPolicyType>
    : public ContiguousEnumSerializer<nsContentPolicyType,
                                      nsIContentPolicy::TYPE_INVALID,
                                      nsIContentPolicy::TYPE_END> {};

template <>
struct ParamTraits<mozilla::TimeDuration> {
  typedef mozilla::TimeDuration paramType;
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mValue);
  }
  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mValue);
  };
};

template <>
struct ParamTraits<mozilla::TimeStamp> {
  typedef mozilla::TimeStamp paramType;
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mValue);
  }
  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mValue);
  };
};

#ifdef XP_WIN

template <>
struct ParamTraits<mozilla::TimeStampValue> {
  typedef mozilla::TimeStampValue paramType;
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mGTC);
    WriteParam(aWriter, aParam.mQPC);
    WriteParam(aWriter, aParam.mIsNull);
    WriteParam(aWriter, aParam.mHasQPC);
  }
  static bool Read(MessageReader* aReader, paramType* aResult) {
    return (ReadParam(aReader, &aResult->mGTC) &&
            ReadParam(aReader, &aResult->mQPC) &&
            ReadParam(aReader, &aResult->mIsNull) &&
            ReadParam(aReader, &aResult->mHasQPC));
  }
};

#endif

template <>
struct ParamTraits<mozilla::dom::ipc::StructuredCloneData> {
  typedef mozilla::dom::ipc::StructuredCloneData paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    aParam.WriteIPCParams(aWriter);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return aResult->ReadIPCParams(aReader);
  }
};

template <class T>
struct ParamTraits<mozilla::Maybe<T>> {
  typedef mozilla::Maybe<T> paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    if (param.isSome()) {
      WriteParam(writer, true);
      WriteParam(writer, param.ref());
    } else {
      WriteParam(writer, false);
    }
  }

  static void Write(MessageWriter* writer, paramType&& param) {
    if (param.isSome()) {
      WriteParam(writer, true);
      WriteParam(writer, std::move(param.ref()));
    } else {
      WriteParam(writer, false);
    }
  }

  static bool Read(MessageReader* reader, paramType* result) {
    bool isSome;
    if (!ReadParam(reader, &isSome)) {
      return false;
    }
    if (isSome) {
      mozilla::Maybe<T> tmp = ReadParam<T>(reader).TakeMaybe();
      if (!tmp) {
        return false;
      }
      *result = std::move(tmp);
    } else {
      *result = mozilla::Nothing();
    }
    return true;
  }
};

template <typename T, typename U>
struct ParamTraits<mozilla::EnumSet<T, U>> {
  typedef mozilla::EnumSet<T, U> paramType;
  typedef U serializedType;

  static void Write(MessageWriter* writer, const paramType& param) {
    MOZ_RELEASE_ASSERT(IsLegalValue(param.serialize()));
    WriteParam(writer, param.serialize());
  }

  static bool Read(MessageReader* reader, paramType* result) {
    serializedType tmp;

    if (ReadParam(reader, &tmp)) {
      if (IsLegalValue(tmp)) {
        result->deserialize(tmp);
        return true;
      }
    }

    return false;
  }

  static constexpr serializedType AllEnumBits() {
    return ~serializedType(0) >> (std::numeric_limits<serializedType>::digits -
                                  (mozilla::MaxEnumValue<T>::value + 1));
  }

  static constexpr bool IsLegalValue(const serializedType value) {
    static_assert(mozilla::MaxEnumValue<T>::value <
                      std::numeric_limits<serializedType>::digits,
                  "Enum max value is not in the range!");
    static_assert(
        std::is_unsigned<decltype(mozilla::MaxEnumValue<T>::value)>::value,
        "Type of MaxEnumValue<T>::value specialization should be unsigned!");

    return (value & AllEnumBits()) == value;
  }
};

template <class... Ts>
struct ParamTraits<mozilla::Variant<Ts...>> {
  typedef mozilla::Variant<Ts...> paramType;
  using Tag = typename mozilla::detail::VariantTag<Ts...>::Type;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.tag);
    param.match([writer](const auto& t) { WriteParam(writer, t); });
  }

  // Because VariantReader is a nested struct, we need the dummy template
  // parameter to avoid making VariantReader<0> an explicit specialization,
  // which is not allowed for a nested class template
  template <size_t N, typename dummy = void>
  struct VariantReader {
    using Next = VariantReader<N - 1>;

    static bool Read(MessageReader* reader, Tag tag, paramType* result) {
      // Since the VariantReader specializations start at N , we need to
      // subtract one to look at N - 1, the first valid tag.  This means our
      // comparisons are off by 1.  If we get to N = 0 then we have failed to
      // find a match to the tag.
      if (tag == N - 1) {
        // Recall, even though the template parameter is N, we are
        // actually interested in the N - 1 tag.
        // Default construct our field within the result outparameter and
        // directly deserialize into the variant. Note that this means that
        // every type in Ts needs to be default constructible
        return ReadParam(reader, &result->template emplace<N - 1>());
      } else {
        return Next::Read(reader, tag, result);
      }
    }

  };  // VariantReader<N>

  // Since we are conditioning on tag = N - 1 in the preceding specialization,
  // if we get to `VariantReader<0, dummy>` we have failed to find
  // a matching tag.
  template <typename dummy>
  struct VariantReader<0, dummy> {
    static bool Read(MessageReader* reader, Tag tag, paramType* result) {
      return false;
    }
  };

  static bool Read(MessageReader* reader, paramType* result) {
    Tag tag;
    if (ReadParam(reader, &tag)) {
      return VariantReader<sizeof...(Ts)>::Read(reader, tag, result);
    }
    return false;
  }
};

template <typename T>
struct ParamTraits<mozilla::dom::Optional<T>> {
  typedef mozilla::dom::Optional<T> paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    if (aParam.WasPassed()) {
      WriteParam(aWriter, true);
      WriteParam(aWriter, aParam.Value());
      return;
    }

    WriteParam(aWriter, false);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    bool wasPassed = false;

    if (!ReadParam(aReader, &wasPassed)) {
      return false;
    }

    aResult->Reset();

    if (wasPassed) {
      if (!ReadParam(aReader, &aResult->Construct())) {
        return false;
      }
    }

    return true;
  }
};

template <>
struct ParamTraits<nsAtom*> {
  typedef nsAtom paramType;

  static void Write(MessageWriter* aWriter, const paramType* aParam);
  static bool Read(MessageReader* aReader, RefPtr<paramType>* aResult);
};

struct CrossOriginOpenerPolicyValidator {
  using IntegralType =
      std::underlying_type_t<nsILoadInfo::CrossOriginOpenerPolicy>;

  static bool IsLegalValue(const IntegralType e) {
    return AreIntegralValuesEqual(e, nsILoadInfo::OPENER_POLICY_UNSAFE_NONE) ||
           AreIntegralValuesEqual(e, nsILoadInfo::OPENER_POLICY_SAME_ORIGIN) ||
           AreIntegralValuesEqual(
               e, nsILoadInfo::OPENER_POLICY_SAME_ORIGIN_ALLOW_POPUPS) ||
           AreIntegralValuesEqual(
               e, nsILoadInfo::
                      OPENER_POLICY_SAME_ORIGIN_EMBEDDER_POLICY_REQUIRE_CORP);
  }

 private:
  static bool AreIntegralValuesEqual(
      const IntegralType aLhs,
      const nsILoadInfo::CrossOriginOpenerPolicy aRhs) {
    return aLhs == static_cast<IntegralType>(aRhs);
  }
};

template <>
struct ParamTraits<nsILoadInfo::CrossOriginOpenerPolicy>
    : EnumSerializer<nsILoadInfo::CrossOriginOpenerPolicy,
                     CrossOriginOpenerPolicyValidator> {};

struct CrossOriginEmbedderPolicyValidator {
  using IntegralType =
      std::underlying_type_t<nsILoadInfo::CrossOriginEmbedderPolicy>;

  static bool IsLegalValue(const IntegralType e) {
    return AreIntegralValuesEqual(e, nsILoadInfo::EMBEDDER_POLICY_NULL) ||
           AreIntegralValuesEqual(e,
                                  nsILoadInfo::EMBEDDER_POLICY_REQUIRE_CORP) ||
           AreIntegralValuesEqual(e,
                                  nsILoadInfo::EMBEDDER_POLICY_CREDENTIALLESS);
  }

 private:
  static bool AreIntegralValuesEqual(
      const IntegralType aLhs,
      const nsILoadInfo::CrossOriginEmbedderPolicy aRhs) {
    return aLhs == static_cast<IntegralType>(aRhs);
  }
};

template <>
struct ParamTraits<nsILoadInfo::CrossOriginEmbedderPolicy>
    : EnumSerializer<nsILoadInfo::CrossOriginEmbedderPolicy,
                     CrossOriginEmbedderPolicyValidator> {};

template <>
struct ParamTraits<nsIThread::QoSPriority>
    : public ContiguousEnumSerializerInclusive<nsIThread::QoSPriority,
                                               nsIThread::QOS_PRIORITY_NORMAL,
                                               nsIThread::QOS_PRIORITY_LOW> {};

template <size_t N, typename Word>
struct ParamTraits<mozilla::BitSet<N, Word>> {
  typedef mozilla::BitSet<N, Word> paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    for (Word word : aParam.Storage()) {
      WriteParam(aWriter, word);
    }
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    for (Word& word : aResult->Storage()) {
      if (!ReadParam(aReader, &word)) {
        return false;
      }
    }
    return true;
  }
};

template <typename T>
struct ParamTraits<mozilla::UniquePtr<T>> {
  typedef mozilla::UniquePtr<T> paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    bool isNull = aParam == nullptr;
    WriteParam(aWriter, isNull);

    if (!isNull) {
      WriteParam(aWriter, *aParam.get());
    }
  }

  static bool Read(IPC::MessageReader* aReader, paramType* aResult) {
    bool isNull = true;
    if (!ReadParam(aReader, &isNull)) {
      return false;
    }

    if (isNull) {
      aResult->reset();
    } else {
      *aResult = mozilla::MakeUnique<T>();
      if (!ReadParam(aReader, aResult->get())) {
        return false;
      }
    }
    return true;
  }
};

template <typename... Ts>
struct ParamTraits<std::tuple<Ts...>> {
  typedef std::tuple<Ts...> paramType;

  template <typename U>
  static void Write(IPC::MessageWriter* aWriter, U&& aParam) {
    WriteInternal(aWriter, std::forward<U>(aParam),
                  std::index_sequence_for<Ts...>{});
  }

  static bool Read(IPC::MessageReader* aReader, std::tuple<Ts...>* aResult) {
    return ReadInternal(aReader, *aResult, std::index_sequence_for<Ts...>{});
  }

 private:
  template <size_t... Is>
  static void WriteInternal(IPC::MessageWriter* aWriter,
                            const std::tuple<Ts...>& aParam,
                            std::index_sequence<Is...>) {
    WriteParams(aWriter, std::get<Is>(aParam)...);
  }

  template <size_t... Is>
  static void WriteInternal(IPC::MessageWriter* aWriter,
                            std::tuple<Ts...>&& aParam,
                            std::index_sequence<Is...>) {
    WriteParams(aWriter, std::move(std::get<Is>(aParam))...);
  }

  template <size_t... Is>
  static bool ReadInternal(IPC::MessageReader* aReader,
                           std::tuple<Ts...>& aResult,
                           std::index_sequence<Is...>) {
    return ReadParams(aReader, std::get<Is>(aResult)...);
  }
};

template <>
struct ParamTraits<mozilla::net::LinkHeader> {
  typedef mozilla::net::LinkHeader paramType;
  constexpr static int kNumberOfMembers = 14;
  constexpr static int kSizeOfEachMember = sizeof(nsString);
  constexpr static int kExpectedSizeOfParamType =
      kNumberOfMembers * kSizeOfEachMember;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    static_assert(sizeof(paramType) == kExpectedSizeOfParamType,
                  "All members of should be written below.");
    // Bug 1860565: `aParam.mAnchor` is not written.

    WriteParam(aWriter, aParam.mHref);
    WriteParam(aWriter, aParam.mRel);
    WriteParam(aWriter, aParam.mTitle);
    WriteParam(aWriter, aParam.mNonce);
    WriteParam(aWriter, aParam.mIntegrity);
    WriteParam(aWriter, aParam.mSrcset);
    WriteParam(aWriter, aParam.mSizes);
    WriteParam(aWriter, aParam.mType);
    WriteParam(aWriter, aParam.mMedia);
    WriteParam(aWriter, aParam.mCrossOrigin);
    WriteParam(aWriter, aParam.mReferrerPolicy);
    WriteParam(aWriter, aParam.mAs);
    WriteParam(aWriter, aParam.mFetchPriority);
  }
  static bool Read(MessageReader* aReader, paramType* aResult) {
    static_assert(sizeof(paramType) == kExpectedSizeOfParamType,
                  "All members of should be handled below.");
    // Bug 1860565: `aParam.mAnchor` is not handled.

    if (!ReadParam(aReader, &aResult->mHref)) {
      return false;
    }
    if (!ReadParam(aReader, &aResult->mRel)) {
      return false;
    }
    if (!ReadParam(aReader, &aResult->mTitle)) {
      return false;
    }
    if (!ReadParam(aReader, &aResult->mNonce)) {
      return false;
    }
    if (!ReadParam(aReader, &aResult->mIntegrity)) {
      return false;
    }
    if (!ReadParam(aReader, &aResult->mSrcset)) {
      return false;
    }
    if (!ReadParam(aReader, &aResult->mSizes)) {
      return false;
    }
    if (!ReadParam(aReader, &aResult->mType)) {
      return false;
    }
    if (!ReadParam(aReader, &aResult->mMedia)) {
      return false;
    }
    if (!ReadParam(aReader, &aResult->mCrossOrigin)) {
      return false;
    }
    if (!ReadParam(aReader, &aResult->mReferrerPolicy)) {
      return false;
    }
    if (!ReadParam(aReader, &aResult->mAs)) {
      return false;
    }
    return ReadParam(aReader, &aResult->mFetchPriority);
  };
};

template <>
struct ParamTraits<mozilla::dom::UserActivation::Modifiers> {
  typedef mozilla::dom::UserActivation::Modifiers paramType;
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mModifiers);
  }
  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mModifiers);
  };
};

} /* namespace IPC */

#endif /* __IPC_GLUE_IPCMESSAGEUTILSSPECIALIZATIONS_H__ */
