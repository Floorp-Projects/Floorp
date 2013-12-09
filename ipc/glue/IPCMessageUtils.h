/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __IPC_GLUE_IPCMESSAGEUTILS_H__
#define __IPC_GLUE_IPCMESSAGEUTILS_H__

#include "base/process_util.h"
#include "chrome/common/ipc_message_utils.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/TimeStamp.h"
#ifdef XP_WIN
#include "mozilla/TimeStamp_windows.h"
#endif

#include <stdint.h>

#include "nsID.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsTArray.h"
#include "js/StructuredClone.h"
#include "nsCSSProperty.h"

#ifdef _MSC_VER
#pragma warning( disable : 4800 )
#endif

#if !defined(OS_POSIX)
// This condition must be kept in sync with the one in
// ipc_message_utils.h, but this dummy definition of
// base::FileDescriptor acts as a static assert that we only get one
// def or the other (or neither, in which case code using
// FileDescriptor fails to build)
namespace base { struct FileDescriptor { }; }
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

struct SerializedStructuredCloneBuffer
{
  SerializedStructuredCloneBuffer()
  : data(nullptr), dataLength(0)
  { }

  SerializedStructuredCloneBuffer(const JSAutoStructuredCloneBuffer& aOther)
  {
    *this = aOther;
  }

  bool
  operator==(const SerializedStructuredCloneBuffer& aOther) const
  {
    return this->data == aOther.data &&
           this->dataLength == aOther.dataLength;
  }

  SerializedStructuredCloneBuffer&
  operator=(const JSAutoStructuredCloneBuffer& aOther)
  {
    data = aOther.data();
    dataLength = aOther.nbytes();
    return *this;
  }

  uint64_t* data;
  size_t dataLength;
};

} // namespace mozilla

namespace IPC {

/**
 * Generic enum serializer.
 *
 * This is a generic serializer for any enum type used in IPDL.
 * Programmers can define ParamTraits<E> for enum type E by deriving
 * EnumSerializer<E, smallestLegal, highGuard>.
 *
 * The serializer would check value againts a range specified by
 * smallestLegal and highGuard.  Only values from smallestLegal to
 * highGuard are valid, include smallestLegal but highGuard.
 *
 * For example, following is definition of serializer for enum type FOO.
 * \code
 * enum FOO { FOO_FIRST, FOO_SECOND, FOO_LAST, NUM_FOO };
 *
 * template <>
 * struct ParamTraits<FOO>:
 *     public EnumSerializer<FOO, FOO_FIRST, NUM_FOO> {};
 * \endcode
 * FOO_FIRST, FOO_SECOND, and FOO_LAST are valid value.
 *
 * \sa https://developer.mozilla.org/en/IPDL/Type_Serialization
 */
template <typename E, E smallestLegal, E highBound>
struct EnumSerializer {
  typedef E paramType;

  static bool IsLegalValue(const paramType &aValue) {
    return smallestLegal <= aValue && aValue < highBound;
  }

  static void Write(Message* aMsg, const paramType& aValue) {
    MOZ_ASSERT(IsLegalValue(aValue));
    WriteParam(aMsg, (int32_t)aValue);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult) {
    int32_t value;
    if(!ReadParam(aMsg, aIter, &value) ||
       !IsLegalValue(paramType(value))) {
      return false;
    }
    *aResult = paramType(value);
    return true;
  }
};

template <>
struct ParamTraits<base::ChildPrivileges>
  : public EnumSerializer<base::ChildPrivileges,
                          base::PRIVILEGES_DEFAULT,
                          base::PRIVILEGES_LAST>
{ };

template<>
struct ParamTraits<int8_t>
{
  typedef int8_t paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteBytes(&aParam, sizeof(aParam));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    const char* outp;
    if (!aMsg->ReadBytes(aIter, &outp, sizeof(*aResult)))
      return false;

    *aResult = *reinterpret_cast<const paramType*>(outp);
    return true;
  }
};

template<>
struct ParamTraits<uint8_t>
{
  typedef uint8_t paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteBytes(&aParam, sizeof(aParam));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    const char* outp;
    if (!aMsg->ReadBytes(aIter, &outp, sizeof(*aResult)))
      return false;

