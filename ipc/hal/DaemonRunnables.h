/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_DaemonRunnables_h
#define mozilla_ipc_DaemonRunnables_h

#include "mozilla/Unused.h"
#include "mozilla/UniquePtr.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace ipc {

namespace details {

class DaemonRunnable : public Runnable
{
protected:
  DaemonRunnable() = default;
  virtual ~DaemonRunnable() = default;

  template<typename Out, typename In>
  static Out& ConvertArg(In& aArg)
  {
    return aArg;
  }

  template<typename Out, typename In>
  static Out ConvertArg(UniquePtr<In>& aArg)
  {
    return aArg.get();
  }
};

} // namespace detail

//
// Result handling
//
// The classes of type |DaemonResultRunnable[0..3]| transfer a result
// handler from the I/O thread to the main thread for execution. Call
// the methods |Create| and |Dispatch| to create or create-and-dispatch
// a result runnable.
//
// You need to specify the called method. The |Create| and |Dispatch|
// methods of |DaemonResultRunnable[1..3]| receive an extra argument
// for initializing the result's arguments. During creation, the result
// runnable calls the supplied class's call operator with the result's
// argument. This is where initialization and conversion from backend-
// specific types is performed.
//

template <typename Obj, typename Res>
class DaemonResultRunnable0 final : public details::DaemonRunnable
{
public:
  typedef DaemonResultRunnable0<Obj, Res> SelfType;

  template <typename InitOp>
  static already_AddRefed<SelfType>
  Create(Obj* aObj, Res (Obj::*aMethod)(), const InitOp& aInitOp)
  {
    RefPtr<SelfType> runnable(new SelfType(aObj, aMethod));
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
    RefPtr<SelfType> runnable = Create(aObj, aMethod, aInitOp);
    if (!runnable) {
      return;
    }
    Unused << NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(runnable)));
  }

  NS_IMETHOD Run() override
  {
    ((*mObj).*mMethod)();
    return NS_OK;
  }

private:
  DaemonResultRunnable0(Obj* aObj, Res (Obj::*aMethod)())
    : mObj(aObj)
    , mMethod(aMethod)
  {
    MOZ_ASSERT(mObj);
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult Init(const InitOp& aInitOp)
  {
    return aInitOp();
  }

  RefPtr<Obj> mObj;
  void (Obj::*mMethod)();
};

template <typename Obj, typename Res, typename Tin1, typename Arg1>
class DaemonResultRunnable1 final : public details::DaemonRunnable
{
public:
  typedef DaemonResultRunnable1<Obj, Res, Tin1, Arg1> SelfType;

  template <typename InitOp>
  static already_AddRefed<SelfType>
  Create(Obj* aObj, Res (Obj::*aMethod)(Arg1), const InitOp& aInitOp)
  {
    RefPtr<SelfType> runnable(new SelfType(aObj, aMethod));
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
    RefPtr<SelfType> runnable = Create(aObj, aMethod, aInitOp);
    if (!runnable) {
      return;
    }
    Unused << NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(runnable)));
  }

  NS_IMETHOD Run() override
  {
    ((*mObj).*mMethod)(ConvertArg<Arg1>(mArg1));
    return NS_OK;
  }

private:
  DaemonResultRunnable1(Obj* aObj, Res (Obj::*aMethod)(Arg1))
    : mObj(aObj)
    , mMethod(aMethod)
  {
    MOZ_ASSERT(mObj);
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult Init(const InitOp& aInitOp)
  {
    return aInitOp(mArg1);
  }

  RefPtr<Obj> mObj;
  Res (Obj::*mMethod)(Arg1);
  Tin1 mArg1;
};

template <typename Obj, typename Res,
          typename Tin1, typename Tin2, typename Tin3,
          typename Arg1, typename Arg2, typename Arg3>
class DaemonResultRunnable3 final : public details::DaemonRunnable
{
public:
  typedef DaemonResultRunnable3<Obj, Res,
                                Tin1, Tin2, Tin3,
                                Arg1, Arg2, Arg3> SelfType;

  template<typename InitOp>
  static already_AddRefed<SelfType>
  Create(Obj* aObj, Res (Obj::*aMethod)(Arg1, Arg2, Arg3),
         const InitOp& aInitOp)
  {
    RefPtr<SelfType> runnable(new SelfType(aObj, aMethod));
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
    RefPtr<SelfType> runnable = Create(aObj, aMethod, aInitOp);
    if (!runnable) {
      return;
    }
    Unused << NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(runnable)));
  }

  NS_IMETHOD Run() override
  {
    ((*mObj).*mMethod)(ConvertArg<Arg1>(mArg1),
                       ConvertArg<Arg2>(mArg2),
                       ConvertArg<Arg3>(mArg3));
    return NS_OK;
  }

