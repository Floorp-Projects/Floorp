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

#if MOZ_IS_GCC && MOZ_GCC_VERSION_AT_LEAST(4, 7, 0)
/* use designated array initializers if supported */
#define CONVERT(in_, out_) \
  [in_] = out_
#else
/* otherwise init array element by position */
#define CONVERT(in_, out_) \
  out_
#endif

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
// PDUs can only store primitive data types, such as intergers or
// strings. Gecko often uses more complex data types, such as
// enumators or stuctures. Conversion functions convert between
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

template<typename T1, uint8_t Service, uint8_t Opcode>
inline nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, T1& aArg1)
{
  return UnpackPDU(aPDU, aArg1);
}

template<typename T1, typename T2, uint8_t Service, uint8_t Opcode>
inline nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, T1& aArg1, T2& aArg2)
{
  nsresult rv = UnpackPDU(aPDU, aArg1);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return UnpackPDU(aPDU, aArg2);
}

template<typename T1, typename T2, typename T3,
         uint8_t Service, uint8_t Opcode>
inline nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, T1& aArg1, T2& aArg2, T3& aArg3)
{
  nsresult rv = UnpackPDU(aPDU, aArg1);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = UnpackPDU(aPDU, aArg2);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return UnpackPDU(aPDU, aArg3);
}

template<typename T1, typename T2, typename T3, typename T4,
         uint8_t Service, uint8_t Opcode>
inline nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, T1& aArg1, T2& aArg2, T3& aArg3, T4& aArg4)
{
  nsresult rv = UnpackPDU(aPDU, aArg1);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = UnpackPDU(aPDU, aArg2);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = UnpackPDU(aPDU, aArg3);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return UnpackPDU(aPDU, aArg4);
}

template<typename T1, typename T2, typename T3, typename T4, typename T5,
         uint8_t Service, uint8_t Opcode>
inline nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU,
          T1& aArg1, T2& aArg2, T3& aArg3, T4& aArg4, T5& aArg5)
{
  nsresult rv = UnpackPDU(aPDU, aArg1);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = UnpackPDU(aPDU, aArg2);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = UnpackPDU(aPDU, aArg3);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = UnpackPDU(aPDU, aArg4);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return UnpackPDU(aPDU, aArg5);
}

//
// Result handling
//

template <typename Obj, typename Res>
class BluetoothDaemonInterfaceRunnable0 : public nsRunnable
{
public:
  typedef BluetoothDaemonInterfaceRunnable0<Obj, Res> SelfType;

  static already_AddRefed<SelfType> Create(
    Obj* aObj, Res (Obj::*aMethod)())
  {
    nsRefPtr<SelfType> runnable(new SelfType(aObj, aMethod));

    return runnable.forget();
  }

  static void
  Dispatch(Obj* aObj, Res (Obj::*aMethod)())
  {
    nsRefPtr<SelfType> runnable = Create(aObj, aMethod);
    if (!runnable) {
      BT_WARNING("BluetoothDaemonInterfaceRunnable0::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    ((*mObj).*mMethod)();
    return NS_OK;
  }

private:
  BluetoothDaemonInterfaceRunnable0(Obj* aObj, Res (Obj::*aMethod)())
  : mObj(aObj)
  , mMethod(aMethod)
  {
    MOZ_ASSERT(mObj);
    MOZ_ASSERT(mMethod);
  }

  nsRefPtr<Obj> mObj;
  void (Obj::*mMethod)();
};

template <typename Obj, typename Res, typename Tin1, typename Arg1>
class BluetoothDaemonInterfaceRunnable1 : public nsRunnable
{
public:
  typedef BluetoothDaemonInterfaceRunnable1<Obj, Res, Tin1, Arg1> SelfType;

  template<uint8_t Service, uint8_t Opcode>
  static already_AddRefed<SelfType> Create(
    Obj* aObj, Res (Obj::*aMethod)(Arg1), BluetoothDaemonPDU& aPDU)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aObj, aMethod));

