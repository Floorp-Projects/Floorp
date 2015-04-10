/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerPeriodicUpdater.h"
#include "mozilla/ClearOnShutdown.h"

#define OBSERVER_TOPIC_IDLE_DAILY "idle-daily"

namespace mozilla {
namespace dom {
namespace workers {

NS_IMPL_ISUPPORTS(ServiceWorkerPeriodicUpdater, nsIObserver)

StaticRefPtr<ServiceWorkerPeriodicUpdater>
ServiceWorkerPeriodicUpdater::sInstance;

already_AddRefed<ServiceWorkerPeriodicUpdater>
ServiceWorkerPeriodicUpdater::GetSingleton()
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);

  if (!sInstance) {
    sInstance = new ServiceWorkerPeriodicUpdater();
    ClearOnShutdown(&sInstance);
  }
  nsRefPtr<ServiceWorkerPeriodicUpdater> copy(sInstance.get());
  return copy.forget();
}

ServiceWorkerPeriodicUpdater::ServiceWorkerPeriodicUpdater()
{
}

ServiceWorkerPeriodicUpdater::~ServiceWorkerPeriodicUpdater()
{
}

NS_IMETHODIMP
ServiceWorkerPeriodicUpdater::Observe(nsISupports* aSubject,
                                      const char* aTopic,
                                      const char16_t* aData)
{
  if (strcmp(aTopic, OBSERVER_TOPIC_IDLE_DAILY) == 0) {
    // TODO: Update the service workers NOW!!!!!
  }

  return NS_OK;
}

} // namespace workers
} // namespace dom
} // namespace mozilla
