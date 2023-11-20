/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* nsPluginHost.cpp - top-level plugin management code */

#include "nsPluginHost.h"

#include "nscore.h"

#include <cstdlib>
#include <stdio.h>
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsXULAppAPI.h"

using namespace mozilla;

StaticRefPtr<nsPluginHost> nsPluginHost::sInst;

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
