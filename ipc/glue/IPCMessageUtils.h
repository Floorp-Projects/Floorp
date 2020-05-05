/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __IPC_GLUE_IPCMESSAGEUTILS_H__
#define __IPC_GLUE_IPCMESSAGEUTILS_H__

#include "base/process_util.h"
#include "chrome/common/ipc_message_utils.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/EnumSet.h"
#include "mozilla/EnumTypeTraits.h"
#include "mozilla/Maybe.h"
#include "mozilla/net/WebSocketFrame.h"
#include "mozilla/TimeStamp.h"
#ifdef XP_WIN
#  include "mozilla/TimeStamp_windows.h"
#endif
#include "mozilla/IntegerTypeTraits.h"
#include "mozilla/Vector.h"

#include <limits>
#include <stdint.h>
#include <type_traits>
#include <vector>

#include "nsDebug.h"
#include "nsExceptionHandler.h"
#include "nsHashKeys.h"
#include "nsID.h"
#include "nsILoadInfo.h"
#include "nsIWidget.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "js/StructuredClone.h"
#include "nsCSSPropertyID.h"

#ifdef _MSC_VER
#  pragma warning(disable : 4800)
#endif

#if !defined(OS_POSIX)
// This condition must be kept in sync with the one in
// ipc_message_utils.h, but this dummy definition of
// base::FileDescriptor acts as a static assert that we only get one
// def or the other (or neither, in which case code using
// FileDescriptor fails to build)
namespace base {
struct FileDescriptor {};
}  // namespace base
#endif

namespace mozilla {

// This is a cross-platform approximation to HANDLE, which we expect
// to be typedef'd to void* or thereabouts.
typedef uintptr_t WindowsHandle;

// XXX there are out of place and might be generally useful.  Could
// move to nscore.h or something.
struct void_t {
  bool operator==(const void_t&) const { return true; }
};
struct null_t {
  bool operator==(const null_t&) const { return true; }
};

struct SerializedStructuredCloneBuffer final {
  SerializedStructuredCloneBuffer() = default;

  SerializedStructuredCloneBuffer(SerializedStructuredCloneBuffer&&) = default;
  SerializedStructuredCloneBuffer& operator=(
      SerializedStructuredCloneBuffer&&) = default;

  SerializedStructuredCloneBuffer(const SerializedStructuredCloneBuffer&) =
      delete;
  SerializedStructuredCloneBuffer& operator=(
      const SerializedStructuredCloneBuffer& aOther) = delete;

  bool operator==(const SerializedStructuredCloneBuffer& aOther) const {
    // The copy assignment operator and the equality operator are
    // needed by the IPDL generated code. We relied on the copy
    // assignment operator at some places but we never use the
    // equality operator.
    return false;
  }

  JSStructuredCloneData data{JS::StructuredCloneScope::Unassigned};
};

}  // namespace mozilla

namespace IPC {

/**
 * Generic enum serializer.
 *
 * Consider using the specializations below, such as ContiguousEnumSerializer.
 *
 * This is a generic serializer for any enum type used in IPDL.
 * Programmers can define ParamTraits<E> for enum type E by deriving
 * EnumSerializer<E, MyEnumValidator> where MyEnumValidator is a struct
 * that has to define a static IsLegalValue function returning whether
 * a given value is a legal value of the enum type at hand.
 *
 * \sa https://developer.mozilla.org/en/IPDL/Type_Serialization
 */
template <typename E, typename EnumValidator>
struct EnumSerializer {
  typedef E paramType;
  typedef typename mozilla::UnsignedStdintTypeForSize<sizeof(paramType)>::Type
      uintParamType;

  static void Write(Message* aMsg, const paramType& aValue) {
    MOZ_RELEASE_ASSERT(EnumValidator::IsLegalValue(aValue));
    WriteParam(aMsg, uintParamType(aValue));
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    uintParamType value;
    if (!ReadParam(aMsg, aIter, &value)) {
      CrashReporter::AnnotateCrashReport(
          CrashReporter::Annotation::IPCReadErrorReason,
          NS_LITERAL_CSTRING("Bad iter"));
      return false;
    } else if (!EnumValidator::IsLegalValue(paramType(value))) {
      CrashReporter::AnnotateCrashReport(
          CrashReporter::Annotation::IPCReadErrorReason,
          NS_LITERAL_CSTRING("Illegal value"));
      return false;
    }
    *aResult = paramType(value);
    return true;
  }
};

template <typename E, E MinLegal, E HighBound>
class ContiguousEnumValidator {
  // Silence overzealous -Wtype-limits bug in GCC fixed in GCC 4.8:
  // "comparison of unsigned expression >= 0 is always true"
  // http://gcc.gnu.org/bugzilla/show_bug.cgi?id=11856
  template <typename T>
  static bool IsLessThanOrEqual(T a, T b) {
    return a <= b;
  }

