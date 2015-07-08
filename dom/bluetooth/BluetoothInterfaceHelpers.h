/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothinterfacehelpers_h
#define mozilla_dom_bluetooth_bluetoothinterfacehelpers_h

#include "BluetoothCommon.h"
#include "nsThreadUtils.h"

BEGIN_BLUETOOTH_NAMESPACE

//
// Conversion
//

nsresult
Convert(nsresult aIn, BluetoothStatus& aOut);

//
// Result handling
//
// The classes of type |BluetoothResultRunnable[0..3]| transfer
// a result handler from the I/O thread to the main thread for
// execution. Call the methods |Create| and |Dispatch| to create or
// create-and-dispatch a result runnable.
//
// You need to specify the called method. The |Create| and |Dispatch|
// methods of |BluetoothResultRunnable[1..3]| receive an extra argument
// for initializing the result's arguments. During creation, the result
// runnable calls the supplied class's call operator with the result's
// argument. This is where initialization and conversion from backend-
// specific types is performed.
//

template <typename Obj, typename Res>
class BluetoothResultRunnable0 : public nsRunnable
{
public:
  typedef BluetoothResultRunnable0<Obj, Res> SelfType;

  template <typename InitOp>
  static already_AddRefed<SelfType>
  Create(Obj* aObj, Res (Obj::*aMethod)(), const InitOp& aInitOp)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aObj, aMethod));
    if (NS_FAILED(runnable->Init(aInitOp))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template <typename InitOp>
  static void
  Dispatch(Obj* aObj, Res (Obj::*aMethod)(), const InitOp& aInitOp)
  {
    if (!aObj) {
      return; // silently return if no result runnable has been given
    }
    nsRefPtr<SelfType> runnable = Create(aObj, aMethod, aInitOp);
    if (!runnable) {
      BT_LOGR("BluetoothResultRunnable0::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_LOGR("NS_DispatchToMainThread failed: %X", unsigned(rv));
    }
  }

  NS_METHOD
  Run() override
  {
    ((*mObj).*mMethod)();
    return NS_OK;
  }

private:
  BluetoothResultRunnable0(Obj* aObj, Res (Obj::*aMethod)())
  : mObj(aObj)
  , mMethod(aMethod)
  {
    MOZ_ASSERT(mObj);
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult
  Init(const InitOp& aInitOp)
  {
    return aInitOp();
  }

  nsRefPtr<Obj> mObj;
  void (Obj::*mMethod)();
};

template <typename Obj, typename Res, typename Tin1, typename Arg1>
class BluetoothResultRunnable1 : public nsRunnable
{
public:
  typedef BluetoothResultRunnable1<Obj, Res, Tin1, Arg1> SelfType;

  template <typename InitOp>
  static already_AddRefed<SelfType>
  Create(Obj* aObj, Res (Obj::*aMethod)(Arg1), const InitOp& aInitOp)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aObj, aMethod));
    if (NS_FAILED(runnable->Init(aInitOp))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template <typename InitOp>
  static void
  Dispatch(Obj* aObj, Res (Obj::*aMethod)(Arg1), const InitOp& aInitOp)
  {
    if (!aObj) {
      return; // silently return if no result runnable has been given
    }
    nsRefPtr<SelfType> runnable = Create(aObj, aMethod, aInitOp);
    if (!runnable) {
      BT_LOGR("BluetoothResultRunnable1::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_LOGR("NS_DispatchToMainThread failed: %X", unsigned(rv));
    }
  }

  NS_METHOD
  Run() override
  {
    ((*mObj).*mMethod)(mArg1);
    return NS_OK;
  }

private:
  BluetoothResultRunnable1(Obj* aObj, Res (Obj::*aMethod)(Arg1))
  : mObj(aObj)
  , mMethod(aMethod)
  {
    MOZ_ASSERT(mObj);
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult
  Init(const InitOp& aInitOp)
  {
    return aInitOp(mArg1);
  }

  nsRefPtr<Obj> mObj;
  Res (Obj::*mMethod)(Arg1);
  Tin1 mArg1;
};

template <typename Obj, typename Res,
          typename Tin1, typename Tin2, typename Tin3,
          typename Arg1, typename Arg2, typename Arg3>
class BluetoothResultRunnable3 : public nsRunnable
{
public:
  typedef BluetoothResultRunnable3<Obj, Res,
                                   Tin1, Tin2, Tin3,
                                   Arg1, Arg2, Arg3> SelfType;

  template<typename InitOp>
  static already_AddRefed<SelfType>
  Create(Obj* aObj, Res (Obj::*aMethod)(Arg1, Arg2, Arg3),
         const InitOp& aInitOp)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aObj, aMethod));
    if (NS_FAILED(runnable->Init(aInitOp))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template<typename InitOp>
  static void
  Dispatch(Obj* aObj, Res (Obj::*aMethod)(Arg1, Arg2, Arg3),
           const InitOp& aInitOp)
  {
    if (!aObj) {
      return; // silently return if no result runnable has been given
    }
    nsRefPtr<SelfType> runnable = Create(aObj, aMethod, aInitOp);
    if (!runnable) {
      BT_LOGR("BluetoothResultRunnable3::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() override
  {
    ((*mObj).*mMethod)(mArg1, mArg2, mArg3);
    return NS_OK;
  }

private:
  BluetoothResultRunnable3(Obj* aObj, Res (Obj::*aMethod)(Arg1, Arg2, Arg3))
  : mObj(aObj)
  , mMethod(aMethod)
  {
    MOZ_ASSERT(mObj);
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult
  Init(const InitOp& aInitOp)
  {
    return aInitOp(mArg1, mArg2, mArg3);
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
// The classes of type |BluetoothNotificationRunnable[0..5]| transfer
// a notification from the I/O thread to a notification handler on the
// main thread. Call the methods |Create| and |Dispatch| to create or
// create-and-dispatch a notification runnable.
//
// Like with result runnables, you need to specify the called method.
// And like with result runnables, the |Create| and |Dispatch| methods
// of |BluetoothNotificationRunnable[1..5]| receive an extra argument
// for initializing the notification's arguments. During creation, the
// notification runnable calls the class's call operator with the
// notification's argument. This is where initialization and conversion
// from backend-specific types is performed.
//

template <typename ObjectWrapper, typename Res>
class BluetoothNotificationRunnable0 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationRunnable0<ObjectWrapper, Res> SelfType;

  template<typename InitOp>
  static already_AddRefed<SelfType>
  Create(Res (ObjectType::*aMethod)(), const InitOp& aInitOp)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));
    if (NS_FAILED(runnable->Init(aInitOp))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template<typename InitOp>
  static void
  Dispatch(Res (ObjectType::*aMethod)(), const InitOp& aInitOp)
  {
    nsRefPtr<SelfType> runnable = Create(aMethod, aInitOp);
    if (!runnable) {
      BT_WARNING("BluetoothNotificationRunnable0::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() override
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
  BluetoothNotificationRunnable0(Res (ObjectType::*aMethod)())
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult
  Init(const InitOp& aInitOp)
  {
    return aInitOp();
  }

  Res (ObjectType::*mMethod)();
};

template <typename ObjectWrapper, typename Res,
          typename Tin1, typename Arg1=Tin1>
class BluetoothNotificationRunnable1 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationRunnable1<ObjectWrapper, Res,
                                         Tin1, Arg1> SelfType;

  template <typename InitOp>
  static already_AddRefed<SelfType>
  Create(Res (ObjectType::*aMethod)(Arg1), const InitOp& aInitOp)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));
    if (NS_FAILED(runnable->Init(aInitOp))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template <typename InitOp>
  static void
  Dispatch(Res (ObjectType::*aMethod)(Arg1), const InitOp& aInitOp)
  {
    nsRefPtr<SelfType> runnable = Create(aMethod, aInitOp);

    if (!runnable) {
      BT_WARNING("BluetoothNotificationRunnable1::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() override
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
  BluetoothNotificationRunnable1(Res (ObjectType::*aMethod)(Arg1))
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult
  Init(const InitOp& aInitOp)
  {
    nsresult rv = aInitOp(mArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
  }

  Res (ObjectType::*mMethod)(Arg1);
  Tin1 mArg1;
};

template <typename ObjectWrapper, typename Res,
          typename Tin1, typename Tin2,
          typename Arg1=Tin1, typename Arg2=Tin2>
class BluetoothNotificationRunnable2 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationRunnable2<ObjectWrapper, Res,
                                         Tin1, Tin2,
                                         Arg1, Arg2> SelfType;

  template <typename InitOp>
  static already_AddRefed<SelfType>
  Create(Res (ObjectType::*aMethod)(Arg1, Arg2), const InitOp& aInitOp)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));
    if (NS_FAILED(runnable->Init(aInitOp))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template <typename InitOp>
  static void
  Dispatch(Res (ObjectType::*aMethod)(Arg1, Arg2), const InitOp& aInitOp)
  {
    nsRefPtr<SelfType> runnable = Create(aMethod, aInitOp);
    if (!runnable) {
      BT_WARNING("BluetoothNotificationRunnable2::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() override
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
  BluetoothNotificationRunnable2(
    Res (ObjectType::*aMethod)(Arg1, Arg2))
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult
  Init(const InitOp& aInitOp)
  {
    nsresult rv = aInitOp(mArg1, mArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
  }

  Res (ObjectType::*mMethod)(Arg1, Arg2);
  Tin1 mArg1;
  Tin2 mArg2;
};

template <typename ObjectWrapper, typename Res,
          typename Tin1, typename Tin2, typename Tin3,
          typename Arg1=Tin1, typename Arg2=Tin2, typename Arg3=Tin3>
class BluetoothNotificationRunnable3 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationRunnable3<ObjectWrapper, Res,
                                             Tin1, Tin2, Tin3,
                                             Arg1, Arg2, Arg3> SelfType;

  template <typename InitOp>
  static already_AddRefed<SelfType>
  Create(Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3), const InitOp& aInitOp)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));
    if (NS_FAILED(runnable->Init(aInitOp))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template <typename InitOp>
  static void
  Dispatch(Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3),
           const InitOp& aInitOp)
  {
    nsRefPtr<SelfType> runnable = Create(aMethod, aInitOp);
    if (!runnable) {
      BT_WARNING("BluetoothNotificationRunnable3::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() override
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
  BluetoothNotificationRunnable3(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3))
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult
  Init(const InitOp& aInitOp)
  {
    nsresult rv = aInitOp(mArg1, mArg2, mArg3);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
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
class BluetoothNotificationRunnable4 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationRunnable4<ObjectWrapper, Res,
    Tin1, Tin2, Tin3, Tin4, Arg1, Arg2, Arg3, Arg4> SelfType;

  template <typename InitOp>
  static already_AddRefed<SelfType> Create(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4),
    const InitOp& aInitOp)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));
    if (NS_FAILED(runnable->Init(aInitOp))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template <typename InitOp>
  static void
  Dispatch(Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4),
           const InitOp& aInitOp)
  {
    nsRefPtr<SelfType> runnable = Create(aMethod, aInitOp);
    if (!runnable) {
      BT_WARNING("BluetoothNotificationRunnable4::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() override
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
  BluetoothNotificationRunnable4(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4))
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult
  Init(const InitOp& aInitOp)
  {
    nsresult rv = aInitOp(mArg1, mArg2, mArg3, mArg4);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
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
class BluetoothNotificationRunnable5 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationRunnable5<ObjectWrapper, Res,
    Tin1, Tin2, Tin3, Tin4, Tin5, Arg1, Arg2, Arg3, Arg4, Arg5> SelfType;

  template <typename InitOp>
  static already_AddRefed<SelfType> Create(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4, Arg5),
    const InitOp& aInitOp)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));
    if (NS_FAILED(runnable->Init(aInitOp))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template <typename InitOp>
  static void
  Dispatch(Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4, Arg5),
           const InitOp& aInitOp)
  {
    nsRefPtr<SelfType> runnable = Create(aMethod, aInitOp);
    if (!runnable) {
      BT_WARNING("BluetoothNotificationRunnable5::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() override
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
  BluetoothNotificationRunnable5(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4, Arg5))
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult
  Init(const InitOp& aInitOp)
  {
    nsresult rv = aInitOp(mArg1, mArg2, mArg3, mArg4, mArg5);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
  }

  Res (ObjectType::*mMethod)(Arg1, Arg2, Arg3, Arg4, Arg5);
  Tin1 mArg1;
  Tin2 mArg2;
  Tin3 mArg3;
  Tin4 mArg4;
  Tin5 mArg5;
};

template <typename ObjectWrapper, typename Res,
          typename Tin1, typename Tin2, typename Tin3,
          typename Tin4, typename Tin5, typename Tin6,
          typename Arg1=Tin1, typename Arg2=Tin2, typename Arg3=Tin3,
          typename Arg4=Tin4, typename Arg5=Tin5, typename Arg6=Tin6>
class BluetoothNotificationRunnable6 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationRunnable6<ObjectWrapper, Res,
    Tin1, Tin2, Tin3, Tin4, Tin5, Tin6, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6>
    SelfType;

  template <typename InitOp>
  static already_AddRefed<SelfType> Create(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6),
    const InitOp& aInitOp)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));
    if (NS_FAILED(runnable->Init(aInitOp))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template <typename InitOp>
  static void
  Dispatch(Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6),
           const InitOp& aInitOp)
  {
    nsRefPtr<SelfType> runnable = Create(aMethod, aInitOp);
    if (!runnable) {
      BT_WARNING("BluetoothNotificationRunnable6::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();
    if (!obj) {
      BT_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(mArg1, mArg2, mArg3, mArg4, mArg5, mArg6);
    }
    return NS_OK;
  }

private:
  BluetoothNotificationRunnable6(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6))
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult
  Init(const InitOp& aInitOp)
  {
    nsresult rv = aInitOp(mArg1, mArg2, mArg3, mArg4, mArg5, mArg6);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
  }

  Res (ObjectType::*mMethod)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6);
  Tin1 mArg1;
  Tin2 mArg2;
  Tin3 mArg3;
  Tin4 mArg4;
  Tin5 mArg5;
  Tin6 mArg6;
};

template <typename ObjectWrapper, typename Res,
          typename Tin1, typename Tin2, typename Tin3,
          typename Tin4, typename Tin5, typename Tin6,
          typename Tin7, typename Tin8, typename Tin9,
          typename Arg1=Tin1, typename Arg2=Tin2, typename Arg3=Tin3,
          typename Arg4=Tin4, typename Arg5=Tin5, typename Arg6=Tin6,
          typename Arg7=Tin7, typename Arg8=Tin8, typename Arg9=Tin9>
class BluetoothNotificationRunnable9 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationRunnable9<ObjectWrapper, Res,
    Tin1, Tin2, Tin3, Tin4, Tin5, Tin6, Tin7, Tin8, Tin9,
    Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9> SelfType;

  template <typename InitOp>
  static already_AddRefed<SelfType> Create(
    Res (ObjectType::*aMethod)(
      Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9),
    const InitOp& aInitOp)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));
    if (NS_FAILED(runnable->Init(aInitOp))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template <typename InitOp>
  static void
  Dispatch(
    Res (ObjectType::*aMethod)(
      Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9),
    const InitOp& aInitOp)
  {
    nsRefPtr<SelfType> runnable = Create(aMethod, aInitOp);
    if (!runnable) {
      BT_WARNING("BluetoothNotificationRunnable8::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();
    if (!obj) {
      BT_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(mArg1, mArg2, mArg3, mArg4,
                        mArg5, mArg6, mArg7, mArg8, mArg9);
    }
    return NS_OK;
  }

private:
  BluetoothNotificationRunnable9(
    Res (ObjectType::*aMethod)(
      Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9))
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult
  Init(const InitOp& aInitOp)
  {
    nsresult rv = aInitOp(mArg1, mArg2, mArg3, mArg4,
                          mArg5, mArg6, mArg7, mArg8, mArg9);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
  }

  Res (ObjectType::*mMethod)(
    Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9);
  Tin1 mArg1;
  Tin2 mArg2;
  Tin3 mArg3;
  Tin4 mArg4;
  Tin5 mArg5;
  Tin6 mArg6;
  Tin7 mArg7;
  Tin8 mArg8;
  Tin9 mArg9;
};


//
// Init operators
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

END_BLUETOOTH_NAMESPACE

#endif
