/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AppProcessChecker_h
#define mozilla_AppProcessChecker_h

#include <stdint.h>

class nsIPrincipal;

namespace mozilla {

namespace dom {
class TabContext;
class PBrowserParent;
class PContentParent;
}

namespace hal_sandbox {
class PHalParent;
}

enum AssertAppProcessType {
  ASSERT_APP_PROCESS_PERMISSION,
  ASSERT_APP_PROCESS_MANIFEST_URL,
  ASSERT_APP_HAS_PERMISSION
};

/**
 * Return true if the specified browser has the specified capability.
 * If this returns false, the browser didn't have the capability and
 * will be killed.
 */
bool
AssertAppProcess(mozilla::dom::PBrowserParent* aActor,
                 AssertAppProcessType aType,
                 const char* aCapability);

/**
 * Return true if the specified app has the specified status.
 * If this returns false, the browser will be killed.
 */
bool
AssertAppStatus(mozilla::dom::PBrowserParent* aActor,
                unsigned short aStatus);

/**
 * Return true if the specified browser has the specified capability.
 * If this returns false, the browser didn't have the capability and
 * will be killed.
 */
bool
AssertAppProcess(const mozilla::dom::TabContext& aContext,
                 AssertAppProcessType aType,
                 const char* aCapability);

/**
 * Return true if the specified app has the specified status.
 * If this returns false, the browser will be killed.
 */
bool
AssertAppStatus(const mozilla::dom::TabContext& aContext,
                unsigned short aStatus);

/**
 * Return true if any of the PBrowsers loaded in this content process
 * has the specified capability.  If this returns false, the process
 * didn't have the capability and will be killed.
 */
bool
AssertAppProcess(mozilla::dom::PContentParent* aActor,
                 AssertAppProcessType aType,
                 const char* aCapability);

/**
 * Return true if any of the PBrowsers loaded in this content process
 * has an app with the specified status. If this returns false, the process
 * didn't have the status and will be killed.
 */
bool
AssertAppStatus(mozilla::dom::PContentParent* aActor,
                unsigned short aStatus);

bool
AssertAppProcess(mozilla::hal_sandbox::PHalParent* aActor,
                 AssertAppProcessType aType,
                 const char* aCapability);

// NB: when adding capability checks for other IPDL actors, please add
// them to this file and have them delegate to the two functions above
// as appropriate.  For example,
//
//   bool AppProcessHasCapability(PNeckoParent* aActor, AssertAppProcessType aType) {
//     return AssertAppProcess(aActor->Manager(), aType);
//   }

bool
AssertAppPrincipal(mozilla::dom::PContentParent* aParent,
                   nsIPrincipal* aPrincipal);

/**
 * Check if the specified principal is valid, and return the saved permission
 * value for permission `aPermission' on that principal.
 * See nsIPermissionManager.idl for possible return values.
 *
 * nsIPermissionManager::UNKNOWN_ACTION is retuned if the principal is invalid.
 */
uint32_t
CheckPermission(mozilla::dom::PContentParent* aParent,
                nsIPrincipal* aPrincipal, const char* aPermission);

/**
 * Inline function for asserting the process's permission.
 */
template<typename T>
inline bool
AssertAppProcessPermission(T* aActor,
                           const char* aPermission) {
  return AssertAppProcess(aActor,
                          ASSERT_APP_PROCESS_PERMISSION,
                          aPermission);
}

/**
 * Inline function for asserting the process's manifest URL.
 */
template<typename T>
inline bool
AssertAppProcessManifestURL(T* aActor,
                            const char* aManifestURL) {
  return AssertAppProcess(aActor,
                          ASSERT_APP_PROCESS_MANIFEST_URL,
                          aManifestURL);
}

/**
 * Inline function for asserting the process's manifest URL.
 */
template<typename T>
inline bool
AssertAppHasPermission(T* aActor,
                       const char* aPermission) {
  return AssertAppProcess(aActor,
                          ASSERT_APP_HAS_PERMISSION,
                          aPermission);
}

template<typename T>
inline bool
AssertAppHasStatus(T* aActor,
                   unsigned short aStatus) {
  return AssertAppStatus(aActor, aStatus);
}

} // namespace mozilla

#endif // mozilla_AppProcessChecker_h