private:
  DaemonResultRunnable3(Obj* aObj, Res (Obj::*aMethod)(Arg1, Arg2, Arg3))
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

  RefPtr<Obj> mObj;
  Res (Obj::*mMethod)(Arg1, Arg2, Arg3);
  Tin1 mArg1;
  Tin2 mArg2;
  Tin3 mArg3;
};

//
// Notification handling
//
// The classes of type |DaemonNotificationRunnable[0..9]| transfer a
// notification from the I/O thread to a notification handler on the
// main thread. Call the methods |Create| and |Dispatch| to create or
// create-and-dispatch a notification runnable.
//
// Like with result runnables, you need to specify the called method.
// And like with result runnables, the |Create| and |Dispatch| methods
// of |DaemonNotificationRunnable[1..9]| receive an extra argument
// for initializing the notification's arguments. During creation, the
// notification runnable calls the class's call operator with the
// notification's argument. This is where initialization and conversion
// from backend-specific types is performed.
//

template <typename ObjectWrapper, typename Res>
class DaemonNotificationRunnable0 final : public details::DaemonRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef DaemonNotificationRunnable0<ObjectWrapper, Res> SelfType;

  template<typename InitOp>
  static already_AddRefed<SelfType>
  Create(Res (ObjectType::*aMethod)(), const InitOp& aInitOp)
  {
    RefPtr<SelfType> runnable(new SelfType(aMethod));
    if (NS_FAILED(runnable->Init(aInitOp))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template<typename InitOp>
  static void
  Dispatch(Res (ObjectType::*aMethod)(), const InitOp& aInitOp)
  {
    RefPtr<SelfType> runnable = Create(aMethod, aInitOp);
    if (!runnable) {
      return;
    }
    Unused << NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(runnable)));
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();
    if (!obj) {
      NS_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)();
    }
    return NS_OK;
  }

private:
  DaemonNotificationRunnable0(Res (ObjectType::*aMethod)())
    : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult Init(const InitOp& aInitOp)
  {
    return aInitOp();
  }

  Res (ObjectType::*mMethod)();
};

template <typename ObjectWrapper, typename Res,
          typename Tin1, typename Arg1=Tin1>
class DaemonNotificationRunnable1 final : public details::DaemonRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef DaemonNotificationRunnable1<ObjectWrapper, Res,
                                      Tin1, Arg1> SelfType;

  template <typename InitOp>
  static already_AddRefed<SelfType>
  Create(Res (ObjectType::*aMethod)(Arg1), const InitOp& aInitOp)
  {
    RefPtr<SelfType> runnable(new SelfType(aMethod));
    if (NS_FAILED(runnable->Init(aInitOp))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template <typename InitOp>
  static void
  Dispatch(Res (ObjectType::*aMethod)(Arg1), const InitOp& aInitOp)
  {
    RefPtr<SelfType> runnable = Create(aMethod, aInitOp);
    if (!runnable) {
      return;
    }
    Unused << NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(runnable)));
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();
    if (!obj) {
      NS_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(ConvertArg<Arg1>(mArg1));
    }
    return NS_OK;
  }

private:
  DaemonNotificationRunnable1(Res (ObjectType::*aMethod)(Arg1))
    : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult Init(const InitOp& aInitOp)
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
class DaemonNotificationRunnable2 final : public details::DaemonRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef DaemonNotificationRunnable2<ObjectWrapper, Res,
                                      Tin1, Tin2,
                                      Arg1, Arg2> SelfType;

  template <typename InitOp>
  static already_AddRefed<SelfType>
  Create(Res (ObjectType::*aMethod)(Arg1, Arg2), const InitOp& aInitOp)
  {
    RefPtr<SelfType> runnable(new SelfType(aMethod));
    if (NS_FAILED(runnable->Init(aInitOp))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template <typename InitOp>
  static void
  Dispatch(Res (ObjectType::*aMethod)(Arg1, Arg2), const InitOp& aInitOp)
  {
    RefPtr<SelfType> runnable = Create(aMethod, aInitOp);
    if (!runnable) {
      return;
    }
    Unused << NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(runnable)));
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();
    if (!obj) {
      NS_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(ConvertArg<Arg1>(mArg1),
                        ConvertArg<Arg2>(mArg2));
    }
    return NS_OK;
  }

