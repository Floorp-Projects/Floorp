/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaUtils.h"
#include "mozilla/Services.h"

namespace mozilla::media {

nsCOMPtr<nsIAsyncShutdownClient> GetShutdownBarrier() {
  nsCOMPtr<nsIAsyncShutdownService> svc = services::GetAsyncShutdownService();
  if (!svc) {
    // We can fail to get the shutdown service if we're already shutting down.
    return nullptr;
  }

  nsCOMPtr<nsIAsyncShutdownClient> barrier;
  nsresult rv = svc->GetProfileBeforeChange(getter_AddRefs(barrier));
  if (!barrier) {
    // We are probably in a content process. We need to do cleanup at
    // XPCOM shutdown in leakchecking builds.
    rv = svc->GetXpcomWillShutdown(getter_AddRefs(barrier));
  }
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  MOZ_RELEASE_ASSERT(barrier);
  return barrier;
}

nsCOMPtr<nsIAsyncShutdownClient> MustGetShutdownBarrier() {
  nsCOMPtr<nsIAsyncShutdownClient> barrier = GetShutdownBarrier();
  MOZ_RELEASE_ASSERT(barrier);
  return barrier;
}

NS_IMPL_ISUPPORTS(ShutdownBlocker, nsIAsyncShutdownBlocker)

}  // namespace mozilla::media
