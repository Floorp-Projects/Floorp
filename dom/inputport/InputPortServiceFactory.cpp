/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FakeInputPortService.h"
#include "InputPortListeners.h"
#include "InputPortServiceFactory.h"
#include "mozilla/Preferences.h"
#include "nsIInputPortService.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {

/* static */ already_AddRefed<FakeInputPortService>
InputPortServiceFactory::CreateFakeInputPortService()
{
  nsRefPtr<FakeInputPortService> service = new FakeInputPortService();
  return service.forget();
}

/* static */ already_AddRefed<nsIInputPortService>
InputPortServiceFactory::AutoCreateInputPortService()
{
  nsresult rv;
  nsCOMPtr<nsIInputPortService> service =
    do_GetService(INPUTPORT_SERVICE_CONTRACTID);
  if (!service) {
    // Fallback to the fake service.
    service = do_GetService(FAKE_INPUTPORT_SERVICE_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }
  MOZ_ASSERT(service);
  rv = service->SetInputPortListener(new InputPortListener());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return service.forget();
}

} // namespace dom
} // namespace mozilla
