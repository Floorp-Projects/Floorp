/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __IPC_GLUE_IPCMESSAGEUTILS_H__
#define __IPC_GLUE_IPCMESSAGEUTILS_H__

#include <cstdint>
#include <string>
#include <type_traits>
#include "build/build_config.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/ipc_message_utils.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/ipc/IPCCore.h"

class PickleIterator;

// XXX Things that are not necessary if moving implementations to the cpp file
#include "base/string_util.h"

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
template <typename...>
class Variant;

namespace detail {
template <typename...>
struct VariantTag;
}
}  // namespace mozilla

namespace IPC {

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

template <typename... Ts>
static void WriteParams(Message* aMsg, const Ts&... aArgs) {
  (WriteParam(aMsg, aArgs), ...);
}

template <typename... Ts>
static bool ReadParams(const Message* aMsg, PickleIterator* aIter,
                       Ts&... aArgs) {
  return (ReadParam(aMsg, aIter, &aArgs) && ...);
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

#define DEFINE_IPC_SERIALIZER_WITH_SUPER_CLASS_AND_FIELDS(Type, Super, ...)  \
  template <>                                                                \
  struct ParamTraits<Type> {                                                 \
    typedef Type paramType;                                                  \
    static void Write(Message* aMsg, const paramType& aParam) {              \
      WriteParam(aMsg, static_cast<const Super&>(aParam));                   \
      WriteParams(aMsg, MOZ_FOR_EACH_SEPARATED(ACCESS_PARAM_FIELD, (, ), (), \
                                               (__VA_ARGS__)));              \
    }                                                                        \
                                                                             \
    static bool Read(const Message* aMsg, PickleIterator* aIter,             \
                     paramType* aResult) {                                   \
      paramType& aParam = *aResult;                                          \
      return ReadParam(aMsg, aIter, static_cast<Super*>(aResult)) &&         \
             ReadParams(aMsg, aIter,                                         \
                        MOZ_FOR_EACH_SEPARATED(ACCESS_PARAM_FIELD, (, ), (), \
                                               (__VA_ARGS__)));              \
    }                                                                        \
  };

#endif /* __IPC_GLUE_IPCMESSAGEUTILS_H__ */
