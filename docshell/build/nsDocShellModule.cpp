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

#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsWebShell.h"
#include "nsDefaultURIFixup.h"

// uriloader
#include "nsURILoader.h"
#include "nsDocLoader.h"
#include "nsOSHelperAppService.h"
#include "nsExternalProtocolHandler.h"
#include "nsPrefetchService.h"

// session history
#include "nsSHEntry.h"
#include "nsSHistory.h"
#include "nsSHTransaction.h"

// global history
#include "nsGlobalHistoryAdapter.h"
#include "nsGlobalHistory2Adapter.h"

// docshell
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebShell)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDefaultURIFixup)

// uriloader
NS_GENERIC_FACTORY_CONSTRUCTOR(nsURILoader)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsDocLoaderImpl, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsOSHelperAppService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsExternalProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBlockedExternalProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrefetchService, Init)

#if defined(XP_MAC) || defined(XP_MACOSX)
#include "nsInternetConfigService.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsInternetConfigService)
#endif

// session history
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSHEntry)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSHTransaction)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsSHistory, Init)

// Currently no-one is instantiating docshell's directly because
// nsWebShell is still our main "shell" class. nsWebShell is a subclass
// of nsDocShell. Once migration is complete, docshells will be the main
// "shell" class and this module will need to register the docshell as
// a component
//NS_GENERIC_FACTORY_CONSTRUCTOR(nsDocShell)

static const nsModuleComponentInfo gDocShellModuleInfo[] = {
  // docshell
    { "WebShell", 
      NS_WEB_SHELL_CID,
      "@mozilla.org/webshell;1",
      nsWebShellConstructor
    },
    { "Default keyword fixup", 
      NS_DEFAULTURIFIXUP_CID,
      NS_URIFIXUP_CONTRACTID,
      nsDefaultURIFixupConstructor
    },

    // uriloader
  { "Netscape URI Loader Service", NS_URI_LOADER_CID, NS_URI_LOADER_CONTRACTID, nsURILoaderConstructor, },
  { "Netscape Doc Loader", NS_DOCUMENTLOADER_CID, NS_DOCUMENTLOADER_CONTRACTID, nsDocLoaderImplConstructor, },
  { "Netscape Doc Loader Service", NS_DOCUMENTLOADER_SERVICE_CID, NS_DOCUMENTLOADER_SERVICE_CONTRACTID, 
     nsDocLoaderImplConstructor, },
  { "Netscape External Helper App Service", NS_EXTERNALHELPERAPPSERVICE_CID, NS_EXTERNALHELPERAPPSERVICE_CONTRACTID, 
     nsOSHelperAppServiceConstructor, },
  { "Netscape External Helper App Service", NS_EXTERNALHELPERAPPSERVICE_CID, NS_EXTERNALPROTOCOLSERVICE_CONTRACTID, 
     nsOSHelperAppServiceConstructor, },
  { "Netscape Mime Mapping Service", NS_EXTERNALHELPERAPPSERVICE_CID, NS_MIMESERVICE_CONTRACTID, 
     nsOSHelperAppServiceConstructor, },
  { "Netscape Default Protocol Handler", NS_EXTERNALPROTOCOLHANDLER_CID, NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX"default", 
     nsExternalProtocolHandlerConstructor, },
  { "Netscape Default Blocked Protocol Handler", NS_BLOCKEDEXTERNALPROTOCOLHANDLER_CID, NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX"default-blocked", 
     nsBlockedExternalProtocolHandlerConstructor, },
  {  NS_PREFETCHSERVICE_CLASSNAME, NS_PREFETCHSERVICE_CID, NS_PREFETCHSERVICE_CONTRACTID,
     nsPrefetchServiceConstructor, },
#if defined(XP_MAC) || defined(XP_MACOSX)
  { "Internet Config Service", NS_INTERNETCONFIGSERVICE_CID, NS_INTERNETCONFIGSERVICE_CONTRACTID,
    nsInternetConfigServiceConstructor, },
#endif
    
    // session history
   { "nsSHEntry", NS_SHENTRY_CID,
      NS_SHENTRY_CONTRACTID, nsSHEntryConstructor },
   { "nsSHEntry", NS_HISTORYENTRY_CID,
      NS_HISTORYENTRY_CONTRACTID, nsSHEntryConstructor },
   { "nsSHTransaction", NS_SHTRANSACTION_CID,
      NS_SHTRANSACTION_CONTRACTID, nsSHTransactionConstructor },
   { "nsSHistory", NS_SHISTORY_CID,
      NS_SHISTORY_CONTRACTID, nsSHistoryConstructor },
   { "nsSHistory", NS_SHISTORY_INTERNAL_CID,
      NS_SHISTORY_INTERNAL_CONTRACTID, nsSHistoryConstructor },

    // global history adapters
    { "nsGlobalHistoryAdapter", NS_GLOBALHISTORYADAPTER_CID,
      nsnull, nsGlobalHistoryAdapter::Create,
      nsGlobalHistoryAdapter::RegisterSelf },
    { "nsGlobalHistory2Adapter", NS_GLOBALHISTORY2ADAPTER_CID,
      nsnull, nsGlobalHistory2Adapter::Create,
      nsGlobalHistory2Adapter::RegisterSelf }
};

// "docshell provider" to illustrate that this thing really *should*
// be dispensing docshells rather than webshells.
NS_IMPL_NSGETMODULE(docshell_provider, gDocShellModuleInfo)

