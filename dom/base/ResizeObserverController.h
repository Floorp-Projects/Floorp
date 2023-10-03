/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ResizeObserverController_h
#define mozilla_dom_ResizeObserverController_h

#include "mozilla/dom/Document.h"
#include "mozilla/dom/ResizeObserver.h"
#include "mozilla/TimeStamp.h"
#include "nsRefreshObservers.h"
#include "nsTObserverArray.h"

class nsRefreshDriver;

namespace mozilla {

class PresShell;

namespace dom {

/**
 * ResizeObserverController contains the list of ResizeObservers and controls
 * the flow of notification.
 */
class ResizeObserverController final {
 public:
  explicit ResizeObserverController(Document* aDocument)
      : mDocument(aDocument) {
    MOZ_ASSERT(mDocument, "Need a non-null document");
  }

  void AddSizeOfIncludingThis(nsWindowSizes&) const;

  void ShellDetachedFromDocument() { UnscheduleNotification(); }
  void AddResizeObserver(ResizeObserver& aObserver) {
    MOZ_ASSERT(!mResizeObservers.Contains(&aObserver));
    // Insert internal ResizeObservers before scripted ones, since they may have
    // observable side-effects and we don't want to expose the insertion time.
    if (aObserver.HasNativeCallback()) {
      mResizeObservers.InsertElementAt(0, &aObserver);
    } else {
      mResizeObservers.AppendElement(&aObserver);
    }
  }

  void RemoveResizeObserver(ResizeObserver& aObserver) {
    MOZ_ASSERT(mResizeObservers.Contains(&aObserver));
    mResizeObservers.RemoveElement(&aObserver);
  }

  /**
   * Schedule/unschedule notification on refresh driver tick.
   */
  void ScheduleNotification();
  void UnscheduleNotification();

  /**
   * Notify all ResizeObservers by gathering and broadcasting all active
   * observations.
   */
  MOZ_CAN_RUN_SCRIPT void Notify();

  bool IsScheduled() const { return mScheduled; }

  ~ResizeObserverController();

 private:
  nsRefreshDriver* GetRefreshDriver() const;

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

  nsTArray<ResizeObserver*> mResizeObservers;

  // Indicates whether the controller awaits for notification from the refresh
  // driver.
  bool mScheduled = false;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ResizeObserverController_h
