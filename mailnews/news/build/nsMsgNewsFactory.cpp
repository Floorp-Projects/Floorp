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
 */

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"

#include "msgCore.h"
#include "nsMsgBaseCID.h"
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
#include "nsNewsMessage.h"
#include "nsNNTPNewsgroup.h"
#include "nsNNTPNewsgroupPost.h"
#include "nsNNTPNewsgroupList.h"
#include "nsNNTPArticleList.h"
#include "nsNNTPHost.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsNntpUrl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNntpService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNntpIncomingServer)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNNTPArticleList)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNNTPHost)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNNTPNewsgroup)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNNTPNewsgroupPost)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNNTPNewsgroupList)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNewsMessage)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgNewsFolder)

static nsModuleComponentInfo components[] =
{
  { NS_NNTPURL_CID,       &nsNntpUrlConstructor,     NS_NNTPURL_PROGID, },
  { NS_NNTPSERVICE_CID,   &nsNntpServiceConstructor, NS_NNTPSERVICE_PROGID, },
  { NS_NNTPSERVICE_CID,   &nsNntpServiceConstructor, NS_NNTPPROTOCOLINFO_PROGID, },
  { NS_NNTPSERVICE_CID,   &nsNntpServiceConstructor, NS_NNTPMESSAGESERVICE_PROGID, },
  { NS_NNTPSERVICE_CID,   &nsNntpServiceConstructor, NS_NEWSMESSAGESERVICE_PROGID, },
  { NS_NNTPSERVICE_CID,   &nsNntpServiceConstructor, NS_NEWSPROTOCOLHANDLER_PROGID, },
  { NS_NEWSFOLDERRESOURCE_CID,  &nsMsgNewsFolderConstructor,      NS_NEWSFOLDERRESOURCE_PROGID, },
  { NS_NEWSMESSAGERESOURCE_CID, &nsNewsMessageConstructor,        NS_NEWSMESSAGERESOURCE_PROGID, },
  { NS_NNTPINCOMINGSERVER_CID,  &nsNntpIncomingServerConstructor, NS_NNTPINCOMINGSERVER_PROGID, },
  { NS_NNTPNEWSGROUP_CID,       &nsNNTPNewsgroupConstructor,      NS_NNTPNEWSGROUP_PROGID, },
  { NS_NNTPNEWSGROUPPOST_CID,   &nsNNTPNewsgroupPostConstructor,  NS_NNTPNEWSGROUP_PROGID, },
  { NS_NNTPNEWSGROUPLIST_CID,   &nsNNTPNewsgroupListConstructor,  NS_NNTPNEWSGROUPLIST_PROGID, },
  { NS_NNTPARTICLELIST_CID,     &nsNNTPArticleListConstructor,    NS_NNTPARTICLELIST_PROGID, },
  { NS_NNTPHOST_CID,            &nsNNTPHostConstructor,           NS_NNTPHOST_PROGID, },
};

NS_IMPL_MODULE(nsMsgNewsModule, components)
NS_IMPL_NSGETMODULE(nsMsgNewsModule)