 public:
  static bool IsLegalValue(E e) {
    return IsLessThanOrEqual(MinLegal, e) && e < HighBound;
  }
};

template <typename E, E MinLegal, E MaxLegal>
class ContiguousEnumValidatorInclusive {
  // Silence overzealous -Wtype-limits bug in GCC fixed in GCC 4.8:
  // "comparison of unsigned expression >= 0 is always true"
  // http://gcc.gnu.org/bugzilla/show_bug.cgi?id=11856
  template <typename T>
  static bool IsLessThanOrEqual(T a, T b) {
    return a <= b;
  }

 public:
  static bool IsLegalValue(E e) {
    return IsLessThanOrEqual(MinLegal, e) && e <= MaxLegal;
  }
};

template <typename E, E AllBits>
struct BitFlagsEnumValidator {
  static bool IsLegalValue(E e) { return (e & AllBits) == e; }
};

/**
 * Specialization of EnumSerializer for enums with contiguous enum values.
 *
 * Provide two values: MinLegal, HighBound. An enum value x will be
 * considered legal if MinLegal <= x < HighBound.
 *
 * For example, following is definition of serializer for enum type FOO.
 * \code
 * enum FOO { FOO_FIRST, FOO_SECOND, FOO_LAST, NUM_FOO };
 *
 * template <>
 * struct ParamTraits<FOO>:
 *     public ContiguousEnumSerializer<FOO, FOO_FIRST, NUM_FOO> {};
 * \endcode
 * FOO_FIRST, FOO_SECOND, and FOO_LAST are valid value.
 */
template <typename E, E MinLegal, E HighBound>
struct ContiguousEnumSerializer
    : EnumSerializer<E, ContiguousEnumValidator<E, MinLegal, HighBound>> {};

/**
 * This is similar to ContiguousEnumSerializer, but the last template
 * parameter is expected to be the highest legal value, rather than a
 * sentinel value. This is intended to support enumerations that don't
 * have sentinel values.
 */
template <typename E, E MinLegal, E MaxLegal>
struct ContiguousEnumSerializerInclusive
    : EnumSerializer<E,
                     ContiguousEnumValidatorInclusive<E, MinLegal, MaxLegal>> {
};

/**
 * Specialization of EnumSerializer for enums representing bit flags.
 *
 * Provide one value: AllBits. An enum value x will be
 * considered legal if (x & AllBits) == x;
 *
 * Example:
 * \code
 * enum FOO {
 *   FOO_FIRST =  1 << 0,
 *   FOO_SECOND = 1 << 1,
 *   FOO_LAST =   1 << 2,
 *   ALL_BITS =   (1 << 3) - 1
 * };
 *
 * template <>
 * struct ParamTraits<FOO>:
 *     public BitFlagsEnumSerializer<FOO, FOO::ALL_BITS> {};
 * \endcode
 */
template <typename E, E AllBits>
struct BitFlagsEnumSerializer
    : EnumSerializer<E, BitFlagsEnumValidator<E, AllBits>> {};

/**
 * A helper class for serializing plain-old data (POD) structures.
 * The memory representation of the structure is written to and read from
 * the serialized stream directly, without individual processing of the
 * structure's members.
 *
 * Derive ParamTraits<T> from PlainOldDataSerializer<T> if T is POD.
 *
 * Note: For POD structures with enumeration fields, this will not do
 *   validation of the enum values the way serializing the fields
 *   individually would. Prefer serializing the fields individually
 *   in such cases.
 */
template <typename T>
struct PlainOldDataSerializer {
  static_assert(
      std::is_trivially_copyable<T>::value,
      "PlainOldDataSerializer can only be used with trivially copyable types!");

  typedef T paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    aMsg->WriteBytes(&aParam, sizeof(aParam));
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return aMsg->ReadBytesInto(aIter, aResult, sizeof(paramType));
  }
};

/**
 * A helper class for serializing empty structs. Since the struct is empty there
 * is nothing to write, and a priori we know the result of the read.
 */
