/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAboutRedirector.h"
#include "nsNetUtil.h"
#include "nsAboutProtocolUtils.h"
#include "nsBaseChannel.h"
#include "mozilla/ArrayUtils.h"
#include "nsIProtocolHandler.h"
#include "nsXULAppAPI.h"
#include "mozilla/Preferences.h"

#define ABOUT_CONFIG_ENABLED_PREF "general.aboutConfig.enable"

NS_IMPL_ISUPPORTS(nsAboutRedirector, nsIAboutModule)

struct RedirEntry {
  const char* id;
  const char* url;
  uint32_t flags;
};

class CrashChannel final : public nsBaseChannel {
 public:
  explicit CrashChannel(nsIURI* aURI) { SetURI(aURI); }

  nsresult OpenContentStream(bool async, nsIInputStream** stream,
                             nsIChannel** channel) override {
    nsAutoCString spec;
    mURI->GetSpec(spec);

    if (spec.EqualsASCII("about:crashparent") && XRE_IsParentProcess()) {
      MOZ_CRASH("Crash via about:crashparent");
    }

    if (spec.EqualsASCII("about:crashcontent") && XRE_IsContentProcess()) {
      MOZ_CRASH("Crash via about:crashcontent");
    }

    NS_WARNING("Unhandled about:crash* URI or wrong process");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

 protected:
  virtual ~CrashChannel() = default;
};

/*
  Entries which do not have URI_SAFE_FOR_UNTRUSTED_CONTENT will run with chrome
  privileges. This is potentially dangerous. Please use
  URI_SAFE_FOR_UNTRUSTED_CONTENT in the third argument to each map item below
  unless your about: page really needs chrome privileges. Security review is
  required before adding new map entries without
  URI_SAFE_FOR_UNTRUSTED_CONTENT.

  URI_SAFE_FOR_UNTRUSTED_CONTENT is not enough to let web pages load that page,
  for that you need MAKE_LINKABLE.
 */
static const RedirEntry kRedirMap[] = {
    {"about", "chrome://global/content/aboutAbout.html", 0},
    {"addons", "chrome://mozapps/content/extensions/extensions.xhtml",
     nsIAboutModule::ALLOW_SCRIPT},
    {"buildconfig", "chrome://global/content/buildconfig.html",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT},
    {"checkerboard", "chrome://global/content/aboutCheckerboard.html",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
         nsIAboutModule::ALLOW_SCRIPT},
#ifndef MOZ_BUILD_APP_IS_BROWSER
    {"config", "chrome://global/content/config.xhtml", 0},
#endif
#ifdef MOZ_CRASHREPORTER
    {"crashes", "chrome://global/content/crashes.html", 0},
#endif
    {"credits", "https://www.mozilla.org/credits/",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT},
    {"license", "chrome://global/content/license.html",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT},
    {"logo", "chrome://branding/content/about.png",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
         // Linkable for testing reasons.
         nsIAboutModule::MAKE_LINKABLE},
    {"memory", "chrome://global/content/aboutMemory.xhtml",
     nsIAboutModule::ALLOW_SCRIPT},
    {"certificate", "chrome://global/content/certviewer/certviewer.html",
     nsIAboutModule::ALLOW_SCRIPT |
         nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
         nsIAboutModule::URI_MUST_LOAD_IN_CHILD |
         nsIAboutModule::URI_CAN_LOAD_IN_PRIVILEGEDABOUT_PROCESS},
    {"mozilla", "chrome://global/content/mozilla.xhtml",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT},
    {"neterror", "chrome://global/content/netError.xhtml",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
         nsIAboutModule::URI_CAN_LOAD_IN_CHILD | nsIAboutModule::ALLOW_SCRIPT |
         nsIAboutModule::HIDE_FROM_ABOUTABOUT},
    {"networking", "chrome://global/content/aboutNetworking.html",
     nsIAboutModule::ALLOW_SCRIPT},
    {"performance", "chrome://global/content/aboutPerformance.html",
     nsIAboutModule::ALLOW_SCRIPT},
    {"plugins", "chrome://global/content/plugins.html",
     nsIAboutModule::URI_MUST_LOAD_IN_CHILD},
    {"processes", "chrome://global/content/aboutProcesses.html",
     nsIAboutModule::ALLOW_SCRIPT},
    // about:serviceworkers always wants to load in the parent process because
    // when dom.serviceWorkers.parent_intercept is set to true (the new default)
    // then the only place nsIServiceWorkerManager has any data is in the
    // parent process.
    //
    // There is overlap without about:debugging, but about:debugging is not
    // available on mobile at this time, and it's useful to be able to know if
    // a ServiceWorker is registered directly from the mobile browser without
    // having to connect the device to a desktop machine and all that entails.
    {"serviceworkers", "chrome://global/content/aboutServiceWorkers.xhtml",
     nsIAboutModule::ALLOW_SCRIPT},
#ifndef ANDROID
    {"profiles", "chrome://global/content/aboutProfiles.xhtml",
     nsIAboutModule::ALLOW_SCRIPT},
#endif
    // about:srcdoc is unresolvable by specification.  It is included here
    // because the security manager would disallow srcdoc iframes otherwise.
    {"srcdoc", "about:blank",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
         nsIAboutModule::HIDE_FROM_ABOUTABOUT |
         // Needs to be linkable so content can touch its own srcdoc frames
         nsIAboutModule::MAKE_LINKABLE | nsIAboutModule::URI_CAN_LOAD_IN_CHILD},
    {"support", "chrome://global/content/aboutSupport.xhtml",
     nsIAboutModule::ALLOW_SCRIPT},
    {"telemetry", "chrome://global/content/aboutTelemetry.xhtml",
     nsIAboutModule::ALLOW_SCRIPT},
    {"url-classifier", "chrome://global/content/aboutUrlClassifier.xhtml",
     nsIAboutModule::ALLOW_SCRIPT},
    {"webrtc", "chrome://global/content/aboutwebrtc/aboutWebrtc.html",
     nsIAboutModule::ALLOW_SCRIPT},
    {"printpreview", "about:blank",
     nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
         nsIAboutModule::HIDE_FROM_ABOUTABOUT |
         nsIAboutModule::URI_CAN_LOAD_IN_CHILD},
    {"crashparent", "about:blank", nsIAboutModule::HIDE_FROM_ABOUTABOUT},
    {"crashcontent", "about:blank",
     nsIAboutModule::HIDE_FROM_ABOUTABOUT |
         nsIAboutModule::URI_CAN_LOAD_IN_CHILD |
         nsIAboutModule::URI_MUST_LOAD_IN_CHILD}};
static const int kRedirTotal = mozilla::ArrayLength(kRedirMap);

NS_IMETHODIMP
nsAboutRedirector::NewChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                              nsIChannel** aResult) {
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aLoadInfo);
  NS_ASSERTION(aResult, "must not be null");

  nsAutoCString path;
  nsresult rv = NS_GetAboutModuleName(aURI, path);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (path.EqualsASCII("crashparent") || path.EqualsASCII("crashcontent")) {
    bool isExternal;
    aLoadInfo->GetLoadTriggeredFromExternal(&isExternal);
    if (isExternal) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    nsCOMPtr<nsIChannel> channel = new CrashChannel(aURI);
    channel->SetLoadInfo(aLoadInfo);
    channel.forget(aResult);
    return NS_OK;
  }

  if (path.EqualsASCII("config") &&
      !mozilla::Preferences::GetBool(ABOUT_CONFIG_ENABLED_PREF, true)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  for (int i = 0; i < kRedirTotal; i++) {
    if (!strcmp(path.get(), kRedirMap[i].id)) {
      nsCOMPtr<nsIChannel> tempChannel;
      nsCOMPtr<nsIURI> tempURI;
      rv = NS_NewURI(getter_AddRefs(tempURI), kRedirMap[i].url);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = NS_NewChannelInternal(getter_AddRefs(tempChannel), tempURI,
                                 aLoadInfo);
      NS_ENSURE_SUCCESS(rv, rv);

      // If tempURI links to an external URI (i.e. something other than
      // chrome:// or resource://) then set result principal URI on the
      // load info which forces the channel principal to reflect the displayed
      // URL rather then being the systemPrincipal.
      bool isUIResource = false;
      rv = NS_URIChainHasFlags(tempURI, nsIProtocolHandler::URI_IS_UI_RESOURCE,
                               &isUIResource);
      NS_ENSURE_SUCCESS(rv, rv);

      bool isAboutBlank = NS_IsAboutBlank(tempURI);

      if (!isUIResource && !isAboutBlank) {
        aLoadInfo->SetResultPrincipalURI(tempURI);
      }

      tempChannel->SetOriginalURI(aURI);

      tempChannel.forget(aResult);
      return rv;
    }
  }

  NS_ERROR("nsAboutRedirector called for unknown case");
  return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
nsAboutRedirector::GetURIFlags(nsIURI* aURI, uint32_t* aResult) {
  NS_ENSURE_ARG_POINTER(aURI);

  nsAutoCString name;
  nsresult rv = NS_GetAboutModuleName(aURI, name);
  NS_ENSURE_SUCCESS(rv, rv);

  for (int i = 0; i < kRedirTotal; i++) {
    if (name.EqualsASCII(kRedirMap[i].id)) {
      *aResult = kRedirMap[i].flags;
      return NS_OK;
    }
  }

  NS_ERROR("nsAboutRedirector called for unknown case");
  return NS_ERROR_ILLEGAL_VALUE;
}

nsresult nsAboutRedirector::Create(nsISupports* aOuter, REFNSIID aIID,
                                   void** aResult) {
  RefPtr<nsAboutRedirector> about = new nsAboutRedirector();
  return about->QueryInterface(aIID, aResult);
}
