/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TVServiceFactory.h"

#ifdef MOZ_WIDGET_GONK
#include "gonk/TVGonkService.h"
#endif
#include "ipc/TVIPCService.h"
#include "nsITVService.h"
#include "nsITVSimulatorService.h"
#include "nsServiceManagerUtils.h"


namespace mozilla {
namespace dom {

/* static */ already_AddRefed<nsITVService>
TVServiceFactory::AutoCreateTVService()
{
  nsCOMPtr<nsITVService> service;

  if (GeckoProcessType_Default != XRE_GetProcessType()) {
    service = new TVIPCService();
    return service.forget();
  }

#ifdef MOZ_WIDGET_GONK
  service = new TVGonkService();
#endif

  if (!service) {
    // Fallback to TV simulator service, especially for TV simulator on WebIDE.
    service = do_CreateInstance(TV_SIMULATOR_SERVICE_CONTRACTID);
  }

  return service.forget();
}

} // namespace dom
} // namespace mozilla
