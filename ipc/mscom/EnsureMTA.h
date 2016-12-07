/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_EnsureMTA_h
#define mozilla_mscom_EnsureMTA_h

#include "MainThreadUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/mscom/COMApartmentRegion.h"
#include "mozilla/mscom/Utils.h"
#include "nsCOMPtr.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"

#include <windows.h>

namespace mozilla {
namespace mscom {

// This class is OK to use as a temporary on the stack.
class MOZ_STACK_CLASS EnsureMTA
{
public:
  /**
   * This constructor just ensures that the MTA thread is up and running.
   */
  EnsureMTA()
  {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIThread> thread = GetMTAThread();
    MOZ_ASSERT(thread);
  }

  template <typename FuncT>
  EnsureMTA(const FuncT& aClosure)
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (IsCurrentThreadMTA()) {
      // We're already on the MTA, we can run aClosure directly
      aClosure();
      return;
    }

    // In this case we need to run aClosure on a background thread in the MTA
    nsCOMPtr<nsIThread> thread = GetMTAThread();
    MOZ_ASSERT(thread);

    HANDLE event = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event) {
      return;
    }

    auto eventSetter = [&]() -> void {
      aClosure();
      ::SetEvent(event);
    };

    nsresult rv =
      thread->Dispatch(NS_NewRunnableFunction(eventSetter), NS_DISPATCH_NORMAL);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      ::CloseHandle(event);
      return;
    }

    DWORD waitResult;
    while ((waitResult = ::WaitForSingleObjectEx(event, INFINITE, TRUE)) ==
           WAIT_IO_COMPLETION) {
    }
    MOZ_ASSERT(waitResult == WAIT_OBJECT_0);
    ::CloseHandle(event);
  }

private:
  static nsCOMPtr<nsIThread> GetMTAThread();
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_EnsureMTA_h

