/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ResourceStatsControl.h"
#include "mozilla/Preferences.h"
#include "nsIPermissionManager.h"
#include "nsJSUtils.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla::dom;

/* static */ bool
ResourceStatsControl::HasResourceStatsSupport(JSContext* /* unused */,
                                              JSObject* aGlobal)
{
  // Disable the constructors if they're disabled by the preference for sure.
  if (!Preferences::GetBool("dom.resource_stats.enabled", false)) {
    return false;
  }

  // Get window.
  nsCOMPtr<nsPIDOMWindow> win =
    do_QueryInterface((nsISupports*)nsJSUtils::GetStaticScriptGlobal(aGlobal));
  MOZ_ASSERT(!win || win->IsInnerWindow());
  if (!win) {
    return false;
  }

  // Check permission.
  nsCOMPtr<nsIPermissionManager> permMgr =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(permMgr, false);

  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  permMgr->TestPermissionFromWindow(win, "resourcestats-manage", &permission);
  return permission == nsIPermissionManager::ALLOW_ACTION;
}
