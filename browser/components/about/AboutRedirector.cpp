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
#include "nsIProtocolHandler.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/Preferences.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"

namespace mozilla {
namespace browser {

NS_IMPL_ISUPPORTS(AboutRedirector, nsIAboutModule)

static const uint32_t ACTIVITY_STREAM_FLAGS =
    nsIAboutModule::ALLOW_SCRIPT | nsIAboutModule::ENABLE_INDEXED_DB |
    nsIAboutModule::URI_MUST_LOAD_IN_CHILD |
    nsIAboutModule::URI_CAN_LOAD_IN_PRIVILEGEDABOUT_PROCESS |
    nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
    nsIAboutModule::ALLOW_UNSANITIZED_CONTENT;

struct RedirEntry {
  const char* id;
  const char* url;
  uint32_t flags;
};

/*
  Entries which do not have URI_SAFE_FOR_UNTRUSTED_CONTENT will run with chrome
  privileges. This is potentially dangerous. Please use
  URI_SAFE_FOR_UNTRUSTED_CONTENT in the third argument to each map item below
  unless your about: page really needs chrome privileges. Security review is
  required before adding new map entries without
  URI_SAFE_FOR_UNTRUSTED_CONTENT.

  NOTE: changes to this redir map need to be accompanied with changes to
    browser/components/about/components.conf
*/
static const RedirEntry kRedirMap[] = {
    {"blocked", "chrome://browser/content/blockedSite.xhtml",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
         nsIAboutModule::URI_CAN_LOAD_IN_CHILD | nsIAboutModule::ALLOW_SCRIPT |
         nsIAboutModule::HIDE_FROM_ABOUTABOUT},
    {"certerror", "chrome://browser/content/certerror/aboutNetError.xhtml",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
         nsIAboutModule::URI_CAN_LOAD_IN_CHILD | nsIAboutModule::ALLOW_SCRIPT |
         nsIAboutModule::HIDE_FROM_ABOUTABOUT},
    {"framecrashed", "chrome://browser/content/aboutFrameCrashed.html",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
         nsIAboutModule::HIDE_FROM_ABOUTABOUT},
    {"logins", "chrome://browser/content/aboutlogins/aboutLogins.html",
     nsIAboutModule::ALLOW_SCRIPT | nsIAboutModule::URI_MUST_LOAD_IN_CHILD |
         nsIAboutModule::URI_CAN_LOAD_IN_PRIVILEGEDABOUT_PROCESS |
         nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT},
    {"loginsimportreport",
     "chrome://browser/content/aboutlogins/aboutLoginsImportReport.html",
     nsIAboutModule::ALLOW_SCRIPT | nsIAboutModule::URI_MUST_LOAD_IN_CHILD |
         nsIAboutModule::URI_CAN_LOAD_IN_PRIVILEGEDABOUT_PROCESS |
         nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT},
    {"tabcrashed", "chrome://browser/content/aboutTabCrashed.xhtml",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
         nsIAboutModule::ALLOW_SCRIPT | nsIAboutModule::HIDE_FROM_ABOUTABOUT},
    {"policies", "chrome://browser/content/policies/aboutPolicies.html",
     nsIAboutModule::ALLOW_SCRIPT},
    {"privatebrowsing", "chrome://browser/content/aboutPrivateBrowsing.html",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
         nsIAboutModule::URI_MUST_LOAD_IN_CHILD | nsIAboutModule::ALLOW_SCRIPT |
         nsIAboutModule::URI_CAN_LOAD_IN_PRIVILEGEDABOUT_PROCESS},
    {"profiling",
     "chrome://devtools/content/performance-new/aboutprofiling/index.xhtml",
     nsIAboutModule::ALLOW_SCRIPT},
    {"rights", "chrome://global/content/aboutRights.xhtml",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
         nsIAboutModule::ALLOW_SCRIPT},
    {"robots", "chrome://browser/content/aboutRobots.xhtml",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
         nsIAboutModule::ALLOW_SCRIPT},
    {"sessionrestore", "chrome://browser/content/aboutSessionRestore.xhtml",
     nsIAboutModule::ALLOW_SCRIPT | nsIAboutModule::HIDE_FROM_ABOUTABOUT},
    {"welcomeback", "chrome://browser/content/aboutWelcomeBack.xhtml",
     nsIAboutModule::ALLOW_SCRIPT | nsIAboutModule::HIDE_FROM_ABOUTABOUT},
    // Actual activity stream URL for home and newtab are set in channel
    // creation
    {"home", "about:blank", ACTIVITY_STREAM_FLAGS},
    {"newtab", "about:blank", ACTIVITY_STREAM_FLAGS},
    {"welcome", "about:blank",
     nsIAboutModule::URI_MUST_LOAD_IN_CHILD |
         nsIAboutModule::URI_CAN_LOAD_IN_PRIVILEGEDABOUT_PROCESS |
         nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
         nsIAboutModule::ALLOW_SCRIPT},
    {"pocket-saved", "chrome://pocket/content/panels/saved.html",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
         nsIAboutModule::URI_CAN_LOAD_IN_CHILD | nsIAboutModule::ALLOW_SCRIPT |
         nsIAboutModule::HIDE_FROM_ABOUTABOUT},
    {"pocket-signup", "chrome://pocket/content/panels/signup.html",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
         nsIAboutModule::URI_CAN_LOAD_IN_CHILD | nsIAboutModule::ALLOW_SCRIPT |
         nsIAboutModule::HIDE_FROM_ABOUTABOUT},
    {"preferences", "chrome://browser/content/preferences/preferences.xhtml",
     nsIAboutModule::ALLOW_SCRIPT},
    {"downloads",
     "chrome://browser/content/downloads/contentAreaDownloadsView.xhtml",
     nsIAboutModule::ALLOW_SCRIPT},
    {"reader", "chrome://global/content/reader/aboutReader.html",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
         nsIAboutModule::ALLOW_SCRIPT | nsIAboutModule::URI_MUST_LOAD_IN_CHILD |
         nsIAboutModule::HIDE_FROM_ABOUTABOUT},
    {"restartrequired", "chrome://browser/content/aboutRestartRequired.xhtml",
     nsIAboutModule::ALLOW_SCRIPT | nsIAboutModule::HIDE_FROM_ABOUTABOUT},
    {"newinstall", "chrome://browser/content/newInstallPage.html",
     nsIAboutModule::URI_MUST_LOAD_IN_CHILD |
         nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
         nsIAboutModule::ALLOW_SCRIPT | nsIAboutModule::HIDE_FROM_ABOUTABOUT},
    {"protections", "chrome://browser/content/protections.html",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
         nsIAboutModule::URI_MUST_LOAD_IN_CHILD | nsIAboutModule::ALLOW_SCRIPT |
         nsIAboutModule::URI_CAN_LOAD_IN_PRIVILEGEDABOUT_PROCESS},
    {"ion", "chrome://browser/content/ion.html",
     nsIAboutModule::ALLOW_SCRIPT | nsIAboutModule::HIDE_FROM_ABOUTABOUT},
};

static nsAutoCString GetAboutModuleName(nsIURI* aURI) {
  nsAutoCString path;
  aURI->GetPathQueryRef(path);

  int32_t f = path.FindChar('#');
  if (f >= 0) path.SetLength(f);

  f = path.FindChar('?');
  if (f >= 0) path.SetLength(f);

  ToLowerCase(path);
  return path;
}

NS_IMETHODIMP
AboutRedirector::NewChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                            nsIChannel** result) {
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aLoadInfo);

