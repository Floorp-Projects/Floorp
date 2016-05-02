/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TVListeners.h"
#include "mozilla/Preferences.h"
#include "nsITVService.h"
#include "nsITVSimulatorService.h"
#include "nsServiceManagerUtils.h"
#include "TVServiceFactory.h"

namespace mozilla {
namespace dom {

/* static */ already_AddRefed<nsITVService>
TVServiceFactory::AutoCreateTVService()
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsITVService> service = do_CreateInstance(TV_SERVICE_CONTRACTID);
  if (!service) {
    // Fallback to the TV Simulator Service
    service = do_CreateInstance(TV_SIMULATOR_SERVICE_CONTRACTID, &rv);

    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }

  rv = service->SetSourceListener(new TVSourceListener());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return service.forget();
}

} // namespace dom
} // namespace mozilla
