/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_DaemonSocketPDUHelpers_h
#define mozilla_ipc_DaemonSocketPDUHelpers_h

#include <stdint.h>
#include "mozilla/ipc/DaemonSocketPDU.h"
#include "nsString.h"

namespace mozilla {
namespace ipc {

struct DaemonSocketPDUHeader {
  DaemonSocketPDUHeader()
    : mService(0x00)
    , mOpcode(0x00)
    , mLength(0x00)
  { }

  DaemonSocketPDUHeader(uint8_t aService, uint8_t aOpcode, uint16_t aLength)
    : mService(aService)
    , mOpcode(aOpcode)
    , mLength(aLength)
  { }

  uint8_t mService;
  uint8_t mOpcode;
  uint16_t mLength;
};

namespace DaemonSocketPDUHelpers {

//
// Logging
//
// The HAL IPC logging macros below print clear error messages for
// failed IPC operations. Use |MOZ_HAL_IPC_CONVERT_WARN_IF|,
// |MOZ_HAL_IPC_PACK_WARN_IF| and |MOZ_HAL_IPC_UNPACK_WARN_IF| to
// test for failures when processing PDUs.
//
// All macros accept the test condition as their first argument, and
// additional type information: the convert macro takes the input and
// output types, the pack macro takes the input type, and the unpack
// macro takes output type. All macros return the result of the test
// condition. If the test fails (i.e., the condition is true), they
// output a warning to the log.
//
// Don't call the functions in the detail namespace. They are helpers
// and not for general use.
//

namespace detail {

void
LogProtocolError(const char*, ...);

inline bool
ConvertWarnIfImpl(const char* aFile, unsigned long aLine,
                  bool aCondition, const char* aExpr, const char* aIn,
                  const char* aOut)
{
  if (MOZ_UNLIKELY(aCondition)) {
    LogProtocolError("%s:%d: Convert('%s' to '%s') failed: %s",
                     aFile, aLine, aIn, aOut, aExpr);
  }
  return aCondition;
}

inline bool
PackWarnIfImpl(const char* aFile, unsigned long aLine,
               bool aCondition, const char* aExpr, const char* aIn)
{
  if (MOZ_UNLIKELY(aCondition)) {
    LogProtocolError("%s:%d: Pack('%s') failed: %s",
                     aFile, aLine, aIn, aExpr);
  }
  return aCondition;
}

inline bool
UnpackWarnIfImpl(const char* aFile, unsigned long aLine,
                 bool aCondition, const char* aExpr, const char* aOut)
{
  if (MOZ_UNLIKELY(aCondition)) {
    LogProtocolError("%s:%d: Unpack('%s') failed: %s",
                     aFile, aLine, aOut, aExpr);
  }
  return aCondition;
}

} // namespace detail

#define MOZ_HAL_IPC_CONVERT_WARN_IF(condition, in, out) \
  ::mozilla::ipc::DaemonSocketPDUHelpers::detail:: \
    ConvertWarnIfImpl(__FILE__, __LINE__, condition, #condition, #in, #out)

#define MOZ_HAL_IPC_PACK_WARN_IF(condition, in) \
  ::mozilla::ipc::DaemonSocketPDUHelpers::detail:: \
    PackWarnIfImpl(__FILE__, __LINE__, condition, #condition, #in)

#define MOZ_HAL_IPC_UNPACK_WARN_IF(condition, out) \
  ::mozilla::ipc::DaemonSocketPDUHelpers::detail:: \
    UnpackWarnIfImpl(__FILE__, __LINE__, condition, #condition, #out)

//
// Conversion
//
// PDUs can only store primitive data types, such as integers or
// byte arrays. Gecko often uses more complex data types, such as
// enumators or stuctures. Conversion functions convert between
// primitive data and internal Gecko's data types during a PDU's
// packing and unpacking.
//

nsresult
Convert(bool aIn, uint8_t& aOut);

nsresult
Convert(bool aIn, int32_t& aOut);

nsresult
Convert(int aIn, uint8_t& aOut);

nsresult
Convert(int aIn, int16_t& aOut);

nsresult
Convert(int aIn, int32_t& aOut);

nsresult
Convert(uint8_t aIn, bool& aOut);

nsresult
Convert(uint8_t aIn, char& aOut);

nsresult
Convert(uint8_t aIn, int& aOut);

nsresult
Convert(uint8_t aIn, unsigned long& aOut);

nsresult
Convert(uint32_t aIn, int& aOut);

nsresult
Convert(uint32_t aIn, uint8_t& aOut);

nsresult
Convert(size_t aIn, uint16_t& aOut);

//
// Packing
//

// introduce link errors on non-handled data types
template <typename T>
nsresult
PackPDU(T aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(bool aIn, DaemonSocketPDU& aPDU);

inline nsresult
PackPDU(uint8_t aIn, DaemonSocketPDU& aPDU)
{
  return aPDU.Write(aIn);
}

inline nsresult
PackPDU(uint16_t aIn, DaemonSocketPDU& aPDU)
{
  return aPDU.Write(aIn);
}

inline nsresult
PackPDU(int32_t aIn, DaemonSocketPDU& aPDU)
{
  return aPDU.Write(aIn);
}

inline nsresult
PackPDU(uint32_t aIn, DaemonSocketPDU& aPDU)
{
  return aPDU.Write(aIn);
}

nsresult
PackPDU(const DaemonSocketPDUHeader& aIn, DaemonSocketPDU& aPDU);

/* |PackConversion| is a helper for packing converted values. Pass
 * an instance of this structure to |PackPDU| to convert a value from
 * the input type to the output type and and write it to the PDU.
 */
template<typename Tin, typename Tout>
struct PackConversion {
  PackConversion(const Tin& aIn)
    : mIn(aIn)
  { }

  const Tin& mIn;
};

template<typename Tin, typename Tout>
inline nsresult
PackPDU(const PackConversion<Tin, Tout>& aIn, DaemonSocketPDU& aPDU)
{
  Tout out;

  nsresult rv = Convert(aIn.mIn, out);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return PackPDU(out, aPDU);
}

/* |PackArray| is a helper for packing arrays. Pass an instance
 * of this structure as the first argument to |PackPDU| to pack
 * an array. The array's maximum default length is 255 elements.
 */
template <typename T>
struct PackArray
{
  PackArray(const T* aData, size_t aLength)
    : mData(aData)
    , mLength(aLength)
  { }

  const T* mData;
  size_t mLength;
};

/* This implementation of |PackPDU| packs the length of an array
 * and the elements of the array one-by-one.
 */
template<typename T>
inline nsresult
PackPDU(const PackArray<T>& aIn, DaemonSocketPDU& aPDU)
{
  for (size_t i = 0; i < aIn.mLength; ++i) {
    nsresult rv = PackPDU(aIn.mData[i], aPDU);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

template<>
inline nsresult
PackPDU<uint8_t>(const PackArray<uint8_t>& aIn, DaemonSocketPDU& aPDU)
{
  /* Write raw bytes in one pass */
  return aPDU.Write(aIn.mData, aIn.mLength);
}

template<>
inline nsresult
PackPDU<char>(const PackArray<char>& aIn, DaemonSocketPDU& aPDU)
{
  /* Write raw bytes in one pass */
  return aPDU.Write(aIn.mData, aIn.mLength);
}

/* |PackCString0| is a helper for packing 0-terminated C string,
 * including the \0 character. Pass an instance of this structure
 * as the first argument to |PackPDU| to pack a string.
 */
struct PackCString0
{
  PackCString0(const nsCString& aString)
    : mString(aString)
  { }

  const nsCString& mString;
};

/* This implementation of |PackPDU| packs a 0-terminated C string.
 */
inline nsresult
PackPDU(const PackCString0& aIn, DaemonSocketPDU& aPDU)
{
  return PackPDU(
    PackArray<uint8_t>(reinterpret_cast<const uint8_t*>(aIn.mString.get()),
                       aIn.mString.Length() + 1), aPDU);
}

/* |PackReversed| is a helper for packing data in reversed order. Pass an
 * instance of this structure as the first argument to |PackPDU| to pack data
 * in reversed order.
 */
template<typename T>
struct PackReversed
{
  PackReversed(const T& aValue)
    : mValue(aValue)
  { }

  const T& mValue;
};

/* No general rules to pack data in reversed order. Signal a link error if the
 * type |T| of |PackReversed| is not defined explicitly.
 */
template<typename T>
nsresult
PackPDU(const PackReversed<T>& aIn, DaemonSocketPDU& aPDU);

/* This implementation of |PackPDU| packs elements in |PackArray| in reversed
 * order. (ex. reversed GATT UUID, see bug 1171866)
 */
template<typename U>
inline nsresult
PackPDU(const PackReversed<PackArray<U>>& aIn, DaemonSocketPDU& aPDU)
{
  for (size_t i = 0; i < aIn.mValue.mLength; ++i) {
    nsresult rv = PackPDU(aIn.mValue.mData[aIn.mValue.mLength - i - 1], aPDU);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

template <typename T1, typename T2>
inline nsresult
PackPDU(const T1& aIn1, const T2& aIn2, DaemonSocketPDU& aPDU)
{
  nsresult rv = PackPDU(aIn1, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return PackPDU(aIn2, aPDU);
}

template <typename T1, typename T2, typename T3>
inline nsresult
PackPDU(const T1& aIn1, const T2& aIn2, const T3& aIn3,
        DaemonSocketPDU& aPDU)
{
  nsresult rv = PackPDU(aIn1, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn2, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return PackPDU(aIn3, aPDU);
}

template <typename T1, typename T2, typename T3, typename T4>
inline nsresult
PackPDU(const T1& aIn1, const T2& aIn2, const T3& aIn3, const T4& aIn4,
        DaemonSocketPDU& aPDU)
{
  nsresult rv = PackPDU(aIn1, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn2, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn3, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return PackPDU(aIn4, aPDU);
}

template <typename T1, typename T2, typename T3,
          typename T4, typename T5>
inline nsresult
PackPDU(const T1& aIn1, const T2& aIn2, const T3& aIn3,
        const T4& aIn4, const T5& aIn5,
        DaemonSocketPDU& aPDU)
{
  nsresult rv = PackPDU(aIn1, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn2, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn3, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn4, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return PackPDU(aIn5, aPDU);
}

template <typename T1, typename T2, typename T3,
          typename T4, typename T5, typename T6>
inline nsresult
PackPDU(const T1& aIn1, const T2& aIn2, const T3& aIn3,
        const T4& aIn4, const T5& aIn5, const T6& aIn6,
        DaemonSocketPDU& aPDU)
{
  nsresult rv = PackPDU(aIn1, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn2, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn3, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn4, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn5, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return PackPDU(aIn6, aPDU);
}

template <typename T1, typename T2, typename T3,
          typename T4, typename T5, typename T6,
          typename T7>
inline nsresult
PackPDU(const T1& aIn1, const T2& aIn2, const T3& aIn3,
        const T4& aIn4, const T5& aIn5, const T6& aIn6,
        const T7& aIn7, DaemonSocketPDU& aPDU)
{
  nsresult rv = PackPDU(aIn1, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn2, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn3, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn4, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn5, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn6, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return PackPDU(aIn7, aPDU);
}

template <typename T1, typename T2, typename T3,
          typename T4, typename T5, typename T6,
          typename T7, typename T8>
inline nsresult
PackPDU(const T1& aIn1, const T2& aIn2, const T3& aIn3,
        const T4& aIn4, const T5& aIn5, const T6& aIn6,
        const T7& aIn7, const T8& aIn8, DaemonSocketPDU& aPDU)
{
  nsresult rv = PackPDU(aIn1, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn2, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn3, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn4, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn5, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn6, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn7, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return PackPDU(aIn8, aPDU);
}

template <typename T1, typename T2, typename T3,
          typename T4, typename T5, typename T6,
          typename T7, typename T8, typename T9,
          typename T10, typename T11, typename T12,
          typename T13>
inline nsresult
PackPDU(const T1& aIn1, const T2& aIn2, const T3& aIn3,
        const T4& aIn4, const T5& aIn5, const T6& aIn6,
        const T7& aIn7, const T8& aIn8, const T9& aIn9,
        const T10& aIn10, const T11& aIn11, const T12& aIn12,
        const T13& aIn13, DaemonSocketPDU& aPDU)
{
  nsresult rv = PackPDU(aIn1, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn2, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn3, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn4, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn5, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn6, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn7, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn8, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn9, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn10, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn11, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn12, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return PackPDU(aIn13, aPDU);
}

//
// Unpacking
//

// introduce link errors on non-handled data types
template <typename T>
nsresult
UnpackPDU(DaemonSocketPDU& aPDU, T& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, bool& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, char& aOut);

inline nsresult
UnpackPDU(DaemonSocketPDU& aPDU, int8_t& aOut)
{
  return aPDU.Read(aOut);
}

inline nsresult
UnpackPDU(DaemonSocketPDU& aPDU, uint8_t& aOut)
{
  return aPDU.Read(aOut);
}

inline nsresult
UnpackPDU(DaemonSocketPDU& aPDU, uint16_t& aOut)
{
  return aPDU.Read(aOut);
}

inline nsresult
UnpackPDU(DaemonSocketPDU& aPDU, int32_t& aOut)
{
  return aPDU.Read(aOut);
}

inline nsresult
UnpackPDU(DaemonSocketPDU& aPDU, uint32_t& aOut)
{
  return aPDU.Read(aOut);
}

inline nsresult
UnpackPDU(DaemonSocketPDU& aPDU, DaemonSocketPDUHeader& aOut)
{
  nsresult rv = UnpackPDU(aPDU, aOut.mService);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = UnpackPDU(aPDU, aOut.mOpcode);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return UnpackPDU(aPDU, aOut.mLength);
}

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, nsDependentCString& aOut);

/* |UnpackCString0| is a helper for unpacking 0-terminated C string,
 * including the \0 character. Pass an instance of this structure
 * as the first argument to |UnpackPDU| to unpack a string.
 */
struct UnpackCString0
{
  UnpackCString0(nsCString& aString)
    : mString(&aString)
  { }

  nsCString* mString; // non-null by construction
};

/* This implementation of |UnpackPDU| unpacks a 0-terminated C string.
 */
nsresult
UnpackPDU(DaemonSocketPDU& aPDU, const UnpackCString0& aOut);

/* |UnpackString0| is a helper for unpacking 0-terminated C string,
 * including the \0 character. Pass an instance of this structure
 * as the first argument to |UnpackPDU| to unpack a C string and convert
 * it to wide-character encoding.
 */
struct UnpackString0
{
  UnpackString0(nsString& aString)
    : mString(&aString)
  { }

  nsString* mString; // non-null by construction
};

/* This implementation of |UnpackPDU| unpacks a 0-terminated C string
 * and converts it to wide-character encoding.
 */
nsresult
UnpackPDU(DaemonSocketPDU& aPDU, const UnpackString0& aOut);

/* |UnpackConversion| is a helper for convering unpacked values. Pass
 * an instance of this structure to |UnpackPDU| to read a value from
 * the PDU in the input type and convert it to the output type.
 */
template<typename Tin, typename Tout>
struct UnpackConversion {
  UnpackConversion(Tout& aOut)
    : mOut(aOut)
  { }

  Tout& mOut;
};

template<typename Tin, typename Tout>
inline nsresult
UnpackPDU(DaemonSocketPDU& aPDU, const UnpackConversion<Tin, Tout>& aOut)
{
  Tin in;
  nsresult rv = UnpackPDU(aPDU, in);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return Convert(in, aOut.mOut);
}

/* |UnpackArray| is a helper for unpacking arrays. Pass an instance
 * of this structure as the second argument to |UnpackPDU| to unpack
 * an array.
 */
template <typename T>
struct UnpackArray
{
  UnpackArray(T* aData, size_t aLength)
    : mData(aData)
    , mLength(aLength)
  { }

  UnpackArray(nsAutoArrayPtr<T>& aData, size_t aLength)
    : mData(nullptr)
    , mLength(aLength)
  {
    aData = new T[mLength];
    mData = aData.get();
  }

  UnpackArray(nsAutoArrayPtr<T>& aData, size_t aSize, size_t aElemSize)
    : mData(nullptr)
    , mLength(aSize / aElemSize)
  {
    aData = new T[mLength];
    mData = aData.get();
  }

  T* mData;
  size_t mLength;
};

template<typename T>
inline nsresult
UnpackPDU(DaemonSocketPDU& aPDU, const UnpackArray<T>& aOut)
{
  for (size_t i = 0; i < aOut.mLength; ++i) {
    nsresult rv = UnpackPDU(aPDU, aOut.mData[i]);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

template<typename T>
inline nsresult
UnpackPDU(DaemonSocketPDU& aPDU, UnpackArray<T>& aOut)
{
  for (size_t i = 0; i < aOut.mLength; ++i) {
    nsresult rv = UnpackPDU(aPDU, aOut.mData[i]);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

template<>
inline nsresult
UnpackPDU<uint8_t>(DaemonSocketPDU& aPDU, const UnpackArray<uint8_t>& aOut)
{
  /* Read raw bytes in one pass */
  return aPDU.Read(aOut.mData, aOut.mLength);
}

template<typename T>
inline nsresult
UnpackPDU(DaemonSocketPDU& aPDU, nsTArray<T>& aOut)
{
  for (typename nsTArray<T>::size_type i = 0; i < aOut.Length(); ++i) {
    nsresult rv = UnpackPDU(aPDU, aOut[i]);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

/* |UnpackReversed| is a helper for unpacking data in reversed order. Pass an
 * instance of this structure as the second argument to |UnpackPDU| to unpack
 * data in reversed order.
 */
template<typename T>
struct UnpackReversed
{
  UnpackReversed(T& aValue)
    : mValue(&aValue)
  { }

  UnpackReversed(T&& aValue)
    : mValue(&aValue)
  { }

  T* mValue;
};

/* No general rules to unpack data in reversed order. Signal a link error if
 * the type |T| of |UnpackReversed| is not defined explicitly.
 */
template<typename T>
nsresult
UnpackPDU(DaemonSocketPDU& aPDU, const UnpackReversed<T>& aOut);

template<typename U>
inline nsresult
UnpackPDU(DaemonSocketPDU& aPDU, const UnpackReversed<UnpackArray<U>>& aOut)
{
  for (size_t i = 0; i < aOut.mValue->mLength; ++i) {
    nsresult rv = UnpackPDU(aPDU,
                            aOut.mValue->mData[aOut.mValue->mLength - i - 1]);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

//
// Init operators
//

//
// Below are general-purpose init operators for Bluetooth. The classes
// of type |ConstantInitOp[1..3]| initialize results or notifications
// with constant values.
//

template <typename T1>
class ConstantInitOp1 final
{
public:
  ConstantInitOp1(const T1& aArg1)
    : mArg1(aArg1)
  { }

  nsresult operator () (T1& aArg1) const
  {
    aArg1 = mArg1;

    return NS_OK;
  }

private:
  const T1& mArg1;
};

template <typename T1, typename T2>
class ConstantInitOp2 final
{
public:
  ConstantInitOp2(const T1& aArg1, const T2& aArg2)
    : mArg1(aArg1)
    , mArg2(aArg2)
  { }

  nsresult operator () (T1& aArg1, T2& aArg2) const
  {
    aArg1 = mArg1;
    aArg2 = mArg2;

    return NS_OK;
  }

private:
  const T1& mArg1;
  const T2& mArg2;
};

template <typename T1, typename T2, typename T3>
class ConstantInitOp3 final
{
public:
  ConstantInitOp3(const T1& aArg1, const T2& aArg2, const T3& aArg3)
    : mArg1(aArg1)
    , mArg2(aArg2)
    , mArg3(aArg3)
  { }

  nsresult operator () (T1& aArg1, T2& aArg2, T3& aArg3) const
  {
    aArg1 = mArg1;
    aArg2 = mArg2;
    aArg3 = mArg3;

    return NS_OK;
  }

private:
  const T1& mArg1;
  const T2& mArg2;
  const T3& mArg3;
};

// |PDUInitOP| provides functionality for init operators that unpack PDUs.
class PDUInitOp
{
protected:
  PDUInitOp(DaemonSocketPDU& aPDU)
    : mPDU(&aPDU)
  { }

  DaemonSocketPDU& GetPDU() const
  {
    return *mPDU; // cannot be nullptr
  }

  void WarnAboutTrailingData() const;

private:
  DaemonSocketPDU* mPDU; // Hold pointer to allow for constant instances
};

// |UnpackPDUInitOp| is a general-purpose init operator for all variants
// of |DaemonResultRunnable| and |DaemonNotificationRunnable|. The call
// operators of |UnpackPDUInitOp| unpack a PDU into the supplied
// arguments.
class UnpackPDUInitOp final : private PDUInitOp
{
public:
  UnpackPDUInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult operator () () const
  {
    WarnAboutTrailingData();
    return NS_OK;
  }

  template<typename T1>
  nsresult operator () (T1& aArg1) const
  {
    nsresult rv = UnpackPDU(GetPDU(), aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
    WarnAboutTrailingData();
    return NS_OK;
  }

  template<typename T1, typename T2>
  nsresult operator () (T1& aArg1, T2& aArg2) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = UnpackPDU(pdu, aArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
    WarnAboutTrailingData();
    return NS_OK;
  }

  template<typename T1, typename T2, typename T3>
  nsresult operator () (T1& aArg1, T2& aArg2, T3& aArg3) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = UnpackPDU(pdu, aArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = UnpackPDU(pdu, aArg3);
    if (NS_FAILED(rv)) {
      return rv;
    }
    WarnAboutTrailingData();
    return NS_OK;
  }

  template<typename T1, typename T2, typename T3, typename T4>
  nsresult operator () (T1& aArg1, T2& aArg2, T3& aArg3, T4& aArg4) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = UnpackPDU(pdu, aArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = UnpackPDU(pdu, aArg3);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = UnpackPDU(pdu, aArg4);
    if (NS_FAILED(rv)) {
      return rv;
    }
    WarnAboutTrailingData();
    return NS_OK;
  }

  template<typename T1, typename T2, typename T3, typename T4, typename T5>
  nsresult operator () (T1& aArg1, T2& aArg2, T3& aArg3, T4& aArg4,
                        T5& aArg5) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = UnpackPDU(pdu, aArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = UnpackPDU(pdu, aArg3);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = UnpackPDU(pdu, aArg4);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = UnpackPDU(pdu, aArg5);
    if (NS_FAILED(rv)) {
      return rv;
    }
    WarnAboutTrailingData();
    return NS_OK;
  }

  template<typename T1, typename T2, typename T3, typename T4,
           typename T5, typename T6>
  nsresult operator () (T1& aArg1, T2& aArg2, T3& aArg3, T4& aArg4,
                        T5& aArg5, T6& aArg6) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = UnpackPDU(pdu, aArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = UnpackPDU(pdu, aArg3);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = UnpackPDU(pdu, aArg4);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = UnpackPDU(pdu, aArg5);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = UnpackPDU(pdu, aArg6);
    if (NS_FAILED(rv)) {
      return rv;
    }
    WarnAboutTrailingData();
    return NS_OK;
  }
};

} // namespace DaemonSocketPDUHelpers

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_DaemonSocketPDUHelpers_h
