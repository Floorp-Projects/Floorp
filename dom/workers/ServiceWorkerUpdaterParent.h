/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ServiceWorkerUpdaterParent_h
#define mozilla_dom_ServiceWorkerUpdaterParent_h

#include "mozilla/dom/PServiceWorkerUpdaterParent.h"
#include "mozilla/BasePrincipal.h"

namespace mozilla {
namespace dom {
namespace workers {

class ServiceWorkerManagerService;

class ServiceWorkerUpdaterParent final : public PServiceWorkerUpdaterParent
{
public:
  void
  ActorDestroy(ActorDestroyReason aWhy) override;

  bool
  Proceed(ServiceWorkerManagerService* aService);

private:
  RefPtr<ServiceWorkerManagerService> mService;
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ServiceWorkerUpdaterParent_h
