/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeviceProviderHelpers.h"

#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {
namespace presentation {

static const char* const kFxTVPresentationAppUrls[] = {
  "app://fling-player.gaiamobile.org/index.html",
  "app://notification-receiver.gaiamobile.org/index.html",
  nullptr
};

/* static */ bool
DeviceProviderHelpers::IsCommonlySupportedScheme(const nsAString& aUrl)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aUrl);
  if (NS_FAILED(rv) || !uri) {
    return false;
  }

  nsAutoCString scheme;
  uri->GetScheme(scheme);
  if (scheme.LowerCaseEqualsLiteral("http") ||
      scheme.LowerCaseEqualsLiteral("https")) {
    return true;
  }

  return false;
}

/* static */ bool
DeviceProviderHelpers::IsFxTVSupportedAppUrl(const nsAString& aUrl)
{
  // Check if matched with any presentation Apps on TV.
  for (uint32_t i = 0; kFxTVPresentationAppUrls[i]; i++) {
    if (aUrl.EqualsASCII(kFxTVPresentationAppUrls[i])) {
      return true;
    }
  }

  return false;
}

} // namespace presentation
} // namespace dom
} // namespace mozilla
