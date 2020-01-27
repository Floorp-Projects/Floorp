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

#include "mozilla/MozPromise.h"

namespace mozilla {
namespace dom {

/**
 * Main thread only.
 */
class ServiceWorkerShutdownBlocker final : public nsIAsyncShutdownBlocker {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIASYNCSHUTDOWNBLOCKER

  /**
   * Returns the registered shutdown blocker if registration succeeded and
   * nullptr otherwise.
   */
  static already_AddRefed<ServiceWorkerShutdownBlocker> CreateAndRegisterOn(
      nsIAsyncShutdownClient* aShutdownBarrier);

  /**
   * Blocks shutdown until `aPromise` settles.
   *
   * Can be called multiple times, and shutdown will be blocked until all the
   * calls' promises settle, but all of these calls must happen before
   * `StopAcceptingPromises()` is called (assertions will enforce this).
   */
  void WaitOnPromise(GenericNonExclusivePromise* aPromise);

  /**
   * Once this is called, shutdown will be blocked until all promises
   * passed to `WaitOnPromise()` settle, and there must be no more calls to
   * `WaitOnPromise()` (assertions will enforce this).
   */
  void StopAcceptingPromises();

 private:
  ServiceWorkerShutdownBlocker();

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
   * Returns the remaining pending promise count (i.e. excluding the promise
   * that just settled).
   */
  uint32_t PromiseSettled();

  bool IsAcceptingPromises() const;

  uint32_t GetPendingPromises() const;

  struct AcceptingPromises {
    uint32_t mPendingPromises = 0;
  };

  struct NotAcceptingPromises {
    explicit NotAcceptingPromises(AcceptingPromises aPreviousState);

    uint32_t mPendingPromises = 0;
  };

  Variant<AcceptingPromises, NotAcceptingPromises> mState;

  nsCOMPtr<nsIAsyncShutdownClient> mShutdownClient;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_serviceworkershutdownblocker_h__
