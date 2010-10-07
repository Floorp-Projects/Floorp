/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/ModuleUtils.h"
#include "nsDocShellCID.h"

#include "nsDocShell.h"
#include "nsDefaultURIFixup.h"
#include "nsWebNavigationInfo.h"

#include "nsAboutRedirector.h"

// uriloader
#include "nsURILoader.h"
#include "nsDocLoader.h"
#include "nsOSHelperAppService.h"
#include "nsExternalProtocolHandler.h"
#include "nsPrefetchService.h"
#include "nsOfflineCacheUpdate.h"
#include "nsLocalHandlerApp.h"
#ifdef MOZ_ENABLE_DBUS
#include "nsDBusHandlerApp.h"
#endif 
#ifdef ANDROID
#include "nsExternalSharingAppService.h"
#endif

// session history
#include "nsSHEntry.h"
#include "nsSHistory.h"
#include "nsSHTransaction.h"

// download history
#include "nsDownloadHistory.h"

static PRBool gInitialized = PR_FALSE;

// The one time initialization for this module
static nsresult
Initialize()
{
  NS_PRECONDITION(!gInitialized, "docshell module already initialized");
  if (gInitialized) {
    return NS_OK;
  }
  gInitialized = PR_TRUE;

  nsresult rv = nsSHistory::Startup();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsSHEntry::Startup();
  return rv;
}

static void
Shutdown()
{
  nsSHEntry::Shutdown();
  gInitialized = PR_FALSE;
}

// docshell
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsDocShell, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDefaultURIFixup)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsWebNavigationInfo, Init)

// uriloader
NS_GENERIC_FACTORY_CONSTRUCTOR(nsURILoader)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsDocLoader, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsOSHelperAppService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsExternalProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrefetchService, Init)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsOfflineCacheUpdateService,
                                         nsOfflineCacheUpdateService::GetInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsOfflineCacheUpdate)
NS_GENERIC_FACTORY_CONSTRUCTOR(PlatformLocalHandlerApp_t)
#ifdef MOZ_ENABLE_DBUS
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDBusHandlerApp)
#endif 
#ifdef ANDROID
NS_GENERIC_FACTORY_CONSTRUCTOR(nsExternalSharingAppService)
#endif

// session history
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSHEntry)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSHTransaction)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSHistory)

// download history
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDownloadHistory)

NS_DEFINE_NAMED_CID(NS_DOCSHELL_CID);
NS_DEFINE_NAMED_CID(NS_DEFAULTURIFIXUP_CID);
NS_DEFINE_NAMED_CID(NS_WEBNAVIGATION_INFO_CID);
NS_DEFINE_NAMED_CID(NS_ABOUT_REDIRECTOR_MODULE_CID);
NS_DEFINE_NAMED_CID(NS_URI_LOADER_CID);
NS_DEFINE_NAMED_CID(NS_DOCUMENTLOADER_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_EXTERNALHELPERAPPSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_EXTERNALPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_PREFETCHSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_OFFLINECACHEUPDATESERVICE_CID);
NS_DEFINE_NAMED_CID(NS_OFFLINECACHEUPDATE_CID);
NS_DEFINE_NAMED_CID(NS_LOCALHANDLERAPP_CID);
#ifdef MOZ_ENABLE_DBUS
NS_DEFINE_NAMED_CID(NS_DBUSHANDLERAPP_CID);
#endif
#ifdef ANDROID
NS_DEFINE_NAMED_CID(NS_EXTERNALSHARINGAPPSERVICE_CID);
#endif
NS_DEFINE_NAMED_CID(NS_SHENTRY_CID);
NS_DEFINE_NAMED_CID(NS_HISTORYENTRY_CID);
NS_DEFINE_NAMED_CID(NS_SHTRANSACTION_CID);
NS_DEFINE_NAMED_CID(NS_SHISTORY_CID);
NS_DEFINE_NAMED_CID(NS_SHISTORY_INTERNAL_CID);
NS_DEFINE_NAMED_CID(NS_DOWNLOADHISTORY_CID);