template <typename T>
struct EmptyStructSerializer {
  typedef T paramType;

  static void Write(Message* aMsg, const paramType& aParam) {}

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    *aResult = {};
    return true;
  }
};

template <>
struct ParamTraits<int8_t> {
  typedef int8_t paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    aMsg->WriteBytes(&aParam, sizeof(aParam));
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return aMsg->ReadBytesInto(aIter, aResult, sizeof(*aResult));
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    // Use 0xff to avoid sign extension.
    aLog->append(StringPrintf(L"0x%02x", aParam & 0xff));
  }
};

template <>
struct ParamTraits<uint8_t> {
  typedef uint8_t paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    aMsg->WriteBytes(&aParam, sizeof(aParam));
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return aMsg->ReadBytesInto(aIter, aResult, sizeof(*aResult));
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    aLog->append(StringPrintf(L"0x%02x", aParam));
  }
};

#if !defined(OS_POSIX)
// See above re: keeping definitions in sync
template <>
struct ParamTraits<base::FileDescriptor> {
  typedef base::FileDescriptor paramType;
  static void Write(Message* aMsg, const paramType& aParam) {
    MOZ_CRASH("FileDescriptor isn't meaningful on this platform");
  }
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    MOZ_CRASH("FileDescriptor isn't meaningful on this platform");
    return false;
  }
};
#endif  // !defined(OS_POSIX)

template <>
struct ParamTraits<nsACString> {
  typedef nsACString paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    bool isVoid = aParam.IsVoid();
    aMsg->WriteBool(isVoid);

    if (isVoid)
      // represents a nullptr pointer
      return;

    uint32_t length = aParam.Length();
    WriteParam(aMsg, length);
    aMsg->WriteBytes(aParam.BeginReading(), length);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    bool isVoid;
    if (!aMsg->ReadBool(aIter, &isVoid)) return false;

    if (isVoid) {
      aResult->SetIsVoid(true);
      return true;
    }

    uint32_t length;
    if (!ReadParam(aMsg, aIter, &length)) {
      return false;
    }
    if (!aMsg->HasBytesAvailable(aIter, length)) {
      return false;
    }
    aResult->SetLength(length);

    return aMsg->ReadBytesInto(aIter, aResult->BeginWriting(), length);
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    if (aParam.IsVoid())
      aLog->append(L"(NULL)");
    else
      aLog->append(UTF8ToWide(aParam.BeginReading()));
  }
};

template <>
struct ParamTraits<nsAString> {
  typedef nsAString paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    bool isVoid = aParam.IsVoid();
    aMsg->WriteBool(isVoid);

    if (isVoid)
      // represents a nullptr pointer
      return;

    uint32_t length = aParam.Length();
    WriteParam(aMsg, length);
    aMsg->WriteBytes(aParam.BeginReading(), length * sizeof(char16_t));
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    bool isVoid;
    if (!aMsg->ReadBool(aIter, &isVoid)) return false;

    if (isVoid) {
      aResult->SetIsVoid(true);
      return true;
    }

    uint32_t length;
    if (!ReadParam(aMsg, aIter, &length)) {
      return false;
    }

    mozilla::CheckedInt<uint32_t> byteLength =
        mozilla::CheckedInt<uint32_t>(length) * sizeof(char16_t);
    if (!byteLength.isValid() ||
        !aMsg->HasBytesAvailable(aIter, byteLength.value())) {
      return false;
    }

    aResult->SetLength(length);

    return aMsg->ReadBytesInto(aIter, aResult->BeginWriting(),
                               byteLength.value());
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    if (aParam.IsVoid())
      aLog->append(L"(NULL)");
    else {
#ifdef WCHAR_T_IS_UTF16
      aLog->append(reinterpret_cast<const wchar_t*>(aParam.BeginReading()));
#else
      uint32_t length = aParam.Length();
      for (uint32_t index = 0; index < length; index++) {
        aLog->push_back(std::wstring::value_type(aParam[index]));
      }
#endif
    }
  }
};

template <>
struct ParamTraits<nsCString> : ParamTraits<nsACString> {
  typedef nsCString paramType;
};

template <>
struct ParamTraits<nsLiteralCString> : ParamTraits<nsACString> {
  typedef nsLiteralCString paramType;
};

#ifdef MOZILLA_INTERNAL_API

template <>
struct ParamTraits<nsAutoCString> : ParamTraits<nsCString> {
  typedef nsAutoCString paramType;
};

