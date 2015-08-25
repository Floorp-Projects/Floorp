/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppProcessChecker.h"
#include "nsIPermissionManager.h"
#ifdef MOZ_CHILD_PERMISSIONS
#include "ContentParent.h"
#include "mozIApplication.h"
#include "mozilla/hal_sandbox/PHalParent.h"
#include "nsIAppsService.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsPrintfCString.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "TabParent.h"

#include <algorithm>

using namespace mozilla::dom;
using namespace mozilla::hal_sandbox;
using namespace mozilla::services;
#else
namespace mozilla {
namespace dom {
class PContentParent;
} // namespace dom
} // namespace mozilla

class nsIPrincipal;
#endif

namespace mozilla {

#ifdef MOZ_CHILD_PERMISSIONS

static bool
CheckAppTypeHelper(mozIApplication* aApp,
                   AssertAppProcessType aType,
                   const char* aCapability,
                   bool aIsBrowserElement)
{
  bool aValid = false;

  // isBrowser frames inherit their app descriptor to identify their
  // data storage, but they don't inherit the capability associated
  // with that descriptor.
  if (aApp && (aType == ASSERT_APP_HAS_PERMISSION || !aIsBrowserElement)) {
    switch (aType) {
      case ASSERT_APP_HAS_PERMISSION:
      case ASSERT_APP_PROCESS_PERMISSION:
        if (!NS_SUCCEEDED(aApp->HasPermission(aCapability, &aValid))) {
          aValid = false;
        }
        break;
      case ASSERT_APP_PROCESS_MANIFEST_URL: {
        nsAutoString manifestURL;
        if (NS_SUCCEEDED(aApp->GetManifestURL(manifestURL)) &&
            manifestURL.EqualsASCII(aCapability)) {
          aValid = true;
        }
        break;
      }
      default:
        break;
    }
  }
  return aValid;
}

bool
AssertAppProcess(PBrowserParent* aActor,
                 AssertAppProcessType aType,
                 const char* aCapability)
{
  if (!aActor) {
    NS_WARNING("Testing process capability for null actor");
    return false;
  }

  TabParent* tab = TabParent::GetFrom(aActor);
  nsCOMPtr<mozIApplication> app = tab->GetOwnOrContainingApp();

  return CheckAppTypeHelper(app, aType, aCapability, tab->IsBrowserElement());
}

static bool
CheckAppStatusHelper(mozIApplication* aApp,
                     unsigned short aStatus)
{
  bool valid = false;

  if (aApp) {
    unsigned short appStatus = 0;
    if (NS_SUCCEEDED(aApp->GetAppStatus(&appStatus))) {
      valid = appStatus == aStatus;
    }
  }

  return valid;
}

bool
AssertAppStatus(PBrowserParent* aActor,
                unsigned short aStatus)
{
  if (!aActor) {
    NS_WARNING("Testing process capability for null actor");
    return false;
  }

  TabParent* tab = TabParent::GetFrom(aActor);
  nsCOMPtr<mozIApplication> app = tab->GetOwnOrContainingApp();

  return CheckAppStatusHelper(app, aStatus);
}

bool
AssertAppProcess(TabContext& aContext,
                 AssertAppProcessType aType,
                 const char* aCapability)
{

  nsCOMPtr<mozIApplication> app = aContext.GetOwnOrContainingApp();
  return CheckAppTypeHelper(app, aType, aCapability, aContext.IsBrowserElement());
}

bool
AssertAppStatus(TabContext& aContext,
                unsigned short aStatus)
{

  nsCOMPtr<mozIApplication> app = aContext.GetOwnOrContainingApp();
  return CheckAppStatusHelper(app, aStatus);
}

bool
AssertAppProcess(PContentParent* aActor,
                 AssertAppProcessType aType,
                 const char* aCapability)
{
  nsTArray<TabContext> contextArray =
    static_cast<ContentParent*>(aActor)->GetManagedTabContext();
  for (uint32_t i = 0; i < contextArray.Length(); ++i) {
    if (AssertAppProcess(contextArray[i], aType, aCapability)) {
      return true;
    }
  }

  NS_ERROR(
    nsPrintfCString(
      "Security problem: Content process does not have `%s'.  It will be killed.\n",
      aCapability).get());

  static_cast<ContentParent*>(aActor)->KillHard("AssertAppProcess");

  return false;
}

bool
AssertAppStatus(PContentParent* aActor,
                unsigned short aStatus)
{
  nsTArray<TabContext> contextArray =
    static_cast<ContentParent*>(aActor)->GetManagedTabContext();
  for (uint32_t i = 0; i < contextArray.Length(); ++i) {
    if (AssertAppStatus(contextArray[i], aStatus)) {
      return true;
    }
  }

  NS_ERROR(
    nsPrintfCString(
      "Security problem: Content process does not have `%d' status.  It will be killed.",
      aStatus).get());

  static_cast<ContentParent*>(aActor)->KillHard("AssertAppStatus");

  return false;
}

bool
AssertAppProcess(PHalParent* aActor,
                 AssertAppProcessType aType,
                 const char* aCapability)
{
  return AssertAppProcess(aActor->Manager(), aType, aCapability);
}

bool
AssertAppPrincipal(PContentParent* aActor,
                   nsIPrincipal* aPrincipal)
{
  if (!aPrincipal) {
    NS_WARNING("Principal is invalid, killing app process");
    static_cast<ContentParent*>(aActor)->KillHard("AssertAppPrincipal");
    return false;
  }

  uint32_t principalAppId = aPrincipal->GetAppId();
  bool inBrowserElement = aPrincipal->GetIsInBrowserElement();

  // Check if the permission's appId matches a child we manage.
  nsTArray<TabContext> contextArray =
    static_cast<ContentParent*>(aActor)->GetManagedTabContext();
  for (uint32_t i = 0; i < contextArray.Length(); ++i) {
    if (contextArray[i].OwnOrContainingAppId() == principalAppId) {
      // If the child only runs inBrowserElement content and the principal claims
      // it's not in a browser element, it's lying.
      if (!contextArray[i].IsBrowserElement() || inBrowserElement) {
        return true;
      }
      break;
    }
  }

  NS_WARNING("Principal is invalid, killing app process");
  static_cast<ContentParent*>(aActor)->KillHard("AssertAppPrincipal");
  return false;
}

already_AddRefed<nsIPrincipal>
GetAppPrincipal(uint32_t aAppId)
{
  nsCOMPtr<nsIAppsService> appsService = do_GetService(APPS_SERVICE_CONTRACTID);

  nsCOMPtr<mozIApplication> app;
  nsresult rv = appsService->GetAppByLocalId(aAppId, getter_AddRefs(app));
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsString origin;
  rv = app->GetOrigin(origin);
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), origin);

  nsCOMPtr<nsIScriptSecurityManager> secMan =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);

  nsCOMPtr<nsIPrincipal> appPrincipal;
  rv = secMan->GetAppCodebasePrincipal(uri, aAppId, false,
                                       getter_AddRefs(appPrincipal));
  NS_ENSURE_SUCCESS(rv, nullptr);
  return appPrincipal.forget();
}

