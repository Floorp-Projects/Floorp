/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_bluetoothdaemonhelpers_h__
#define mozilla_dom_bluetooth_bluedroid_bluetoothdaemonhelpers_h__

#include "BluetoothCommon.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/ipc/BluetoothDaemonConnection.h"
#include "nsThreadUtils.h"

using namespace mozilla::ipc;

BEGIN_BLUETOOTH_NAMESPACE

struct BluetoothConfigurationParameter {
  uint8_t mType;
  uint16_t mLength;
  nsAutoArrayPtr<uint8_t> mValue;
};

struct BluetoothDaemonPDUHeader {
  BluetoothDaemonPDUHeader()
  : mService(0x00)
  , mOpcode(0x00)
  , mLength(0x00)
  { }

  BluetoothDaemonPDUHeader(uint8_t aService, uint8_t aOpcode, uint8_t aLength)
  : mService(aService)
  , mOpcode(aOpcode)
  , mLength(aLength)
  { }

  uint8_t mService;
  uint8_t mOpcode;
  uint16_t mLength;
};

//
// Conversion
//
// PDUs can only store primitive data types, such as integers or
// strings. Gecko often uses more complex data types, such as
// enumerators or structures. Conversion functions convert between
// primitive data and internal Gecko's data types during a PDU's
// packing and unpacking.
//

nsresult
Convert(uint8_t aIn, BluetoothStatus& aOut);

//
// Packing
//

inline nsresult
PackPDU(uint8_t aIn, BluetoothDaemonPDU& aPDU)
{
  return aPDU.Write(aIn);
}

inline nsresult
PackPDU(uint16_t aIn, BluetoothDaemonPDU& aPDU)
{
  return aPDU.Write(aIn);
}

nsresult
PackPDU(const BluetoothConfigurationParameter& aIn, BluetoothDaemonPDU& aPDU);

nsresult
PackPDU(const BluetoothDaemonPDUHeader& aIn, BluetoothDaemonPDU& aPDU);

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
PackPDU(const PackArray<T>& aIn, BluetoothDaemonPDU& aPDU)
{
  for (size_t i = 0; i < aIn.mLength; ++i) {
    nsresult rv = PackPDU(aIn.mData[i], aPDU);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

template <typename T1, typename T2>
inline nsresult
PackPDU(const T1& aIn1, const T2& aIn2, BluetoothDaemonPDU& aPDU)
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
        BluetoothDaemonPDU& aPDU)
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
        BluetoothDaemonPDU& aPDU)
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

//
// Unpacking
//

inline nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, uint8_t& aOut)
{
  return aPDU.Read(aOut);
}

inline nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, uint16_t& aOut)
{
  return aPDU.Read(aOut);
}

inline nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothDaemonPDUHeader& aOut)
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
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothStatus& aOut);

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
UnpackPDU(BluetoothDaemonPDU& aPDU, const UnpackConversion<Tin, Tout>& aOut)
{
  Tin in;
  nsresult rv = UnpackPDU(aPDU, in);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return Convert(in, aOut.mOut);
}

//
// Init operators
//

// |PDUInitOP| provides functionality for init operators that unpack PDUs.
class PDUInitOp
{
protected:
  PDUInitOp(BluetoothDaemonPDU& aPDU)
  : mPDU(&aPDU)
  { }

  BluetoothDaemonPDU& GetPDU() const
  {
    return *mPDU; // cannot be nullptr
  }

  void WarnAboutTrailingData() const
  {
    size_t size = mPDU->GetSize();

    if (MOZ_LIKELY(!size)) {
      return;
    }

    uint8_t service, opcode;
    uint16_t payloadSize;
    mPDU->GetHeader(service, opcode, payloadSize);

    BT_LOGR("Unpacked PDU of type (%x,%x) still contains %zu Bytes of data.",
            service, opcode, size);
  }

private:
  BluetoothDaemonPDU* mPDU; // Hold pointer to allow for constant instances
};

// |UnpackPDUInitOp| is a general-purpose init operator for all variants
// of |BluetoothResultRunnable| and |BluetoothNotificationRunnable|. The
// call operators of |UnpackPDUInitOp| unpack a PDU into the supplied
// arguments.
class UnpackPDUInitOp MOZ_FINAL : private PDUInitOp
{
public:
  UnpackPDUInitOp(BluetoothDaemonPDU& aPDU)
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
    BluetoothDaemonPDU& pdu = GetPDU();

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
    BluetoothDaemonPDU& pdu = GetPDU();

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
    BluetoothDaemonPDU& pdu = GetPDU();

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
    BluetoothDaemonPDU& pdu = GetPDU();

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
};

END_BLUETOOTH_NAMESPACE

#endif