#endif  // MOZILLA_INTERNAL_API

template <>
struct ParamTraits<nsString> : ParamTraits<nsAString> {
  typedef nsString paramType;
};

template <>
struct ParamTraits<nsLiteralString> : ParamTraits<nsAString> {
  typedef nsLiteralString paramType;
};

template <>
struct ParamTraits<nsDependentSubstring> : ParamTraits<nsAString> {
  typedef nsDependentSubstring paramType;
};

template <>
struct ParamTraits<nsDependentCSubstring> : ParamTraits<nsACString> {
  typedef nsDependentCSubstring paramType;
};

#ifdef MOZILLA_INTERNAL_API

template <>
struct ParamTraits<nsAutoString> : ParamTraits<nsString> {
  typedef nsAutoString paramType;
};

#endif  // MOZILLA_INTERNAL_API

template <>
struct ParamTraits<nsTHashtable<nsUint64HashKey>> {
  typedef nsTHashtable<nsUint64HashKey> paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    uint32_t count = aParam.Count();
    WriteParam(aMsg, count);
    for (auto iter = aParam.ConstIter(); !iter.Done(); iter.Next()) {
      WriteParam(aMsg, iter.Get()->GetKey());
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    uint32_t count;
    if (!ReadParam(aMsg, aIter, &count)) {
      return false;
    }
    paramType table(count);
    for (uint32_t i = 0; i < count; ++i) {
      uint64_t key;
      if (!ReadParam(aMsg, aIter, &key)) {
        return false;
      }
      table.PutEntry(key);
    }
    *aResult = std::move(table);
    return true;
  }
};

// Pickle::ReadBytes and ::WriteBytes take the length in ints, so we must
// ensure there is no overflow. This returns |false| if it would overflow.
// Otherwise, it returns |true| and places the byte length in |aByteLength|.
bool ByteLengthIsValid(uint32_t aNumElements, size_t aElementSize,
                       int* aByteLength);

// Note: IPDL will sometimes codegen specialized implementations of
// nsTArray serialization and deserialization code in
// implementSpecialArrayPickling(). This is needed when ParamTraits<E>
// is not defined.
template <typename E>
struct ParamTraits<nsTArray<E>> {
  typedef nsTArray<E> paramType;

  // We write arrays of integer or floating-point data using a single pickling
  // call, rather than writing each element individually.  We deliberately do
  // not use mozilla::IsPod here because it is perfectly reasonable to have
  // a data structure T for which IsPod<T>::value is true, yet also have a
  // ParamTraits<T> specialization.
  static const bool sUseWriteBytes =
      (std::is_integral_v<E> || std::is_floating_point_v<E>);

  static void Write(Message* aMsg, const paramType& aParam) {
    uint32_t length = aParam.Length();
    WriteParam(aMsg, length);

    if (sUseWriteBytes) {
      int pickledLength = 0;
      MOZ_RELEASE_ASSERT(ByteLengthIsValid(length, sizeof(E), &pickledLength));
      aMsg->WriteBytes(aParam.Elements(), pickledLength);
    } else {
      const E* elems = aParam.Elements();
      for (uint32_t index = 0; index < length; index++) {
        WriteParam(aMsg, elems[index]);
      }
    }
  }

  // This method uses infallible allocation so that an OOM failure will
  // show up as an OOM crash rather than an IPC FatalError.
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    uint32_t length;
    if (!ReadParam(aMsg, aIter, &length)) {
      return false;
    }

    if (sUseWriteBytes) {
      int pickledLength = 0;
      if (!ByteLengthIsValid(length, sizeof(E), &pickledLength)) {
        return false;
      }

      E* elements = aResult->AppendElements(length);
      return aMsg->ReadBytesInto(aIter, elements, pickledLength);
    } else {
      // Each ReadParam<E> may read more than 1 byte each; this is an attempt
      // to minimally validate that the length isn't much larger than what's
      // actually available in aMsg.
      if (!aMsg->HasBytesAvailable(aIter, length)) {
        return false;
      }

      aResult->SetCapacity(length);

      for (uint32_t index = 0; index < length; index++) {
        E* element = aResult->AppendElement();
        if (!ReadParam(aMsg, aIter, element)) {
          return false;
        }
      }
      return true;
    }
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    for (uint32_t index = 0; index < aParam.Length(); index++) {
      if (index) {
        aLog->append(L" ");
      }
      LogParam(aParam[index], aLog);
    }
  }
};