private:
  DaemonNotificationRunnable2(
    Res (ObjectType::*aMethod)(Arg1, Arg2))
    : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult Init(const InitOp& aInitOp)
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
class DaemonNotificationRunnable3 final : public details::DaemonRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef DaemonNotificationRunnable3<ObjectWrapper, Res,
                                      Tin1, Tin2, Tin3,
                                      Arg1, Arg2, Arg3> SelfType;

  template <typename InitOp>
  static already_AddRefed<SelfType>
  Create(Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3), const InitOp& aInitOp)
  {
    RefPtr<SelfType> runnable(new SelfType(aMethod));
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
    RefPtr<SelfType> runnable = Create(aMethod, aInitOp);
    if (!runnable) {
      return;
    }
    Unused << NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(runnable)));
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();
    if (!obj) {
      NS_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(ConvertArg<Arg1>(mArg1),
                        ConvertArg<Arg2>(mArg2),
                        ConvertArg<Arg3>(mArg3));
    }
    return NS_OK;
  }

private:
  DaemonNotificationRunnable3(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3))
    : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult Init(const InitOp& aInitOp)
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
class DaemonNotificationRunnable4 final : public details::DaemonRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef DaemonNotificationRunnable4<ObjectWrapper, Res,
    Tin1, Tin2, Tin3, Tin4, Arg1, Arg2, Arg3, Arg4> SelfType;

  template <typename InitOp>
  static already_AddRefed<SelfType> Create(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4),
    const InitOp& aInitOp)
  {
    RefPtr<SelfType> runnable(new SelfType(aMethod));
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
    RefPtr<SelfType> runnable = Create(aMethod, aInitOp);
    if (!runnable) {
      return;
    }
    Unused << NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(runnable)));
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();
    if (!obj) {
      NS_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(ConvertArg<Arg1>(mArg1),
                        ConvertArg<Arg2>(mArg2),
                        ConvertArg<Arg3>(mArg3),
                        ConvertArg<Arg4>(mArg4));
    }
    return NS_OK;
  }

private:
  DaemonNotificationRunnable4(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4))
    : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult Init(const InitOp& aInitOp)
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
class DaemonNotificationRunnable5 final : public details::DaemonRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef DaemonNotificationRunnable5<ObjectWrapper, Res,
    Tin1, Tin2, Tin3, Tin4, Tin5, Arg1, Arg2, Arg3, Arg4, Arg5> SelfType;

  template <typename InitOp>
  static already_AddRefed<SelfType> Create(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4, Arg5),
    const InitOp& aInitOp)
  {
    RefPtr<SelfType> runnable(new SelfType(aMethod));
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
    RefPtr<SelfType> runnable = Create(aMethod, aInitOp);
    if (!runnable) {
      return;
    }
    Unused << NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(runnable)));
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();
    if (!obj) {
      NS_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(ConvertArg<Arg1>(mArg1),
                        ConvertArg<Arg2>(mArg2),
                        ConvertArg<Arg3>(mArg3),
                        ConvertArg<Arg4>(mArg4),
                        ConvertArg<Arg5>(mArg5));
    }
    return NS_OK;
  }

private:
  DaemonNotificationRunnable5(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4, Arg5))
    : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult Init(const InitOp& aInitOp)
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
class DaemonNotificationRunnable6 final : public details::DaemonRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef DaemonNotificationRunnable6<ObjectWrapper, Res,
    Tin1, Tin2, Tin3, Tin4, Tin5, Tin6, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6>
    SelfType;

  template <typename InitOp>
  static already_AddRefed<SelfType> Create(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6),
    const InitOp& aInitOp)
  {
    RefPtr<SelfType> runnable(new SelfType(aMethod));
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
    RefPtr<SelfType> runnable = Create(aMethod, aInitOp);
    if (!runnable) {
      return;
    }
    Unused << NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(runnable)));
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();
    if (!obj) {
      NS_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(ConvertArg<Arg1>(mArg1),
                        ConvertArg<Arg2>(mArg2),
                        ConvertArg<Arg3>(mArg3),
                        ConvertArg<Arg4>(mArg4),
                        ConvertArg<Arg5>(mArg5),
                        ConvertArg<Arg6>(mArg6));
    }
    return NS_OK;
  }

