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
#include "mozilla/MozPromise.h"
#include "mozilla/Unused.h"
#include "mozilla/mscom/AgileReference.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/RefPtr.h"
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

class ProcessRuntime;

// This class is OK to use as a temporary on the stack.
class MOZ_STACK_CLASS EnsureMTA final {
 public:
  enum class Option {
    Default,
    // Forcibly dispatch to the thread returned by GetPersistentMTAThread(),
    // even if the current thread is already inside a MTA.
    ForceDispatchToPersistentThread,
  };

  /**
   * Synchronously run |aClosure| on a thread living in the COM multithreaded
   * apartment. If the current thread lives inside the COM MTA, then it runs
   * |aClosure| immediately unless |aOpt| ==
   * Option::ForceDispatchToPersistentThread.
   */
  template <typename FuncT>
  explicit EnsureMTA(FuncT&& aClosure, Option aOpt = Option::Default) {
    if (aOpt != Option::ForceDispatchToPersistentThread &&
        IsCurrentThreadMTA()) {
      // We're already on the MTA, we can run aClosure directly
      aClosure();
      return;
    }

    // In this case we need to run aClosure on a background thread in the MTA
    nsCOMPtr<nsIRunnable> runnable(
        NS_NewRunnableFunction("EnsureMTA::EnsureMTA", std::move(aClosure)));
    SyncDispatch(std::move(runnable), aOpt);
  }

 private:
  static nsCOMPtr<nsIThread> GetPersistentMTAThread();

  static void SyncDispatch(nsCOMPtr<nsIRunnable>&& aRunnable, Option aOpt);
  static void SyncDispatchToPersistentThread(nsIRunnable* aRunnable);

  // The following function is private in order to force any consumers to be
  // declared as friends of EnsureMTA. The intention is to prevent
  // AsyncOperation from becoming some kind of free-for-all mechanism for
  // asynchronously executing work on a background thread.
  template <typename FuncT>
  static void AsyncOperation(FuncT&& aClosure) {
    if (IsCurrentThreadMTA()) {
      aClosure();
      return;
    }

    nsCOMPtr<nsIThread> thread(GetPersistentMTAThread());
    MOZ_ASSERT(thread);
    if (!thread) {
      return;
    }

    DebugOnly<nsresult> rv = thread->Dispatch(
        NS_NewRunnableFunction("mscom::EnsureMTA::AsyncOperation",
                               std::move(aClosure)),
        NS_DISPATCH_NORMAL);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  /**
   * This constructor just ensures that the MTA is up and running. This should
   * only be called by ProcessRuntime.
   */
  EnsureMTA();

  friend class mozilla::mscom::ProcessRuntime;

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
