/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_MainThreadInvoker_h
#define mozilla_mscom_MainThreadInvoker_h

#include <windows.h>

#include <utility>

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TimeStamp.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"

class nsIRunnable;

namespace mozilla {
namespace mscom {

class MainThreadInvoker {
 public:
  MainThreadInvoker();

  bool Invoke(already_AddRefed<nsIRunnable>&& aRunnable);
  const TimeDuration& GetDuration() const { return mDuration; }
  static HANDLE GetTargetThread() { return sMainThread; }

 private:
  TimeDuration mDuration;

  static bool InitStatics();
  static VOID CALLBACK MainThreadAPC(ULONG_PTR aParam);

  static HANDLE sMainThread;
};

template <typename Class, typename... Args>
inline bool InvokeOnMainThread(const char* aName, Class* aObject,
                               void (Class::*aMethod)(Args...),
                               Args&&... aArgs) {
  nsCOMPtr<nsIRunnable> runnable(NewNonOwningRunnableMethod<Args...>(
      aName, aObject, aMethod, std::forward<Args>(aArgs)...));

  MainThreadInvoker invoker;
  return invoker.Invoke(runnable.forget());
}

}  // namespace mscom
}  // namespace mozilla

#endif  // mozilla_mscom_MainThreadInvoker_h
