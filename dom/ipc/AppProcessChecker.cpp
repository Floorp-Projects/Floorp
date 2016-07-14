/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppProcessChecker.h"
#include "nsIPermissionManager.h"

namespace mozilla {
namespace dom {
class PContentParent;
} // namespace dom
} // namespace mozilla

class nsIPrincipal;

namespace mozilla {

#if DEBUG
  #define LOG(...) printf_stderr(__VA_ARGS__)
#else
  #define LOG(...)
#endif

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

} // namespace mozilla
