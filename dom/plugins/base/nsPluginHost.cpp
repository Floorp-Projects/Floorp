/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* nsPluginHost.cpp - top-level plugin management code */

#include "nsPluginHost.h"

#include "nscore.h"

#include <cstdlib>
#include <stdio.h>
#include "nsPluginLogging.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerLabels.h"
#include "nsIBlocklistService.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsXULAppAPI.h"

using namespace mozilla;

LazyLogModule nsPluginLogging::gNPNLog(NPN_LOG_NAME);
LazyLogModule nsPluginLogging::gNPPLog(NPP_LOG_NAME);
LazyLogModule nsPluginLogging::gPluginLog(PLUGIN_LOG_NAME);

StaticRefPtr<nsPluginHost> nsPluginHost::sInst;

nsPluginHost::nsPluginHost() : mPluginEpoch(0) {
#ifdef PLUGIN_LOGGING
  MOZ_LOG(nsPluginLogging::gNPNLog, PLUGIN_LOG_ALWAYS,
          ("NPN Logging Active!\n"));
  MOZ_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_ALWAYS,
          ("General Plugin Logging Active! (nsPluginHost::ctor)\n"));
  MOZ_LOG(nsPluginLogging::gNPPLog, PLUGIN_LOG_ALWAYS,
          ("NPP Logging Active!\n"));

  PLUGIN_LOG(PLUGIN_LOG_ALWAYS, ("nsPluginHost::ctor\n"));
  PR_LogFlush();
#endif

  // Load plugins on creation, as there's a good chance we'll need to send them
  // to content processes directly after creation.
  if (XRE_IsParentProcess()) {
    // Always increment the chrome epoch when we bring up the nsPluginHost in
    // the parent process.
    IncrementChromeEpoch();
  }
}

nsPluginHost::~nsPluginHost() {
  PLUGIN_LOG(PLUGIN_LOG_ALWAYS, ("nsPluginHost::dtor\n"));
}

NS_IMPL_ISUPPORTS(nsPluginHost, nsISupportsWeakReference)

already_AddRefed<nsPluginHost> nsPluginHost::GetInst() {
  if (!sInst) {
    sInst = new nsPluginHost();
    ClearOnShutdown(&sInst);
  }

  return do_AddRef(sInst);
}

bool nsPluginHost::HavePluginForType(const nsACString& aMimeType,
                                     PluginFilter aFilter) {
  bool checkEnabled = aFilter & eExcludeDisabled;
  bool allowFake = !(aFilter & eExcludeFake);
  return FindPluginForType(aMimeType, allowFake, checkEnabled);
}

nsIInternalPluginTag* nsPluginHost::FindPluginForType(
    const nsACString& aMimeType, bool aIncludeFake, bool aCheckEnabled) {
  if (aIncludeFake) {
    return FindFakePluginForType(aMimeType, aCheckEnabled);
  }

  return nullptr;
}

