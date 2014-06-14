/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LoadManager.h"
#include "LoadManagerFactory.h"
#include "MainThreadUtils.h"
#include "nsIObserverService.h"

#include "mozilla/Preferences.h"

namespace mozilla {

// Assume stored in an nsAutoPtr<>
LoadManager *
LoadManagerBuild(void)
{
  return new LoadManager(LoadManagerSingleton::Get());
}

/* static */  LoadManagerSingleton*
LoadManagerSingleton::Get() {
  if (!sSingleton) {
    MOZ_ASSERT(NS_IsMainThread());

    int loadMeasurementInterval =
      mozilla::Preferences::GetInt("media.navigator.load_adapt.measure_interval", 1000);
    int averagingSeconds =
      mozilla::Preferences::GetInt("media.navigator.load_adapt.avg_seconds", 3);
    float highLoadThreshold =
      mozilla::Preferences::GetFloat("media.navigator.load_adapt.high_load", 0.90);
    float lowLoadThreshold =
      mozilla::Preferences::GetFloat("media.navigator.load_adapt.low_load", 0.40);

    sSingleton = new LoadManagerSingleton(loadMeasurementInterval,
                                          averagingSeconds,
                                          highLoadThreshold,
                                          lowLoadThreshold);

    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      obs->AddObserver(sSingleton, "xpcom-shutdown", false);
    }
  }
  return sSingleton;
}

}; // namespace
