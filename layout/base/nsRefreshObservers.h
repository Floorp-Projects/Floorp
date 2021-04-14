/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Code to notify things that animate before a refresh, at an appropriate
 * refresh rate.  (Perhaps temporary, until replaced by compositor.)
 */

#ifndef LAYOUT_BASE_NSREFRESHOBSERVERS_H_
#define LAYOUT_BASE_NSREFRESHOBSERVERS_H_

#include <functional>

#include "mozilla/Attributes.h"
#include "mozilla/TimeStamp.h"
#include "nsISupports.h"

namespace mozilla {
class AnimationEventDispatcher;
class PendingFullscreenEvent;
class PresShell;
class RefreshDriverTimer;
}  // namespace mozilla

/**
 * An abstract base class to be implemented by callers wanting to be
 * notified at refresh times.  When nothing needs to be painted, callers
 * may not be notified.
 */
class nsARefreshObserver {
 public:
  // AddRef and Release signatures that match nsISupports.  Implementors
  // must implement reference counting, and those that do implement
  // nsISupports will already have methods with the correct signature.
  //
  // The refresh driver does NOT hold references to refresh observers
  // except while it is notifying them.
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  MOZ_CAN_RUN_SCRIPT virtual void WillRefresh(mozilla::TimeStamp aTime) = 0;
};

/**
 * An abstract base class to be implemented by callers wanting to be notified
 * when the observing refresh driver updated mMostRecentRefresh due to active
 * timer changes. Callers must ensure an observer is removed before it is
 * destroyed.
 */
class nsATimerAdjustmentObserver {
 public:
  virtual void NotifyTimerAdjusted(mozilla::TimeStamp aTime) = 0;
};

/**
 * An abstract base class to be implemented by callers wanting to be notified
 * that a refresh has occurred. Callers must ensure an observer is removed
 * before it is destroyed.
 */
class nsAPostRefreshObserver {
 public:
  virtual void DidRefresh() = 0;
};

namespace mozilla {

/**
 * A wrapper for nsAPostRefreshObserver that's refcounted and might unregister
 * itself after firing.
 *
 * Note, while the observer unregisters itself, the registering still needs to
 * be done by the caller.
 */
class ManagedPostRefreshObserver : public nsAPostRefreshObserver {
 public:
  // Whether the post-refresh observer should be unregistered after it has
  // fired.
  enum class Unregister : bool { No, Yes };
  using Action = std::function<Unregister(bool aWasCanceled)>;
  NS_INLINE_DECL_REFCOUNTING(ManagedPostRefreshObserver)
  ManagedPostRefreshObserver(mozilla::PresShell*, Action&&);
  explicit ManagedPostRefreshObserver(mozilla::PresShell*);
  void DidRefresh() override;
  void Cancel();

 protected:
  virtual ~ManagedPostRefreshObserver();
  RefPtr<mozilla::PresShell> mPresShell;
  Action mAction;
};

}  // namespace mozilla

#endif