    *aResult = *reinterpret_cast<const paramType*>(outp);
    return true;
  }
};

#if !defined(OS_POSIX)
// See above re: keeping definitions in sync
template<>
struct ParamTraits<base::FileDescriptor>
{
  typedef base::FileDescriptor paramType;
  static void Write(Message* aMsg, const paramType& aParam) {
    NS_RUNTIMEABORT("FileDescriptor isn't meaningful on this platform");
  }
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult) {
    NS_RUNTIMEABORT("FileDescriptor isn't meaningful on this platform");
    return false;
  }
};
#endif  // !defined(OS_POSIX)

template <>
struct ParamTraits<nsACString>
{
  typedef nsACString paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    bool isVoid = aParam.IsVoid();
    aMsg->WriteBool(isVoid);

    if (isVoid)
      // represents a nullptr pointer
      return;

    uint32_t length = aParam.Length();
    WriteParam(aMsg, length);
    aMsg->WriteBytes(aParam.BeginReading(), length);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    bool isVoid;
    if (!aMsg->ReadBool(aIter, &isVoid))
      return false;

    if (isVoid) {
      aResult->SetIsVoid(true);
      return true;
    }

    uint32_t length;
    if (ReadParam(aMsg, aIter, &length)) {
      const char* buf;
      if (aMsg->ReadBytes(aIter, &buf, length)) {
        aResult->Assign(buf, length);
        return true;
      }
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    if (aParam.IsVoid())
      aLog->append(L"(NULL)");
    else
      aLog->append(UTF8ToWide(aParam.BeginReading()));
  }
};

template <>
struct ParamTraits<nsAString>
{
  typedef nsAString paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    bool isVoid = aParam.IsVoid();
    aMsg->WriteBool(isVoid);

    if (isVoid)
      // represents a nullptr pointer
      return;

    uint32_t length = aParam.Length();
    WriteParam(aMsg, length);
    aMsg->WriteBytes(aParam.BeginReading(), length * sizeof(PRUnichar));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    bool isVoid;
    if (!aMsg->ReadBool(aIter, &isVoid))
      return false;

    if (isVoid) {
      aResult->SetIsVoid(true);
      return true;
    }

    uint32_t length;
    if (ReadParam(aMsg, aIter, &length)) {
      const PRUnichar* buf;
      if (aMsg->ReadBytes(aIter, reinterpret_cast<const char**>(&buf),
                       length * sizeof(PRUnichar))) {
        aResult->Assign(buf, length);
        return true;
      }
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
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
struct ParamTraits<nsCString> : ParamTraits<nsACString>
{
  typedef nsCString paramType;
};

#ifdef MOZILLA_INTERNAL_API

template<>
struct ParamTraits<nsAutoCString> : ParamTraits<nsCString>
{
  typedef nsAutoCString paramType;
};

#endif  // MOZILLA_INTERNAL_API

template <>
struct ParamTraits<nsString> : ParamTraits<nsAString>
{
  typedef nsString paramType;
};

template <typename E>
struct ParamTraits<FallibleTArray<E> >
{
  typedef FallibleTArray<E> paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    uint32_t length = aParam.Length();
    WriteParam(aMsg, length);
    for (uint32_t index = 0; index < length; index++) {
      WriteParam(aMsg, aParam[index]);
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    uint32_t length;
    if (!ReadParam(aMsg, aIter, &length)) {
      return false;
    }

    aResult->SetCapacity(length);
    for (uint32_t index = 0; index < length; index++) {
      E* element = aResult->AppendElement();
      if (!(element && ReadParam(aMsg, aIter, element))) {
        return false;
      }
    }

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    for (uint32_t index = 0; index < aParam.Length(); index++) {
      if (index) {
        aLog->append(L" ");
      }
      LogParam(aParam[index], aLog);
    }
  }
};

template<typename E>
struct ParamTraits<InfallibleTArray<E> >
{
  typedef InfallibleTArray<E> paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<const FallibleTArray<E>&>(aParam));
  }

  // deserialize the array fallibly, but return an InfallibleTArray
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    FallibleTArray<E> temp;
    if (!ReadParam(aMsg, aIter, &temp))
      return false;

    aResult->SwapElements(temp);
    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    LogParam(static_cast<const FallibleTArray<E>&>(aParam), aLog);
  }
};

