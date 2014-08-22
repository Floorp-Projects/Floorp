/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FakeTVService.h"
#include "mozilla/dom/TVListeners.h"
#include "mozilla/Preferences.h"
#include "nsITVService.h"
#include "nsServiceManagerUtils.h"
#include "TVServiceFactory.h"

namespace mozilla {
namespace dom {

/* static */ already_AddRefed<FakeTVService>
TVServiceFactory::CreateFakeTVService()
{
  nsRefPtr<FakeTVService> service = new FakeTVService();
  nsresult rv = service->SetSourceListener(new TVSourceListener());
  NS_ENSURE_SUCCESS(rv, nullptr);

  return service.forget();
}

} // namespace dom
} // namespace mozilla