    if (NS_FAILED((runnable->UnpackAndSet<Service, Opcode>(aPDU)))) {
      return nullptr;
    }
    return runnable.forget();
  }

  static already_AddRefed<SelfType> Create(
    Obj* aObj, Res (Obj::*aMethod)(Arg1), const Arg1& aArg1)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aObj, aMethod, aArg1));
    return runnable.forget();
  }

  template<uint8_t Service, uint8_t Opcode>
  static void
  Dispatch(Obj* aObj, Res (Obj::*aMethod)(Arg1),
           BluetoothDaemonPDU& aPDU)
  {
    nsRefPtr<SelfType> runnable = Create<Service, Opcode>(aObj, aMethod, aPDU);
    if (!runnable) {
      BT_WARNING("BluetoothDaemonInterfaceRunnable1::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  static void
  Dispatch(Obj* aObj, Res (Obj::*aMethod)(Arg1), const Tin1& aArg1)
  {
    nsRefPtr<SelfType> runnable = Create(aObj, aMethod, aArg1);
    if (!runnable) {
      BT_WARNING("BluetoothDaemonInterfaceRunnable1::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    ((*mObj).*mMethod)(mArg1);
    return NS_OK;
  }

private:
  BluetoothDaemonInterfaceRunnable1(Obj* aObj, Res (Obj::*aMethod)(Arg1))
  : mObj(aObj)
  , mMethod(aMethod)
  {
    MOZ_ASSERT(mObj);
    MOZ_ASSERT(mMethod);
  }

  BluetoothDaemonInterfaceRunnable1(Obj* aObj, Res (Obj::*aMethod)(Arg1),
                                    const Tin1& aArg1)
  : mObj(aObj)
  , mMethod(aMethod)
  , mArg1(aArg1)
  {
    MOZ_ASSERT(mObj);
    MOZ_ASSERT(mMethod);
  }

  template<uint8_t Service, uint8_t Opcode>
  nsresult
  UnpackAndSet(BluetoothDaemonPDU& aPDU)
  {
    return UnpackPDU<Tin1, Service, Opcode>(aPDU, mArg1);
  }

  nsRefPtr<Obj> mObj;
  Res (Obj::*mMethod)(Arg1);
  Tin1 mArg1;
};

template <typename Obj, typename Res,
          typename Tin1, typename Tin2, typename Tin3,
          typename Arg1, typename Arg2, typename Arg3>
class BluetoothDaemonInterfaceRunnable3 : public nsRunnable
{
public:
  typedef BluetoothDaemonInterfaceRunnable3<Obj, Res,
                                            Tin1, Tin2, Tin3,
                                            Arg1, Arg2, Arg3> SelfType;

  template<uint8_t Service, uint8_t Opcode>
  static already_AddRefed<SelfType> Create(
    Obj* aObj, Res (Obj::*aMethod)(Arg1, Arg2, Arg3), BluetoothDaemonPDU& aPDU)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aObj, aMethod));

    if (NS_FAILED((runnable->UnpackAndSet<Service, Opcode>(aPDU)))) {
      return nullptr;
    }
    return runnable.forget();
  }

  static already_AddRefed<SelfType> Create(
    Obj* aObj, Res (Obj::*aMethod)(Arg1, Arg2, Arg3),
    const Tin1& aArg1, const Tin2& aArg2, const Tin3& aArg3)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aObj, aMethod,
                                             aArg1, aArg2, aArg3));
    return runnable.forget();
  }

  template<uint8_t Service, uint8_t Opcode>
  static void
  Dispatch(Obj* aObj, Res (Obj::*aMethod)(Arg1, Arg2, Arg3),
           BluetoothDaemonPDU& aPDU)
  {
    nsRefPtr<SelfType> runnable = Create<Service, Opcode>(aObj, aMethod, aPDU);
    if (!runnable) {
      BT_WARNING("BluetoothDaemonInterfaceRunnable3::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  static void
  Dispatch(Obj* aObj, Res (Obj::*aMethod)(Arg1, Arg2, Arg3),
           const Tin1& aArg1, const Tin2& aArg2, const Tin3& aArg3)
  {
    nsRefPtr<SelfType> runnable = Create(aObj, aMethod, aArg1, aArg2, aArg3);
    if (!runnable) {
      BT_WARNING("BluetoothDaemonInterfaceRunnable3::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    ((*mObj).*mMethod)(mArg1, mArg2, mArg3);
    return NS_OK;
  }

private:
  BluetoothDaemonInterfaceRunnable3(Obj* aObj,
                                    Res (Obj::*aMethod)(Arg1, Arg2, Arg3))
  : mObj(aObj)
  , mMethod(aMethod)
  {
    MOZ_ASSERT(mObj);
    MOZ_ASSERT(mMethod);
  }

  BluetoothDaemonInterfaceRunnable3(Obj* aObj,
                                    Res (Obj::*aMethod)(Arg1, Arg2, Arg3),
                                    const Tin1& aArg1, const Tin2& aArg2,
                                    const Tin3& aArg3)
  : mObj(aObj)
  , mMethod(aMethod)
  , mArg1(aArg1)
  , mArg2(aArg2)
  , mArg3(aArg3)
  {
    MOZ_ASSERT(mObj);
    MOZ_ASSERT(mMethod);
  }

  template<uint8_t Service, uint8_t Opcode>
  nsresult
  UnpackAndSet(BluetoothDaemonPDU& aPDU)
  {
    return UnpackPDU<Tin1, Tin2, Tin3, Service, Opcode>(aPDU,
                                                        mArg1, mArg2,
                                                        mArg3);
  }

  nsRefPtr<Obj> mObj;
  Res (Obj::*mMethod)(Arg1, Arg2, Arg3);
  Tin1 mArg1;
  Tin2 mArg2;
  Tin3 mArg3;
};

//
// Notification handling
//

template <typename ObjectWrapper, typename Res>
class BluetoothDaemonNotificationRunnable0 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothDaemonNotificationRunnable0<ObjectWrapper, Res> SelfType;

  static already_AddRefed<SelfType> Create(Res (ObjectType::*aMethod)())
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));

    return runnable.forget();
  }

  static void
  Dispatch(Res (ObjectType::*aMethod)())
  {
    nsRefPtr<SelfType> runnable = Create(aMethod);

    if (!runnable) {
      BT_WARNING("BluetoothDaemonNotificationRunnable0::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();

    if (!obj) {
      BT_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)();
    }
    return NS_OK;
  }

private:
  BluetoothDaemonNotificationRunnable0(Res (ObjectType::*aMethod)())
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  Res (ObjectType::*mMethod)();
};

template <typename ObjectWrapper, typename Res,
          typename Tin1, typename Arg1=Tin1>
class BluetoothDaemonNotificationRunnable1 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothDaemonNotificationRunnable1<ObjectWrapper, Res,
                                               Tin1, Arg1> SelfType;

  template<uint8_t Service, uint8_t Opcode>
  static already_AddRefed<SelfType> Create(
    Res (ObjectType::*aMethod)(Arg1), BluetoothDaemonPDU& aPDU)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));

    if (NS_FAILED((runnable->UnpackAndSet<Service, Opcode>(aPDU)))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template<uint8_t Service, uint8_t Opcode>
  static void
  Dispatch(Res (ObjectType::*aMethod)(Arg1), BluetoothDaemonPDU& aPDU)
  {
    nsRefPtr<SelfType> runnable = Create<Service, Opcode>(aMethod, aPDU);

    if (!runnable) {
      BT_WARNING("BluetoothDaemonNotificationRunnable1::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();

    if (!obj) {
      BT_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(mArg1);
    }
    return NS_OK;
  }

private:
  BluetoothDaemonNotificationRunnable1(Res (ObjectType::*aMethod)(Arg1))
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<uint8_t Service, uint8_t Opcode>
  nsresult
  UnpackAndSet(BluetoothDaemonPDU& aPDU)
  {
    return UnpackPDU<Tin1, Service, Opcode>(aPDU, mArg1);
  }

  Res (ObjectType::*mMethod)(Arg1);
  Tin1 mArg1;
};

template <typename ObjectWrapper, typename Res,
          typename Tin1, typename Tin2,
          typename Arg1=Tin1, typename Arg2=Tin2>
class BluetoothDaemonNotificationRunnable2 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothDaemonNotificationRunnable2<ObjectWrapper, Res,
                                               Tin1, Tin2,
                                               Arg1, Arg2> SelfType;

  template<uint8_t Service, uint8_t Opcode>
  static already_AddRefed<SelfType> Create(
    Res (ObjectType::*aMethod)(Arg1, Arg2), BluetoothDaemonPDU& aPDU)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));

    if (NS_FAILED((runnable->UnpackAndSet<Service, Opcode>(aPDU)))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template<uint8_t Service, uint8_t Opcode>
  static void
  Dispatch(Res (ObjectType::*aMethod)(Arg1, Arg2),
           BluetoothDaemonPDU& aPDU)
  {
    nsRefPtr<SelfType> runnable = Create<Service, Opcode>(aMethod, aPDU);

    if (!runnable) {
      BT_WARNING("BluetoothDaemonNotificationRunnable2::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();

    if (!obj) {
      BT_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(mArg1, mArg2);
    }
    return NS_OK;
  }

private:
  BluetoothDaemonNotificationRunnable2(
    Res (ObjectType::*aMethod)(Arg1, Arg2))
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<uint8_t Service, uint8_t Opcode>
  nsresult
  UnpackAndSet(BluetoothDaemonPDU& aPDU)
  {
    return UnpackPDU<Tin1, Tin2, Service, Opcode>(aPDU, mArg1, mArg2);
  }

  Res (ObjectType::*mMethod)(Arg1, Arg2);
  Tin1 mArg1;
  Tin2 mArg2;
};

template <typename ObjectWrapper, typename Res,
          typename Tin1, typename Tin2, typename Tin3,
          typename Arg1=Tin1, typename Arg2=Tin2, typename Arg3=Tin3>
class BluetoothDaemonNotificationRunnable3 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothDaemonNotificationRunnable3<ObjectWrapper, Res,
                                               Tin1, Tin2, Tin3,
                                               Arg1, Arg2, Arg3> SelfType;

  template<uint8_t Service, uint8_t Opcode>
  static already_AddRefed<SelfType> Create(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3),
    BluetoothDaemonPDU& aPDU)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));

    if (NS_FAILED((runnable->UnpackAndSet<Service, Opcode>(aPDU)))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template<uint8_t Service, uint8_t Opcode>
  static void
  Dispatch(Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3),
           BluetoothDaemonPDU& aPDU)
  {
    nsRefPtr<SelfType> runnable = Create<Service, Opcode>(aMethod, aPDU);
    if (!runnable) {
      BT_WARNING("BluetoothDaemonNotificationRunnable3::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();

    if (!obj) {
      BT_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(mArg1, mArg2, mArg3);
    }
    return NS_OK;
  }

private:
  BluetoothDaemonNotificationRunnable3(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3))
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<uint8_t Service, uint8_t Opcode>
  nsresult
  UnpackAndSet(BluetoothDaemonPDU& aPDU)
  {
    return UnpackPDU<Tin1, Tin2, Tin3, Service, Opcode>(aPDU,
                                                        mArg1, mArg2,
                                                        mArg3);
  }

  Res (ObjectType::*mMethod)(Arg1, Arg2, Arg3);
  Tin1 mArg1;
  Tin2 mArg2;
  Tin3 mArg3;
};

