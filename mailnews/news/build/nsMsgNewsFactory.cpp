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

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"

#include "msgCore.h"
#include "pratom.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"

#include "nsNewsFolder.h"
#include "nsMsgNewsCID.h"

/* Include all of the interfaces our factory can generate components for */
#include "nsNntpUrl.h"
#include "nsNntpService.h"
#include "nsNntpIncomingServer.h"
#include "nsNNTPNewsgroupPost.h"
#include "nsNNTPNewsgroupList.h"
#include "nsNNTPArticleList.h"
#include "nsNewsDownloadDialogArgs.h"
#include "nsIContentHandler.h"
#include "nsCURILoader.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsNntpUrl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNntpService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNntpIncomingServer)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNNTPArticleList)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNNTPNewsgroupPost)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNNTPNewsgroupList)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgNewsFolder)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNewsDownloadDialogArgs)

static nsModuleComponentInfo components[] =
{
  { "NNTP URL",
    NS_NNTPURL_CID,
    NS_NNTPURL_CONTRACTID,
    nsNntpUrlConstructor },
  { "NNTP Service",
    NS_NNTPSERVICE_CID,
    NS_NNTPSERVICE_CONTRACTID,
    nsNntpServiceConstructor },
  { "News Startup Handler",
    NS_NNTPSERVICE_CID,
    NS_NEWSSTARTUPHANDLER_CONTRACTID,
    nsNntpServiceConstructor,
    nsNntpService::RegisterProc,
    nsNntpService::UnregisterProc },
  { "NNTP Protocol Info",
    NS_NNTPSERVICE_CID,
    NS_NNTPPROTOCOLINFO_CONTRACTID,
    nsNntpServiceConstructor },
  { "NNTP Message Service",
    NS_NNTPSERVICE_CID,
    NS_NNTPMESSAGESERVICE_CONTRACTID,
    nsNntpServiceConstructor },
  { "News Message Service",
    NS_NNTPSERVICE_CID,
    NS_NEWSMESSAGESERVICE_CONTRACTID,
    nsNntpServiceConstructor },
  { "News Protocol Handler",
    NS_NNTPSERVICE_CID,
    NS_NEWSPROTOCOLHANDLER_CONTRACTID,
    nsNntpServiceConstructor },
  { "News Message Protocol Handler",
    NS_NNTPSERVICE_CID,
    NS_NEWSMESSAGEPROTOCOLHANDLER_CONTRACTID,
    nsNntpServiceConstructor },
  { "Secure News Protocol Handler",
    NS_NNTPSERVICE_CID,
    NS_SNEWSPROTOCOLHANDLER_CONTRACTID,
    nsNntpServiceConstructor },
  { "newsgroup content handler",
    NS_NNTPSERVICE_CID,
    NS_CONTENT_HANDLER_CONTRACTID_PREFIX"x-application-newsgroup",
    nsNntpServiceConstructor },
  { "News Folder Resource",
    NS_NEWSFOLDERRESOURCE_CID,
    NS_NEWSFOLDERRESOURCE_CONTRACTID,
    nsMsgNewsFolderConstructor },
  { "NNTP Incoming Servier",
    NS_NNTPINCOMINGSERVER_CID,
    NS_NNTPINCOMINGSERVER_CONTRACTID,
    nsNntpIncomingServerConstructor },
  { "NNTP Newsgroup Post",
    NS_NNTPNEWSGROUPPOST_CID,
    NS_NNTPNEWSGROUPPOST_CONTRACTID,
    nsNNTPNewsgroupPostConstructor },
  { "NNTP Newsgroup List",
    NS_NNTPNEWSGROUPLIST_CID,
    NS_NNTPNEWSGROUPLIST_CONTRACTID,
    nsNNTPNewsgroupListConstructor },
  { "NNTP Article List",
    NS_NNTPARTICLELIST_CID,
    NS_NNTPARTICLELIST_CONTRACTID,
    nsNNTPArticleListConstructor },
  { "News download dialog args",
    NS_NEWSDOWNLOADDIALOGARGS_CID,
    NS_NEWSDOWNLOADDIALOGARGS_CONTRACTID,
    nsNewsDownloadDialogArgsConstructor }
};

NS_IMPL_NSGETMODULE(nsMsgNewsModule, components)


