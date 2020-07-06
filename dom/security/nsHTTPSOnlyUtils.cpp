/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/net/DNS.h"
#include "nsContentUtils.h"
#include "nsHTTPSOnlyUtils.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "prnetdb.h"

/* static */
bool nsHTTPSOnlyUtils::IsHttpsOnlyModeEnabled(bool aFromPrivateWindow) {
  // if the general pref is set to true, then we always return
  if (mozilla::StaticPrefs::dom_security_https_only_mode()) {
    return true;
  }

  // otherwise we check if executing in private browsing mode and return true
  // if the PBM pref for HTTPS-Only is set.
  if (aFromPrivateWindow &&
      mozilla::StaticPrefs::dom_security_https_only_mode_pbm()) {
    return true;
  }
  return false;
}

/* static */
bool nsHTTPSOnlyUtils::ShouldUpgradeRequest(nsIURI* aURI,
                                            nsILoadInfo* aLoadInfo) {
  // 1. Check if the HTTPS-Only Mode is even enabled, before we do anything else
  bool isPrivateWin = aLoadInfo->GetOriginAttributes().mPrivateBrowsingId > 0;
  if (!IsHttpsOnlyModeEnabled(isPrivateWin)) {
    return false;
  }

  // 2. Check for general exceptions
  if (OnionException(aURI) || LoopbackOrLocalException(aURI)) {
    return false;
  }

  // 3. Check if NoUpgrade-flag is set in LoadInfo
  uint32_t httpsOnlyStatus = aLoadInfo->GetHttpsOnlyStatus();
  if (httpsOnlyStatus & nsILoadInfo::HTTPS_ONLY_EXEMPT) {
    // Let's log to the console, that we didn't upgrade this request
    uint32_t innerWindowId = aLoadInfo->GetInnerWindowID();
    AutoTArray<nsString, 1> params = {
        NS_ConvertUTF8toUTF16(aURI->GetSpecOrDefault())};
    nsHTTPSOnlyUtils::LogLocalizedString("HTTPSOnlyNoUpgradeException", params,
                                         nsIScriptError::infoFlag,
                                         innerWindowId, isPrivateWin, aURI);
    return false;
  }

  // We can upgrade the request - let's log it to the console
  // Appending an 's' to the scheme for the logging. (http -> https)
  nsAutoCString scheme;
  aURI->GetScheme(scheme);
  scheme.AppendLiteral("s");
  NS_ConvertUTF8toUTF16 reportSpec(aURI->GetSpecOrDefault());
  NS_ConvertUTF8toUTF16 reportScheme(scheme);

  uint32_t innerWindowId = aLoadInfo->GetInnerWindowID();
  AutoTArray<nsString, 2> params = {reportSpec, reportScheme};
  nsHTTPSOnlyUtils::LogLocalizedString(
      "HTTPSOnlyUpgradeRequest", params, nsIScriptError::warningFlag,
      innerWindowId, !!aLoadInfo->GetOriginAttributes().mPrivateBrowsingId,
      aURI);

  // If the status was not determined before, we now indicate that the request
  // will get upgraded, but no event-listener has been registered yet.
  if (httpsOnlyStatus & nsILoadInfo::HTTPS_ONLY_UNINITIALIZED) {
    httpsOnlyStatus ^= nsILoadInfo::HTTPS_ONLY_UNINITIALIZED;
    httpsOnlyStatus |= nsILoadInfo::HTTPS_ONLY_UPGRADED_LISTENER_NOT_REGISTERED;
    aLoadInfo->SetHttpsOnlyStatus(httpsOnlyStatus);
  }
  return true;
}

/* static */
bool nsHTTPSOnlyUtils::ShouldUpgradeWebSocket(nsIURI* aURI,
                                              int32_t aInnerWindowId,
                                              bool aFromPrivateWindow,
                                              uint32_t aHttpsOnlyStatus) {
  // 1. Check if the HTTPS-Only Mode is even enabled, before we do anything else
  if (!IsHttpsOnlyModeEnabled(aFromPrivateWindow)) {
    return false;
  }

  // 2. Check for general exceptions
  if (OnionException(aURI) || LoopbackOrLocalException(aURI)) {
    return false;
  }

  // 3. Check if NoUpgrade-flag is set in LoadInfo
  if (aHttpsOnlyStatus & nsILoadInfo::HTTPS_ONLY_EXEMPT) {
    // Let's log to the console, that we didn't upgrade this request
    AutoTArray<nsString, 1> params = {
        NS_ConvertUTF8toUTF16(aURI->GetSpecOrDefault())};
    nsHTTPSOnlyUtils::LogLocalizedString(
        "HTTPSOnlyNoUpgradeException", params, nsIScriptError::infoFlag,
        aInnerWindowId, aFromPrivateWindow, aURI);
    return false;
  }

  // We can upgrade the request - let's log it to the console
  // Appending an 's' to the scheme for the logging. (ws -> wss)
  nsAutoCString scheme;
  aURI->GetScheme(scheme);
  scheme.AppendLiteral("s");
  NS_ConvertUTF8toUTF16 reportSpec(aURI->GetSpecOrDefault());
  NS_ConvertUTF8toUTF16 reportScheme(scheme);

  AutoTArray<nsString, 2> params = {reportSpec, reportScheme};
  nsHTTPSOnlyUtils::LogLocalizedString(
      "HTTPSOnlyUpgradeRequest", params, nsIScriptError::warningFlag,
      aInnerWindowId, aFromPrivateWindow, aURI);

  return true;
}

