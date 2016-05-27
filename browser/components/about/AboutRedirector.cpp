/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// See also: docshell/base/nsAboutRedirector.cpp

#include "AboutRedirector.h"
#include "nsNetUtil.h"
#include "nsIAboutNewTabService.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsIScriptSecurityManager.h"
#include "nsIProtocolHandler.h"
#include "mozilla/ArrayUtils.h"
#include "nsDOMString.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace browser {

NS_IMPL_ISUPPORTS(AboutRedirector, nsIAboutModule)

struct RedirEntry {
  const char* id;
  const char* url;
  uint32_t flags;
  const char* idbOriginPostfix;
};

/*
  Entries which do not have URI_SAFE_FOR_UNTRUSTED_CONTENT will run with chrome
  privileges. This is potentially dangerous. Please use
  URI_SAFE_FOR_UNTRUSTED_CONTENT in the third argument to each map item below
  unless your about: page really needs chrome privileges. Security review is
  required before adding new map entries without
  URI_SAFE_FOR_UNTRUSTED_CONTENT.
*/
static RedirEntry kRedirMap[] = {
#ifdef MOZ_SAFE_BROWSING
  { "blocked", "chrome://browser/content/blockedSite.xhtml",
    nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
    nsIAboutModule::URI_CAN_LOAD_IN_CHILD |
    nsIAboutModule::ALLOW_SCRIPT |
    nsIAboutModule::HIDE_FROM_ABOUTABOUT },
#endif
  { "certerror", "chrome://browser/content/aboutNetError.xhtml",
    nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
    nsIAboutModule::URI_CAN_LOAD_IN_CHILD |
    nsIAboutModule::ALLOW_SCRIPT |
    nsIAboutModule::HIDE_FROM_ABOUTABOUT },
  { "socialerror", "chrome://browser/content/aboutSocialError.xhtml",
    nsIAboutModule::ALLOW_SCRIPT |
    nsIAboutModule::HIDE_FROM_ABOUTABOUT },
  { "providerdirectory", "chrome://browser/content/aboutProviderDirectory.xhtml",
    nsIAboutModule::ALLOW_SCRIPT |
    nsIAboutModule::HIDE_FROM_ABOUTABOUT },
  { "tabcrashed", "chrome://browser/content/aboutTabCrashed.xhtml",
    nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
    nsIAboutModule::ALLOW_SCRIPT |
    nsIAboutModule::HIDE_FROM_ABOUTABOUT },
  { "feeds", "chrome://browser/content/feeds/subscribe.xhtml",
    nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
    nsIAboutModule::ALLOW_SCRIPT |
    nsIAboutModule::HIDE_FROM_ABOUTABOUT },
  { "privatebrowsing", "chrome://browser/content/aboutPrivateBrowsing.xhtml",
    nsIAboutModule::URI_MUST_LOAD_IN_CHILD |
    nsIAboutModule::ALLOW_SCRIPT },
  { "rights",
    "chrome://global/content/aboutRights.xhtml",
    nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
    nsIAboutModule::ALLOW_SCRIPT },
  { "robots", "chrome://browser/content/aboutRobots.xhtml",
    nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
    nsIAboutModule::ALLOW_SCRIPT },
  { "searchreset", "chrome://browser/content/search/searchReset.xhtml",
    nsIAboutModule::ALLOW_SCRIPT |
    nsIAboutModule::HIDE_FROM_ABOUTABOUT },
  { "sessionrestore", "chrome://browser/content/aboutSessionRestore.xhtml",
    nsIAboutModule::ALLOW_SCRIPT },
  { "welcomeback", "chrome://browser/content/aboutWelcomeBack.xhtml",
    nsIAboutModule::ALLOW_SCRIPT },
  { "sync-tabs", "chrome://browser/content/sync/aboutSyncTabs.xul",
    nsIAboutModule::ALLOW_SCRIPT },
  // Linkable because of indexeddb use (bug 1228118)
  { "home", "chrome://browser/content/abouthome/aboutHome.xhtml",
    nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
    nsIAboutModule::URI_MUST_LOAD_IN_CHILD |
    nsIAboutModule::ALLOW_SCRIPT |
    nsIAboutModule::MAKE_LINKABLE |
    nsIAboutModule::ENABLE_INDEXED_DB },
  // the newtab's actual URL will be determined when the channel is created
  { "newtab", "about:blank",
    nsIAboutModule::ALLOW_SCRIPT },
  { "preferences", "chrome://browser/content/preferences/in-content/preferences.xul",
    nsIAboutModule::ALLOW_SCRIPT },
  { "downloads", "chrome://browser/content/downloads/contentAreaDownloadsView.xul",
    nsIAboutModule::ALLOW_SCRIPT },
#ifdef MOZ_SERVICES_HEALTHREPORT
  { "healthreport", "chrome://browser/content/abouthealthreport/abouthealth.xhtml",
    nsIAboutModule::ALLOW_SCRIPT },
#endif
  { "accounts", "chrome://browser/content/aboutaccounts/aboutaccounts.xhtml",
    nsIAboutModule::ALLOW_SCRIPT },
  // Linkable because of indexeddb use (bug 1228118)
  { "loopconversation", "chrome://loop/content/panels/conversation.html",
    nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
    nsIAboutModule::ALLOW_SCRIPT |
    nsIAboutModule::HIDE_FROM_ABOUTABOUT |
    nsIAboutModule::MAKE_LINKABLE |
    nsIAboutModule::ENABLE_INDEXED_DB },
  // Linkable because of indexeddb use (bug 1228118)
  { "looppanel", "chrome://loop/content/panels/panel.html",
    nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
    nsIAboutModule::ALLOW_SCRIPT |
    nsIAboutModule::HIDE_FROM_ABOUTABOUT |
    nsIAboutModule::MAKE_LINKABLE |
    nsIAboutModule::ENABLE_INDEXED_DB,
    // Shares an IndexedDB origin with about:loopconversation.
    "loopconversation" },
  { "reader", "chrome://global/content/reader/aboutReader.html",
    nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
    nsIAboutModule::ALLOW_SCRIPT |
    nsIAboutModule::URI_MUST_LOAD_IN_CHILD |
    nsIAboutModule::HIDE_FROM_ABOUTABOUT },
};
static const int kRedirTotal = ArrayLength(kRedirMap);

