/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRefreshObservers.h"
#include "PresShell.h"

namespace mozilla {

ManagedPostRefreshObserver::ManagedPostRefreshObserver(PresShell* aPresShell,
                                                       Action&& aAction)
    : mPresShell(aPresShell), mAction(std::move(aAction)) {}

ManagedPostRefreshObserver::ManagedPostRefreshObserver(PresShell* aPresShell)
    : mPresShell(aPresShell) {}

ManagedPostRefreshObserver::~ManagedPostRefreshObserver() = default;

void ManagedPostRefreshObserver::Cancel() {
  mAction(true);
  mAction = nullptr;
  mPresShell = nullptr;
}

void ManagedPostRefreshObserver::DidRefresh() {
  if (!mPresShell) {
    MOZ_ASSERT_UNREACHABLE(
        "Post-refresh observer fired again after failed attempt at "
        "unregistering it");
    return;
  }
  Unregister unregister = mAction(false);
  if (bool(unregister)) {
    nsPresContext* presContext = mPresShell->GetPresContext();
    if (!presContext) {
      MOZ_ASSERT_UNREACHABLE(
          "Unable to unregister post-refresh observer! Leaking it instead of "
          "leaving garbage registered");
      // Graceful handling, just in case...
      mPresShell = nullptr;
      mAction = nullptr;
      return;
    }
    presContext->UnregisterManagedPostRefreshObserver(this);
  }
}

}  // namespace mozilla
