/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerUpdaterParent.h"
#include "ServiceWorkerManagerService.h"

namespace mozilla {
namespace dom {
namespace workers {

bool
ServiceWorkerUpdaterParent::Proceed(ServiceWorkerManagerService* aService)
{
  if (!SendProceed(true)) {
    return false;
  }

  mService = aService;
  return true;
}

void
ServiceWorkerUpdaterParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mService) {
    mService->UpdaterActorDestroyed(this);
  }
}

} // namespace workers
} // namespace dom
} // namespace mozilla