  NS_ASSERTION(result, "must not be null");

  nsAutoCString path = GetAboutModuleName(aURI);

  nsresult rv;
  nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we're accessing about:home in the "privileged about content
  // process", then we give the nsIAboutNewTabService the responsibility
  // to return the nsIChannel, since it might be from the about:home
  // startup cache.
  if (XRE_IsContentProcess() && path.EqualsLiteral("home")) {
    auto& remoteType = dom::ContentChild::GetSingleton()->GetRemoteType();
    if (remoteType == PRIVILEGEDABOUT_REMOTE_TYPE) {
      nsCOMPtr<nsIAboutNewTabService> aboutNewTabService =
          do_GetService("@mozilla.org/browser/aboutnewtab-service;1", &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      return aboutNewTabService->AboutHomeChannel(aURI, aLoadInfo, result);
    }
  }

  for (auto& redir : kRedirMap) {
    if (!strcmp(path.get(), redir.id)) {
      nsAutoCString url;

      // Let the aboutNewTabService decide where to redirect for about:home and
      // enabled about:newtab. Disabled about:newtab page uses fallback.
      if (path.EqualsLiteral("home") ||
          (StaticPrefs::browser_newtabpage_enabled() &&
           path.EqualsLiteral("newtab"))) {
        nsCOMPtr<nsIAboutNewTabService> aboutNewTabService =
            do_GetService("@mozilla.org/browser/aboutnewtab-service;1", &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = aboutNewTabService->GetDefaultURL(url);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (path.EqualsLiteral("welcome")) {
        nsCOMPtr<nsIAboutNewTabService> aboutNewTabService =
            do_GetService("@mozilla.org/browser/aboutnewtab-service;1", &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = aboutNewTabService->GetWelcomeURL(url);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // fall back to the specified url in the map
      if (url.IsEmpty()) {
        url.AssignASCII(redir.url);
      }

      nsCOMPtr<nsIChannel> tempChannel;
      nsCOMPtr<nsIURI> tempURI;
      rv = NS_NewURI(getter_AddRefs(tempURI), url);
      NS_ENSURE_SUCCESS(rv, rv);

      // If tempURI links to an external URI (i.e. something other than
      // chrome:// or resource://) then set the result principal URI on the
      // load info which forces the channel prncipal to reflect the displayed
      // URL rather then being the systemPrincipal.
      bool isUIResource = false;
      rv = NS_URIChainHasFlags(tempURI, nsIProtocolHandler::URI_IS_UI_RESOURCE,
                               &isUIResource);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = NS_NewChannelInternal(getter_AddRefs(tempChannel), tempURI,
                                 aLoadInfo);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!isUIResource) {
        aLoadInfo->SetResultPrincipalURI(tempURI);
      }
      tempChannel->SetOriginalURI(aURI);

      NS_ADDREF(*result = tempChannel);
      return rv;
    }
  }

  return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
AboutRedirector::GetURIFlags(nsIURI* aURI, uint32_t* result) {
  NS_ENSURE_ARG_POINTER(aURI);

  nsAutoCString name = GetAboutModuleName(aURI);

  for (auto& redir : kRedirMap) {
    if (name.Equals(redir.id)) {
      *result = redir.flags;
      return NS_OK;
    }
  }

  return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
AboutRedirector::GetChromeURI(nsIURI* aURI, nsIURI** chromeURI) {
  NS_ENSURE_ARG_POINTER(aURI);

  nsAutoCString name = GetAboutModuleName(aURI);

  for (const auto& redir : kRedirMap) {
    if (name.Equals(redir.id)) {
      return NS_NewURI(chromeURI, redir.url);
    }
  }

  return NS_ERROR_ILLEGAL_VALUE;
}

nsresult AboutRedirector::Create(nsISupports* aOuter, REFNSIID aIID,
                                 void** result) {
  AboutRedirector* about = new AboutRedirector();
  if (about == nullptr) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(about);
  nsresult rv = about->QueryInterface(aIID, result);
  NS_RELEASE(about);
  return rv;
}

}  // namespace browser
}  // namespace mozilla