static nsAutoCString
GetAboutModuleName(nsIURI *aURI)
{
  nsAutoCString path;
  aURI->GetPath(path);

  int32_t f = path.FindChar('#');
  if (f >= 0)
    path.SetLength(f);

  f = path.FindChar('?');
  if (f >= 0)
    path.SetLength(f);

  ToLowerCase(path);
  return path;
}

NS_IMETHODIMP
AboutRedirector::NewChannel(nsIURI* aURI,
                            nsILoadInfo* aLoadInfo,
                            nsIChannel** result)
{
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ASSERTION(result, "must not be null");

  nsAutoCString path = GetAboutModuleName(aURI);

  nsresult rv;
  nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  for (int i = 0; i < kRedirTotal; i++) {
    if (!strcmp(path.get(), kRedirMap[i].id)) {
      nsAutoCString url;
      nsLoadFlags loadFlags = static_cast<nsLoadFlags>(nsIChannel::LOAD_NORMAL);

      if (path.EqualsLiteral("newtab")) {
        // let the aboutNewTabService decide where to redirect
        nsCOMPtr<nsIAboutNewTabService> aboutNewTabService =
          do_GetService("@mozilla.org/browser/aboutnewtab-service;1", &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = aboutNewTabService->GetDefaultURL(url);
        NS_ENSURE_SUCCESS(rv, rv);

        // if about:newtab points to an external resource we have to make sure
        // the content is signed and trusted
        bool remoteEnabled = false;
        rv = aboutNewTabService->GetRemoteEnabled(&remoteEnabled);
        NS_ENSURE_SUCCESS(rv, rv);
        if (remoteEnabled) {
          NS_ENSURE_ARG_POINTER(aLoadInfo);
          aLoadInfo->SetVerifySignedContent(true);
          loadFlags = static_cast<nsLoadFlags>(nsIChannel::LOAD_REPLACE);
        }
      }
      // fall back to the specified url in the map
      if (url.IsEmpty()) {
        url.AssignASCII(kRedirMap[i].url);
      }

      nsCOMPtr<nsIChannel> tempChannel;
      nsCOMPtr<nsIURI> tempURI;
      rv = NS_NewURI(getter_AddRefs(tempURI), url);
      NS_ENSURE_SUCCESS(rv, rv);

      // If tempURI links to an external URI (i.e. something other than
      // chrome:// or resource://) then set the LOAD_REPLACE flag on the
      // channel which forces the channel owner to reflect the displayed
      // URL rather then being the systemPrincipal.
      bool isUIResource = false;
      rv = NS_URIChainHasFlags(tempURI, nsIProtocolHandler::URI_IS_UI_RESOURCE,
                               &isUIResource);
      NS_ENSURE_SUCCESS(rv, rv);

      loadFlags = isUIResource
                    ? static_cast<nsLoadFlags>(nsIChannel::LOAD_NORMAL)
                    : static_cast<nsLoadFlags>(nsIChannel::LOAD_REPLACE);

      rv = NS_NewChannelInternal(getter_AddRefs(tempChannel),
                                 tempURI,
                                 aLoadInfo,
                                 nullptr, // aLoadGroup
                                 nullptr, // aCallbacks
                                 loadFlags);
      NS_ENSURE_SUCCESS(rv, rv);

      tempChannel->SetOriginalURI(aURI);

      NS_ADDREF(*result = tempChannel);
      return rv;
    }
  }

  return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
AboutRedirector::GetURIFlags(nsIURI *aURI, uint32_t *result)
{
  NS_ENSURE_ARG_POINTER(aURI);

  nsAutoCString name = GetAboutModuleName(aURI);

  for (int i = 0; i < kRedirTotal; i++) {
    if (name.Equals(kRedirMap[i].id)) {
      *result = kRedirMap[i].flags;
      return NS_OK;
    }
  }

  return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
AboutRedirector::GetIndexedDBOriginPostfix(nsIURI *aURI, nsAString &result)
{
  NS_ENSURE_ARG_POINTER(aURI);

  nsAutoCString name = GetAboutModuleName(aURI);

  for (int i = 0; i < kRedirTotal; i++) {
    if (name.Equals(kRedirMap[i].id)) {
      const char* postfix = kRedirMap[i].idbOriginPostfix;
      if (!postfix) {
        break;
      }

      result.AssignASCII(postfix);
      return NS_OK;
    }
  }

  SetDOMStringToNull(result);
  return NS_ERROR_ILLEGAL_VALUE;
}

nsresult
AboutRedirector::Create(nsISupports *aOuter, REFNSIID aIID, void **result)
{
  AboutRedirector* about = new AboutRedirector();
  if (about == nullptr)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(about);
  nsresult rv = about->QueryInterface(aIID, result);
  NS_RELEASE(about);
  return rv;
}

} // namespace browser
} // namespace mozilla