template<>
struct ParamTraits<float>
{
  typedef float paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteBytes(&aParam, sizeof(paramType));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    const char* outFloat;
    if (!aMsg->ReadBytes(aIter, &outFloat, sizeof(float)))
      return false;
    *aResult = *reinterpret_cast<const float*>(outFloat);
    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"%g", aParam));
  }
};

template <>
struct ParamTraits<nsCSSProperty>
  : public EnumSerializer<nsCSSProperty,
                          eCSSProperty_UNKNOWN,
                          eCSSProperty_COUNT>
{};

template<>
struct ParamTraits<mozilla::void_t>
{
  typedef mozilla::void_t paramType;
  static void Write(Message* aMsg, const paramType& aParam) { }
  static bool
  Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    *aResult = paramType();
    return true;
  }
};

template<>
struct ParamTraits<mozilla::null_t>
{
  typedef mozilla::null_t paramType;
  static void Write(Message* aMsg, const paramType& aParam) { }
  static bool
  Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    *aResult = paramType();
    return true;
  }
};

template<>
struct ParamTraits<nsID>
{
  typedef nsID paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.m0);
    WriteParam(aMsg, aParam.m1);
    WriteParam(aMsg, aParam.m2);
    for (unsigned int i = 0; i < mozilla::ArrayLength(aParam.m3); i++) {
      WriteParam(aMsg, aParam.m3[i]);
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if(!ReadParam(aMsg, aIter, &(aResult->m0)) ||
       !ReadParam(aMsg, aIter, &(aResult->m1)) ||
       !ReadParam(aMsg, aIter, &(aResult->m2)))
      return false;

    for (unsigned int i = 0; i < mozilla::ArrayLength(aResult->m3); i++)
      if (!ReadParam(aMsg, aIter, &(aResult->m3[i])))
        return false;

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(L"{");
    aLog->append(StringPrintf(L"%8.8X-%4.4X-%4.4X-",
                              aParam.m0,
                              aParam.m1,
                              aParam.m2));
    for (unsigned int i = 0; i < mozilla::ArrayLength(aParam.m3); i++)
      aLog->append(StringPrintf(L"%2.2X", aParam.m3[i]));
    aLog->append(L"}");
  }
};

template<>
struct ParamTraits<mozilla::TimeDuration>
{
  typedef mozilla::TimeDuration paramType;
  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mValue);
  }
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mValue);
  };
};

template<>
struct ParamTraits<mozilla::TimeStamp>
{
  typedef mozilla::TimeStamp paramType;
  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mValue);
  }
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mValue);
  };
};

#ifdef XP_WIN

template<>
struct ParamTraits<mozilla::TimeStampValue>
{
  typedef mozilla::TimeStampValue paramType;
  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mGTC);
    WriteParam(aMsg, aParam.mQPC);
    WriteParam(aMsg, aParam.mHasQPC);
    WriteParam(aMsg, aParam.mIsNull);
  }
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mGTC) &&
            ReadParam(aMsg, aIter, &aResult->mQPC) &&
            ReadParam(aMsg, aIter, &aResult->mHasQPC) &&
            ReadParam(aMsg, aIter, &aResult->mIsNull));
  }
};

#endif

template <>
struct ParamTraits<mozilla::SerializedStructuredCloneBuffer>
{
  typedef mozilla::SerializedStructuredCloneBuffer paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.dataLength);
    if (aParam.dataLength) {
      // Structured clone data must be 64-bit aligned.
      aMsg->WriteBytes(aParam.data, aParam.dataLength, sizeof(uint64_t));
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &aResult->dataLength)) {
      return false;
    }

    if (aResult->dataLength) {
      const char** buffer =
        const_cast<const char**>(reinterpret_cast<char**>(&aResult->data));
      // Structured clone data must be 64-bit aligned.
      if (!aMsg->ReadBytes(aIter, buffer, aResult->dataLength,
                           sizeof(uint64_t))) {
        return false;
      }
    } else {
      aResult->data = nullptr;
    }

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    LogParam(aParam.dataLength, aLog);
  }
};

} /* namespace IPC */

#endif /* __IPC_GLUE_IPCMESSAGEUTILS_H__ */
