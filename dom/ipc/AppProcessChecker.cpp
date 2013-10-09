/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppProcessChecker.h"
#ifdef MOZ_CHILD_PERMISSIONS
#include "ContentParent.h"
#include "mozIApplication.h"
#include "mozilla/hal_sandbox/PHalParent.h"
#include "TabParent.h"

using namespace mozilla::dom;
using namespace mozilla::hal_sandbox;
using namespace mozilla::services;
#endif

namespace mozilla {

#ifdef MOZ_CHILD_PERMISSIONS

bool
AssertAppProcess(PBrowserParent* aActor,
                 AssertAppProcessType aType,
                 const char* aCapability)
{
  if (!aActor) {
    NS_WARNING("Testing process capability for null actor");
    return false;
  }

  TabParent* tab = static_cast<TabParent*>(aActor);
  nsCOMPtr<mozIApplication> app = tab->GetOwnOrContainingApp();
  bool aValid = false;

  // isBrowser frames inherit their app descriptor to identify their
  // data storage, but they don't inherit the capability associated
  // with that descriptor.
  if (app && (aType == ASSERT_APP_HAS_PERMISSION || !tab->IsBrowserElement())) {
    switch (aType) {
      case ASSERT_APP_HAS_PERMISSION:
      case ASSERT_APP_PROCESS_PERMISSION:
        if (!NS_SUCCEEDED(app->HasPermission(aCapability, &aValid))) {
          aValid = false;
        }
        break;
      case ASSERT_APP_PROCESS_MANIFEST_URL: {
        nsAutoString manifestURL;
        if (NS_SUCCEEDED(app->GetManifestURL(manifestURL)) &&
            manifestURL.EqualsASCII(aCapability)) {
          aValid = true;
        }
        break;
      }
      default:
        break;
    }
  }

  if (!aValid) {
    printf_stderr("Security problem: Content process does not have `%s'.  It will be killed.\n", aCapability);
    ContentParent* process = tab->Manager();
    process->KillHard();
  }
  return aValid;
}

bool
AssertAppStatus(PBrowserParent* aActor,
                unsigned short aStatus)
{
  if (!aActor) {
    NS_WARNING("Testing process capability for null actor");
    return false;
  }

  TabParent* tab = static_cast<TabParent*>(aActor);
  nsCOMPtr<mozIApplication> app = tab->GetOwnOrContainingApp();

  if (app) {
    unsigned short appStatus = 0;
    if (NS_SUCCEEDED(app->GetAppStatus(&appStatus))) {
      return appStatus == aStatus;
    }
  }

  return false;
}

bool
AssertAppProcess(PContentParent* aActor,
                 AssertAppProcessType aType,
                 const char* aCapability)
{
  const InfallibleTArray<PBrowserParent*>& browsers =
    aActor->ManagedPBrowserParent();
  for (uint32_t i = 0; i < browsers.Length(); ++i) {
    if (AssertAppProcess(browsers[i], aType, aCapability)) {
      return true;
    }
  }
  return false;
}

bool
AssertAppStatus(PContentParent* aActor,
                unsigned short aStatus)
{
  const InfallibleTArray<PBrowserParent*>& browsers =
    aActor->ManagedPBrowserParent();
  for (uint32_t i = 0; i < browsers.Length(); ++i) {
    if (AssertAppStatus(browsers[i], aStatus)) {
      return true;
    }
  }
  return false;
}

bool
AssertAppProcess(PHalParent* aActor,
                 AssertAppProcessType aType,
                 const char* aCapability)
{
  return AssertAppProcess(aActor->Manager(), aType, aCapability);
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

#endif

} // namespace mozilla
