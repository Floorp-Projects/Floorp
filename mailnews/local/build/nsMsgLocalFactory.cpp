/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Adam D. Moss <adam@gimp.org>
 *   Seth Spitzer <sspitzer@mozilla.org>
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

#include "msgCore.h" // for pre-compiled headers...
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsMsgLocalCID.h"

// include files for components this factory creates...
#include "nsMailboxUrl.h"
#include "nsPop3URL.h"
#include "nsMailboxService.h"
#include "nsLocalMailFolder.h"
#include "nsParseMailbox.h"
#include "nsPop3Service.h"
#ifdef HAVE_MOVEMAIL
#include "nsMovemailService.h"
#include "nsMovemailIncomingServer.h"
#endif /* HAVE_MOVEMAIL */
#include "nsNoneService.h"
#include "nsPop3IncomingServer.h"
#include "nsNoIncomingServer.h"
#include "nsLocalStringBundle.h"
#include "nsRssService.h"
#include "nsRssIncomingServer.h"
#include "nsCOMPtr.h"

// private factory declarations for each component we know how to produce

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMailboxUrl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPop3URL)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgMailboxParser)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMailboxService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPop3Service)
#ifdef HAVE_MOVEMAIL
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMovemailService)
#endif /* HAVE_MOVEMAIL */
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNoneService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgLocalMailFolder)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsParseMailMessageState)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPop3IncomingServer)
#ifdef HAVE_MOVEMAIL
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMovemailIncomingServer)
#endif /* HAVE_MOVEMAIL */
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNoIncomingServer)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLocalStringService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRssService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRssIncomingServer)

// The list of components we register
static const nsModuleComponentInfo gComponents[] = {
    { "Mailbox URL", NS_MAILBOXURL_CID,
      NS_MAILBOXURL_CONTRACTID, nsMailboxUrlConstructor },

    { "Mailbox Service", NS_MAILBOXSERVICE_CID,
      NS_MAILBOXSERVICE_CONTRACTID1, nsMailboxServiceConstructor },

    { "Mailbox Service", NS_MAILBOXSERVICE_CID,
      NS_MAILBOXSERVICE_CONTRACTID2, nsMailboxServiceConstructor },

    { "Mailbox Service", NS_MAILBOXSERVICE_CID,
      NS_MAILBOXSERVICE_CONTRACTID3, nsMailboxServiceConstructor },

    { "Mailbox Protocol Handler", NS_MAILBOXSERVICE_CID,
      NS_MAILBOXSERVICE_CONTRACTID4, nsMailboxServiceConstructor },

    { "Mailbox Parser", NS_MAILBOXPARSER_CID,
      NS_MAILBOXPARSER_CONTRACTID, nsMsgMailboxParserConstructor },

    { "Pop3 URL", NS_POP3URL_CID,
      NS_POP3URL_CONTRACTID, nsPop3URLConstructor },

    { "Pop3 Service", NS_POP3SERVICE_CID,
      NS_POP3SERVICE_CONTRACTID1, nsPop3ServiceConstructor },

    { "POP Protocol Handler", NS_POP3SERVICE_CID,
      NS_POP3SERVICE_CONTRACTID2, nsPop3ServiceConstructor },

    { "None Service", NS_NONESERVICE_CID,
      NS_NONESERVICE_CONTRACTID, nsNoneServiceConstructor },

#ifdef HAVE_MOVEMAIL
    { "Movemail Service", NS_MOVEMAILSERVICE_CID,
      NS_MOVEMAILSERVICE_CONTRACTID, nsMovemailServiceConstructor },
#endif /* HAVE_MOVEMAIL */

    { "pop3 Protocol Information", NS_POP3SERVICE_CID,
      NS_POP3PROTOCOLINFO_CONTRACTID, nsPop3ServiceConstructor },

    { "none Protocol Information", NS_NONESERVICE_CID,
      NS_NONEPROTOCOLINFO_CONTRACTID, nsNoneServiceConstructor },

#ifdef HAVE_MOVEMAIL
    { "movemail Protocol Information", NS_MOVEMAILSERVICE_CID,
      NS_MOVEMAILPROTOCOLINFO_CONTRACTID, nsMovemailServiceConstructor },
#endif /* HAVE_MOVEMAIL */

    { "Local Mail Folder Resource Factory", NS_LOCALMAILFOLDERRESOURCE_CID,
      NS_LOCALMAILFOLDERRESOURCE_CONTRACTID, nsMsgLocalMailFolderConstructor },

    { "Pop3 Incoming Server", NS_POP3INCOMINGSERVER_CID,
      NS_POP3INCOMINGSERVER_CONTRACTID, nsPop3IncomingServerConstructor },

#ifdef HAVE_MOVEMAIL 
    { "Movemail Incoming Server", NS_MOVEMAILINCOMINGSERVER_CID,
      NS_MOVEMAILINCOMINGSERVER_CONTRACTID, nsMovemailIncomingServerConstructor },
#endif /* HAVE_MOVEMAIL */

    { "No Incoming Server", NS_NOINCOMINGSERVER_CID,
      NS_NOINCOMINGSERVER_CONTRACTID, nsNoIncomingServerConstructor },

    { "Parse MailMessage State", NS_PARSEMAILMSGSTATE_CID,
      NS_PARSEMAILMSGSTATE_CONTRACTID, nsParseMailMessageStateConstructor },

    { "Mailbox String Bundle Service", NS_MSG_LOCALSTRINGSERVICE_CID,
      NS_MSG_MAILBOXSTRINGSERVICE_CONTRACTID, nsLocalStringServiceConstructor },

    { "Pop String Bundle Service", NS_MSG_LOCALSTRINGSERVICE_CID,
      NS_MSG_POPSTRINGSERVICE_CONTRACTID, nsLocalStringServiceConstructor },
      
    { "RSS Service", NS_RSSSERVICE_CID,
      NS_RSSSERVICE_CONTRACTID, nsRssServiceConstructor },
    
    { "RSS Protocol Information", NS_RSSSERVICE_CID,
      NS_RSSPROTOCOLINFO_CONTRACTID, nsRssServiceConstructor },
        
    { "RSS Incoming Server", NS_RSSINCOMINGSERVER_CID,
      NS_RSSINCOMINGSERVER_CONTRACTID, nsRssIncomingServerConstructor },
};

NS_IMPL_NSGETMODULE(local_mail_services, gComponents)