uint32_t
CheckPermission(PContentParent* aActor,
                nsIPrincipal* aPrincipal,
                const char* aPermission)
{
  if (!AssertAppPrincipal(aActor, aPrincipal)) {
    return nsIPermissionManager::DENY_ACTION;
  }

  nsCOMPtr<nsIPermissionManager> pm =
    services::GetPermissionManager();
  NS_ENSURE_TRUE(pm, nsIPermissionManager::DENY_ACTION);

  // Make sure that `aPermission' is an app permission before checking the origin.
  nsCOMPtr<nsIPrincipal> appPrincipal = GetAppPrincipal(aPrincipal->GetAppId());
  uint32_t appPerm = nsIPermissionManager::UNKNOWN_ACTION;
  nsresult rv = pm->TestExactPermissionFromPrincipal(appPrincipal, aPermission, &appPerm);
  NS_ENSURE_SUCCESS(rv, nsIPermissionManager::UNKNOWN_ACTION);
  // Setting to "deny" in the settings UI should deny everywhere.
  if (appPerm == nsIPermissionManager::UNKNOWN_ACTION ||
      appPerm == nsIPermissionManager::DENY_ACTION) {
    return appPerm;
  }

  uint32_t permission = nsIPermissionManager::UNKNOWN_ACTION;
  rv = pm->TestExactPermissionFromPrincipal(aPrincipal, aPermission, &permission);
  NS_ENSURE_SUCCESS(rv, nsIPermissionManager::UNKNOWN_ACTION);
  if (permission == nsIPermissionManager::UNKNOWN_ACTION ||
      permission == nsIPermissionManager::DENY_ACTION) {
    return permission;
  }

  // For browser content (and if the app hasn't explicitly denied this),
  // consider the requesting origin, not the app.
  if (appPerm == nsIPermissionManager::PROMPT_ACTION &&
      aPrincipal->GetIsInBrowserElement()) {
    return permission;
  }

  // Setting to "prompt" in the settings UI should prompt everywhere in
  // non-browser content.
  if (appPerm == nsIPermissionManager::PROMPT_ACTION ||
      permission == nsIPermissionManager::PROMPT_ACTION) {
    return nsIPermissionManager::PROMPT_ACTION;
  }

  if (appPerm == nsIPermissionManager::ALLOW_ACTION ||
      permission == nsIPermissionManager::ALLOW_ACTION) {
    return nsIPermissionManager::ALLOW_ACTION;
  }

  NS_RUNTIMEABORT("Invalid permission value");
  return nsIPermissionManager::DENY_ACTION;
}

#else

bool
AssertAppProcess(mozilla::dom::PBrowserParent* aActor,
                 AssertAppProcessType aType,
                 const char* aCapability)
{
  return true;
}

bool
AssertAppStatus(mozilla::dom::PBrowserParent* aActor,
                unsigned short aStatus)
{
  return true;
}

bool
AssertAppProcess(const mozilla::dom::TabContext& aContext,
                 AssertAppProcessType aType,
                 const char* aCapability)
{
  return true;
}

bool
AssertAppStatus(const mozilla::dom::TabContext& aContext,
                unsigned short aStatus)
{
  return true;
}


bool
AssertAppProcess(mozilla::dom::PContentParent* aActor,
                 AssertAppProcessType aType,
                 const char* aCapability)
{
  return true;
}

bool
AssertAppStatus(mozilla::dom::PContentParent* aActor,
                unsigned short aStatus)
{
  return true;
}

bool
AssertAppProcess(mozilla::hal_sandbox::PHalParent* aActor,
                 AssertAppProcessType aType,
                 const char* aCapability)
{
  return true;
}

bool
AssertAppPrincipal(mozilla::dom::PContentParent* aActor,
                   nsIPrincipal* aPrincipal)
{
  return true;
}

uint32_t
CheckPermission(mozilla::dom::PContentParent* aActor,
                nsIPrincipal* aPrincipal,
                const char* aPermission)
{
  return nsIPermissionManager::ALLOW_ACTION;
}

#endif

} // namespace mozilla
