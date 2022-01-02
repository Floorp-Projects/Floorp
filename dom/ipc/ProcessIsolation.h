/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ProcessIsolation_h
#define mozilla_dom_ProcessIsolation_h

#include <stdint.h>

#include "mozilla/Logging.h"
#include "mozilla/dom/RemoteType.h"
#include "mozilla/dom/SessionHistoryEntry.h"
#include "nsString.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"

namespace mozilla::dom {

class CanonicalBrowsingContext;
class WindowGlobalParent;

extern mozilla::LazyLogModule gProcessIsolationLog;

constexpr nsLiteralCString kHighValueCOOPPermission = "highValueCOOP"_ns;
constexpr nsLiteralCString kHighValueHasSavedLoginPermission =
    "highValueHasSavedLogin"_ns;
constexpr nsLiteralCString kHighValueIsLoggedInPermission =
    "highValueIsLoggedIn"_ns;

// NavigationIsolationOptions is passed through the methods to store the state
// of the possible process and/or browsing context change.
struct NavigationIsolationOptions {
  nsCString mRemoteType;
  bool mReplaceBrowsingContext = false;
  uint64_t mSpecificGroupId = 0;
  bool mTryUseBFCache = false;
  RefPtr<SessionHistoryEntry> mActiveSessionHistoryEntry;
};

/**
 * Given a specific channel, determines which process the navigation should
 * complete in, and whether or not to perform a BrowsingContext-replace load
 * or enter the BFCache.
 *
 * This method will always return a `NavigationIsolationOptions` even if the
 * current remote type is compatible. Compatibility with the current process
 * should be checked at the call-site. An error should only be returned in
 * exceptional circumstances, and should lead to the load being cancelled.
 *
 * This method is only intended for use with document navigations.
 */
Result<NavigationIsolationOptions, nsresult> IsolationOptionsForNavigation(
    CanonicalBrowsingContext* aTopBC, WindowGlobalParent* aParentWindow,
    nsIURI* aChannelCreationURI, nsIChannel* aChannel,
    const nsACString& aCurrentRemoteType, bool aHasCOOPMismatch,
    uint32_t aLoadStateLoadType, const Maybe<uint64_t>& aChannelId,
    const Maybe<nsCString>& aRemoteTypeOverride);

/**
 * Adds a `highValue` permission to the permissions database, and make loads of
 * that origin isolated.
 *
 * The 'aPermissionType' parameter indicates why the site is treated as a high
 * value site. The possible values are:
 *
 * kHighValueCOOPPermission
 *     Called when a document request responds with a
 * `Cross-Origin-Opener-Policy` header.
 *
 * kHighValueHasSavedLoginPermission
 *     Called for sites that have an associated login saved in the password
 * manager.
 *
 * kHighValueIsLoggedInPermission
 *     Called when we detect a form with a password is submitted.
 */
void AddHighValuePermission(nsIPrincipal* aResultPrincipal,
                            const nsACString& aPermissionType);

void AddHighValuePermission(const nsACString& aOrigin,
                            const nsACString& aPermissionType);

/**
 * Returns true when fission is enabled and the
 * `fission.webContentIsolationStrategy` pref is set to `IsolateHighValue`.
 */
bool IsIsolateHighValueSiteEnabled();

}  // namespace mozilla::dom

#endif
