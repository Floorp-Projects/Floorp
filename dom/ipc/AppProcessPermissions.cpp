/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppProcessPermissions.h"
#include "ContentParent.h"
#include "mozIApplication.h"
#include "mozilla/hal_sandbox/PHalParent.h"
#include "nsIDOMApplicationRegistry.h"
#include "TabParent.h"

using namespace mozilla::dom;
using namespace mozilla::hal_sandbox;
using namespace mozilla::services;

namespace mozilla {

bool
AppProcessHasPermission(PBrowserParent* aActor, const char* aPermission)
{
  if (!aActor) {
    NS_WARNING("Testing permissions for null actor");
    return false;
  }

  TabParent* tab = static_cast<TabParent*>(aActor);
  nsCOMPtr<mozIApplication> app = tab->GetApp();
  // isBrowser frames inherit their app descriptor to identify their
  // data storage, but they don't inherit the permissions associated
  // with that descriptor.
  if (!app || tab->IsBrowserElement()) {
    return false;
  }

  bool hasPermission = false;
  return (NS_SUCCEEDED(app->HasPermission(aPermission, &hasPermission)) &&
          hasPermission);
}

bool
AppProcessHasPermission(PContentParent* aActor, const char* aPermission)
{
  const InfallibleTArray<PBrowserParent*>& browsers =
    aActor->ManagedPBrowserParent();
  for (uint32_t i = 0; i < browsers.Length(); ++i) {
    if (AppProcessHasPermission(browsers[i], aPermission)) {
      return true;
    }
  }
  return false;
}

bool
AppProcessHasPermission(PHalParent* aActor, const char* aPermission)
{
  return AppProcessHasPermission(aActor->Manager(), aPermission);
}

} // namespace mozilla
