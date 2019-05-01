/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ResizeObserverController_h
#define mozilla_dom_ResizeObserverController_h

#include "mozilla/dom/ResizeObserver.h"
#include "mozilla/TimeStamp.h"
#include "nsRefreshDriver.h"
#include "nsTObserverArray.h"

namespace mozilla {

class PresShell;

namespace dom {

class Document;
class ResizeObserverController;

/**
 * ResizeObserverNotificationHelper will trigger ResizeObserver notifications
 * by registering with the Refresh Driver.
 */
class ResizeObserverNotificationHelper final : public nsARefreshObserver {
 public:
  NS_INLINE_DECL_REFCOUNTING(ResizeObserverNotificationHelper, override)

  explicit ResizeObserverNotificationHelper(ResizeObserverController* aOwner)
      : mOwner(aOwner), mRegistered(false) {
    MOZ_ASSERT(mOwner, "Need a non-null owner");
  }

  MOZ_CAN_RUN_SCRIPT void WillRefresh(TimeStamp aTime) override;

  nsRefreshDriver* GetRefreshDriver() const;

  void Register();

  void Unregister();

  bool IsRegistered() const { return mRegistered; }

  void DetachFromOwner() { mOwner = nullptr; }

 private:
  virtual ~ResizeObserverNotificationHelper();

  ResizeObserverController* mOwner;
  bool mRegistered;
};

/**
 * ResizeObserverController contains the list of ResizeObservers and controls
 * the flow of notification.
 */
class ResizeObserverController final {
 public:
  explicit ResizeObserverController(Document* aDocument)
      : mDocument(aDocument),
        mResizeObserverNotificationHelper(
            new ResizeObserverNotificationHelper(this)) {
    MOZ_ASSERT(mDocument, "Need a non-null document");
  }

  // Methods for supporting cycle-collection
  void Traverse(nsCycleCollectionTraversalCallback& aCb);
  void Unlink();

  void ShellDetachedFromDocument();
  void AddResizeObserver(ResizeObserver* aObserver);

  /**
   * Schedule the notification via ResizeObserverNotificationHelper refresh
   * observer.
   */
  void ScheduleNotification();

  /**
   * Notify all ResizeObservers by gathering and broadcasting all active
   * observations.
   */
  MOZ_CAN_RUN_SCRIPT void Notify();

  PresShell* GetPresShell() const { return mDocument->GetPresShell(); }

  ~ResizeObserverController();

 private:
  /**
   * Calls GatherActiveObservations(aDepth) for all ResizeObservers in this
   * controller. All observations in each ResizeObserver with element's depth
   * more than aDepth will be gathered.
   */
  void GatherAllActiveObservations(uint32_t aDepth);

  /**
   * Calls BroadcastActiveObservations() for all ResizeObservers in this
   * controller. It also returns the shallowest depth of observed target
   * elements with active observations from all ResizeObservers or
   * numeric_limits<uint32_t>::max() if there aren't any active observations
   * at all.
   */
  MOZ_CAN_RUN_SCRIPT uint32_t BroadcastAllActiveObservations();

  /**
   * Returns whether there is any ResizeObserver that has active observations.
   */
  bool HasAnyActiveObservations() const;

  /**
   * Returns whether there is any ResizeObserver that has skipped observations.
   */
  bool HasAnySkippedObservations() const;

  // Raw pointer is OK because mDocument strongly owns us & hence must outlive
  // us.
  Document* const mDocument;

  RefPtr<ResizeObserverNotificationHelper> mResizeObserverNotificationHelper;
  nsTObserverArray<RefPtr<ResizeObserver>> mResizeObservers;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ResizeObserverController_h