NS_IMETHODIMP
nsPluginHost::GetPluginTagForType(const nsACString& aMimeType,
                                  uint32_t aExcludeFlags,
                                  nsIPluginTag** aResult) {
  bool includeFake = !(aExcludeFlags & eExcludeFake);
  bool includeDisabled = !(aExcludeFlags & eExcludeDisabled);

  // First look for an enabled plugin.
  RefPtr<nsIInternalPluginTag> tag =
      FindPluginForType(aMimeType, includeFake, true);
  if (!tag && includeDisabled) {
    tag = FindPluginForType(aMimeType, includeFake, false);
  }

  if (tag) {
    tag.forget(aResult);
    return NS_OK;
  }

  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsPluginHost::GetPermissionStringForTag(nsIPluginTag* aTag,
                                        uint32_t aExcludeFlags,
                                        nsACString& aPermissionString) {
  NS_ENSURE_TRUE(aTag, NS_ERROR_FAILURE);

  aPermissionString.Truncate();
  uint32_t blocklistState;
  nsresult rv = aTag->GetBlocklistState(&blocklistState);
  NS_ENSURE_SUCCESS(rv, rv);

  aPermissionString.AssignLiteral("plugin:");

  nsCString niceName;
  rv = aTag->GetNiceName(niceName);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(!niceName.IsEmpty(), NS_ERROR_FAILURE);

  aPermissionString.Append(niceName);

  return NS_OK;
}

nsFakePluginTag* nsPluginHost::FindFakePluginForType(
    const nsACString& aMimeType, bool aCheckEnabled) {
  int32_t numFakePlugins = mFakePlugins.Length();
  for (int32_t i = 0; i < numFakePlugins; i++) {
    nsFakePluginTag* plugin = mFakePlugins[i];
    bool active;
    if ((!aCheckEnabled ||
         (NS_SUCCEEDED(plugin->GetActive(&active)) && active)) &&
        plugin->HasMimeType(aMimeType)) {
      return plugin;
    }
  }

  return nullptr;
}

static bool MimeTypeIsAllowedForFakePlugin(const nsString& aMimeType) {
  static const char* const allowedFakePlugins[] = {
      // PDF
      "application/pdf",
      "application/vnd.adobe.pdf",
      "application/vnd.adobe.pdfxml",
      "application/vnd.adobe.x-mars",
      "application/vnd.adobe.xdp+xml",
      "application/vnd.adobe.xfdf",
      "application/vnd.adobe.xfd+xml",
      "application/vnd.fdf",
  };

  for (const auto allowed : allowedFakePlugins) {
    if (aMimeType.EqualsASCII(allowed)) {
      return true;
    }
  }
  return false;
}

nsPluginHost::SpecialType nsPluginHost::GetSpecialType(
    const nsACString& aMIMEType) {
  if (aMIMEType.LowerCaseEqualsASCII("application/x-test")) {
    return eSpecialType_Test;
  }

  if (aMIMEType.LowerCaseEqualsASCII("application/x-shockwave-flash") ||
      aMIMEType.LowerCaseEqualsASCII("application/futuresplash") ||
      aMIMEType.LowerCaseEqualsASCII("application/x-shockwave-flash-test")) {
    return eSpecialType_Flash;
  }

  return eSpecialType_None;
}

// Check whether or not a tag is a live, valid tag, and that it's loaded.
bool nsPluginHost::IsLiveTag(nsIPluginTag* aPluginTag) {
  nsCOMPtr<nsIInternalPluginTag> internalTag(do_QueryInterface(aPluginTag));
  uint32_t fakeCount = mFakePlugins.Length();
  for (uint32_t i = 0; i < fakeCount; i++) {
    if (mFakePlugins[i] == internalTag) {
      return true;
    }
  }
  return false;
}

void nsPluginHost::IncrementChromeEpoch() {
  MOZ_ASSERT(XRE_IsParentProcess());
  mPluginEpoch++;
}

uint32_t nsPluginHost::ChromeEpoch() {
  MOZ_ASSERT(XRE_IsParentProcess());
  return mPluginEpoch;
}

uint32_t nsPluginHost::ChromeEpochForContent() {
  MOZ_ASSERT(XRE_IsContentProcess());
  return mPluginEpoch;
}

void nsPluginHost::SetChromeEpochForContent(uint32_t aEpoch) {
  MOZ_ASSERT(XRE_IsContentProcess());
  mPluginEpoch = aEpoch;
}

/* static */
bool nsPluginHost::CanUsePluginForMIMEType(const nsACString& aMIMEType) {
  // We "support" these in the sense that we show a special transparent
  // fallback element in their place.
  if (nsPluginHost::GetSpecialType(aMIMEType) ==
          nsPluginHost::eSpecialType_Flash ||
      MimeTypeIsAllowedForFakePlugin(NS_ConvertUTF8toUTF16(aMIMEType)) ||
      aMIMEType.LowerCaseEqualsLiteral("application/x-test")) {
    return true;
  }

  return false;
}
