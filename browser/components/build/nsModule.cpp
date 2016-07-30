/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"

#include "nsBrowserCompsCID.h"
#include "DirectoryProvider.h"

#if defined(XP_WIN)
#include "nsWindowsShellService.h"
#elif defined(XP_MACOSX)
#include "nsMacShellService.h"
#elif defined(MOZ_WIDGET_GTK)
#include "nsGNOMEShellService.h"
#endif

#if defined(XP_WIN)
#include "nsIEHistoryEnumerator.h"
#endif

#include "rdf.h"
#include "nsFeedSniffer.h"
#include "AboutRedirector.h"
#include "nsIAboutModule.h"

#include "nsNetCID.h"

using namespace mozilla::browser;

/////////////////////////////////////////////////////////////////////////////

NS_GENERIC_FACTORY_CONSTRUCTOR(DirectoryProvider)
#if defined(XP_WIN)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindowsShellService)
#elif defined(XP_MACOSX)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacShellService)
#elif defined(MOZ_WIDGET_GTK)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsGNOMEShellService, Init)
#endif

#if defined(XP_WIN)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsIEHistoryEnumerator)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(nsFeedSniffer)

NS_DEFINE_NAMED_CID(NS_BROWSERDIRECTORYPROVIDER_CID);
#if defined(XP_WIN)
NS_DEFINE_NAMED_CID(NS_SHELLSERVICE_CID);
#elif defined(MOZ_WIDGET_GTK)
NS_DEFINE_NAMED_CID(NS_SHELLSERVICE_CID);
#endif
NS_DEFINE_NAMED_CID(NS_FEEDSNIFFER_CID);
NS_DEFINE_NAMED_CID(NS_BROWSER_ABOUT_REDIRECTOR_CID);
#if defined(XP_WIN)
NS_DEFINE_NAMED_CID(NS_WINIEHISTORYENUMERATOR_CID);
#elif defined(XP_MACOSX)
NS_DEFINE_NAMED_CID(NS_SHELLSERVICE_CID);
#endif

static const mozilla::Module::CIDEntry kBrowserCIDs[] = {
    { &kNS_BROWSERDIRECTORYPROVIDER_CID, false, nullptr, DirectoryProviderConstructor },
#if defined(XP_WIN)
    { &kNS_SHELLSERVICE_CID, false, nullptr, nsWindowsShellServiceConstructor },
#elif defined(MOZ_WIDGET_GTK)
    { &kNS_SHELLSERVICE_CID, false, nullptr, nsGNOMEShellServiceConstructor },
#endif
    { &kNS_FEEDSNIFFER_CID, false, nullptr, nsFeedSnifferConstructor },
    { &kNS_BROWSER_ABOUT_REDIRECTOR_CID, false, nullptr, AboutRedirector::Create },
#if defined(XP_WIN)
    { &kNS_WINIEHISTORYENUMERATOR_CID, false, nullptr, nsIEHistoryEnumeratorConstructor },
#elif defined(XP_MACOSX)
    { &kNS_SHELLSERVICE_CID, false, nullptr, nsMacShellServiceConstructor },
#endif
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kBrowserContracts[] = {
    { NS_BROWSERDIRECTORYPROVIDER_CONTRACTID, &kNS_BROWSERDIRECTORYPROVIDER_CID },
#if defined(XP_WIN)
    { NS_SHELLSERVICE_CONTRACTID, &kNS_SHELLSERVICE_CID },
#elif defined(MOZ_WIDGET_GTK)
    { NS_SHELLSERVICE_CONTRACTID, &kNS_SHELLSERVICE_CID },
#endif
    { NS_FEEDSNIFFER_CONTRACTID, &kNS_FEEDSNIFFER_CID },
#ifdef MOZ_SAFE_BROWSING
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "blocked", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
#endif
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "certerror", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "socialerror", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "providerdirectory", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "tabcrashed", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "feeds", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "privatebrowsing", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "rights", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "robots", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "searchreset", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "sessionrestore", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "welcomeback", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "sync-tabs", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "home", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "newtab", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "preferences", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "downloads", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "accounts", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
#ifdef MOZ_SERVICES_HEALTHREPORT
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "healthreport", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
#endif
    { NS_ABOUT_MODULE_CONTRACTID_PREFIX "reader", &kNS_BROWSER_ABOUT_REDIRECTOR_CID },
#if defined(XP_WIN)
    { NS_IEHISTORYENUMERATOR_CONTRACTID, &kNS_WINIEHISTORYENUMERATOR_CID },
#elif defined(XP_MACOSX)
    { NS_SHELLSERVICE_CONTRACTID, &kNS_SHELLSERVICE_CID },
#endif
    { nullptr }
};

static const mozilla::Module::CategoryEntry kBrowserCategories[] = {
    { XPCOM_DIRECTORY_PROVIDER_CATEGORY, "browser-directory-provider", NS_BROWSERDIRECTORYPROVIDER_CONTRACTID },
    { NS_CONTENT_SNIFFER_CATEGORY, "Feed Sniffer", NS_FEEDSNIFFER_CONTRACTID },
    { nullptr }
};

static const mozilla::Module kBrowserModule = {
    mozilla::Module::kVersion,
    kBrowserCIDs,
    kBrowserContracts,
    kBrowserCategories
};

NSMODULE_DEFN(nsBrowserCompsModule) = &kBrowserModule;
