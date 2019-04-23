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
#include "mozilla/Unused.h"
#include "mozilla/mscom/Utils.h"
#include "nsCOMPtr.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "nsWindowsHelpers.h"

#include <windows.h>

namespace mozilla {
namespace mscom {
namespace detail {

// Forward declarations
template <typename T>
struct MTADelete;

template <typename T>
struct MTARelease;

template <typename T>
struct MTAReleaseInChildProcess;

struct PreservedStreamDeleter;

}  // namespace detail

// This class is OK to use as a temporary on the stack.
class MOZ_STACK_CLASS EnsureMTA final {
 public:
  /**
   * This constructor just ensures that the MTA thread is up and running.
   */
  EnsureMTA() {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIThread> thread = GetMTAThread();
    MOZ_ASSERT(thread);
    Unused << thread;
  }

  enum class Option {
    Default,
    // Forcibly dispatch to the thread returned by GetMTAThread(), even if the
    // current thread is already inside a MTA.
    ForceDispatch,
  };

  template <typename FuncT>
  explicit EnsureMTA(const FuncT& aClosure, Option aOpt = Option::Default) {
    if (aOpt != Option::ForceDispatch && IsCurrentThreadMTA()) {
      // We're already on the MTA, we can run aClosure directly
      aClosure();
      return;
    }

    MOZ_ASSERT(NS_IsMainThread());

    // In this case we need to run aClosure on a background thread in the MTA
    nsCOMPtr<nsIThread> thread = GetMTAThread();
    MOZ_ASSERT(thread);
    if (!thread) {
      return;
    }

    // Note that we might reenter the EnsureMTA constructor while we wait on
    // this event due to APC dispatch, therefore we need a unique event object
    // for each entry. If perf becomes an issue then we will want to maintain
    // an array of events where the Nth event is unique to the Nth reentry.
    nsAutoHandle event(::CreateEventW(nullptr, FALSE, FALSE, nullptr));
    if (!event) {
      return;
    }

    HANDLE eventHandle = event.get();

    auto eventSetter = [&aClosure, eventHandle]() -> void {
      aClosure();
      ::SetEvent(eventHandle);
    };

    nsresult rv = thread->Dispatch(
        NS_NewRunnableFunction("EnsureMTA", eventSetter), NS_DISPATCH_NORMAL);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    if (NS_FAILED(rv)) {
      return;
    }

    DWORD waitResult;
    while ((waitResult = ::WaitForSingleObjectEx(event, INFINITE, TRUE)) ==
           WAIT_IO_COMPLETION) {
    }
    MOZ_ASSERT(waitResult == WAIT_OBJECT_0);
  }

 private:
  static nsCOMPtr<nsIThread> GetMTAThread();

  // The following function is private in order to force any consumers to be
  // declared as friends of EnsureMTA. The intention is to prevent
  // AsyncOperation from becoming some kind of free-for-all mechanism for
  // asynchronously executing work on a background thread.
  template <typename FuncT>
  static void AsyncOperation(const FuncT& aClosure) {
    if (IsCurrentThreadMTA()) {
      aClosure();
      return;
    }

    nsCOMPtr<nsIThread> thread(GetMTAThread());
    MOZ_ASSERT(thread);
    if (!thread) {
      return;
    }

    DebugOnly<nsresult> rv = thread->Dispatch(
        NS_NewRunnableFunction("mscom::EnsureMTA::AsyncOperation", aClosure),
        NS_DISPATCH_NORMAL);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  template <typename T>
  friend struct mozilla::mscom::detail::MTADelete;

  template <typename T>
  friend struct mozilla::mscom::detail::MTARelease;

  template <typename T>
  friend struct mozilla::mscom::detail::MTAReleaseInChildProcess;

  friend struct mozilla::mscom::detail::PreservedStreamDeleter;
};

}  // namespace mscom
}  // namespace mozilla

#endif  // mozilla_mscom_EnsureMTA_h