template <typename ObjectWrapper, typename Res,
          typename Tin1, typename Tin2, typename Tin3, typename Tin4,
          typename Arg1=Tin1, typename Arg2=Tin2,
          typename Arg3=Tin3, typename Arg4=Tin4>
class BluetoothDaemonNotificationRunnable4 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothDaemonNotificationRunnable4<ObjectWrapper, Res,
    Tin1, Tin2, Tin3, Tin4, Arg1, Arg2, Arg3, Arg4> SelfType;

  template<uint8_t Service, uint8_t Opcode>
  static already_AddRefed<SelfType> Create(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4),
    BluetoothDaemonPDU& aPDU)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));

    if (NS_FAILED((runnable->UnpackAndSet<Service, Opcode>(aPDU)))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template<uint8_t Service, uint8_t Opcode>
  static void
  Dispatch(Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4),
           BluetoothDaemonPDU& aPDU)
  {
    nsRefPtr<SelfType> runnable = Create<Service, Opcode>(aMethod, aPDU);
    if (!runnable) {
      BT_WARNING("BluetoothDaemonNotificationRunnable4::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();

    if (!obj) {
      BT_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(mArg1, mArg2, mArg3, mArg4);
    }
    return NS_OK;
  }

private:
  BluetoothDaemonNotificationRunnable4(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4))
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<uint8_t Service, uint8_t Opcode>
  nsresult
  UnpackAndSet(BluetoothDaemonPDU& aPDU)
  {
    return UnpackPDU<Tin1, Tin2, Tin3, Tin4, Service, Opcode>(aPDU,
                                                              mArg1, mArg2,
                                                              mArg3, mArg4);
  }

  Res (ObjectType::*mMethod)(Arg1, Arg2, Arg3, Arg4);
  Tin1 mArg1;
  Tin2 mArg2;
  Tin3 mArg3;
  Tin4 mArg4;
};

