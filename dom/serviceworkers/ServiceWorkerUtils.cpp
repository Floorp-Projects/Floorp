/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerUtils.h"

#include "mozilla/Preferences.h"

namespace mozilla {
namespace dom {

bool
ServiceWorkerParentInterceptEnabled()
{
  // For right now we only support main thread.  In the future we could make
  // this use an atomic bool if we need to support worker threads.
  MOZ_ASSERT(NS_IsMainThread());

  static bool sInit = false;
  static bool sEnabled;

  if (!sInit) {
    MOZ_ASSERT(NS_IsMainThread());
    Preferences::AddBoolVarCache(&sEnabled,
                                 "dom.serviceWorkers.parent_intercept",
                                 false);
    sInit = true;
  }

  return sEnabled;
}

bool
ServiceWorkerRegistrationDataIsValid(const ServiceWorkerRegistrationData& aData)
{
  return !aData.scope().IsEmpty() &&
         !aData.currentWorkerURL().IsEmpty() &&
         !aData.cacheName().IsEmpty();
}

} // namespace dom
} // namespace mozilla