/* static */
bool nsHTTPSOnlyUtils::CouldBeHttpsOnlyError(nsresult aError) {
  // This list of error codes is largely drawn from
  // nsDocShell::DisplayLoadError()
  return !(NS_ERROR_UNKNOWN_PROTOCOL == aError ||
           NS_ERROR_FILE_NOT_FOUND == aError ||
           NS_ERROR_FILE_ACCESS_DENIED == aError ||
           NS_ERROR_UNKNOWN_HOST == aError || NS_ERROR_PHISHING_URI == aError ||
           NS_ERROR_MALWARE_URI == aError || NS_ERROR_UNWANTED_URI == aError ||
           NS_ERROR_HARMFUL_URI == aError ||
           NS_ERROR_CONTENT_CRASHED == aError ||
           NS_ERROR_FRAME_CRASHED == aError);
}

/* ------ Logging ------ */

/* static */
void nsHTTPSOnlyUtils::LogLocalizedString(
    const char* aName, const nsTArray<nsString>& aParams, uint32_t aFlags,
    uint64_t aInnerWindowID, bool aFromPrivateWindow, nsIURI* aURI) {
  nsAutoString logMsg;
  nsContentUtils::FormatLocalizedString(nsContentUtils::eSECURITY_PROPERTIES,
                                        aName, aParams, logMsg);
  LogMessage(logMsg, aFlags, aInnerWindowID, aFromPrivateWindow, aURI);
}

/* static */
void nsHTTPSOnlyUtils::LogMessage(const nsAString& aMessage, uint32_t aFlags,
                                  uint64_t aInnerWindowID,
                                  bool aFromPrivateWindow, nsIURI* aURI) {
  // Prepending HTTPS-Only to the outgoing console message
  nsString message;
  message.AppendLiteral(u"HTTPS-Only Mode: ");
  message.Append(aMessage);

  // Allow for easy distinction in devtools code.
  nsCString category("HTTPSOnly");

  if (aInnerWindowID > 0) {
    // Send to content console
    nsContentUtils::ReportToConsoleByWindowID(message, aFlags, category,
                                              aInnerWindowID, aURI);
  } else {
    // Send to browser console
    nsContentUtils::LogSimpleConsoleError(
        message, category.get(), aFromPrivateWindow,
        true /* from chrome context */, aFlags);
  }
}

/* ------ Exceptions ------ */

/* static */
bool nsHTTPSOnlyUtils::OnionException(nsIURI* aURI) {
  // Onion-host exception can get disabled with a pref
  if (mozilla::StaticPrefs::dom_security_https_only_mode_upgrade_onion()) {
    return false;
  }
  nsAutoCString host;
  aURI->GetHost(host);
  return StringEndsWith(host, ".onion"_ns);
}

/* static */
bool nsHTTPSOnlyUtils::LoopbackOrLocalException(nsIURI* aURI) {
  nsAutoCString asciiHost;
  nsresult rv = aURI->GetAsciiHost(asciiHost);
  NS_ENSURE_SUCCESS(rv, false);

  // Let's make a quick check if the host matches these loopback strings before
  // we do anything else
  if (asciiHost.EqualsLiteral("localhost") || asciiHost.EqualsLiteral("::1")) {
    return true;
  }

  // The local-ip and loopback checks expect a NetAddr struct. We only have a
  // host-string but can convert it to a NetAddr by first converting it to
  // PRNetAddr.
  PRNetAddr tempAddr;
  memset(&tempAddr, 0, sizeof(PRNetAddr));
  // PR_StringToNetAddr does not properly initialize the output buffer in the
  // case of IPv6 input. See bug 223145.
  if (PR_StringToNetAddr(asciiHost.get(), &tempAddr) != PR_SUCCESS) {
    return false;
  }

  // The linter wants this struct to get initialized,
  // but PRNetAddrToNetAddr will do that.
  mozilla::net::NetAddr addr;  // NOLINT(cppcoreguidelines-pro-type-member-init)
  PRNetAddrToNetAddr(&tempAddr, &addr);

  // Loopback IPs are always exempt
  if (IsLoopBackAddress(&addr)) {
    return true;
  }

  // Local IP exception can get disabled with a pref
  bool upgradeLocal =
      mozilla::StaticPrefs::dom_security_https_only_mode_upgrade_local();
  return (!upgradeLocal && IsIPAddrLocal(&addr));
}
