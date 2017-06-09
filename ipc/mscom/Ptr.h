/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_Ptr_h
#define mozilla_mscom_Ptr_h

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/mscom/EnsureMTA.h"
#include "mozilla/UniquePtr.h"
#include "nsError.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

/**
 * The glue code in mozilla::mscom often needs to pass around interface pointers
 * belonging to a different apartment from the current one. We must not touch
 * the reference counts of those objects on the wrong apartment. By using these
 * UniquePtr specializations, we may ensure that the reference counts are always
 * handled correctly.
 */

namespace mozilla {
namespace mscom {

namespace detail {

template <typename T>
struct MainThreadRelease
{
  void operator()(T* aPtr)
  {
    if (!aPtr) {
      return;
    }
    if (NS_IsMainThread()) {
      aPtr->Release();
      return;
    }
    DebugOnly<nsresult> rv =
      NS_DispatchToMainThread(NewNonOwningRunnableMethod(aPtr,
                                                         &T::Release));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
};

template <typename T>
struct MTADelete
{
  void operator()(T* aPtr)
  {
    if (!aPtr) {
      return;
    }

    EnsureMTA::AsyncOperation([aPtr]() -> void {
      delete aPtr;
    });
  }
};

template <typename T>
struct MTARelease
{
  void operator()(T* aPtr)
  {
    if (!aPtr) {
      return;
    }

    // Static analysis doesn't recognize that, even though aPtr escapes the
    // current scope, we are in effect moving our strong ref into the lambda.
    void* ptr = aPtr;
    EnsureMTA::AsyncOperation([ptr]() -> void {
      reinterpret_cast<T*>(ptr)->Release();
    });
  }
};

template <typename T>
struct MTAReleaseInChildProcess
{
  void operator()(T* aPtr)
  {
    if (!aPtr) {
      return;
    }

    if (XRE_IsParentProcess()) {
      MOZ_ASSERT(NS_IsMainThread());
      aPtr->Release();
      return;
    }

    // Static analysis doesn't recognize that, even though aPtr escapes the
    // current scope, we are in effect moving our strong ref into the lambda.
    void* ptr = aPtr;
    EnsureMTA::AsyncOperation([ptr]() -> void {
      reinterpret_cast<T*>(ptr)->Release();
    });
  }
};

struct InterceptorTargetDeleter
{
  void operator()(IUnknown* aPtr)
  {
    // We intentionally do not touch the refcounts of interceptor targets!
  }
};

} // namespace detail

template <typename T>
using STAUniquePtr = mozilla::UniquePtr<T, detail::MainThreadRelease<T>>;

template <typename T>
using MTAUniquePtr = mozilla::UniquePtr<T, detail::MTARelease<T>>;

template <typename T>
using MTADeletePtr = mozilla::UniquePtr<T, detail::MTADelete<T>>;

template <typename T>
using ProxyUniquePtr = mozilla::UniquePtr<T, detail::MTAReleaseInChildProcess<T>>;

template <typename T>
using InterceptorTargetPtr =
  mozilla::UniquePtr<T, detail::InterceptorTargetDeleter>;

namespace detail {

// We don't have direct access to UniquePtr's storage, so we use mPtrStorage
// to receive the pointer and then set the target inside the destructor.
template <typename T, typename Deleter>
class UniquePtrGetterAddRefs
{
public:
  explicit UniquePtrGetterAddRefs(UniquePtr<T, Deleter>& aSmartPtr)
    : mTargetSmartPtr(aSmartPtr)
    , mPtrStorage(nullptr)
  {
  }

  ~UniquePtrGetterAddRefs()
  {
    mTargetSmartPtr.reset(mPtrStorage);
  }

  operator void**()
  {
    return reinterpret_cast<void**>(&mPtrStorage);
  }

  operator T**()
  {
    return &mPtrStorage;
  }