template <typename E>
struct ParamTraits<CopyableTArray<E>> : ParamTraits<nsTArray<E>> {};

template <typename E>
struct ParamTraits<FallibleTArray<E>> {
  typedef FallibleTArray<E> paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<const nsTArray<E>&>(aParam));
  }

  // Deserialize the array infallibly, but return a FallibleTArray.
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    nsTArray<E> temp;
    if (!ReadParam(aMsg, aIter, &temp)) return false;

    aResult->SwapElements(temp);
    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    LogParam(static_cast<const nsTArray<E>&>(aParam), aLog);
  }
};

template <typename E, size_t N>
struct ParamTraits<AutoTArray<E, N>> : ParamTraits<nsTArray<E>> {
  typedef AutoTArray<E, N> paramType;
};

template <typename E, size_t N>
struct ParamTraits<CopyableAutoTArray<E, N>> : ParamTraits<AutoTArray<E, N>> {};

template <typename E, size_t N, typename AP>
struct ParamTraits<mozilla::Vector<E, N, AP>> {
  typedef mozilla::Vector<E, N, AP> paramType;

  // We write arrays of integer or floating-point data using a single pickling
  // call, rather than writing each element individually.  We deliberately do
  // not use mozilla::IsPod here because it is perfectly reasonable to have
  // a data structure T for which IsPod<T>::value is true, yet also have a
  // ParamTraits<T> specialization.
  static const bool sUseWriteBytes =
      (std::is_integral_v<E> || std::is_floating_point_v<E>);

  static void Write(Message* aMsg, const paramType& aParam) {
    uint32_t length = aParam.length();
    WriteParam(aMsg, length);

    if (sUseWriteBytes) {
      int pickledLength = 0;
      MOZ_RELEASE_ASSERT(ByteLengthIsValid(length, sizeof(E), &pickledLength));
      aMsg->WriteBytes(aParam.begin(), pickledLength);
      return;
    }

    for (const E& elem : aParam) {
      WriteParam(aMsg, elem);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    uint32_t length;
    if (!ReadParam(aMsg, aIter, &length)) {
      return false;
    }

    if (sUseWriteBytes) {
      int pickledLength = 0;
      if (!ByteLengthIsValid(length, sizeof(E), &pickledLength)) {
        return false;
      }

      if (!aResult->resizeUninitialized(length)) {
        // So that OOM failure shows up as OOM crash instead of IPC FatalError.
        NS_ABORT_OOM(length * sizeof(E));
      }

      E* elements = aResult->begin();
      return aMsg->ReadBytesInto(aIter, elements, pickledLength);
    }

    // Each ReadParam<E> may read more than 1 byte each; this is an attempt
    // to minimally validate that the length isn't much larger than what's
    // actually available in aMsg.
    if (!aMsg->HasBytesAvailable(aIter, length)) {
      return false;
    }

    if (!aResult->resize(length)) {
      // So that OOM failure shows up as OOM crash instead of IPC FatalError.
      NS_ABORT_OOM(length * sizeof(E));
    }

    for (uint32_t index = 0; index < length; ++index) {
      if (!ReadParam(aMsg, aIter, &((*aResult)[index]))) {
        return false;
      }
    }

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    for (uint32_t index = 0, len = aParam.length(); index < len; ++index) {
      if (index) {
        aLog->append(L" ");
      }
      LogParam(aParam[index], aLog);
    }
  }
};

template <typename E>
struct ParamTraits<std::vector<E>> {
  typedef std::vector<E> paramType;

  // We write arrays of integer or floating-point data using a single pickling
  // call, rather than writing each element individually.  We deliberately do
  // not use mozilla::IsPod here because it is perfectly reasonable to have
  // a data structure T for which IsPod<T>::value is true, yet also have a
  // ParamTraits<T> specialization.
  static const bool sUseWriteBytes =
      (std::is_integral_v<E> || std::is_floating_point_v<E>);

  static void Write(Message* aMsg, const paramType& aParam) {
    uint32_t length = aParam.size();
    WriteParam(aMsg, length);

    if (sUseWriteBytes) {
      int pickledLength = 0;
      MOZ_RELEASE_ASSERT(ByteLengthIsValid(length, sizeof(E), &pickledLength));
      aMsg->WriteBytes(aParam.data(), pickledLength);
      return;
    }

    for (const E& elem : aParam) {
      WriteParam(aMsg, elem);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    uint32_t length;
    if (!ReadParam(aMsg, aIter, &length)) {
      return false;
    }

    if (sUseWriteBytes) {
      int pickledLength = 0;
      if (!ByteLengthIsValid(length, sizeof(E), &pickledLength)) {
        return false;
      }

      aResult->resize(length);

      E* elements = aResult->data();
      return aMsg->ReadBytesInto(aIter, elements, pickledLength);
    }

    // Each ReadParam<E> may read more than 1 byte each; this is an attempt
    // to minimally validate that the length isn't much larger than what's
    // actually available in aMsg.
    if (!aMsg->HasBytesAvailable(aIter, length)) {
      return false;
    }

    aResult->resize(length);

    for (uint32_t index = 0; index < length; ++index) {
      if (!ReadParam(aMsg, aIter, &((*aResult)[index]))) {
        return false;
      }
    }

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    for (uint32_t index = 0, len = aParam.size(); index < len; ++index) {
      if (index) {
        aLog->append(L" ");
      }
      LogParam(aParam[index], aLog);
    }
  }
};

template <>
struct ParamTraits<float> {
  typedef float paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    aMsg->WriteBytes(&aParam, sizeof(paramType));
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return aMsg->ReadBytesInto(aIter, aResult, sizeof(*aResult));
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    aLog->append(StringPrintf(L"%g", aParam));
  }
};

template <>
struct ParamTraits<nsCSSPropertyID>
    : public ContiguousEnumSerializer<nsCSSPropertyID, eCSSProperty_UNKNOWN,
                                      eCSSProperty_COUNT> {};

template <>
struct ParamTraits<mozilla::void_t> {
  typedef mozilla::void_t paramType;
  static void Write(Message* aMsg, const paramType& aParam) {}
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    *aResult = paramType();
    return true;
  }
};

template <>
struct ParamTraits<mozilla::null_t> {
  typedef mozilla::null_t paramType;
  static void Write(Message* aMsg, const paramType& aParam) {}
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    *aResult = paramType();
    return true;
  }
};

template <>
struct ParamTraits<nsID> {
  typedef nsID paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.m0);
    WriteParam(aMsg, aParam.m1);
    WriteParam(aMsg, aParam.m2);
    for (unsigned int i = 0; i < mozilla::ArrayLength(aParam.m3); i++) {
      WriteParam(aMsg, aParam.m3[i]);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &(aResult->m0)) ||
        !ReadParam(aMsg, aIter, &(aResult->m1)) ||
        !ReadParam(aMsg, aIter, &(aResult->m2)))
      return false;

    for (unsigned int i = 0; i < mozilla::ArrayLength(aResult->m3); i++)
      if (!ReadParam(aMsg, aIter, &(aResult->m3[i]))) return false;

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    aLog->append(L"{");
    aLog->append(
        StringPrintf(L"%8.8X-%4.4X-%4.4X-", aParam.m0, aParam.m1, aParam.m2));
    for (unsigned int i = 0; i < mozilla::ArrayLength(aParam.m3); i++)
      aLog->append(StringPrintf(L"%2.2X", aParam.m3[i]));
    aLog->append(L"}");
  }
};

template <>
struct ParamTraits<mozilla::TimeDuration> {
  typedef mozilla::TimeDuration paramType;
  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mValue);
  }
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mValue);
  };
};

template <>
struct ParamTraits<mozilla::TimeStamp> {
  typedef mozilla::TimeStamp paramType;
  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mValue);
  }
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mValue);
  };
};

#ifdef XP_WIN

template <>
struct ParamTraits<mozilla::TimeStampValue> {
  typedef mozilla::TimeStampValue paramType;
  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mGTC);
    WriteParam(aMsg, aParam.mQPC);
    WriteParam(aMsg, aParam.mUsedCanonicalNow);
    WriteParam(aMsg, aParam.mIsNull);
    WriteParam(aMsg, aParam.mHasQPC);
  }
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mGTC) &&
            ReadParam(aMsg, aIter, &aResult->mQPC) &&
            ReadParam(aMsg, aIter, &aResult->mUsedCanonicalNow) &&
            ReadParam(aMsg, aIter, &aResult->mIsNull) &&
            ReadParam(aMsg, aIter, &aResult->mHasQPC));
  }
};

#else