const mozilla::Module::CIDEntry kDocShellCIDs[] = {
  { &kNS_DOCSHELL_CID, false, NULL, nsDocShellConstructor },
  { &kNS_DEFAULTURIFIXUP_CID, false, NULL, nsDefaultURIFixupConstructor },
  { &kNS_WEBNAVIGATION_INFO_CID, false, NULL, nsWebNavigationInfoConstructor },
  { &kNS_ABOUT_REDIRECTOR_MODULE_CID, false, NULL, nsAboutRedirector::Create },
  { &kNS_URI_LOADER_CID, false, NULL, nsURILoaderConstructor },
  { &kNS_DOCUMENTLOADER_SERVICE_CID, false, NULL, nsDocLoaderConstructor },
  { &kNS_EXTERNALHELPERAPPSERVICE_CID, false, NULL, nsOSHelperAppServiceConstructor },
  { &kNS_EXTERNALPROTOCOLHANDLER_CID, false, NULL, nsExternalProtocolHandlerConstructor },
  { &kNS_PREFETCHSERVICE_CID, false, NULL, nsPrefetchServiceConstructor },
  { &kNS_OFFLINECACHEUPDATESERVICE_CID, false, NULL, nsOfflineCacheUpdateServiceConstructor },
  { &kNS_OFFLINECACHEUPDATE_CID, false, NULL, nsOfflineCacheUpdateConstructor },
  { &kNS_LOCALHANDLERAPP_CID, false, NULL, PlatformLocalHandlerApp_tConstructor },
#ifdef MOZ_ENABLE_DBUS
  { &kNS_DBUSHANDLERAPP_CID, false, NULL, nsDBusHandlerAppConstructor },
#endif
#ifdef ANDROID
  { &kNS_EXTERNALSHARINGAPPSERVICE_CID, false, NULL, nsExternalSharingAppServiceConstructor },
#endif
  { &kNS_SHENTRY_CID, false, NULL, nsSHEntryConstructor },
  { &kNS_HISTORYENTRY_CID, false, NULL, nsSHEntryConstructor },
  { &kNS_SHTRANSACTION_CID, false, NULL, nsSHTransactionConstructor },
  { &kNS_SHISTORY_CID, false, NULL, nsSHistoryConstructor },
  { &kNS_SHISTORY_INTERNAL_CID, false, NULL, nsSHistoryConstructor },
  { &kNS_DOWNLOADHISTORY_CID, false, NULL, nsDownloadHistoryConstructor },
  { NULL }
};

const mozilla::Module::ContractIDEntry kDocShellContracts[] = {
  { "@mozilla.org/docshell;1", &kNS_DOCSHELL_CID },
  { NS_URIFIXUP_CONTRACTID, &kNS_DEFAULTURIFIXUP_CID },
  { NS_WEBNAVIGATION_INFO_CONTRACTID, &kNS_WEBNAVIGATION_INFO_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "about", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "config", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
#ifdef MOZ_CRASHREPORTER
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "crashes", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
#endif
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "credits", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "plugins", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "mozilla", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "logo", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "buildconfig", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "license", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "neterror", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "memory", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "addons", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "support", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_URI_LOADER_CONTRACTID, &kNS_URI_LOADER_CID },
  { NS_DOCUMENTLOADER_SERVICE_CONTRACTID, &kNS_DOCUMENTLOADER_SERVICE_CID },
  { NS_EXTERNALHELPERAPPSERVICE_CONTRACTID, &kNS_EXTERNALHELPERAPPSERVICE_CID },
  { NS_EXTERNALPROTOCOLSERVICE_CONTRACTID, &kNS_EXTERNALHELPERAPPSERVICE_CID },
  { NS_MIMESERVICE_CONTRACTID, &kNS_EXTERNALHELPERAPPSERVICE_CID },
  { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX"default", &kNS_EXTERNALPROTOCOLHANDLER_CID },
  { NS_PREFETCHSERVICE_CONTRACTID, &kNS_PREFETCHSERVICE_CID },
  { NS_OFFLINECACHEUPDATESERVICE_CONTRACTID, &kNS_OFFLINECACHEUPDATESERVICE_CID },
  { NS_OFFLINECACHEUPDATE_CONTRACTID, &kNS_OFFLINECACHEUPDATE_CID },
  { NS_LOCALHANDLERAPP_CONTRACTID, &kNS_LOCALHANDLERAPP_CID },
#ifdef MOZ_ENABLE_DBUS
  { NS_DBUSHANDLERAPP_CONTRACTID, &kNS_DBUSHANDLERAPP_CID },
#endif
#ifdef ANDROID
  { NS_EXTERNALSHARINGAPPSERVICE_CONTRACTID, &kNS_EXTERNALSHARINGAPPSERVICE_CID },
#endif
  { NS_SHENTRY_CONTRACTID, &kNS_SHENTRY_CID },
  { NS_HISTORYENTRY_CONTRACTID, &kNS_HISTORYENTRY_CID },
  { NS_SHTRANSACTION_CONTRACTID, &kNS_SHTRANSACTION_CID },
  { NS_SHISTORY_CONTRACTID, &kNS_SHISTORY_CID },
  { NS_SHISTORY_INTERNAL_CONTRACTID, &kNS_SHISTORY_INTERNAL_CID },
  { NS_DOWNLOADHISTORY_CONTRACTID, &kNS_DOWNLOADHISTORY_CID },
  { NULL }
};

static const mozilla::Module kDocShellModule = {
  mozilla::Module::kVersion,
  kDocShellCIDs,
  kDocShellContracts,
  NULL,
  NULL,
  Initialize,
  Shutdown
};

NSMODULE_DEFN(docshell_provider) = &kDocShellModule;