  T*& operator*()
  {
    return mPtrStorage;
  }

private:
  UniquePtr<T, Deleter>& mTargetSmartPtr;
  T*                     mPtrStorage;
};

} // namespace detail

template <typename T>
inline STAUniquePtr<T>
ToSTAUniquePtr(RefPtr<T>&& aRefPtr)
{
  return STAUniquePtr<T>(aRefPtr.forget().take());
}

template <typename T>
inline STAUniquePtr<T>
ToSTAUniquePtr(const RefPtr<T>& aRefPtr)
{
  MOZ_ASSERT(NS_IsMainThread());
  return STAUniquePtr<T>(do_AddRef(aRefPtr).take());
}

template <typename T>
inline STAUniquePtr<T>
ToSTAUniquePtr(T* aRawPtr)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (aRawPtr) {
    aRawPtr->AddRef();
  }
  return STAUniquePtr<T>(aRawPtr);
}

template <typename T, typename U>
inline STAUniquePtr<T>
ToSTAUniquePtr(const InterceptorTargetPtr<U>& aTarget)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<T> newRef(static_cast<T*>(aTarget.get()));
  return ToSTAUniquePtr(Move(newRef));
}

template <typename T>
inline MTAUniquePtr<T>
ToMTAUniquePtr(RefPtr<T>&& aRefPtr)
{
  return MTAUniquePtr<T>(aRefPtr.forget().take());
}

template <typename T>
inline MTAUniquePtr<T>
ToMTAUniquePtr(const RefPtr<T>& aRefPtr)
{
  MOZ_ASSERT(IsCurrentThreadMTA());
  return MTAUniquePtr<T>(do_AddRef(aRefPtr).take());
}

template <typename T>
inline MTAUniquePtr<T>
ToMTAUniquePtr(T* aRawPtr)
{
  MOZ_ASSERT(IsCurrentThreadMTA());
  if (aRawPtr) {
    aRawPtr->AddRef();
  }
  return MTAUniquePtr<T>(aRawPtr);
}

template <typename T>
inline ProxyUniquePtr<T>
ToProxyUniquePtr(RefPtr<T>&& aRefPtr)
{
  return ProxyUniquePtr<T>(aRefPtr.forget().take());
}

template <typename T>
inline ProxyUniquePtr<T>
ToProxyUniquePtr(const RefPtr<T>& aRefPtr)
{
  MOZ_ASSERT(IsProxy(aRawPtr));
  MOZ_ASSERT((XRE_IsParentProcess() && NS_IsMainThread()) ||
             (XRE_IsContentProcess() && IsCurrentThreadMTA()));

  return ProxyUniquePtr<T>(do_AddRef(aRefPtr).take());
}

template <typename T>
inline ProxyUniquePtr<T>
ToProxyUniquePtr(T* aRawPtr)
{
  MOZ_ASSERT(IsProxy(aRawPtr));
  MOZ_ASSERT((XRE_IsParentProcess() && NS_IsMainThread()) ||
             (XRE_IsContentProcess() && IsCurrentThreadMTA()));

  if (aRawPtr) {
    aRawPtr->AddRef();
  }
  return ProxyUniquePtr<T>(aRawPtr);
}

template <typename T, typename Deleter>
inline InterceptorTargetPtr<T>
ToInterceptorTargetPtr(const UniquePtr<T, Deleter>& aTargetPtr)
{
  return InterceptorTargetPtr<T>(aTargetPtr.get());
}

template <typename T, typename Deleter>
inline detail::UniquePtrGetterAddRefs<T, Deleter>
getter_AddRefs(UniquePtr<T, Deleter>& aSmartPtr)
{
  return detail::UniquePtrGetterAddRefs<T, Deleter>(aSmartPtr);
}

} // namespace mscom
} // namespace mozilla

// This block makes it possible for these smart pointers to be correctly
// applied in NewRunnableMethod and friends
namespace detail {

template<typename T>
struct SmartPointerStorageClass<mozilla::mscom::STAUniquePtr<T>>
{
  typedef StoreCopyPassByRRef<mozilla::mscom::STAUniquePtr<T>> Type;
};

template<typename T>
struct SmartPointerStorageClass<mozilla::mscom::MTAUniquePtr<T>>
{
  typedef StoreCopyPassByRRef<mozilla::mscom::MTAUniquePtr<T>> Type;
};

template<typename T>
struct SmartPointerStorageClass<mozilla::mscom::ProxyUniquePtr<T>>
{
  typedef StoreCopyPassByRRef<mozilla::mscom::ProxyUniquePtr<T>> Type;
};

template<typename T>
struct SmartPointerStorageClass<mozilla::mscom::InterceptorTargetPtr<T>>
{
  typedef StoreCopyPassByRRef<mozilla::mscom::InterceptorTargetPtr<T>> Type;
};

} // namespace detail

#endif // mozilla_mscom_Ptr_h

