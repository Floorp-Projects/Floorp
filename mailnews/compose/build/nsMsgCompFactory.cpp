/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h"

#include "nsISupports.h"
#include "nsCOMPtr.h"

#include "nsIFactory.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIModule.h"

#include "pratom.h"
#include "nsMsgCompCID.h"

/* Include all of the interfaces our factory can generate components for */
#include "nsMsgSendLater.h"
#include "nsSmtpUrl.h"
#include "nsISmtpService.h"
#include "nsSmtpService.h"
#include "nsMsgComposeService.h"
#include "nsMsgComposeContentHandler.h"
#include "nsMsgCompose.h"
#include "nsMsgComposeParams.h"
#include "nsMsgComposeProgressParams.h"
#include "nsMsgAttachment.h"
#include "nsMsgSend.h"
#include "nsMsgQuote.h"
#include "nsIMsgDraft.h"
#include "nsMsgCreate.h"    // For drafts...I know, awful file name...
#include "nsURLFetcher.h"
#include "nsSmtpServer.h"
#include "nsSmtpDataSource.h"
#include "nsSmtpDelegateFactory.h"
#include "nsIContentHandler.h"
#include "nsMsgRecipientArray.h"
#include "nsMsgComposeStringBundle.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsSmtpService);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSmtpServer);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgCompose);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgComposeParams);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgComposeSendListener);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgComposeProgressParams);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgCompFields);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgAttachment);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgComposeAndSend);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgSendLater);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgDraft)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgComposeService, Init);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgComposeContentHandler);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgQuote);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgQuoteListener);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSmtpUrl);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMailtoUrl);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgRecipientArray);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsComposeStringService);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSmtpDataSource);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSmtpDelegateFactory);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsURLFetcher);

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

static nsModuleComponentInfo components[] =
{
  { "Msg Compose",
    NS_MSGCOMPOSE_CID,
    NS_MSGCOMPOSE_CONTRACTID,
    nsMsgComposeConstructor },
  { "Msg Compose Service",
    NS_MSGCOMPOSESERVICE_CID,
    NS_MSGCOMPOSESERVICE_CONTRACTID,
    nsMsgComposeServiceConstructor },
  { "Msg Compose Startup Handler",
    NS_MSGCOMPOSESERVICE_CID,
    NS_MSGCOMPOSESTARTUPHANDLER_CONTRACTID,
    nsMsgComposeServiceConstructor,
    nsMsgComposeService::RegisterProc,
    nsMsgComposeService::UnregisterProc },
  { "mailto content handler",
     NS_MSGCOMPOSECONTENTHANDLER_CID,
     NS_MSGCOMPOSECONTENTHANDLER_CONTRACTID,
     nsMsgComposeContentHandlerConstructor },
  { "Msg Compose Parameters",
    NS_MSGCOMPOSEPARAMS_CID,
    NS_MSGCOMPOSEPARAMS_CONTRACTID,
    nsMsgComposeParamsConstructor },
  { "Msg Compose Send Listener",
    NS_MSGCOMPOSESENDLISTENER_CID,
    NS_MSGCOMPOSESENDLISTENER_CONTRACTID,
    nsMsgComposeSendListenerConstructor },
  { "Msg Compose Progress Parameters",
    NS_MSGCOMPOSEPROGRESSPARAMS_CID,
    NS_MSGCOMPOSEPROGRESSPARAMS_CONTRACTID,
    nsMsgComposeProgressParamsConstructor },
  { "Msg Compose Fields",
    NS_MSGCOMPFIELDS_CID,
    NS_MSGCOMPFIELDS_CONTRACTID,
    nsMsgCompFieldsConstructor },
  { "Msg Compose Attachment",
    NS_MSGATTACHMENT_CID,
    NS_MSGATTACHMENT_CONTRACTID,
    nsMsgAttachmentConstructor },
  { "Msg Draft",
    NS_MSGDRAFT_CID,
    NS_MSGDRAFT_CONTRACTID,
    nsMsgDraftConstructor },
  { "Msg Send",
    NS_MSGSEND_CID,
    NS_MSGSEND_CONTRACTID,
    nsMsgComposeAndSendConstructor },
  { "Msg Send Later",
    NS_MSGSENDLATER_CID,
    NS_MSGSENDLATER_CONTRACTID,
    nsMsgSendLaterConstructor },
  { "SMTP Service",
    NS_SMTPSERVICE_CID,
    NS_SMTPSERVICE_CONTRACTID,
    nsSmtpServiceConstructor },
  { "SMTP Service",
    NS_SMTPSERVICE_CID,
    NS_MAILTOHANDLER_CONTRACTID,
    nsSmtpServiceConstructor },
  { "SMTP Server",
    NS_SMTPSERVER_CID,
    NS_SMTPSERVER_CONTRACTID,
    nsSmtpServerConstructor },
  { "SMTP URL",
    NS_SMTPURL_CID,
    NS_SMTPURL_CONTRACTID,
    nsSmtpUrlConstructor },
  { "MAILTO URL",
    NS_MAILTOURL_CID,
    NS_MAILTOURL_CONTRACTID,
    nsMailtoUrlConstructor },
  { "Msg Quote",
    NS_MSGQUOTE_CID,
    NS_MSGQUOTE_CONTRACTID,
    nsMsgQuoteConstructor },
  { "Msg Quote Listener",
    NS_MSGQUOTELISTENER_CID,
    NS_MSGQUOTELISTENER_CONTRACTID,
    nsMsgQuoteListenerConstructor },
  { "Msg Recipient Array",
    NS_MSGRECIPIENTARRAY_CID,
    NS_MSGRECIPIENTARRAY_CONTRACTID,
    nsMsgRecipientArrayConstructor },
  { "Compose string bundle",
    NS_MSG_COMPOSESTRINGSERVICE_CID,
    NS_MSG_COMPOSESTRINGSERVICE_CONTRACTID,
    nsComposeStringServiceConstructor },
  { "SMTP string bundle",
    NS_MSG_COMPOSESTRINGSERVICE_CID,
    NS_MSG_SMTPSTRINGSERVICE_CONTRACTID,
    nsComposeStringServiceConstructor },
  { "SMTP Datasource",
    NS_SMTPDATASOURCE_CID,
    NS_SMTPDATASOURCE_CONTRACTID,
    nsSmtpDataSourceConstructor },
  { "SMTP Delegate Factory",
    NS_SMTPDELEGATEFACTORY_CID,
    NS_SMTPDELEGATEFACTORY_CONTRACTID,
    nsSmtpDelegateFactoryConstructor },
  { "URL Fetcher",
    NS_URLFETCHER_CID,
    NS_URLFETCHER_CONTRACTID,
    nsURLFetcherConstructor },
};

  
NS_IMPL_NSGETMODULE(nsMsgComposeModule, components)
