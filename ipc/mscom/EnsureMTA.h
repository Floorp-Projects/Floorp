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

  using CreateInstanceAgileRefPromise =
      MozPromise<AgileReference, HRESULT, false>;

  /**
   *       *** A MSCOM PEER SHOULD REVIEW ALL NEW USES OF THIS API! ***
   *
   * Asynchronously instantiate a new COM object from a MTA thread, unless the
   * current thread is already living inside the multithreaded apartment, in
   * which case the object is immediately instantiated.
   *
   * This function only supports the most common configurations for creating
   * a new object, so it only supports in-process servers. Furthermore, this
   * function does not support aggregation (ie. the |pUnkOuter| parameter to
   * CoCreateInstance).
   *
   * Given that attempting to instantiate an Apartment-threaded COM object
   * inside the MTA results in a *loss* of performance, we assert when that
   * situation arises.
   *
   * The resulting promise, once resolved, provides an AgileReference that may
   * be passed between any COM-initialized thread in the current process.
   *
   *       *** A MSCOM PEER SHOULD REVIEW ALL NEW USES OF THIS API! ***
   *
   * WARNING:
   * Some COM objects do not support creation in the multithreaded apartment,
   * in which case this function is not available as an option. In this case,
   * the promise will always be rejected. In debug builds we will assert.
   *
   *       *** A MSCOM PEER SHOULD REVIEW ALL NEW USES OF THIS API! ***
   *
   * WARNING:
   * Any in-process COM objects whose interfaces accept HWNDs are probably
   * *not* safe to instantiate in the multithreaded apartment! Even if this
   * function succeeds when creating such an object, you *MUST NOT* do so, as
   * these failures might not become apparent until your code is running out in
   * the wild on the release channel!
   *
   *       *** A MSCOM PEER SHOULD REVIEW ALL NEW USES OF THIS API! ***
   *
   * WARNING:
   * When you obtain an interface from the AgileReference, it may or may not be
   * a proxy to the real object. This depends entirely on the implementation of
   * the underlying class and the multithreading capabilities that the class
   * declares to the COM runtime. If the interface is proxied, it might be
   * expensive to invoke methods on that interface! *Always* test the
   * performance of your method calls when calling interfaces that are resolved
   * via this function!
   *
   *       *** A MSCOM PEER SHOULD REVIEW ALL NEW USES OF THIS API! ***
   *
   * (Despite this myriad of warnings, it is still *much* safer to use this
   * function to asynchronously create COM objects than it is to roll your own!)
   *
   *       *** A MSCOM PEER SHOULD REVIEW ALL NEW USES OF THIS API! ***
   */
  static RefPtr<CreateInstanceAgileRefPromise> CreateInstance(REFCLSID aClsid,
                                                              REFIID aIid);

 private:
  static RefPtr<CreateInstanceAgileRefPromise> CreateInstanceInternal(
      REFCLSID aClsid, REFIID aIid);

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
