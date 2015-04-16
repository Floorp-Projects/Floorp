/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServiceWorkerPeriodicUpdater_h
#define mozilla_ServiceWorkerPeriodicUpdater_h

#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace dom {
namespace workers {

/**
 * This XPCOM component is main-process only, which means that it will never
 * get instantiated in child processes.  When we receive the idle-daily
 * notification in this component, we iterate over all PContent children, and
 * send each one a message that will trigger a call to
 * nsIServiceWorkerManager::UpdateAllRegistrations() in all child processes.
 */

class ServiceWorkerPeriodicUpdater final : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static already_AddRefed<ServiceWorkerPeriodicUpdater> GetSingleton();

private:
  ServiceWorkerPeriodicUpdater();
  ~ServiceWorkerPeriodicUpdater();

  static StaticRefPtr<ServiceWorkerPeriodicUpdater> sInstance;
  static bool sPeriodicUpdatesEnabled;
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif
