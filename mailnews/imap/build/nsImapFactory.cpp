/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "msgCore.h" // for pre-compiled headers...
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"

#include "nsIMAPHostSessionList.h"
#include "nsImapIncomingServer.h"
#include "nsImapService.h"
#include "nsImapMailFolder.h"
#include "nsImapUrl.h"
#include "nsImapProtocol.h"
#include "nsMsgImapCID.h"

// private factory declarations for each component we know how to produce
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsImapUrl, Initialize)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImapProtocol)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsIMAPHostSessionList)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImapIncomingServer)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImapService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImapMailFolder)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImapMockChannel)

// The list of components we register
static nsModuleComponentInfo gComponents[] = {
    { "IMAP URL", NS_IMAPURL_CID,
      nsnull, nsImapUrlConstructor },

    { "IMAP Protocol Channel", NS_IMAPPROTOCOL_CID,
      nsnull, nsImapProtocolConstructor },    

    { "IMAP Mock Channel", NS_IMAPMOCKCHANNEL_CID,
      nsnull, nsImapMockChannelConstructor },

    { "IMAP Host Session List", NS_IIMAPHOSTSESSIONLIST_CID,
      nsnull, nsIMAPHostSessionListConstructor },

    { "IMAP Incoming Server", NS_IMAPINCOMINGSERVER_CID,
      NS_IMAPINCOMINGSERVER_CONTRACTID, nsImapIncomingServerConstructor },

    { "Mail/News IMAP Resource Factory", NS_IMAPRESOURCE_CID,
      NS_RDF_RESOURCE_FACTORY_CONTRACTID_PREFIX "imap", 
      nsImapMailFolderConstructor },

    { "IMAP Service", NS_IMAPSERVICE_CID,
      "@mozilla.org/messenger/messageservice;1?type=imap_message",
      nsImapServiceConstructor },

    { "IMAP Service", NS_IMAPSERVICE_CID,
      "@mozilla.org/messenger/messageservice;1?type=imap",
      nsImapServiceConstructor },

    { "IMAP Protocol Handler", NS_IMAPSERVICE_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "imap", nsImapServiceConstructor},

    { "IMAP Protocol Handler", NS_IMAPSERVICE_CID,
      NS_IMAPPROTOCOLINFO_CONTRACTID, nsImapServiceConstructor }
};

NS_IMPL_NSGETMODULE("IMAP factory", gComponents);

#ifdef XP_WIN32
  //in addition to returning a version number for this module,
  //this also provides a convenient hook for the preloader
  //to keep (some if not all) of the module resident.
extern "C" __declspec(dllexport) float GetVersionNumber(void) {
  return 1.0;
}
#endif

