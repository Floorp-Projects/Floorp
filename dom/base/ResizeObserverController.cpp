/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ResizeObserverController.h"

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/PresShell.h"
#include "mozilla/Unused.h"
#include "nsPresContext.h"
#include "nsRefreshDriver.h"
#include <limits>

namespace mozilla::dom {

void ResizeObserverNotificationHelper::WillRefresh(TimeStamp aTime) {
  MOZ_DIAGNOSTIC_ASSERT(mOwner, "Should've de-registered on-time!");
  mOwner->Notify();
  // Note that mOwner may be null / dead here.
}

nsRefreshDriver* ResizeObserverNotificationHelper::GetRefreshDriver() const {
  PresShell* presShell = mOwner->GetPresShell();
  if (MOZ_UNLIKELY(!presShell)) {
    return nullptr;
  }

  nsPresContext* presContext = presShell->GetPresContext();
  if (MOZ_UNLIKELY(!presContext)) {
    return nullptr;
  }

  return presContext->RefreshDriver();
}

void ResizeObserverNotificationHelper::Register() {
  if (mRegistered) {
    return;
  }

  nsRefreshDriver* refreshDriver = GetRefreshDriver();
  if (!refreshDriver) {
    // We maybe navigating away from this page or currently in an iframe with
    // display: none. Just abort the Register(), no need to do notification.
    return;
  }

  refreshDriver->AddRefreshObserver(this, FlushType::Display, "ResizeObserver");
  mRegistered = true;
}

void ResizeObserverNotificationHelper::Unregister() {
  if (!mRegistered) {
    return;
  }

  nsRefreshDriver* refreshDriver = GetRefreshDriver();
  MOZ_RELEASE_ASSERT(
      refreshDriver,
      "We should not leave a dangling reference to the observer around");

  bool rv = refreshDriver->RemoveRefreshObserver(this, FlushType::Display);
  MOZ_DIAGNOSTIC_ASSERT(rv, "Should remove the observer successfully");
  Unused << rv;

  mRegistered = false;
}

ResizeObserverNotificationHelper::~ResizeObserverNotificationHelper() {
  MOZ_RELEASE_ASSERT(!mRegistered, "How can we die when registered?");
  MOZ_RELEASE_ASSERT(!mOwner, "Forgot to clear weak pointer?");
}

void ResizeObserverController::ShellDetachedFromDocument() {
  mResizeObserverNotificationHelper->Unregister();
}

static void FlushLayoutForWholeBrowsingContextTree(Document& aDoc) {
  if (BrowsingContext* bc = aDoc.GetBrowsingContext()) {
    RefPtr<BrowsingContext> top = bc->Top();
    top->PreOrderWalk([](BrowsingContext* aCur) {
      if (Document* doc = aCur->GetExtantDocument()) {
        doc->FlushPendingNotifications(FlushType::Layout);
      }
    });
  } else {
    // If there is no browsing context, we just flush this document itself.
    aDoc.FlushPendingNotifications(FlushType::Layout);
  }
}

void ResizeObserverController::Notify() {
  if (mResizeObservers.IsEmpty()) {
    return;
  }

  // We may call BroadcastAllActiveObservations(), which might cause mDocument
  // to be destroyed (and the ResizeObserverController with it).
  // e.g. if mDocument is in an iframe, and the observer JS removes it from the
  // parent document and we trigger an unlucky GC/CC (or maybe if the observer
  // JS navigates to a different URL). Or the author uses elem.offsetTop API,
  // which could flush style + layout and make the document destroyed if we're
  // inside an iframe that has suddenly become |display:none| via the author
  // doing something in their ResizeObserver callback.
  //
  // Therefore, we ref-count mDocument here to make sure it and its members
  // (e.g. mResizeObserverController, which is `this` pointer) are still alive
  // in the rest of this function because after it goes up, `this` is possible
  // deleted.
  RefPtr<Document> doc(mDocument);

  uint32_t shallowestTargetDepth = 0;

  GatherAllActiveObservations(shallowestTargetDepth);

  while (HasAnyActiveObservations()) {
    DebugOnly<uint32_t> oldShallowestTargetDepth = shallowestTargetDepth;
    shallowestTargetDepth = BroadcastAllActiveObservations();
    NS_ASSERTION(oldShallowestTargetDepth < shallowestTargetDepth,
                 "shallowestTargetDepth should be getting strictly deeper");

    // Flush layout, so that any callback functions' style changes / resizes
    // get a chance to take effect. The callback functions may do changes in its
    // sub-documents or ancestors, so flushing layout for the whole browsing
    // context tree makes sure we don't miss anyone.
    FlushLayoutForWholeBrowsingContextTree(*doc);

    // To avoid infinite resize loop, we only gather all active observations
    // that have the depth of observed target element more than current
    // shallowestTargetDepth.
    GatherAllActiveObservations(shallowestTargetDepth);
  }

  mResizeObserverNotificationHelper->Unregister();

  if (HasAnySkippedObservations()) {
    // Per spec, we deliver an error if the document has any skipped
    // observations. Also, we re-register via ScheduleNotification().
    RootedDictionary<ErrorEventInit> init(RootingCx());

    init.mMessage.AssignLiteral(
        "ResizeObserver loop completed with undelivered notifications.");
    init.mBubbles = false;
    init.mCancelable = false;

    nsEventStatus status = nsEventStatus_eIgnore;

    if (nsCOMPtr<nsPIDOMWindowInner> window = doc->GetInnerWindow()) {
      nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(window);
      MOZ_ASSERT(sgo);

      if (NS_WARN_IF(sgo->HandleScriptError(init, &status))) {
        status = nsEventStatus_eIgnore;
      }
    } else {
      // We don't fire error events at any global for non-window JS on the main
      // thread.
    }

    // We need to deliver pending notifications in next cycle.
    ScheduleNotification();
  }
}

void ResizeObserverController::GatherAllActiveObservations(uint32_t aDepth) {
  for (ResizeObserver* observer : mResizeObservers) {
    observer->GatherActiveObservations(aDepth);
  }
}

uint32_t ResizeObserverController::BroadcastAllActiveObservations() {
  uint32_t shallowestTargetDepth = std::numeric_limits<uint32_t>::max();

  // Copy the observers as this invokes the callbacks and could register and
  // unregister observers at will.
  const auto observers =
      ToTArray<nsTArray<RefPtr<ResizeObserver>>>(mResizeObservers);
  for (auto& observer : observers) {
    // MOZ_KnownLive because 'observers' is guaranteed to keep it
    // alive.
    //
    // This can go away once
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1620312 is fixed.
    uint32_t targetDepth =
        MOZ_KnownLive(observer)->BroadcastActiveObservations();
    if (targetDepth < shallowestTargetDepth) {
      shallowestTargetDepth = targetDepth;
    }
  }

  return shallowestTargetDepth;
}

bool ResizeObserverController::HasAnyActiveObservations() const {
  for (auto& observer : mResizeObservers) {
    if (observer->HasActiveObservations()) {
      return true;
    }
  }
  return false;
}

bool ResizeObserverController::HasAnySkippedObservations() const {
  for (auto& observer : mResizeObservers) {
    if (observer->HasSkippedObservations()) {
      return true;
    }
  }
  return false;
}

void ResizeObserverController::ScheduleNotification() {
  mResizeObserverNotificationHelper->Register();
}

ResizeObserverController::~ResizeObserverController() {
  MOZ_RELEASE_ASSERT(
      !mResizeObserverNotificationHelper->IsRegistered(),
      "Nothing else should keep a reference to our helper when we go away");
  mResizeObserverNotificationHelper->DetachFromOwner();
}

void ResizeObserverController::AddSizeOfIncludingThis(
    nsWindowSizes& aSizes) const {
  MallocSizeOf mallocSizeOf = aSizes.mState.mMallocSizeOf;
  size_t size = mallocSizeOf(this);
  size += mResizeObservers.ShallowSizeOfExcludingThis(mallocSizeOf);
  // TODO(emilio): Measure the observers individually or something? They aren't
  // really owned by us.
  aSizes.mDOMSizes.mDOMResizeObserverControllerSize += size;
}

}  // namespace mozilla::dom