template <>
struct ParamTraits<mozilla::TimeStamp63Bit> {
  typedef mozilla::TimeStamp63Bit paramType;
  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mUsedCanonicalNow);
    WriteParam(aMsg, aParam.mTimeStamp);
  }
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    bool success = true;
    uint64_t result;

    success &= ReadParam(aMsg, aIter, &result);
    aResult->mUsedCanonicalNow = result & 0x01;

    success &= ReadParam(aMsg, aIter, &result);
    aResult->mTimeStamp = result & 0x7FFFFFFFFFFFFFFF;

    return success;
  }
};

#endif

template <>
struct ParamTraits<mozilla::dom::ipc::StructuredCloneData> {
  typedef mozilla::dom::ipc::StructuredCloneData paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    aParam.WriteIPCParams(aMsg);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return aResult->ReadIPCParams(aMsg, aIter);
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    LogParam(aParam.DataLength(), aLog);
  }
};

template <>
struct ParamTraits<mozilla::net::WebSocketFrameData> {
  typedef mozilla::net::WebSocketFrameData paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    aParam.WriteIPCParams(aMsg);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return aResult->ReadIPCParams(aMsg, aIter);
  }
};

template <>
struct ParamTraits<JSStructuredCloneData> {
  typedef JSStructuredCloneData paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    MOZ_ASSERT(!(aParam.Size() % sizeof(uint64_t)));
    WriteParam(aMsg, aParam.Size());
    aParam.ForEachDataChunk([&](const char* aData, size_t aSize) {
      return aMsg->WriteBytes(aData, aSize, sizeof(uint64_t));
    });
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    size_t length = 0;
    if (!ReadParam(aMsg, aIter, &length)) {
      return false;
    }
    MOZ_ASSERT(!(length % sizeof(uint64_t)));

    mozilla::BufferList<InfallibleAllocPolicy> buffers(0, 0, 4096);

    // Borrowing is not suitable to use for IPC to hand out data
    // because we often want to store the data somewhere for
    // processing after IPC has released the underlying buffers. One
    // case is PContentChild::SendGetXPCOMProcessAttributes. We can't
    // return a borrowed buffer because the out param outlives the
    // IPDL callback.
    if (length &&
        !aMsg->ExtractBuffers(aIter, length, &buffers, sizeof(uint64_t))) {
      return false;
    }

    bool success;
    mozilla::BufferList<js::SystemAllocPolicy> out =
        buffers.MoveFallible<js::SystemAllocPolicy>(&success);
    if (!success) {
      return false;
    }

    *aResult = JSStructuredCloneData(
        std::move(out), JS::StructuredCloneScope::DifferentProcess);

    return true;
  }
};

template <>
struct ParamTraits<mozilla::SerializedStructuredCloneBuffer> {
  typedef mozilla::SerializedStructuredCloneBuffer paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.data);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->data);
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    LogParam(aParam.data.Size(), aLog);
  }
};

template <>
struct ParamTraits<nsIWidget::TouchPointerState>
    : public BitFlagsEnumSerializer<nsIWidget::TouchPointerState,
                                    nsIWidget::TouchPointerState::ALL_BITS> {};

template <class T>
struct ParamTraits<mozilla::Maybe<T>> {
  typedef mozilla::Maybe<T> paramType;

  static void Write(Message* msg, const paramType& param) {
    if (param.isSome()) {
      WriteParam(msg, true);
      WriteParam(msg, param.value());
    } else {
      WriteParam(msg, false);
    }
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    bool isSome;
    if (!ReadParam(msg, iter, &isSome)) {
      return false;
    }
    if (isSome) {
      T tmp;
      if (!ReadParam(msg, iter, &tmp)) {
        return false;
      }
      *result = mozilla::Some(std::move(tmp));
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

  static void Write(Message* msg, const paramType& param) {
    MOZ_RELEASE_ASSERT(IsLegalValue(param.serialize()));
    WriteParam(msg, param.serialize());
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    serializedType tmp;

    if (ReadParam(msg, iter, &tmp)) {
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

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.tag);
    param.match([msg](const auto& t) { WriteParam(msg, t); });
  }

  // Because VariantReader is a nested struct, we need the dummy template
  // parameter to avoid making VariantReader<0> an explicit specialization,
  // which is not allowed for a nested class template
  template <size_t N, typename dummy = void>
  struct VariantReader {
    using Next = VariantReader<N - 1>;

    static bool Read(const Message* msg, PickleIterator* iter, Tag tag,
                     paramType* result) {
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
        return ReadParam(msg, iter, &result->template emplace<N - 1>());
      } else {
        return Next::Read(msg, iter, tag, result);
      }
    }

  };  // VariantReader<N>

  // Since we are conditioning on tag = N - 1 in the preceding specialization,
  // if we get to `VariantReader<0, dummy>` we have failed to find
  // a matching tag.
  template <typename dummy>
  struct VariantReader<0, dummy> {
    static bool Read(const Message* msg, PickleIterator* iter, Tag tag,
                     paramType* result) {
      return false;
    }
  };

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    Tag tag;
    if (ReadParam(msg, iter, &tag)) {
      return VariantReader<sizeof...(Ts)>::Read(msg, iter, tag, result);
    }
    return false;
  }
};