private:
  DaemonNotificationRunnable6(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6))
    : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult Init(const InitOp& aInitOp)
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
          typename Tin7, typename Tin8,
          typename Arg1=Tin1, typename Arg2=Tin2, typename Arg3=Tin3,
          typename Arg4=Tin4, typename Arg5=Tin5, typename Arg6=Tin6,
          typename Arg7=Tin7, typename Arg8=Tin8>
class DaemonNotificationRunnable8 final : public details::DaemonRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef DaemonNotificationRunnable8<ObjectWrapper, Res,
    Tin1, Tin2, Tin3, Tin4, Tin5, Tin6, Tin7, Tin8,
    Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8> SelfType;

  template <typename InitOp>
  static already_AddRefed<SelfType> Create(
    Res (ObjectType::*aMethod)(
      Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8),
    const InitOp& aInitOp)
  {
    RefPtr<SelfType> runnable(new SelfType(aMethod));
    if (NS_FAILED(runnable->Init(aInitOp))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template <typename InitOp>
  static void
  Dispatch(
    Res (ObjectType::*aMethod)(
      Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8),
    const InitOp& aInitOp)
  {
    RefPtr<SelfType> runnable = Create(aMethod, aInitOp);
    if (!runnable) {
      return;
    }
    Unused << NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(runnable)));
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();
    if (!obj) {
      NS_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(ConvertArg<Arg1>(mArg1),
                        ConvertArg<Arg2>(mArg2),
                        ConvertArg<Arg3>(mArg3),
                        ConvertArg<Arg4>(mArg4),
                        ConvertArg<Arg5>(mArg5),
                        ConvertArg<Arg6>(mArg6),
                        ConvertArg<Arg7>(mArg7),
                        ConvertArg<Arg8>(mArg8));
    }
    return NS_OK;
  }

private:
  DaemonNotificationRunnable8(
    Res (ObjectType::*aMethod)(
      Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8))
    : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult Init(const InitOp& aInitOp)
  {
    nsresult rv = aInitOp(mArg1, mArg2, mArg3, mArg4,
                          mArg5, mArg6, mArg7, mArg8);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
  }

  Res (ObjectType::*mMethod)(
    Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8);
  Tin1 mArg1;
  Tin2 mArg2;
  Tin3 mArg3;
  Tin4 mArg4;
  Tin5 mArg5;
  Tin6 mArg6;
  Tin7 mArg7;
  Tin8 mArg8;
};

template <typename ObjectWrapper, typename Res,
          typename Tin1, typename Tin2, typename Tin3,
          typename Tin4, typename Tin5, typename Tin6,
          typename Tin7, typename Tin8, typename Tin9,
          typename Arg1=Tin1, typename Arg2=Tin2, typename Arg3=Tin3,
          typename Arg4=Tin4, typename Arg5=Tin5, typename Arg6=Tin6,
          typename Arg7=Tin7, typename Arg8=Tin8, typename Arg9=Tin9>
class DaemonNotificationRunnable9 final : public details::DaemonRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef DaemonNotificationRunnable9<ObjectWrapper, Res,
    Tin1, Tin2, Tin3, Tin4, Tin5, Tin6, Tin7, Tin8, Tin9,
    Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9> SelfType;

  template <typename InitOp>
  static already_AddRefed<SelfType> Create(
    Res (ObjectType::*aMethod)(
      Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9),
    const InitOp& aInitOp)
  {
    RefPtr<SelfType> runnable(new SelfType(aMethod));
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
    RefPtr<SelfType> runnable = Create(aMethod, aInitOp);
    if (!runnable) {
      return;
    }
    Unused << NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(runnable)));
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();
    if (!obj) {
      NS_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(ConvertArg<Arg1>(mArg1),
                        ConvertArg<Arg2>(mArg2),
                        ConvertArg<Arg3>(mArg3),
                        ConvertArg<Arg4>(mArg4),
                        ConvertArg<Arg5>(mArg5),
                        ConvertArg<Arg6>(mArg6),
                        ConvertArg<Arg7>(mArg7),
                        ConvertArg<Arg8>(mArg8),
                        ConvertArg<Arg9>(mArg9));
    }
    return NS_OK;
  }

private:
  DaemonNotificationRunnable9(
    Res (ObjectType::*aMethod)(
      Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9))
    : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename InitOp>
  nsresult Init(const InitOp& aInitOp)
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

}
}

#endif // mozilla_ipc_DaemonRunnables_h
