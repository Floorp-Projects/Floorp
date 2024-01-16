/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRefreshObservers.h"
#include "nsPresContext.h"
#include "mozilla/Likely.h"

namespace mozilla {

ManagedPostRefreshObserver::ManagedPostRefreshObserver(nsPresContext* aPc,
                                                       Action&& aAction)
    : mPresContext(aPc), mAction(std::move(aAction)) {}

ManagedPostRefreshObserver::ManagedPostRefreshObserver(nsPresContext* aPc)
    : mPresContext(aPc) {
  MOZ_ASSERT(mPresContext, "Can't observe without a nsPresContext");
}

ManagedPostRefreshObserver::~ManagedPostRefreshObserver() = default;

void ManagedPostRefreshObserver::Cancel() {
  // Caller holds a strong reference, so no need to reference stuff from here.
  if (mAction) {
    mAction(true);
  }
  mAction = nullptr;
  mPresContext = nullptr;
}

void ManagedPostRefreshObserver::DidRefresh() {
  if (MOZ_UNLIKELY(!mAction)) {
    return;
  }

  RefPtr<ManagedPostRefreshObserver> thisObject = this;
  auto action = std::move(mAction);
  Unregister unregister = action(false);

  if (unregister == Unregister::Yes) {
    if (RefPtr<nsPresContext> pc = std::move(mPresContext)) {
      // We have to null-check mPresContext here because in theory, mAction
      // could've ended up in `Cancel` being called, which would clear
      // mPresContext. In that case, we're already unregistered so no need to
      // do anything.
      pc->UnregisterManagedPostRefreshObserver(this);
    }
  } else {
    mAction = std::move(action);
  }
}

}  // namespace mozilla
