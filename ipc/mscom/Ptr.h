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
struct MTARelease
{
  void operator()(T* aPtr)
  {
    if (!aPtr) {
      return;
    }
    EnsureMTA([&]() -> void
    {
      aPtr->Release();
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
    EnsureMTA([&]() -> void
    {
      aPtr->Release();
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
using ProxyUniquePtr = mozilla::UniquePtr<T, detail::MTAReleaseInChildProcess<T>>;

using InterceptorTargetPtr =
  mozilla::UniquePtr<IUnknown, detail::InterceptorTargetDeleter>;

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_Ptr_h

