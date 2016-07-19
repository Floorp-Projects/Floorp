/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_MainThreadInvoker_h
#define mozilla_mscom_MainThreadInvoker_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/StaticPtr.h"

#include <windows.h>

class nsIRunnable;

namespace mozilla {
namespace mscom {

class MainThreadInvoker
{
public:
  MainThreadInvoker();
  ~MainThreadInvoker();

  bool WaitForCompletion(DWORD aTimeout = INFINITE);
  bool Invoke(already_AddRefed<nsIRunnable>&& aRunnable,
              DWORD aTimeout = INFINITE);
  HANDLE GetTargetThread() const { return sMainThread; }

private:
  static bool InitStatics();
  static VOID CALLBACK MainThreadAPC(ULONG_PTR aParam);

  HANDLE  mDoneEvent;

  static HANDLE sMainThread;
  static StaticRefPtr<nsIRunnable> sAlertRunnable;
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_MainThreadInvoker_h