template <typename ObjectWrapper, typename Res,
          typename Tin1, typename Tin2, typename Tin3,
          typename Tin4, typename Tin5,
          typename Arg1=Tin1, typename Arg2=Tin2, typename Arg3=Tin3,
          typename Arg4=Tin4, typename Arg5=Tin5>
class BluetoothDaemonNotificationRunnable5 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothDaemonNotificationRunnable5<ObjectWrapper, Res,
    Tin1, Tin2, Tin3, Tin4, Tin5, Arg1, Arg2, Arg3, Arg4, Arg5> SelfType;

  template<uint8_t Service, uint8_t Opcode>
  static already_AddRefed<SelfType> Create(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4, Arg5),
    BluetoothDaemonPDU& aPDU)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));

    if (NS_FAILED((runnable->UnpackAndSet<Service, Opcode>(aPDU)))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template<uint8_t Service, uint8_t Opcode>
  static void
  Dispatch(Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4, Arg5),
           BluetoothDaemonPDU& aPDU)
  {
    nsRefPtr<SelfType> runnable = Create<Service, Opcode>(aMethod, aPDU);
    if (!runnable) {
      BT_WARNING("BluetoothDaemonNotificationRunnable5::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();

    if (!obj) {
      BT_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(mArg1, mArg2, mArg3, mArg4, mArg5);
    }
    return NS_OK;
  }

private:
  BluetoothDaemonNotificationRunnable5(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4, Arg5))
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<uint8_t Service, uint8_t Opcode>
  nsresult
  UnpackAndSet(BluetoothDaemonPDU& aPDU)
  {
    return UnpackPDU<Tin1, Tin2, Tin3, Tin4, Tin5, Service, Opcode>(aPDU,
                                                                    mArg1,
                                                                    mArg2,
                                                                    mArg3,
                                                                    mArg4,
                                                                    mArg5);
  }

  Res (ObjectType::*mMethod)(Arg1, Arg2, Arg3, Arg4, Arg5);
  Tin1 mArg1;
  Tin2 mArg2;
  Tin3 mArg3;
  Tin4 mArg4;
  Tin5 mArg5;
};

END_BLUETOOTH_NAMESPACE

#endif
