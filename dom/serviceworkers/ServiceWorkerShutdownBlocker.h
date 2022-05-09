/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkershutdownblocker_h__
#define mozilla_dom_serviceworkershutdownblocker_h__

#include "nsCOMPtr.h"
#include "nsIAsyncShutdown.h"
#include "nsISupportsImpl.h"
#include "nsITimer.h"

#include "ServiceWorkerShutdownState.h"
#include "mozilla/InitializedOnce.h"
#include "mozilla/MozPromise.h"
#include "mozilla/NotNull.h"
#include "mozilla/HashTable.h"

namespace mozilla::dom {

class ServiceWorkerManager;

/**
 * Main thread only.
 *
 * A ServiceWorkerShutdownBlocker will "accept promises", and each of these
 * promises will be a "pending promise" while it hasn't settled. At some point,
 * `StopAcceptingPromises()` should be called and the state will change to "not
 * accepting promises" (this is a one way state transition). The shutdown phase
 * of the shutdown client the blocker is created with will be blocked until
 * there are no more pending promises.
 *
 * It doesn't matter whether the state changes to "not accepting promises"
 * before or during the associated shutdown phase.
 *
 * In beta/release builds there will be an additional timer that starts ticking
 * once both the shutdown phase has been reached and the state is "not accepting
 * promises". If when the timer expire there are still pending promises,
 * shutdown will be forcefully unblocked.
 */
class ServiceWorkerShutdownBlocker final : public nsIAsyncShutdownBlocker,
                                           public nsITimerCallback,
                                           public nsINamed {
 public:
  using Progress = ServiceWorkerShutdownState::Progress;
  static const uint32_t kInvalidShutdownStateId = 0;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIASYNCSHUTDOWNBLOCKER
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  /**
   * Returns the registered shutdown blocker if registration succeeded and
   * nullptr otherwise.
   */
  static already_AddRefed<ServiceWorkerShutdownBlocker> CreateAndRegisterOn(
      nsIAsyncShutdownClient& aShutdownBarrier,
      ServiceWorkerManager& aServiceWorkerManager);

  /**
   * Blocks shutdown until `aPromise` settles.
   *
   * Can be called multiple times, and shutdown will be blocked until all the
   * calls' promises settle, but all of these calls must happen before
   * `StopAcceptingPromises()` is called (assertions will enforce this).
   *
   * See `CreateShutdownState` for aShutdownStateId, which is needed to clear
   * the shutdown state if the shutdown process aborts for some reason.
   */
  void WaitOnPromise(GenericNonExclusivePromise* aPromise,
                     uint32_t aShutdownStateId);

  /**
   * Once this is called, shutdown will be blocked until all promises
   * passed to `WaitOnPromise()` settle, and there must be no more calls to
   * `WaitOnPromise()` (assertions will enforce this).
   */
  void StopAcceptingPromises();

  /**
   * Start tracking the shutdown of an individual ServiceWorker for hang
   * reporting purposes. Returns a "shutdown state ID" that should be used
   * in subsequent calls to ReportShutdownProgress. The shutdown of an
   * individual ServiceWorker is presumed to be completed when its `Progress`
   * reaches `Progress::ShutdownCompleted`.
   */
  uint32_t CreateShutdownState();

  void ReportShutdownProgress(uint32_t aShutdownStateId, Progress aProgress);

 private:
  explicit ServiceWorkerShutdownBlocker(
      ServiceWorkerManager& aServiceWorkerManager);

  ~ServiceWorkerShutdownBlocker();

  /**
   * No-op if any of the following are true:
   * 1) `BlockShutdown()` hasn't been called yet, or
   * 2) `StopAcceptingPromises()` hasn't been called yet, or
   * 3) `StopAcceptingPromises()` HAS been called, but there are still pending
   *    promises.
   */
  void MaybeUnblockShutdown();

  /**
   * Requires `BlockShutdown()` to have been called.
   */
  void UnblockShutdown();

  /**
   * Returns the remaining pending promise count (i.e. excluding the promise
   * that just settled).
   */
  uint32_t PromiseSettled();

  bool IsAcceptingPromises() const;

  uint32_t GetPendingPromises() const;

  /**
   * Initializes a timer that will unblock shutdown unconditionally once it's
   * expired (even if there are still pending promises). No-op if:
   * 1) not a beta or release build, or
   * 2) shutdown is not being blocked or `StopAcceptingPromises()` has not been
   *    called.
   */
  void MaybeInitUnblockShutdownTimer();

  struct AcceptingPromises {
    uint32_t mPendingPromises = 0;
  };

  struct NotAcceptingPromises {
    explicit NotAcceptingPromises(AcceptingPromises aPreviousState);

    uint32_t mPendingPromises = 0;
  };

  Variant<AcceptingPromises, NotAcceptingPromises> mState;

  nsCOMPtr<nsIAsyncShutdownClient> mShutdownClient;

  HashMap<uint32_t, ServiceWorkerShutdownState> mShutdownStates;

  nsCOMPtr<nsITimer> mTimer;
  LazyInitializedOnceEarlyDestructible<
      const NotNull<RefPtr<ServiceWorkerManager>>>
      mServiceWorkerManager;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_serviceworkershutdownblocker_h__