template <typename T>
struct ParamTraits<mozilla::dom::Optional<T>> {
  typedef mozilla::dom::Optional<T> paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    if (aParam.WasPassed()) {
      WriteParam(aMsg, true);
      WriteParam(aMsg, aParam.Value());
      return;
    }

    WriteParam(aMsg, false);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    bool wasPassed = false;

    if (!ReadParam(aMsg, aIter, &wasPassed)) {
      return false;
    }

    aResult->Reset();

    if (wasPassed) {
      if (!ReadParam(aMsg, aIter, &aResult->Construct())) {
        return false;
      }
    }

    return true;
  }
};

struct CrossOriginOpenerPolicyValidator {
  static bool IsLegalValue(nsILoadInfo::CrossOriginOpenerPolicy e) {
    return e == nsILoadInfo::OPENER_POLICY_UNSAFE_NONE ||
           e == nsILoadInfo::OPENER_POLICY_SAME_ORIGIN ||
           e == nsILoadInfo::OPENER_POLICY_SAME_ORIGIN_ALLOW_POPUPS ||
           e == nsILoadInfo::
                    OPENER_POLICY_SAME_ORIGIN_EMBEDDER_POLICY_REQUIRE_CORP;
  }
};

template <>
struct ParamTraits<nsILoadInfo::CrossOriginOpenerPolicy>
    : EnumSerializer<nsILoadInfo::CrossOriginOpenerPolicy,
                     CrossOriginOpenerPolicyValidator> {};

struct CrossOriginEmbedderPolicyValidator {
  static bool IsLegalValue(nsILoadInfo::CrossOriginEmbedderPolicy e) {
    return e == nsILoadInfo::EMBEDDER_POLICY_NULL ||
           e == nsILoadInfo::EMBEDDER_POLICY_REQUIRE_CORP;
  }
};

template <>
struct ParamTraits<nsILoadInfo::CrossOriginEmbedderPolicy>
    : EnumSerializer<nsILoadInfo::CrossOriginEmbedderPolicy,
                     CrossOriginEmbedderPolicyValidator> {};

// Helper class for reading bitfields.
// If T has bitfields members, derive ParamTraits<T> from BitfieldHelper<T>.
template <typename ParamType>
struct BitfieldHelper {
  // We need this helper because we can't get the address of a bitfield to
  // pass directly to ReadParam. So instead we read it into a temporary bool
  // and set the bitfield using a setter function
  static bool ReadBoolForBitfield(const Message* aMsg, PickleIterator* aIter,
                                  ParamType* aResult,
                                  void (ParamType::*aSetter)(bool)) {
    bool value;
    if (ReadParam(aMsg, aIter, &value)) {
      (aResult->*aSetter)(value);
      return true;
    }
    return false;
  }
};

// A couple of recursive helper functions, allows syntax like:
// WriteParams(aMsg, aParam.foo, aParam.bar, aParam.baz)
// ReadParams(aMsg, aIter, aParam.foo, aParam.bar, aParam.baz)

// Base case
void WriteParams(Message* aMsg);

template <typename T0, typename... Tn>
static void WriteParams(Message* aMsg, const T0& aArg,
                        const Tn&... aRemainingArgs) {
  WriteParam(aMsg, aArg);                // Write first arg
  WriteParams(aMsg, aRemainingArgs...);  // Recurse for the rest
}

// Base case
bool ReadParams(const Message* aMsg, PickleIterator* aIter);

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

#define DEFINE_IPC_SERIALIZER_WITHOUT_FIELDS(Type) \
  template <>                                      \
  struct ParamTraits<Type> : public EmptyStructSerializer<Type> {};

} /* namespace IPC */

#endif /* __IPC_GLUE_IPCMESSAGEUTILS_H__ */
